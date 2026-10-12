# Roadmap

What this fork is working on, in order. Requirements are kept as they were asked for:
investigate before building, every feature can be switched off, credit the original authors,
and never offer a toggle for something that does not work.

Sources checked for ideas (last on 12 October 2026): shadPS4 (our renderer matches its
`d1cb90db`, 1 October), [Supermedo's pull requests](https://github.com/Supermedo/bloodborne_pc/pulls)
(through #49), [deadinside28's pull requests](https://github.com/deadinside28/bloodborne_pc/pulls)
(through #157), [droogie's bbhost](https://github.com/droogie/bbhost) and
[Mrsuss60's Windows port](https://github.com/Mrsuss60/bloodborne_pc_windows_port).

## Main work, in order

1. **Mouse pointer in the game's menus.** Hover a row to move the game's cursor there, click
   to confirm, right-click to go back, wheel to scroll; only while a menu is open, out of the
   mouse camera's way, with its own off switch. After bbhost's `menu_pointer.cpp`, whose list
   hooks the PC menu already uses. Start with System and the PC sections, then the other menus.
2. **Shader pre-build.** Finish the capture playthrough, then the `feature/shader-prebuild`
   branch: translate known shaders from the player's own files before play.
3. **Boss Rematches** (Bloodborne Enhanced features, independently implemented). A Cleric
   Beast prototype runs as an experiment with `BB_PC_REMATCHES=1`. Next: a copied-save test with
   the Cleric Beast defeated (the entry, the arena, Insight, the reward, death and leaving), then
   one boss at a time. Requirements:
   - Rematch bosses already defeated on the current character, without NG+, from their lamps;
     also through the Broken Lamp in the Hunter's Dream (raised garden near the tree, back and
     to the right of the spawn point). Chalice dungeon bosses only from their own lamps.
   - A rematch consumes one Insight when the player has any, and is still allowed with none.
   - Original progression and completed-boss flags are preserved, including after a death or quit.
   - Distorted Memories: Father Gascoigne, Ludwig and Orphan of Kos with both phases at once;
     Gehrman and the Moon Presence together (Broken Lamp only); Mergo's Wet Nurse with its clone
     phase always on.

   [Bloodborne Enhanced](https://www.nexusmods.com/bloodborne/mods/19)'s files may not be
   redistributed or modified, so everything is made from the player's own game files. Notes:
   [ENHANCED_LAMPS_RESEARCH.md](ENHANCED_LAMPS_RESEARCH.md).
4. **Catch up with shadPS4**, only after the shader capture is finished. Port, in order, the
   renderer and shader commits since `d1cb90db`: shader correctness (`945dbc3c`, `c4952908`,
   `1c2b8f97`), texture layout (`7e778987`, `e7929178`, `c679ce60`, `69b6041d`, `14ada48a`,
   `995e37e4`, and small-mip detiling, shadPS4 #5196 / Supermedo
   [#42](https://github.com/Supermedo/bloodborne_pc/pull/42)), draw edge cases (`643c7997`,
   `7721a1ef`, `00276961`, `c5dd8b8b`, `3a9f5726`, `8bafaa5a`) and speed-ups (`8e23388a`,
   `99e33b7e`, `145932bf`, `dade3afd`, `0f6d1b87`). Where a commit changes the shader records
   (`.meta` v8, `.key` v5), keep them compatible or convert the capture's catalog. Then merge
   background shader compiling (`feature/async-pipelines`, `BB_ASYNC_PIPELINES=0` off) after
   measuring new areas. Test in the Research Hall and shadowed areas.
5. **Frame generation and AMD Anti-Lag:** FSR 3.1 frame generation on any GPU, DLSS frame
   generation on RTX 40 and later, from AwesomeObserver via Supermedo
   [#48](https://github.com/Supermedo/bloodborne_pc/pull/48) and
   [#49](https://github.com/Supermedo/bloodborne_pc/pull/49). They build on the bbport 0.4 merge
   (#37), which this fork does not take, so they are ported, not merged.
6. **Seamless co-op** (wanted, after the above): two players, Elden Ring style, own LAN server,
   no PSN: deadinside28 [#130](https://github.com/deadinside28/bloodborne_pc/pull/130), a work in
   progress not yet tested in game. Check its progress at every upstream review.

## Small fixes, alongside the main work

Independent, each with an off switch where it changes behaviour; none touches the shader
recompiler or the pipeline cache.

- Saves written to a copy and swapped in one rename
  ([Supermedo #43](https://github.com/Supermedo/bloodborne_pc/pull/43), from bbport 0.5).
- Implausible indirect dispatch counts skipped instead of losing the device
  ([Supermedo #41](https://github.com/Supermedo/bloodborne_pc/pull/41)).
- Xbox pads: Back + d-pad left/right clicks the touchpad halves (key items)
  ([Supermedo #39](https://github.com/Supermedo/bloodborne_pc/pull/39)); check against our bindings.
- Every retail serial recognised ([Supermedo #47](https://github.com/Supermedo/bloodborne_pc/pull/47)).
- The overlay menu kept within the screen ([Supermedo #46](https://github.com/Supermedo/bloodborne_pc/pull/46)).
- Skip draws that cannot change anything ([Supermedo #34](https://github.com/Supermedo/bloodborne_pc/pull/34)).
- DLSS falls back to FSR 3.1 when its device extensions fail at start
  ([deadinside28 #140](https://github.com/deadinside28/bloodborne_pc/pull/140)).
- Stick neutral and dead zone per pad, for clone pads that drift or never reach full deflection
  ([deadinside28 #71](https://github.com/deadinside28/bloodborne_pc/pull/71)).
- Swapchain recreated through `oldSwapchain` (optional;
  [deadinside28 #93](https://github.com/deadinside28/bloodborne_pc/pull/93)).
- Dark spots on Intel CPUs (Old Hunters areas): shadPS4 `235dcabf` (RCPPS / RSQRTPS answer
  slightly differently on Intel). It changes the game's CPU code, not shaders.

## Ready on branches, waiting for testing in play

Pushed to the fork, not merged into `windows`:

- `fix/frame-cap-28`: no single frame faster than the cap (Supermedo #28, DUDUYG).
  Test: sprinting at a 120 cap.
- `fix/sprint-speed-33`: sprint no longer stuck at half speed above 90-120 FPS (Supermedo #33,
  Aljoscha-Ide; #31, DUDUYG). Test: sprinting at 120, and above with a higher cap.
- `fix/null-descriptor-table`: map-load crash on an unbound descriptor table (parsifal295, from
  Mrsuss60's port). Touches `shader_recompiler/info.h` only on the path that crashed; merge after
  the capture to be safe.
- `feature/launcher-update-check`: direct launches check for updates quietly; Install update on
  the Home banner. Test: a real update offer once a release is out.
- `feature/graphics-presets`: Low, Balanced, High, Ultra and Custom, suggested from the GPU.
  Test: each preset after a restart.
- `feature/ultrawide-12`: ultrawide, 16:10 and portrait outputs with a centred 16:9 HUD
  (Supermedo #12, FIocker, after bbhost). Test: 3440x1440 camera, HUD, enemy bars, title and
  loading screens. Merge after the capture (it forces the startup resolution patch off 16:9).

Older, still to confirm in play: the 1.6.12 preview fixes (character creation preview, area
loads, video memory over a long session), Rebirth at the Altar of Despair, two invaders, and
keyboard prompt text when keys are rebound away from the PCBButtons layout.

## Parked

Wanted at some point, not scheduled: mouse settings rows in the game's menus (extra rows crash
the menu movie), a Key Bindings screen, Switch-style prompts (Supermedo #25), FOV, more bbhost
image settings (AO strength, shadow distance, fog, bloom, saturation, vignette), achievement
icons in the game's menus, a debug menu without extra files, Brazilian Portuguese and Circle to
confirm (Supermedo #20, #17), a performance overlay and idle cursor hiding, data-field mods and
gameplay plugins, the mirror's character preview.

## Not planned

- **deadinside28's GPU memory model (0.5)** and the Windows sync built on it (deadinside28
  #138): Linux-first, about 7,000 lines of core GPU memory code on 0.3's model, which this
  Windows line never took. Revisit if AMD players report the bus-bound slowdowns it fixes.
- **bbport 0.3/0.4 merges** (Supermedo #37, #38): this fork has its own texture collector,
  controls, mouse camera and English menu. Only DS4-style touchpad reporting might be worth
  checking on its own.
- Supermedo #45 (the same as #26, included), #40 and #30 (we build with clang 23 already), #44
  (Quit Game is in the title menu), #35 (the same as #4, included), #27 and #13 (English menu
  included). deadinside28 #127 (our achievements cover it), #8 (FSR 4.1.1 FP8 on RX 9000, niche),
  #123 (only with the 0.5 memory model), translations of its launcher.
- shadPS4's kernel, network, file system and frontend changes; bbhost's libc and shader
  translator: they belong to their own architectures.
