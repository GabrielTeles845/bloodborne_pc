import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'launcher'))
import bbport_launcher_win as core
from bbport_launcher_backend import LauncherBackend


class FirstRunWizardTests(unittest.TestCase):
    def test_wizard_survives_partial_setup_and_stops_after_finish(self):
        parent = Path(__file__).resolve().parents[1] / 'out'
        parent.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent) as temporary:
            root = Path(temporary)
            with (patch.object(core, 'CONFIG_DIR', root),
                  patch.object(core, 'CONFIG_FILE', root / 'settings.json'),
                  patch.dict(os.environ, {'BB_CONFIG': str(root / 'bbport.ini')}),
                  patch.dict(core.APP_DEFAULTS, {'check_updates': False}),
                  patch.object(LauncherBackend, '_detect_gpu', lambda self: None)):
                first = LauncherBackend()
                self.assertTrue(first.setupWizardNeeded)
                first.setValue('game_dir', str(root / 'game'))
                self.assertTrue(LauncherBackend().setupWizardNeeded)
                first.finishSetupWizard()
                self.assertFalse(LauncherBackend().setupWizardNeeded)


if __name__ == '__main__':
    unittest.main()
