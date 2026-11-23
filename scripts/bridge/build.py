#!/usr/bin/env python3
"""Package a pinned Amiga-MCP source export as a standalone macOS gateway.

Use a separate build venv with upstream requirements.lock and PyInstaller.
No running gateway, backend, reservation file or user configuration is copied.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tomllib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "amiga"))
from release_policy import check_tree
from release_docs import write_docs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--revision', required=True, help='Full upstream Git commit ID')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('Build this macOS package on macOS')
    if len(args.revision) != 40 or any(c not in '0123456789abcdef' for c in args.revision):
        parser.error('Supply the full upstream revision')
    source = args.source.resolve()
    metadata = tomllib.loads((source/'pyproject.toml').read_text())
    assert metadata['project']['name'] == 'amiga-mcp'
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    entry = output/'gateway-entry.py'
    entry.write_text('from amiga_mcp.__main__ import main\nif __name__ == "__main__":\n    main()\n')
    subprocess.run([sys.executable, '-m', 'PyInstaller', '--clean', '--noconfirm', '--onedir',
                    '--name', 'Amiga-MCP', '--paths', str(source/'src'),
                    '--collect-submodules', 'amiga_mcp', '--collect-submodules', 'uvicorn',
                    '--hidden-import', 'mcp.client.streamable_http', '--exclude-module', 'mcp.cli', '--copy-metadata', 'mcp',
                    '--distpath', str(output/'dist'), '--workpath', str(output/'work'),
                    '--specpath', str(output), str(entry)], check=True)
    bundle = output/'dist/Amiga-MCP'
    shutil.copytree(source, bundle/'Source')
    shutil.copyfile(source/'LICENSE', bundle/'LICENSE')
    launcher = bundle/'Start-Amiga-MCP.command'
    launcher.write_text('''#!/bin/sh
set -eu
cd "$(dirname "$0")"
printf '%s\\n' 'Amiga-MCP multi-client gateway: http://127.0.0.1:55110/mcp' 'DevBench must already be running on port 55010.' 'Starts paused. Inspect the target, then enable through an MCP client.'
exec ./Amiga-MCP --backend http://127.0.0.1:55010/mcp --port 55110 "$@"
''')
    launcher.chmod(0o755)
    (bundle/'Gateway-Setup.txt').write_text('''Amiga-MCP: multi-client gateway for macOS Apple Silicon

This is the shared multi-client gateway, not the old direct DevBench server.
Up to 64 MCP sessions and 32 queued requests are supported by default.
It serializes complete hardware transactions over one persistent backend
session. Multiple clients can connect; the Amiga does not execute their
hardware commands concurrently.

Keep this whole folder together. Open Start-Amiga-MCP.command, or run
./Amiga-MCP --help in Terminal. No Python installation is required.
The executable is locally signed, not Developer ID notarized. Use macOS
Privacy & Security to approve it if blocked after download.

Prerequisite: an existing connected DevBench MCP server at
http://127.0.0.1:55010/mcp and its Amiga-side bridge. They are not bundled.
Connect Termic AI Proxy and all agents to http://127.0.0.1:55110/mcp.
Do not start a second gateway if one is already serving that backend.

Every start is paused. After confirming the target is idle and direct
clients have finished, call amiga_bridge_enable with confirmed_idle=true
through an MCP client. Never enable after a timeout without inspecting the
Amiga. Hardware tests reserve/resume/release through the gateway's tools.
Stop the gateway with Ctrl-C only after clients and hardware work finish.

The original source, MIT license and pinned dependencies are in Source.
See Source/README.md for the complete workflow and recovery instructions.
provenance.json identifies the exact upstream revision and source hashes.
''')
    files = {str(p.relative_to(source)): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in sorted(source.rglob('*')) if p.is_file()}
    (bundle/'provenance.json').write_text(json.dumps({
        'repository': 'https://github.com/SkiltonUSA/Amiga-MCP',
        'revision': args.revision, 'upstream_version': metadata['project']['version'],
        'architecture': platform.machine(), 'default_sessions': 64, 'default_queue': 32,
        'source_sha256': files,
    }, indent=2)+'\n')
    subprocess.run(['codesign', '--verify', '--strict', str(bundle/'Amiga-MCP')], check=True)
    archive = output/('Amiga-MCP-MultiClient-macOS-'+platform.machine()+'.zip')
    write_docs(bundle, "bridge", upstream_license=source/"LICENSE")
    check_tree(bundle)
    subprocess.run(['ditto', '-c', '-k', '--keepParent', str(bundle), str(archive)], check=True)
    print(archive)


if __name__ == '__main__':
    main()
