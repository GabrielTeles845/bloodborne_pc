#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Minimal Bloodborne 1.09 EMEVD64 editor for player-owned event files.

Keeps the game's existing event/instruction/argument/parameter/string bytes verbatim. Only
the count/offset header, affected event offsets, and parameter instruction indices change when
new events are inserted. Unsupported layouts are rejected before any output is produced.
"""

import struct


HEADER = 0x90
EVENT = 0x30
INSTRUCTION = 0x20
PARAMETER = 0x20
LINK = 8


class EmevdError(ValueError):
    pass


def _q(data, at):
    return struct.unpack_from('<q', data, at)[0]


def _u32(data, at):
    return struct.unpack_from('<I', data, at)[0]


class Emevd:
    def __init__(self, data: bytes):
        if len(data) < HEADER or data[:4] != b'EVD\0' or data[4:12] != b'\0\xff\0\0\xcc\0\0\0':
            raise EmevdError('not a supported little-endian Bloodborne EMEVD64')
        if _u32(data, 0x0c) != len(data):
            raise EmevdError('EMEVD size field does not match the file')
        counts = [_q(data, at) for at in (0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80)]
        offsets = [_q(data, at) for at in (0x18, 0x28, 0x38, 0x48, 0x58, 0x68, 0x78, 0x88)]
        events, instructions, zero_a, zero_b, params, links, arg_size, strings_size = counts
        event_at, ins_at, zero_a_at, zero_b_at, param_at, link_at, arg_at, strings_at = offsets
        if (min(counts) < 0 or events > 100000 or instructions > 1000000 or params > 1000000
                or zero_a or zero_b):
            raise EmevdError('unsupported EMEVD section counts')
        if (event_at != HEADER or ins_at != event_at + EVENT * events
                or arg_at != ins_at + INSTRUCTION * instructions
                or zero_a_at != arg_at or zero_b_at != arg_at
                or param_at != arg_at + arg_size
                or link_at != param_at + PARAMETER * params
                or strings_at != link_at + LINK * links
                or strings_at + strings_size != len(data)):
            raise EmevdError('unsupported EMEVD section layout')
        self.header = bytearray(data[:HEADER])
        self.events = [bytearray(data[event_at + i * EVENT:event_at + (i + 1) * EVENT])
                       for i in range(events)]
        self.instructions = [bytearray(data[ins_at + i * INSTRUCTION:ins_at + (i + 1) * INSTRUCTION])
                             for i in range(instructions)]
        self.args = bytearray(data[arg_at:param_at])
        self.params = [bytearray(data[param_at + i * PARAMETER:param_at + (i + 1) * PARAMETER])
                       for i in range(params)]
        self.links = data[link_at:strings_at]
        self.strings = data[strings_at:]
        self._validate_refs()

    def _validate_refs(self):
        next_instruction = 0
        ids = set()
        for row in self.events:
            event_id, count, first = struct.unpack_from('<qqq', row)
            if event_id in ids or count < 0 or first != next_instruction * INSTRUCTION:
                raise EmevdError('event ids or instruction ranges are inconsistent')
            ids.add(event_id)
            next_instruction += count
        if next_instruction != len(self.instructions):
            raise EmevdError('event instruction count does not match the table')
        for row in self.instructions:
            length, offset = struct.unpack_from('<qq', row, 8)
            if length < 0 or (length and (offset < 0 or offset + length > len(self.args))):
                raise EmevdError('instruction argument lies outside the argument blob')
        for row in self.params:
            index = _u32(row, 0)
            if _u32(row, 4) != 0 or index >= len(self.instructions):
                raise EmevdError('parameter refers to an absent instruction')

    def append_initialized_event(self, event_id: int, instructions: list[tuple[int, int, bytes]],
                                 *, slot: int = 0):
        """Append an event and add InitializeEvent(slot, event_id) to constructor event 0."""
        if not self.events or _q(self.events[0], 0) != 0 or any(_q(e, 0) == event_id for e in self.events):
            raise EmevdError('constructor missing or new event id already exists')
        if not instructions or event_id <= 0 or not 0 <= slot <= 0xffffffff:
            raise EmevdError('new event must have instructions and a valid id/slot')
        insertion = _q(self.events[0], 8)
        if insertion > len(self.instructions):
            raise EmevdError('constructor instruction range is invalid')
        init_args = struct.pack('<ii', slot, event_id)
        init = self._new_instruction(2000, 0, init_args)
        self.instructions.insert(insertion, init)
        struct.pack_into('<q', self.events[0], 8, insertion + 1)
        for row in self.events[1:]:
            struct.pack_into('<q', row, 16, _q(row, 16) + INSTRUCTION)
        for row in self.params:
            index = _u32(row, 0)
            if index >= insertion:
                struct.pack_into('<I', row, 0, index + 1)
        first = len(self.instructions) * INSTRUCTION
        new_rows = [self._new_instruction(bank, instruction_id, args)
                    for bank, instruction_id, args in instructions]
        self.instructions.extend(new_rows)
        self.events.append(bytearray(struct.pack('<6q', event_id, len(new_rows), first,
                                                 0, 0xffffffff, 0)))
        self._validate_refs()

    def _new_instruction(self, bank: int, instruction_id: int, args: bytes) -> bytearray:
        if not isinstance(args, bytes) or len(args) % 4:
            raise EmevdError('instruction arguments must be 4-byte aligned bytes')
        while len(self.args) % 8:
            self.args.append(0)
        offset = len(self.args)
        self.args.extend(args)
        return bytearray(struct.pack('<iiqqq', bank, instruction_id, len(args), offset,
                                     0xffffffff))

    def to_bytes(self) -> bytes:
        self._validate_refs()
        event_at = HEADER
        ins_at = event_at + EVENT * len(self.events)
        arg_at = ins_at + INSTRUCTION * len(self.instructions)
        param_at = arg_at + len(self.args)
        link_at = param_at + PARAMETER * len(self.params)
        strings_at = link_at + len(self.links)
        total = strings_at + len(self.strings)
        if total > 0xffffffff:
            raise EmevdError('EMEVD exceeds 32-bit file size')
        head = bytearray(self.header)
        struct.pack_into('<I', head, 0x0c, total)
        for at, value in ((0x10, len(self.events)), (0x18, event_at),
                          (0x20, len(self.instructions)), (0x28, ins_at),
                          (0x30, 0), (0x38, arg_at), (0x40, 0), (0x48, arg_at),
                          (0x50, len(self.params)), (0x58, param_at),
                          (0x60, len(self.links) // LINK), (0x68, link_at),
                          (0x70, len(self.args)), (0x78, arg_at),
                          (0x80, len(self.strings)), (0x88, strings_at)):
            struct.pack_into('<q', head, at, value)
        return b''.join((head, *self.events, *self.instructions, self.args, *self.params,
                         self.links, self.strings))
