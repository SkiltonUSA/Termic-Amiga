"""Offline documents preserve source links and licence scope across packages."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from release_docs import plain_readme, write_docs, ROOT
from release_policy import check_package_docs


class ReleaseDocumentsTests(unittest.TestCase):
    def test_readme_keeps_useful_links_but_no_image_dependencies(self):
        result = plain_readme('# Title\n![UI](docs/screenshots/test.png)\n'
            '**Setup**: [Guide](docs/setup.md) and [Web](https://acme.com).\n'
            '| Name | Value |\n| --- | --- |\n| A | B |\n'
            '```sh\necho "**literal**"\n```\n', 'https://acme.com/source/')
        self.assertIn('Guide (https://acme.com/source/docs/setup.md)', result)
        self.assertIn('Web (https://acme.com)', result)
        self.assertIn('Name / Value', result)
        self.assertIn('echo "**literal**"', result)
        for bad in ('test.png', '![UI]', '| ---', '```', '**Setup**'):
            self.assertNotIn(bad, result)

    def test_main_readme_and_complete_licence_are_bundled(self):
        with tempfile.TemporaryDirectory() as temp:
            write_docs(temp, 'native', ref='v0.2.2-beta')
            root = Path(temp)
            text = (root/'README.txt').read_text(encoding='ascii')
            self.assertIn('Termic Amiga', text)
            self.assertIn('double-click Termic Amiga', text)
            self.assertIn('/blob/v0.2.2-beta/README.md', text)
            about = (root/'ABOUT-LICENSE.txt').read_text()
            self.assertIn((ROOT/'LICENSE').read_text(), about)
            self.assertIn('Original Termic: Simion Agavriloaei', about)
            self.assertIn('Components retain their own licences', about)

    def test_gateway_keeps_its_own_licence(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp);licence = root/'MIT'
            licence.write_text('MIT License\nSynthetic upstream notice.\n')
            with self.assertRaises(ValueError):
                write_docs(root/'package', 'bridge')
            write_docs(root/'package', 'bridge', upstream_license=licence)
            about = (root/'package/ABOUT-LICENSE.txt').read_text()
            self.assertIn(licence.read_text(), about)
            self.assertIn('gateway is MIT licensed', about)

    def test_package_requires_both_documents_beside_application(self):
        check_package_docs(['App/README.txt', 'App/ABOUT-LICENSE.txt', 'App/Program'])
        for names in (['App/README.txt'], ['A/README.txt', 'B/ABOUT-LICENSE.txt'],
                      ['App/_internal/README.txt', 'App/_internal/ABOUT-LICENSE.txt']):
            with self.assertRaises(ValueError):
                check_package_docs(names)

    def test_proxy_packaging_preserves_payload(self):
        spec = importlib.util.spec_from_file_location('proxy_package', ROOT/'scripts/proxy/package.py')
        module = importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp);bundle = root/'input';bundle.mkdir()
            (bundle/'Termic AI Proxy.exe').write_bytes(b'MZ synthetic executable')
            (bundle/'_internal').mkdir();(bundle/'_internal/dependency.dll').write_bytes(b'synthetic dependency')
            output = module.prepare(bundle, root/'output')
            for name in ('Termic AI Proxy.exe', '_internal/dependency.dll'):
                self.assertEqual((output/name).read_bytes(), (bundle/name).read_bytes())
            check_package_docs('output/'+p.name for p in output.iterdir() if p.is_file())
            with self.assertRaises(ValueError):module.prepare(bundle, output)

if __name__ == '__main__':
    unittest.main()
