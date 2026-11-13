#!/usr/bin/env python3
"""Termic AI Proxy desktop entry point and bundled worker modes."""
import asyncio
import json
import os
from pathlib import Path
import shutil
import sys

if not getattr(sys, 'frozen', False):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'amiga'))
import codex_relay
from host_process import executable_command

VERSION = '0.1.0 Beta'
PROVIDERS = ('codex', 'claude', 'cursor')


def find_agent(name):
    names = ('cursor-agent', 'agent') if name == 'cursor' else (name,)
    for item in names:
        found = shutil.which(item)
        if found: return found
        for parent in (Path.home() / '.local/bin', Path('/opt/homebrew/bin'), Path('/usr/local/bin')):
            for suffix in ('.exe', '') if os.name == 'nt' else ('',):
                path = parent / (item + suffix)
                if path.is_file(): return str(path)
    return names[0]


def read_config(state):
    defaults = {'bridge': 'http://127.0.0.1:55010/mcp', 'autostart': True,
                **{name: find_agent(name) for name in PROVIDERS}}
    path = state / 'proxy.json'
    if path.exists():
        saved = json.loads(path.read_text())
        for key in defaults:
            if key in saved and type(saved[key]) is type(defaults[key]): defaults[key] = saved[key]
    return defaults


def arguments(state, config):
    args = codex_relay.parser().parse_args([])
    args.state_dir = state
    for key in ('bridge', *PROVIDERS): setattr(args, key, config[key])
    return args


async def relay_worker(state):
    task = asyncio.create_task(codex_relay.main(arguments(state, read_config(state))))
    async def stopping():
        while not (state / 'proxy.stop').exists(): await asyncio.sleep(.25)
    stop = asyncio.create_task(stopping())
    try:
        done, _ = await asyncio.wait([task, stop], return_when=asyncio.FIRST_COMPLETED)
        if task in done: await task
        else:
            print('Stopping AI proxy...', flush=True)
            task.cancel()
            try: await task
            except asyncio.CancelledError: pass
    finally:
        stop.cancel()
        await asyncio.gather(stop, return_exceptions=True)


def worker_entry():
    if len(sys.argv) < 2: return False
    if sys.argv[1] == '--mcp-worker':
        # Windowed builds still need real pipes for the CLI's stdio MCP client.
        if os.name == 'nt' and (sys.stdin is None or sys.stdout is None):
            import ctypes, msvcrt
            get = ctypes.windll.kernel32.GetStdHandle
            get.restype = ctypes.c_void_p
            if sys.stdin is None: sys.stdin = os.fdopen(msvcrt.open_osfhandle(get(-10), os.O_RDONLY), 'r', encoding='utf-8')
            if sys.stdout is None: sys.stdout = os.fdopen(msvcrt.open_osfhandle(get(-11), os.O_WRONLY), 'w', encoding='utf-8')
        else:
            if sys.stdin is None: sys.stdin = os.fdopen(0, 'r', encoding='utf-8')
            if sys.stdout is None: sys.stdout = os.fdopen(1, 'w', encoding='utf-8')
        import relay_mcp
        asyncio.run(relay_mcp.main(sys.argv[2])); return True
    if sys.argv[1] == '--relay-worker':
        state = Path(sys.argv[2])
        with (state / 'proxy.log').open('w', buffering=1, encoding='utf-8') as output:
            sys.stdout = sys.stderr = output
            try: asyncio.run(relay_worker(state))
            except Exception as error:
                print('Proxy stopped: ' + (codex_relay.connection_reason(error) if isinstance(error, BaseExceptionGroup) else str(error)), flush=True)
                return 1
        return True
    if sys.argv[1] == '--self-test':
        import cli_relay, relay_mcp
        from PySide6.QtWidgets import QApplication
        app = QApplication([])
        from PySide6.QtCore import QTimer
        QTimer.singleShot(50, app.quit)
        app.exec()
        return True
    return False


def gui():
    from PySide6.QtCore import QLockFile, QProcess, QProcessEnvironment, QTimer, Qt
    from PySide6.QtGui import QDesktopServices
    from PySide6.QtCore import QUrl
    from PySide6.QtWidgets import (QApplication, QCheckBox, QFileDialog, QFormLayout,
        QGridLayout, QGroupBox, QHBoxLayout, QLabel, QLineEdit, QMainWindow,
        QMessageBox, QPlainTextEdit, QPushButton, QVBoxLayout, QWidget)
    from cli_relay import clean_env

    class Window(QMainWindow):
        def __init__(self, state, fixture=False):
            super().__init__()
            self.state, self.fixture = state, fixture
            self.config = read_config(state) if not fixture else {'bridge': 'http://127.0.0.1:55010/mcp',
                'autostart': False, **{p: p for p in PROVIDERS}}
            self.setWindowTitle('Termic AI Proxy'); self.resize(860, 680)
            self.relay = QProcess(self); self.relay.finished.connect(self.finished)
            self.relay.errorOccurred.connect(self.worker_error)
            self.jobs = []; self.closing = False; self.offset = 0
            page = QWidget(); self.setCentralWidget(page); layout = QVBoxLayout(page)
            layout.setContentsMargins(24, 22, 24, 20); layout.setSpacing(14)
            title = QLabel('Termic AI Proxy'); title.setStyleSheet('font-size: 26px; font-weight: 600;')
            layout.addWidget(title)
            layout.addWidget(QLabel('Connect your Amiga to Codex, Claude and Cursor on this computer.'))
            connection = QGroupBox('Amiga connection'); form = QFormLayout(connection); form.setFieldGrowthPolicy(QFormLayout.AllNonFixedFieldsGrow)
            self.bridge = QLineEdit(self.config['bridge']); form.addRow('DevBench MCP URL', self.bridge)
            self.auto = QCheckBox('Start the proxy when this app opens'); self.auto.setChecked(self.config['autostart'])
            form.addRow(self.auto); layout.addWidget(connection)
            group = QGroupBox('AI agents'); grid = QGridLayout(group)
            self.fields = {}; self.labels = {}; self.controls = [self.bridge, self.auto]
            for row, provider in enumerate(PROVIDERS):
                label = QLabel(provider.title()); grid.addWidget(label, row, 0)
                field = QLineEdit(self.config[provider]); field.setPlaceholderText('Path to agent executable')
                self.fields[provider] = field; grid.addWidget(field, row, 1)
                browse = QPushButton('Browse'); browse.clicked.connect(lambda _, p=provider: self.browse(p)); grid.addWidget(browse, row, 2)
                check = QPushButton('Check'); check.clicked.connect(lambda _, p=provider: self.account(p, False)); grid.addWidget(check, row, 3)
                login = QPushButton('Sign in'); login.clicked.connect(lambda _, p=provider: self.account(p, True)); grid.addWidget(login, row, 4)
                status = QLabel('Not checked'); self.labels[provider] = status; grid.addWidget(status, row, 5)
                self.controls.extend([field, browse, login])
            layout.addWidget(group)
            hint = QLabel('On the Amiga: Settings > Agents / CLI / Scripts. Add an agent, set its Model, then Save.\nCredentials stay on this computer. One Amiga task runs at a time.')
            hint.setWordWrap(True); layout.addWidget(hint)
            actions = QHBoxLayout(); self.start_button = QPushButton('Start proxy'); self.stop_button = QPushButton('Stop')
            self.start_button.clicked.connect(self.start); self.stop_button.clicked.connect(self.stop); self.stop_button.setEnabled(False)
            actions.addWidget(self.start_button); actions.addWidget(self.stop_button)
            self.status = QLabel('Stopped'); actions.addWidget(self.status); actions.addStretch()
            help_button = QPushButton('Setup help'); help_button.clicked.connect(lambda: QDesktopServices.openUrl(QUrl('https://github.com/SkiltonUSA/Termic-Amiga/blob/main/docs/ai-proxy.md')))
            actions.addWidget(help_button); layout.addLayout(actions)
            self.log = QPlainTextEdit(); self.log.setReadOnly(True); self.log.setMaximumBlockCount(500)
            self.log.setPlaceholderText('Connection status and task progress appear here.'); layout.addWidget(self.log, 1)
            footer = QLabel('Termic-Amiga  |  ' + VERSION); footer.setStyleSheet('color: #7b8492;'); layout.addWidget(footer)
            self.timer = QTimer(self); self.timer.timeout.connect(self.poll); self.timer.start(300)
            if fixture:
                self.labels['codex'].setText('Login saved'); self.labels['claude'].setText('Login saved'); self.labels['cursor'].setText('Sign in needed')
                self.status.setText('Connected, waiting for Amiga'); self.start_button.setEnabled(False); self.stop_button.setEnabled(True)
                self.log.setPlainText('AI proxy ready for Codex, Claude and Cursor.\nChoose a profile and send a task from your Amiga.')
            elif self.auto.isChecked(): QTimer.singleShot(250, self.start)

        def save(self):
            from urllib.parse import urlparse
            url = self.bridge.text().strip(); parsed = urlparse(url)
            if parsed.scheme not in ('http', 'https') or not parsed.hostname or parsed.username or parsed.password:
                raise ValueError('Enter an HTTP(S) MCP URL without embedded credentials.')
            self.config = {'bridge': url, 'autostart': self.auto.isChecked(),
                **{p: self.fields[p].text().strip() for p in PROVIDERS}}
            if not all(self.config[p] for p in PROVIDERS): raise ValueError('Enter each agent command or executable path.')
            self.state.mkdir(parents=True, exist_ok=True, mode=0o700)
            path = self.state / 'proxy.json'; temporary = path.with_suffix('.tmp')
            temporary.write_text(json.dumps(self.config, indent=2)); temporary.replace(path)

        def browse(self, provider):
            path, _ = QFileDialog.getOpenFileName(self, 'Choose ' + provider.title() + ' executable')
            if path: self.fields[provider].setText(path)

        def start(self):
            if self.relay.state() != QProcess.NotRunning: return
            try: self.save()
            except Exception as error: QMessageBox.warning(self, 'Proxy settings', str(error)); return
            (self.state / 'proxy.stop').unlink(missing_ok=True)
            (self.state / 'proxy.log').write_text(''); self.offset = 0
            self.log.clear(); self.status.setText('Connecting...')
            self.start_button.setEnabled(False); self.stop_button.setEnabled(True)
            for control in self.controls: control.setEnabled(False)
            command = [sys.executable]
            if not getattr(sys, 'frozen', False): command += [str(Path(__file__).resolve())]
            self.relay.start(command[0], command[1:] + ['--relay-worker', str(self.state)])

        def stop(self):
            if self.relay.state() != QProcess.NotRunning:
                (self.state / 'proxy.stop').touch(); self.status.setText('Stopping...'); self.stop_button.setEnabled(False)

        def worker_error(self, error):
            self.log.appendPlainText('Could not launch proxy worker.')
            if error == QProcess.FailedToStart: self.finished()

        def finished(self, *args):
            self.poll(); self.status.setText('Stopped'); self.start_button.setEnabled(True); self.stop_button.setEnabled(False)
            for control in self.controls: control.setEnabled(True)
            if self.closing: self.close()

        def poll(self):
            path = self.state / 'proxy.log'
            if self.fixture or not path.exists(): return
            with path.open(encoding='utf-8', errors='replace') as stream:
                stream.seek(self.offset); text = stream.read(); self.offset = stream.tell()
            if text:
                self.log.appendPlainText(text.rstrip())
                if self.status.text() != 'Stopping...':
                    for line in text.splitlines():
                        if line.startswith('AI proxy ready'): self.status.setText('Connected, waiting for Amiga')
                        elif line.startswith('Bridge unavailable:'): self.status.setText('Reconnecting to Amiga bridge...')
                        elif line.startswith('Processing '): self.status.setText('Running task')
                        elif line in ('Task completed.', 'Task failed or stopped.'): self.status.setText('Connected, waiting for Amiga')


        def account(self, provider, login):
            if self.fixture: return
            try: self.save()
            except Exception as error: QMessageBox.warning(self, 'Proxy settings', str(error)); return
            process = QProcess(self); self.jobs.append(process)
            environment = QProcessEnvironment()
            env = clean_env(provider, self.state)
            # Codex uses the normal host sign-in; the worker links its auth locally.
            for key, value in env.items(): environment.insert(key, value)
            process.setProcessEnvironment(environment)
            command = self.config[provider]
            args = (['auth', 'login'] if provider == 'claude' else ['login']) if login else (
                ['auth', 'status'] if provider == 'claude' else ['status', '--format', 'json'] if provider == 'cursor' else ['login', 'status'])
            output = bytearray(); process.setProcessChannelMode(QProcess.MergedChannels)
            def ready():
                raw = bytes(process.readAllStandardOutput()); output.extend(raw)
                if login: self.log.appendPlainText(raw.decode(errors='replace').rstrip())
            def finished(code, *_):
                ready()
                ok = code == 0
                if not login and provider in ('claude', 'cursor'):
                    try:
                        data = json.loads(output)
                        ok = bool(data.get('loggedIn') and data.get('authMethod') == 'claude.ai') if provider == 'claude' else bool(data.get('isAuthenticated'))
                    except ValueError: ok = False
                self.labels[provider].setText('Login saved' if ok else 'Sign in needed')
                self.jobs.remove(process); process.deleteLater()
            process.readyReadStandardOutput.connect(ready); process.finished.connect(finished)
            process.errorOccurred.connect(lambda _: self.labels[provider].setText('CLI not found'))
            self.labels[provider].setText('Signing in...' if login else 'Checking...')
            try:
                prefix = executable_command(command)
                process.start(prefix[0], prefix[1:] + args)
            except ValueError as error:
                self.labels[provider].setText('Choose native CLI'); self.log.appendPlainText(str(error))
                self.jobs.remove(process); process.deleteLater()

        def closeEvent(self, event):
            if self.relay.state() != QProcess.NotRunning:
                self.closing = True; self.stop(); event.ignore(); return
            for job in self.jobs: job.kill(); job.waitForFinished(3000)
            if not self.fixture:
                try: self.save()
                except Exception: pass
            event.accept()

    app = QApplication(sys.argv); app.setApplicationName('Termic AI Proxy')
    state = Path(sys.argv[sys.argv.index('--ui-test')+1]) if '--ui-test' in sys.argv else codex_relay.default_state(); state.mkdir(parents=True, exist_ok=True, mode=0o700)
    fixture = '--screenshot-fixture' in sys.argv
    lock = QLockFile(str(state / 'proxy-ui.lock'))
    if not fixture and not lock.tryLock(0):
        QMessageBox.information(None, 'Termic AI Proxy', 'Termic AI Proxy is already open.'); return 0
    window = Window(state, fixture); window.show()
    if fixture:
        path = Path(sys.argv[sys.argv.index('--screenshot-fixture') + 1])
        def capture():
            window.grab().save(str(path)); app.quit()
        QTimer.singleShot(500, capture)
    if '--ui-test' in sys.argv:
        import time
        phase = [0]; deadline = [time.monotonic() + 40]; failed = []
        timer = QTimer(window)
        def exercise():
            if time.monotonic() > deadline[0]:
                failed.append('Desktop lifecycle timeout at phase ' + str(phase[0]))
                window.stop(); timer.stop(); app.exit(1); return
            connected = window.status.text().startswith('Connected')
            reconnecting = window.status.text().startswith('Reconnecting')
            if phase[0] == 0:
                window.start_button.click(); phase[0] = 1
            elif phase[0] == 1 and reconnecting:
                (state / 'test-bridge-down').unlink(); phase[0] = 2
            elif phase[0] == 2 and connected:
                (state / 'test-bridge-down').touch(); phase[0] = 3
            elif phase[0] == 3 and reconnecting:
                (state / 'test-bridge-down').unlink(); phase[0] = 4
            elif phase[0] in (4, 6) and connected:
                assert not window.start_button.isEnabled() and window.stop_button.isEnabled()
                if phase[0] == 4: window.stop_button.click(); phase[0] = 5
                else: phase[0] = 7; window.close()
            elif phase[0] == 5 and window.relay.state() == QProcess.NotRunning:
                assert window.start_button.isEnabled() and not window.stop_button.isEnabled()
                window.start_button.click(); phase[0] = 6
        timer.timeout.connect(exercise); timer.start(200)
        result = app.exec()
        if result or failed or phase[0] != 7: return 1
        (state / 'ui-passed.json').write_text(json.dumps({'start':True,'stop':True,'restart':True,'close':True,'startup_retry':True,'disconnect_retry':True}))
        return 0
    return app.exec()


if __name__ == '__main__':
    worker = worker_entry()
    if worker is False: sys.exit(gui())
    sys.exit(0 if worker is True else worker)
