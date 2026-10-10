/* SPDX-License-Identifier: GPL-2.0-or-later
 * PC features installed through the engine hooks (engine_features.h). The features themselves
 * (in-game PC menu, enhancements, keyboard and mouse) add themselves to these lists. */
#include "engine_features.h"
#include "engine_hooks.h"
#include "pc_menu.h"

#include <stddef.h>

typedef int (*Install)(void);
typedef void (*Frame)(void);

static const Install installs[] = {bb_pc_menu_install};
static const Frame frames[] = {bb_pc_menu_frame};

int bb_engine_install(void) {
    int hooks = 0;
    for (size_t i = 0; i < sizeof(installs) / sizeof(*installs); ++i)
        if (installs[i]) hooks += installs[i]();
    return hooks;
}

void bb_engine_frame(void) {
    for (size_t i = 0; i < sizeof(frames) / sizeof(*frames); ++i)
        if (frames[i]) frames[i]();
}
