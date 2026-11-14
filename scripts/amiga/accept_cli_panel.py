"""Opt-in Files/CLI layout checks, inside accept_agents' restoration guard."""
import asyncio
import json
import struct
import subprocess


async def exercise(root, out, fixture, shell, push, call, state, click, capture):
    project = fixture + '/Projects/fixture'
    report = out / 'cli-panel-passed.json'
    report.unlink(missing_ok=True)
    await push(root / '.context/inspect-ui', fixture + '/ClipboardProbe')
    await shell('Protect ' + fixture + '/ClipboardProbe +e\n' + fixture +
                '/ClipboardProbe save-clip ' + fixture + '/clipboard-backup')
    failing = out / 'cli-panel-fail'
    failing.write_text('FailAt 21\nQuit 20\n')
    await push(failing, fixture + '/cli-fail')

    async def key(code, mods=()):
        try:
            for mod in mods:
                await call('amiga_input_key', {'rawkey': mod, 'direction': 'down'})
            await call('amiga_input_key', {'rawkey': code, 'direction': 'down'})
        finally:
            await call('amiga_input_key', {'rawkey': code, 'direction': 'up'})
            for mod in reversed(mods):
                await call('amiga_input_key', {'rawkey': mod, 'direction': 'up'})

    async def pull(name):
        local = out / ('cli-panel-' + name.replace('/', '-'))
        await call('amiga_pull_file', {'amiga_path': project + '/' + name, 'local_path': str(local)})
        return local.read_text()

    async def paste(text):
        raw = text.encode('ascii')
        chunk = b'CHRS' + struct.pack('>I', len(raw)) + raw + (b'\0' if len(raw) % 2 else b'')
        local = out / 'cli-panel-input.iff'
        local.write_bytes(b'FORM' + struct.pack('>I', len(chunk) + 4) + b'FTXT' + chunk)
        await push(local, fixture + '/cli-input.iff')
        await shell(fixture + '/ClipboardProbe restore-clip ' + fixture + '/cli-input.iff')
        # Intuition string fields use Shift Backspace/Delete to clear each
        # side of the cursor; TextEditor's Select All is a different control.
        await key('41', ('60',))
        await key('46', ('60',))
        await key('34', ('67',))  # Right Amiga V

    async def copy_at(x, y):
        await click(x, y)
        await key('20', ('67',))  # App's Edit > Select All menu shortcut
        await key('33', ('67',))  # Right Amiga C
        await shell(fixture + '/ClipboardProbe save-clip ' + fixture + '/cli-copy.iff')
        local = out / 'cli-panel-copy.iff'
        await call('amiga_pull_file', {'amiga_path': fixture + '/cli-copy.iff', 'local_path': str(local)})
        raw = local.read_bytes()
        assert raw[:4] == b'FORM' and raw[8:12] == b'FTXT'
        pos = 12
        chunks = []
        while pos + 8 <= len(raw):
            size = struct.unpack('>I', raw[pos + 4:pos + 8])[0]
            if raw[pos:pos + 4] == b'CHRS':
                chunks.append(raw[pos + 8:pos + 8 + size])
            pos += 8 + size + size % 2
        if chunks:
            return b''.join(chunks).decode('latin1')
        raise AssertionError('Copied text missing from clipboard')

    async def run(command, expected_code):
        status = await shell('If EXISTS ' + project + '/.busy\n Echo BUSY\nEndIf')
        assert 'BUSY' not in status
        await shell('If EXISTS ' + project + '/.result\n Delete ' + project + '/.result\nEndIf')
        await key('52', ('60',))  # Shift F3: persistent command field
        await paste(command)
        await key('44')  # Return executes, keeping the centre tab
        for _ in range(30):
            t = await shell('If EXISTS ' + project + '/.busy\n Echo BUSY\nEndIf\n'
                            'If EXISTS ' + project + '/.result\n Echo RESULT\nEndIf')
            if 'RESULT' in t and 'BUSY' not in t:
                break
            await asyncio.sleep(1)
        else:
            raise AssertionError('CLI job did not finish')
        await asyncio.sleep(1)
        log = await pull('job.log')
        assert 'Finished with exit code ' + str(expected_code) in log, log
        return log

    async def capture_window(name):
        await capture('', name + '-desktop.png')
        p = await state()
        image = out / (name + '-desktop.png')
        if image.exists():
            subprocess.run(['sips', '--cropToHeightWidth', str(p['height']), str(p['width']),
                            '--cropOffset', str(p['y']), str(p['x']), str(image),
                            '--out', str(out / (name + '.png'))], check=True, capture_output=True)

    try:
        await key('51')  # Editor stays visible while commands run
        original_source = await pull('main.c')
        log = await run('Echo CLI_PANEL_OK', 0)
        assert 'CLI_PANEL_OK' in log
        p = await state()
        # The centre remains the source editor, while the lower right shows output.
        centre = await copy_at(p['x'] + p['width'] // 2, p['y'] + p['height'] // 3)
        assert centre.strip() == original_source.strip(), 'Centre editor did not copy the expected source after the CLI job'
        await key('4e')  # Clear selection
        await key('4c', ('63',))  # Ctrl Up: start of source
        await key('32')  # Insert one x, not zero or two
        await key('57')  # F8 Save
        assert await pull('main.c') == 'x' + original_source, 'Native key input was lost or duplicated'
        await key('31', ('67',))  # Right Amiga Z: undo that one edit
        await key('57')
        assert await pull('main.c') == original_source, 'Undo did not restore source'
        side = await copy_at(p['x'] + p['width'] * 9 // 10, p['y'] + p['height'] // 2 + 100)
        assert side.strip() == log.strip(), 'Lower Files sidebar does not show current CLI output'
        await key('52', ('60',))
        await capture_window('cli-panel-windowed')
        await run('Execute ' + fixture + '/cli-fail', 20)
        p = await state()
        side = await copy_at(p['x'] + p['width'] * 9 // 10, p['y'] + p['height'] // 2 + 100)
        assert 'Finished with exit code 20' in side
        # Restore useful output, then exercise actual window resizing.
        await run('Echo CLI_PANEL_OK', 0)
        await call('amiga_window_resize', {'window': format(p['address'], '08x'), 'width': 1920, 'height': 789})
        await call('amiga_window_move', {'window': format(p['address'], '08x'), 'x': 0, 'y': 11})
        await state(front=True)
        p = await state()
        assert (p['width'], p['height']) == (1920, 789)
        side = await copy_at(p['x'] + p['width'] * 9 // 10, p['y'] + p['height'] // 2 + 100)
        assert 'CLI_PANEL_OK' in side and 'Finished with exit code 0' in side
        await key('51')  # Leave the source editor active for the final capture.
        await capture_window('cli-panel-wide')
        report.write_text(json.dumps({'fixture': fixture, 'cli_return': True, 'native_failure': True,
            'editor_preserved': True, 'sidebar_output': True, 'windowed_and_wide': True,
            'text_input_once': True, 'undo': True}, indent=2))
    finally:
        await shell(fixture + '/ClipboardProbe restore-clip ' + fixture + '/clipboard-backup\nDelete ' + fixture + '/clipboard-backup')
