#ifndef BB_RESPAWN_PATCH_H
#define BB_RESPAWN_PATCH_H

#include <stddef.h>
#include <string.h>

enum { BB_RESPAWN_UNSUPPORTED, BB_RESPAWN_APPLIED, BB_RESPAWN_ALREADY_APPLIED };

/* CUSA03173 1.09: the player-death flag initializes a minimum loading time to
 * 12 seconds. The world-loading step tests this countdown independently of
 * character readiness. Change only its initialization; keep the character
 * readiness branch, resource-loading steps and multiplayer timeout intact.
 * Offsets refer to the loaded image, not SELF/ELF file offsets.
 */
static inline int bb_patch_respawn_delay(unsigned char *image, size_t size) {
    static const unsigned char init[] = {
        0x41,0x80,0xbc,0x24,0xe4,0x00,0x00,0x00,0x00, /* cmp [r12+e4],0 */
        0x74,0x0a,
        0xc5,0xfa,0x10,0x05,0x5c,0xf5,0xfe,0x02, /* vmovss xmm0,[12.0] */
        0xeb,0x04,0xc5,0xf8,0x57,0xc0,
        0xc4,0xc1,0x7a,0x11,0x84,0x24,0x78,0x02,0x00,0x00, /* countdown */
        0x41,0xc7,0x84,0x24,0x7c,0x02,0x00,0x00,0x00,0x00,0x70,0x41 /* 15s */
    };
    static const unsigned char ready[] = {
        0xc5,0xf8,0x2e,0xca,0x0f,0x87,0x53,0x06,0x00,0x00,
        0x45,0x84,0xff,0x0f,0x84,0x4a,0x06,0x00,0x00
    };
    static const unsigned char twelve[] = {0x00,0x00,0x40,0x41};
    static const unsigned char zero[] = {0xc5,0xf8,0x57,0xc0,0x0f,0x1f,0x40,0x00};
    /* The linked image can place the same code/data at either of these positions,
     * depending on the port's linker layout. Verify the full instructions before
     * changing either layout. */
    static const struct { size_t init_at, ready_at, twelve_at; } layouts[] = {
        {0x1938b59, 0x193a581, 0x49280c8},
        {0x208fe59, 0x2091881, 0x507f3c8}
    };
    if (!image) return BB_RESPAWN_UNSUPPORTED;
    for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); ++i) {
        const size_t init_at = layouts[i].init_at;
        const size_t instruction_at = init_at + 11;
        unsigned char expected[sizeof(init)];
        if (size < init_at + sizeof(init) || size < layouts[i].ready_at + sizeof(ready) ||
            size < layouts[i].twelve_at + sizeof(twelve)) continue;
        if (memcmp(image + layouts[i].twelve_at, twelve, sizeof(twelve)) ||
            memcmp(image + layouts[i].ready_at, ready, sizeof(ready))) continue;
        if (!memcmp(image + init_at, init, sizeof(init))) {
            memcpy(image + instruction_at, zero, sizeof(zero));
            return BB_RESPAWN_APPLIED;
        }
        memcpy(expected, init, sizeof(init));
        memcpy(expected + instruction_at - init_at, zero, sizeof(zero));
        if (!memcmp(image + init_at, expected, sizeof(expected)))
            return BB_RESPAWN_ALREADY_APPLIED;
    }
    return BB_RESPAWN_UNSUPPORTED;
}

#endif
