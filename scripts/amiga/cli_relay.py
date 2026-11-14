"""Claude Code and Cursor CLI adapters for the bounded Amiga mailbox."""
import asyncio
import hmac
import json
import os
from pathlib import Path
import secrets
import signal
import sys
import tempfile

from host_process import executable_command, process_options
from codex_relay import Mailbox, THREAD, TOOLS, WRITE, dynamic_tools, LiveText

NAMES = {'claude': 'Claude', 'cursor': 'Cursor'}
POLL_SECONDS = 20
WAIT_TOOL = {
    'name': 'await_operation',
    'description': 'Collect an Amiga operation result. Waits up to 20 seconds. If still running, call again with the same operation_id. Never repeat the original tool to check progress.',
    'inputSchema': {'type': 'object', 'properties': {'operation_id': {'type': 'string'}},
                    'required': ['operation_id'], 'additionalProperties': False},
}


def clean_env(provider, state):
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(('ANTHROPIC_', 'CLAUDE_CODE_', 'CURSOR_', 'OPENAI_', 'CODEX_'))
           and k not in ('CLAUDECODE', 'ENABLE_TOOL_SEARCH')}
    if provider == 'cursor':
        env['CURSOR_CONFIG_DIR'] = str(state / 'cursor')
    else:
        env['ENABLE_TOOL_SEARCH'] = 'false'
        env['CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC'] = '1'
    return env


async def check_login(provider, executable, state):
    command = executable_command(executable) + (['auth', 'status'] if provider == 'claude' else ['status', '--format', 'json'])
    proc = await asyncio.create_subprocess_exec(*command, env=clean_env(provider, state),
        stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL, **process_options())
    try:
        raw, _ = await asyncio.wait_for(proc.communicate(), 30)
        data = json.loads(raw)
        ok = (data.get('loggedIn') and data.get('authMethod') == 'claude.ai') if provider == 'claude' else data.get('isAuthenticated')
        if proc.returncode or not ok:
            raise RuntimeError('Sign in to ' + NAMES[provider] + ' on the Mac or Windows PC first')
    finally:
        if proc.returncode is None:
            proc.kill(); await proc.wait()


def command_for(provider, executable, job, work, config):
    if provider == 'claude':
        allowed = ','.join('mcp__amiga__' + t['name'] for t in dynamic_tools(job['readonly']))
        command = executable_command(executable) + ['-p', '--output-format', 'stream-json', '--verbose', '--include-partial-messages',
                   '--tools', '', '--allowedTools', allowed, '--permission-mode', 'dontAsk',
                   '--strict-mcp-config', '--mcp-config', str(config), '--setting-sources', '',
                   '--settings', '{"disableAllHooks":true}', '--disable-slash-commands']
    else:
        command = executable_command(executable) + ['-p', '--output-format', 'stream-json', '--stream-partial-output', '--trust',
                   '--approve-mcps', '--workspace', str(work)]
    if job['model']:
        command += ['--model', job['model']]
    if job['thread']:
        command += ['--resume', job['thread']]
    return command


def mcp_command(endpoint):
    if getattr(sys, 'frozen', False):
        return {'command': sys.executable, 'args': ['--mcp-worker', str(endpoint)]}
    return {'command': sys.executable, 'args': [str(Path(__file__).with_name('relay_mcp.py')), str(endpoint)]}


class CLIText:
    """Only visible assistant text, never reasoning or partial tool arguments."""
    def __init__(self, provider, live):
        self.provider, self.live = provider, live
        self.seen = False
        self.message = None
        self.streamed = set()
        self.text_blocks = set()

    def add(self, text):
        if text:
            self.live.add(text)
            self.seen = True

    def event(self, event):
        kind = event.get('type')
        if self.provider == 'claude' and kind == 'stream_event':
            raw = event.get('event', {})
            subtype = raw.get('type')
            if subtype == 'message_start':
                self.message = raw.get('message', {}).get('id')
            elif subtype == 'content_block_start' and raw.get('content_block', {}).get('type') == 'text':
                self.streamed.add(self.message)
                self.text_blocks.add((self.message, raw.get('index')))
                self.add(raw['content_block'].get('text', ''))
            elif subtype == 'content_block_delta' and raw.get('delta', {}).get('type') == 'text_delta':
                self.streamed.add(self.message)
                self.text_blocks.add((self.message, raw.get('index')))
                self.add(raw['delta'].get('text', ''))
            elif subtype == 'content_block_stop':
                block = (self.message, raw.get('index'))
                if block in self.text_blocks:
                    self.add('\n')
                    self.text_blocks.remove(block)
        elif kind == 'assistant':
            message = event.get('message', {})
            if self.provider == 'cursor':
                # Cursor's pre-tool and final flush events repeat its deltas.
                if 'timestamp_ms' not in event or 'model_call_id' in event:
                    return
            elif message.get('id') in self.streamed:
                return
            for block in message.get('content', []):
                if block.get('type') == 'text':
                    self.add(block.get('text', '') + ('\n' if self.provider == 'claude' else ''))
        elif kind == 'result' and not self.seen and isinstance(event.get('result'), str):
            self.add(event['result'] + '\n')


class ToolServer:
    def __init__(self, mailbox, job, live=None):
        self.mailbox, self.job, self.live = mailbox, job, live
        self.token = secrets.token_urlsafe(32)
        self.calls = 0
        self.lock = asyncio.Lock()
        self.clients = set()
        self.operations = {}
        self.pending = None

    async def dispatch(self, request):
        if not isinstance(request, dict) or not hmac.compare_digest(str(request.get('token', '')), self.token):
            raise ValueError('Invalid relay client')
        if request.get('op') == 'list':
            catalog = [{k: v for k, v in t.items() if k != 'type'} for t in dynamic_tools(self.job['readonly'])]
            if self.job.get('provider') == 'cursor':
                for tool in catalog:
                    tool['description'] += ' Starts an operation; collect its actual result using await_operation before taking the next step.'
                catalog.append(WAIT_TOOL)
            return catalog
        if request.get('op') != 'call':
            raise ValueError('Unknown relay operation')
        name, arguments = request.get('name'), request.get('arguments')
        polling = self.job.get('provider') == 'cursor'
        if polling and name == 'await_operation':
            if not isinstance(arguments, dict) or set(arguments) != {'operation_id'}:
                raise ValueError('Supply operation_id')
            return await self.await_operation(arguments['operation_id'])
        if name not in TOOLS or (self.job['readonly'] and name in WRITE) or not isinstance(arguments, dict):
            raise ValueError('Tool unavailable')
        if polling:
            # The CLI may time out a synchronous MCP call before the Amiga
            # finishes. Keep ownership here, outside the MCP request lifetime.
            # No second native action starts until the first result is collected.
            if self.pending:
                return self.ticket(self.pending)
            if self.calls >= self.job['limit']:
                raise ValueError('Task tool-call limit reached')
            ident = secrets.token_hex(8)
            self.pending = ident
            self.operations[ident] = (name, asyncio.create_task(self.operation(name, arguments)))
            return self.ticket(ident)
        return await self.execute(name, arguments)

    def ticket(self, ident):
        return json.dumps({'status': 'running', 'operation_id': ident,
                           'tool': self.operations[ident][0],
                           'next': 'Call await_operation with this operation_id. Do not repeat the original tool.'})

    async def operation(self, name, arguments):
        try:
            return {'status': 'completed', 'result': await self.execute(name, arguments)}
        except Exception as error:
            return {'status': 'failed', 'error': str(error) if isinstance(error, ValueError)
                    else 'Amiga tool connection failed. The outcome may be unknown; do not replay the operation.'}

    async def await_operation(self, ident):
        if not isinstance(ident, str) or ident not in self.operations:
            raise ValueError('Unknown operation_id for this task')
        name, task = self.operations[ident]
        try:
            # A cancelled MCP poll must not cancel the native operation.
            result = await asyncio.wait_for(asyncio.shield(task), POLL_SECONDS)
        except asyncio.TimeoutError:
            return self.ticket(ident)
        if self.pending == ident:
            self.pending = None
        # Retain results for this bounded task so a lost poll reply can be
        # collected again without replaying a build, edit or program.
        return json.dumps({'operation_id': ident, 'tool': name, **result})

    async def execute(self, name, arguments):
        async with self.lock:
            if self.calls >= self.job['limit']:
                raise ValueError('Task tool-call limit reached')
            self.calls += 1
            if self.live:
                await self.live.finish()
            await self.mailbox.text('\n[Amiga: ' + name + ']\n')
            result = await self.mailbox.send('tool', name=name, arguments=arguments)
            await self.mailbox.text('[Amiga tool returned: ' + name + ']\n')
            return result

    async def handle(self, reader, writer):
        task = asyncio.current_task(); self.clients.add(task)
        try:
            raw = await asyncio.wait_for(reader.readline(), 10)
            if len(raw) > 65536: raise ValueError('Tool request too large')
            try:
                result = {'result': await self.dispatch(json.loads(raw))}
            except Exception as error:
                result = {'error': str(error) if isinstance(error, ValueError) else 'Amiga tool connection failed'}
            writer.write((json.dumps(result) + '\n').encode()); await writer.drain()
        except (ValueError, ConnectionError, asyncio.TimeoutError):
            pass
        finally:
            writer.close()
            await writer.wait_closed()
            self.clients.discard(task)

    async def close(self):
        tasks = list(self.clients) + [task for _, task in self.operations.values()]
        for task in tasks: task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)


async def terminate(proc):
    if proc is None or proc.returncode is not None: return
    if os.name == 'nt':
        # Terminate only this CLI and its MCP children, never other user agents.
        killer = await asyncio.create_subprocess_exec('taskkill', '/PID', str(proc.pid), '/T', '/F',
            stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
        await killer.wait()
    else:
        try: os.killpg(proc.pid, signal.SIGTERM)
        except ProcessLookupError: pass
    try: await asyncio.wait_for(proc.wait(), 5)
    except asyncio.TimeoutError:
        if os.name != 'nt':
            try: os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError: pass
        else: proc.kill()
        await proc.wait()


async def run_cli_job(bridge, job, executable, state):
    provider = job['provider']; label = NAMES[provider]
    mailbox = Mailbox(bridge, job)
    proc = heartbeat = reader_task = server = cursor_config = None
    live = LiveText(mailbox)
    tools = ToolServer(mailbox, job, live)
    status = 'failed'
    state.mkdir(parents=True, exist_ok=True, mode=0o700)
    work = state / (provider + '-workspace'); work.mkdir(exist_ok=True, mode=0o700)
    try:
        await mailbox.send('ping')
        await mailbox.text('[Connecting to ' + label + ' on host...]\n')
        async def keepalive():
            while True:
                await asyncio.sleep(10); await mailbox.send('ping')
        heartbeat = asyncio.create_task(keepalive())
        await check_login(provider, executable, state)
        with tempfile.TemporaryDirectory(prefix='termic-job-', dir=state) as temp:
            directory = Path(temp)
            server = await asyncio.start_server(tools.handle, '127.0.0.1', 0, limit=65537)
            endpoint = directory / 'endpoint.json'
            endpoint.write_text(json.dumps({'port': server.sockets[0].getsockname()[1], 'token': tools.token}))
            endpoint.chmod(0o600)
            config = directory / 'mcp.json'
            config.write_text(json.dumps({'mcpServers': {'amiga': mcp_command(endpoint)}}))
            if provider == 'cursor':
                cfg = state / 'cursor'; cfg.mkdir(exist_ok=True, mode=0o700)
                # Dedicated relay config, preserve login metadata if CLI stores any here.
                path = cfg / 'cli-config.json'
                settings = json.loads(path.read_text()) if path.exists() else {}
                settings.update({'version': 1, 'editor': {'vimMode': False}, 'permissions': {
                    'allow': ['Mcp(amiga:' + t['name'] + ')' for t in dynamic_tools(job['readonly'])] + ['Mcp(amiga:await_operation)'],
                    'deny': ['Shell(*)', 'Read(**)', 'Write(**)', 'WebFetch(*)']}})
                path.write_text(json.dumps(settings)); path.chmod(0o600)
                # CURSOR_CONFIG_DIR selects CLI settings/auth, but MCP discovery
                # uses the workspace's .cursor/mcp.json on current Cursor CLI.
                project_config = work / '.cursor'
                project_config.mkdir(exist_ok=True, mode=0o700)
                cursor_config = project_config / 'mcp.json'
                cursor_config.write_text(config.read_text())
                cursor_config.chmod(0o600)
            command = command_for(provider, executable, job, work, config)
            proc = await asyncio.create_subprocess_exec(*command, cwd=work, env=clean_env(provider, state),
                stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.DEVNULL, limit=4 * 1024 * 1024, start_new_session=os.name != 'nt', **process_options())
            guidance = '\n\n'.join(g['path'] + ':\n' + g['text'] for g in job['guidance'])
            prompt = (job['instructions'] + '\nUse only the amiga MCP tools for the remote Amiga task. '
                      'The host workspace is empty. Do not use local files, commands, browser, subagents or other tools. '
                      'Write ASCII C89 or ARexx. Use run_arexx or build then run_program when execution is requested.\n' + guidance + '\nUser task:\n' + job['prompt'])
            if provider == 'cursor':
                prompt += ('\nAmiga tools return an operation_id immediately. Call await_operation with that ID '
                           'until completed or failed, then inspect the actual result. A running response is not '
                           'success or failure. Do not repeat the original tool, switch tools, or finish the task '
                           'while its operation is pending. Polls do not consume the native tool-call limit.')
            proc.stdin.write(prompt.encode()); await proc.stdin.drain(); proc.stdin.close()
            async def consume():
                text = CLIText(provider, live)
                result = None; saved_session = None
                while raw := await proc.stdout.readline():
                    event = json.loads(raw)
                    session = event.get('session_id')
                    if session:
                        if not isinstance(session, str) or not THREAD.fullmatch(session):
                            raise ValueError('Invalid host conversation identifier')
                        if saved_session and session != saved_session:
                            raise ValueError('Host changed conversation identifier during a turn')
                        if session != saved_session:
                            await mailbox.send('session', thread=session)
                            saved_session = session
                    text.event(event)
                    if event.get('type') == 'result':
                        result = event
                await live.finish()
                code = await proc.wait()
                return code == 0 and result is not None and not result.get('is_error', False) and result.get('subtype') == 'success'
            reader_task = asyncio.create_task(consume())
            done, _ = await asyncio.wait([reader_task, heartbeat, live.task], timeout=1500, return_when=asyncio.FIRST_COMPLETED)
            if live.task in done: await live.task
            if heartbeat in done: await heartbeat
            if reader_task not in done: raise TimeoutError('Host task exceeded 25 minutes')
            succeeded = await reader_task
            if tools.pending:
                await mailbox.text('[Agent finished before collecting its Amiga operation result. Task failed.]\n')
            elif succeeded: status = 'completed'
            else: await mailbox.text('[' + label + ' failed. Check host sign-in, selected model and usage limits.]\n')
    except (Exception, asyncio.CancelledError) as error:
        try:
            await live.finish()
            message = str(error) if isinstance(error, FileNotFoundError) else 'Check host sign-in, selected model and the proxy log.'
            await mailbox.text('[' + label + ' relay stopped. ' + message + ']\n')
        except Exception: pass
        print(label + ' relay: ' + type(error).__name__, flush=True)
        if isinstance(error, asyncio.CancelledError): raise
    finally:
        await terminate(proc)
        for task in (heartbeat, reader_task):
            if task: task.cancel()
        await asyncio.gather(*[t for t in (heartbeat, reader_task) if t], return_exceptions=True)
        await live.close()
        if server: server.close(); await server.wait_closed()
        await tools.close()
        if cursor_config:
            cursor_config.unlink(missing_ok=True)
        try:
            await mailbox.send('done', status=status)
            await bridge.remove(job['id'] + '.in', job['id'] + '.out', job['id'] + '.stopped')
        except Exception: pass
    return status == 'completed'
