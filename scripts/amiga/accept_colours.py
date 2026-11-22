#!/usr/bin/env python3
"""Opt-in hardware colour/import/export check; only operates on a new fixture."""

import argparse
import asyncio
import time
from pathlib import Path
from bridge_client import (AcceptanceRun, ClientSession, add_bridge_arguments,
                           capture_png, workspace_cleanup)
from mcp.client.streamable_http import streamablehttp_client


async def main(args):
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    async with (
        AcceptanceRun(out, args.server, args.reservation_file) as run,
        streamablehttp_client(run.url) as (r, w, _),
    ):
        async with ClientSession(r, w, bridge_url=run.url, reservation_path=run.path) as s:
            await s.initialize()

            async def call(name, args):
                result = await s.call_tool(name, args)
                text = "\n".join(getattr(c, "text", "") for c in result.content)
                with (out / "evidence.log").open("a") as f:
                    f.write(name + "\n" + text + "\n")
                if result.isError:
                    raise RuntimeError(text)
                return text

            async def key(k):
                windows = await call("amiga_list_screen_windows", {})
                first = windows.split("\n\n", 1)[-1].splitlines()[0].strip()
                assert first.startswith("Termic Amiga Workspace  pos="), first
                for direction in ["down", "up"]:
                    await call("amiga_input_key", {"rawkey": k, "direction": direction})
                await asyncio.sleep(0.3)

            await call(
                "amiga_push_file",
                {
                    "local_path": str(
                        Path(".context/amiga-native/TermicNativeMUI").resolve()
                    ),
                    "amiga_path": "Termic:TermicColour",
                },
            )
            s.cleanup = lambda: workspace_cleanup(s)
            await key("0x59")
            fixture = "TMGIT:Projects/colour-" + str(int(time.time()))
            await call(
                "amiga_run_script",
                {
                    "script": "FailAt 10\nMakeDir "
                    + fixture
                    + "\nCopy Termic:Template/main.c "
                    + fixture
                    + "/main.c\nCopy Termic:Template/task.txt "
                    + fixture
                    + "/task.txt\nProtect Termic:TermicColour +e\nRun >NIL: <NIL: Termic:TermicColour --workspace "
                    + fixture,
                    "timeout": 15,
                },
            )
            await asyncio.sleep(3)
            await key("0x51")
            mapping = {
                c: hex(start + i)
                for row, start in [
                    ("qwertyuiop", 0x10),
                    ("asdfghjkl", 0x20),
                    ("zxcvbnm", 0x31),
                ]
                for i, c in enumerate(row)
            }
            mapping.update({"/": "0x3a", " ": "0x40", "\n": "0x44"})
            for c in "// colour\n":
                await key(mapping[c])
            await key("0x57")
            await asyncio.sleep(1)
            await call(
                "amiga_pull_file",
                {
                    "amiga_path": fixture + "/main.c",
                    "local_path": str((out / "saved.c").resolve()),
                },
            )
            expected = b"// colour\n" + Path("amiga/native/Project/main.c").read_bytes()
            assert (
                out / "saved.c"
            ).read_bytes() == expected, "Formatting altered saved source"
            await key("0x59")
            await call(
                "amiga_run_script",
                {"script": "Run >NIL: <NIL: Termic:TermicColour", "timeout": 10},
            )
            await asyncio.sleep(2)
            await key("0x51")
            await capture_png(s, out / "editor.png", "Termic Amiga Workspace")
            run.passed = {"fixture": fixture, "saved_source_exact": True}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('.context/amiga-colours'))
    add_bridge_arguments(parser)
    asyncio.run(main(parser.parse_args()))
