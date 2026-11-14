import asyncio
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import cli_relay
import codex_relay
from test_codex_relay import FakeBridge, job

class CLITests(unittest.TestCase):
    def test_provider_validation_and_model_arguments(self):
        for provider in ('claude', 'cursor'):
            data = job(); data.update(provider=provider, protocol=2, model='fixture-model', thread='abcd-1234')
            codex_relay.validate_job(data)
            command = cli_relay.command_for(provider, provider, data, Path('/fixture'), Path('/fixture/mcp.json'))
            self.assertEqual(command[command.index('--model')+1], 'fixture-model')
            self.assertEqual(command[command.index('--resume')+1], 'abcd-1234')
            self.assertNotIn('--force', command)
            if provider == 'claude':
                self.assertEqual(command[command.index('--tools')+1], '')
                self.assertIn('--strict-mcp-config', command)
            data['protocol'] = 1
            with self.assertRaises(ValueError): codex_relay.validate_job(data)
        data = job(); data.update(protocol=2, provider='unknown')
        with self.assertRaises(ValueError): codex_relay.validate_job(data)

    def test_remote_file_dump_validation(self):
        self.assertEqual(codex_relay.decode_hex_dump('00000000  ' + '61 62 63'.ljust(48) + '  abc', 0), b'abc')
        for bad in ['Not found or unreadable: fixture', '00000001  ' + '61'.ljust(48) + '  a', '']:
            with self.assertRaises(ConnectionError): codex_relay.decode_hex_dump(bad, 0)

    def test_single_proxy_lock(self):
        with tempfile.TemporaryDirectory() as temp:
            lock = codex_relay.RelayLock(Path(temp))
            try:
                with self.assertRaises(RuntimeError): codex_relay.RelayLock(Path(temp))
            finally: lock.close()
            codex_relay.RelayLock(Path(temp)).close()

class CLIAsyncTests(unittest.IsolatedAsyncioTestCase):
    async def test_mcp_permissions_limit_and_real_stdio(self):
        data = job(); data['readonly'] = True
        bridge = FakeBridge(); mailbox = codex_relay.Mailbox(bridge, data)
        server = cli_relay.ToolServer(mailbox, data)
        token = server.token
        with self.assertRaises(ValueError): await server.dispatch({'token': 'wrong', 'op': 'list'})
        self.assertEqual(len(await server.dispatch({'token': token, 'op': 'list'})), 4)
        for name in ('write_source', 'build', 'run_arexx', 'run_program', 'shell'):
            with self.assertRaises(ValueError): await server.dispatch({'token': token, 'op': 'call', 'name': name, 'arguments': {}})
        listener = await asyncio.start_server(server.handle, '127.0.0.1', 0)
        try:
            from mcp import ClientSession, StdioServerParameters
            from mcp.client.stdio import stdio_client
            with tempfile.TemporaryDirectory() as temp:
                path = Path(temp) / 'endpoint.json'
                path.write_text(json.dumps({'port': listener.sockets[0].getsockname()[1], 'token': token}))
                params = StdioServerParameters(command=sys.executable, args=[str(Path(cli_relay.__file__).with_name('relay_mcp.py')), str(path)])
                async with stdio_client(params) as (read, write):
                    async with ClientSession(read, write) as session:
                        await session.initialize()
                        self.assertEqual(len((await session.list_tools()).tools), 4)
                        result = await session.call_tool('list_sources', {})
                        self.assertFalse(result.isError)
                        self.assertIn('fixture.c', result.content[0].text)
                        await session.call_tool('read_source', {})
                        self.assertTrue((await session.call_tool('read_source', {})).isError)
        finally:
            listener.close(); await listener.wait_closed(); await server.close()
        self.assertEqual(len([f for f in bridge.frames if f['kind'] == 'tool']), 2)

    async def test_cli_output_resume_failure_and_cleanup(self):
        class Input:
            def write(self, data): self.data = data
            async def drain(self): pass
            def close(self): pass
        class Process:
            def __init__(self, failed):
                self.stdin = Input(); self.stdout = asyncio.StreamReader(); self.returncode = None
                events = [{'type': 'system', 'session_id': 'abcd-1234'},
                          {'type': 'system', 'session_id': 'abcd-1234'},
                          {'type': 'assistant', 'timestamp_ms': 1, 'message': {'content': [{'type': 'text', 'text': 'Fixture reply'}]}},
                          {'type': 'result', 'session_id': 'abcd-1234', 'subtype': 'error' if failed else 'success', 'is_error': failed}]
                for event in events: self.stdout.feed_data((json.dumps(event)+'\n').encode())
                self.stdout.feed_eof()
            async def wait(self): self.returncode = 0; return 0
        async def login(*args): pass
        for provider in ('claude', 'cursor'):
            for failed in (False, True):
                data = job(); data.update(provider=provider, protocol=2, thread='abcd-1234', model='fixture-model')
                bridge = FakeBridge(); proc = Process(failed)
                async def spawn(*command, **kw):
                    self.assertEqual(command[command.index('--model')+1], data['model'])
                    self.assertEqual(command[command.index('--resume')+1], data['thread'])
                    if provider == 'cursor':
                        config = json.loads((kw['cwd'] / '.cursor/mcp.json').read_text())
                        self.assertEqual(list(config['mcpServers']), ['amiga'])
                        endpoint = Path(config['mcpServers']['amiga']['args'][-1])
                        self.assertTrue(endpoint.is_file())
                    return proc
                with tempfile.TemporaryDirectory() as temp, patch.object(cli_relay, 'check_login', login), patch.object(cli_relay.asyncio, 'create_subprocess_exec', spawn):
                    ok = await cli_relay.run_cli_job(bridge, data, provider, Path(temp))
                    self.assertEqual(ok, not failed)
                    self.assertFalse(list(Path(temp).glob('termic-job-*')))
                    self.assertFalse((Path(temp) / 'cursor-workspace/.cursor/mcp.json').exists())
                self.assertEqual(bridge.frames[-1]['status'], 'failed' if failed else 'completed')
                self.assertEqual([f['thread'] for f in bridge.frames if f['kind']=='session'], ['abcd-1234'])

    async def test_missing_login_finishes_task(self):
        async def no_login(*args): raise RuntimeError('Sign in required')
        data=job(); data.update(provider='cursor', protocol=2)
        bridge=FakeBridge()
        with tempfile.TemporaryDirectory() as temp, patch.object(cli_relay, 'check_login', no_login):
            self.assertFalse(await cli_relay.run_cli_job(bridge, data, 'cursor-agent', Path(temp)))
        self.assertEqual(bridge.frames[-1]['status'], 'failed')


class CLIStreamingTests(unittest.IsolatedAsyncioTestCase):
    async def test_provider_deltas_ignore_reasoning_and_duplicate_flushes(self):
        for provider in ('claude', 'cursor'):
            bridge = FakeBridge()
            live = codex_relay.LiveText(codex_relay.Mailbox(bridge, job()), interval=.001)
            parser = cli_relay.CLIText(provider, live)
            try:
                def assistant(text, **fields):
                    return {'type':'assistant', 'message':{'id':'m1', 'content':[{'type':'text','text':text}]}, **fields}
                if provider == 'claude':
                    def stream(**event): parser.event({'type':'stream_event','event':event})
                    stream(type='message_start', message={'id':'m1'})
                    stream(type='content_block_start', index=0, content_block={'type':'text','text':''})
                    stream(type='content_block_delta', index=0, delta={'type':'text_delta','text':'Hello'})
                    stream(type='content_block_delta', index=1, delta={'type':'thinking_delta','thinking':'private'})
                    stream(type='content_block_delta', index=2, delta={'type':'input_json_delta','partial_json':'private'})
                else:
                    parser.event(assistant('Hello', timestamp_ms=1))
                for _ in range(100):
                    if bridge.frames: break
                    await asyncio.sleep(.005)
                self.assertEqual(bridge.frames[0]['text'], 'Hello')
                if provider == 'claude':
                    stream(type='content_block_delta', index=0, delta={'type':'text_delta','text':' world'})
                    parser.event(assistant('Hello world'))
                    stream(type='content_block_stop', index=0)
                else:
                    parser.event(assistant(' world', timestamp_ms=2))
                    parser.event(assistant('Hello world', timestamp_ms=3, model_call_id='call1'))
                    parser.event(assistant('Hello world'))
                parser.event({'type':'result','result':'Hello world'})
                await live.finish()
                self.assertEqual(''.join(f['text'] for f in bridge.frames), 'Hello world' + ('\n' if provider=='claude' else ''))
            finally: await live.close()

    async def test_result_fallback_and_tool_barrier(self):
        data = job(); bridge = FakeBridge()
        live = codex_relay.LiveText(codex_relay.Mailbox(bridge, data))
        server = cli_relay.ToolServer(live.mailbox, data, live)
        try:
            parser = cli_relay.CLIText('cursor', live)
            parser.event({'type':'result','result':'Fallback'})
            await server.dispatch({'op':'call','token':server.token,'name':'build','arguments':{}})
            frames = bridge.frames
            self.assertEqual(frames[0]['text'], 'Fallback\n')
            self.assertEqual([f['kind'] for f in frames], ['text','text','tool','text'])
            self.assertIn('returned: build',frames[-1]['text'])
        finally:
            await live.close(); await server.close()
