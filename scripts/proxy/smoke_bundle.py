"""Exercise bundled Qt startup and real stdio MCP pipes, without cloud access."""
import asyncio
import json
from pathlib import Path
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'amiga'))
from cli_relay import ToolServer
from codex_relay import Mailbox
from test_codex_relay import FakeBridge, job
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

async def main(binary):
    subprocess.run([binary, '--self-test'], check=True, timeout=30)
    with tempfile.TemporaryDirectory() as temp:
        picture = Path(temp) / 'proxy.png'
        subprocess.run([binary, '--screenshot-fixture', str(picture)], check=True, timeout=30)
        assert picture.stat().st_size > 5000
        data = job(); data['readonly'] = True
        bridge = FakeBridge(); tools = ToolServer(Mailbox(bridge, data), data)
        listener = await asyncio.start_server(tools.handle, '127.0.0.1', 0)
        path = Path(temp) / 'endpoint.json'
        path.write_text(json.dumps({'port': listener.sockets[0].getsockname()[1], 'token': tools.token}))
        try:
            params = StdioServerParameters(command=binary, args=['--mcp-worker', str(path)])
            async with stdio_client(params) as (read, write):
                async with ClientSession(read, write) as session:
                    await session.initialize()
                    assert len((await session.list_tools()).tools) == 3
                    result = await session.call_tool('list_sources', {})
                    assert not result.isError and result.content[0].text == 'fixture.c'
        finally:
            listener.close(); await listener.wait_closed(); await tools.close()
    print('Bundled GUI and MCP worker passed.')

if __name__ == '__main__': asyncio.run(main(str(Path(sys.argv[1]).resolve())))
