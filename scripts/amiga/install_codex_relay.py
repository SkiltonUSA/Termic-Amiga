#!/usr/bin/env python3
"""Install an on-demand Mac relay and a Finder launcher. No login/startup changes."""
import argparse
from pathlib import Path
import shlex
import shutil
import subprocess
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--directory', type=Path, default=Path.home() / 'Library/Application Support/Termic-Amiga/relay')
    p.add_argument('--launcher', type=Path, default=Path.home() / 'Applications/Termic Amiga Codex.command')
    p.add_argument('--codex', default=shutil.which('codex'))
    args = p.parse_args()
    if not args.codex:
        p.error('Install Codex CLI and run codex login first')
    args.directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    venv = args.directory / 'venv'
    if not (venv / 'bin/python').exists():
        subprocess.run([sys.executable, '-m', 'venv', str(venv)], check=True)
    python = venv / 'bin/python'
    subprocess.run([str(python), '-m', 'pip', 'install', '--disable-pip-version-check', 'mcp==1.30.0'], check=True)
    script = args.directory / 'codex_relay.py'
    shutil.copyfile(Path(__file__).with_name('codex_relay.py'), script)
    command = [str(python), str(script), '--codex', args.codex, '--state-dir', str(args.directory)]
    subprocess.run(command + ['--check'], check=True)
    args.launcher.parent.mkdir(parents=True, exist_ok=True)
    args.launcher.write_text('#!/bin/zsh\n' + shlex.join(command) + '\nresult=$?\n'
                             'echo "Relay stopped. Press Return to close."\nread reply\nexit $result\n')
    args.launcher.chmod(0o700)
    print('Installed Mac relay launcher:', args.launcher)
    print('Double-click the launcher. Keep its window open; Ctrl C stops the relay.')

if __name__ == '__main__':
    main()
