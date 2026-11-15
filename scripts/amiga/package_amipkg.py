#!/usr/bin/env python3
"""Generate a pinned AmiPkg entry for an already-built Termic LHA.

The URL must identify the exact archive being published. This does not upload
an archive, change a catalog, or install anything on a machine.
"""
import argparse
from datetime import date
import hashlib
import json
from pathlib import Path
from urllib.parse import urlparse


def entry(archive, url, version):
    if urlparse(url).scheme not in ('http', 'https') or not urlparse(url).netloc:
        raise ValueError('Supply the HTTP(S) URL of this exact release archive')
    if not version or len(version) > 47:
        raise ValueError('Supply a version of at most 47 characters')
    raw = Path(archive).read_bytes()
    if len(raw) < 7 or raw[2:5] != b'-lh':
        raise ValueError('Expected an LHA archive')
    return {
        'id': 'termic-amiga', 'name': 'Termic Amiga', 'version': version,
        'description': 'Native MUI agent workspace, task board, editor and build tools',
        'conflicts': [], 'provides': [],
        'category': 'Development', 'added': date.today().isoformat(), 'tier': 'A',
        'archive': {'url': url, 'mirrors': [], 'sha256': hashlib.sha256(raw).hexdigest(), 'sizeBytes': len(raw)},
        'requirements': {'minCPU': '68020', 'minKS': '3.2', 'network': True, 'amiSSL': True},
        'deps': [{'id': 'texteditormcc', 'min': '15.56'}, {'id': 'amissl', 'min': '5.27'}],
        'requiredCapabilities': ['copy-tree-v1', 'pre-post-script-v1', 'make-assign-v1'],
        'recipe': {'recipeSchema': 2, 'ops': [
            {'op': 'preScript', 'lines': [
                'FailAt 10',
                'Version muimaster.library 21', 'If WARN', ' Quit 20', 'EndIf',
                'If NOT EXISTS C:lha',
                ' Echo "Install LhA in C: before installing Termic."',
                ' Quit 20', 'EndIf',
                'If EXISTS SYS:Programs/Termic-Amiga',
                ' Echo "Existing Termic drawer: back it up and use the documented upgrade procedure."',
                ' Quit 20', 'EndIf']},
            {'op': 'copyTree', 'src': 'TermicNative', 'dest': 'Programs/Termic-Amiga'},
            {'op': 'makeAssign', 'name': 'Termic', 'path': 'SYS:Programs/Termic-Amiga'},
            {'op': 'postScript', 'lines': [
                'FailAt 10', 'Stack 262144', 'Assign Termic: SYS:Programs/Termic-Amiga',
                'Execute Termic:InstallSDK', 'Execute Termic:InstallUI', 'Termic:TermicCheck',
                'Echo "Termic installed. Run Termic:TermicNative."']}
        ]}
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--archive-url', required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    value = entry(args.archive, args.archive_url, args.version)
    with args.output.open('x') as stream:
        stream.write(json.dumps(value, indent=2) + '\n')
    print(args.output)


if __name__ == '__main__':
    main()
