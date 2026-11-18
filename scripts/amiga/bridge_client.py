"""Shared Amiga-MCP client with session reservations and private recovery tokens.

Idle reads do not reserve the target. Protected operations reserve lazily;
normal exit releases only after the gateway and Termic workers are idle.
An interrupted/uncertain run retains its token for explicit recovery.
"""
import asyncio
import json
import os
import re
import shutil
import subprocess
from pathlib import Path
import sys
import uuid

DEFAULT_BRIDGE = 'http://127.0.0.1:55110/mcp'
SHARED_READS = {'amiga_connect', 'amiga_disconnect', 'amiga_ping', 'amiga_capabilities',
    'amiga_list_tasks', 'amiga_list_clients', 'amiga_sysinfo', 'amiga_arexx_ports',
    'amiga_list_dir', 'amiga_read_file', 'amiga_screenshot', 'amiga_logs'}


def reservation_directory():
    base = Path(os.environ.get('LOCALAPPDATA', str(Path.home() / 'AppData/Local'))) if sys.platform == 'win32' else Path.home() / 'Library/Application Support'
    return base / 'Termic-Amiga/bridge-reservations'


def text(result):
    return '\n'.join(getattr(block, 'text', '') for block in result.content)


class ClientSession:
    def __init__(self, *args, bridge_url=DEFAULT_BRIDGE, reservation_path=None, **kwargs):
        from mcp import ClientSession as MCPClientSession
        self.session = MCPClientSession(*args, **kwargs)
        self.url = bridge_url
        self.path = Path(reservation_path) if reservation_path else reservation_directory() / (uuid.uuid4().hex + '.json')
        self.gateway = False
        self.token = None
        self.lock = asyncio.Lock()

    def __getattr__(self, name):
        return getattr(self.session, name)

    async def __aenter__(self):
        await self.session.__aenter__()
        return self

    async def __aexit__(self, kind, value, traceback):
        try:
            if kind is None or kind is asyncio.CancelledError:
                for _ in range(12):
                    if await self.release_if_idle(): break
                    await asyncio.sleep(.25)
        finally:
            if self.token:
                print('Bridge reservation retained. Recovery file: ' + str(self.path), file=sys.stderr)
            await self.session.__aexit__(kind, value, traceback)

    async def initialize(self):
        result = await self.session.initialize()
        self.gateway = getattr(getattr(result, 'serverInfo', None), 'name', '') == 'Amiga-MCP'
        if self.gateway and self.path.exists():
            saved = json.loads(self.path.read_text())
            if saved.get('url') != self.url:
                raise ConnectionError('Reservation belongs to a different bridge endpoint')
            state = await self.control('amiga_bridge_status', {})
            if state.get('reservation'):
                await self.control('amiga_bridge_resume', {'token': saved['token']})
                self.token = saved['token']
            else:
                self.path.unlink()
        return result

    async def control(self, name, arguments):
        result = await asyncio.wait_for(self.session.call_tool(name, arguments), 45)
        if result.isError:
            raise ConnectionError('Amiga bridge control failed: ' + name)
        try:
            return json.loads(text(result))
        except (TypeError, ValueError):
            raise ConnectionError('Invalid Amiga bridge control reply') from None

    async def acquire(self):
        if self.token: return
        reply = await self.control('amiga_bridge_reserve', {'label': 'Termic client'})
        self.token = reply['token']
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
            fd = os.open(self.path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            with os.fdopen(fd, 'w') as file:
                json.dump({'url': self.url, 'token': self.token}, file)
        except BaseException:
            # No hardware operation has been submitted by this session yet.
            await self.control('amiga_bridge_release', {'token': self.token, 'confirmed_idle': True})
            self.token = None
            raise

    async def call_tool(self, name, arguments=None, **kwargs):
        async with self.lock:
            if self.gateway and name not in SHARED_READS and not name.startswith('amiga_bridge_'):
                await self.acquire()
            return await self.session.call_tool(name, arguments, **kwargs)

    async def release_if_idle(self):
        async with self.lock:
            if not self.gateway or not self.token: return True
            state = await self.control('amiga_bridge_status', {})
            if not state.get('enabled') or state.get('active') or state.get('queued'): return False
            ports = await self.session.call_tool('amiga_list_ports', {})
            if ports.isError or 'TermicJob.' in text(ports): return False
            await self.control('amiga_bridge_release', {'token': self.token, 'confirmed_idle': True})
            self.token = None
            self.path.unlink(missing_ok=True)
            return True


async def capture_png(session, target, window=None):
    """Capture through MCP; the reservation gateway has no REST screenshot route."""
    result = await session.call_tool('amiga_screenshot', {'window': window} if window else {})
    match = re.search(r'Screenshot saved: (.+) \((\d+)x(\d+), (RLE|truecolor), (\d+) rows received\)', text(result))
    if result.isError or not match or match[3] != match[5]:
        raise RuntimeError('Incomplete Amiga screenshot')
    source = Path(match[1])
    if source.suffix.lower() == '.png':
        shutil.copyfile(source, target)
    else:
        subprocess.run(['sips', '-s', 'format', 'png', str(source), '--out', str(target)],
                       check=True, capture_output=True)
