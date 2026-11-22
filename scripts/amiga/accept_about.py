#!/usr/bin/env python3
"""Opt-in physical checks for the native About window and Amiga version tags."""

import argparse
import asyncio
import re
from pathlib import Path
from bridge_client import (AcceptanceRun, ClientSession, add_bridge_arguments,
                           capture_png, workspace_cleanup)
from mcp.client.streamable_http import streamablehttp_client


async def main(args):
    version = re.search(
        r'#define TERMIC_VERSION "([^"]+)"', Path("amiga/native/version.h").read_text()
    ).group(1)
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    async with (
        AcceptanceRun(out, args.server, args.reservation_file) as run,
        streamablehttp_client(run.url) as (r, w, _),
    ):
        async with ClientSession(r, w, bridge_url=run.url, reservation_path=run.path) as session:
            await session.initialize()

            async def call(name, args):
                result = await session.call_tool(name, args)
                text = "\n".join(getattr(c, "text", "") for c in result.content)
                with (out / "evidence.log").open("a") as f:
                    f.write(name + "\n" + text + "\n")
                if result.isError:
                    raise RuntimeError(text)
                return text

            async def key(raw, title="Termic Amiga Workspace"):
                windows = await call("amiga_list_screen_windows", {})
                first = windows.split("\n\n", 1)[-1].splitlines()[0].strip()
                assert first.startswith(title + "  pos="), first
                for direction in ["down", "up"]:
                    await call(
                        "amiga_input_key", {"rawkey": raw, "direction": direction}
                    )
                await asyncio.sleep(0.7)

            async def capture(title, name):
                await capture_png(session, out / name, title)

            for local, remote in [
                ("TermicNativeMUI", "TermicAbout"),
                ("TermicAgentMUI", "TermicAgentVer"),
                ("TermicStorage", "TermicStorageVer"),
            ]:
                await call(
                    "amiga_push_file",
                    {
                        "local_path": str(
                            Path(".context/amiga-native", local).resolve()
                        ),
                        "amiga_path": "Termic:" + remote,
                    },
                )
                result = await call(
                    "amiga_run_script",
                    {
                        "script": "Protect Termic:"
                        + remote
                        + " +e\nTermic:"
                        + remote
                        + " --version\nVersion Termic:"
                        + remote
                        + " FILE FULL",
                        "timeout": 10,
                    },
                )
                assert result.count(version) == 2, result
            session.cleanup = lambda: workspace_cleanup(session)
            await key("0x59")
            await call(
                "amiga_run_script",
                {"script": "Run >NIL: <NIL: Termic:TermicAbout", "timeout": 10},
            )
            session.cleanup = lambda: workspace_cleanup(session, about=True)
            await asyncio.sleep(2)
            await key("0x51")  # Focus the embedded editor first
            await key("0x5f")  # Help opens About, even from the embedded editor
            windows = await call("amiga_list_screen_windows", {})
            assert windows.count("About Termic Amiga  pos=") == 1, windows
            await capture("About Termic Amiga", "about.png")
            await key("0x45", "About Termic Amiga")  # Escape closes only About
            windows = await call("amiga_list_screen_windows", {})
            assert (
                "About Termic Amiga  pos=" not in windows
                and "Termic Amiga Workspace  pos=" in windows
            )
            await key("0x5f")
            windows = await call("amiga_list_screen_windows", {})
            assert windows.count("About Termic Amiga  pos=") == 1, windows
            await key("0x44", "About Termic Amiga")  # Return closes the reused window
            await key("0x51")
            await capture("Termic Amiga Workspace", "workspace.png")
            run.passed = {"version": version, "open_close_reopen": True, "version_tags": 3}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('.context/amiga-about'))
    add_bridge_arguments(parser)
    asyncio.run(main(parser.parse_args()))
