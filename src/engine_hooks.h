/* SPDX-License-Identifier: GPL-2.0-or-later
 * Guarded hooks into the loaded CUSA03173 1.09 image, for the PC features (in-game menu,
 * enhancements, keyboard and mouse). Approach and addresses after droogie's bbhost
 * (https://github.com/droogie/bbhost, GPL-3.0-or-later).
 *
 * Addresses are image offsets: Binary Ninja / PS4 virtual address minus 0x400000 (patches.py).
 * Every hook first checks the original bytes and refuses, leaving the image unchanged, when
 * they differ - another layout, another game version or a patch already there.
 *
 * Host functions run on the game's own threads with the game's calling convention, so they
 * are declared BB_GUEST_ABI (System V). */
#ifndef BB_ENGINE_HOOKS_H
#define BB_ENGINE_HOOKS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BB_GUEST_ABI __attribute__((sysv_abi))

/* Image offset of a Binary Ninja address (eboot at its preferred 0x400000 base). */
#define BB_BN(address) ((uint64_t)(address) - 0x400000u)

/* Reserves the stub area within reach of rel32 jumps from the image. The image must still be
 * writable. Returns 0 on success. */
int bb_hooks_init(unsigned char *image, size_t size);
/* Makes the stubs executable and read-only; no hooks can be added after it. */
void bb_hooks_seal(void);
/* Guest pointer of an image offset (to call a game function from a hook). */
void *bb_guest(uint64_t offset);

/* `at` holds `call rel32` (E8) or `jmp rel32` (E9) to `target`: sends it to `host` instead.
 * `host` has the callee's signature; it may call the original through bb_guest(target).
 * Returns 0, or -1 (nothing changed) when the instruction is not that. */
int bb_hook_call(uint64_t at, uint64_t target, void *host);

/* Runs `host` with the function's arguments (integer and vector registers preserved) each time
 * the function at `at` is entered, then the function itself. `prologue` is the first `n` (>= 5)
 * bytes expected there; they must not be RIP-relative, since they run from the stub. */
int bb_hook_entry(uint64_t at, const unsigned char *prologue, size_t n, void *host);

/* Why the last hook was refused (static text). */
const char *bb_hooks_error(void);

/* BB_ENGINE_HOOKS=0 turns every engine hook off. */
int bb_hooks_enabled(void);

#ifdef __cplusplus
}
#endif

#endif
