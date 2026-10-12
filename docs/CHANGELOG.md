# Changelog

Windows releases of this fork, newest first. Each is on the
[releases page](https://github.com/Jgamrot/bloodborne_pc/releases) as `bbport-windows.zip`.
Planned work is in [ROADMAP.md](ROADMAP.md).

## Unreleased

- **Redesigned launcher.** One minimal layout: an icon rail (Home, Settings), a Home page
  with a single status line and one primary button (Play, or the fix that blocks it), and four
  rows for Graphics, Controls, Game files and Mods. Settings is a category list with search on
  the left and sectioned rows on the right. The setup wizard uses the same style. Builds on
  Codex's redesign branch; colours and type come from one theme file (`launcher/qml/Theme.qml`).
- **Redesigned in-game guide**, in the launcher's style: an icon rail, a Hunt page of menu
  rows, and settings laid out as label/control rows with switches. Every option is where it
  was and does the same thing.
- **Keyboard prompts included.** JinkiJinn's [PCBButtons](https://www.nexusmods.com/bloodborne/mods/460)
  key images ship with the port (its permissions allow it, with credit), so choosing Keyboard no
  longer asks for a download. They arrive switched off beside the Xbox prompts; the launcher and
  the guide can both choose them.
- **Guide controls.** Console-style movement: along the icon rail the page follows, Right enters
  the page and Left returns to the rail. L3 + R3 is shown as the way to close; on a keyboard,
  Insert opens and closes the guide. The mouse pointer shows as soon as the guide opens.
- **Mouse pointer in the game's menus.** Point at a row to move the game's cursor there, click
  to choose it (on an option's value, the side you click steps it), right-click to go back, and
  scroll with the wheel. It works in the title, System, the PC sections and the other menus
  with lists. While a menu is open the window shows the pointer; the mouse camera takes the
  mouse back when the menu closes. Mouse buttons do not trigger their gameplay bindings in
  menus. Off with *Mouse pointer in the game's menus* on the Controller page (`mouse_menu=0`,
  `BB_MOUSE_MENU=0`). Ported from [droogie's bbhost](https://github.com/droogie/bbhost)
  (`menu_pointer.cpp`, GPL-3.0-or-later).
- Developers: `overlay-preview` (an opt-in build target) renders the guide to an image
  without starting the game.

## 1.6.13 preview

A pre-release, like 1.6.12: the launcher's updater does not offer it until a full release
follows. It includes everything in 1.6.12.

- **Frame cap holds for every frame**, not only on average: no single frame comes faster than
  the cap any more, which made running slow down for a moment at 120 FPS
  ([#28](https://github.com/Supermedo/bloodborne_pc/pull/28), DUDUYG). `BB_STRICT_FRAME_CAP=0`
  restores the old limiter.
- **Sprinting no longer sticks at half speed above 90-120 FPS** after bumping into something:
  the game's blocked-movement check assumed 30 FPS and now uses the real frame time. Applied
  with every preset above 30 FPS, only when the game's code is exactly as expected
  ([#33](https://github.com/Supermedo/bloodborne_pc/pull/33), Aljoscha-Ide; the same fix as
  [#31](https://github.com/Supermedo/bloodborne_pc/pull/31), DUDUYG). `sprint_fix=0` in
  bbport.ini or `BB_SPRINT_FIX=0` turns it off. The 120 FPS cap stays the default.
- **A crash while a map loads** when a shader's descriptor table is unbound: the read now
  returns an empty descriptor, which the renderer already handles, instead of reading address
  zero (parsifal295's fix from [Mrsuss60's Windows port](https://github.com/Mrsuss60/bloodborne_pc_windows_port)).
- **Mods when started from Armoury Crate** (ROG Ally) and other launchers that start games with
  Windows' redirection trust policy: the game no longer stops with "the path cannot be traversed
  because it contains an untrusted mount point". When folder links are refused, the mod overlay
  uses real folders of hard-linked files instead (a few seconds longer at start, only then).
  Reported by Sjknight413 on r/BloodbornePC.
- **Update check on direct launches.** Starting the game straight from a shortcut or Steam now
  asks GitHub quietly, 20 seconds after the game starts and at most every 6 hours, and only
  remembers the answer. The next time the launcher opens, the Home banner shows the new version
  at once, with **Install update** beside **View release**. Off with the launcher's *Check for
  updates* switch, like every other check.
- **Graphics presets** on the Graphics page and in the setup wizard: Low, Balanced, High and
  Ultra, with a ★ on the one suggested for the detected graphics card (by video memory). They
  change only settings that cost GPU time (upscaling factor, model detail, SSAO, dynamic-light
  shadows, screen-space reflections, frame rate); the upscaler, output resolution and look-only
  effects stay as you set them. Balanced is the shipped defaults, and any change of your own
  shows as Custom. Each graphics setting now says what it does. After Lantern Launcher's idea
  (par274), not its values.
- **Ultrawide, 16:10 and other aspect ratios**: new output resolutions (2560x1080, 3440x1440,
  5120x2160, 5120x1440, 1280x800, portrait) and any WxH typed in the classic launcher. The
  camera is widened with the vertical field of view kept, the HUD stays centered at 16:9, and
  enemy health bars reach the wider edges. Only on the verified 1.09 game; aspect changes need
  a restart. `BB_ASPECT=0` turns it off. From
  [Supermedo #12](https://github.com/Supermedo/bloodborne_pc/pull/12) by FIocker, adapting
  [bbhost](https://github.com/droogie/bbhost)'s camera and UI fixes (GPL-3.0-or-later).

## 1.6.12 preview

A pre-release: the launcher's updater does not offer it until a full release follows.

- **Keyboard button prompts.** Choose Keyboard on the launcher's Controller page, or Keyboard
  keys in the Insert/L3+R3 overlay's Mods tab, beside PlayStation and Xbox; either place can
  switch back. The first time, the launcher imports the key images from your own download of
  [PCBButtons](https://www.nexusmods.com/bloodborne/mods/460) (not included) and binds the keys
  they show. If you rebind keys, menu and tutorial text names your keys instead (after bbhost's
  key prompts; started by Codex).
- **Lamps: Level Up and Storage** (`pc_enhanced_lamps`, off by default). Every lit lamp also
  offers the game's own Level Up and Storage screens once the Doll and the storage box have
  been used. Works alongside Rebirth, which edits the same Nightmare archive.
- **Fixes from Supermedo pull requests**, with their authors' commits:
  - the black character preview in character creation: the texture cache no longer copies
    stale game memory over images the GPU drew
    ([#36](https://github.com/Supermedo/bloodborne_pc/pull/36), yumlevi's fix);
  - a crash while an area loads, in shader walkers loaded from the pipeline cache
    ([#29](https://github.com/Supermedo/bloodborne_pc/pull/29), DUDUYG);
  - video memory growing with every area visited: images that cannot be freed no longer block
    the collector, and idle textures go after 20 s (`BB_GC_IDLE_SECONDS`)
    ([#32](https://github.com/Supermedo/bloodborne_pc/pull/32), parsifal295, from bbport 0.3).
- **Boss rematches, experimental:** a Cleric Beast rematch at its lamp for testing in play
  (`BB_PC_REMATCHES=1` in the launcher's Advanced overrides; no toggle yet). The permanent
  defeat flag and first-kill rewards are left alone. Not offered to players until tested.
- A tidier README, this changelog and a roadmap.

## 1.6.11

- Achievements is back in the game's own System menu; the six custom sections are
  Achievements, PC Graphics, PC Display, PC Effects, PC Settings and PC Gameplay.
- PC Enhancements (Hunter's Dream mirror, two invaders, Rebirth) moved to the launcher's Mods
  page.
- The Online page links to shadNet registration.

## 1.6.10

- Mouse camera: click the game window and the mouse turns the camera 1:1 (imedved's camera,
  ported by mcrib884 and Jhony-Jhansen). Settings on the Controller page.
- DLC folder for base-game dumps ([ayozetr, #26](https://github.com/Supermedo/bloodborne_pc/pull/26)).
- Upstream fixes, with their authors' commits: the Intel 12th-gen attack crash (DarkSpartan),
  texture eviction under video-memory pressure (gtggdd4mzb-source, yumlevi), a GPU command
  race (andrecabralgit), UTF-8 configuration (noatdk), the 1.09 FPS physics address
  (FIocker), quiet stereo audio (andrecabralgit), shadNet summons (jpramosjp) and mods on
  Windows with Developer Mode (gStone).

## 1.6.9

- The L3+R3 overlay's settings in the game's own Options menu, with fullscreen while playing
  and Restart to Apply; menu engine ported from [bbhost](https://github.com/droogie/bbhost).
- Achievements as a System row on the title screen and in game.
- Rebirth in the Nightmare at the Altar of Despair (`pc_rebirth`, off by default).
- Keyboard and mouse alongside the gamepad, with rebindable keys (`key_<action>=` in
  `bbport.ini`) and mouse-wheel actions.

## 1.6.8

- Display buffers are released before the game hears a flip has completed, fixing the
  intermittent "Out of order flip IRQ" assertion.

## 1.6.7 and earlier

- Qt Quick launcher with a first-run setup wizard, direct launch (`Play Bloodborne.exe`), an
  installer for your own ZIP or PKG dump, and an update check.
- Xbox button prompts by Dommo bundled in every package.
- Insert/L3+R3 guide with Home, Achievements, Settings and Mods tabs, local trophy history and
  achievement toasts (names and scores need your own trophy key in `user\keys.json`).
- Optional Hunter's Dream mirror and two invaders (`pc_mirror_appearance`, `pc_two_invaders`).
- English in-game settings menu ([#13](https://github.com/Supermedo/bloodborne_pc/pull/13)) and
  working checkboxes and sliders ([#19](https://github.com/Supermedo/bloodborne_pc/pull/19)).
- Death respawns skip the fixed 12-second wait (after
  [AwesomeObserver](https://github.com/AwesomeObserver/bloodborne_pc/blob/windows/docs/respawn-loading-windows.md));
  `BB_RESPAWN_DELAY_FIX=0` restores it.
- Shader preload reuses loaded modules; motion vectors keep images for several render sizes,
  addressing the [water-area slowdown](https://github.com/deadinside28/bloodborne_pc/issues/30).
