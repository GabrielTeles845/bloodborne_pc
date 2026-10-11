# Enhanced lamps and boss rematches: native Windows implementation notes

Status: research only. Neither feature is installed, enabled, or presented as a working toggle. This note is based on Bloodborne 1.09 CUSA03173 from the player's installed dump, the port's own archive/ESD tools, and the author's public [Bloodborne Enhanced description](https://www.nexusmods.com/bloodborne/mods/19) and [script directory](https://github.com/colorsolid/Bloodborne-Enhanced-Directory). The author's scripts were inspected read-only; none are copied into this repository.

## Rights and packaging boundary

The mod page forbids reuploading its files, modifying them without permission, and using its assets without permission. A public source repository does not grant a code or asset license. The Windows ZIP must not include the mod, its edited game files, map object, text, icons, scripts, patcher, or repackaged binary data. Implement the requested gameplay independently, using the player's own dumped files at setup time and the fork's own generated-file mechanism. If the project later wants to incorporate any of the author's code or assets, obtain the necessary permission first and record it before shipping.

## Lamp findings

- The author's lamp menu invokes the game's native level-up action (`OpenSoul`) and storage action (`OpenRepository`). It waits for the corresponding person menu to close before returning to the lamp menu. First-use tutorial flags for these interfaces are handled separately. An independent port should keep the vanilla level-up and storage implementation as the source of truth; it should not edit character stats, inventory, or save records itself.
- The existing fork can parse, edit, and serialize Bloodborne's DCX/BND4 and long-format ESD talk scripts (`scripts/fromsoft.py`, `scripts/esd.py`). Rebirth already uses this path to add a native talk-menu item from the player's own dump. Lamp support needs a distinct rewrite that retains the normal activation/warp paths and any map-specific branches.
- The installed CUSA03173 dump contains lamp talk archives under `dvdroot_ps4/script/talk`. One representative ordinary lamp, `m22_00_00_00/t220000.esd`, has ten state groups and 68 parsed states. It round-trips with the fork's parser. The reference mod's public sources show lamp talk variants across the base-game areas, DLC, and the shared chalice scripts. A single patched script is insufficient.
- The reference adds a menu to activated lamps and retains normal lamp behavior. The requested initial scope is only **Level Up** and **Storage**. Do not silently bring along rest/reawaken, warp changes, shops, kindling, appearance editing, or other Enhanced features.

## Rematch findings

- The author's implementation spans a common event script, the Hunter's Dream event script, map-specific scripts, talk scripts, messages, and Hunter's Dream map data. Its public source has 18 event files, 17 of which refer to rematches. A rematch is not just a menu flag or a teleport.
- The standard flow checks the permanent defeated-boss flag before offering a boss. On selection it sets a separate trigger, then an event marks rematch active, applies the transition and moves the respawn point near that boss. Map-specific events temporarily make the defeated boss and its arena active. On completion or subsequent load, cleanup restores the permanent completion state and related progression flags. An independent implementation needs both success and interrupted/death recovery paths; a save must never retain a false undefeated state.
- The original uses a game effect to deduct one Insight when a rematch begins. The requested zero-Insight case means the interaction must remain available with zero and never underflow. Deduct once per start, including starts through the Broken Lamp; define and test whether automatic retry is a new start before enabling it.
- The Broken Lamp requires an actual map entity and talk interaction in Hunter's Dream, as well as its raised-garden placement. The reference uses a distinct talk script and enables its NPC/object conditionally. This must be generated from the player's own `m21_00_00_00.msb.dcx` and related files; no reference map asset can be bundled.
- Chalice dungeons use separate shared event/talk paths. Their bosses must be listed only at the respective local lamp, never in the Broken Lamp list. Runtime dungeon instance identity and completed flags must be verified before enabling the action.
- The named Distorted Memories need map-specific encounter logic, not merely two boss spawns: Gascoigne, Ludwig, and Orphan have simultaneous phase actors; Wet Nurse holds the clone sequence active; Gehrman and Moon Presence fight together and are accessible only through the Broken Lamp. The reference also guards transformation scripts, music, cameras, arena state, boss defeat rewards, death retry, and progression cleanup. Inspect and reproduce each transition against the original behavior, with no approximation labeled as complete.

## Delivery sequence and gates

1. Add dump-version and archive/ESD compatibility checks for every affected game file. Keep all generated outputs separate from the original dump and other mods. On any unknown hash or conflicting rewrite, leave that file untouched and report why.
2. Implement the two lamp menu actions in a small independently authored rewrite. First prove that an ordinary lamp, boss lamp, DLC lamp, and chalice lamp preserve activation and warp behavior, and that leveling/storage use the same native state as the Doll/box. Verify save integrity on a copied save.
3. Add ordinary rematches one boss/map at a time. Before exposing a boss, verify the defeat flag, start point, fog/arena, rewards, death path, and post-rematch progression. Include recovery if the game exits during a rematch.
4. Generate the Broken Lamp placement and native menu from the player's own Dream files; verify location, Gehrman/Moon restriction, and exclusions for chalice bosses.
5. Implement each requested Distorted Memory using the original encounter as a behavioral reference and the player's own game files as inputs. Test each phase, death/retry, reward, and flag restoration separately.
6. Only after installation and runtime tests pass, add `Enhanced Lamps`, `Boss Rematches`, and `Distorted Memories` to the setup wizard/mod manager. Each toggle needs a complete file dependency set, conflict handling, and a clean uninstall path.

## Validation required before a working release

- Baseline save snapshot and post-use comparison: level, attributes, blood echoes, inventory, storage, boss flags, headstone/lamp flags, and unlocked areas.
- New and existing characters; zero and positive Insight; first and repeated rematch; death, quit/reload, and victory; offline and network-host behavior.
- Ordinary, DLC, and chalice lamps, including each map's menu variant and original warp path.
- All five requested distorted encounter types with visible phase/clone behavior.
- Mod-overlay interaction with the fork's existing PC menu, Dream mirror, Rebirth, and other user mods.

The current 1.6.10 release contains none of these Enhanced features.
