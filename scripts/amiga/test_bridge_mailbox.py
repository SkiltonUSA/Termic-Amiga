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
        self.on_checksum = None; self.check_replies = []; self.read_reply = None; self.delete = True; self.bad_crc = False; self.bad_rename = False
        self.after = {}
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
            if self.on_checksum: self.on_checksum()
            raw = self.files[args['path']]; crc = 0 if self.bad_crc else zlib.crc32(raw)
            text = self.check_replies.pop(0) if self.check_replies else f'File: {args["path"]}\nCRC32: {crc:08X}\nSize: {len(raw)} bytes'
        elif name == 'amiga_rename':
            if self.bad_rename: text = 'Error: fixture rename failed'
            else:
                self.files[args['new_path']] = self.files.pop(args['old_path'])
                text = 'Renamed: '+args['old_path']+' -> '+args['new_path']
        elif name == 'amiga_run_script':
            self.assert_delete(args['script'])
            text = '[OK] T:fixture-capture'
        else: raise AssertionError(name)
        if self.after.get(name):
            fault = self.after[name].pop(0)
            if isinstance(fault, Exception): raise fault
            text = fault
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
        self.assertEqual({n for n,a in self.session.calls},{'amiga_list_dir','amiga_read_file','amiga_checksum'})
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

    async def test_same_size_wrong_bytes_and_transient_timeout_are_reread(self):
        self.session.files[self.path('job.out')] = b'correct'
        self.session.after['amiga_read_file'] = [dump(b'incorrc', 0), TimeoutError()]
        async def pause(seconds): pass
        with patch.object(relay.asyncio, 'sleep', pause):
            self.assertEqual(await self.bridge.read('job.out'), 'correct')
        self.assertEqual(sum(n=='amiga_read_file' for n,a in self.session.calls), 3)
        self.assertFalse(any(n in ('amiga_write_file','amiga_rename','amiga_run_script') for n,a in self.session.calls))

    async def test_checksum_must_name_requested_file_and_match_entire_reply(self):
        raw = b'fixture'; self.session.files[self.path('job.out')] = raw
        expected = f'CRC32: {zlib.crc32(raw):08X}\nSize: {len(raw)} bytes'
        async def pause(seconds): pass
        for bad in (expected, 'File: DH0:Other/job.out\n'+expected,
                    'File: '+self.path('job.out')+'\n'+expected+'\nextra'):
            self.session.check_replies = [bad]*9
            with patch.object(relay.asyncio, 'sleep', pause), self.assertRaises(ConnectionError):
                await self.bridge.read('job.out')

    async def test_malformed_hex_is_transport_failure(self):
        # Correct display width, but an unmatched hex digit cannot form a byte.
        with self.assertRaises(ConnectionError):
            relay.decode_hex_dump('00000000  '+'a'.ljust(47)+'   fixture', 0)

    async def test_lost_write_ack_checks_prefix_without_resending_chunks(self):
        self.session.after['amiga_write_file'] = [TimeoutError()]
        value = {'fixture': 'x'*2500}
        await self.bridge.write('job.in', value)
        self.assertEqual(json.loads(self.session.files[self.path('job.in')]), value)
        offsets=[a['offset'] for n,a in self.session.calls if n=='amiga_write_file']
        self.assertEqual(offsets, [0,1024,2048])
        self.assertEqual(sum(n=='amiga_rename' for n,a in self.session.calls),1)

    async def test_lost_delete_ack_requires_absence_and_never_repeats_delete(self):
        self.session.files[self.path('job.out')] = b'fixture'
        self.session.after['amiga_run_script'] = [TimeoutError()]
        await self.bridge.remove('job.out')
        self.assertNotIn(self.path('job.out'), self.session.files)
        self.assertEqual(sum(n=='amiga_run_script' for n,a in self.session.calls), 1)
        self.session.files[self.path('job.out')] = b'fixture'
        self.session.delete = False
        self.session.after['amiga_run_script'] = [ConnectionError()]
        with self.assertRaises(ConnectionError): await self.bridge.remove('job.out')
        self.assertEqual(sum(n=='amiga_run_script' for n,a in self.session.calls), 2)

    async def test_lost_rename_ack_recovers_only_with_matching_reply(self):
        from test_codex_relay import job
        for slots in (False, True):
            self.setUp(); data=job();data['reply_slots']=slots
            original=self.session.call_tool
            async def respond(name,args):
                reply=await original(name,args)
                if name=='amiga_rename':
                    frame=json.loads(self.session.files.pop(args['new_path']))
                    suffix='.r1' if slots else '.out'
                    self.session.files[self.path(data['id']+suffix)] = json.dumps(
                        {'id':data['id'],'seq':frame['seq'],'ok':True,'text':'built once'}).encode()
                    raise TimeoutError()
                return reply
            self.session.call_tool=respond
            self.assertEqual(await relay.Mailbox(self.bridge,data).send('tool',name='build',arguments={}), 'built once')
            self.assertEqual(sum(n=='amiga_write_file' for n,a in self.session.calls),1)
            self.assertEqual(sum(n=='amiga_rename' for n,a in self.session.calls),1)

    async def test_replacement_during_multichunk_read_cannot_mix_versions(self):
        path=self.path('job.out');self.session.files[path]=b'a'*2500
        original=self.session.call_tool
        async def replace(name,args):
            reply=await original(name,args)
            if name=='amiga_read_file' and args['offset']==0:
                self.session.files[path]=b'b'*2500
            return reply
        self.session.call_tool=replace
        async def pause(seconds): pass
        with patch.object(relay.asyncio,'sleep',pause):
            self.assertEqual(await self.bridge.read('job.out'),'b'*2500)
        self.assertEqual(sum(n=='amiga_read_file' for n,a in self.session.calls),6)

    async def test_unconfirmed_write_is_never_published_or_replayed(self):
        original=self.session.call_tool
        async def lose(name,args):
            reply=await original(name,args)
            if name=='amiga_write_file':
                self.session.files[args['path']]=b'partial'
                raise TimeoutError()
            return reply
        self.session.call_tool=lose
        with self.assertRaises(ConnectionError): await self.bridge.write('job.in',{'text':'fixture'})
        self.assertEqual(sum(n=='amiga_write_file' for n,a in self.session.calls),1)
        self.assertFalse(any(n=='amiga_rename' for n,a in self.session.calls))

    async def test_uncertain_publication_does_not_accept_stale_reply_or_ignore_stop(self):
        from test_codex_relay import job
        data=job();data['reply_slots']=True
        original=self.session.call_tool
        async def stop(name,args):
            reply=await original(name,args)
            if name=='amiga_rename':
                self.session.files.pop(args['new_path'])
                self.session.files[self.path(data['id']+'.r1')]=json.dumps(
                    {'id':data['id'],'seq':0,'ok':True,'text':'stale'}).encode()
                self.session.files[self.path(data['id']+'.stopped')]=b'stopped'
                raise ConnectionError()
            return reply
        self.session.call_tool=stop
        box=relay.Mailbox(self.bridge,data)
        with self.assertRaises(ConnectionError): await box.send('tool',name='build',arguments={})
        with self.assertRaises(ConnectionError): await box.send('tool',name='build',arguments={})
        self.assertTrue(box.broken)
        self.assertEqual(sum(n=='amiga_rename' for n,a in self.session.calls),1)

    async def test_reply_and_stop_share_one_fresh_snapshot_per_poll(self):
        self.session.files[self.path('job.r1')] = b'fixture reply'
        values = await self.bridge.read_many('job.r1', 'job.stopped')
        self.assertEqual(values, {'job.r1':'fixture reply','job.stopped':None})
        self.assertEqual(sum(n=='amiga_list_dir' for n,a in self.session.calls),1)
        del self.session.files[self.path('job.r1')]
        self.session.files[self.path('job.stopped')] = b'stopped'
        values = await self.bridge.read_many('job.r1', 'job.stopped')
        self.assertEqual(values, {'job.r1':None,'job.stopped':'stopped'})
        self.assertEqual(sum(n=='amiga_list_dir' for n,a in self.session.calls),2)

    async def test_stop_before_upload_or_before_publication_never_publishes(self):
        stop = self.path('job.stopped')
        self.session.files[stop] = b'stopped'
        with self.assertRaises(ConnectionError):
            await self.bridge.write('job.in', {'kind':'tool'}, stopped='job.stopped')
        self.assertFalse(any(n=='amiga_write_file' for n,a in self.session.calls))
        del self.session.files[stop]
        self.session.on_checksum = lambda: self.session.files.update({stop:b'stopped'})
        with self.assertRaises(ConnectionError):
            await self.bridge.write('job.in', {'kind':'tool'}, stopped='job.stopped')
        self.assertEqual(sum(n=='amiga_write_file' for n,a in self.session.calls),1)
        self.assertFalse(any(n=='amiga_rename' for n,a in self.session.calls))

    async def test_legacy_cleanup_refreshes_snapshot_without_overwriting_request(self):
        self.session.files[self.path('job.out')] = b'old reply'
        self.session.files[self.path('job.in.tmp')] = b'stale upload'
        await self.bridge.write('job.in', {'kind':'ping'}, stopped='job.stopped', previous='job.out')
        self.assertNotIn(self.path('job.out'),self.session.files)
        self.assertNotIn(self.path('job.in.tmp'),self.session.files)
        self.assertEqual(json.loads(self.session.files[self.path('job.in')]),{'kind':'ping'})
        self.session.calls.clear()
        with self.assertRaises(ConnectionError):
            await self.bridge.write('job.in', {'kind':'tool'}, stopped='job.stopped', previous='job.out')
        self.assertEqual([n for n,a in self.session.calls],['amiga_list_dir'])
