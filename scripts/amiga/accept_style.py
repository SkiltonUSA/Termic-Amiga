#!/usr/bin/env python3
"""Opt-in hardware UI test in a fresh installation; restores app and clipboard.

Requires a running DevBench MCP server. Pass built GUI and inspect_ui helper.
Do not run while a person uses the Amiga. No credentials are read or submitted.
"""

import argparse
import asyncio
import json
from pathlib import Path
import re
import struct
import subprocess
import time
from bridge_client import ClientSession, capture_png
from mcp.client.streamable_http import streamablehttp_client

ROOT = Path(__file__).resolve().parents[2]


async def main(args):
    out = ROOT / ".context/amiga-style"
    out.mkdir(parents=True, exist_ok=True)
    fixture = args.install + "/style-" + str(int(time.time()))
    app_title = "Termic Amiga Workspace"
    mapping = {
        c: hex(start + i)
        for row, start in [("qwertyuiop", 0x10), ("asdfghjkl", 0x20), ("zxcvbnm", 0x31)]
        for i, c in enumerate(row)
    }
    mapping.update({"-": "0x0b", ",": "0x38", "\n": "0x44", " ": "0x40"})
    async with streamablehttp_client("http://127.0.0.1:55110/mcp") as (r, w, _):
        async with ClientSession(r, w) as session:
            await session.initialize()

            async def call(name, data):
                result = await session.call_tool(name, data)
                text = "\n".join(getattr(c, "text", "") for c in result.content)
                with (out / "evidence.log").open("a") as f:
                    f.write(name + "\n" + text + "\n")
                if result.isError:
                    raise RuntimeError(text)
                return text

            async def shell(script):
                result = await call(
                    "amiga_run_script",
                    {"script": "FailAt 10\nStack 262144\n" + script, "timeout": 15},
                )
                assert "[STILL RUNNING]" not in result, result
                assert "[ERROR]" not in result, result
                return result

            async def key(raw, mods=(), title=app_title):
                windows = await call("amiga_list_screen_windows", {})
                first = windows.split("\n\n", 1)[-1].splitlines()[0].strip()
                assert first.startswith(title + "  pos="), first
                try:
                    for mod in mods:
                        await call(
                            "amiga_input_key", {"rawkey": mod, "direction": "down"}
                        )
                    await call("amiga_input_key", {"rawkey": raw, "direction": "down"})
                finally:
                    await call("amiga_input_key", {"rawkey": raw, "direction": "up"})
                    for mod in reversed(mods):
                        await call(
                            "amiga_input_key", {"rawkey": mod, "direction": "up"}
                        )
                await asyncio.sleep(0.25)

            async def close_app():
                # Native MUI strings consume F-keys while editing. Leave the
                # field with Tab before retrying; never launch over a live app.
                for _ in range(16):
                    await key("0x59")
                    windows = await call("amiga_list_screen_windows", {})
                    if app_title + "  pos=" not in windows:
                        return
                    await key("0x42")
                raise AssertionError("Application did not close; preserve its assign")

            async def type_text(text):
                for char in text:
                    await key(mapping[char])

            async def capture(title, filename):
                await capture_png(session, out / filename, title)

            async def push(local, remote):
                await call(
                    "amiga_push_file",
                    {"local_path": str(Path(local).resolve()), "amiga_path": remote},
                )

            async def pull(remote, name):
                local = out / name
                await call(
                    "amiga_pull_file", {"amiga_path": remote, "local_path": str(local)}
                )
                return local.read_bytes()

            probe = await shell(
                "If EXISTS ENV:Termic\n Echo ENV_ALREADY_PRESENT\nEndIf"
            )
            assert (
                "ENV_ALREADY_PRESENT" not in probe
            ), "Preserve existing ENV preferences; test requires no ENV:Termic."
            await shell(
                "MakeDir "
                + fixture
                + "\nMakeDir "
                + fixture
                + "/Config\nMakeDir "
                + fixture
                + "/Projects\nMakeDir "
                + fixture
                + "/Projects/fixture\nEcho saved-model >"
                + fixture
                + "/Config/model"
            )
            await push(args.gui, fixture + "/TermicNative")
            await push(args.probe, fixture + "/InspectUI")
            await push(ROOT / "amiga/native/Termic.guide", fixture + "/Termic.guide")
            (out / "main.c").write_text("int main(void) { return 0; }\n")
            (out / "task.txt").write_text("Synthetic UI fixture.\n")
            await push(out / "main.c", fixture + "/Projects/fixture/main.c")
            await push(out / "task.txt", fixture + "/Projects/fixture/task.txt")
            await shell(
                "Protect "
                + fixture
                + "/TermicNative +e\nProtect "
                + fixture
                + "/InspectUI +e\n"
                + fixture
                + "/InspectUI save-clip "
                + fixture
                + "/clipboard.original"
            )
            started = False
            closed_original = False
            try:
                await close_app()
                closed_original = True
                await shell(
                    "Run >NIL: <NIL: "
                    + fixture
                    + "/TermicNative --workspace Termic:Projects/fixture"
                )
                started = True
                await asyncio.sleep(2)
                await key("0x02", ("0x67",))
                if not args.mui_custom_menus:
                    menu = await shell(fixture + "/InspectUI menus")
                    assert re.findall(r"^MENU (.+)$", menu, re.M) == [
                        "Project",
                        "Edit",
                        "Workspace",
                        "View",
                        "Settings",
                        "Help",
                    ], menu
                    assert "ITEM Copy enabled=0" in menu, menu
                    assert "ITEM Save enabled=1 key=S" in menu, menu
                # Select the newly typed prefix and use standard native menu keys.
                await type_text("ab")
                await key("0x4f", ("0x60",))  # Shift-left
                await key("0x4f", ("0x60",))
                if not args.mui_custom_menus:
                    menu = await shell(fixture + "/InspectUI menus")
                    assert "ITEM Copy enabled=1 key=C" in menu, menu
                await key(mapping["c"], ("0x67",))
                await shell(
                    fixture + "/InspectUI save-clip " + fixture + "/clipboard.fixture"
                )
                clip = await pull(fixture + "/clipboard.fixture", "clipboard.iff")
                assert clip[:4] == b"FORM" and clip[8:12] == b"FTXT", clip[:12]
                chunks, offset = {}, 12
                while offset + 8 <= len(clip):
                    name, length = (
                        clip[offset : offset + 4],
                        struct.unpack(">I", clip[offset + 4 : offset + 8])[0],
                    )
                    chunks[name] = clip[offset + 8 : offset + 8 + length]
                    offset += 8 + length + (length & 1)
                assert chunks.get(b"CHRS") == b"ab", chunks
                await key("0x01", ("0x67",))  # focus task instructions
                await key(mapping["v"], ("0x67",))
                await key(mapping["s"], ("0x67",))
                task = await pull(
                    fixture + "/Projects/fixture/task.txt", "saved-task.txt"
                )
                assert task == b"abSynthetic UI fixture.\n", task
                await key("0x02", ("0x67",))
                source = await pull(
                    fixture + "/Projects/fixture/main.c", "saved-main.c"
                )
                assert source == b"abint main(void) { return 0; }\n", source
                await key(mapping["z"], ("0x67",))
                await key(mapping["s"], ("0x67",))
                source = await pull(
                    fixture + "/Projects/fixture/main.c", "undone-main.c"
                )
                assert source == b"a" + (out / "main.c").read_bytes(), source
                # Remove the remaining fixture prefix with normal editing.
                await key("0x41")  # Backspace
                await key(mapping["s"], ("0x67",))
                source = await pull(
                    fixture + "/Projects/fixture/main.c", "clean-main.c"
                )
                assert source == (out / "main.c").read_bytes(), source
                await capture(app_title, "editor.png")
                # Right-Amiga comma invokes Preferences and focuses the model field.
                await key(mapping[","], ("0x67",))
                await key(mapping["x"], ("0x67",))  # native String clear
                await type_text("temporary-model")
                await capture(app_title, "preferences-edited.png")
                for _ in range(7):
                    await key("0x42")
                await capture(app_title, "preferences-use-focus.png")
                await key("0x44")  # Use
                persisted = await shell(
                    "Type "
                    + fixture
                    + "/Config/model\nIf EXISTS ENV:Termic/model\n Type ENV:Termic/model\nEndIf"
                )
                assert (
                    "saved-model" in persisted and "temporary-model" in persisted
                ), persisted
                await key(mapping[","], ("0x67",))
                await key(mapping["x"], ("0x67",))
                await type_text("discarded-model")
                for _ in range(8):
                    await key("0x42")
                await key("0x44")  # Cancel
                await key(mapping[","], ("0x67",))
                await capture(app_title, "preferences.png")
                # Save the restored effective (temporary) model as the new default.
                for _ in range(6):
                    await key("0x42")
                await key("0x44")  # Save
                persisted = await shell("Type " + fixture + "/Config/model")
                assert (
                    "temporary-model" in persisted
                    and "discarded-model" not in persisted
                ), persisted
                await key("0x5f", ("0x60",))
                windows = await call("amiga_list_screen_windows", {})
                assert "About Termic Amiga  pos=" in windows, windows
                await capture("About Termic Amiga", "about.png")
                await key("0x45", title="About Termic Amiga")
                await key("0x5f")
                await asyncio.sleep(2)
                windows = await call("amiga_list_screen_windows", {})
                first = (
                    windows.split("\n\n", 1)[-1]
                    .splitlines()[0]
                    .strip()
                    .split("  pos=")[0]
                )
                assert "Termic" in first and first != app_title, windows
                (out / "guide-window.txt").write_text(first)
                await capture(first, "guide.png")
                await key(mapping["q"], ("0x67",), title=first)
                await asyncio.sleep(1)
                # Quit shortcut must leave the whole app, then restore production below.
                await key(mapping["q"], ("0x67",))
                windows = await call("amiga_list_screen_windows", {})
                assert app_title + "  pos=" not in windows, windows
                started = False
                (out / "passed.json").write_text(
                    json.dumps(
                        {
                            "fixture": fixture,
                            "menu_shortcuts": True,
                            "intuition_menu_structure": not args.mui_custom_menus,
                            "clipboard_ftxt": True,
                            "preferences_use_cancel_save": True,
                            "guide": True,
                            "about": True,
                        },
                        indent=2,
                    )
                )
            finally:
                if started:
                    windows = await call("amiga_list_screen_windows", {})
                    first = windows.split("\n\n", 1)[-1].splitlines()[0].strip()
                    if first.startswith("About Termic Amiga  pos="):
                        await key("0x45", title="About Termic Amiga")
                    elif first.startswith("Termic Amiga - User Guide  pos="):
                        await key(
                            mapping["q"], ("0x67",), title="Termic Amiga - User Guide"
                        )
                    windows = await call("amiga_list_screen_windows", {})
                    if app_title + "  pos=" in windows:
                        await close_app()
                await shell(
                    fixture
                    + "/InspectUI restore-clip "
                    + fixture
                    + "/clipboard.original\nDelete "
                    + fixture
                    + "/clipboard.original\nIf EXISTS ENV:Termic\n Delete ENV:Termic ALL\nEndIf"
                )
                if closed_original:
                    await shell("Run >NIL: <NIL: " + args.install + "/TermicNative")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gui", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--install", default="SD032G:Termic-Amiga")
    parser.add_argument(
        "--mui-custom-menus",
        action="store_true",
        help="MUI 5: verify menu shortcuts; skip unavailable Intuition MenuStrip inspection",
    )
    asyncio.run(main(parser.parse_args()))
