"""No screenshots may enter release downloads or installable packages."""
from pathlib import Path
import tempfile
import subprocess
import tarfile
import io
import unittest
from unittest.mock import patch
from release_policy import check_documentation, check_names, check_tree, check_lha_names, expected_assets, check_repository_names, check_repository


class ReleasePolicyTests(unittest.TestCase):
    def test_png_names_rejected_including_nested_and_uppercase(self):
        for name in ('screen.png', 'Source/Demo.PNG', 'App.app/Contents/screenshot.PnG'):
            with self.assertRaises(ValueError):
                check_names([name])
        check_names(['TermicNative', 'Program.info', 'ReadMe', 'LICENSE', 'termic-amiga.json'])

    def test_repository_allows_only_documentation_pngs(self):
        check_repository_names(['README.md', 'docs/screenshots/workspace.png'])
        for name in ('docs/screen.png', 'amiga/native/screen.png', 'release/screen.PNG'):
            with self.assertRaises(ValueError):
                check_repository_names([name])

    def test_documentation_png_is_excluded_from_source_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.check_output(['git', *args], cwd=root, stderr=subprocess.DEVNULL)
            git('init', '-q')
            (root/'docs/screenshots').mkdir(parents=True)
            (root/'docs/screenshots/workspace.png').write_bytes(b'synthetic fixture')
            (root/'README.md').write_text('![Workspace](docs/screenshots/workspace.png)')
            (root/'.gitattributes').write_text('/docs/screenshots export-ignore\n')
            git('add', '.')
            check_repository(root)
            git('-c', 'user.name=Fixture', '-c', 'user.email=user@example.com',
                '-c', 'commit.gpgsign=false', 'commit', '-qm', 'Synthetic documentation fixture')
            with tarfile.open(fileobj=io.BytesIO(git('archive', 'HEAD'))) as archive:
                self.assertIn('README.md', archive.getnames())
                self.assertNotIn('docs/screenshots/workspace.png', archive.getnames())
            (root/'.gitattributes').write_text('')
            with self.assertRaises(ValueError):
                check_repository(root)

    def test_install_tree_rejects_png(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);(root/'docs').mkdir()
            (root/'ReadMe').write_text('Synthetic package')
            check_tree(root)
            (root/'docs/screen.png').write_bytes(b'fixture')
            with self.assertRaises(ValueError):
                check_tree(root)

    def test_documentation_cannot_link_release_pngs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);(root/'docs').mkdir()
            readme = root/'README.md';readme.write_text('See docs for installation.')
            check_documentation(root)
            readme.write_text('![fixture](https://example.invalid/releases/download/v1/test.png)')
            with self.assertRaises(ValueError):
                check_documentation(root)

    def test_nested_sdk_lha_listing_rejects_png(self):
        with patch('release_policy.subprocess.check_output', return_value='-rw-r--r-- 100 100 SDK/docs/screen.PNG\n'):
            with self.assertRaises(ValueError):
                check_lha_names('sdk.lha', 'lha')

    def test_release_manifest_is_downloads_only(self):
        assets = expected_assets('0.2.0-beta')
        self.assertEqual(len(assets), 6)
        self.assertIn('Termic-Amiga-0.2.0-beta.lha', assets)
        self.assertIn('Termic-AI-Proxy-macOS.zip', assets)
        self.assertIn('Amiga-MCP-MultiClient-macOS-arm64.zip', assets)
        check_names(assets)


if __name__ == '__main__':
    unittest.main()
