#!/usr/bin/env python3
"""Opt-in custom-titlebar acceptance on an already running Termic candidate.

Uses a native window probe and real injected input on Termic's custom controls.
Does not type into the editor or change preferences; X uses normal save-and-quit. Native Intuition drag loops cannot be
validated with synthetic dragging; these controls use Termic's own event handler.
"""

import asyncio, json, re, subprocess, urllib.request
from pathlib import Path
from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client


async def main(args):
    Path(".context").mkdir(exist_ok=True)
    async with streamablehttp_client("http://127.0.0.1:55010/mcp") as (r, w, _):
        async with ClientSession(r, w) as s:
            await s.initialize()

            async def call(n, a):
                r = await s.call_tool(n, a)
                t = "\n".join(getattr(x, "text", "") for x in r.content)
                print(n, t, flush=True)
                assert not r.isError and "ERROR:" not in t, t
                await asyncio.sleep(0.2)
                return t

            await call(
                "amiga_push_file",
                {
                    "local_path": str(args.probe.resolve()),
                    "amiga_path": args.install + "/ChromeProbe",
                },
            )
            await call(
                "amiga_run_script",
                {"script": "Protect " + args.install + "/ChromeProbe +e", "timeout": 5},
            )

            async def state():
                t = await call(
                    "amiga_run_script",
                    {"script": args.install + "/ChromeProbe", "timeout": 5},
                )
                p = dict(re.findall(r"(\w+)=([0-9a-f]+)", t))
                assert "address" in p and int(p["address"], 16), t
                return {k: int(v, 16 if k == "address" else 10) for k, v in p.items()}

            async def point(x, y):
                for _ in range(4):
                    p = await state()
                    assert p["active"] and p["front"], p
                    if abs(p["mx"] - x) <= 1 and abs(p["my"] - y) <= 1:
                        return
                    await call(
                        "amiga_input_mouse_move", {"dx": x - p["mx"], "dy": y - p["my"]}
                    )
                raise AssertionError("Pointer did not reach target")

            async def click(x, y):
                await point(x, y)
                try:
                    await call(
                        "amiga_input_click", {"button": "left", "direction": "down"}
                    )
                finally:
                    await call(
                        "amiga_input_click", {"button": "left", "direction": "up"}
                    )
                await asyncio.sleep(1)

            original = await state()
            assert original["width"] == 1400 and original["height"] == 650, original
            # Editor tab in the fixed acceptance layout, without typing into source.
            await click(original["x"] + 550, original["y"] + 108)
            await click(original["x"] + original["width"] - 30, original["y"] + 20)
            zoom = await state()
            assert zoom["width"] == 1920 and zoom["x"] == 0, zoom
            await click(zoom["x"] + zoom["width"] - 30, zoom["y"] + 20)
            restored = await state()
            for k in ["x", "y", "width", "height"]:
                assert restored[k] == original[k], (k, original, restored)
            await point(restored["x"] + 300, restored["y"] + 20)
            try:
                await call("amiga_input_click", {"button": "left", "direction": "down"})
                await call("amiga_input_mouse_move", {"dx": 60, "dy": 40})
                await asyncio.sleep(1)
            finally:
                await call("amiga_input_click", {"button": "left", "direction": "up"})
            moved = await state()
            assert (
                moved["x"] == original["x"] + 60 and moved["y"] == original["y"] + 40
            ), moved
            await point(
                moved["x"] + moved["width"] - 15, moved["y"] + moved["height"] - 10
            )
            try:
                await call("amiga_input_click", {"button": "left", "direction": "down"})
                await call("amiga_input_mouse_move", {"dx": 40, "dy": 20})
                await asyncio.sleep(1)
            finally:
                await call("amiga_input_click", {"button": "left", "direction": "up"})
            sized = await state()
            assert (
                sized["width"] == moved["width"] + 40
                and sized["height"] == moved["height"] + 20
            ), sized
            # Restore initial geometry after exercising actual custom controls.
            await call(
                "amiga_window_resize",
                {
                    "window": format(sized["address"], "x"),
                    "width": original["width"],
                    "height": original["height"],
                },
            )
            await call(
                "amiga_window_move",
                {
                    "window": format(sized["address"], "x"),
                    "x": original["x"],
                    "y": original["y"],
                },
            )
            await click(original["x"] + original["width"] - 95, original["y"] + 20)
            behind = await state()
            assert not behind["front"], behind
            await call(
                "amiga_run_script",
                {
                    "script": args.install + "/ChromeProbe front",
                    "timeout": 5,
                },
            )
            foreground = await state()
            assert foreground["front"] and foreground["active"], foreground
            with urllib.request.urlopen(
                "http://127.0.0.1:55010/api/screenshot", timeout=55
            ) as r:
                d = json.load(r)
            subprocess.run(
                [
                    "sips",
                    "-s",
                    "format",
                    "png",
                    d["path"],
                    "--out",
                    ".context/chrome-tested.png",
                ],
                check=True,
            )
            if args.close_at_end:
                await click(original["x"] + 20, original["y"] + 20)
                result = await call(
                    "amiga_run_script",
                    {
                        "script": args.install + "/ChromeProbe",
                        "timeout": 5,
                    },
                )
                assert "address=00000000" in result, result

            Path(".context/chrome-passed.json").write_text(
                json.dumps(
                    {
                        "original": original,
                        "zoom": zoom,
                        "restored": restored,
                        "dragged": moved,
                        "resized": sized,
                        "depth": behind,
                        "close_button": args.close_at_end,
                    },
                    indent=2,
                )
            )


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--close-at-end", action="store_true")
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--install", default="SD032G:Dev/Termic")
    asyncio.run(main(parser.parse_args()))
