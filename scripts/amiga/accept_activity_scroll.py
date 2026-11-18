"""Run the synthetic native MUI scrolling regression. Stop AI Proxy first.
Requires an existing DevBench bridge; never reads production task content.
"""
import argparse
import asyncio
from pathlib import Path
import time
import zlib
from codex_relay import Bridge, RelayLock, default_state

async def main(args):
    raw = args.binary.read_bytes()
    assert raw[:4] == b'\0\0\3\xf3', 'Expected an Amiga Hunk executable'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    lock = RelayLock(default_state())
    remote = 'RAM:ta-' + str(int(time.time()))
    try:
        async with Bridge(args.bridge) as bridge:
            assert await bridge.read('request.json') is None, 'Wait for the active task'
            await bridge.call('amiga_push_file', {'local_path':str(args.binary.resolve()), 'amiga_path':remote})
            check = await bridge.call('amiga_checksum', {'path':remote})
            assert f'{zlib.crc32(raw):08X}' in check and f'{len(raw)} bytes' in check
            await bridge.call('amiga_run_script', {'script':'FailAt 21\nProtect '+remote+' +e\nStack 262144\n'+remote+' >'+remote+'.log', 'timeout':10})
            await bridge.call('amiga_pull_file', {'amiga_path':remote+'.log', 'local_path':str(args.output.resolve())})
            result = args.output.read_text()
            print(result, end='')
            assert 'Activity follow-bottom: PASS' in result, 'Native MUI scroll check failed'
            await bridge.shell('Delete '+remote+' '+remote+'.log')
    finally:
        lock.close()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--bridge', default='http://127.0.0.1:55110/mcp')
    parser.add_argument('--output', type=Path, default=Path('.context/activity-scroll/result.log'))
    asyncio.run(main(parser.parse_args()))
