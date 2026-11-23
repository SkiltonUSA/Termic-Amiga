#!/usr/bin/env python3
"""Create a local LHA installation kit from verified SDKs and Hunk binaries.

Never packages a live installation, personal project, credentials or logs.
Use original upstream SDK archives; their licenses remain inside each archive.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import urllib.request
from fetch_micropython import fetch as fetch_micropython
from release_policy import check_tree, check_lha_names

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "amiga/native"
SCRIPTS = [
    "Setup",
    "BuildNative",
    "BuildProject",
    "InstallSDK",
    "StartAgent",
    "StartBuild",
    "StartRun",
    "TestHTTPS",
    "ReadMe",
    "Termic.guide",
    "InstallUI",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--downloads", type=Path, required=True)
    parser.add_argument("--binaries", type=Path, required=True)
    parser.add_argument("--tiny", type=Path, required=True, help="Offline runtime drawer produced by build_tiny.py")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--lha", default="lha")
    parser.add_argument(
        "--download", action="store_true", help="Fetch missing SDK archives"
    )
    args = parser.parse_args()
    tiny_lock = json.loads((SOURCE / "tiny-lock.json").read_text())
    tiny_receipt = json.loads((args.tiny / "provenance.json").read_text())
    for name in ("tiny68k", "tiny.t68", "LICENSE", "DATA_LICENSES.md", "ReadMe"):
        actual = hashlib.sha256((args.tiny/name).read_bytes()).hexdigest()
        if actual != tiny_receipt['artifacts'][name]:
            raise ValueError("Tiny runtime checksum mismatch: " + name)
    if hashlib.sha256((args.tiny/'tiny.t68').read_bytes()).hexdigest() != tiny_lock['converted_sha256']:
        raise ValueError("Tiny model does not match the pinned offline model")
    if (args.tiny/'tiny68k').read_bytes()[:4] != b"\0\0\3\xf3":
        raise ValueError("Tiny runtime is not an Amiga Hunk executable")
    manifest = json.loads((SOURCE / "sdk-lock.json").read_text())
    args.downloads.mkdir(parents=True, exist_ok=True)
    runtime = args.downloads / "MicroPython"
    if args.download:
        fetch_micropython(runtime)
    runtime_lock = json.loads((SOURCE / "micropython-lock.json").read_text())
    for name, entry in runtime_lock['files'].items():
        if hashlib.sha256((runtime/name).read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError("MicroPython checksum mismatch: " + name)
    for name, entry in manifest.items():
        path = args.downloads / name
        if not path.exists() and args.download:
            with urllib.request.urlopen(entry["url"], timeout=60) as response:
                path.write_bytes(response.read())
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError(f"SDK checksum mismatch: {name}")
    output = args.output.resolve()
    if output.exists():
        raise ValueError("Choose a new output path; existing archives are not modified")
    output.parent.mkdir(parents=True, exist_ok=True)
    lha = shutil.which(args.lha)
    if not lha:
        raise ValueError("Supply an LHA executable with --lha")
    lha = str(Path(lha).resolve())
    with tempfile.TemporaryDirectory() as directory:
        stage = Path(directory) / "Termic-Amiga"
        stage.mkdir()
        for name in ("TermicNative", "TermicAgent", "TermicStorage", "TermicMCP", "TermicCheck"):
            binary = args.binaries / name
            if binary.read_bytes()[:4] != b"\x00\x00\x03\xf3":
                raise ValueError(f"Not an Amiga Hunk executable: {name}")
            shutil.copyfile(binary, stage / name)
        launcher = args.binaries / "TermicLauncher"
        if launcher.read_bytes()[:4] != b"\x00\x00\x03\xf3":
            raise ValueError("Not an Amiga Hunk executable: TermicLauncher")
        shutil.copyfile(launcher, stage / "Termic Amiga")
        for name, destination in (("Termic Amiga.info", stage / "Termic Amiga.info"),
                                  ("Termic-Amiga.info", Path(directory) / "Termic-Amiga.info")):
            icon = args.binaries / name
            if icon.read_bytes()[:2] != b"\xe3\x10":
                raise ValueError("Not a Workbench DiskObject: " + name)
            shutil.copyfile(icon, destination)
        for script in SCRIPTS:
            shutil.copyfile(SOURCE / script, stage / script)
        (stage / "Source").mkdir()
        for path in sorted(SOURCE.glob("*")):
            if path.suffix in (".c", ".h"):
                shutil.copyfile(path, stage / "Source" / path.name)
        shutil.copytree(SOURCE / "vendor", stage / "Source/vendor")
        shutil.copytree(ROOT / "amiga/apps/process-monitor", stage / "apps/process-monitor")
        shutil.copytree(SOURCE / "Project", stage / "Project")
        shutil.copytree(SOURCE / "Project", stage / "Template")
        shutil.copytree(SOURCE / "Project", stage / "Source/Template")
        shutil.copytree(SOURCE / "Examples", stage / "Examples")
        shutil.copytree(SOURCE / "Prompts", stage / "Prompts")
        (stage / "Tools/MicroPython").mkdir(parents=True)
        for name in runtime_lock['files']:
            shutil.copyfile(runtime/name, stage/"Tools/MicroPython"/name)
        (stage/"Tools/MicroPython/micropython").chmod(0o755)
        shutil.copyfile(SOURCE/"micropython-lock.json",stage/"Tools/MicroPython/provenance.json")
        (stage / "Tools/Tiny").mkdir()
        for name in ("tiny68k", "tiny.t68", "LICENSE", "DATA_LICENSES.md", "ReadMe", "provenance.json"):
            shutil.copyfile(args.tiny/name, stage/"Tools/Tiny"/name)
        (stage/"Tools/Tiny/tiny68k").chmod(0o755)
        (stage / "Config").mkdir()
        (stage / "Config/api-key").write_text("")
        (stage / "Config/model").write_text("gpt-5-mini\n")
        (stage / "SDK").mkdir()
        for name in manifest:
            shutil.copyfile(args.downloads / name, stage / "SDK" / name)
        shutil.copyfile(SOURCE / "sdk-lock.json", stage / "SDK/sdk-lock.json")
        shutil.copyfile(ROOT / "LICENSE", stage / "LICENSE")
        # Amiga lha preserves executable attributes from POSIX permissions.
        for path in stage.iterdir():
            if path.is_file():
                path.chmod(
                    0o755
                    if path.name in ("TermicNative", "TermicAgent", "TermicStorage", "TermicMCP", "TermicCheck", "Termic Amiga")
                    else 0o644
                )
        check_tree(stage)
        for archive in (stage/'SDK').glob('*.lha'):
            check_lha_names(archive, lha)
        subprocess.run(
            [lha, "aq", str(output), stage.name, "Termic-Amiga.info"], cwd=directory, check=True
        )
        subprocess.run([lha, "tq", str(output)], check=True, capture_output=True)
    print(f"{output}\nSHA256 {hashlib.sha256(output.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
