#!/usr/bin/env python3
"""Build on the target OS: macOS .app or Windows portable application folder."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[2]
command = [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onedir',
    '--name', 'Termic AI Proxy', '--paths', str(root / 'scripts/amiga'),
    '--copy-metadata', 'PySide6-Essentials', '--copy-metadata', 'shiboken6', '--copy-metadata', 'mcp',
    '--hidden-import', 'cli_relay', '--hidden-import', 'relay_mcp',
    '--hidden-import', 'mcp.client.streamable_http', '--hidden-import', 'mcp.server.stdio', '--distpath', str(root / 'dist/proxy'),
    '--workpath', str(root / '.context/proxy-build'), '--specpath', str(root / '.context')]
if sys.platform == 'darwin':
    command += ['--windowed', '--osx-bundle-identifier', 'com.skiltonusa.termic.ai-proxy']
else:
    command += ['--windowed']
command += [str(root / 'scripts/proxy/app.py')]
subprocess.run(command, check=True, cwd=root)
