/* SPDX-License-Identifier: GPL-2.0-or-later
 * engine_hooks.c on a small executable "image": an entry hook sees the arguments and leaves the
 * function's integer and vector arguments intact, a call-site hook replaces the callee and can
 * call the original, and both refuse unexpected bytes without changing anything. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "engine_hooks.h"

typedef int64_t (BB_GUEST_ABI *IntFn)(int64_t, int64_t);
typedef double (BB_GUEST_ABI *VecFn)(double, double);

enum { SUM = 0x100, ADD = 0x200, CALLER = 0x300, IMAGE = 0x2000 };
/* sum(a, b) = a + 2b: push rbp; mov rbp,rsp; lea rax,[rdi+rsi*2]; pop rbp; ret */
static const unsigned char sum_code[] = {0x55, 0x48, 0x89, 0xe5, 0x48, 0x8d, 0x04, 0x77, 0x5d, 0xc3};
/* add(x, y) = x + y: addsd xmm0,xmm1; ret (the whole function is the displaced prologue) */
static const unsigned char add_code[] = {0xf2, 0x0f, 0x58, 0xc1, 0xc3};

static int64_t seen_a, seen_b;
static int entries;
static volatile double sink;

static BB_GUEST_ABI void on_sum(int64_t a, int64_t b) {
    seen_a = a;
    seen_b = b;
    ++entries;
}
/* Uses the vector registers itself, so a missing save would change add()'s arguments. */
static BB_GUEST_ABI void on_add(double x, double y) {
    sink = x * 3.0 + y * 7.0 + 0.25;
    ++entries;
}
static BB_GUEST_ABI int64_t replace_sum(int64_t a, int64_t b) {
    return ((IntFn)bb_guest(SUM))(a, b) * 10;
}

int main(void) {
    unsigned char *image = VirtualAlloc(NULL, IMAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(image);
    memset(image, 0xcc, IMAGE);
    memcpy(image + SUM, sum_code, sizeof(sum_code));
    memcpy(image + ADD, add_code, sizeof(add_code));
    /* caller(a, b): call sum; ret */
    image[CALLER] = 0xe8;
    int32_t rel = SUM - (CALLER + 5);
    memcpy(image + CALLER + 1, &rel, 4);
    image[CALLER + 5] = 0xc3;

    assert(bb_hooks_init(image, IMAGE) == 0);
    unsigned char before[IMAGE];
    memcpy(before, image, IMAGE);
    const unsigned char wrong[] = {0x55, 0x48, 0x89, 0xe5, 0x90};
    assert(bb_hook_entry(SUM, wrong, sizeof(wrong), (void *)on_sum) == -1);
    assert(bb_hook_call(CALLER, ADD, (void *)replace_sum) == -1);
    assert(bb_hook_call(SUM, SUM, (void *)replace_sum) == -1);
    assert(!memcmp(before, image, IMAGE));  /* refusals change nothing */

    /* A guarded data write: only over the expected bytes, nothing changed otherwise. */
    const unsigned char pad[4] = {0xcc, 0xcc, 0xcc, 0xcc}, data[4] = {1, 2, 3, 4}, other[4] = {9, 9, 9, 9};
    assert(bb_patch(0x1800, other, data, 4) == -1 && !memcmp(before, image, IMAGE));
    assert(bb_patch(IMAGE - 2, pad, data, 4) == -1);
    assert(bb_patch(0x1800, pad, data, 4) == 0 && !memcmp(image + 0x1800, data, 4));
    assert(bb_patch(0x1800, pad, data, 4) == -1);  /* already written */

    assert(bb_hook_entry(SUM, sum_code, 8, (void *)on_sum) == 0);
    assert(bb_hook_entry(ADD, add_code, sizeof(add_code), (void *)on_add) == 0);
    assert(bb_hook_call(CALLER, SUM, (void *)replace_sum) == 0);
    assert(bb_hook_entry(SUM, sum_code, 8, (void *)on_sum) == -1);  /* already hooked */
    bb_hooks_seal();
    DWORD old;
    assert(VirtualProtect(image, IMAGE, PAGE_EXECUTE_READ, &old));
    FlushInstructionCache(GetCurrentProcess(), image, IMAGE);

    assert(((IntFn)(image + SUM))(5, 7) == 19);
    assert(seen_a == 5 && seen_b == 7 && entries == 1);
    assert(((VecFn)(image + ADD))(1.5, 2.25) == 3.75);
    assert(entries == 2);
    assert(((IntFn)(image + CALLER))(1, 2) == 50);  /* replaced, and the original still ran */
    assert(seen_a == 1 && seen_b == 2 && entries == 3);
    puts("engine hooks: ok");
    return 0;
}
