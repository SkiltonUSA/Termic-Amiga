"""Release metadata must pin exact bytes and preserve existing installations."""
import hashlib
import tempfile
from pathlib import Path
import unittest
from package_amipkg import entry


class PackageTests(unittest.TestCase):
    def test_hash_prerequisites_and_safe_install(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / 'fixture.lha'
            archive.write_bytes(b'\x20\x00-lh5-fixture')
            result = entry(archive, 'https://acme.com/fixture.lha', '0.1')
            self.assertEqual(result['archive']['sha256'], hashlib.sha256(archive.read_bytes()).hexdigest())
            self.assertEqual(result['archive']['sizeBytes'], archive.stat().st_size)
            self.assertEqual(result['requirements']['minCPU'], '68020')
            self.assertNotIn('mui38', [d['id'] for d in result['deps']])
            ops = result['recipe']['ops']
            pre = '\n'.join(ops[0]['lines'])
            self.assertIn('Version muimaster.library 21', pre)
            self.assertIn('If EXISTS SYS:Programs/Termic-Amiga', pre)
            self.assertIn('Quit 20', pre)
            self.assertEqual(ops[1]['dest'], 'Programs/Termic-Amiga')
            self.assertIn('Termic:TermicCheck', ops[-1]['lines'])
            self.assertLess(len(pre), 640)
            self.assertLess(len('\n'.join(ops[-1]['lines'])), 640)
            for url in ('file:///tmp/a.lha', 'https:///missing-host', 'ftp://acme.com/a.lha'):
                with self.assertRaises(ValueError): entry(archive, url, '0.1')
            archive.write_bytes(b'not-an-archive')
            with self.assertRaises(ValueError): entry(archive, 'https://acme.com/a.lha', '0.1')


if __name__ == '__main__':
    unittest.main()
