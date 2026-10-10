#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""PC enhancements made from the player's own CUSA03173 1.09 files at start.

Each one rewrites a few of the game's files into its own folder, which run.py lays under the
player's mods (mods.py --generated); switched off, the folder is not used and the game is as it
shipped. Every rewrite checks the dump's file and its own result by SHA-256, so an unexpected
dump leaves that file alone.

Ported from droogie's bbhost (https://github.com/droogie/bbhost, GPL-3.0-or-later):
engine/maiden_events.cpp and engine/dream_mirror_layout.cpp.

  pc_two_invaders       (BB_PC_TWO_INVADERS)  Mensis, Mergo's Loft, the Nightmare Frontier and
                                              the Old Hunters' areas hold two invaders, as the
                                              chalice dungeons do. Every player in a session
                                              needs it on.
  pc_mirror_appearance  (BB_PC_MIRROR)        the Hunter's Dream mirror opens the appearance
                                              editor (the developers' unused mirror character)

  pc_menu               (BB_PC_MENU)          the in-game PC menu's assets (menu_assets.py): its
                                              sections in the options movie and its strings in
                                              every language. On by default: it only adds rows.

Rebirth in the Nightmare follows: its altar entries use message ids the PC menu's files add.
"""
import argparse
import bisect
import hashlib
import os
from pathlib import Path
import shutil
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fromsoft import FormatError, dcx_compress, dcx_decompress  # noqa: E402

# ini key -> (environment override, folder name, label)
FEATURES = {
    'pc_two_invaders': ('BB_PC_TWO_INVADERS', 'two-invaders', 'Two invaders in the late areas'),
    'pc_mirror_appearance': ('BB_PC_MIRROR', 'dream-mirror', "Hunter's Dream mirror: change appearance"),
    'pc_menu': ('BB_PC_MENU', 'pc-menu', 'In-game PC menu (System > PC Settings)'),
}
# Off unless switched on, except these: they change nothing the game does by themselves.
DEFAULT_ON = {'pc_menu'}
# Bumped when a rewrite changes, so every install makes its files again.
VERSION = 'enhancements 2'  # 2: the PC menu's assets


def sha256(data):
    return hashlib.sha256(data).hexdigest()


class Skip(Exception):
    """The dump's file is not the one a rewrite knows: the game keeps its own."""


# --- Two invaders (bbhost engine/maiden_events.cpp) ------------------------------------------
#
# A map's Chime Maiden rings while fewer invaders than allowed are in and a stop event ends her
# at the first one. Both test 3[29] (client count): its argument is [condition group s8][client
# type u8, 1 invader][comparison u8, 0 ==, 3 <, 4 >=][count u8]. "== 0" becomes "< 2" and ">= 1"
# becomes ">= 2"; two maps also start an event with another map's flag, which is corrected.
MAIDEN_MAPS = [
    # map, input sha256, output sha256, ring event, stop event, [(event, slot, index, was, now)]
    ('m26_00_00_00', '95d168643c0ca1e96d77577be2723dc15ad6af8588e729ca6ab76cd609bc9331',
     '4d84a87b673117ea2158ba7ccaa50ba22796bb2303b5d333121ea1dee8c1ec8b', 12604710, 12604720, []),
    ('m33_00_00_00', '9375a331f1727e6d1f89f233773af1ffdeaa1eba97f97e1122dc6fc83ee78ff4',
     '533f8409794420e62e1bcaea0f2297c85801e7cc04286a05d8e31a17d81e875c', 13304710, 13304720, []),
    ('m34_00_00_00', '61cbe85daf9097619094e053edcab1233432c86619b96198845d5469e917268c',
     '499cd490e24214f81f3fa1d50b459acd945a802574b09a7c9bfe74b883a24311', 13404710, 13404720,
     [(13404710, 5, 0, 3400701, 3400791)]),
    ('m35_00_00_00', '4989dfbe383c30e65847c1abfc5d8e09adea0b6caff11320f864069566e1e628',
     'e38558b6f8ebd80b07cdf16e4759e901c91922c687c9de59396e8d94773479af', 13504710, 13504720,
     [(13504700, 5, 2, 12604712, 13504712)]),
    ('m36_00_00_00', '9ea8f27a0e5b93884b528bbab79bc7da52a890aef07ae886f777df2450583639',
     'e257328d7eafad901c39575615e9b6d5e01251c3438a55ce49265757b1da820e', 13604710, 13604720, []),
]
RING_WAS, RING_NOW = bytes([1, 1, 0, 0]), bytes([1, 1, 3, 2])
STOP_WAS, STOP_NOW = bytes([0xff, 1, 4, 1]), bytes([0xff, 1, 4, 2])


def event_instructions(b, event_id):
    """(bank, id, argument offset, argument length) of each instruction of an event that has
    arguments. EMEVD (64-bit): header int64s at +0x10 - [0] events, [1] event table,
    [3] instruction table, [13] argument blob; events 0x30 bytes, instructions 0x20."""
    if len(b) < 0x90:
        return []
    events, event_at, ins_at, args_at = (struct.unpack_from('<q', b, o)[0] for o in (0x10, 0x18, 0x28, 0x78))
    out = []
    for e in range(events):
        ev = event_at + e * 0x30
        if ev + 0x30 > len(b) or struct.unpack_from('<q', b, ev)[0] != event_id:
            continue
        count, first = struct.unpack_from('<qq', b, ev + 8)
        for k in range(count):
            at = ins_at + first + k * 0x20
            if at + 0x20 > len(b):
                raise Skip('the event table runs past the file')
            bank, ins_id, length, offset = struct.unpack_from('<iiqq', b, at)
            if length:
                if args_at + offset + length > len(b):
                    raise Skip('an argument runs past the file')
                out.append((bank, ins_id, args_at + offset, length))
    return out


def two_invaders_map(emevd, rule):
    name, sha_in, sha_out, ring, stop, fixes = rule
    if sha256(emevd) != sha_in:
        raise Skip(f'{name}: not the 1.09 event script')
    out = bytearray(emevd)
    for event in (ring, stop):
        for bank, ins_id, at, length in event_instructions(out, event):
            if bank != 3 or ins_id != 29 or length < 4:
                continue
            if out[at:at + 4] == RING_WAS:
                out[at:at + 4] = RING_NOW
            elif out[at:at + 4] == STOP_WAS:
                out[at:at + 4] = STOP_NOW
    for bank, ins_id, at, length in event_instructions(out, 0):
        if bank != 2000 or ins_id != 0 or length < 8:
            continue
        slot, event = struct.unpack_from('<ii', out, at)
        for f_event, f_slot, index, was, now in fixes:
            field = at + 8 + 4 * index
            if (event, slot) == (f_event, f_slot) and field + 4 <= at + length \
                    and struct.unpack_from('<i', out, field)[0] == was:
                struct.pack_into('<i', out, field, now)
    if sha256(out) != sha_out:
        raise Skip(f'{name}: the rewrite came out unexpected')
    return bytes(out)


def two_invaders(dvdroot):
    files = {}
    for rule in MAIDEN_MAPS:
        relative = Path('event') / f'{rule[0]}.emevd.dcx'
        source = dvdroot / relative
        try:
            packed = source.read_bytes()
            files[relative] = dcx_compress(two_invaders_map(dcx_decompress(packed), rule), template=packed)
        except (OSError, FormatError, Skip) as error:
            print(f'Enhancements: two invaders: {rule[0]} keeps the game\'s maidens ({error})', file=sys.stderr)
    return files


# --- Hunter's Dream mirror (bbhost engine/dream_mirror_layout.cpp) ---------------------------
#
# m21_00_00_00.msb places ダミーCHR_再キャラメイク ("dummy character for re-making the character")
# at the workshop's mirror: entity 2100202, talk script t210696 ("Put on Disguise", then the
# appearance editor). The developers left it a DummyEnemy (type 10), which never spawns. As an
# Enemy (type 2) it spawns like the workbench dummy beside it. Parts are grouped by type in file
# order, so the entry moves from slot 807 to 742 and every field naming a part by slot follows.
MIRROR_IN = '8e1de0ee223cd5137a93383cfc7bb1d27964bc480c63e35013674717b403ef8f'
MIRROR_OUT = '99d5c7b7f52307344c6e8296b499d5e57969f21c443fcc607cb42cb2c38c8afa'
MOVE_FROM, MOVE_TO, FIRST_ENEMY = 807, 742, 736
ENEMY, DUMMY_ENEMY = 2, 10
EVENT_FIELDS = (0x48, 0x50, 0x5c)
PART_FIELDS = (0x128, 0x13c, 0x144, 0x178, 0x180, 0x188)


def msb_lists(b):
    """The MSB's MODEL, EVENT, POINT and PARTS lists: (offset table, [entry offsets])."""
    lists, at = [], 0x10
    for k in range(4):
        if at + 16 > len(b):
            raise Skip('the layout\'s lists do not read')
        count = struct.unpack_from('<i', b, at + 4)[0]
        table = at + 16
        if count < 1 or table + 8 * count > len(b):
            raise Skip('the layout\'s lists do not read')
        entries = [struct.unpack_from('<q', b, table + 8 * i)[0] for i in range(count - 1)]
        if any(e <= 0 or e >= len(b) for e in entries):
            raise Skip('the layout\'s lists do not read')
        lists.append((table, entries))
        at = struct.unpack_from('<q', b, table + 8 * (count - 1))[0]
        if k < 3 and not 0 < at < len(b):
            raise Skip('the layout\'s lists do not read')
    return lists


def remap(slot):
    if MOVE_TO <= slot < MOVE_FROM:
        return slot + 1
    return MOVE_TO if slot == MOVE_FROM else slot


def mirror_layout(msb):
    if sha256(msb) != MIRROR_IN:
        raise Skip('not the 1.09 m21_00_00_00 layout')
    b = bytearray(msb)
    lists = msb_lists(b)
    events, (parts_table, parts) = lists[1], lists[3]
    if len(parts) <= MOVE_FROM + 1:
        raise Skip('too few parts')
    starts = sorted(e for _, entries in lists for e in entries)

    def extent(entry):
        after = bisect.bisect_right(starts, entry)
        return (starts[after] if after < len(starts) else len(b)) - entry

    for entries, fields in ((events[1], EVENT_FIELDS), (parts, PART_FIELDS)):
        for e in entries:
            size = extent(e)
            for field in fields:
                if field + 4 > size:
                    continue
                value = struct.unpack_from('<i', b, e + field)[0]
                if value >= 0 and remap(value) != value:
                    struct.pack_into('<i', b, e + field, remap(value))
    for i, e in enumerate(parts):
        kind, index = struct.unpack_from('<ii', b, e + 0x14)
        if kind == ENEMY and i >= MOVE_TO:
            struct.pack_into('<i', b, e + 0x18, index + 1)
        if kind == DUMMY_ENEMY and i > MOVE_FROM:
            struct.pack_into('<i', b, e + 0x18, index - 1)
    moved = parts[MOVE_FROM]
    if struct.unpack_from('<i', b, moved + 0x14)[0] != DUMMY_ENEMY:
        raise Skip('the stand-in is not a DummyEnemy')
    struct.pack_into('<ii', b, moved + 0x14, ENEMY, MOVE_TO - FIRST_ENEMY)
    start, source, end = parts[MOVE_TO], parts[MOVE_FROM], parts[MOVE_FROM + 1]
    if not start < source < end:
        raise Skip('the parts are not in file order')
    size = end - source
    b[start:end] = b[source:end] + b[start:source]
    for i in range(MOVE_TO, MOVE_FROM):
        struct.pack_into('<q', b, parts_table + 8 * (i + 1), parts[i] + size)
    struct.pack_into('<q', b, parts_table + 8 * MOVE_TO, start)
    if sha256(b) != MIRROR_OUT:
        raise Skip('the rewrite is not the known result')
    return bytes(b)


def dream_mirror(dvdroot):
    relative = Path('map') / 'MapStudio' / 'm21_00_00_00.msb.dcx'
    try:
        packed = (dvdroot / relative).read_bytes()
        return {relative: dcx_compress(mirror_layout(dcx_decompress(packed)), template=packed)}
    except (OSError, FormatError, Skip) as error:
        print(f'Enhancements: Hunter\'s Dream mirror: the Dream keeps its layout ({error})', file=sys.stderr)
        return {}


def pc_menu(dvdroot):
    import menu_assets
    return menu_assets.make(dvdroot, log=lambda line: print(f'Enhancements: {line}', file=sys.stderr))


MAKERS = {'pc_two_invaders': two_invaders, 'pc_mirror_appearance': dream_mirror, 'pc_menu': pc_menu}


def enabled(settings, env):
    """The features switched on: the environment overrides bbport.ini; both default to off."""
    on = []
    for key, (variable, _folder, _label) in FEATURES.items():
        value = env.get(variable, settings.get(key, '1' if key in DEFAULT_ON else '0'))
        if str(value).strip() == '1':
            on.append(key)
    return on


def read_settings(path):
    values = {}
    try:
        for line in Path(path).read_text(encoding='utf-8').splitlines():
            if '=' in line and not line.lstrip().startswith('#'):
                key, value = line.split('=', 1)
                values[key.strip()] = value.strip()
    except OSError:
        pass
    return values


def generator():
    """The generator's own identity: its version and the scripts and strings it runs."""
    here = Path(__file__).resolve().parent
    digest = hashlib.sha256()
    for name in ('enhancements.py', 'menu_assets.py', 'fromsoft.py', 'pc_menu_messages.tsv'):
        try:
            digest.update((here / name).read_bytes())
        except OSError:
            pass
    return f'{VERSION} {digest.hexdigest()[:16]}'


def stamp_for(dvdroot, files):
    """What a feature's folder was made from: the generator and the dump's source files."""
    lines = [generator()]
    for relative in sorted(files):
        try:
            info = (dvdroot / relative).stat()
            lines.append(f'{relative.as_posix()} {info.st_size} {int(info.st_mtime)}')
        except OSError:
            lines.append(f'{relative.as_posix()} missing')
    return '\n'.join(lines) + '\n'


def make(game, out, keys):
    """Makes (or reuses) each feature's folder; returns those with files in them."""
    dvdroot = Path(game) / 'dvdroot_ps4'
    folders = []
    for key in keys:
        folder = Path(out) / FEATURES[key][1]
        stamp_file = folder / 'stamp.txt'
        try:
            previous = stamp_file.read_text(encoding='utf-8')
        except OSError:
            previous = None
        if previous and previous.splitlines()[0] == generator():
            made = [Path(line.rsplit(' ', 2)[0]) for line in previous.splitlines()[1:]]
            if made and stamp_for(dvdroot, made) == previous and \
                    all((folder / 'dvdroot_ps4' / m).is_file() for m in made):
                folders.append(folder)
                print(f'Enhancements: {FEATURES[key][2]} (up to date)', file=sys.stderr)
                continue
        files = MAKERS[key](dvdroot)
        shutil.rmtree(folder, ignore_errors=True)
        if not files:
            continue
        for relative, data in files.items():
            target = folder / 'dvdroot_ps4' / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        stamp_file.write_text(stamp_for(dvdroot, files), encoding='utf-8')
        folders.append(folder)
        print(f'Enhancements: {FEATURES[key][2]} ({len(files)} files)', file=sys.stderr)
    return folders


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--settings', type=Path)
    args = parser.parse_args()
    keys = enabled(read_settings(args.settings) if args.settings else {}, os.environ)
    for folder in make(args.game, args.out, keys):
        print(folder)


if __name__ == '__main__':
    main()
