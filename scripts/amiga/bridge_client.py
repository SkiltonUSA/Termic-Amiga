"""Reservation-aware Amiga gateway client and acceptance-run recovery.

Tokens stay in private per-run files. Output evidence contains only a pointer.
Timeouts retain ownership because cancelling a request does not stop hardware.
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
from urllib.parse import urlsplit, urlunsplit

DEFAULT_BRIDGE = 'http://127.0.0.1:55110/mcp'
SHARED_READS = {'amiga_connect', 'amiga_disconnect', 'amiga_ping', 'amiga_capabilities',
    'amiga_list_tasks', 'amiga_list_clients', 'amiga_sysinfo', 'amiga_arexx_ports',
    'amiga_list_dir', 'amiga_read_file', 'amiga_screenshot', 'amiga_logs'}


def bridge_endpoint(url):
    parts = urlsplit(url)
    if parts.scheme not in ('http', 'https') or not parts.netloc or parts.query or parts.fragment:
        raise ValueError('Use an HTTP(S) gateway URL without query or fragment')
    path = parts.path.rstrip('/')
    if not path.endswith('/mcp'):
        path += '/mcp'
    return urlunsplit((parts.scheme, parts.netloc, path, '', ''))


def reservation_directory():
    base = Path(os.environ.get('LOCALAPPDATA', str(Path.home() / 'AppData/Local'))) if sys.platform == 'win32' else Path.home() / 'Library/Application Support'
    return base / 'Termic-Amiga/bridge-reservations'


def text(result):
    return '\n'.join(getattr(block, 'text', '') for block in result.content)


def failed_tool_reply(result):
    body = text(result).lstrip()
    return result.isError or body.startswith(('ERROR:', 'ERR|', 'Timeout')) or '[STILL RUNNING]' in body


class FileLease:
    """Nonblocking process lock. Never unlink the inode other waiters may use."""
    def __init__(self, path):
        self.path = Path(str(Path(path).resolve()) + '.lock')
        self.file = None

    def acquire(self):
        self.path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        fd = os.open(self.path, os.O_RDWR | os.O_CREAT, 0o600)
        file = os.fdopen(fd, 'r+b')
        try:
            if sys.platform == 'win32':
                import msvcrt
                if not os.fstat(fd).st_size:
                    file.write(b'\0'); file.flush()
                file.seek(0)
                msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            file.close()
            raise ConnectionError('Another process is using this recovery file or output directory') from None
        self.file = file

    def release(self):
        if self.file:
            self.file.close()
            self.file = None


class AcceptanceRun:
    """Persist a non-secret recovery pointer and exclude concurrent output writers."""
    def __init__(self, output, url, reservation_file=None, resume=False):
        self.output = Path(output)
        self.url = bridge_endpoint(url)
        self.explicit = Path(reservation_file).expanduser().resolve() if reservation_file else None
        self.resume = resume
        self.path = None
        self.passed = None
        self.lease = FileLease(self.output / 'bridge-recovery.json')

    def __enter__(self):
        self.lease.acquire()
        try:
            evidence = self.output / 'bridge-recovery.json'
            saved = json.loads(evidence.read_text()) if evidence.exists() else None
            if saved and saved['url'] != self.url:
                raise ConnectionError('Output directory belongs to a different gateway; use a new output directory')
            previous = Path(saved['reservation_file']) if saved else None
            if previous and previous.exists() and self.explicit and self.explicit != previous:
                raise ConnectionError('This output directory has another unresolved reservation')
            if previous and previous.exists() and not (self.resume or self.explicit):
                raise ConnectionError('Previous run needs recovery. Rerun with --reservation-file ' + str(previous))
            if self.explicit and not self.explicit.is_file():
                raise ConnectionError('Recovery file does not exist: ' + str(self.explicit))
            if self.resume and not saved and not self.explicit:
                raise ConnectionError('No recovery evidence; supply the saved --reservation-file')
            self.path = self.explicit or (previous if self.resume else None) or reservation_directory() / (uuid.uuid4().hex + '.json')
            self.path = self.path.resolve()
            pending = evidence.with_suffix('.tmp')
            pending.write_text(json.dumps({'url': self.url, 'reservation_file': str(self.path)}, indent=2))
            pending.replace(evidence)
            (self.output / 'passed.json').unlink(missing_ok=True)
            return self
        except BaseException:
            self.lease.release()
            raise

    def __exit__(self, kind, value, traceback):
        try:
            if kind is None and self.passed is not None:
                (self.output / 'passed.json').write_text(json.dumps(self.passed))
        finally:
            self.lease.release()

    async def __aenter__(self):
        return self.__enter__()

    async def __aexit__(self, *args):
        self.__exit__(*args)


def add_bridge_arguments(parser):
    parser.add_argument('--server', default=DEFAULT_BRIDGE, help='Reservation gateway URL (base or /mcp)')
    parser.add_argument('--reservation-file', type=Path, help='Explicit private recovery file from this failed run')


class ClientSession:
    def __init__(self, *args, bridge_url=DEFAULT_BRIDGE, reservation_path=None,
                 operation_timeout=60, cleanup_timeout=15, **kwargs):
        from mcp import ClientSession as MCPClientSession
        self.session = MCPClientSession(*args, **kwargs)
        self.url = bridge_endpoint(bridge_url)
        self.path = (Path(reservation_path) if reservation_path else reservation_directory() / (uuid.uuid4().hex + '.json')).resolve()
        self.lease = FileLease(self.path)
        self.gateway = False
        self.token = None
        self.lock = asyncio.Lock()
        self.operation_timeout = operation_timeout
        self.cleanup_timeout = cleanup_timeout
        self.uncertain = False
        self.cleanup = None

    def __getattr__(self, name):
        return getattr(self.session, name)

    async def __aenter__(self):
        self.lease.acquire()
        try:
            await self.session.__aenter__()
        except BaseException:
            self.lease.release()
            raise
        return self

    async def __aexit__(self, kind, value, traceback):
        failure = None
        # Callers may impose their own deadline around a tool operation.
        if isinstance(value, (TimeoutError, ConnectionError)):
            self.uncertain = True
        try:
            if self.token and not self.uncertain:
                try:
                    await asyncio.wait_for(self._finish(), self.cleanup_timeout)
                except BaseException as error:
                    failure = error
            if self.token:
                print('Bridge reservation retained. Recover with --reservation-file ' + str(self.path), file=sys.stderr)
            try:
                await self.session.__aexit__(kind, value, traceback)
            except BaseException as error:
                failure = failure or error
        finally:
            self.lease.release()
        if kind is None:
            if failure:
                raise failure
            if self.token:
                raise ConnectionError('Cleanup could not be verified; reservation retained at ' + str(self.path))
            if self.uncertain:
                raise ConnectionError('Bridge operation outcome is uncertain; inspect gateway status before another run')
        elif failure:
            # The original assertion, cancellation or transport failure is primary.
            print('Bridge cleanup also failed (' + type(failure).__name__ + '); original error preserved', file=sys.stderr)
        return False

    async def _finish(self):
        state = await self.control('amiga_bridge_status', {})
        if not state.get('enabled') or state.get('active') or state.get('queued'):
            return
        ports = await self._request('amiga_list_ports', {})
        if failed_tool_reply(ports) or 'TermicJob.' in text(ports):
            return
        if self.cleanup:
            await self.cleanup()
        for _ in range(12):
            if await self.release_if_idle():
                return
            await asyncio.sleep(.25)

    async def _request(self, name, arguments, timeout=None, **kwargs):
        try:
            return await asyncio.wait_for(self.session.call_tool(name, arguments, **kwargs),
                                          self.operation_timeout if timeout is None else timeout)
        except BaseException:
            self.uncertain = True
            raise

    async def initialize(self):
        result = await asyncio.wait_for(self.session.initialize(), self.operation_timeout)
        self.gateway = getattr(getattr(result, 'serverInfo', None), 'name', '') == 'Amiga-MCP'
        if not self.gateway:
            raise ConnectionError('Expected the Amiga-MCP reservation gateway on port 55110; direct backend use is unsupported')
        if self.path.exists():
            saved = json.loads(self.path.read_text())
            if saved.get('url') != self.url:
                raise ConnectionError('Reservation belongs to a different bridge endpoint')
            if not isinstance(saved.get('token'), str) or not saved['token']:
                raise ConnectionError('Invalid recovery file; keep it for inspection: ' + str(self.path))
            state = await self.control('amiga_bridge_status', {})
            if state.get('reservation'):
                try:
                    await self.control('amiga_bridge_resume', {'token': saved['token']})
                except ConnectionError:
                    raise ConnectionError('Recovery token was not accepted; the gateway may belong to another run. File preserved: ' + str(self.path)) from None
                self.token = saved['token']
                if not state.get('enabled'):
                    raise ConnectionError('Gateway is paused. Inspect the target and resolve the pause before resuming; token preserved')
            else:
                # The gateway authoritatively confirms there is no owner.
                self.path.unlink()
        return result

    async def control(self, name, arguments):
        result = await self._request(name, arguments, timeout=min(45, self.operation_timeout))
        if result.isError:
            raise ConnectionError('Amiga bridge control failed: ' + name)
        try:
            reply = json.loads(text(result))
            if not isinstance(reply, dict):
                raise ValueError()
            return reply
        except (TypeError, ValueError):
            self.uncertain = True
            raise ConnectionError('Invalid Amiga bridge control reply') from None

    async def acquire(self):
        if self.token:
            return
        reply = await self.control('amiga_bridge_reserve', {'label': 'Termic acceptance ' + self.path.stem})
        if not isinstance(reply.get('token'), str) or not reply['token']:
            self.uncertain = True
            raise ConnectionError('Reserve reply has no recovery token; gateway operator recovery is required')
        self.token = reply['token']
        created = False
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
            fd = os.open(self.path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            created = True
            with os.fdopen(fd, 'w') as file:
                json.dump({'url': self.url, 'token': self.token}, file)
                file.flush()
                os.fsync(file.fileno())
        except BaseException:
            # No hardware request has been submitted by this session yet.
            try:
                await self.control('amiga_bridge_release', {'token': self.token, 'confirmed_idle': True})
                self.token = None
                if created:
                    self.path.unlink(missing_ok=True)
            except BaseException:
                print('Reservation persistence/release failed; gateway operator recovery is required', file=sys.stderr)
            raise

    async def call_tool(self, name, arguments=None, **kwargs):
        async with self.lock:
            if not self.gateway:
                raise ConnectionError("Initialize the reservation gateway before calling hardware tools")
            if name not in SHARED_READS and not name.startswith('amiga_bridge_'):
                await self.acquire()
            result = await self._request(name, arguments, **kwargs)
            if failed_tool_reply(result):
                self.uncertain = True
            return result

    async def release_if_idle(self):
        async with self.lock:
            if not self.gateway or not self.token:
                return True
            if self.uncertain:
                return False
            state = await self.control('amiga_bridge_status', {})
            if not state.get('enabled') or state.get('active') or state.get('queued'):
                return False
            ports = await self._request('amiga_list_ports', {})
            if failed_tool_reply(ports) or 'TermicJob.' in text(ports):
                return False
            await self.control('amiga_bridge_release', {'token': self.token, 'confirmed_idle': True})
            self.token = None
            self.path.unlink(missing_ok=True)
            return True


async def workspace_cleanup(session, title='Termic Amiga Workspace', about=False):
    """Known test end state: one frontmost workspace, no owned About requester.

    Never quit a window merely because it happens to be frontmost. A foreign
    or ambiguous window leaves the reservation available for explicit recovery.
    """
    async def windows():
        result = await session.call_tool('amiga_list_screen_windows', {})
        if result.isError:
            raise RuntimeError('Cannot verify test window cleanup')
        return [line.strip().split('  pos=', 1)[0] for line in text(result).splitlines() if '  pos=' in line]
    titles = await windows()
    modal = 'About Termic Amiga'
    if about and titles and titles[0] == modal and titles.count(modal) == 1 and titles.count(title) == 1:
        for direction in ('down', 'up'):
            result = await session.call_tool('amiga_input_key', {'rawkey': '0x45', 'direction': direction})
            if result.isError:
                raise RuntimeError('Could not close the test About requester')
        for _ in range(10):
            await asyncio.sleep(.1)
            titles = await windows()
            if modal not in titles:
                break
    if not titles or titles[0] != title or titles.count(title) != 1 or modal in titles or 'Termic Project' in titles:
        raise RuntimeError('Test UI state is not verified; inspect the target before resuming this run')


async def capture_png(session, target, window=None):
    """Use the bounded MCP operation, not the backend's REST screenshot route."""
    result = await session.call_tool('amiga_screenshot', {'window': window} if window else {})
    match = re.search(r'Screenshot saved: (.+) \((\d+)x(\d+), (RLE|truecolor), (\d+) rows received\)', text(result))
    if result.isError or not match or match[3] != match[5]:
        raise RuntimeError('Incomplete Amiga screenshot')
    source = Path(match[1])
    if source.suffix.lower() == '.png':
        shutil.copyfile(source, target)
    else:
        await asyncio.to_thread(subprocess.run,
            ['sips', '-s', 'format', 'png', str(source), '--out', str(target)],
            check=True, capture_output=True, timeout=30)
