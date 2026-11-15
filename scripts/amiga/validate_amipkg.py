#!/usr/bin/env python3
"""Check an LHA/entry using upstream AmiPkg code and a fresh local install tree.

Supply separately cloned amiga-pkg and amipkg repositories. This does not fetch
or trust a remote catalog, publish an entry, or modify Amiga system drawers.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('archive', 'entry', 'catalog', 'client'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--lha', default='lha')
    args = p.parse_args()
    entry = json.loads(args.entry.read_text())
    raw = args.archive.read_bytes()
    assert entry['archive']['sha256'] == hashlib.sha256(raw).hexdigest()
    assert entry['archive']['sizeBytes'] == len(raw)
    subprocess.run(['python3', str(args.catalog/'amigapkg.py'), 'validate', str(args.entry)], check=True)
    deps = [json.loads((args.catalog/'packages'/ (d['id']+'.json')).read_text()) for d in entry['deps']]
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        index = work/'index.json'
        index.write_text(json.dumps({'schema': 1, 'indexVersion': 1, 'packages': [entry, *deps]}))
        core = args.client.resolve()/'src/core'
        binary = work/'check'
        subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                        '-I', str(core), str(ROOT/'scripts/amiga/validate_amipkg.c'),
                        *[str(core/(name+'.c')) for name in ('aindex','ajson','arecipe','arun','aver','resolve')],
                        '-o', str(binary)], check=True)
        extract=work/'extract';extract.mkdir()
        subprocess.run([str(Path(shutil.which(args.lha)).resolve()), 'xq', str(args.archive.resolve())], cwd=extract, check=True, capture_output=True)
        files=work/'files.txt'
        files.write_text(''.join(str(path.relative_to(extract))+'\n' for path in sorted(extract.rglob('*')) if path.is_file()))
        output=subprocess.check_output([str(binary), str(index), str(files)], text=True)
        for line in output.splitlines():
            if not line.startswith('COPY\t'):continue
            _, src, dest=line.split('\t')
            target=work/'fresh'/dest;target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(extract/src,target)
            assert target.read_bytes()==(extract/src).read_bytes()
        installed=work/'fresh/Programs/Termic-Amiga'
        assert (installed/'Config/api-key').read_bytes()==b''
        for name in ('TermicNative','TermicAgent','TermicMCP','TermicCheck','TermicStorage'):
            assert (installed/name).read_bytes()[:4]==b'\0\0\x03\xf3'
        assert (installed/'Termic.guide').read_text().startswith('@DATABASE')
        assert not list(installed.rglob('queue.json')) and not list(installed.rglob('job.log'))
        print(output.splitlines()[-1])
        print('Fresh local install tree, binary headers, blank credentials and archive hash: PASS')
    print('AmigaDOS pre/post scripts and external dependencies still require target acceptance.')


if __name__ == '__main__':
    main()
