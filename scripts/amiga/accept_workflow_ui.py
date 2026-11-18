#!/usr/bin/env python3
"""Opt-in MUI workflow acceptance on an idle Amiga; synthetic task copies only.

Compile tests/test_workflow_window.c with the GUI dependencies and embedded
monitor, plus tests/input_gesture.c and tests/inspect_chrome.c. The installed
TermicAgent, BuildProject and VBCC must work. Stop AI Proxy before running.
The normal Termic window can remain open. Do not use the mouse during this test.
"""
import argparse
import asyncio
import json
from pathlib import Path
import re
import shutil
import subprocess
import time
import zlib

from codex_relay import Bridge, RelayLock, default_state

TITLES = {
    'board': 'Termic: Kanban board',
    'keymap': 'Termic: Configure shortcuts',
    'shortcuts': 'Termic: Keyboard shortcuts',
    'broadcast': 'Termic: Broadcast to agents',
    'queue': 'Termic: Task queue',
}


async def accept(args):
    args.output.mkdir(parents=True, exist_ok=True)
    lock = RelayLock(default_state())
    stage = args.drawer.rstrip('/') + '/workflow-' + str(int(time.time()))
    fixture = stage + '/fixture'
    launched = False
    try:
        async with Bridge(args.server) as bridge:
            async def call(name, parameters):
                reply = await bridge.session.call_tool(name, parameters)
                text = '\n'.join(getattr(c, 'text', '') for c in reply.content)
                with (args.output/'acceptance.log').open('a') as log:
                    log.write(name + ' ' + text + '\n')
                if reply.isError or any(line.startswith(('ERROR:', '[ERROR]', '[STILL RUNNING]')) for line in text.splitlines()):
                    raise RuntimeError(text)
                await asyncio.sleep(.3)
                return text

            async def shell(script):
                return await call('amiga_run_script', {'script': 'FailAt 10\nStack 262144\n' + script, 'timeout': 120})

            reader = Bridge(args.server, fixture)
            reader.session = bridge.session

            async def read(name):
                for attempt in range(3):
                    try:
                        text = await reader.read(name)
                        return text or ''
                    except ValueError:
                        # A truncated bridge hex dump is a transport failure.
                        # Retrying this read cannot repeat a fixture action.
                        if attempt == 2:
                            raise
                        await asyncio.sleep(1)

            async def wait_text(name, expected):
                for _ in range(20 if name=="drop.txt" else 90):
                    if expected in await read(name):
                        return
                    if 'FAIL' in await read('result.txt'):
                        raise AssertionError('Native fixture failed')
                    await asyncio.sleep(1)
                raise AssertionError('Missing fixture marker: ' + expected)

            async def window(mode, front=False):
                for _ in range(4):
                    raw = await shell(stage+'/ChromeProbe ' + ('front' if front else 'query') + ' "' + TITLES[mode] + '"')
                    fields = dict(re.findall(r'(\w+)=([0-9a-f]+)', raw))
                    if 'address' in fields:
                        return {k: int(v, 16 if k == 'address' else 10) for k, v in fields.items()}
                raise AssertionError('No valid window response')

            async def capture(mode, name):
                await window(mode, True)
                await asyncio.sleep(2)
                for _ in range(3):
                    raw = await call('amiga_screenshot', {'window': TITLES[mode]})
                    match = re.search(r'Screenshot saved: (.+) \((\d+)x(\d+), (.+), (\d+) rows received\)', raw)
                    if match and match[4] in ('RLE', 'truecolor') and int(match[3]) == int(match[5]):
                        break
                    await asyncio.sleep(1)
                assert match and match[4] in ('RLE', 'truecolor') and int(match[3]) == int(match[5]), raw
                source, target = Path(match[1]), args.output/name
                if source.suffix == '.png':
                    shutil.copyfile(source, target)
                elif shutil.which('sips'):
                    subprocess.run(['sips', '-s', 'format', 'png', str(source), '--out', str(target)], check=True, capture_output=True)
                else:
                    from PIL import Image
                    with Image.open(source) as image:
                        image.save(target)
                print('Captured ' + name, flush=True)

            assert 'TermicJob.' not in await call('amiga_list_ports', {}), 'A job is active'
            assert await bridge.read('request.json') is None, 'A proxy request is pending'
            await shell('MakeDir ' + stage)
            (args.output/'stage.json').write_text(json.dumps({'stage': stage, 'fixture': fixture}))
            for name, local in [('WorkflowCheck', args.binary), ('InputGesture', args.gesture), ('ChromeProbe', args.probe)]:
                await call('amiga_push_file', {'local_path': str(local.resolve()), 'amiga_path': stage+'/'+name})
                assert f'{zlib.crc32(local.read_bytes()):08X}' in await call('amiga_checksum', {'path': stage+'/'+name})
                await shell('Protect ' + stage+'/'+name + ' +e')
            await shell('MakeDir '+fixture+'\nRun >'+fixture+'/native.log <NIL: '+stage+'/WorkflowCheck '+fixture+' board')
            launched = True
            try:
                await wait_text('ready.txt', 'board')
                p = await window('board', True)
                cells = {}
                for line in (await read('geometry.txt')).splitlines():
                    match = re.fullmatch(r'(\d+) (\d+) (\d+) (\d+)', line)
                    if match:
                        cells[int(match[1])] = tuple(map(int, match.groups()[1:]))
                assert len(cells) == 6, cells
                x, y, width = cells[3]
                dx, dy, dw = cells[4]
                # Native input.device sequence carries button qualifiers during
                # the gesture. Read actual geometry; never guess task indices.
                await shell(f'{stage}/InputGesture {p["x"]+x+width//2} {p["y"]+y+8} {p["x"]+x+width//2} {p["y"]+y+8}')
                gesture=await shell(f'FailAt 21\n{stage}/InputGesture {p["x"]+x+width//2} {p["y"]+y+8} {p["x"]+dx+dw//2} {p["y"]+dy+8}\nEcho GESTURE_FINISHED')
                assert "INPUT_GESTURE_PASS" in gesture,gesture
                print(await read('trace.txt'),flush=True)
                drag_error = None
                try:
                    await wait_text('drop.txt', 'DROP_DONE_PASS')
                except AssertionError as error:
                    drag_error = error
                await capture('board', 'kanban-dragged.png' if drag_error is None else 'kanban-drag-failed.png')
                for mode, name in [('keymap', 'shortcut-preferences.png'), ('shortcuts', 'keyboard-shortcuts.png'),
                                   ('broadcast', 'broadcast.png'), ('queue', 'parallel-queue.png')]:
                    await shell('Echo "'+mode+'" >'+fixture+'/control.txt')
                    await wait_text('ready.txt', mode)
                    await capture(mode, name)
                await shell('Echo "close" >'+fixture+'/control.txt')
                await wait_text('result.txt', 'WORKFLOW_WINDOW_PASS')
                launched = False
                if drag_error is not None:
                    raise drag_error
                (args.output/'passed.json').write_text(json.dumps({'native_checks': True, 'parallel_native_builds': True,
                    'keymap_persistence': True, 'broadcast_copies': True, 'mouse_drag': True, 'complete_captures': True}))
                print('Workflow acceptance passed', flush=True)
            finally:
                if launched:
                    # Close only the fixture. In-flight fixture builds finish;
                    # production workers and the normal Termic app stay alive.
                    await shell('Echo "close" >'+fixture+'/control.txt')
    finally:
        lock.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', default='http://127.0.0.1:55110/mcp')
    parser.add_argument('--drawer', default='Termic:Acceptance')
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--gesture', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('.context/workflow-acceptance'))
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9:_/-]+', args.drawer) or not args.drawer.endswith((':Acceptance', '/Acceptance')):
        parser.error('--drawer must be an existing Acceptance drawer without spaces')
    asyncio.run(accept(args))


if __name__ == '__main__':
    main()
