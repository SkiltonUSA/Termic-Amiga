#!/usr/bin/env python3
"""Fetch the pinned Amiga MicroPython runtime and its upstream license/docs."""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request
ROOT = Path(__file__).resolve().parents[2]
def fetch(output):
    manifest = json.loads((ROOT/'amiga/native/micropython-lock.json').read_text())
    output.mkdir(parents=True, exist_ok=True)
    for name, entry in manifest['files'].items():
        target = output/name
        data = target.read_bytes() if target.exists() else b''
        if hashlib.sha256(data).hexdigest() != entry['sha256']:
            with urllib.request.urlopen(entry['url'], timeout=60) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != entry['sha256']:
                raise ValueError('MicroPython checksum mismatch: '+name)
            if name == 'micropython' and data[:4] != b'\x00\x00\x03\xf3':
                raise ValueError('Expected an Amiga Hunk executable')
            temporary = target.with_name(name+'.download')
            temporary.write_bytes(data)
            temporary.replace(target)
        target.chmod(0o755 if name == 'micropython' else 0o644)
    (output/'provenance.json').write_text(json.dumps(manifest,indent=2)+'\n')
if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    fetch(parser.parse_args().output)
