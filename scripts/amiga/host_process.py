"""Launch native CLIs without interpreting model names as shell commands."""
import os
from pathlib import Path
import shutil
import subprocess

def executable_command(executable):
    path = Path(shutil.which(executable) or executable)
    if os.name == 'nt' and path.suffix.lower() in ('.cmd', '.bat'):
        # npm's Codex shim is shell code. Run its documented Node entry directly.
        entry = path.parent / 'node_modules/@openai/codex/bin/codex.js'
        node = shutil.which('node')
        if path.stem.lower() == 'codex' and entry.is_file() and node:
            return [node, str(entry)]
        raise ValueError('Choose the native agent .exe, not a .cmd or .bat wrapper.')
    return [str(path)]


def process_options():
    return {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
