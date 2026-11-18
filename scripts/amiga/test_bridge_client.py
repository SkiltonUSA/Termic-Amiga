"""Synthetic reservation and endpoint migration checks, with no hardware calls."""
import asyncio
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace as NS
import unittest
from unittest.mock import patch
from bridge_client import ClientSession, DEFAULT_BRIDGE, capture_png


def reply(value, error=False):
    return NS(isError=error, content=[NS(text=json.dumps(value) if isinstance(value, dict) else value)])


class Backend:
    def __init__(self):
        self.calls = []
        self.name = 'Amiga-MCP'
        self.state = dict(enabled=True, active=None, queued=0, reservation=None)
        self.ports = ''
        self.closed = False
        self.failed = None

    async def __aenter__(self): return self
    async def __aexit__(self, *args): self.closed = True
    async def initialize(self): return NS(serverInfo=NS(name=self.name))
    async def call_tool(self, name, args=None, **kwargs):
        self.calls.append((name, args))
        if name == self.failed: return reply('synthetic failure', True)
        if name == 'amiga_bridge_status': return reply(self.state)
        if name == 'amiga_bridge_reserve':
            if self.state['reservation']: return reply('already reserved', True)
            self.state['reservation'] = {'label': 'test'}
            return reply({'token': 'synthetic-token'})
        if name == 'amiga_bridge_resume':
            return reply({}, args['token'] != 'synthetic-token')
        if name == 'amiga_bridge_release':
            self.state['reservation'] = None
            return reply({})
        if name == 'amiga_list_ports': return reply(self.ports)
        return reply('ok')


class ReservationTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'reservation.json'
        self.backend = Backend()
        self.mock = patch('mcp.ClientSession', return_value=self.backend)
        self.mock.start(); self.addCleanup(self.mock.stop)

    def session(self): return ClientSession(None, None, reservation_path=self.path)

    async def test_idle_reads_do_not_reserve(self):
        async with self.session() as session:
            await session.initialize()
            await session.call_tool('amiga_list_dir', {'path': 'T:'})
        self.assertFalse(self.path.exists())
        self.assertEqual([c[0] for c in self.backend.calls], ['amiga_list_dir'])

    async def test_write_reserves_once_and_normal_exit_releases(self):
        async with self.session() as session:
            await session.initialize()
            await asyncio.gather(session.call_tool('amiga_write_file', {}), session.call_tool('amiga_checksum', {}))
            self.assertEqual(json.loads(self.path.read_text())['token'], 'synthetic-token')
            if os.name != 'nt': self.assertEqual(self.path.stat().st_mode & 0o777, 0o600)
        names = [c[0] for c in self.backend.calls]
        self.assertEqual(names.count('amiga_bridge_reserve'), 1)
        self.assertEqual(names[-1], 'amiga_bridge_release')
        self.assertFalse(self.path.exists())

    async def test_uncertain_exit_retains_and_next_session_resumes(self):
        session = self.session()
        await session.__aenter__(); await session.initialize()
        await session.call_tool('amiga_write_file', {})
        output = io.StringIO()
        with contextlib.redirect_stderr(output):
            await session.__aexit__(TimeoutError, TimeoutError(), None)
        self.assertTrue(self.path.exists())
        self.assertNotIn('synthetic-token', output.getvalue())
        self.assertTrue(self.backend.closed)
        async with self.session() as second:
            await second.initialize()
            self.assertEqual(second.token, 'synthetic-token')
        self.assertIn('amiga_bridge_resume', [c[0] for c in self.backend.calls])
        self.assertFalse(self.path.exists())

    async def test_busy_disabled_and_workers_retain_lease(self):
        session = self.session(); await session.initialize()
        await session.call_tool('amiga_write_file', {})
        for field, value in [('enabled', False), ('active', {'tool': 'test'}), ('queued', 1)]:
            old = self.backend.state[field]; self.backend.state[field] = value
            self.assertFalse(await session.release_if_idle())
            self.backend.state[field] = old
        self.backend.ports = 'TermicJob.fixture'
        self.assertFalse(await session.release_if_idle())
        self.backend.ports = ''
        self.backend.failed = 'amiga_list_ports'
        self.assertFalse(await session.release_if_idle())
        self.assertTrue(self.path.exists())
        self.assertNotIn('amiga_bridge_enable', [c[0] for c in self.backend.calls])
        self.backend.failed = None
        self.assertTrue(await session.release_if_idle())

    async def test_other_owner_blocks_write_without_resume_or_release(self):
        self.backend.state['reservation'] = {'label': 'other client'}
        session = self.session(); await session.initialize()
        with self.assertRaises(ConnectionError): await session.call_tool('amiga_write_file', {})
        self.assertEqual([c[0] for c in self.backend.calls], ['amiga_bridge_reserve'])
        self.assertFalse(self.path.exists())

    async def test_wrong_recovery_token_and_endpoint_fail_closed(self):
        self.backend.state['reservation'] = {'label': 'other client'}
        self.path.write_text(json.dumps({'url': DEFAULT_BRIDGE, 'token': 'wrong'}))
        with self.assertRaises(ConnectionError): await self.session().initialize()
        self.assertTrue(self.path.exists())
        self.path.write_text(json.dumps({'url': 'http://localhost:12345/mcp', 'token': 'synthetic-token'}))
        with self.assertRaises(ConnectionError): await self.session().initialize()
        self.assertNotIn('amiga_bridge_release', [c[0] for c in self.backend.calls])

    async def test_legacy_server_no_gateway_controls(self):
        self.backend.name = 'fixture-mailbox'
        async with self.session() as session:
            await session.initialize()
            await session.call_tool('amiga_write_file', {})
        self.assertEqual([c[0] for c in self.backend.calls], ['amiga_write_file'])

    async def test_screenshot_rejects_incomplete_transfer(self):
        from unittest.mock import AsyncMock
        session = NS(call_tool=AsyncMock(return_value=reply('Screenshot saved: /tmp/fixture.png (20x30, RLE, 29 rows received)')))
        with self.assertRaises(RuntimeError): await capture_png(session, self.path, 'Fixture')
        self.assertFalse(self.path.exists())


class ConfigTests(unittest.TestCase):
    def test_old_local_endpoint_migrates_custom_endpoint_preserved(self):
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'proxy'))
        import app
        with tempfile.TemporaryDirectory() as temp:
            state = Path(temp)
            for url, expected in [('http://127.0.0.1:55010/mcp', DEFAULT_BRIDGE),
                                  ('http://localhost:55010/mcp/', DEFAULT_BRIDGE),
                                  ('http://localhost:12345/mcp', 'http://localhost:12345/mcp')]:
                (state/'proxy.json').write_text(json.dumps({'bridge': url, 'autostart': False, 'claude': 'fixture-claude'}))
                actual = app.read_config(state)
                self.assertEqual(actual['bridge'], expected)
                self.assertFalse(actual['autostart'])
                self.assertEqual(actual['claude'], 'fixture-claude')


class StdioIdleTests(unittest.IsolatedAsyncioTestCase):
    async def test_waiting_client_releases_after_async_job_finishes(self):
        import mcp_bridge
        from unittest.mock import AsyncMock
        ready = asyncio.Event()
        calls = []
        async def read(*args):
            await ready.wait()
            return b'fixture request\n'
        async def release():
            calls.append(True)
            ready.set()
            return True
        bridge = NS(release_if_idle=release)
        with patch('asyncio.to_thread', new=read):
            self.assertEqual(await mcp_bridge.read_request(bridge), b'fixture request\n')
        self.assertEqual(len(calls), 1)

    async def test_failed_idle_check_keeps_stdin_and_does_not_leak_details(self):
        import mcp_bridge
        ready = asyncio.Event()
        async def read(*args):
            await ready.wait()
            return b''
        async def release():
            ready.set()
            raise ConnectionError('synthetic-private-detail')
        output = io.StringIO()
        with patch('asyncio.to_thread', new=read), contextlib.redirect_stderr(output):
            self.assertEqual(await mcp_bridge.read_request(NS(release_if_idle=release)), b'')
        self.assertNotIn('synthetic-private-detail', output.getvalue())
