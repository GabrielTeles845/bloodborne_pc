from paths import ROOT
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


run = load('bb_run', 'run.py')
sys.path.insert(0, str(ROOT / 'launcher'))
launcher = load('bb_launcher', 'launcher/bbport_launcher_win.py')
import bbport_coop as coop


class HuntersDreamTests(unittest.TestCase):
    """The Hunter's Dream pool: the engine's own backend (BB_NET_BACKEND=thd), from Supermedo's
    port; and seamless co-op, which must never use it."""

    def setUp(self):
        patcher = mock.patch.object(launcher, 'HUNTERS_DREAM_POOL', True)
        patcher.start()
        self.addCleanup(patcher.stop)
        self.settings = {**launcher.APP_DEFAULTS, 'game_dir': 'x', 'online': True,
                         'online_pool': 'huntersdream', 'thd_online_id': 'Hunter1', 'thd_token': 'tok',
                         'online_npid': 'shadnet_name', 'online_password': 'secret'}

    def test_run_sets_the_backend_and_http_overrides(self):
        with tempfile.TemporaryDirectory() as temp:
            env = {'BB_THD_TOKEN': 't', 'BB_THD_ONLINE_ID': 'Hunter1', 'BB_SHADNET_SERVER': 'x:1'}
            run.hunters_dream(env, Path(temp))
            overrides = json.loads((Path(temp) / 'host_overrides.json').read_text(encoding='utf-8'))
        self.assertEqual(overrides['*.scej-network.jp'], 'https://thehuntersdream.com')
        self.assertEqual(overrides['bb-playlog-prod.s3.amazonaws.com'], 'https://thehuntersdream.com:18671')
        self.assertEqual(env['BB_THD_SERVER'], 'https://thehuntersdream.com:18671')
        self.assertNotIn('BB_SHADNET_SERVER', env)

    def test_run_refuses_the_pool_without_a_sign_in(self):
        with tempfile.TemporaryDirectory() as temp, self.assertRaises(SystemExit):
            run.hunters_dream({}, Path(temp))

    def test_shadnet_pool_sends_game_functions_over_http_only(self):
        with tempfile.TemporaryDirectory() as temp:
            env = {'BB_SHADNET_WEBAPI': 'http://srv.shadps4.net:31315'}
            run.online_overrides(env, Path(temp))
            overrides = json.loads((Path(temp) / 'host_overrides.json').read_text(encoding='utf-8'))
        self.assertFalse(any('31315' in v for v in overrides.values()))
        self.assertEqual(overrides['https://bb-playlog-prod.s3.amazonaws.com:443'],
                         'https://thehuntersdream.com:18671')

    def test_launcher_environment(self):
        env = launcher.game_environment(self.settings)
        self.assertEqual((env['BB_NET_BACKEND'], env['BB_THD_ONLINE_ID'], env['BB_THD_TOKEN']),
                         ('thd', 'Hunter1', 'tok'))
        self.assertNotIn('BB_SHADNET_NPID', env)
        env = launcher.game_environment({**self.settings, 'online_pool': 'shadnet'})
        self.assertNotIn('BB_NET_BACKEND', env)
        self.assertEqual(env['BB_SHADNET_NPID'], 'shadnet_name')
        with mock.patch.object(launcher, 'HUNTERS_DREAM_POOL', False):
            self.assertNotIn('BB_NET_BACKEND', launcher.game_environment(self.settings))
        self.assertNotIn('BB_NET_BACKEND', launcher.game_environment({**self.settings, 'online': False}))

    def test_client_version_names_this_fork(self):
        env = launcher.game_environment(self.settings)
        self.assertEqual(env['BB_CLIENT_VERSION'], f'bbport-jgamrot {launcher.VERSION}')
        self.assertNotIn('shadPS4', env['BB_CLIENT_VERSION'])

    def test_pool_is_shown_unless_switched_off(self):
        with mock.patch.dict(os.environ, {}, clear=False):
            os.environ.pop('BB_HUNTERS_DREAM', None)
            shown = load('bb_launcher_shown', 'launcher/bbport_launcher_win.py')
        with mock.patch.dict(os.environ, {'BB_HUNTERS_DREAM': '0'}):
            hidden = load('bb_launcher_hidden', 'launcher/bbport_launcher_win.py')
        self.assertTrue(shown.HUNTERS_DREAM_POOL)
        self.assertFalse(hidden.HUNTERS_DREAM_POOL)

    def test_co_op_never_uses_the_hunters_dream(self):
        base = launcher.game_environment(self.settings)
        with tempfile.TemporaryDirectory() as temp:
            with mock.patch.dict(os.environ, {'LOCALAPPDATA': temp}):
                env = coop.co_op_environment(base, '100.65.141.9', 'hunter', 'secret',
                                             Path(temp) / 'bbport-windows' / 'bin' / 'bb-probe.exe')
        for key in run.THD_ENV:
            self.assertNotIn(key, env)
        self.assertEqual(env['BB_SHADNET_SERVER'], '100.65.141.9:31313')
        self.assertEqual(base['BB_NET_BACKEND'], 'thd')  # the normal environment is untouched


if __name__ == '__main__':
    unittest.main()
