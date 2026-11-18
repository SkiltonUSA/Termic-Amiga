#!/usr/bin/env python3
"""Capture the shipped AmigaGuide in a separate MultiView instance.
Uses a unique ARexx port and never closes an existing user document.
"""
import argparse
import asyncio
import json
import re
import time
from pathlib import Path
from codex_relay import Bridge
from bridge_client import DEFAULT_BRIDGE, capture_png


def window_titles(text):
    return set(re.findall(r'^\s*(.+?)\s+pos=', text, re.M))


async def main(args):
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    stage = 'SD032G:Dev/Termic/Acceptance/guide-' + str(int(time.time()))
    port = 'TGALGUIDE'
    (out / 'stage.json').write_text(json.dumps({'stage': stage}))
    async with Bridge(args.server, stage) as b:
        assert port not in await b.call('amiga_list_ports', {})
        await b.shell('MakeDir ' + stage)
        for source, name in [(Path('amiga/native/Termic.guide'), 'Termic.guide')]:
            await b.call('amiga_push_file', {'local_path': str(source.resolve()),
                                           'amiga_path': stage + '/' + name})
            await b.verify_file(name, source.read_bytes())
        before = window_titles(await b.call('amiga_list_screen_windows', {}))
        launched = False
        try:
            await b.call('amiga_launch', {'command': 'SYS:Utilities/MultiView ' +
                         stage + '/Termic.guide PORTNAME ' + port})
            launched = True
            title = None
            for _ in range(30):
                await asyncio.sleep(1)
                titles = window_titles(await b.call('amiga_list_screen_windows', {})) - before
                candidates = [t for t in titles if 'Termic' in t and 'Loading' not in t]
                if len(candidates) == 1:
                    title = candidates[0]
                    break
            assert title, 'No unique loaded Termic guide window appeared'
            assert port in await b.call('amiga_list_ports', {})
            # MultiView opens in front. Capture at its native geometry;
            # do not resize a datatype window during loading.
            async def in_front():
                windows = await b.call('amiga_list_screen_windows', {})
                titles = re.findall(r'^\s*(.+?)\s+pos=', windows, re.M)
                return bool(titles and titles[0] == title)
            await asyncio.sleep(2)
            assert await in_front(), 'Guide is obscured; no screenshot taken'
            await capture_png(b.session, out / 'guide.png', title)
            assert await in_front(), 'Guide was obscured during capture'
            (out / 'passed.json').write_text(json.dumps({'guide': True, 'title': title,
                                                       'fixture': stage}, indent=2))
            print('Captured guide.png', flush=True)
        finally:
            state = await b.session.control('amiga_bridge_status', {})
            if not state.get('enabled'):
                print('Bridge paused; inspect the target before guide cleanup.', flush=True)
            elif launched and port in await b.call('amiga_list_ports', {}):
                await b.call('amiga_arexx_send', {'port': port, 'command': 'QUIT'})
                for _ in range(20):
                    if port not in await b.call('amiga_list_ports', {}):
                        break
                    await asyncio.sleep(.5)
                else:
                    raise AssertionError('Gallery guide did not close')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--server', default=DEFAULT_BRIDGE)
    asyncio.run(main(parser.parse_args()))
