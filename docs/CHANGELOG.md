# Changelog

Windows releases of this fork, newest first. Each is on the
[releases page](https://github.com/Jgamrot/bloodborne_pc/releases) as `bbport-windows.zip`.
Planned work is in [ROADMAP.md](ROADMAP.md).

## 1.9.6

Everything from 1.9.4 and 1.9.5 as a full release, plus a clearer way to choose how you play.

- **Choose how PLAY starts the game, on the Home page:** *Offline*, *Online · The Hunter's
  Dream* or *Online · Seamless co-op*. The button says which (Play offline / Play online / Play
  co-op), and a line under it says what will happen (signed in as..., hosting on..., joining your
  host...). In co-op mode PLAY starts co-op itself. When a mode still needs setting up, PLAY opens
  the right settings page.
- **Settings → Online is The Hunter's Dream only:** the Discord sign-in, UPnP and Test connection.
  The shadNet server account and address are gone; the shadPS4 team's public server refuses this
  port, and an old "Play online" with a shadNet server now starts offline.
- **The Hunter's Dream sign-in opens its website with the code filled in**, as bbhost does: sign in
  with Discord there and approve it. (Before, the code had to be typed, and the page's "code from
  the game" box is for something else.)

## 1.9.5 experimental (co-op network test)

A pre-release to test seamless co-op on the new network code; it includes everything listed under
1.9.4, which was not released on its own. Host and guests all need 1.9.5: it cannot connect to
1.9.3 or older.

- **Network code from shadPS4 0.20**, as in Supermedo's port (Awaken's adaptation): the network,
  PSN and shadNet libraries replace the shadp2p-based ones. This is what The Hunter's Dream team
  asked ports to use.
- **Co-op hosts run a fixed co-op server.** BBSC's server (Wozzardman's shadnet-p2p) ignored the
  new network code's connection-setup packets, so the bells could not bring players together.
  The port now ships that server with one fix (it answers both packet formats), built from source
  by `packaging/windows/build_coop_server.sh`. *Start hosting* uses it automatically and keeps the
  registration key, so invite codes stay valid; co-op accounts are made again on first *Play co-op*.
- **The Hunter's Dream with Discord sign-in, on the Online page:** choose The Hunter's Dream as the
  co-op pool, then *Sign in with Discord* and approve the code on its website. Test build: we are
  checking it with The Hunter's Dream team. `BB_HUNTERS_DREAM=0` hides it.

## 1.9.4 experimental (not released on its own)

Fixes from Supermedo's upstream that this fork still lacked, each checked against our own code first,
and The Hunter's Dream online backend, still hidden.

- **No more stop when the graphics driver refuses a pipeline** ("Failed to create graphics pipeline:
  ErrorUnknown", code 23, mostly AMD): its draws are skipped and it isn't cached. Motion-vector
  pipelines are no longer preloaded from the cache, the root cause of most of those refusals.
- **Crash fixes:** the texture cache's image list no longer moves in memory while an image is in use
  (a crash in FindDepthTarget); the GPU command recorder always has a chunk to write into; a garbage
  GPU command header drops the rest of that buffer instead of stopping the game.
- **AMD RX 6000 (RDNA2):** object motion no longer loses the device; it is off on AMD cards before
  RDNA3 (`BB_OBJECT_MOTION=1` to test it).
- **Physics at 60, 90 and uncapped frame rates:** the FPS presets now set the physics step to the
  right value, not the patch file's 1/15 or 1/30 of a second (ragdolls, cloth, falling).
- **Video memory full:** an allocation the driver's budget refuses is made past it instead of
  stopping the game; old render targets of areas left behind can be freed.
- **Fewer texture-upload checks:** only images the GPU may write keep a hash of their upload.
- **Shader resource walker:** reads made ahead of the game fall back to their recovery point instead
  of permanently changing the walker.
- **Display:** no title-menu flicker above 1080p (parsifal295, #56); the loading screen's item
  picture stays at outputs other than 1080p (bmy via parsifal295, #62).
- The GPU, its video memory and the driver are named at the top of every log.
- **The Hunter's Dream:** Supermedo's port of Awaken's backend
  (bbhost's protocol, GPL-3): Discord sign-in by code on the Online page, the game's server
  functions over HTTP to the game server only. Not offered until its team has agreed. This build
  reports itself as `bbport-jgamrot <version>`. Seamless co-op keeps its own network code and
  never uses this backend.

## 1.9.3 experimental

- **Co-op bells: BBSC now gives its Hunt Bell and Kin Bell.** BBSC replaces the vanilla bells with
  its own co-op kit, but only after backing up the save, and it couldn't find this port's saves
  ("BBSC kit: no save folder found to back up"), so nobody got the kit and the guest couldn't
  ring a bell. Co-op launches now tell BBSC where the save is.

## 1.9.2 experimental

A pre-release: the launcher does not offer it as an update. Fixes for seamless co-op, found in
the first test of 1.9.1:

- **The game no longer exits (code 3) when it starts with seamless co-op.** While the game
  loaded, every page of its memory was briefly made unreadable before each part got its own
  protection. BBSC checks some of that memory from another thread every few milliseconds, and
  a check in that moment ended the game (`Fault: read access ... at bbsc.dll+0x1933`). Only
  memory the game doesn't use is made unreadable now. For co-op, the game code also stays
  writable, as BBSC expects from shadPS4.
- **BBSC finds the host's server.** It looks for the host's address in its own places, and
  1.9.1 put it somewhere else, so warps and world sync had no server ("Relay: no game server
  address"). Co-op launches now give BBSC the address directly.
- **Stop hosting stops the whole server.** BBSC's game server runs a second copy of itself, which
  kept running after Stop hosting or closing the launcher and kept the co-op ports busy. Both
  copies stop now, and Start hosting first ends any that were left over.

## 1.9.1 experimental

A pre-release: the launcher does not offer it as an update. It is 1.8.3 (with everything in
1.8.2) plus the seamless co-op of 1.9 and boss rematches for every boss with a lamp.

- **Seamless co-op is how to play together for now.** The public shadPS4 server
  (srv.shadps4.net) signs this port out since its 14 October update, whatever version the port
  reports (1.8.3's `shadPS4 Jgamrot/v0.20.0` too); shadNet's stock rules would accept it, so this
  is that server's own setting. Co-op doesn't use it: one player hosts the servers on their own
  PC. The Online page now says this.
- **Seamless co-op (experimental):** Settings > Co-op sets up and runs
  [BBSC](https://github.com/Classmg22/BBSC) (Bloodborne Seamless Co-op by CLS_AMG; servers from
  Wozzardman's shadnet-p2p): one co-op session through deaths, lamps, areas and bosses, each hunter
  keeping their own character. One player hosts the server on their own PC; both use
  [Tailscale](https://tailscale.com). *Set up co-op* downloads BBSC's official packages and checks
  them against its published checksums; *Start hosting* opens the firewall for Tailscale and the
  home network only (one Windows prompt, the first time), starts the servers and gives an invite
  code; the guest pastes it; *Play co-op* backs up the saves and starts the game with BBSC, pointed
  at the host for that session only. The game runs on this release's engine, so 1.8.2's video
  memory and menu fixes apply in co-op too. Not yet played through on BBport: please report
  problems with `user\last_run.log` and BBSC's logs.
- **Boss rematches (experimental, off):** *PC gameplay enhancements* has a toggle for rematches
  at the bosses' own lamps: once a boss is defeated, the lamp its defeat lit offers "Rematch ..."
  next to Level Up and Storage, for one Insight when you have any. All 19 bosses with a lamp of
  their own: Cleric Beast, Father Gascoigne, Blood-starved Beast, Vicar Amelia, the Witch of
  Hemwick, Darkbeast Paarl, Shadow of Yharnam, Rom, the One Reborn, Martyr Logarius, Amygdala,
  Celestial Emissary, Ebrietas, Mergo's Wet Nurse, Ludwig, Living Failures, Lady Maria, Laurence
  and Orphan of Kos (not Gehrman, the Moon Presence or Micolash, who have no lamp). The fight is
  staged as the first meeting; winning, dying or leaving puts the arena back. The save's
  defeated-boss flags are never cleared and a rematch kill gives no first-kill items, trophy or
  progression again. Played through on the Cleric Beast; the other 18 are made the same way and
  checked against the game's scripts, but not yet played: back up your saves first. Boss data
  after droogie's bbhost (boss rush plugin).
- **Fixed with it:** with rematches on, Central Yharnam's other lamps lost Level Up and Storage;
  with the two invaders on as well, the rematch layer undid their change in five late maps.

## 1.8.3

- **Version report changed; online still refused.** Since 14 October the shadNet server checks
  the version a client reports and signed 1.8.2 out ("This shadNet server doesn't accept your
  shadPS4 build"). 1.8.3 reports itself the way shadPS4's own fork releases do,
  `shadPS4 Jgamrot/v0.20.0`, and keeps the port's version in the log. The public server refuses
  that too (see 1.9.1).

## 1.8.2

- **Video memory no longer runs out on 4 GB cards** ([#3](https://github.com/Jgamrot/bloodborne_pc/issues/3)).
  Memory given to the game's buffers was never released, so usage grew with every area until
  `ErrorOutOfDeviceMemory` after an hour or two. When video memory gets short, buffers unused
  for a few seconds now give theirs back; cards with room to spare behave as before.
  `BB_BUFFER_GC=0` turns it off. *Settings → Graphics → Video memory* can start clearing
  earlier (a pressure target, not a hard cap; Automatic by default).
- **Opening PC Graphics, PC Display and the other PC sections no longer crashes with button
  prompt mods** ([#3](https://github.com/Jgamrot/bloodborne_pc/issues/3)). The menus' fixed
  Scaleform memory is mostly taken by `menu/common.tpf`; the Xbox 360 Prompts mod's larger copy
  left too little for a PC section (a crash at `0xdcf79`). The menus get 8 MB more
  (`BB_MENU_HEAP_EXTRA_MB=0` keeps the original size).
- **A damaged `.dcx` file no longer stops the game** ([#8](https://github.com/Jgamrot/bloodborne_pc/issues/8)):
  the feature that needs it turns off with a log line, and a failing PC enhancements or shader
  pre-build step lets the game start as with that step off.
- **Installer: a renamed plain 1.09 update PKG is accepted** (a browser's `(1)`, store-style
  names). Only names that look like the 60 FPS variants are refused, each step says whether it
  wants the base game or the update, and picking the same PKG twice is refused.
- **Touchpad gestures:** each touch now gets its own ID and a hold time, as a DualShock 4
  reports them, instead of zero for every sample.
- **Missing walls and floors you could walk through** ([#6](https://github.com/Jgamrot/bloodborne_pc/issues/6)).
  Now and then a piece of a map loaded without its model or collision, and it stayed broken
  until the game was closed. The game reads its archives two ways at once: one thread moves to a
  position and reads, another reads at a given position. On Windows the second kind moved the
  file's position and put it back, so a read from the first thread could land in the wrong place
  and load garbage. Those reads now take turns on each open file (Windows already ran them one
  at a time). A stress test that failed every run before now passes; confirming it in play
  needs time, since the bug was rare.

## 1.8.1

- **Online works again.** Since 12 October the shadNet server signs out clients that do not
  report their version straight after signing in, then refuses their logins for two minutes:
  every login of the port failed and the game showed it could not connect. The game now reports
  itself (`shadPS4 Jgamrot/bloodborne_pc v1.8.1`), as shadPS4 does; if a server ever turns it
  away, the log shows the server's message.
- **Trophy names after choosing a saves folder.** The setup wizard offers *Import trophy keys*
  before *Choose a saves folder*; the key stayed in the old folder and trophy names were missing.
  Choosing a saves folder now copies `keys.json` over when the new folder has none.

## 1.8

A full release: the launcher offers it as an update. It is 1.7 plus the following.

- **Shader pre-build.** Shaders the game has been seen to use are built while the game starts
  ("Preparing shaders: N%" in the window title), not the first time an area or effect appears.
  The shipped catalog (`shader_catalog/`) lists which shaders and pipelines the game uses, never
  game code: each install builds them from its own game files, for its own GPU and driver, and
  keeps them, so later starts take a second or two. Every catalog entry was checked to build
  byte for byte like the game's own. This first catalog covers the game up to the
  Blood-starved Beast; later releases extend it. On a fresh install loading into Central
  Yharnam it cut the shaders compiled during play from 479 to 2 and the worst frame from 639 to
  181 ms (RTX 4060 Laptop). On AMD and Intel GPUs most fragment shaders do not match yet and
  still compile in play as before. Off with `BB_SHADER_PREBUILD=0`.
- **Keyboard movement smoothing.** Holding W and then pressing A or D no longer stops the
  character for a moment: the stick swings to the new direction over a few frames
  ([#4](https://github.com/Jgamrot/bloodborne_pc/issues/4), after MKB2Controller's setting,
  suggested by tmntanno). Settings > Controls > Keyboard, *Off* to switch it off
  (`move_smoothing=100`).
- **GPU readbacks: Precise no longer stays on a black screen at start**
  ([#2](https://github.com/Jgamrot/bloodborne_pc/issues/2), reported by HerOisPerO). Precise
  waits for the GPU whenever the game reads memory the GPU wrote; the port's draw recording and
  copy threads deadlocked against it. They are off under Precise, so it costs some speed; it
  should catch the stale face vertices (FaceGen, the "vertex explosions") that Relaxed can miss.
- **No crash out of bounds.** A texture descriptor with an impossible format (seen after
  falling out of the map) no longer stops the game.
- **Video memory:** the texture cache starts evicting at 80% of a discrete GPU's budget, not
  40%, which removed constant evictions and uneven frame times in Yharnam on 8 GB cards.
- **The last three game logs are kept** (`last_run.log`, `last_run.1.log`, `last_run.2.log`),
  so starting again after a crash keeps the log of the run that crashed.

## 1.7

A full release: the launcher offers it as an update. It includes everything in the 1.6.12 and
1.6.13 previews below. Shader pre-building and the shadPS4 catch-up come in 1.8; boss
rematches stay a hidden experiment (`BB_PC_REMATCHES=1`) until tested.

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
  menus. Off with *Mouse pointer in the game's menus* in Settings > Controls > Mouse (`mouse_menu=0`,
  `BB_MOUSE_MENU=0`). Ported from [droogie's bbhost](https://github.com/droogie/bbhost)
  (`menu_pointer.cpp`, GPL-3.0-or-later).
- **The game no longer stops at start when DLSS cannot load** although the NVIDIA driver
  advertised it: device creation is retried without DLSS and FSR 3.1 is used; your DLSS choice
  is kept for the next start ([deadinside28 #140](https://github.com/deadinside28/bloodborne_pc/pull/140),
  Obrikash).
- **A lost GPU device on a particle effect** whose indirect dispatch read garbage group
  counts: implausible counts are now skipped and reported
  ([Supermedo #41](https://github.com/Supermedo/bloodborne_pc/pull/41), parsifal295, from
  bbport 0.5). `BB_INDIRECT_GUARD=0` restores the old behaviour.
- **Clone pads that spin the camera or never run:** each pad's stick neutral is measured when
  it connects, and an axis that only uses part of the range is scaled to full deflection, so
  knock-off controllers (for example Switch Pro clones) read correctly. A genuine pad reads as
  before. `BB_PAD_CENTER_CAL=0` turns it off; `BB_PAD_DEADZONE` sets an inner dead zone
  ([deadinside28 #71](https://github.com/deadinside28/bloodborne_pc/pull/71), Bainianzzz).
- **Every retail release recognised:** external patch files, mods wrapped in a serial folder
  and the save sound fix now accept all nine retail Bloodborne serials (Japanese and US Old
  Hunters editions included), which run the same 1.09 game
  ([Supermedo #47](https://github.com/Supermedo/bloodborne_pc/pull/47), parsifal295, from
  bbport 0.51).
- **Safer saves:** a save file is written to a copy and replaces the old one in a single step,
  so a crash or a kill while the game saves can no longer leave a damaged save
  ([Supermedo #43](https://github.com/Supermedo/bloodborne_pc/pull/43), parsifal295, from
  bbport 0.5). `BB_SAFE_SAVES=0` writes in place as before.
- **Key items on Xbox pads:** Back + d-pad right clicks the right half of the touchpad (key
  items, and the right-hand choice of some prompts), Back + d-pad left the left half; Back on
  its own still opens gestures, now when released
  ([Supermedo #39](https://github.com/Supermedo/bloodborne_pc/pull/39), parsifal295).
  `BB_PAD_BACK_COMBO=0` restores the old Back.
- **Less GPU work for nothing:** draws with no colour or depth target and no memory writes,
  which on the PS4 only fed occlusion counters this port answers itself, are skipped
  ([Supermedo #34](https://github.com/Supermedo/bloodborne_pc/pull/34), Aljoscha-Ide).
  `BB_SKIP_EMPTY_DRAWS=0` draws them.
- **Smoother frame times on 8 GB GPUs:** the texture cache starts evicting at 80% of a
  discrete GPU's memory budget instead of 40%, so textures in use are no longer dropped and
  uploaded again in Yharnam (issue #32).
- **The log of a crashed run is kept:** the launcher keeps the last three game logs
  (`last_run.log`, `last_run.1.log`, `last_run.2.log`) instead of overwriting one.
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
