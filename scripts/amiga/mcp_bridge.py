#!/usr/bin/env python3
"""MCP stdio connector to TermicMCP through an existing DevBench MCP bridge.

Does not start a listener, alter agent registrations, or print credentials.
Install the Python `mcp` package in the interpreter used by your MCP client.
"""
import argparse
import asyncio
import json
from pathlib import Path
import sys
import tempfile
import uuid
from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client

LIMIT = 65536

async def main(args):
    async with streamablehttp_client(args.bridge) as (reader, writer, _):
        async with ClientSession(reader, writer) as bridge:
            await bridge.initialize()

            async def call(name, data):
                result = await bridge.call_tool(name, data)
                text = '\n'.join(getattr(c, 'text', '') for c in result.content)
                if result.isError or '[ERROR]' in text or 'ERROR:' in text or '[STILL RUNNING]' in text:
                    raise RuntimeError('Amiga bridge operation failed: ' + name)
                return text

            while True:
                raw = await asyncio.to_thread(sys.stdin.buffer.readline, LIMIT + 2)
                if not raw:
                    break
                request = None
                try:
                    if len(raw) > LIMIT:
                        while raw and not raw.endswith(b'\n'):
                            raw = await asyncio.to_thread(sys.stdin.buffer.readline, LIMIT + 2)
                        raise ValueError('Request exceeds 64 KiB')
                    request = json.loads(raw)
                    if not isinstance(request, dict):
                        raise ValueError('Expected one JSON-RPC request')
                    # Notifications never invoke a tool on the Amiga.
                    if 'id' not in request:
                        continue
                    tag = uuid.uuid4().hex
                    remote_in = 'T:termic-mcp-' + tag + '.in'
                    remote_out = 'T:termic-mcp-' + tag + '.out'
                    with tempfile.TemporaryDirectory(prefix='termic-mcp-') as directory:
                        incoming, outgoing = Path(directory)/'request.json', Path(directory)/'reply.json'
                        incoming.write_bytes(raw.rstrip(b'\r\n') + b'\n')
                        try:
                            await call('amiga_push_file', {'local_path': str(incoming), 'amiga_path': remote_in})
                            await call('amiga_run_script', {'script': f'FailAt 10\nStack 262144\nTermic:TermicMCP --request {remote_in} --reply {remote_out}', 'timeout': 15})
                            await call('amiga_pull_file', {'amiga_path': remote_out, 'local_path': str(outgoing)})
                            response = json.loads(outgoing.read_bytes())
                            if response.get('id') != request.get('id'):
                                raise ValueError('Amiga response id mismatch')
                        finally:
                            await call('amiga_run_script', {'script': f'If EXISTS {remote_in}\n Delete {remote_in}\nEndIf\nIf EXISTS {remote_out}\n Delete {remote_out}\nEndIf', 'timeout': 5})
                except Exception:
                    # Do not echo request bodies, credentials or bridge internals.
                    response = {'jsonrpc': '2.0', 'id': request.get('id') if isinstance(request, dict) else None,
                                'error': {'code': -32603, 'message': 'Amiga MCP request failed. Check the bridge, TermicMCP installation and Agent access settings.'}}
                sys.stdout.write(json.dumps(response, separators=(',', ':')) + '\n')
                sys.stdout.flush()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bridge', default='http://127.0.0.1:55010/mcp')
    asyncio.run(main(parser.parse_args()))
