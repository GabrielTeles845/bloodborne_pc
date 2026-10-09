# Bloodborne for Windows — Jgamrot fork

This fork is based on [Supermedo's Windows port](https://github.com/Supermedo/bloodborne_pc),
created by Mohammed Albarghouthi. It runs your own decrypted Bloodborne game files natively
on Windows 10 or 11. It builds on [deadinside28's bbport](https://github.com/deadinside28/bloodborne_pc)
and the [shadPS4](https://github.com/shadps4-emu/shadPS4) renderer.

## Which download do I click?

On the [release page](https://github.com/Jgamrot/bloodborne_pc/releases/tag/windows-v1.6.4),
click **[Bloodborne-Windows-v1.6.4-Jgamrot.zip](https://github.com/Jgamrot/bloodborne_pc/releases/download/windows-v1.6.4/Bloodborne-Windows-v1.6.4-Jgamrot.zip)**
if this is your first installation. It contains the launcher, the working game executable,
and Xbox button prompts. You do **not** need the other assets.

The separate `bb-probe.exe` and Xbox prompt zip are only for people updating an existing
installation. The two **Source code** downloads are for developers; they are not the game.
None of these downloads includes Bloodborne's game files.

## First-time setup

1. Right-click **Bloodborne-Windows-v1.6.4-Jgamrot.zip** in Downloads and choose
   **Extract All**. Put the resulting `bbport-windows` folder somewhere you want to keep it,
   such as `C:\Games\Bloodborne\bbport-windows`. Do not run the game from inside the zip.
2. Get your own extracted Bloodborne **CUSA03173 version 1.09** game folder ready. It is a
   *different* folder from `bbport-windows`. The correct folder has `eboot.bin` directly
   inside it, alongside `dvdroot_ps4`, `sce_module`, and `sce_sys`.
3. Open `bbport-windows` and double-click **Bloodborne.exe**. In the launcher, open
   **Game & effects**. Beside **Game folder**, choose the folder that contains `eboot.bin`.
   Select the folder itself, not `eboot.bin` and not the port's `bbport-windows` folder.
4. Click **PLAY**. The first launch prepares files and may take a while. New areas may stutter
   briefly while shaders are built. After setup, `Play Bloodborne.exe` starts the game using
   your saved settings without opening the launcher.

For example, this folder layout works:

```text
C:\Games\Bloodborne\
├── bbport-windows\
│   ├── Bloodborne.exe
│   └── bin\bb-probe.exe
└── CUSA03173\                 ← choose this as Game folder
    ├── eboot.bin
    ├── dvdroot_ps4\
    ├── sce_module\
    └── sce_sys\
```

### If your game is in `cusa-03173.zip`

That archive is **not** ready for the launcher when you use Windows **Extract All**: it
contains `CUSA03173.pkg` and several update `.pkg` files. The launcher cannot open the ZIP
or a PKG directly. If you already have a folder with `eboot.bin`, use that and skip this part.

1. Right-click `cusa-03173.zip` and choose **Extract All**. Open its `cusa-03173` folder.
2. Download and extract [ShadPs4Plus PKG Extractor 1.1](https://github.com/AzaharPlus/shadPS4Plus/releases/tag/PKG_EXTRACTOR_1_1).
   Drag `CUSA03173.pkg` onto `pkg_extractor.exe` and wait for extraction to finish.
3. Drag **`CUSA03173 Update v1.09.pkg`** onto the same `pkg_extractor.exe` and wait again.
   Use this plain 1.09 update, not the separate 720p or 1080p variants.
4. You should now have a base `CUSA03173` folder and an update folder (often called
   `CUSA03173-patch`). Open the update folder, select everything inside it, copy it into
   the base `CUSA03173` folder, and choose **Replace** when Windows asks.
5. Check that the base folder now contains `eboot.bin`, `dvdroot_ps4`, `sce_module`, and
   `sce_sys`. Move that complete folder to a permanent location, then choose it in the
   launcher's **Game folder** box as described above.

If the launcher says **No eboot.bin**, you selected the wrong folder or have only extracted
the PKG archive. If it reports game version 1.00, the 1.09 update was not copied into the
base folder. The game's own files are never uploaded to this repository.

## Already have Supermedo's v1.6 port?

Close the game, [download the current `bb-probe.exe`](https://github.com/Jgamrot/bloodborne_pc/releases/download/windows-v1.6.4/bb-probe.exe),
back up your existing `bin\bb-probe.exe`, and replace it with this file. Keep your `user`
saves, `bbport.ini`, and mods. For this fork's launcher and future updates, install the full
package instead.

The full package already includes Xbox button prompts. For an existing installation,
[download the separate mod zip](https://github.com/Jgamrot/bloodborne_pc/releases/download/windows-v1.6.4/Xbox-360-Prompts-Elden-Ring.zip)
and extract its folder into `mods`; it appears under **Mods & patches** in the launcher.

## What's changed

- The in-game settings menu (Insert or L3+R3) is in English, using the translation proposed
  in [upstream PR #13](https://github.com/Supermedo/bloodborne_pc/pull/13).
- Menu checkboxes and sliders save the value you selected, from
  [upstream PR #19](https://github.com/Supermedo/bloodborne_pc/pull/19).
- A v1.6.1 executable passed our fresh character-creation test: the preview appeared and the
  character looked right in the game on one RTX 4060 Laptop GPU at 1440p with DLSS. This
  v1.6.4 executable was rebuilt for the performance and achievement changes and has not
  repeated that test.
  The earlier preview problem's cause remains unisolated.
- The full package has a launcher that checks this fork for updates.
- Death respawns no longer wait for the game's fixed 12-second minimum. Asset and character
  readiness checks still apply. This is on by default for supported CUSA03173 1.09 images;
  set `BB_RESPAWN_DELAY_FIX=0` in the launcher's Advanced environment overrides to restore
  the original delay. The fix is adapted from
  [AwesomeObserver's Windows work](https://github.com/AwesomeObserver/bloodborne_pc/blob/windows/docs/respawn-loading-windows.md).
- Shader preload now reuses already loaded shader modules before reading their binaries again,
  and creates the Vulkan pipeline cache before warmup. This removes redundant startup work;
  gameplay FPS gains have not been measured. See [technical notes](docs/respawn-performance-windows.md).
- Motion vectors now keep images for several render sizes instead of draining the GPU whenever
  a scene switches sizes. This addresses the [water-area slowdown reported upstream](https://github.com/deadinside28/bloodborne_pc/issues/30).
  The fix is adapted from [deadinside28's port](https://github.com/deadinside28/bloodborne_pc);
  we have not measured its FPS effect on this Windows build.
- The game's trophy unlock calls now show a six-second in-game notification and save a local
  unlock history in `user\trophies.txt`. Open **Achievements (local)** in the Insert/L3+R3 menu
  to see trophies earned with this build. The encrypted game trophy pack does not provide
  readable names to this port yet, so entries show their trophy ID. Existing progress cannot
  be reconstructed from old saves. These achievements do not sync to PlayStation Network.

## Requirements and notes

- Windows 10 (1903 or later) or Windows 11, 64-bit, and a Vulkan 1.3 graphics card.
- About 6 GB of available memory (RAM plus page file), or 10 GB for 1440p or 4K.
- DLSS requires an NVIDIA GeForce RTX 20 series or newer. FSR is available on other GPUs.
- Keep the frame cap at 120 FPS or lower; higher caps can slow game movement.
- The first visit to an area may stutter while shaders compile. If you get a black screen,
  try **Advanced → Clear shader cache** in the launcher.

## Help and source

Open an [issue on this fork](https://github.com/Jgamrot/bloodborne_pc/issues) with your GPU,
what happened, and `user\last_run.log` from the game folder. Build instructions are in
[packaging/windows/README.md](packaging/windows/README.md).

The port is free software under the repository's [LICENSE](LICENSE). The release includes
third-party license notices in its `licenses` folder. Credit for the original Windows port
goes to [Supermedo](https://github.com/Supermedo/bloodborne_pc); credit for the English menu
translation goes to the author of [PR #13](https://github.com/Supermedo/bloodborne_pc/pull/13).
The Xbox prompts are by **Dommo**, from
[Xbox (and switch) UI for Bloodborne](https://www.nexusmods.com/bloodborne/mods/30),
redistributed under the upload permission on the mod page.
Bloodborne itself belongs to Sony Interactive Entertainment and FromSoftware; this project
is not affiliated with them.
