#!/usr/bin/env python3
"""Verify the packaged multi-client gateway against an isolated fake backend."""
import argparse
import asyncio
from contextlib import AsyncExitStack
import importlib.util
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile

import httpx
from mcp import ClientSession
from mcp.client.streamable_http import streamable_http_client


async def main(args):
    source = args.source.resolve()
    sys.path.insert(0, str(source/'src'))
    spec = importlib.util.spec_from_file_location('gateway_http_fixture', source/'tests/test_http.py')
    fixture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fixture)
    backend, stats, tools = fixture.fake_backend()
    binary = str(args.binary.resolve())
    subprocess.run([binary, '--help'], check=True, stdout=subprocess.DEVNULL, timeout=20)
    with tempfile.TemporaryDirectory(prefix='gateway-bundle-') as temp:
        async with fixture.running(backend) as upstream:
            with socket.socket() as sock:
                sock.bind(('127.0.0.1', 0)); port = sock.getsockname()[1]
            with (Path(temp)/'gateway.log').open('w') as log:
                process = subprocess.Popen([binary, '--backend', upstream+'/mcp', '--port', str(port),
                                            '--state', str(Path(temp)/'reservation.json')], stdout=log, stderr=log)
                try:
                    async with httpx.AsyncClient(timeout=1) as http:
                        async with asyncio.timeout(20):
                            while True:
                                if process.poll() is not None:
                                    raise RuntimeError((Path(temp)/'gateway.log').read_text())
                                try:
                                    response = await http.get(f'http://127.0.0.1:{port}/health')
                                    if response.status_code == 200: break
                                except httpx.TransportError:
                                    pass
                                await asyncio.sleep(.1)
                    async with AsyncExitStack() as stack:
                        sessions = []
                        for _ in range(5):
                            read, write, _ = await stack.enter_async_context(streamable_http_client(f'http://127.0.0.1:{port}/mcp'))
                            session = await stack.enter_async_context(ClientSession(read, write))
                            info = await session.initialize()
                            assert info.serverInfo.name == 'Amiga-MCP'
                            sessions.append(session)
                        owner, other = sessions[:2]
                        assert (await owner.call_tool('echo', {'id': 0})).isError
                        assert not stats['calls'], 'Startup must be paused'
                        assert not (await owner.call_tool('amiga_bridge_enable', {'confirmed_idle': True})).isError
                        replies = await asyncio.gather(*(sessions[i % 5].call_tool('echo', {'id': i}) for i in range(24)))
                        assert [r.content[0].text for r in replies] == [str(i) for i in range(24)]
                        assert stats['peak'] == 1, 'Hardware transactions overlapped'
                        token = json.loads((await owner.call_tool('amiga_bridge_reserve', {'label': 'synthetic bundle test'})).content[0].text)['token']
                        assert (await other.call_tool('write', {'id': 90})).isError
                        assert not (await owner.call_tool('write', {'id': 91})).isError
                        assert not (await other.call_tool('amiga_bridge_resume', {'token': token})).isError
                        assert (await owner.call_tool('write', {'id': 92})).isError
                        assert not (await other.call_tool('amiga_bridge_release', {'token': token, 'confirmed_idle': True})).isError
                        async with httpx.AsyncClient() as http:
                            state = (await http.get(f'http://127.0.0.1:{port}/health')).json()
                            assert state['reservation'] is None and state['queue_capacity'] == 32
                    print('Packaged gateway: five MCP sessions, 24 routed calls, serial backend, pause and ownership recovery: PASS')
                finally:
                    process.terminate()
                    try: await asyncio.to_thread(process.wait, 10)
                    except subprocess.TimeoutExpired:
                        process.kill(); await asyncio.to_thread(process.wait, 5)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    asyncio.run(main(parser.parse_args()))
