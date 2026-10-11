"""Validate the rematch event writer against the player's unmodified 1.09 files."""
import os
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from emevd import Emevd, EmevdError  # noqa: E402
from fromsoft import dcx_decompress  # noqa: E402


@unittest.skipUnless(os.environ.get('BB_TEST_GAME_DIR'), 'BB_TEST_GAME_DIR not set')
class EmevdTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.events = Path(os.environ['BB_TEST_GAME_DIR']) / 'dvdroot_ps4' / 'event'

    def load(self, name):
        return dcx_decompress((self.events / name).read_bytes())

    def test_every_game_event_file_round_trips_byte_identically(self):
        files = sorted(self.events.glob('*.emevd.dcx'))
        self.assertEqual(len(files), 27)
        for file in files:
            with self.subTest(file=file.name):
                original = dcx_decompress(file.read_bytes())
                self.assertEqual(Emevd(original).to_bytes(), original)

    def test_insertion_keeps_existing_instructions_and_parameter_targets(self):
        for name in ('common.emevd.dcx', 'm24_01_00_00.emevd.dcx'):
            with self.subTest(file=name):
                script = Emevd(self.load(name))
                original_instructions = [bytes(row) for row in script.instructions]
                original_params = [bytes(row) for row in script.params]
                insertion = struct.unpack_from('<q', script.events[0], 8)[0]
                script.append_initialized_event(19999990, [(1000, 4, b'\0\0\0\0')])
                edited = Emevd(script.to_bytes())
                self.assertEqual([bytes(row) for row in edited.instructions[:insertion]],
                                 original_instructions[:insertion])
                self.assertEqual([bytes(row) for row in edited.instructions[insertion + 1:-1]],
                                 original_instructions[insertion:])
                self.assertEqual([struct.unpack_from('<I', row)[0] for row in edited.params],
                                 [struct.unpack_from('<I', row)[0] +
                                  (struct.unpack_from('<I', row)[0] >= insertion)
                                  for row in original_params])

    def test_unknown_layout_is_rejected(self):
        damaged = bytearray(self.load('common.emevd.dcx'))
        struct.pack_into('<q', damaged, 0x58, len(damaged) + 1)
        with self.assertRaises(EmevdError):
            Emevd(damaged)

    def test_inserting_before_parameterized_instructions_rebases_refs(self):
        script = Emevd(self.load('m24_01_00_00.emevd.dcx'))
        original_events = script.event_instructions(12411700)
        original_params = [struct.unpack_from('<I', row)[0] for row in script.params]
        original_first = next(struct.unpack_from('<q', row, 16)[0] for row in script.events
                              if struct.unpack_from('<q', row)[0] == 12411700) // 32
        script.insert_instructions(12411700, 0,
                                   [(1000, 4, b'\0\0\0\0'), (1000, 4, b'\x01\0\0\0')])
        edited = Emevd(script.to_bytes())
        self.assertEqual(edited.event_instructions(12411700)[2:], original_events)
        self.assertEqual([struct.unpack_from('<I', row)[0] for row in edited.params],
                         [index + (2 if index >= original_first else 0)
                          for index in original_params])

    def test_replacement_requires_exact_expected_instruction(self):
        script = Emevd(self.load('m24_01_00_00.emevd.dcx'))
        original = script.event_instructions(12411700)
        untouched = script.to_bytes()
        with self.assertRaisesRegex(EmevdError, 'differs'):
            script.replace_instruction(12411700, 0, (0, 0, b''), original[0])
        self.assertEqual(script.to_bytes(), untouched)
        replacement = (1000, 4, b'\0\0\0\0')
        script.replace_instruction(12411700, 0, original[0], replacement)
        edited = Emevd(script.to_bytes())
        self.assertEqual(edited.event_instructions(12411700), [replacement, *original[1:]])

    def test_malformed_append_does_not_change_input(self):
        script = Emevd(self.load('common.emevd.dcx'))
        original = script.to_bytes()
        with self.assertRaisesRegex(EmevdError, 'invalid EMEVD instruction'):
            script.append_initialized_event(19999990, [(1000, 4, b'good'), (1000, 4, b'bad')])
        self.assertEqual(script.to_bytes(), original)
