#!/usr/bin/env python3
"""Build a pinned offline Tiny runtime. No upload, target install or retraining."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ('source', 'model', 'notices', 'vc-config', 'tools', 'output'):
        parser.add_argument('--' + field, type=Path, required=True)
    args = parser.parse_args()
    lock = json.loads((ROOT/'amiga/native/tiny-lock.json').read_text())
    for name, expected in lock['files'].items():
        if name.startswith('src/'):
            assert digest(args.source/name) == expected, 'Tiny source pin mismatch: ' + name
        elif name in ('LICENSE', 'DATA_LICENSES.md'):
            assert digest(args.notices/name) == expected, 'Tiny notice pin mismatch: ' + name
    assert digest(args.model) == lock['converted_sha256'], 'Tiny model pin mismatch'
    args.output.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env['PATH'] = str(args.tools.resolve()) + os.pathsep + env.get('PATH', '')
    vc = str((args.tools/'vc').resolve())
    common = [vc, '+' + str(args.vc_config.resolve()), '-cpu=68000', '-c99', '-O2']
    obj = args.output/'tiny.o'; asm = args.output/'matmul.o'
    subprocess.run(common + ['-DUSE_ASM', '-c', str((args.source/'src/tiny68k.c').resolve()),
                             '-o', str(obj.resolve())], env=env, check=True)
    subprocess.run([str((args.tools/'vasmm68k_mot').resolve()), '-quiet', '-Fhunk', '-m68000', '-no-opt',
                    str((args.source/'src/matmul68k.s').resolve()), '-o', str(asm.resolve())], env=env, check=True)
    subprocess.run(common + ['-o', str((args.output/'tiny68k').resolve()), str(obj.resolve()), str(asm.resolve())], env=env, check=True)
    obj.unlink(); asm.unlink()
    assert (args.output/'tiny68k').read_bytes()[:4] == b'\0\0\3\xf3'
    (args.output/'tiny68k').chmod(0o755)
    shutil.copyfile(args.model, args.output/'tiny.t68')
    for name in ('LICENSE', 'DATA_LICENSES.md'):
        shutil.copyfile(args.notices/name, args.output/name)
    (args.output/'ReadMe').write_text(
        'TINY 2.1 by WollyMonjac: https://github.com/WollyMonjac/Tiny-2.1\n'
        'Amiga port: https://github.com/SkiltonUSA/tiny68k\n'
        'Model tiny-2.1, 367K parameters, local 68k inference, 72-character questions.\n'
        'No network, API key, Mac proxy, code-generation tools or conversation memory.\n'
        'Copy this whole drawer to Termic:Tools/Tiny and select Tiny in Agents.\n'
        'Contains information from mledoze/countries (ODbL) and\n'
        'Periodic-Table-JSON (CC BY-SA 3.0); see DATA_LICENSES.md.\n'
        'Training kit: https://github.com/WollyMonjac/Tiny-2.1/tree/main/training\n')
    receipt = dict(lock)
    receipt['artifacts'] = {name: digest(args.output/name) for name in
                           ('tiny68k', 'tiny.t68', 'LICENSE', 'DATA_LICENSES.md', 'ReadMe')}
    (args.output/'provenance.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print('Offline Tiny runtime ready:', args.output)


if __name__ == '__main__':
    main()
