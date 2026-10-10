/* SPDX-License-Identifier: GPL-2.0-or-later
 * The in-game PC menu (pc_menu.c): System > PC Settings and PC Enhancements. On when its assets
 * were made (BB_PC_MENU_ASSETS=1, run.py); BB_PC_MENU=0 turns it off. */
#ifndef BB_PC_MENU_H
#define BB_PC_MENU_H

/* Installs the menu's patches and hook; returns how many went in (0: off, image unchanged). */
int bb_pc_menu_install(void);
/* Once a frame: the rows' changes become bbport.ini settings. */
void bb_pc_menu_frame(void);

#endif
