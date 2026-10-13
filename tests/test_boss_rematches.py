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

    def test_generated_events_keep_permanent_progress_flags(self):
        files = self.files

        def writes(script, flag):
            return [(int.from_bytes(row[:8], 'little'), bank, code, args)
                    for row in script.events
                    for bank, code, args in script.event_instructions(
                        int.from_bytes(row[:8], 'little'))
                    if (bank, code) == (2003, 2)
                    and int.from_bytes(args[:4], 'little') == flag]

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
            self.assertEqual(len(modified.events), len(original.events) + len(added))
            for event_id in added:
                self.assertTrue(modified.event_instructions(event_id))
            for row in original.events:
                event_id = int.from_bytes(row[:8], 'little')
                before = original.event_instructions(event_id)
                after = modified.event_instructions(event_id)
                if name == 'common':
                    self.assertEqual(after[:-1] if event_id == 0 else after, before, event_id)
                elif event_id not in (0, boss_rematches.DEFEATED, boss_rematches.FIRST_ENTRY):
                    self.assertEqual(len(after), len(before), event_id)
                    for old, new in zip(before, after):
                        if all(flag.to_bytes(4, 'little') not in old[2]
                               for flag in (boss_rematches.DEFEATED, boss_rematches.FIRST_ENTRY)):
                            self.assertEqual(new, old, event_id)
            for flag in (boss_rematches.DEFEATED, boss_rematches.FIRST_ENTRY):
                self.assertEqual(writes(modified, flag), writes(original, flag))
            if name == 'm24_01_00_00':
                boss = modified.event_instructions(boss_rematches.DEFEATED)
                self.assertEqual(boss[0], boss_rematches.instruction(
                    1003, 101, 'BBBi', 0, 0, 0, boss_rematches.ARENA_DEFEATED))
                # The private arena view must update for the original first kill as well as
                # rematches, before either branch can end the boss event.
                self.assertEqual(boss[16], boss_rematches._set(
                    boss_rematches.ARENA_DEFEATED, True))
                self.assertEqual(boss[17], boss_rematches.instruction(
                    1003, 1, 'BBBi', 2, 0, 0, boss_rematches.ACTIVE))
                # The start warps with the game's own instruction to a respawn point the
                # vanilla map has, never to a point only an edited map would add.
                start = modified.event_instructions(boss_rematches.START_EVENT)
                self.assertEqual(start[-2], boss_rematches.instruction(
                    2003, 49, 'i', boss_rematches.LAMP_RESPAWN))
                self.assertNotIn((2002, 8), [(bank, code) for bank, code, _ in start])
                entry = modified.event_instructions(boss_rematches.FIRST_ENTRY)
                self.assertEqual(entry[1], boss_rematches.instruction(
                    1003, 101, 'BBBi', 0, 0, 0, boss_rematches.ARENA_ENTRY))
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
