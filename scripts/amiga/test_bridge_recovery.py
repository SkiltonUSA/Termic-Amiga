"""Reservation and acceptance recovery tests; no MCP server or hardware needed."""
import asyncio
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import bridge_client as bridge


def reply(value, error=False):
    return SimpleNamespace(isError=error, content=[SimpleNamespace(text=value if isinstance(value, str) else json.dumps(value))])


class Gateway:
    def __init__(self):
        self.token = None
        self.owner = None
        self.enabled = True
        self.active = None
        self.queued = 0
        self.ports = ''
        self.windows = ['Termic Amiga Workspace']
        self.calls = []
        self.hang = None
        self.error = None
        self.server = 'Amiga-MCP'
        self.closed = 0
        self.close_error = None
        self.serial = 0

    def transport(self, *args, **kwargs):
        gateway = self

        class Transport:
            async def __aenter__(self):
                return self

            async def __aexit__(self, *args):
                gateway.closed += 1
                if gateway.close_error:
                    raise gateway.close_error

            async def initialize(self):
                return SimpleNamespace(serverInfo=SimpleNamespace(name=gateway.server))

            async def call_tool(self, name, args=None, **kwargs):
                args = args or {}
                gateway.calls.append((name, dict(args)))
                if name == gateway.hang:
                    await asyncio.Event().wait()
                if name == gateway.error:
                    return reply('synthetic failure', True)
                if name == 'amiga_bridge_status':
                    return reply(dict(enabled=gateway.enabled, active=gateway.active,
                                      queued=gateway.queued, reservation={'id': 'fixture-owner'} if gateway.token else None))
                if name == 'amiga_bridge_reserve':
                    if gateway.token:
                        return reply('already reserved', True)
                    gateway.serial += 1
                    gateway.token = f'fixture-token-{gateway.serial}'
                    gateway.owner = self
                    return reply({'token': gateway.token})
                if name == 'amiga_bridge_resume':
                    if args['token'] != gateway.token:
                        return reply('wrong token', True)
                    gateway.owner = self
                    return reply({})
                if name == 'amiga_bridge_release':
                    if args['token'] != gateway.token or gateway.owner is not self:
                        return reply('wrong owner', True)
                    gateway.token = gateway.owner = None
                    return reply({})
                if name == 'amiga_list_ports':
                    return reply(gateway.ports)
                if name == 'amiga_list_screen_windows':
                    return reply('[OK]\n\n' + '\n'.join(title + '  pos=0,0' for title in gateway.windows))
                if name == 'amiga_input_key' and args.get('direction') == 'up':
                    if gateway.windows[0] == 'About Termic Amiga':
                        gateway.windows.pop(0)
                if name == 'amiga_screenshot':
                    return reply('incomplete screenshot')
                return reply('[OK]')
        return Transport()


class ClientTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.gateway = Gateway()
        self.patches = [patch.dict(sys.modules, {'mcp': SimpleNamespace(ClientSession=self.gateway.transport)}),
                        patch.object(bridge, 'reservation_directory', return_value=self.root / 'private'),
                        contextlib.redirect_stderr(io.StringIO())]
        for p in self.patches:
            p.__enter__()

    def tearDown(self):
        for p in reversed(self.patches):
            p.__exit__(None, None, None)
        self.tmp.cleanup()

    def client(self, **kwargs):
        return bridge.ClientSession(None, None, operation_timeout=.05, cleanup_timeout=.2, **kwargs)

    async def reserve(self, session):
        await session.initialize()
        await session.call_tool('amiga_run_script', {})

    def names(self):
        return [name for name, _ in self.gateway.calls]

    async def test_lazy_acquisition_clean_release_and_private_token(self):
        async with self.client() as session:
            await session.initialize()
            await session.call_tool('amiga_ping', {})
            self.assertIsNone(self.gateway.token)
            await session.call_tool('amiga_run_script', {})
            self.assertEqual(json.loads(session.path.read_text())['token'], self.gateway.token)
            if os.name != 'nt':
                self.assertEqual(session.path.stat().st_mode & 0o777, 0o600)
        self.assertIsNone(self.gateway.token)
        self.assertFalse(session.path.exists())
        self.assertEqual(self.gateway.closed, 1)

    async def test_assertion_releases_after_verified_cleanup(self):
        cleaned = []
        async def cleanup():
            self.assertIsNotNone(self.gateway.token)
            cleaned.append(True)
        with self.assertRaisesRegex(AssertionError, 'original'):
            async with self.client() as session:
                await self.reserve(session)
                session.cleanup = cleanup
                raise AssertionError('original')
        self.assertEqual(cleaned, [True])
        self.assertIsNone(self.gateway.token)

    async def test_timeout_retains_and_explicit_resume_works(self):
        with self.assertRaises(TimeoutError):
            async with self.client() as failed:
                await self.reserve(failed)
                self.gateway.hang = 'amiga_screenshot'
                await bridge.capture_png(failed, self.root/'shot.png')
        self.assertTrue(failed.path.exists())
        self.assertNotIn('amiga_bridge_release', self.names())
        self.assertFalse(failed.lock.locked())
        self.gateway.hang = None
        with self.assertRaises(ConnectionError):
            async with self.client() as other:
                await self.reserve(other)
        async with self.client(reservation_path=failed.path) as recovered:
            await recovered.initialize()
            self.assertEqual(recovered.token, self.gateway.token)
        self.assertIsNone(self.gateway.token)

    async def test_cancellation_during_request_retains_token(self):
        started = asyncio.Event()
        async def work():
            async with self.client() as session:
                await self.reserve(session)
                self.gateway.hang = 'amiga_run_script'
                started.set()
                await session.call_tool('amiga_run_script', {})
        task = asyncio.create_task(work())
        await started.wait()
        await asyncio.sleep(0)
        task.cancel()
        with self.assertRaises(asyncio.CancelledError):
            await task
        self.assertIsNotNone(self.gateway.token)
        self.assertNotIn('amiga_bridge_release', self.names())

    async def test_busy_or_paused_gateway_retains_without_ui_actions(self):
        for condition in ('enabled', 'active', 'queued', 'ports'):
            with self.subTest(condition=condition):
                self.gateway.token = self.gateway.owner = None
                self.gateway.enabled, self.gateway.active, self.gateway.queued, self.gateway.ports = True, None, 0, ''
                with self.assertRaises(ConnectionError):
                    async with self.client() as session:
                        await self.reserve(session)
                        async def cleanup():
                            self.fail('must not touch UI while gateway is busy/paused')
                        session.cleanup = cleanup
                        setattr(self.gateway, condition, {'enabled': False, 'active': {}, 'queued': 1, 'ports': 'TermicJob.fixture'}[condition])
                        if condition == 'active':
                            self.gateway.active = {'operation': 'fixture'}
                self.assertTrue(session.path.exists())

    async def test_text_only_port_error_does_not_prove_idle(self):
        with self.assertRaises(ConnectionError):
            async with self.client() as session:
                await self.reserve(session)
                self.gateway.ports = 'ERROR: unable to enumerate ports'
        self.assertTrue(session.path.exists())
        self.assertNotIn('amiga_bridge_release', self.names())

    async def test_stale_token_does_not_take_or_release_foreign_reservation(self):
        path = self.root/'stale.json'
        path.write_text(json.dumps({'url': bridge.DEFAULT_BRIDGE, 'token': 'stale'}))
        self.gateway.token = 'other-owner'
        with self.assertRaisesRegex(ConnectionError, 'may belong to another run'):
            async with self.client(reservation_path=path) as session:
                await session.initialize()
        self.assertEqual(self.gateway.token, 'other-owner')
        self.assertTrue(path.exists())
        self.assertNotIn('amiga_bridge_release', self.names())

    async def test_released_token_file_can_start_fresh(self):
        path = self.root/'released.json'
        path.write_text(json.dumps({'url': bridge.DEFAULT_BRIDGE, 'token': 'old'}))
        async with self.client(reservation_path=path) as session:
            await session.initialize()
            self.assertFalse(path.exists())
            await session.call_tool('amiga_run_script', {})
        self.assertIsNone(self.gateway.token)

    async def test_same_recovery_file_cannot_be_used_concurrently(self):
        async with self.client() as first:
            await self.reserve(first)
            with self.assertRaisesRegex(ConnectionError, 'Another process'):
                async with self.client(reservation_path=first.path):
                    self.fail('duplicate recovery lease acquired')
            self.assertNotIn('amiga_bridge_resume', self.names())

    async def test_different_runs_cannot_reserve_same_gateway(self):
        async with self.client() as first:
            await self.reserve(first)
            with self.assertRaises(ConnectionError):
                async with self.client() as second:
                    self.assertNotEqual(first.path, second.path)
                    await self.reserve(second)
            self.assertEqual(first.token, self.gateway.token)

    async def test_mismatched_url_and_direct_backend_rejected(self):
        path = self.root/'other.json'
        path.write_text(json.dumps({'url': 'http://example.invalid:55110/mcp', 'token': 'fixture'}))
        with self.assertRaisesRegex(ConnectionError, 'different bridge endpoint'):
            async with self.client(reservation_path=path) as session:
                await session.initialize()
        self.gateway.server = 'DevBench'
        with self.assertRaisesRegex(ConnectionError, 'reservation gateway'):
            async with self.client() as session:
                await session.initialize()
        self.assertNotIn('amiga_bridge_reserve', self.names())

    async def test_paused_resume_keeps_token_and_explains_next_step(self):
        path = self.root/'paused.json'
        path.write_text(json.dumps({'url': bridge.DEFAULT_BRIDGE, 'token': 'fixture'}))
        self.gateway.token, self.gateway.enabled = 'fixture', False
        with self.assertRaisesRegex(ConnectionError, 'Inspect the target'):
            async with self.client(reservation_path=path) as session:
                await session.initialize()
        self.assertTrue(path.exists())
        self.assertNotIn('amiga_list_ports', self.names())
        self.assertEqual(self.gateway.token, 'fixture')

    async def test_cleanup_port_timeout_is_bounded_and_keeps_original_error(self):
        with self.assertRaisesRegex(AssertionError, 'original failure'):
            async with self.client() as session:
                await self.reserve(session)
                self.gateway.hang = 'amiga_list_ports'
                raise AssertionError('original failure')
        self.assertTrue(session.path.exists())
        self.assertEqual(self.gateway.closed, 1)

    async def test_transport_close_does_not_mask_original_exception(self):
        self.gateway.close_error = RuntimeError('close failed')
        with self.assertRaisesRegex(AssertionError, 'original failure'):
            async with self.client() as session:
                await session.initialize()
                raise AssertionError('original failure')

    async def test_uncertain_tool_response_keeps_reservation(self):
        with self.assertRaises(ConnectionError):
            async with self.client() as session:
                await self.reserve(session)
                self.gateway.error = 'amiga_input_key'
                result = await session.call_tool('amiga_input_key', {})
                self.assertTrue(result.isError)
        self.assertTrue(session.path.exists())

    async def test_known_about_requester_closed_and_workspace_left_running(self):
        async with self.client() as session:
            await self.reserve(session)
            self.gateway.windows.insert(0, 'About Termic Amiga')
            session.cleanup = lambda: bridge.workspace_cleanup(session, about=True)
        self.assertEqual(self.gateway.windows, ['Termic Amiga Workspace'])
        self.assertIsNone(self.gateway.token)
        keys = [args for name, args in self.gateway.calls if name == 'amiga_input_key']
        self.assertEqual([k['direction'] for k in keys], ['down', 'up'])

    async def test_about_requester_is_not_closed_without_test_ownership(self):
        with self.assertRaisesRegex(RuntimeError, 'UI state'):
            async with self.client() as session:
                await self.reserve(session)
                self.gateway.windows.insert(0, 'About Termic Amiga')
                session.cleanup = lambda: bridge.workspace_cleanup(session)
        self.assertNotIn('amiga_input_key', self.names())
        self.assertTrue(session.path.exists())

    async def test_foreign_or_ambiguous_windows_are_not_driven(self):
        for windows in (['Other application', 'Termic Amiga Workspace'],
                        ['Termic Amiga Workspace', 'Termic Amiga Workspace'],
                        ['Termic Amiga Workspace', 'Termic Project']):
            self.gateway.token = self.gateway.owner = None
            self.gateway.calls = []
            with self.assertRaisesRegex(RuntimeError, 'UI state'):
                async with self.client() as session:
                    await self.reserve(session)
                    self.gateway.windows = windows
                    session.cleanup = lambda: bridge.workspace_cleanup(session, about=True)
            self.assertNotIn('amiga_input_key', self.names())
            self.assertTrue(session.path.exists())

    async def test_pass_marker_waits_for_release_and_cleanup_failure_prevents_it(self):
        output = self.root/'successful'
        async with bridge.AcceptanceRun(output, bridge.DEFAULT_BRIDGE) as run:
            async with self.client(reservation_path=run.path) as session:
                await self.reserve(session)
                run.passed = {'fixture': 'synthetic'}
                self.assertFalse((output/'passed.json').exists())
        self.assertIsNone(self.gateway.token)
        self.assertEqual(json.loads((output/'passed.json').read_text()), {'fixture': 'synthetic'})
        output = self.root/'failed'
        with self.assertRaisesRegex(RuntimeError, 'UI state'):
            async with bridge.AcceptanceRun(output, bridge.DEFAULT_BRIDGE) as run:
                async with self.client(reservation_path=run.path) as session:
                    await self.reserve(session)
                    self.gateway.windows = ['Other application']
                    session.cleanup = lambda: bridge.workspace_cleanup(session)
                    run.passed = {'must_not_publish': True}
        self.assertFalse((output/'passed.json').exists())
        self.assertTrue(run.path.exists())

    async def test_cancel_between_requests_can_release_verified_idle_target(self):
        with self.assertRaises(asyncio.CancelledError):
            async with self.client() as session:
                await self.reserve(session)
                raise asyncio.CancelledError()
        self.assertIsNone(self.gateway.token)

    async def test_token_write_failure_releases_without_masking_io_error(self):
        with self.assertRaisesRegex(OSError, 'fixture write failure'):
            async with self.client() as session:
                await session.initialize()
                with patch.object(bridge.json, 'dump', side_effect=OSError('fixture write failure')):
                    await session.call_tool('amiga_run_script', {})
        self.assertIsNone(self.gateway.token)
        self.assertFalse(session.path.exists())
        self.assertNotIn('amiga_run_script', self.names())

    async def test_release_timeout_keeps_recoverable_file(self):
        with self.assertRaises(TimeoutError):
            async with self.client() as session:
                await self.reserve(session)
                self.gateway.hang = 'amiga_bridge_release'
        self.assertTrue(session.path.exists())
        self.assertEqual(self.gateway.closed, 1)

    async def test_cleanup_callback_error_keeps_primary_assertion(self):
        async def failed_cleanup():
            raise RuntimeError('cleanup failed')
        with self.assertRaisesRegex(AssertionError, 'original check'):
            async with self.client() as session:
                await self.reserve(session)
                session.cleanup = failed_cleanup
                raise AssertionError('original check')
        self.assertTrue(session.path.exists())
        self.assertNotIn('amiga_bridge_release', self.names())

    async def test_caught_timeout_still_cannot_report_success(self):
        with self.assertRaisesRegex(ConnectionError, 'outcome is uncertain'):
            async with self.client() as session:
                await session.initialize()
                self.gateway.hang = 'amiga_screenshot'
                try:
                    await bridge.capture_png(session, self.root/'shot.png')
                except TimeoutError:
                    pass
        self.assertIsNone(self.gateway.token)

    async def test_incomplete_screenshot_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'Incomplete'):
            async with self.client() as session:
                await session.initialize()
                await bridge.capture_png(session, self.root/'shot.png')
        self.assertFalse((self.root/'shot.png').exists())


class RecoveryEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.patch = patch.object(bridge, 'reservation_directory', return_value=self.root/'private')
        self.patch.start()

    def tearDown(self):
        self.patch.stop()
        self.tmp.cleanup()

    def test_run_evidence_resolves_resume_without_publishing_token(self):
        output = self.root/'run'
        with bridge.AcceptanceRun(output, bridge.DEFAULT_BRIDGE) as run:
            saved_path = run.path
            saved_path.parent.mkdir()
            saved_path.write_text(json.dumps({'url': run.url, 'token': 'private-fixture-token'}))
        evidence = (output/'bridge-recovery.json').read_text()
        self.assertNotIn('private-fixture-token', evidence)
        with self.assertRaisesRegex(ConnectionError, '--reservation-file'):
            with bridge.AcceptanceRun(output, bridge.DEFAULT_BRIDGE):
                pass
        with bridge.AcceptanceRun(output, bridge.DEFAULT_BRIDGE, resume=True) as resumed:
            self.assertEqual(resumed.path, saved_path)
        with bridge.AcceptanceRun(output, bridge.DEFAULT_BRIDGE, reservation_file=saved_path) as resumed:
            self.assertEqual(resumed.path, saved_path)

    def test_new_runs_use_distinct_paths_and_clear_old_pass_marker(self):
        output = self.root/'run'
        with bridge.AcceptanceRun(output, bridge.DEFAULT_BRIDGE) as first:
            path = first.path
        (output/'passed.json').write_text('{}')
        with bridge.AcceptanceRun(output, bridge.DEFAULT_BRIDGE) as second:
            self.assertNotEqual(path, second.path)
            self.assertFalse((output/'passed.json').exists())

    def test_output_directory_excludes_second_process(self):
        output = self.root/'run'
        with bridge.AcceptanceRun(output, bridge.DEFAULT_BRIDGE):
            code = ('from bridge_client import AcceptanceRun,DEFAULT_BRIDGE\n'
                    'import sys\n'
                    'try:\n with AcceptanceRun(sys.argv[1],DEFAULT_BRIDGE): pass\n'
                    'except ConnectionError: sys.exit(0)\n'
                    'sys.exit(1)\n')
            result = subprocess.run([sys.executable, '-c', code, str(output)],
                                    cwd=Path(__file__).parent, timeout=10, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_bad_explicit_path_missing_evidence_and_endpoint_mismatch(self):
        with self.assertRaisesRegex(ConnectionError, 'does not exist'):
            with bridge.AcceptanceRun(self.root/'run', bridge.DEFAULT_BRIDGE, self.root/'missing.json'):
                pass
        with self.assertRaisesRegex(ConnectionError, 'No recovery evidence'):
            with bridge.AcceptanceRun(self.root/'resume', bridge.DEFAULT_BRIDGE, resume=True):
                pass
        with bridge.AcceptanceRun(self.root/'run', bridge.DEFAULT_BRIDGE):
            pass
        with self.assertRaisesRegex(ConnectionError, 'different gateway'):
            with bridge.AcceptanceRun(self.root/'run', 'http://example.invalid:55110'):
                pass

    def test_endpoint_normalization(self):
        self.assertEqual(bridge.bridge_endpoint('http://127.0.0.1:55110/'), bridge.DEFAULT_BRIDGE)
        self.assertEqual(bridge.bridge_endpoint(bridge.DEFAULT_BRIDGE + '/'), bridge.DEFAULT_BRIDGE)
        with self.assertRaises(ValueError):
            bridge.bridge_endpoint('not-a-url')


if __name__ == '__main__':
    unittest.main()
