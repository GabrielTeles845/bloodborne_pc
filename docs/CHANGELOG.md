# Changelog

Windows releases of this fork, newest first. Each is on the
[releases page](https://github.com/Jgamrot/bloodborne_pc/releases) as `bbport-windows.zip`.
Planned work is in [ROADMAP.md](ROADMAP.md).

## Unreleased

- **Mods when started from Armoury Crate** (ROG Ally) and other launchers that start games with
  Windows' redirection trust policy: the game no longer stops with "the path cannot be traversed
  because it contains an untrusted mount point". When folder links are refused, the mod overlay
  uses real folders of hard-linked files instead (a few seconds longer at start, only then).
  Reported by Sjknight413 on r/BloodbornePC.

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
