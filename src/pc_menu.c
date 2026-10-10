/* SPDX-License-Identifier: GPL-2.0-or-later
 * The in-game PC menu: "PC Settings" and "PC Enhancements" rows in the game's own System menu,
 * each opening a section drawn by the game like Controls or Language.
 *
 * Ported from droogie's bbhost (https://github.com/droogie/bbhost, GPL-3.0-or-later,
 * engine/option_menu.cpp), which has the full map of the engine's option screens. In short:
 * System is a command list - rows appended to a builder by sub_1f4e2a0 and finalised by
 * sub_1fea630. A row's std::function opens a section through sub_1fb4c70, whose opener calls
 * sub_1f20900(root, params, "<Name>", handler): that binds the movie's sprite of that name,
 * makes the dialog and calls handler(dialog, params), which builds the rows. The engine owns
 * the cursor, widgets, key guide and fade.
 *
 * So the menu adds its own rows to System (the one call that finalises the builder goes through
 * a hook) and its own opener and handlers. sub_1f20900 only opens names in a six-entry table, so
 * slot [6] - unreferenced padding - points at the name being opened and the loop walks seven.
 * The sections' sprites and strings come from scripts/menu_assets.py (BB_PC_MENU_ASSETS=1).
 *
 * A row writes its value through a pointer into this file; once a frame the changes become
 * bbport.ini settings, the same ones the Insert menu and the launcher change. */
#include "pc_menu.h"
#include "engine_hooks.h"
#include "gpu/bbgpu.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Binary Ninja addresses of CUSA03173 1.09 (eboot at 0x400000). */
enum {
    OPEN_SECTION = 0x1f20900,      /* sub_1f20900(root, params, name, handler, 0, 0) */
    SCRATCH_INIT = 0x208af20,      /* a handler's scratch block */
    ADD_CHOICE_ROW = 0x1f2a100,    /* (dialog, captions, value*, list*, default*) */
    BUILD_ON_OFF = 0x1f2b3b0,      /* the game's two-entry On/Off list */
    MSG_REPOSITORY = 0x1ee8cc0,    /* (caption*, group, id) */
    ADD_COMMAND_ROW = 0x1f4e2a0,   /* (builder, captions, std::function*, flag*) */
    ADD_EXIT_ROW = 0x1f4c9a0,      /* Exit Game's own adder (builder, captions, std::function*) */
    EXIT_GAME_OPENER = 0x200c9f0,
    SYSTEM_FINALIZE = 0x1fea630,
    SYSTEM_FINALIZE_CALL = 0x1fb4978,
    OPEN_SECTION_FLOW = 0x1fb4c70, /* (out, ctx, std::function*) */
    ROW_FUNCTOR_VTABLE = 0x573d120,
    OPENER_FUNCTOR_VTABLE = 0x5743880,
    SECTION_NAME_TABLE = 0x57398b0,
    SECTION_COUNT_IMM = 0x1f20e21, /* the 6 in cmp rdx, 6 */
};
enum { MSG_MENU_TEXT = 0xc8, MSG_LINE_HELP = 0xc9, EXIT_GAME_ROW_ID = 0x1adb4 };
enum { BUILDER_BEGIN = 0x68, BUILDER_END = 0x70, BUILDER_ROW = 0x180 };
/* The menu's message ids (scripts/pc_menu_messages.tsv). */
enum { ROW_SETTINGS = 110006, ROW_ENHANCE = 110013 };

typedef int64_t (BB_GUEST_ABI *GuestFn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

static int g_installed;
static char g_open_name[32] = "PCSetting";

static GuestFn fn(uint64_t bn) { return (GuestFn)bb_guest(BB_BN(bn)); }
static uint64_t addr(uint64_t bn) { return (uint64_t)(uintptr_t)bb_guest(BB_BN(bn)); }
static uint64_t ptr(const void *p) { return (uint64_t)(uintptr_t)p; }
static uint64_t rd64(const void *p) {
    uint64_t v;
    memcpy(&v, p, 8);
    return v;
}

/* A row's caption pair: the label at +0x00 and its line help at +0x40, each the raw UTF-16
 * pointer drawn at +0x00 and an MSVC wstring copy at +0x08. */
typedef struct {
    _Alignas(16) unsigned char buf[0x80];
} Captions;

/* MSVC _String_val at `ws`: the buffer pointer at +0, the capacity at +0x18 (below 8 is the
 * inline buffer), the allocator at +0x20 whose vtable slot +0x70 frees. */
static void free_wstring(unsigned char *ws) {
    const uint64_t capacity = rd64(ws + 0x18), buffer = rd64(ws), allocator = rd64(ws + 0x20);
    if (capacity < 8 || !buffer || !allocator) return;
    const uint64_t vtable = rd64((void *)(uintptr_t)allocator);
    ((GuestFn)(uintptr_t)rd64((void *)(uintptr_t)(vtable + 0x70)))(allocator, buffer, 0, 0, 0, 0);
}

static void captions_init(Captions *c, uint32_t id) {
    memset(c, 0, sizeof(*c));
    fn(MSG_REPOSITORY)(ptr(c->buf), MSG_MENU_TEXT, id, 0, 0, 0);
    fn(MSG_REPOSITORY)(ptr(c->buf + 0x40), MSG_LINE_HELP, id, 0, 0, 0);
}

static void captions_free(Captions *c) {
    free_wstring(c->buf + 0x08);
    free_wstring(c->buf + 0x48);
}

/* An engine std::function (MSVC): 0x20 bytes of inline storage holding the callable - its
 * vtable and the wrapped function pointer - then a pointer to that storage at +0x20. The
 * consumers copy it, so one on the stack is enough. */
typedef struct {
    _Alignas(16) unsigned char buf[0x28];
} GuestFunction;

static void function_init(GuestFunction *f, uint64_t vtable_bn, const void *target) {
    memset(f, 0, sizeof(*f));
    const uint64_t vtable = addr(vtable_bn), callee = ptr(target), self = ptr(f->buf);
    memcpy(f->buf, &vtable, 8);
    memcpy(f->buf + 8, &callee, 8);
    memcpy(f->buf + 0x20, &self, 8);
}

/* --- Settings ----------------------------------------------------------------------------- */

/* One On/Off row: the bbport.ini key, its message id, the byte the row writes and the value
 * last pushed (a frame pushes a change only). */
typedef struct {
    const char *key;
    uint32_t id;
    uint8_t value, last;
} Toggle;

static Toggle g_settings[] = {
    {"show_fps", 116007, 0, 0},
    {"skip_intro", 116004, 0, 0},
};
static Toggle g_enhance[] = {
    {"pc_mirror_appearance", 124001, 0, 0},
    {"pc_two_invaders", 124003, 0, 0},
};
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static void seed(Toggle *t) {
    const int value = bbgpu_setting_get(t->key);
    t->value = t->last = value > 0;
}

static void push(Toggle *t) {
    if (t->value == t->last) return;
    t->last = t->value;
    bbgpu_setting_set(t->key, t->value != 0);
    printf("PC menu: %s %s\n", t->key, t->value ? "on" : "off");
}

/* An On/Off row from the game's own list; `def` is what the section's Defaults row restores
 * (the base's default for the setting). */
static void add_toggle_row(void *dialog, Toggle *t) {
    Captions captions;
    captions_init(&captions, t->id);
    _Alignas(16) unsigned char list[0x100];
    memset(list, 0, sizeof(list));
    uint8_t def = bbgpu_setting_default(t->key) > 0;
    fn(BUILD_ON_OFF)(ptr(list), 0, 0, 0, 0, 0);
    fn(ADD_CHOICE_ROW)(ptr(dialog), ptr(captions.buf), ptr(&t->value), ptr(list), ptr(&def), 0);
    const uint64_t entries = rd64(list + 0x98);
    for (uint64_t k = 0; k < entries && k < 2; ++k) free_wstring(list + k * 0x48 + 0x18);
    captions_free(&captions);
}

static void build_section(void *dialog, Toggle *rows, size_t count, const char *name) {
    _Alignas(16) unsigned char scratch[0x100];
    memset(scratch, 0, sizeof(scratch));
    fn(SCRATCH_INIT)(ptr(scratch), 0, 0, 0, 0, 0);
    for (size_t i = 0; i < count; ++i) {
        seed(&rows[i]);
        add_toggle_row(dialog, &rows[i]);
    }
    printf("PC menu: %s opened\n", name);
}

static BB_GUEST_ABI void settings_handler(void *dialog, void *params) {
    (void)params;
    build_section(dialog, g_settings, COUNT(g_settings), "PC Settings");
}

static BB_GUEST_ABI void enhance_handler(void *dialog, void *params) {
    (void)params;
    build_section(dialog, g_enhance, COUNT(g_enhance), "PC Enhancements");
}

/* --- Opening a section -------------------------------------------------------------------- */

static int64_t open_named(void *root, void *params, const char *name, const void *handler) {
    snprintf(g_open_name, sizeof(g_open_name), "%s", name);
    return fn(OPEN_SECTION)(ptr(root), ptr(params), ptr(g_open_name), ptr(handler), 0, 0);
}

static BB_GUEST_ABI int64_t open_settings(void *root, void *params) {
    return open_named(root, params, "PCSetting", (const void *)settings_handler);
}

static BB_GUEST_ABI int64_t open_enhance(void *root, void *params) {
    return open_named(root, params, "PCEnhance", (const void *)enhance_handler);
}

/* What a System row runs when chosen: the two lines sub_1fb4be0 runs for Language, with the
 * menu's opener in place of the game's. */
static void *row_open(void *out, void *ctx, const void *opener) {
    GuestFunction f;
    function_init(&f, OPENER_FUNCTOR_VTABLE, opener);
    fn(OPEN_SECTION_FLOW)(ptr(out), ptr(ctx), ptr(f.buf), 0, 0, 0);
    return out;
}

static BB_GUEST_ABI void *settings_row(void *out, void *ctx) { return row_open(out, ctx, (const void *)open_settings); }
static BB_GUEST_ABI void *enhance_row(void *out, void *ctx) { return row_open(out, ctx, (const void *)open_enhance); }

/* --- The System list ---------------------------------------------------------------------- */

static unsigned char *builder_last_row(void *builder) {
    const unsigned char *b = builder;
    const uint64_t begin = rd64(b + BUILDER_BEGIN), end = rd64(b + BUILDER_END);
    if (!begin || end <= begin || (end - begin) % BUILDER_ROW) return NULL;
    return (unsigned char *)(uintptr_t)(end - BUILDER_ROW);
}

/* Exit Game's row holds its opener somewhere in its 0x180 bytes. */
static int row_is_exit_game(const unsigned char *row) {
    const uint64_t want = addr(EXIT_GAME_OPENER);
    for (size_t i = 0; i + 8 <= BUILDER_ROW; i += 8)
        if (rd64(row + i) == want) return 1;
    return 0;
}

/* The game's own pop: slot 1 of the row's vtable, then the end pointer back one row. */
static void pop_last_row(void *builder) {
    unsigned char *row = builder_last_row(builder);
    if (!row) return;
    const uint64_t vtable = rd64(row);
    ((GuestFn)(uintptr_t)rd64((void *)(uintptr_t)(vtable + 8)))(ptr(row), 0, 0, 0, 0, 0);
    const uint64_t end = rd64((unsigned char *)builder + BUILDER_END) - BUILDER_ROW;
    memcpy((unsigned char *)builder + BUILDER_END, &end, 8);
}

static void add_command_row(void *builder, uint32_t id, const void *open) {
    Captions captions;
    GuestFunction f;
    int64_t flag = 0;
    captions_init(&captions, id);
    function_init(&f, ROW_FUNCTOR_VTABLE, open);
    fn(ADD_COMMAND_ROW)(ptr(builder), ptr(captions.buf), ptr(f.buf), ptr(&flag), 0, 0);
    captions_free(&captions);
}

/* The System builder, just before it becomes a dialog: the menu's rows go last, except that
 * Exit Game (in game) comes off first and goes back on at the end, as the game's own code adds
 * and pops these rows. */
static BB_GUEST_ABI void *system_finalize(void *out, void *builder, void *arg3) {
    unsigned char *last = builder_last_row(builder);
    const int move_exit = last && row_is_exit_game(last);
    if (move_exit) pop_last_row(builder);
    add_command_row(builder, ROW_SETTINGS, (const void *)settings_row);
    add_command_row(builder, ROW_ENHANCE, (const void *)enhance_row);
    if (move_exit) {
        Captions captions;
        GuestFunction f;
        captions_init(&captions, EXIT_GAME_ROW_ID);
        function_init(&f, ROW_FUNCTOR_VTABLE, bb_guest(BB_BN(EXIT_GAME_OPENER)));
        fn(ADD_EXIT_ROW)(ptr(builder), ptr(captions.buf), ptr(f.buf), 0, 0, 0);
        captions_free(&captions);
    }
    fn(SYSTEM_FINALIZE)(ptr(out), ptr(builder), ptr(arg3), 0, 0, 0);
    return out;
}

/* --- Install and frame -------------------------------------------------------------------- */

int bb_pc_menu_install(void) {
    const char *menu = getenv("BB_PC_MENU"), *assets = getenv("BB_PC_MENU_ASSETS");
    if (menu && !strcmp(menu, "0")) {
        puts("PC menu: off (BB_PC_MENU=0)");
        return 0;
    }
    if (!assets || strcmp(assets, "1")) {
        puts("PC menu: off - its sections and strings were not made (see the Enhancements lines)");
        return 0;
    }
    /* Slot [6] of the section-name table points at the name being opened; the loop that walks
     * the table goes to seven. Both are checked before anything is written. */
    static const unsigned char six = 6, seven = 7, empty[16] = {0};
    unsigned char slot[16] = {0};
    const uint64_t name = ptr(g_open_name);
    memcpy(slot, &name, 8);
    if (bb_patch(BB_BN(SECTION_COUNT_IMM), &six, &seven, 1)) {
        printf("PC menu: off - section loop: %s\n", bb_hooks_error());
        return 0;
    }
    if (bb_patch(BB_BN(SECTION_NAME_TABLE) + 6 * 8, empty, slot, 16)) {
        printf("PC menu: off - section table: %s\n", bb_hooks_error());
        bb_patch(BB_BN(SECTION_COUNT_IMM), &seven, &six, 1);
        return 0;
    }
    if (bb_hook_call(BB_BN(SYSTEM_FINALIZE_CALL), BB_BN(SYSTEM_FINALIZE), (void *)system_finalize)) {
        printf("PC menu: off - System list: %s\n", bb_hooks_error());
        bb_patch(BB_BN(SECTION_NAME_TABLE) + 6 * 8, slot, empty, 16);
        bb_patch(BB_BN(SECTION_COUNT_IMM), &seven, &six, 1);
        return 0;
    }
    for (size_t i = 0; i < COUNT(g_settings); ++i) seed(&g_settings[i]);
    for (size_t i = 0; i < COUNT(g_enhance); ++i) seed(&g_enhance[i]);
    g_installed = 1;
    puts("PC menu: System gains PC Settings and PC Enhancements");
    return 3;
}

void bb_pc_menu_frame(void) {
    if (!g_installed) return;
    for (size_t i = 0; i < COUNT(g_settings); ++i) push(&g_settings[i]);
    for (size_t i = 0; i < COUNT(g_enhance); ++i) push(&g_enhance[i]);
}
