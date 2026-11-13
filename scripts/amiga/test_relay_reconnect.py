import asyncio
from contextlib import asynccontextmanager, redirect_stdout
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import codex_relay as relay
from test_codex_relay import job
import json

class ReconnectTests(unittest.IsolatedAsyncioTestCase):
    async def test_connection_cleanup_on_initialize_failure(self):
        closed = []
        @asynccontextmanager
        async def transport(*args):
            try: yield None, None, None
            finally: closed.append('transport')
        class Session:
            def __init__(self, *args): pass
            async def __aenter__(self): return self
            async def __aexit__(self, *args): closed.append('session')
            async def initialize(self): raise ConnectionError('fixture disconnected')
        with patch('mcp.client.streamable_http.streamablehttp_client', transport), patch('mcp.ClientSession', Session):
            with self.assertRaises(ConnectionError):
                async with relay.Bridge('http://localhost/mcp'): self.fail('Initialization should fail')
        self.assertEqual(closed, ['session', 'transport'])

    async def test_reconnect_does_not_replay_interrupted_job(self):
        connections = []; calls = []; delays = []; reads = 0
        first = job(); second = job(); second['id'] = '12345678-12345679-1234'
        class Bridge:
            def __init__(self, *args): self.number = len(connections); connections.append(self)
            async def __aenter__(self):
                if self.number == 0: raise ConnectionRefusedError('fixture address')
                return self
            async def __aexit__(self, *args): self.closed = True
            async def read(self, name):
                nonlocal reads
                if self.number == 1: return json.dumps(first)
                reads += 1
                if reads == 1: return json.dumps(first)  # still pending after lost ack
                if reads == 2: return json.dumps(second)
                raise asyncio.CancelledError()
        async def dispatch(bridge, data, args):
            calls.append(data['id'])
            if data['id'] == first['id']:
                raise ExceptionGroup('opaque transport wrapper', [ConnectionError('PRIVATE_FIXTURE_TOKEN')])
            return True
        async def sleep(delay): delays.append(delay)
        args = relay.parser().parse_args([])
        output = io.StringIO()
        with patch.object(relay, 'Bridge', Bridge), patch.object(relay, 'dispatch_job', dispatch), patch.object(relay.asyncio, 'sleep', sleep), redirect_stdout(output):
            with self.assertRaises(asyncio.CancelledError): await relay.serve(args)
        self.assertEqual(calls, [first['id'], second['id']])
        self.assertEqual(len(connections), 3)
        self.assertTrue(all(b.closed for b in connections[1:]))
        self.assertNotIn('PRIVATE_FIXTURE_TOKEN', output.getvalue())
        self.assertIn('Bridge unavailable:', output.getvalue())
        self.assertIn('AI proxy ready', output.getvalue())

    async def test_stop_during_retry_releases_lock(self):
        entered = asyncio.Event()
        class Offline:
            def __init__(self, *args): pass
            async def __aenter__(self): entered.set(); raise ConnectionRefusedError()
            async def __aexit__(self, *args): pass
        with tempfile.TemporaryDirectory() as directory, patch.object(relay, 'Bridge', Offline):
            args = relay.parser().parse_args([]); args.state_dir = Path(directory)
            task = asyncio.create_task(relay.main(args))
            await entered.wait()
            task.cancel()
            with self.assertRaises(asyncio.CancelledError): await task
            relay.RelayLock(args.state_dir).close()

    async def test_real_unreachable_transport_closes_cleanly(self):
        import socket
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0)); port = sock.getsockname()[1]
        with tempfile.TemporaryDirectory() as directory:
            args = relay.parser().parse_args(['--once', '--bridge', f'http://127.0.0.1:{port}/mcp'])
            args.state_dir = Path(directory)
            self.assertEqual(await asyncio.wait_for(relay.main(args), 10), 1)
            relay.RelayLock(args.state_dir).close()

    async def test_backoff_and_once(self):
        delays = []; attempts = 0
        class Offline:
            def __init__(self, *args): pass
            async def __aenter__(self):
                nonlocal attempts
                attempts += 1
                if attempts == 8: raise asyncio.CancelledError()
                raise ConnectionRefusedError()
            async def __aexit__(self, *args): pass
        async def sleep(delay): delays.append(delay)
        args = relay.parser().parse_args([])
        with patch.object(relay, 'Bridge', Offline), patch.object(relay.asyncio, 'sleep', sleep):
            with self.assertRaises(asyncio.CancelledError): await relay.serve(args)
            self.assertEqual(delays, [2,4,8,16,30,30,30])
            args.once = True
            self.assertEqual(await relay.serve(args), 1)

    async def test_invalid_request_stops_instead_of_retrying(self):
        class Invalid:
            def __init__(self, *args): pass
            async def __aenter__(self): return self
            async def __aexit__(self, *args): pass
            async def read(self, name): return '{}'
        with patch.object(relay, 'Bridge', Invalid):
            with self.assertRaises(ValueError): await relay.serve(relay.parser().parse_args([]))

    def test_error_classification(self):
        import httpx
        self.assertFalse(relay.connection_failure(ValueError('invalid input')))
        self.assertFalse(relay.connection_failure(FileNotFoundError('missing CLI')))
        from mcp.shared.exceptions import McpError
        from mcp.types import ErrorData
        for code, retry in [(-32000,True),(408,True),(-32601,False),(-32602,False)]:
            self.assertEqual(relay.connection_failure(McpError(ErrorData(code=code,message='fixture'))),retry)
        self.assertFalse(relay.connection_failure(ExceptionGroup('mixed', [ConnectionError(), ValueError()])))
        for status, retry in [(401,False),(403,False),(404,True),(429,True),(503,True)]:
            error = httpx.HTTPStatusError('private response body', request=httpx.Request('GET','http://localhost'), response=httpx.Response(status))
            self.assertEqual(relay.connection_failure(error), retry)
            self.assertNotIn('private', relay.connection_reason(error))
