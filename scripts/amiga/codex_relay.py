#!/usr/bin/env python3
"""Termic Amiga -> existing DevBench MCP -> local Codex app-server.

No network listener. Credentials and Codex history stay on the Mac. Only the
native worker's bounded tools are exposed; the model cannot call the bridge.
"""
import argparse
import asyncio
from contextlib import AsyncExitStack
import fcntl
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import time
import unicodedata

ID = re.compile(r'[0-9a-f]{8}-[0-9a-f]{8}-[0-9a-f]{4}\Z')
THREAD = re.compile(r'[0-9a-fA-F-]{1,64}\Z')
TOOLS = {'list_sources', 'read_source', 'read_file', 'write_source', 'write_file', 'build'}
WRITE = {'write_source', 'write_file', 'build'}
MAX_JSON = 65536


def validate_job(job):
    if not isinstance(job, dict) or job.get('protocol') != 1 or not ID.fullmatch(job.get('id', '')):
        raise ValueError('Unsupported relay request')
    for key, limit in [('prompt', 8192), ('instructions', 8192), ('model', 127), ('thread', 64)]:
        value = job.get(key)
        if not isinstance(value, str) or len(value.encode('utf-8')) > limit or '\0' in value:
            raise ValueError('Invalid relay request field')
    if not job['prompt'].strip() or (job['thread'] and not THREAD.fullmatch(job['thread'])):
        raise ValueError('Invalid prompt or conversation identifier')
    if type(job.get('readonly')) is not bool or type(job.get('limit')) is not int or not 1 <= job['limit'] <= 32:
        raise ValueError('Invalid relay access limits')
    guides = job.get('guidance')
    if not isinstance(guides, list) or len(guides) > 2:
        raise ValueError('Invalid project guidance')
    for guide in guides:
        if guide.get('path') not in ('AGENTS.md', '.agents/skills/termic/SKILL.md') or not isinstance(guide.get('text'), str) or len(guide['text'].encode('utf-8')) > 8192:
            raise ValueError('Invalid project guidance')
    # Tool descriptions/schema come from our code, never an untrusted mailbox.
    return job


def dynamic_tools(readonly):
    def spec(name, description, fields):
        return {'type': 'function', 'name': name, 'description': description,
                'inputSchema': {'type': 'object', 'properties': {x: {'type': 'string'} for x in fields},
                                'required': fields, 'additionalProperties': False}}
    all_tools = [spec('list_sources', 'List C/H sources in the selected Amiga task.', []),
                 spec('read_source', 'Read main.c from the Amiga task.', []),
                 spec('read_file', 'Read a task C/H source or named project guidance.', ['path']),
                 spec('write_source', 'Replace main.c, preserving a backup. Maximum 32 KiB.', ['source']),
                 spec('write_file', 'Save a task C/H file with a backup. Maximum 32 KiB.', ['path', 'source']),
                 spec('build', 'Compile the selected task on the Amiga using its native build command.', [])]
    return [t for t in all_tools if not readonly or t['name'] not in WRITE]


def display_text(text):
    # Classic TextEditor transcript is plain text, not UTF-8 or terminal escapes.
    text = text.replace('\u2014', '-').replace('\u2013', '-').replace('\u2019', "'")
    text = unicodedata.normalize('NFKD', text).encode('ascii', 'replace').decode('ascii')
    return ''.join(c for c in text if c in '\n\t' or 32 <= ord(c) < 127)


class Bridge:
    def __init__(self, url, root='Termic:Relay'):
        if not re.fullmatch(r'[A-Za-z0-9:_/-]+', root):
            raise ValueError('Invalid Amiga relay drawer')
        self.url, self.root = url, root.rstrip('/')
        self.stack = AsyncExitStack()
        self.lock = asyncio.Lock()

    async def __aenter__(self):
        from mcp import ClientSession
        from mcp.client.streamable_http import streamablehttp_client
        read, write, _ = await self.stack.enter_async_context(streamablehttp_client(self.url))
        self.session = await self.stack.enter_async_context(ClientSession(read, write))
        await self.session.initialize()
        return self

    async def __aexit__(self, *args):
        await self.stack.__aexit__(*args)

    def path(self, name):
        if not re.fullmatch(r'[a-zA-Z0-9.-]+', name):
            raise ValueError('Invalid mailbox filename')
        return self.root + '/' + name

    async def call(self, name, args):
        r = await asyncio.wait_for(self.session.call_tool(name, args), 45)
        text = '\n'.join(getattr(c, 'text', '') for c in r.content)
        if r.isError or any(x in text for x in ('[ERROR]', 'ERROR:', '[STILL RUNNING]', 'Cannot read remote file')):
            raise ConnectionError('Amiga bridge operation failed: ' + name)
        return text

    async def shell(self, script):
        return await self.call('amiga_run_script', {'script': 'FailAt 10\n' + script, 'timeout': 10})

    async def read(self, name):
        async with self.lock:
            remote = self.path(name)
            exists = await self.shell(f'If EXISTS {remote}\n Echo TERMIC_FILE_PRESENT\nEndIf')
            if 'TERMIC_FILE_PRESENT' not in exists:
                return None
            with tempfile.TemporaryDirectory(prefix='termic-relay-') as d:
                local = Path(d) / 'message'
                await self.call('amiga_pull_file', {'amiga_path': remote, 'local_path': str(local)})
                raw = local.read_bytes()
                if len(raw) > MAX_JSON:
                    raise ValueError('Relay message too large')
                return raw.decode('utf-8')

    async def write(self, name, value):
        raw = json.dumps(value, ensure_ascii=True, separators=(',', ':')).encode()
        if len(raw) > MAX_JSON:
            raise ValueError('Relay message too large')
        async with self.lock:
            remote = self.path(name)
            with tempfile.TemporaryDirectory(prefix='termic-relay-') as d:
                local = Path(d) / 'message'
                local.write_bytes(raw)
                await self.call('amiga_push_file', {'local_path': str(local), 'amiga_path': remote + '.tmp'})
                await self.shell(f'If EXISTS {remote}\n Delete {remote}\nEndIf\nRename {remote}.tmp {remote}')

    async def remove(self, *names):
        async with self.lock:
            for name in names:
                remote = self.path(name)
                await self.shell(f'If EXISTS {remote}\n Delete {remote}\nEndIf')


class Mailbox:
    def __init__(self, bridge, job):
        self.bridge, self.job = bridge, job
        self.seq = 0
        self.broken = False
        self.lock = asyncio.Lock()

    async def send(self, kind, **fields):
        async with self.lock:
            if self.broken:
                raise ConnectionError('Relay mailbox is disconnected')
            try:
                return await self.exchange(kind, fields)
            except (TimeoutError, ConnectionError, json.JSONDecodeError):
                self.broken = True
                raise

    async def exchange(self, kind, fields):
        ident = self.job['id']
        if await self.bridge.read(ident + '.stopped') is not None:
            raise ConnectionError('Amiga task stopped')
        self.seq += 1
        await self.bridge.write(ident + '.in', {'id': ident, 'seq': self.seq, 'kind': kind, **fields})
        deadline = time.monotonic() + (180 if kind == 'tool' else 45)
        while time.monotonic() < deadline:
            raw = await self.bridge.read(ident + '.out')
            if raw:
                reply = json.loads(raw)
                if reply.get('id') == ident and reply.get('seq') == self.seq:
                    if not reply.get('ok'):
                        raise RuntimeError('Amiga rejected operation or tool-call limit reached')
                    return reply.get('text', '')
            if await self.bridge.read(ident + '.stopped') is not None:
                raise ConnectionError('Amiga task stopped')
            await asyncio.sleep(.25)
        raise TimeoutError('Amiga relay did not acknowledge the message')

    async def text(self, text):
        plain = display_text(text)
        for offset in range(0, len(plain), 4096):
            await self.send('text', text=plain[offset:offset + 4096])


class Codex:
    def __init__(self, executable, home, work):
        self.executable, self.home, self.work = executable, home, work
        self.pending, self.events, self.next_id = {}, asyncio.Queue(), 0
        self.proc = self.reader = None

    async def start(self):
        # Isolate MCP, plugins and project configuration from the user's normal Codex.
        env = {k: v for k, v in os.environ.items()
               if not k.startswith(('CODEX_', 'OPENAI_'))}
        env['CODEX_HOME'] = str(self.home)
        flags = ['shell_tool', 'multi_agent', 'apps', 'browser_use', 'computer_use', 'skill_search']
        command = [self.executable, 'app-server', '--listen', 'stdio://']
        for flag in flags:
            command += ['--disable', flag]
        command += ['-c', 'web_search="disabled"', '-c', 'features.skip_host_skill_discovery=true']
        self.proc = await asyncio.create_subprocess_exec(*command, cwd=self.work, env=env,
            stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.DEVNULL, limit=4 * 1024 * 1024)
        self.reader = asyncio.create_task(self.read_loop())
        await self.rpc('initialize', {'clientInfo': {'name': 'termic_amiga', 'version': '0.1.0'},
                                      'capabilities': {'experimentalApi': True}})
        await self.send({'method': 'initialized'})
        account = await self.rpc('account/read', {'refreshToken': False})
        if not account.get('account') or account['account'].get('type') != 'chatgpt':
            raise RuntimeError('Sign in with ChatGPT on the Mac before starting the relay')

    async def send(self, obj):
        self.proc.stdin.write((json.dumps(obj) + '\n').encode())
        await self.proc.stdin.drain()

    async def rpc(self, method, params):
        self.next_id += 1
        ident = self.next_id
        future = asyncio.get_running_loop().create_future()
        self.pending[ident] = future
        try:
            await self.send({'id': ident, 'method': method, 'params': params})
            return await asyncio.wait_for(future, 60)
        finally:
            self.pending.pop(ident, None)

    async def read_loop(self):
        try:
            while raw := await self.proc.stdout.readline():
                event = json.loads(raw)
                if 'method' not in event and event.get('id') in self.pending:
                    future = self.pending[event['id']]
                    if not future.done():
                        if 'error' in event:
                            future.set_exception(RuntimeError('Codex rejected request: ' + str(event['error'].get('code'))))
                        else:
                            future.set_result(event.get('result', {}))
                else:
                    await self.events.put(event)
        finally:
            for f in self.pending.values():
                if not f.done():
                    f.set_exception(RuntimeError('Codex app-server disconnected'))
            await self.events.put({'method': '_disconnected'})

    async def close(self):
        if self.proc and self.proc.returncode is None:
            self.proc.terminate()
            try:
                await asyncio.wait_for(self.proc.wait(), 5)
            except asyncio.TimeoutError:
                self.proc.kill()
                await self.proc.wait()
        if self.reader:
            await self.reader


def prepare_home(state, auth_home):
    state.mkdir(parents=True, exist_ok=True, mode=0o700)
    home, work = state / 'codex', state / 'workspace'
    home.mkdir(exist_ok=True, mode=0o700)
    work.mkdir(exist_ok=True, mode=0o700)
    auth = auth_home / 'auth.json'
    target = home / 'auth.json'
    if not target.exists():
        if not auth.is_file():
            raise RuntimeError('Run codex login on the Mac first; no file-based sign-in was found')
        target.symlink_to(auth.resolve())
    return home, work


async def run_job(bridge, job, executable, home, work):
    mailbox = Mailbox(bridge, job)
    codex = Codex(executable, home, work)
    thread = turn = None
    heartbeat = None
    status = 'failed'
    calls = 0
    try:
        await mailbox.send('ping')
        await mailbox.text('[Connecting to Codex on Mac...]\n')
        await codex.start()
        print("Codex connected using ChatGPT sign-in.", flush=True)
        async def keepalive():
            while True:
                await asyncio.sleep(10)
                await mailbox.send('ping')
        heartbeat = asyncio.create_task(keepalive())
        config = {'cwd': str(work), 'approvalPolicy': 'never', 'sandbox': 'read-only',
                  'developerInstructions': job['instructions'] + '\nUse only the supplied Amiga tools. '
                  'There is no local project, shell, browser or subagent access. Ask the user to run programs. '
                  'Write ASCII C89 source. Do not attempt to install tools or obtain credentials.'}
        if job['model']:
            config['model'] = job['model']
        if job['thread']:
            response = await codex.rpc('thread/resume', {**config, 'threadId': job['thread'], 'excludeTurns': True})
        else:
            response = await codex.rpc('thread/start', {**config, 'environments': [],
                                                       'dynamicTools': dynamic_tools(job['readonly'])})
        thread = response['thread']['id']
        await mailbox.send('session', thread=thread)
        prompt = ''.join(f"Current project guidance ({g['path']}):\n{g['text']}\n\n" for g in job['guidance'])
        prompt += 'Current user task:\n' + job['prompt']
        started = await codex.rpc('turn/start', {'threadId': thread, 'environments': [],
            'input': [{'type': 'text', 'text': prompt}]})
        turn = started['turn']['id']
        deadline = time.monotonic() + 1500
        while time.monotonic() < deadline:
            get_event = asyncio.create_task(codex.events.get())
            finished, _ = await asyncio.wait([get_event, heartbeat], timeout=min(15, max(.1, deadline-time.monotonic())), return_when=asyncio.FIRST_COMPLETED)
            if not finished:
                get_event.cancel()
                await asyncio.gather(get_event, return_exceptions=True)
                continue
            if heartbeat in finished:
                get_event.cancel()
                await heartbeat
            event = await get_event
            method, params = event.get('method'), event.get('params', {})
            if method == '_disconnected':
                raise RuntimeError('Codex disconnected')
            if 'id' in event and method:
                if method == 'item/tool/call':
                    name = params.get('tool')
                    calls += 1
                    if params.get('threadId') != thread or name not in TOOLS or (job['readonly'] and name in WRITE) or calls > job['limit']:
                        result, ok = 'Tool unavailable or task tool-call limit reached.', False
                    else:
                        print('Amiga tool: ' + name, flush=True)
                        await mailbox.text('\n[Amiga: ' + name + ']\n')
                        result = await mailbox.send('tool', name=name, arguments=params.get('arguments', {}))
                        ok = True
                    await codex.send({'id': event['id'], 'result': {
                        'contentItems': [{'type': 'inputText', 'text': result}], 'success': ok}})
                    if calls > job['limit']:
                        raise RuntimeError('Task tool-call limit reached')
                else:
                    # Never silently approve extra host permissions or command execution.
                    await codex.send({'id': event['id'], 'error': {'code': -32601,
                                      'message': 'This client supports only bounded Amiga tool calls.'}})
            elif method == 'item/completed' and params.get('item', {}).get('type') == 'agentMessage':
                await mailbox.text(params['item'].get('text', '') + '\n')
            elif method == 'turn/completed' and params.get('turn', {}).get('id') == turn:
                status = params['turn']['status']
                if status != 'completed':
                    await mailbox.text('[Codex turn failed or interrupted. Check Mac login, model access and usage limits.]\n')
                break
        else:
            raise TimeoutError('Codex turn exceeded the 25-minute limit')
    except (Exception, asyncio.CancelledError) as error:
        if thread and turn:
            try:
                await codex.rpc('turn/interrupt', {'threadId': thread, 'turnId': turn})
            except Exception:
                pass
        try:
            await mailbox.text('[Relay stopped: ' + type(error).__name__ + '. Check the Mac relay, sign-in and bridge.]\n')
        except Exception:
            pass
        print('Relay task stopped: ' + type(error).__name__, flush=True)
        if isinstance(error, asyncio.CancelledError):
            raise
    finally:
        if heartbeat:
            heartbeat.cancel()
            await asyncio.gather(heartbeat, return_exceptions=True)
        await codex.close()
        try:
            await mailbox.send('done', status=status if status in ('completed', 'failed', 'interrupted') else 'failed')
            await bridge.remove(job['id'] + '.in', job['id'] + '.out', job['id'] + '.stopped')
        except Exception:
            pass
    return status == 'completed'


async def main(args):
    home, work = prepare_home(args.state_dir, args.auth_home)
    lock = (args.state_dir / 'relay.lock').open('a')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise RuntimeError('Another Mac relay is running') from None
    if args.check:
        codex = Codex(args.codex, home, work)
        try:
            await codex.start()
            print('Codex ChatGPT sign-in verified. No credential copied to Amiga.')
        finally:
            await codex.close()
        return
    async with Bridge(args.bridge, args.relay_drawer) as bridge:
        print('Mac relay ready. Select Codex on Mac in Termic, then send a task. Ctrl C stops the relay.', flush=True)
        last = None
        while True:
            raw = await bridge.read('request.json')
            if raw:
                job = validate_job(json.loads(raw))
                if job['id'] != last:
                    last = job['id']
                    print('Processing Amiga task.', flush=True)
                    ok = await run_job(bridge, job, args.codex, home, work)
                    print('Task completed.' if ok else 'Task failed or stopped.', flush=True)
                    if args.once:
                        return 0 if ok else 1
            await asyncio.sleep(2)


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bridge', default='http://127.0.0.1:55010/mcp')
    p.add_argument('--relay-drawer', default='Termic:Relay')
    p.add_argument('--codex', default=shutil.which('codex') or 'codex')
    p.add_argument('--state-dir', type=Path, default=Path.home() / 'Library/Application Support/Termic-Amiga/relay')
    p.add_argument('--auth-home', type=Path, default=Path(os.environ.get('CODEX_HOME', str(Path.home() / '.codex'))))
    p.add_argument('--once', action='store_true', help='Exit after one task')
    p.add_argument('--check', action='store_true', help='Check Mac ChatGPT sign-in without running a model')
    return p

if __name__ == '__main__':
    try:
        sys.exit(asyncio.run(main(parser().parse_args())))
    except KeyboardInterrupt:
        print('Mac relay stopped.')
    except Exception as error:
        print('Relay error: ' + str(error), file=sys.stderr)
        sys.exit(1)
