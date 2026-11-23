"""Project release policy: installable assets only, no PNGs or screenshots."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import zipfile


def check_names(names):
    for name in names:
        if str(name).lower().endswith('.png'):
            raise ValueError('PNG files are forbidden in releases and installed packages: ' + str(name))


def check_repository_names(names):
    """Only documentation screenshots may be tracked; packages allow no PNGs."""
    check_names(name for name in names if not str(name).startswith('docs/screenshots/'))


def check_repository(root):
    check_documentation(root)
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=root, text=True)
    check_repository_names(name for name in tracked.split('\0') if name)
    attributes = subprocess.check_output(
        ['git', 'check-attr', '-z', 'export-ignore', '--', 'docs/screenshots'], cwd=root)
    if attributes.split(b'\0')[:3] != [b'docs/screenshots', b'export-ignore', b'set']:
        raise ValueError('Documentation screenshots must be excluded from Git source archives')


def check_tree(root):
    check_names(path for path in Path(root).rglob('*') if path.is_file())


def check_lha_names(path, lha):
    listing = subprocess.check_output([str(Path(lha).resolve()), 'lq', str(Path(path).resolve())], text=True)
    check_names(line.strip() for line in listing.splitlines())


def expected_assets(version):
    assets = {f'Termic-Amiga-{version}.lha', f'Termic-Process-Monitor-{version}.lha',
            'Termic-AI-Proxy-macOS.zip', 'Amiga-MCP-MultiClient-macOS-arm64.zip',
            'termic-amiga.json', 'SHA256SUMS'}
    # Preserve the published 0.2.0 manifest; Windows is a release asset from 0.2.1.
    if version != '0.2.0-beta':
        assets.add('Termic-AI-Proxy-Windows.zip')
    if version not in ('0.2.0-beta', '0.2.1-beta'):
        assets.update({'README.txt', 'ABOUT-LICENSE.txt'})
    return assets


def check_package_docs(names):
    """Require both documents beside the app, never hidden in a dependency tree."""
    names = set(names)
    for readme in names:
        if readme.count('/') == 1 and readme.endswith('/README.txt'):
            if readme.rsplit('/', 1)[0]+'/ABOUT-LICENSE.txt' in names:
                return
    raise ValueError('Package must contain README.txt and ABOUT-LICENSE.txt at its top application level')


def validate_assets(directory, version, lha):
    directory = Path(directory)
    check_tree(directory)
    expected = expected_assets(version)
    if {p.name for p in directory.iterdir()} != expected:
        raise ValueError('Release staging must contain exactly the documented downloads and checksums')
    hashes = {}
    for line in (directory/'SHA256SUMS').read_text().splitlines():
        digest, name = line.split('  ', 1)
        if name in hashes or name not in expected - {'SHA256SUMS'}:
            raise ValueError('Invalid checksum manifest entry')
        hashes[name] = digest
    if hashes.keys() != expected - {'SHA256SUMS'}:
        raise ValueError('Checksum manifest does not cover every release asset')
    for name, digest in hashes.items():
        path = directory/name
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise ValueError('Checksum mismatch: ' + name)
        if path.suffix == '.zip':
            with zipfile.ZipFile(path) as archive:
                check_names(archive.namelist())
                if 'README.txt' in expected:
                    check_package_docs(archive.namelist())
                if archive.testzip():
                    raise ValueError('Damaged ZIP: ' + name)
        elif path.suffix == '.lha':
            with tempfile.TemporaryDirectory() as temp:
                subprocess.run([str(Path(lha).resolve()), 'xq', str(path.resolve())],
                               cwd=temp, check=True, capture_output=True)
                check_tree(temp)
                if 'README.txt' in expected:
                    check_package_docs(p.relative_to(temp).as_posix() for p in Path(temp).rglob('*') if p.is_file())
                for nested in Path(temp).rglob('*.lha'):
                    check_lha_names(nested, lha)
    entry = json.loads((directory/'termic-amiga.json').read_text())
    archive_name = f'Termic-Amiga-{version}.lha'
    if entry['version'] != version or entry['archive']['sha256'] != hashes[archive_name]:
        raise ValueError('AmiPkg version/archive pin differs from this release')
    print('Release asset list, archive contents, no-PNG policy, checksums and AmiPkg pin: PASS')


def check_documentation(root):
    for path in [Path(root)/'README.md', *Path(root).joinpath('docs').glob('*.md')]:
        for line in path.read_text().splitlines():
            if '/releases/download/' in line and '.png' in line.lower():
                raise ValueError('Documentation must not depend on PNG release assets: ' + str(path))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path)
    parser.add_argument('--version')
    parser.add_argument('--lha')
    parser.add_argument('--repository', type=Path)
    args = parser.parse_args()
    if args.repository:
        check_repository(args.repository)
    if args.directory:
        if not args.version or not args.lha:
            parser.error('--directory requires --version and --lha')
        validate_assets(args.directory, args.version, args.lha)
