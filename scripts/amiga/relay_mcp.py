#!/usr/bin/env python3
"""Private stdio MCP adapter for one relay task. No direct DevBench access."""
import argparse
import asyncio
import json

async def exchange(endpoint, request):
    # Loopback port plus per-task secret works on macOS and Windows alike.
    reader, writer = await asyncio.open_connection('127.0.0.1', endpoint['port'], limit=131072)
    try:
        writer.write((json.dumps({'token': endpoint['token'], **request}) + '\n').encode())
        await writer.drain()
        raw = await asyncio.wait_for(reader.readline(), 190)
        if not raw or len(raw) > 131072:
            raise RuntimeError('Relay disconnected')
        reply = json.loads(raw)
        if 'error' in reply:
            raise RuntimeError(reply['error'])
        return reply['result']
    finally:
        writer.close()
        await writer.wait_closed()

async def main(path):
    from pathlib import Path
    from mcp.server import Server
    from mcp.server.stdio import stdio_server
    from mcp.types import Tool, TextContent
    endpoint = json.loads(Path(path).read_text())
    app = Server('termic-amiga')
    @app.list_tools()
    async def list_tools():
        return [Tool(**tool) for tool in await exchange(endpoint, {'op': 'list'})]
    @app.call_tool()
    async def call_tool(name, arguments):
        result = await exchange(endpoint, {'op': 'call', 'name': name, 'arguments': arguments})
        return [TextContent(type='text', text=result)]
    async with stdio_server() as (read, write):
        await app.run(read, write, app.create_initialization_options())

if __name__ == '__main__':
    p = argparse.ArgumentParser(); p.add_argument('endpoint')
    asyncio.run(main(p.parse_args().endpoint))
