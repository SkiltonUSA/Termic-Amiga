"""Exercise the real mailbox transport with synthetic DevBench tool replies."""
import json
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import zlib
import codex_relay as relay


def dump(raw, offset):
    return '\n'.join(f'{offset+i:08x}  '+raw[i:i+16].hex(' ').ljust(47)+'   '+''.join(chr(b) if 32 <= b < 127 else '.' for b in raw[i:i+16])
                     for i in range(0,len(raw),16))


class Session:
    def __init__(self):
        self.files = {}; self.calls = []; self.list_replies = []
        self.check_replies = []; self.read_reply = None; self.delete = True; self.bad_crc = False; self.bad_rename = False
    async def call_tool(self, name, args):
        self.calls.append((name, dict(args)))
        if name == 'amiga_list_dir':
            root = args['path']
            if self.list_replies: text = self.list_replies.pop(0)
            elif not self.files: text = 'Empty directory: '+root
            else:
                text = f'Directory: {root} ({len(self.files)} entries)\n' + '\n'.join(
                    f'  FILE {path.rsplit("/",1)[-1]:30s} {len(raw):>8}  '
                    for path,raw in self.files.items())
        elif name == 'amiga_read_file':
            raw = self.files[args['path']][args['offset']:args['offset']+args['size']]
            text = self.read_reply if self.read_reply is not None else dump(raw,args['offset'])
        elif name == 'amiga_write_file':
            raw = bytes.fromhex(args['hex_data']); offset = args['offset']; path = args['path']
            self.files[path] = self.files.get(path,b'')[:offset] + raw
            text = 'Write command sent'
        elif name == 'amiga_checksum':
            raw = self.files[args['path']]; crc = 0 if self.bad_crc else zlib.crc32(raw)
            text = self.check_replies.pop(0) if self.check_replies else f'CRC32: {crc:08X}\nSize: {len(raw)} bytes'
        elif name == 'amiga_rename':
            if self.bad_rename: text = 'Error: fixture rename failed'
            else:
                self.files[args['new_path']] = self.files.pop(args['old_path'])
                text = 'Renamed: '+args['old_path']+' -> '+args['new_path']
        elif name == 'amiga_run_script':
            self.assert_delete(args['script'])
            text = '[OK] T:fixture-capture'
        else: raise AssertionError(name)
        return SimpleNamespace(isError=False,content=[SimpleNamespace(text=text)])
    def assert_delete(self, script):
        assert script.startswith('FailAt 10\nDelete '), script
        if self.delete: self.files.pop(script.split('Delete ',1)[1],None)


class BridgeMailboxTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.bridge = relay.Bridge('http://localhost/mcp', 'DH0:Fixture/Relay')
        self.session = self.bridge.session = Session()
    def path(self, name): return self.bridge.path(name)

    async def test_present_missing_empty_and_multichunk_files_without_shell(self):
        raw = json.dumps({'fixture':'x'*2500}).encode()
        self.session.files[self.path('request.json')] = raw
        self.session.files[self.path('empty.out')] = b''
        self.assertEqual(await self.bridge.read('request.json'),raw.decode())
        self.assertEqual(await self.bridge.read('empty.out'),'')
        self.assertIsNone(await self.bridge.read('missing.out'))
        self.assertEqual({n for n,a in self.session.calls},{'amiga_list_dir','amiga_read_file'})
        offsets = [a['offset'] for n,a in self.session.calls if n=='amiga_read_file']
        self.assertEqual(offsets,[0,1024,2048])
        with self.assertRaises(ValueError): await self.bridge.read('../outside')

    async def test_bad_directory_replies_never_mean_absent(self):
        bad = ['[OK] T:fixture-capture','Not found or unreadable: DH0:Fixture/Relay',
               'Empty directory: DH0:Other', 'Directory: DH0:Fixture/Relay (1 entries)',
               'Directory: DH0:Fixture/Relay (1 entries)\ninvalid entry', 'Error: fixture failure']
        async def pause(seconds): pass
        for reply in bad:
            self.session.list_replies = [reply]*3
            with patch.object(relay.asyncio,'sleep',pause), self.assertRaises(ConnectionError):
                await self.bridge.read('request.json')
        self.session.list_replies = ['Not found or unreadable: DH0:Fixture/Relay']
        with patch.object(relay.asyncio,'sleep',pause):
            self.assertIsNone(await self.bridge.read('request.json'))

    async def test_unreadable_or_partial_file_is_not_empty_or_absent(self):
        self.session.files[self.path('request.json')] = b'fixture'
        async def pause(seconds): pass
        for reply in ('Not found or unreadable: DH0:Fixture/Relay/request.json',
                      'File is empty or could not be read: DH0:Fixture/Relay/request.json',
                      dump(b'fix',0),dump(b'fixture',1)):
            self.session.read_reply = reply
            with patch.object(relay.asyncio,'sleep',pause), self.assertRaises(ConnectionError):
                await self.bridge.read('request.json')
        self.session.files[self.path('request.json')] = b'x'*(relay.MAX_JSON+1)
        with self.assertRaises(ValueError): await self.bridge.read('request.json')

    async def test_publish_checks_crc_and_uses_one_rename(self):
        self.session.files[self.path('job.in.tmp')] = b'stale longer partial data'
        value = {'text':'fixture '*400}
        await self.bridge.write('job.in',value)
        self.assertEqual(json.loads(self.session.files[self.path('job.in')]),value)
        self.assertNotIn(self.path('job.in.tmp'),self.session.files)
        self.assertEqual(sum(n=='amiga_rename' for n,a in self.session.calls),1)
        self.assertTrue(all('Rename' not in a.get('script','') for n,a in self.session.calls))
        old = self.session.files[self.path('job.in')]
        with self.assertRaises(ConnectionError): await self.bridge.write('job.in',{'text':'second'})
        self.assertEqual(self.session.files[self.path('job.in')],old)

    async def test_failed_checksum_or_rename_never_replays_publication(self):
        self.session.bad_crc = True
        with self.assertRaises(ConnectionError): await self.bridge.write('job.in',{'text':'fixture'})
        self.assertFalse(any(n=='amiga_rename' for n,a in self.session.calls))
        self.session.bad_crc = False; self.session.bad_rename = True
        with self.assertRaises(ConnectionError): await self.bridge.write('job.in',{'text':'fixture'})
        self.assertEqual(sum(n=='amiga_rename' for n,a in self.session.calls),1)
        self.assertNotIn(self.path('job.in'),self.session.files)

    async def test_removal_requires_confirmed_absence(self):
        self.session.files[self.path('job.out')] = b'fixture'
        self.session.delete = False
        with self.assertRaises(ConnectionError): await self.bridge.remove('job.out')
        self.assertEqual(sum(n=='amiga_run_script' for n,a in self.session.calls),1)
        self.session.delete = True
        await self.bridge.remove('job.out')
        count = len(self.session.calls)
        await self.bridge.remove('job.out')
        self.assertEqual([n for n,a in self.session.calls[count:]],['amiga_list_dir'])

    async def test_unrelated_checksum_error_retries_only_verification(self):
        self.session.check_replies = ['Error: T:fixture-unrelated-reader']
        await self.bridge.write('job.in', {'text':'fixture'})
        self.assertEqual(sum(n=='amiga_checksum' for n,a in self.session.calls),2)
        self.assertEqual(sum(n=='amiga_write_file' for n,a in self.session.calls),1)
        self.assertEqual(sum(n=='amiga_rename' for n,a in self.session.calls),1)
        self.assertEqual(json.loads(self.session.files[self.path('job.in')]),{'text':'fixture'})

    async def test_error_words_inside_file_data_are_not_bridge_errors(self):
        raw = b'Error: fixture\nERROR: fixture\n[ERROR] fixture'
        self.session.files[self.path('job.out')] = raw
        self.assertEqual(await self.bridge.read('job.out'), raw.decode())
