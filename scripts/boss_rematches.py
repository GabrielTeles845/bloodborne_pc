#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Experimental Cleric Beast rematch assets generated from the player's 1.09 dump.

This is deliberately not installed by the launcher. It keeps the game's completed-boss and
first-entry flags intact, but the rematch arena, reward path, and recovery still need a copied-save
live test before it can be offered to players.
"""

import hashlib
from pathlib import Path
import struct

from emevd import Emevd
from fromsoft import Bnd4, dcx_compress, dcx_decompress
import enhanced_lamps
import menu_assets


INPUT_SHA256 = {
    'm24_01_00_00': '4341aca7efe61fd390dc90cb4996af2fd5de8ad3ac39b7e5a2e06b6681925672',
    'common': '8a76ce5746747e2696d20e2f5d0695bbdb044e83a9d64a3d073b58d76f48825b',
}
DEFEATED = 12411700
FIRST_ENTRY = 12411702
REQUEST = 12411711
ACTIVE = 12411712
ARENA_DEFEATED = 12411990
ARENA_ENTRY = 12411991
START_EVENT = 12411980
VICTORY_EVENT = 12411981
DEATH_EVENT = 12411982
LEAVE_MAP_EVENT = 10009980


class UnsupportedRematch(ValueError):
    pass


def instruction(bank: int, code: int, fmt: str, *values: int | float) -> tuple[int, int, bytes]:
    """Build one EMEVD instruction argument list from the game's documented EMEDF types."""
    if len(fmt) != len(values) or any(ch not in 'bBif' for ch in fmt):
        raise ValueError('unsupported event argument format')
    args = bytearray()
    for kind, value in zip(fmt, values):
        size = 4 if kind in 'if' else 1
        args.extend(bytes(-len(args) % size))
        args.extend(struct.pack('<' + kind, value))
    args.extend(bytes(-len(args) % 4))
    return bank, code, bytes(args)


def _set(flag: int, enabled: bool) -> tuple[int, int, bytes]:
    return instruction(2003, 2, 'iB', flag, int(enabled))


def _end_if_flag_off(flag: int) -> tuple[int, int, bytes]:
    return instruction(1003, 2, 'BBBi', 0, 0, 0, flag)


def _await_flag_on(flag: int) -> tuple[int, int, bytes]:
    return instruction(3, 0, 'bBBi', 0, 1, 0, flag)


def _unused(script: Emevd, event_ids: tuple[int, ...], flags: tuple[int, ...]) -> None:
    existing = {struct.unpack_from('<q', row)[0] for row in script.events}
    if existing.intersection(event_ids):
        raise UnsupportedRematch('experimental event id already exists')
    # A raw byte search is deliberately conservative: these flag IDs cannot already occur as
    # immediate values anywhere in this known game event file, even in unrelated instructions.
    args = bytes(script.args)
    if any(struct.pack('<I', flag) in args for flag in flags):
        raise UnsupportedRematch('experimental rematch flag already occurs in game events')


def _assert_global_ids_available(dvdroot: Path) -> None:
    """Avoid hijacking flags or events used by another map in the player's dump."""
    event_ids = {START_EVENT, VICTORY_EVENT, DEATH_EVENT, LEAVE_MAP_EVENT}
    flags = (REQUEST, ACTIVE, ARENA_DEFEATED, ARENA_ENTRY)
    event_files = sorted((dvdroot / 'event').glob('*.emevd.dcx'))
    if not event_files:
        raise UnsupportedRematch('game dump has no event scripts')
    for path in event_files:
        script = Emevd(dcx_decompress(path.read_bytes()))
        if any(struct.unpack_from('<q', row)[0] in event_ids for row in script.events):
            raise UnsupportedRematch(f'{path.name}: experimental rematch event ID is already used')
        args = bytes(script.args)
        if any(struct.pack('<I', flag) in args for flag in flags):
            raise UnsupportedRematch(f'{path.name}: experimental rematch flag is already used')


def _map_events(script: Emevd) -> None:
    _unused(script, (START_EVENT, VICTORY_EVENT, DEATH_EVENT),
            (REQUEST, ACTIVE, ARENA_DEFEATED, ARENA_ENTRY))
    boss = script.event_instructions(DEFEATED)
    entry = script.event_instructions(FIRST_ENTRY)
    if boss[0] != instruction(1003, 101, 'BBBi', 0, 0, 1, 0):
        # A pinned full-file hash already protects us, but this also makes the key guard explicit.
        raise UnsupportedRematch('Cleric Beast defeat guard differs from expected vanilla event')
    if entry[0] != instruction(1003, 2, 'BBBi', 0, 1, 0, DEFEATED):
        raise UnsupportedRematch('Cleric Beast first-entry guard differs from vanilla event')

    # Every map event reads a private arena view of the boss and entry state. Original completion
    # flags stay intact even if the mod disappears during a rematch. Leave writes to those original
    # flags alone so first-time vanilla progression still saves normally.
    originals = [(struct.unpack_from('<q', row)[0], script.event_instructions(
        struct.unpack_from('<q', row)[0])) for row in script.events]
    substitutions = ((DEFEATED, ARENA_DEFEATED), (FIRST_ENTRY, ARENA_ENTRY))
    for event_id, rows in originals:
        for position, (bank, code, args) in enumerate(rows):
            if (bank, code) == (2003, 2):  # SetEventFlag writes: preserve permanent game flags.
                continue
            new_args = args
            for original, arena in substitutions:
                old = struct.pack('<I', original)
                if old not in new_args:
                    continue
                # The second word in InitializeEvent names an event; it is not a flag.
                if (bank, code) == (2000, 0) and new_args[4:8] == old:
                    new_args = new_args[:8] + new_args[8:].replace(old, struct.pack('<I', arena))
                else:
                    new_args = new_args.replace(old, struct.pack('<I', arena))
            if new_args != args:
                script.replace_instruction(event_id, position, (bank, code, args),
                                           (bank, code, new_args))

    script.replace_instruction(DEFEATED, 0, boss[0],
                               instruction(1003, 101, 'BBBi', 0, 0, 0, ARENA_DEFEATED))
    script.replace_instruction(FIRST_ENTRY, 1, entry[1],
                               instruction(1003, 101, 'BBBi', 0, 0, 0, ARENA_ENTRY))
    script.replace_instruction(FIRST_ENTRY, 8, entry[8],
                               instruction(3, 0, 'bBBi', 1, 0, 0, ARENA_ENTRY))
    # Mark the first-entry animation as done in the arena view, without altering the original
    # game's saved first-entry flag. The vanilla write to FIRST_ENTRY still runs on first play.
    script.insert_instructions(FIRST_ENTRY, 20, [_set(ARENA_ENTRY, True)])
    # A completed rematch must not repeat first-clear item lots or achievement/progression writes.
    # KillBoss and arena cleanup remain vanilla; the private arena flag then ends the rematch.
    script.insert_instructions(DEFEATED, 16, [
        instruction(1003, 1, 'BBBi', 3, 0, 0, ACTIVE),
        _set(ARENA_DEFEATED, True),
        _set(ACTIVE, False),
        instruction(1000, 4, 'B', 0),
    ])

    # The constructor initializes arena views before starting any original map events.
    script.insert_instructions(0, 0, [
        _set(ARENA_DEFEATED, False),
        _set(ARENA_ENTRY, False),
        instruction(1003, 1, 'BBBi', 4, 1, 0, ACTIVE),
        instruction(1003, 1, 'BBBi', 1, 0, 0, DEFEATED),
        _set(ARENA_DEFEATED, True),
        instruction(1003, 1, 'BBBi', 1, 0, 0, FIRST_ENTRY),
        _set(ARENA_ENTRY, True),
    ])
    script.append_initialized_event(START_EVENT, [
        _await_flag_on(REQUEST),
        instruction(1003, 1, 'BBBi', 2, 1, 0, DEFEATED),
        _set(REQUEST, False),
        instruction(1000, 4, 'B', 0),
        _set(REQUEST, False),
        _set(ACTIVE, True),
        _set(ARENA_DEFEATED, False),
        _set(ARENA_ENTRY, False),
        instruction(3, 26, 'bBb', 1, 0, 2),  # Insight > 0
        instruction(1000, 1, 'BBb', 1, 0, 1),
        instruction(2004, 8, 'iiB', 10000, 4681, 0),  # one Insight, only if positive
        instruction(1001, 0, 'f', 1.0),
        instruction(2002, 8, 'iBB', 2411759, 24, 1),  # Cleric Beast entrance
        instruction(1000, 4, 'B', 1),
    ])
    script.append_initialized_event(VICTORY_EVENT, [
        _end_if_flag_off(ACTIVE),
        _await_flag_on(ARENA_DEFEATED),
        _set(ACTIVE, False),
    ])
    script.append_initialized_event(DEATH_EVENT, [
        _end_if_flag_off(ACTIVE),
        instruction(4, 2, 'bibf', 0, 10000, 5, 0.0),  # hunter HP <= 0
        _set(ARENA_DEFEATED, True),
        _set(ARENA_ENTRY, True),
        _set(ACTIVE, False),
    ])


def _common_events(script: Emevd) -> None:
    _unused(script, (LEAVE_MAP_EVENT,), (REQUEST, ACTIVE, ARENA_DEFEATED, ARENA_ENTRY))
    script.append_initialized_event(LEAVE_MAP_EVENT, [
        _await_flag_on(ACTIVE),
        instruction(1001, 0, 'f', 3.0),
        instruction(1003, 7, 'BBBB', 1, 0, 24, 1),  # outside Central Yharnam
        instruction(1000, 4, 'B', 1),  # still in arena: check again later
        _set(ARENA_DEFEATED, True),
        _set(ARENA_ENTRY, True),
        _set(ACTIVE, False),
        instruction(1000, 4, 'B', 1),
    ])


def make(dvdroot: Path) -> dict[Path, bytes]:
    """Return event, talk, and text assets; fail closed on any unknown game version."""
    _assert_global_ids_available(dvdroot)
    files = {}
    for name, edit in (('m24_01_00_00', _map_events), ('common', _common_events)):
        relative = Path('event') / f'{name}.emevd.dcx'
        original = (dvdroot / relative).read_bytes()
        unpacked = dcx_decompress(original)
        if hashlib.sha256(unpacked).hexdigest() != INPUT_SHA256[name]:
            raise UnsupportedRematch(f'{relative}: not the supported CUSA03173 1.09 event file')
        script = Emevd(unpacked)
        edit(script)
        result = script.to_bytes()
        if Emevd(result).to_bytes() != result:
            raise UnsupportedRematch(f'{relative}: generated event file did not round-trip')
        files[relative] = dcx_compress(result, template=original)

    # Compose with the independent Enhanced Lamps rewrite, rather than overlaying a second
    # copy of the same talk archive and silently removing Level Up or Storage at this lamp.
    files.update(enhanced_lamps.make(dvdroot))
    map_name, script_name = 'm24_01_00_00', 't241002.esd'
    talk_path = Path('script') / 'talk' / f'{map_name}.talkesdbnd.dcx'
    original = (dvdroot / talk_path).read_bytes()
    archive = Bnd4(dcx_decompress(original))
    index = archive.index(script_name)
    archive.files[index] = enhanced_lamps.add_lamp_menu(
        archive.files[index], enhanced_lamps.INPUTS[(map_name, script_name)],
        rematch=(DEFEATED, REQUEST, 14000337))
    files[talk_path] = dcx_compress(archive.to_bytes(), template=original)

    # Use the same full message rows as the PC menu layer, so either generated layer yields
    # identical message bundles if both are active. No authored game assets are distributed.
    rows = menu_assets.read_messages()
    if rows.get(14000337, ('',))[0] != 'Rematch Cleric Beast':
        raise UnsupportedRematch('Cleric Beast rematch message is missing')
    message_path = Path('msg')
    languages = sorted(p.name for p in (dvdroot / message_path).iterdir()
                       if (p / menu_assets.MESSAGE_FILE).is_file())
    if not languages:
        raise UnsupportedRematch('game dump has no message languages')
    for language in languages:
        relative = message_path / language / menu_assets.MESSAGE_FILE
        files[relative] = menu_assets.build_menu_messages((dvdroot / relative).read_bytes(), rows)
    return files
