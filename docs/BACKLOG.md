# Backlog

Work agreed but not done yet, in rough order. Each item keeps the requirements as they were
asked for; investigate before building, and do not expose a toggle for anything that does not
actually work.

## In progress: bbhost features (droogie/bbhost, GPL-3.0-or-later)

Branches build on each other: `feature/f0-engine-hooks` → `feature/f2-pc-enhancements` →
`feature/f1-pc-menu`.

- **Rebirth in the Nightmare** (F2, last piece): the Altar of Despair's talk-script rewrite
  (bbhost `engine/esd.cpp`, `engine/rebirth_script.cpp`) and the runtime handshake
  (`engine/rebirth.cpp`: event flags 72500390-93, player level/stats/echoes, CharaInitParam and
  CalcCorrectGraph rows, the level-up menu's steps). Its altar texts are already in the PC
  menu's message files (14000332-14000336).
- **Mirror preview frame**: the appearance editor at the mirror opens without its character
  preview; bbhost opens ChrMake_BG around it (`engine/change_appearance.cpp`, `menu_steps`).
- **F3 keyboard and mouse**: last-used device switching (today the keyboard only works with no
  gamepad), mouse camera (bbhost's hook path, not its decompiled camera), pointer in menus
  (`engine/menu_pointer.cpp`), rebindable keys with a Key Bindings section, key prompts
  (`engine/key_prompts.cpp`). Keep controller play and the Xbox prompts working.
- **F1 graphics settings** in the in-game menu (see "Native menus" below): AO strength, shadow
  distance, fog, bloom, saturation, vignette, FOV, camera distance/height are portable engine
  hooks; AA/SSAO/DoF/CA/motion blur/frame cap/resolution map to settings the base already has;
  ultrawide needs renderer work.
- **Unverified in play**: two invaders (needs every player in the session to have it on).

## Launcher updates

- A direct launch skips the update check, so players who never open the launcher never hear of
  a new release. Check quietly in the background during a direct launch and keep the result
  for the next time the launcher (or the launch toast) shows.
- The Home banner offers only "View release"; add "Install update" there too (today it is on
  the Advanced page).

## Graphics presets (after Lantern Launcher)

Reference: https://github.com/par274/lantern-bloodborne-launcher - study its source, interface,
screenshots and features; adopt the presets concept, not its values.

- Profiles: Low / Performance, Balanced, High / Quality, Ultra, Custom.
- First establish which settings this native port really exposes (output and internal render
  resolution, upscaler and preset, FPS cap, frame pacing / presentation mode, VSync, anisotropic
  filtering, the game effects); do not assume ShadPS4 settings apply here.
- Detect the GPU and relevant capabilities (bb-gpu-capabilities already reports the GPU, DLSS
  and FSR 4 support) and recommend a preset.
- Individual overrides switch the profile to Custom; explain every option in plain English; warn
  about expensive settings; remember preferences; apply reliably; say when a restart is needed.
- Integrate with the setup wizard and the existing configuration (bbport.ini, settings.json).

## Native menus instead of the L3+R3 overlay

Done on `feature/native-menus` (checked in game, 2026-10-10): System offers Achievements, PC
Graphics, PC Display (with live Fullscreen), PC Effects, PC Settings, PC Gameplay, PC Cheats, PC
Enhancements and Restart to Apply, on the title screen and in game; the title menu has Quit
Game. Approach: genuine game menus extended through hooks and generated movie/message assets
(`src/pc_menu.c`, `scripts/menu_assets.py`). Sections do not scroll (six rows each); pick lists
sit below toggles and blank the rows under them while open. The L3+R3 overlay still works.

Still open:

- Achievement icons in the game's menus: option rows show no images. Either put the trophy
  icons into the menu movie's textures (Scaleform image swap) or draw them over the section
  from the renderer; both are larger jobs. The overlay keeps icons and the Xbox-style toasts.
- Overlay functions not moved yet: button prompts (Xbox / PlayStation), the mods list and its
  order, the FSR 4 checks, the reactive mask and debug views, sharpness amount.
- An Achievements entry in the in-game pause menu itself (a separate grid; not investigated).
- Retire the overlay's duplicated settings once the native menus have proven stable.

## Bloodborne Enhanced features

Reference: https://www.nexusmods.com/bloodborne/mods/19 - examine how it implements each
feature; it targets PS4 files and ShadPS4 workflows, this port is native. Check the mod's
permissions before reusing any of its files or data; otherwise reimplement from its behaviour.
Offer through the setup wizard and mod manager (Gameplay Enhancements: Enhanced Lamps, Boss
Rematches, Distorted Memories) only once installation and runtime behaviour really work.

### Enhanced Lamps

- Level up directly at lamps; open the storage box directly from lamps.
- Through the existing lamp menu, fully native; preserve vanilla progression, character state,
  storage and normal lamp functions.

### Boss Rematches

- Rematch bosses already defeated on the current character, without NG+, from their boss lamps.
- Also through the Broken Lamp in the Hunter's Dream, placed in the raised garden area near the
  tree, back and to the right of the spawn point.
- Chalice dungeon bosses only from their own lamps.
- Starting a rematch consumes one Insight when the player has any, and is still allowed with
  zero Insight.
- Preserve the original progression and completed-boss flags.
- Distorted Memories: Father Gascoigne, Ludwig and Orphan of Kos with both phases at once;
  Gehrman + Moon Presence (Broken Lamp only); Mergo's Wet Nurse with its clone phase always on.
  Match the mod's exact encounter behaviour.
