import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'launcher'))
import bbport_launcher_win as core
import bbport_startup
from bbport_launcher_backend import LauncherBackend


class FirstRunWizardTests(unittest.TestCase):
    def setUp(self):
        parent = Path(__file__).resolve().parents[1] / 'out'
        parent.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=parent)
        self.root = Path(self.temporary.name)
        self.patches = [patch.object(core, 'CONFIG_DIR', self.root),
                        patch.object(core, 'CONFIG_FILE', self.root / 'settings.json'),
                        patch.object(core, 'DATA_DIR', self.root),
                        patch.dict(os.environ, {'BB_CONFIG': str(self.root / 'bbport.ini')}),
                        patch.dict(core.APP_DEFAULTS, {'check_updates': False,
                                                       'game_dir': str(self.root / 'missing')}),
                        patch.object(LauncherBackend, '_detect_gpu', lambda self: None)]
        for active in self.patches:
            active.start()

    def tearDown(self):
        for active in reversed(self.patches):
            active.stop()
        self.temporary.cleanup()

    def settings(self):
        return {**core.APP_DEFAULTS, **json.loads((self.root / 'settings.json').read_text(encoding='utf-8'))}

    def route(self, args=()):
        return bbport_startup.route(self.settings(), list(args), self.root, False)

    def test_wizard_survives_partial_setup_and_stops_after_finish(self):
        first = LauncherBackend()
        first.setValue('game_dir', str(self.root / 'game'))
        self.assertEqual(self.route()['page'], 'Setup')
        first.completeSetup()
        saved = self.settings()
        self.assertTrue(saved['setupCompleted'])
        self.assertEqual(saved['setupVersion'], bbport_startup.SETUP_VERSION)
        self.assertNotEqual(self.route()['page'], 'Setup')

    def test_finished_setup_with_game_launches_directly(self):
        game = self.root / 'CUSA03173'
        game.mkdir()
        (game / 'eboot.bin').write_bytes(b'\0')
        backend = LauncherBackend()
        backend.setValue('game_dir', str(game))
        backend.completeSetup()
        self.assertEqual(self.route()['mode'], 'direct')
        self.assertEqual(self.route(['--launcher'])['page'], 'Home')
        self.assertEqual(self.route(['--setup'])['page'], 'Setup')
        backend.setValue('launchDirectly', False)
        self.assertEqual(self.route()['mode'], 'launcher')

    def test_skip_opens_game_page_with_fix_banner(self):
        backend = LauncherBackend()
        backend.skipSetup()
        self.assertTrue(self.settings()['setupSkipped'])
        self.assertEqual(backend.startupProblem, 'Game files not found')
        route = self.route()
        self.assertEqual((route['page'], route['problem'], route['step']), ('Game', 'game', 1))
        backend.dismissProblem()
        self.assertEqual(backend.startupProblem, '')

    def test_fix_banner_clears_once_game_folder_is_valid(self):
        backend = LauncherBackend({'mode': 'launcher', 'page': 'Game', 'step': 1, 'problem': 'game'})
        self.assertEqual(backend.startPage, 'Game')
        self.assertEqual(backend.fixStep, 1)
        self.assertEqual(backend.startupProblem, 'Game files not found')
        game = self.root / 'game'
        game.mkdir()
        (game / 'eboot.bin').write_bytes(b'\0')
        backend.setValue('game_dir', str(game))
        self.assertEqual(backend.startupProblem, '')

    def test_bundled_xbox_prompts_are_on_by_default_and_only_on_controller_page(self):
        source = Path(__file__).resolve().parents[1] / 'packaging' / 'windows' / 'mods'
        import shutil
        shutil.copytree(source, self.root / 'mods')
        backend = LauncherBackend()
        self.assertEqual(backend.promptChoice, 'Xbox')
        self.assertEqual(backend.contentMods, [])
        backend.setPrompt('PlayStation')
        self.assertEqual(backend.promptChoice, 'PlayStation')
        # Keyboard imports the player's PCBButtons download (a stand-in archive here), enables it
        # instead of the Xbox images and binds the keys its images show.
        import zipfile
        download = self.root / 'PCBButtons-460-1-2.zip'
        with zipfile.ZipFile(download, 'w') as archive:
            archive.writestr('PCBButtons/common.tpf.dcx', b'DCX\0' + b'\0' * 60)
            archive.writestr('PCBButtons/BBlauncher keybind.txt', 'cross = e\n')
        backend.setPrompt('Keyboard', str(download))
        self.assertEqual(backend.promptChoice, 'Keyboard')
        profile = json.loads((self.root / 'mods.json').read_text())
        self.assertIn('Xbox 360 Prompts (Elden Ring)', profile['disabled'])
        self.assertIn(core.KEYBOARD_PROMPTS, profile['enabled'])
        self.assertTrue((self.root / 'mods' / core.KEYBOARD_PROMPTS / 'menu' / 'common.tpf.dcx').is_file())
        ini = (self.root / 'bbport.ini').read_text(encoding='utf-8')
        self.assertIn('key_roll=Left Shift', ini)
        self.assertIn('key_lock_on=Mouse3', ini)
        env = core.game_environment(backend.app)
        self.assertEqual((env['BB_BUTTON_PROMPTS'], env['BB_KEY_PROMPT_IMAGES']), ('Keyboard', '1'))
        self.assertNotIn(core.KEYBOARD_PROMPTS, [m['name'] for m in backend.contentMods])
        backend.setPrompt('Xbox')
        profile = json.loads((self.root / 'mods.json').read_text())
        self.assertIn(core.KEYBOARD_PROMPTS, profile['disabled'])
        self.assertEqual(backend.promptChoice, 'Xbox')

    def test_keyboard_prompts_refuse_a_download_without_the_images(self):
        import zipfile
        download = self.root / 'other.zip'
        with zipfile.ZipFile(download, 'w') as archive:
            archive.writestr('readme.txt', 'nothing here')
        backend = LauncherBackend()
        backend.setPrompt('Keyboard', str(download))
        self.assertNotEqual(backend.promptChoice, 'Keyboard')
        self.assertIn('common.tpf.dcx', backend.notice)

    def test_launch_options_moved_out_of_advanced(self):
        backend = LauncherBackend()
        keys = {field['key'] for page in ('Advanced', 'Diagnostics') for field in backend.fields(page)}
        self.assertFalse(keys & {'close_on_play', 'launchDirectly', 'check_updates'})
        self.assertEqual([f['key'] for f in backend.fields('Diagnostics')],
                         ['frame_stats', 'gpu_profile', 'vk_validation'])
        pages = ('Graphics', 'Display', 'Game', 'Controller', 'Cheats', 'Online', 'Mods', 'Advanced', 'Diagnostics')
        every = [field['key'] for page in pages for field in backend.fields(page)]
        self.assertEqual(len(every), len(set(every)), 'a setting appears on two pages')


if __name__ == '__main__':
    unittest.main()
