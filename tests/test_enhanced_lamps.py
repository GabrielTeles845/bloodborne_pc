"""Static validation of the experimental lamp patch against a real 1.09 dump."""
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import enhanced_lamps  # noqa: E402
import esd  # noqa: E402
from fromsoft import Bnd4, dcx_decompress  # noqa: E402


@unittest.skipUnless(os.environ.get('BB_TEST_GAME_DIR'), 'BB_TEST_GAME_DIR not set')
class LampPrototypeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dvdroot = Path(os.environ['BB_TEST_GAME_DIR']) / 'dvdroot_ps4'
        cls.outputs = enhanced_lamps.make(cls.dvdroot)

    def test_only_target_scripts_change_and_every_output_round_trips(self):
        self.assertEqual(len(enhanced_lamps.INPUTS), 46)
        self.assertEqual(len(self.outputs), 15)
        changed = 0
        for path, packed in self.outputs.items():
            before = Bnd4(dcx_decompress((self.dvdroot / path).read_bytes()))
            after = Bnd4(dcx_decompress(packed))
            self.assertEqual(before.names, after.names)
            target = {name for map_name, name in enhanced_lamps.INPUTS
                      if map_name == path.name.split('.')[0]}
            for name, old_data, new_data in zip(before.names, before.files, after.files):
                short = name.replace('\\', '/').rsplit('/', 1)[-1]
                if short in target:
                    self.assertNotEqual(old_data, new_data)
                    script = esd.parse(new_data)
                    menu = script.group(enhanced_lamps.LAMP_MENU_GROUP)
                    self.assertIsNotNone(menu)
                    active = script.group(enhanced_lamps.ACTIVE_GROUP)
                    site = script.states[script.state_in(active, enhanced_lamps.ACTIVE_STATE)]
                    call = script.cmds[script.runs[site.entry].items[0]]
                    self.assertEqual((call.bank, call.id), (6, enhanced_lamps.LAMP_MENU_GROUP))
                    options = script.states[script.state_in(menu, 1)]
                    menu_commands = [script.cmds[x].id for x in script.runs[options.entry].items]
                    self.assertEqual(menu_commands, [20, 19, 19, 19, 76])
                    warp = script.states[script.state_in(menu, 4)]
                    warp_call = script.cmds[script.runs[warp.entry].items[0]]
                    self.assertEqual((warp_call.bank, warp_call.id),
                                     (6, enhanced_lamps.VANILLA_WARP_GROUP))
                    self.assertEqual([script.exprs[x].code for x in script.runs[warp_call.args].items],
                                     [b'\x40\xb8\xa1', b'\x41\xb8\xa1', b'\x42\xb8\xa1'])
                    end = script.states[script.state_in(menu, 5)]
                    self.assertEqual(script.conds[script.runs[end.conds].items[0]].target, -1)
                    self.assertEqual(esd.serialize(script), new_data)
                    changed += 1
                else:
                    self.assertEqual(old_data, new_data)
        self.assertEqual(changed, 46)

    def test_modified_source_is_rejected(self):
        (map_name, script_name), digest = next(iter(enhanced_lamps.INPUTS.items()))
        packed = (self.dvdroot / 'script' / 'talk' / f'{map_name}.talkesdbnd.dcx').read_bytes()
        archive = Bnd4(dcx_decompress(packed))
        script = archive.files[archive.index(script_name)]
        with self.assertRaises(enhanced_lamps.UnsupportedLamp):
            enhanced_lamps.add_lamp_menu(script + b'changed', digest)


if __name__ == '__main__':
    unittest.main()
