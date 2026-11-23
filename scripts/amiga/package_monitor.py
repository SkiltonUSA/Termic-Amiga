#!/usr/bin/env python3
"""Package a tested process-monitor binary with source and a Workbench icon."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
from release_policy import check_tree
from release_docs import write_docs

ROOT=Path(__file__).resolve().parents[2]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('binary','icon','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--lha',default='lha');args=p.parse_args()
    if args.binary.read_bytes()[:4]!=b'\0\0\3\xf3':raise ValueError('Expected Amiga Hunk executable')
    if args.icon.read_bytes()[:2]!=b'\xe3\x10':raise ValueError('Expected Workbench DiskObject')
    output=args.output.resolve()
    if output.exists():raise ValueError('Choose a new output path')
    lha=shutil.which(args.lha)
    if not lha:raise ValueError('LhA executable required')
    lha=str(Path(lha).resolve());output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory() as d:
        stage=Path(d)/'TermicProcessMonitor';stage.mkdir()
        shutil.copyfile(args.binary,stage/'Program');(stage/'Program').chmod(0o755)
        shutil.copyfile(args.icon,stage/'Program.info')
        shutil.copytree(ROOT/'amiga/apps/process-monitor',stage/'Source')
        (stage/'ReadMe').write_text('Open Program from Workbench to launch the monitor.\nFor source builds, enter the Source drawer before using the command below.\n\n'+(ROOT/'amiga/apps/process-monitor/README.md').read_text())
        shutil.copyfile(ROOT/'LICENSE',stage/'LICENSE')
        (stage/'services.txt').write_text('# Opt-in only. See ReadMe. No service starts on load.\n# Example|EXAMPLE.SERVICE|Example:Service|EXAMPLE.REQUIRED\n')
        write_docs(stage, "monitor")
        check_tree(stage)
        subprocess.run([lha,'aq',str(output),stage.name],cwd=d,check=True)
        subprocess.run([lha,'tq',str(output)],check=True,capture_output=True)
    print(str(output)+'\nSHA256 '+hashlib.sha256(output.read_bytes()).hexdigest())

if __name__=='__main__':main()
