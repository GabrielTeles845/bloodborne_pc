"""Static validation of the dormant Cleric Beast rematch-event prototype."""
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import boss_rematches  # noqa: E402
import enhanced_lamps  # noqa: E402
import esd  # noqa: E402
import menu_assets  # noqa: E402
from emevd import Emevd  # noqa: E402
from fromsoft import Bnd4, dcx_decompress  # noqa: E402


class InstructionTests(unittest.TestCase):
    def test_emedf_alignment_is_explicit(self):
        self.assertEqual(boss_rematches.instruction(3, 0, 'bBBi', 0, 1, 0, 12411711)[2],
                         bytes.fromhex('00 01 00 00 3f 63 bd 00'))
        self.assertEqual(len(boss_rematches.instruction(4, 2, 'bibf', 0, 10000, 5, 0.0)[2]), 16)


@unittest.skipUnless(os.environ.get('BB_TEST_GAME_DIR'), 'BB_TEST_GAME_DIR not set')
class BossRematchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dvdroot = Path(os.environ['BB_TEST_GAME_DIR']) / 'dvdroot_ps4'
        cls.files = boss_rematches.make(cls.dvdroot)

    def test_generated_events_preserve_vanilla_event_bodies(self):
        files = self.files
        self.assertEqual(len(files), 40)
        for name, added in (
            ('m24_01_00_00', (boss_rematches.START_EVENT, boss_rematches.VICTORY_EVENT,
                              boss_rematches.DEATH_EVENT)),
            ('common', (boss_rematches.LEAVE_MAP_EVENT,)),
        ):
            relative = Path('event') / f'{name}.emevd.dcx'
            original = Emevd(dcx_decompress((self.dvdroot / relative).read_bytes()))
            modified = Emevd(dcx_decompress(files[relative]))
            self.assertEqual(modified.to_bytes(), dcx_decompress(files[relative]))
            for row in original.events:
                event_id = int.from_bytes(row[:8], 'little')
                if event_id != 0:
                    self.assertEqual(modified.event_instructions(event_id),
                                     original.event_instructions(event_id), event_id)
            self.assertEqual(len(modified.events), len(original.events) + len(added))
            for event_id in added:
                self.assertTrue(modified.event_instructions(event_id))
            if name == 'common':
                watcher = modified.event_instructions(boss_rematches.LEAVE_MAP_EVENT)
                self.assertEqual(watcher[0], boss_rematches.instruction(
                    3, 0, 'bBBi', 0, 1, 0, boss_rematches.ACTIVE))
                self.assertEqual(watcher[-1], boss_rematches.instruction(1000, 4, 'B', 1))

    def test_lamp_choice_and_text_are_generated_together(self):
        files = self.files
        relative = Path('script/talk/m24_01_00_00.talkesdbnd.dcx')
        archive = Bnd4(dcx_decompress(files[relative]))
        script = esd.parse(archive.files[archive.index('t241002.esd')])
        menu = script.group(enhanced_lamps.LAMP_MENU_GROUP)
        self.assertEqual(len(menu.states), 14)
        gate = script.states[script.state_in(menu, 11)]
        condition = script.conds[script.runs[gate.conds].items[0]]
        self.assertEqual(script.exprs[condition.eval].code,
                         enhanced_lamps._eq(esd.call(15, [boss_rematches.DEFEATED]), 1))
        action = script.states[script.state_in(menu, 13)]
        command = script.cmds[script.runs[action.entry].items[0]]
        self.assertEqual((command.bank, command.id), (1, 11))
        self.assertEqual([script.exprs[i].code for i in script.runs[command.args].items],
                         [esd.done(esd.lit(boss_rematches.REQUEST)), esd.done(esd.lit(1))])

        languages = sorted(path for path in files if path.parts[0] == 'msg')
        self.assertEqual(len(languages), 22)
        for path in languages:
            messages = Bnd4(dcx_decompress(files[path]))
            event_text = menu_assets.fmg_read(messages.files[
                messages.ids.index(menu_assets.EVENT_TEXT)])
            self.assertEqual(event_text[14000337].decode('utf-16-le'), 'Rematch Cleric Beast')

    def test_unknown_event_file_fails_closed(self):
        source = self.dvdroot / 'event' / 'm24_01_00_00.emevd.dcx'
        from hashlib import sha256
        original = dcx_decompress(source.read_bytes())
        self.assertEqual(sha256(original).hexdigest(), boss_rematches.INPUT_SHA256['m24_01_00_00'])
        with self.assertRaises(boss_rematches.UnsupportedRematch):
            boss_rematches.INPUT_SHA256['m24_01_00_00'] = '0' * 64
            try:
                boss_rematches.make(self.dvdroot)
            finally:
                boss_rematches.INPUT_SHA256['m24_01_00_00'] = sha256(original).hexdigest()
