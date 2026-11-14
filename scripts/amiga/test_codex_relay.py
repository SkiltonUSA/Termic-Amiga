import asyncio
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('relay', Path(__file__).with_name('codex_relay.py'))
relay = importlib.util.module_from_spec(spec)
spec.loader.exec_module(relay)


def job():
    return {'protocol': 1, 'id': '12345678-12345678-1234', 'prompt': 'Fixture task',
            'instructions': 'Fixture guidance', 'model': '', 'thread': '', 'readonly': False,
            'limit': 2, 'guidance': []}


class RelayTests(unittest.TestCase):
    def test_native_protocol(self):
        src = ROOT / 'amiga/native'
        with tempfile.TemporaryDirectory() as d:
            binary = str(Path(d) / 'relay')
            subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-Wno-deprecated-declarations',
                            '-fsanitize=address,undefined', '-I', str(src),
                            *[str(src / f) for f in ['relay.c', 'vendor/cJSON.c', 'tests/test_relay.c']],
                            '-o', binary], check=True)
            subprocess.run([binary], check=True)

    def test_job_boundaries(self):
        relay.validate_job(job())
        for key, value in [('id', '../Config'), ('thread', '../../auth.json'), ('prompt', 'x' * 8193),
                           ('readonly', 1), ('limit', 0), ('limit', 33), ('protocol', 3),
                           ('guidance', [{'path': 'Config/api-key', 'text': 'fixture'}])]:
            data = job(); data[key] = value
            with self.assertRaises(ValueError):
                relay.validate_job(data)

    def test_tool_permissions_and_text(self):
        names = {x['name'] for x in relay.dynamic_tools(True)}
        self.assertEqual(names, {'read_source', 'read_file', 'list_sources', 'system_info'})
        self.assertEqual({x['name'] for x in relay.dynamic_tools(False)}, relay.TOOLS)
        self.assertEqual(relay.display_text('Hello\x1b\x00\n'), 'Hello\n')
        with self.assertRaises(ValueError):
            relay.Bridge('http://localhost', 'DH0:Work\nDelete')

    def test_auth_stays_local(self):
        with tempfile.TemporaryDirectory() as d:
            base = Path(d); source = base / 'source'; source.mkdir()
            ((source / 'auth.json').resolve()).write_text('{"fixture":"not-a-credential"}')
            home, work = relay.prepare_home(base / 'state', source)
            self.assertTrue((home / 'auth.json').is_file())
            if not (home / 'auth.json').is_symlink():
                self.assertEqual((home / 'auth.json').read_bytes(), (source / 'auth.json').read_bytes())
                return
            self.assertEqual((home / 'auth.json').resolve(), (source / 'auth.json').resolve())
            self.assertEqual(list(work.iterdir()), [])


class FakeBridge:
    def __init__(self):
        self.frames = []; self.stop = False; self.operations = []
    async def read(self, name):
        if name.endswith('.stopped'):
            return 'stopped' if self.stop else None
        if name.endswith('.out') and self.frames:
            frame = self.frames[-1]
            return json.dumps({'id': frame['id'], 'seq': frame['seq'], 'ok': True, 'text': 'fixture.c'})
    async def write(self, name, value):
        self.operations.append(('write',name)); self.frames.append(value)
    async def remove(self, *names): self.operations.append(('remove',names))


class AsyncRelayTests(unittest.IsolatedAsyncioTestCase):
    async def test_mailbox_order_and_stop(self):
        bridge = FakeBridge(); box = relay.Mailbox(bridge, job())
        await asyncio.gather(box.send('ping'), box.send('text', text='Fixture'))
        self.assertEqual([f['seq'] for f in bridge.frames], [1, 2])
        self.assertEqual([op[0] for op in bridge.operations], ['remove','write','remove','write'])
        self.assertEqual(bridge.operations[0][1], (job()['id']+'.out',))
        bridge.stop = True
        with self.assertRaises(ConnectionError): await box.send('tool', name='build', arguments={})
        self.assertEqual(len(bridge.frames), 2)

    async def test_run_job_tool_call_completion_and_resume(self):
        class FakeCodex:
            def __init__(self, *args):
                self.events = asyncio.Queue(); self.methods = []; self.closed = False
            async def start(self): pass
            async def rpc(self, method, params):
                self.methods.append(method)
                if method in ('thread/start', 'thread/resume'): return {'thread': {'id': 'abcdef-1234'}}
                if method == 'turn/start':
                    await self.events.put({'id': 7, 'method': 'item/tool/call', 'params': {
                        'threadId': 'abcdef-1234', 'tool': 'list_sources', 'arguments': {}}})
                    return {'turn': {'id': 'turn-1'}}
            async def send(self, obj):
                self.reply = obj
                for thread, text in [('other-thread', 'Wrong task'), ('abcdef-1234', 'Fixture '), ('abcdef-1234', 'complete')]:
                    await self.events.put({'method':'item/agentMessage/delta','params':{
                        'threadId':thread,'turnId':'turn-1','itemId':'message-1','delta':text}})
                await self.events.put({'method': 'item/completed', 'params': {'threadId': 'abcdef-1234', 'turnId': 'turn-1', 'item': {'id': 'message-1', 'type': 'agentMessage', 'text': 'Fixture complete'}}})
                await self.events.put({'method': 'turn/completed', 'params': {'turn': {'id': 'turn-1', 'status': 'completed'}}})
            async def close(self): self.closed = True
        for resume in [False, True]:
            bridge = FakeBridge(); data = job()
            if resume: data['thread'] = 'abcdef-1234'
            instance = FakeCodex()
            with patch.object(relay, 'Codex', return_value=instance):
                self.assertTrue(await relay.run_job(bridge, data, 'fixture', Path('/tmp'), Path('/tmp')))
            self.assertIn('thread/resume' if resume else 'thread/start', instance.methods)
            self.assertTrue(instance.closed)
            self.assertEqual([f['name'] for f in bridge.frames if f['kind'] == 'tool'], ['list_sources'])
            self.assertEqual(bridge.frames[-1]['status'], 'completed')
            self.assertTrue(instance.reply['result']['success'])
            text = ''.join(f.get('text','') for f in bridge.frames if f['kind']=='text')
            self.assertEqual(text.count('Fixture complete'), 1)
            self.assertNotIn('Wrong task', text)


class StreamingTests(unittest.IsolatedAsyncioTestCase):
    async def test_batches_visible_text_before_completion_and_flushes_tail(self):
        bridge = FakeBridge()
        live = relay.LiveText(relay.Mailbox(bridge, job()), interval=.01)
        messages = relay.MessageText(live)
        try:
            messages.delta('item-1', 'Hello')
            messages.delta('item-1', ' world')
            for _ in range(100):
                if bridge.frames: break
                await asyncio.sleep(.005)
            self.assertEqual([f['text'] for f in bridge.frames], ['Hello world'])
            messages.complete('item-1', 'Hello world!')
            messages.complete('item-1', 'Hello world!')
            messages.delta('item-1', 'ignored late delta')
            messages.complete('item-2', 'Fallback')
            await live.finish()
            self.assertEqual(''.join(f['text'] for f in bridge.frames), 'Hello world!\nFallback\n')
            self.assertEqual(len(bridge.frames), 2)
        finally:
            await live.close()

    async def test_transport_failure_is_not_retried_or_hidden(self):
        bridge = FakeBridge(); bridge.stop = True
        live = relay.LiveText(relay.Mailbox(bridge, job()), interval=.001)
        try:
            live.add('partial')
            await asyncio.sleep(.02)
            with self.assertRaises(ConnectionError): await live.finish()
            self.assertEqual(bridge.frames, [])
        finally: await live.close()

    async def test_bound_and_final_reconciliation(self):
        bridge = FakeBridge(); live = relay.LiveText(relay.Mailbox(bridge, job()))
        try:
            messages = relay.MessageText(live)
            messages.delta('a', 'prefix')
            with self.assertRaises(ValueError): messages.complete('a', 'changed')
            with self.assertRaises(ValueError): live.add('x' * 262144)
            await live.finish()
            self.assertEqual(bridge.frames[0]['text'], 'prefix')
        finally: await live.close()
