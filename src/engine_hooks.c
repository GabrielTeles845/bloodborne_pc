/* SPDX-License-Identifier: GPL-2.0-or-later
 * Guarded hooks into the loaded game image (engine_hooks.h). */
#include "engine_hooks.h"

#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

enum { ARENA_SIZE = 0x10000, REACH = 0x7ff00000 };

static unsigned char *g_image, *g_arena;
static size_t g_image_size, g_used;
static int g_sealed;
static const char *g_error = "";

static int refuse(const char *why) {
    g_error = why;
    return -1;
}

const char *bb_hooks_error(void) { return g_error; }

int bb_hooks_enabled(void) {
    const char *value = getenv("BB_ENGINE_HOOKS");
    return !(value && !strcmp(value, "0"));
}

void *bb_guest(uint64_t offset) { return g_image ? g_image + offset : NULL; }

static int reachable(const unsigned char *from, const unsigned char *to) {
    const int64_t distance = (int64_t)((uintptr_t)to - (uintptr_t)from);
    return distance > -(int64_t)REACH && distance < (int64_t)REACH;
}

/* A rel32 jump or call from anywhere in the image must reach the stubs, so they live in a
 * region reserved just past the image (or, failing that, just before it). */
static unsigned char *reserve_near(unsigned char *image, size_t size) {
    const uintptr_t granule = 0x10000;
    const uintptr_t end = ((uintptr_t)image + size + granule - 1) & ~(granule - 1);
    for (uintptr_t step = 0; step < 0x40000000u; step += granule) {
        uintptr_t candidates[2] = {end + step, (uintptr_t)image > step + ARENA_SIZE
                                                   ? (((uintptr_t)image - step - ARENA_SIZE) & ~(granule - 1)) : 0};
        for (int i = 0; i < 2; ++i) {
            if (!candidates[i]) continue;
            unsigned char *want = (unsigned char *)candidates[i];
            if (!reachable(image, want) || !reachable(want + ARENA_SIZE, image + size)) continue;
#ifdef _WIN32
            unsigned char *got = VirtualAlloc(want, ARENA_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (got) return got;
#else
            void *got = mmap(want, ARENA_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (got != MAP_FAILED) {
                if (got == want) return got;
                munmap(got, ARENA_SIZE);
            }
#endif
        }
    }
    return NULL;
}

int bb_hooks_init(unsigned char *image, size_t size) {
    if (g_arena) return refuse("hooks already initialised");
    g_arena = reserve_near(image, size);
    if (!g_arena) return refuse("no memory within rel32 reach of the image");
    memset(g_arena, 0xcc, ARENA_SIZE);
    g_image = image;
    g_image_size = size;
    g_used = 0;
    g_sealed = 0;
    return 0;
}

void bb_hooks_seal(void) {
    if (!g_arena || g_sealed) return;
#ifdef _WIN32
    DWORD old;
    VirtualProtect(g_arena, ARENA_SIZE, PAGE_EXECUTE_READ, &old);
    FlushInstructionCache(GetCurrentProcess(), g_arena, ARENA_SIZE);
#else
    mprotect(g_arena, ARENA_SIZE, PROT_READ | PROT_EXEC);
#endif
    g_sealed = 1;
}

static unsigned char *stub(size_t size) {
    if (!g_arena || g_sealed || g_used + size > ARENA_SIZE) return NULL;
    unsigned char *at = g_arena + g_used;
    g_used = (g_used + size + 15) & ~(size_t)15;
    return at;
}

static int in_image(uint64_t at, size_t n) { return g_image && at <= g_image_size && n <= g_image_size - at; }

static void put_rel32(unsigned char *field, const unsigned char *next, const unsigned char *to) {
    const int32_t rel = (int32_t)((intptr_t)to - (intptr_t)next);
    memcpy(field, &rel, 4);
}

static unsigned char *emit(unsigned char *p, const void *bytes, size_t n) {
    memcpy(p, bytes, n);
    return p + n;
}

static unsigned char *emit_movabs_r11(unsigned char *p, const void *target) {
    const uint64_t value = (uint64_t)(uintptr_t)target;
    *p++ = 0x49; *p++ = 0xbb;  /* movabs r11, imm64 */
    return emit(p, &value, 8);
}

int bb_hook_call(uint64_t at, uint64_t target, void *host) {
    if (!bb_hooks_enabled()) return refuse("engine hooks are off (BB_ENGINE_HOOKS=0)");
    if (!host || !in_image(at, 5) || !in_image(target, 1)) return refuse("address outside the image");
    unsigned char *site = g_image + at;
    if (site[0] != 0xe8 && site[0] != 0xe9) return refuse("not a call or jmp rel32");
    int32_t rel;
    memcpy(&rel, site + 1, 4);
    if (at + 5 + (int64_t)rel != target) return refuse("the call does not go to the expected function");
    unsigned char *s = stub(13);
    if (!s) return refuse("no stub space (not initialised, sealed or full)");
    unsigned char *p = emit_movabs_r11(s, host);
    *p++ = 0x41; *p++ = 0xff; *p++ = 0xe3;  /* jmp r11 */
    put_rel32(site + 1, site + 5, s);
    return 0;
}

int bb_patch(uint64_t at, const void *expected, const void *replacement, size_t n) {
    if (!bb_hooks_enabled()) return refuse("engine hooks are off (BB_ENGINE_HOOKS=0)");
    if (!expected || !replacement || !n || !in_image(at, n)) return refuse("address outside the image");
    if (memcmp(g_image + at, expected, n)) return refuse("unexpected bytes at the patch site");
    memcpy(g_image + at, replacement, n);
    return 0;
}

int bb_hook_entry(uint64_t at, const unsigned char *prologue, size_t n, void *host) {
    if (!bb_hooks_enabled()) return refuse("engine hooks are off (BB_ENGINE_HOOKS=0)");
    if (!host || !prologue || n < 5 || n > 32) return refuse("bad hook arguments");
    if (!in_image(at, n)) return refuse("address outside the image");
    unsigned char *site = g_image + at;
    if (memcmp(site, prologue, n)) return refuse("unexpected bytes at the function entry");
    /* Save the argument registers (integer, rax for variadic callers, xmm0-7), call the host
     * function with them, restore them, run the displaced prologue and continue after it.
     * At entry rsp is 8 mod 16; seven pushes make it 0 and the 0x80-byte area keeps it. */
    static const unsigned char save[] = {
        0x57, 0x56, 0x52, 0x51, 0x41, 0x50, 0x41, 0x51, 0x50,  /* push rdi rsi rdx rcx r8 r9 rax */
        0x48, 0x81, 0xec, 0x80, 0x00, 0x00, 0x00,              /* sub rsp, 0x80 */
    };
    static const unsigned char restore[] = {
        0x48, 0x81, 0xc4, 0x80, 0x00, 0x00, 0x00,              /* add rsp, 0x80 */
        0x58, 0x41, 0x59, 0x41, 0x58, 0x59, 0x5a, 0x5e, 0x5f,  /* pop rax r9 r8 rcx rdx rsi rdi */
    };
    unsigned char *s = stub(sizeof(save) + 8 * 6 + 13 + 8 * 6 + sizeof(restore) + n + 5);
    if (!s) return refuse("no stub space (not initialised, sealed or full)");
    unsigned char *p = emit(s, save, sizeof(save));
    for (int i = 0; i < 8; ++i) {  /* movdqu [rsp+16*i], xmm_i */
        const unsigned char op[] = {0xf3, 0x0f, 0x7f, (unsigned char)(0x44 | i << 3), 0x24, (unsigned char)(16 * i)};
        p = emit(p, op, sizeof(op));
    }
    p = emit_movabs_r11(p, host);
    *p++ = 0x41; *p++ = 0xff; *p++ = 0xd3;  /* call r11 */
    for (int i = 0; i < 8; ++i) {  /* movdqu xmm_i, [rsp+16*i] */
        const unsigned char op[] = {0xf3, 0x0f, 0x6f, (unsigned char)(0x44 | i << 3), 0x24, (unsigned char)(16 * i)};
        p = emit(p, op, sizeof(op));
    }
    p = emit(p, restore, sizeof(restore));
    p = emit(p, prologue, n);
    *p++ = 0xe9;
    put_rel32(p, p + 4, site + n);
    site[0] = 0xe9;
    put_rel32(site + 1, site + 5, s);
    memset(site + 5, 0xcc, n - 5);  /* never reached: the stub returns past them */
    return 0;
}
