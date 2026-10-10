from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'launcher'))
import bbport_pkg_setup as setup


class PackageSetupTests(unittest.TestCase):
    def setUp(self):
        scratch = Path(__file__).resolve().parents[1] / 'out' / 'test-tmp'
        scratch.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.extractor = self.root / 'pkg_extractor.exe'
        self.extractor.write_bytes(b'test double')

    def test_zip_install_applies_plain_update_without_replacing_an_existing_game(self):
        archive = self.root / 'cusa-03173.zip'
        with zipfile.ZipFile(archive, 'w') as out:
            out.writestr('cusa-03173/CUSA03173.pkg', b'base')
            out.writestr('cusa-03173/CUSA03173 Update v1.09.pkg', b'patch')
            out.writestr('cusa-03173/CUSA03173 Update v1.09 720p 60fps.pkg', b'wrong')

        inspected = []
        def check(_, package, kind):
            inspected.append((Path(package).read_bytes(), kind))

        def extract(_, package, output, folder, status):
            folder.mkdir()
            (folder / 'eboot.bin').write_bytes(Path(package).read_bytes())
            (folder / 'sce_sys').mkdir()
            (folder / 'sce_sys' / 'param.sfo').write_bytes(b'1.09')

        with patch.object(setup, '_check_type', side_effect=check), patch.object(setup, '_extract', side_effect=extract):
            game = setup.install_game(archive, self.root, self.extractor)
        self.assertEqual(game, self.root / 'CUSA03173')
        self.assertEqual((game / 'eboot.bin').read_bytes(), b'patch')
        self.assertEqual(inspected, [(b'base', 101), (b'patch', 102)])
        self.assertFalse((self.root / 'CUSA03173-patch').exists())
        self.assertFalse(list(self.root.glob('.bbport-install-*')))

        with self.assertRaisesRegex(setup.PackageSetupError, 'already exists'):
            setup.install_game(archive, self.root, self.extractor)
        self.assertEqual((game / 'eboot.bin').read_bytes(), b'patch')

    def test_missing_plain_update_is_rejected(self):
        archive = self.root / 'wrong.zip'
        with zipfile.ZipFile(archive, 'w') as out:
            out.writestr('CUSA03173.pkg', b'base')
            out.writestr('CUSA03173 Update v1.09 1080p 60fps.pkg', b'wrong')
        with self.assertRaisesRegex(setup.PackageSetupError, 'plain'):
            setup.install_game(archive, self.root, self.extractor)
        self.assertFalse((self.root / 'CUSA03173').exists())

    def test_direct_pkg_requires_plain_update(self):
        base = self.root / 'CUSA03173.pkg'
        other = self.root / 'CUSA03173 Update v1.09 720p 60fps.pkg'
        base.write_bytes(b'base')
        other.write_bytes(b'wrong')
        with self.assertRaisesRegex(setup.PackageSetupError, 'plain'):
            setup.install_game(base, self.root, self.extractor, other)


if __name__ == '__main__':
    unittest.main()
