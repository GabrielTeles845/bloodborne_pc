# Roadmap

What this fork is working on, in order. **Now** is being built, **Next** is agreed and
investigated, **Later** is wanted but not started. Requirements are kept as they were asked
for: investigate before building, and never offer a toggle for something that does not work.

Sources checked for ideas (last on 11 October 2026): shadPS4 (our renderer matches its
`d1cb90db`, 1 October), [Supermedo's pull requests](https://github.com/Supermedo/bloodborne_pc/pulls),
[deadinside28's bbport](https://github.com/deadinside28/bloodborne_pc) 0.5,
[droogie's bbhost](https://github.com/droogie/bbhost) and
[Mrsuss60's Windows port](https://github.com/Mrsuss60/bloodborne_pc_windows_port).

## Now

### Boss Rematches (Bloodborne Enhanced features, independently implemented)

Status: a Cleric Beast prototype (Codex's `scripts/boss_rematches.py`) runs as an experiment with
`BB_PC_REMATCHES=1`; Central Yharnam loads and plays with its rewritten events. Next: a copied-save
test with the Cleric Beast defeated (the entry, the arena, Insight, the reward, death and leaving),
then one boss at a time.

Reference: [Bloodborne Enhanced](https://www.nexusmods.com/bloodborne/mods/19). Its files may not
be redistributed or modified, so everything is made from the player's own game files. Notes:
[ENHANCED_LAMPS_RESEARCH.md](ENHANCED_LAMPS_RESEARCH.md).

- Rematch bosses already defeated on the current character, without NG+, from their boss lamps.
- Also through the Broken Lamp in the Hunter's Dream, in the raised garden near the tree, back
  and to the right of the spawn point.
- Chalice dungeon bosses only from their own lamps.
- A rematch consumes one Insight when the player has any, and is still allowed with none.
- Original progression and completed-boss flags are preserved, including after a death or quit.
- Distorted Memories: Father Gascoigne, Ludwig and Orphan of Kos with both phases at once;
  Gehrman and the Moon Presence together (Broken Lamp only); Mergo's Wet Nurse with its clone
  phase always on. Match the original encounter behaviour.

## Next

### Stability and correctness

- **More Supermedo fixes:** the frame cap letting single frames through too fast
  ([#28](https://github.com/Supermedo/bloodborne_pc/pull/28)); sprinting stuck at half speed
  above 90-120 FPS ([#33](https://github.com/Supermedo/bloodborne_pc/pull/33), same fix as #31).
- **Map-load crash on an unbound descriptor table:** parsifal295's guard in Mrsuss60's port.
- **Dark spots on Intel CPUs (Old Hunters areas):** the game's own code runs natively and Intel
  answers RCPPS / RSQRTPS slightly differently from the PS4's AMD chip; port shadPS4 `235dcabf`.
- **Catch up with shadPS4:** port, in order, the renderer and shader commits since `d1cb90db`:
  shader correctness (`945dbc3c`, `c4952908`, `1c2b8f97`), texture layout (`7e778987`,
  `e7929178`, `c679ce60`, `69b6041d`, `14ada48a`, `995e37e4`), draw edge cases (`643c7997`,
  `7721a1ef`, `00276961`, `c5dd8b8b`, `3a9f5726`, `8bafaa5a`) and speed-ups (`8e23388a`,
  `99e33b7e`, `145932bf`, `dade3afd`, `0f6d1b87`). Test in the Research Hall and shadowed areas.

### Smoother play

- **Compile shaders in the background**, as deadinside28 0.5 and Mrsuss60 do: geometry waits a
  frame for its pipeline, full-screen passes wait for theirs.
- **Evaluate the new GPU memory model** from deadinside28 0.5 (the game's memory used in place;
  default on AMD there, mixed on NVIDIA) before committing to it.
- **Skip draws that cannot change anything** ([#34](https://github.com/Supermedo/bloodborne_pc/pull/34)).

### Launcher

- A direct launch never checks for updates; check quietly in the background and keep the result
  for the next time the launcher shows. Add **Install update** to the Home banner.
- **Graphics presets** after [Lantern Launcher](https://github.com/par274/lantern-bloodborne-launcher)'s
  concept (not its values): Low, Balanced, High, Ultra and Custom, recommended from the detected
  GPU, in the setup wizard and launcher. Establish first which settings this port really has;
  overrides switch to Custom; explain each option and say when a restart is needed.

## Later

- **Mouse settings in the game's menus.** Adding rows to PC Settings or PC Gameplay crashes the
  generated menu movie, even with the hooks off; find the movie's real limits first.
- **Keyboard and mouse, the rest:** a pointer in every menu, a Key Bindings screen
  (bbhost `menu_pointer.cpp`), Switch-style prompts ([#25](https://github.com/Supermedo/bloodborne_pc/pull/25)).
- **Camera, FOV and ultrawide** with the HUD kept in place (bbhost PC Camera,
  [#12](https://github.com/Supermedo/bloodborne_pc/pull/12)).
- **More in-game settings** from bbhost: AO strength, shadow distance, fog, bloom, saturation,
  vignette.
- **Achievement icons in the game's menus**, an Achievements entry in the pause menu, and
  retiring the overlay's duplicated settings once the native menus have proven stable.
- **Debug menu without extra files** (bbhost builds its font from public-domain fonts).
- **Languages:** Brazilian Portuguese with a separate voice language
  ([#20](https://github.com/Supermedo/bloodborne_pc/pull/20)); Circle to confirm
  ([#17](https://github.com/Supermedo/bloodborne_pc/pull/17)).
- **Performance overlay** (FPS, frame times, memory, GPU and CPU use) and hiding an idle cursor,
  after Mrsuss60's port.
- **Mods by data field and gameplay plugins** after bbhost (parameter tables in TOML, a
  randomizer, mutators).
- **Mirror preview:** check whether the mirror's appearance editor shows the character model;
  if not, bbhost opens character creation's window around it (`change_appearance.cpp`).

## Needs testing in play

- The fixes in 1.6.12 preview: the character creation preview (#36), area loads (#29) and
  video memory over a long session (#32).
- Rebirth at the Altar of Despair.
- Two invaders (every player in the session needs it on).
- Keyboard prompt text when keys are rebound away from the PCBButtons layout.

## Not planned

- shadPS4's kernel, network, file system and frontend changes: this port has its own runtime.
- bbhost's libc and shader translator: they belong to its own architecture.
- Supermedo #30 (we build with clang 23 already), #35 (the same fix as #4, included), #27 and
  #13 (the English menu is included).
