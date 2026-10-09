# Bloodborne for Windows — Jgamrot fork

This fork is based on [Supermedo's Windows port](https://github.com/Supermedo/bloodborne_pc),
created by Mohammed Albarghouthi. It runs your own decrypted Bloodborne game files natively
on Windows 10 or 11. It builds on [deadinside28's bbport](https://github.com/deadinside28/bloodborne_pc)
and the [shadPS4](https://github.com/shadps4-emu/shadPS4) renderer.

## Download

- **New installation:** [Download the complete Windows package](https://github.com/Jgamrot/bloodborne_pc/releases/download/windows-v1.6.1/Bloodborne-Windows-v1.6.1-Jgamrot.zip), extract it, and run `Bloodborne.exe`.
- **Already using Supermedo's v1.6 package:** [Download the tested `bb-probe.exe`](https://github.com/Jgamrot/bloodborne_pc/releases/download/windows-v1.6.1/bb-probe.exe). Close the game, back up the existing `bin\bb-probe.exe`, and replace it with this file. Keep your `user` saves, `bbport.ini`, and mods.
- **Xbox button prompts:** Included in the full package and enabled by default. For an existing
  installation, [download the mod zip](https://github.com/Jgamrot/bloodborne_pc/releases/download/windows-v1.6.1/Xbox-360-Prompts-Elden-Ring.zip)
  and extract its folder into `mods`. The launcher lists it under **Mods & patches**.

Choose your own decrypted CUSA03173 game folder in the launcher, then press **PLAY**. The
download contains **no game files**. Game version 1.09 is needed for community patches such
as 60 FPS, higher resolutions and effects.

## What's changed

- The in-game settings menu (Insert or L3+R3) is in English, using the translation proposed
  in [upstream PR #13](https://github.com/Supermedo/bloodborne_pc/pull/13).
- Menu checkboxes and sliders save the value you selected, from
  [upstream PR #19](https://github.com/Supermedo/bloodborne_pc/pull/19).
- The downloadable `bb-probe.exe` is the exact executable used in our successful fresh
  character creation test: the character preview appeared and the character looked right
  in the game. This was tested on one PC with an RTX 4060 Laptop GPU at 1440p with DLSS.
  We have not isolated why the preview was missing before, so results on other PCs may vary.
- The full package has a launcher that checks this fork for updates.

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
