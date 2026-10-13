import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'launcher'))
import bbport_coop as coop


class CoopTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        patcher = mock.patch.dict(os.environ, {'LOCALAPPDATA': self.folder.name})
        patcher.start()
        self.addCleanup(patcher.stop)
        self.addCleanup(self.folder.cleanup)

    def test_invite_round_trip_and_damage(self):
        code = coop.make_invite('100.65.141.9', 'abc123')
        self.assertTrue(code.startswith(coop.INVITE_PREFIX))
        self.assertEqual(coop.read_invite('  ' + code + '\n'), ('100.65.141.9', 'abc123'))
        with self.assertRaises(coop.CoopError):
            coop.read_invite('hello')
        with self.assertRaises(coop.CoopError):
            coop.read_invite(coop.INVITE_PREFIX + '!!!')

    def test_registration_key_is_made_once(self):
        cfg = coop.package_dir('server') / 'server' / 'shadnet.cfg'
        cfg.parent.mkdir(parents=True)
        cfg.write_text('[General]\nHost=0.0.0.0\nRegistrationSecretKey=\n', encoding='utf-8')
        key = coop.registration_key()
        self.assertEqual(len(key), 40)
        self.assertEqual(coop.registration_key(), key)
        self.assertIn(f'RegistrationSecretKey={key}', cfg.read_text(encoding='utf-8'))

    def test_environment_points_this_launch_at_the_host(self):
        base = {'BB_ONLINE': '0', 'BB_SHADNET_NPID': 'normal', 'BB_GAME_DIR': 'game'}
        env = coop.co_op_environment(base, '100.65.141.9', 'hunter', 'secret', Path('bin/bb-probe.exe'))
        self.assertEqual(base['BB_SHADNET_NPID'], 'normal')  # the normal environment is untouched
        self.assertEqual(env['BB_SHADNET_SERVER'], '100.65.141.9:31313')
        self.assertEqual(env['BB_SHADNET_WEBAPI'], 'http://100.65.141.9:31315')
        self.assertEqual(env['BB_SHADNET_NPID'], 'hunter')
        self.assertTrue(env['BB_PROBE'].endswith('bbsc-launch.exe'))
        self.assertEqual(env['BB_GAME_DIR'], 'game')
        overrides = json.loads(Path(env['BB_HOST_OVERRIDES']).read_text(encoding='utf-8'))
        self.assertEqual(overrides['https://ss4.scej-network.jp:20443'], 'http://100.65.141.9:20443')
        self.assertEqual(overrides['https://thehuntersdream.com:18671/summon_messenger'],
                         'http://100.65.141.9:31315')

    def test_unsafe_package_paths_are_refused(self):
        data = io.BytesIO()
        with zipfile.ZipFile(data, 'w') as archive:
            archive.writestr('../outside.txt', 'x')
        with zipfile.ZipFile(data) as archive, self.assertRaises(coop.CoopError):
            coop._safe_extract(archive, Path(self.folder.name) / 'unpack')

    def test_firewall_rules_stay_on_tailscale_and_the_home_network(self):
        script = coop._firewall_script(r'C:\Games\bbport\bin\bb-probe.exe')
        self.assertIn("@('100.64.0.0/10','LocalSubnet')", script)
        self.assertNotIn('Any', script)
        self.assertEqual(script.count('New-NetFirewallRule'), len(coop.FIREWALL_PORTS) + 1)

    def test_tailscale_addresses(self):
        self.assertTrue(coop.is_tailscale('100.65.141.9'))
        self.assertFalse(coop.is_tailscale('100.128.0.1'))
        self.assertFalse(coop.is_tailscale('192.168.1.5'))

    def test_account_names_are_checked_before_asking_the_server(self):
        with self.assertRaises(coop.CoopError):
            coop.create_account('100.65.141.9', '', 'x', 'secret')
        with self.assertRaises(coop.CoopError):
            coop.create_account('100.65.141.9', '', 'hunter', 'abc')


if __name__ == '__main__':
    unittest.main()
