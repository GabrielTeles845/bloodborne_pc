#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Merge loose-file mods through links, preserving the original game and mod files."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

# Top-level folders of the game's dvdroot_ps4: a mod made of these is a dvdroot_ps4 itself.
GAME_FOLDERS = {'action', 'chr', 'event', 'facegen', 'font', 'map', 'menu', 'movie', 'msg', 'mtd',
                'obj', 'other', 'param', 'paramdef', 'parts', 'remo', 'script', 'sfx', 'shader',
                'sound'}


def child(folder, name):
    """`name` inside `folder`, matching an existing entry case-insensitively (mods made on
    Windows often differ from the game's lower-case names)."""
    folder = Path(folder)
    for entry in (folder.iterdir() if folder.is_dir() else ()):
        if entry.name.casefold() == name.casefold():
            return entry
    return folder / name


def content_root(folder):
    """(root, prefix): where a mod's files are and the game path they go to, or None.

    Accepted layouts: <mod>/dvdroot_ps4, <mod>/app0/dvdroot_ps4, <mod>/CUSA03173/dvdroot_ps4,
    one wrapper folder around any of these (an archive extracted into a folder of its name), and
    the game's folders without dvdroot_ps4 (<mod>/chr, <mod>/parts, ...)."""
    folder = Path(folder)
    if not folder.is_dir():
        return None
    for wrapper in ('', 'app0', 'CUSA03173'):
        base = child(folder, wrapper) if wrapper else folder
        dvdroot = child(base, 'dvdroot_ps4')
        if dvdroot.is_dir():
            return base, ''
    entries = [e for e in folder.iterdir() if not e.name.startswith('.')]
    folders = [e for e in entries if e.is_dir()]
    if folders and all(e.name.casefold() in GAME_FOLDERS for e in folders):
        return folder, 'dvdroot_ps4'
    if len(folders) == 1 and not [e for e in entries if e.is_file() and
                                  e.suffix.casefold() not in ('.txt', '.md', '.jpg', '.png', '.ini')]:
        return content_root(folders[0])
    return None


def discover(root):
    """Named mods: folders of `root` with an accepted layout (content_root)."""
    root = Path(root)
    if not root.is_dir():
        return []
    return sorted((p.name for p in root.iterdir() if p.is_dir() and content_root(p)),
                  key=lambda name: (name.casefold(), name))


def selected(root, config):
    available = discover(root)
    if not config or not Path(config).is_file():
        return available
    settings = json.loads(Path(config).read_text(encoding='utf-8'))
    disabled_names = settings.get('disabled', [])
    if not isinstance(disabled_names, list) or not all(isinstance(n, str) for n in disabled_names):
        raise ValueError('Disabled mods must be a list of folder names')
    disabled = set(disabled_names)
    order = settings.get('order', [])
    if not isinstance(order, list) or not all(isinstance(n, str) for n in order):
        raise ValueError('Mod order must be a list of folder names')
    # New folders are enabled automatically and appended in alphabetical order.
    return list(dict.fromkeys(n for n in [*order, *available]
                             if n in available and n not in disabled))


def mod_files(folder):
    """(game path, source) of every file a mod replaces or adds under dvdroot_ps4."""
    layout = content_root(folder)
    if not layout:
        raise ValueError(f'{folder}: expected dvdroot_ps4 (or chr/, parts/, ...) inside the mod folder')
    root, prefix = layout
    root = root.resolve()
    for directory, folders, files in os.walk(root, followlinks=False):
        directory = Path(directory)
        for name in [*folders, *files]:
            if (directory / name).is_symlink():
                raise ValueError(f'Mod symlinks are unsupported: {directory / name}')
        for name in sorted(files):
            source = directory / name
            relative = Path(prefix) / source.relative_to(root) if prefix else source.relative_to(root)
            # Readme/metadata stay outside the mounted game. This loader handles assets;
            # executable patches use the existing patch compiler, with address validation.
            if relative.parts[0].casefold() != 'dvdroot_ps4':
                if relative.parts[0].casefold() in ('eboot.bin', 'sce_module', 'sce_sys'):
                    raise ValueError(f'Executable/system replacement is unsupported: {source}')
                continue
            if not source.is_file():
                raise ValueError(f'Not a regular mod file: {source}')
            yield relative, source


# Windows: files are always hard links (copies across volumes), never symlinks. The runtime's
# UCRT _stat64 and FindFirstFileA report a file symlink's own size (0), not its target's, and
# the game panics loading it (FileTransferTask.cpp(865)). Directories are symlinks, or
# junctions without the symlink privilege (Developer Mode off). Hard links and junctions are
# not symlinks, so the overlay remembers what it linked and where to.
#
# A process started with Windows' redirection trust policy (RedirectionGuard; Armoury Crate on
# the ROG Ally starts games this way, and children inherit it) refuses to follow a junction or
# symlink a non-administrator made: ERROR_UNTRUSTED_MOUNT_POINT. The first directory link is
# tried; if it is refused, every directory becomes a real folder of hard-linked files (about
# 6 s for the whole game on an SSD) for the rest of this overlay.
LINKED = {}
UNTRUSTED_MOUNT_POINT = 448
MIRROR_DIRECTORIES = False


def _directory_link_followed(link):
    try:
        os.listdir(link)
        return True
    except OSError as error:
        if getattr(error, 'winerror', None) == UNTRUSTED_MOUNT_POINT:
            return False
        raise


def _mirror_directory(link, target):
    link.mkdir()
    for entry in target.iterdir():
        make_link(link / entry.name, entry)


def make_link(link, target):
    global MIRROR_DIRECTORIES
    if os.name != 'nt':
        link.symlink_to(target, target_is_directory=target.is_dir())
        return
    if target.is_dir():
        if MIRROR_DIRECTORIES:
            _mirror_directory(link, target)
            return
        try:
            link.symlink_to(target, target_is_directory=True)
        except OSError:
            import _winapi
            _winapi.CreateJunction(str(target), str(link))
            LINKED[str(link)] = target
        if _directory_link_followed(link):
            return
        remove_link(link)
        MIRROR_DIRECTORIES = True
        print('Mods: this process may not follow folder links (Windows redirection trust); '
              'copying the folder structure with hard links instead', file=sys.stderr)
        _mirror_directory(link, target)
        return
    else:
        try:
            os.link(target, link)
        except OSError:
            shutil.copy2(target, link)
    LINKED[str(link)] = target


def is_link(path):
    return path.is_symlink() or str(path) in LINKED or (hasattr(path, 'is_junction') and path.is_junction())


def link_target(path):
    return LINKED.get(str(path)) or path.resolve(strict=True)


def remove_link(path):
    if hasattr(path, 'is_junction') and path.is_junction():
        os.rmdir(path)
    else:
        path.unlink()
    LINKED.pop(str(path), None)


def expand(directory):
    """Materialize one directory level; never write through a directory link."""
    if is_link(directory):
        target = link_target(directory)
        if not target.is_dir():
            raise ValueError(f'File/directory conflict at {directory.name}')
        remove_link(directory)
        directory.mkdir()
        for entry in target.iterdir():
            make_link(directory / entry.name, entry)
    elif directory.exists() and not directory.is_dir():
        raise ValueError(f'File/directory conflict at {directory.name}')
    else:
        directory.mkdir(exist_ok=True)


def build_overlay(game, out, mods):
    game = Path(game).resolve(strict=True)
    replacements = []
    owners = {}
    for name, root in mods:
        count = 0
        for relative, source in mod_files(root):
            key = relative.as_posix().casefold()
            if key in owners:
                print(f'Mods: {relative}: {owners[key]} -> {name}', file=sys.stderr)
            owners[key] = name
            replacements.append((relative, source))
            count += 1
        print(f'Mods: {name}: {count} files', file=sys.stderr)
    if not replacements:
        return game
    out = Path(out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if os.name == 'nt' and out.drive.casefold() != game.drive.casefold():
        # Hard links only reach files on the same volume: the overlay goes beside the game.
        out = game.parent
    overlay = Path(tempfile.mkdtemp(prefix='mod-game-', dir=out))
    try:
        for entry in game.iterdir():
            make_link(overlay / entry.name, entry)
        replaced = added = 0
        for relative, source in replacements:
            # Each component takes the game's spelling when it exists in another case.
            parent = overlay
            for part in relative.parts[:-1]:
                parent = child(parent, part)
                expand(parent)
            destination = child(parent, relative.parts[-1])
            if is_link(destination):
                replaced += link_target(destination).is_relative_to(game)
                remove_link(destination)
            elif destination.exists():
                raise ValueError(f'File/directory conflict: {relative}')
            else:
                added += 1
            make_link(destination, source)
        print(f'Mods: {replaced} game files replaced, {added} added', file=sys.stderr)
        return overlay
    except BaseException:
        shutil.rmtree(overlay)
        raise


SEEDED = '.bbport-bundled.json'


def seed_bundled(mods_dir, bundled, off=(), config=None):
    """Copies mods the port ships (the Xbox and keyboard button prompts) into the player's mods
    folder once: a data folder of its own (BB_DATA_DIR) or a source checkout starts without them.
    Those in `off` arrive switched off in `config` (the keyboard prompts would otherwise replace
    the same images as the Xbox ones). What was seeded is remembered, so a mod the player
    deletes stays deleted."""
    mods_dir = Path(mods_dir)
    off_names = {Path(folder).name for folder in off}
    record = mods_dir / SEEDED
    try:
        seeded = set(json.loads(record.read_text(encoding='utf-8')))
    except (OSError, ValueError, TypeError):
        seeded = set()
    changed = False
    for source in [*bundled, *off]:
        source = Path(source)
        if source.name in seeded or not source.is_dir():
            continue
        target = child(mods_dir, source.name)
        if not target.exists() and source.resolve() != target.resolve():
            if source.name in off_names and config:
                config = Path(config)
                try:
                    settings = json.loads(config.read_text(encoding='utf-8'))
                except (OSError, ValueError):
                    settings = {}
                disabled = settings.get('disabled', [])
                if source.name not in disabled and source.name not in settings.get('enabled', []):
                    settings['disabled'] = sorted({*disabled, source.name})
                    config.parent.mkdir(parents=True, exist_ok=True)
                    config.write_text(json.dumps(settings, indent=2), encoding='utf-8')
            shutil.copytree(source, target)
            print(f'Mods: added the bundled {source.name}', file=sys.stderr)
        seeded.add(source.name)
        changed = True
    if changed:
        mods_dir.mkdir(parents=True, exist_ok=True)
        record.write_text(json.dumps(sorted(seeded)), encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--mods-dir', required=True, type=Path)
    parser.add_argument('--config', type=Path)
    parser.add_argument('--enabled', choices=('0', '1'), default='1')
    # Files the port's own features make from the game (PC enhancements, menu assets), each a
    # folder with a dvdroot_ps4 layout. They lie under the player's mods, which still win, and
    # apply with mods off: every feature has its own switch.
    parser.add_argument('--generated', type=Path, action='append', default=[])
    # Mods the port ships, copied into --mods-dir once (seed_bundled).
    parser.add_argument('--bundled', type=Path, action='append', default=[])
    # Shipped mods that arrive switched off (the keyboard prompts).
    parser.add_argument('--bundled-off', type=Path, action='append', default=[])
    args = parser.parse_args()
    seed_bundled(args.mods_dir, args.bundled, args.bundled_off, args.config)
    layers = [(folder.name, folder) for folder in args.generated if content_root(folder)]
    if args.enabled == '1':
        # shadPS4's loose overlay convention, beside the original game.
        legacy = Path(str(args.game.resolve()) + '-mods')
        if legacy.is_dir():
            layers.append((legacy.name, legacy))
        # A directly selected loose overlay is also accepted.
        if child(args.mods_dir, 'dvdroot_ps4').is_dir():
            layers.append((args.mods_dir.name, args.mods_dir))
        else:
            for name in selected(args.mods_dir, args.config):
                layers.append((name, args.mods_dir / name))
    print(build_overlay(args.game, args.out, layers))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, TypeError) as error:
        print(f'Mods: {error}', file=sys.stderr)
        sys.exit(1)
