"""Real input acceptance for the native MUI workspace, in an isolated install.

Invoked by accept_agents --editor-workflow-only, inside its restoration guard.
Only synthetic source/clipboard data is copied to the host. No model is called.
"""
import asyncio
import json
import re
import struct
import zlib

from codex_relay import decode_checksum

SOURCE = '#include <stdio.h>\nint main(void)\n{\n    puts("MUI_WORKFLOW_OK");\n    return 0;\n}\n'
BROKEN = '#include <stdio.h>\nint main(void) { this is not valid C; }\n'


async def stage(root, out, fixture, shell, push, call):
    sdk = 'SD032G:Dev/Termic/SDK'
    binpath = sdk + '/vbcc_bin_amigaos68k/bin/'
    target = sdk + '/vbcc_target_m68k-amigaos/targets/m68k-amigaos/'
    config = '\n'.join([
        '-cc=' + binpath + 'vbccm68k -quiet %s -o= %s %s -O=%ld -I' + target + 'include',
        '-as=' + binpath + 'vasmm68k_mot -quiet -Fhunk -nowarn=62 %s -o %s',
        '-rm=delete quiet %s',
        '-ld=' + binpath + 'vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -mrel ' + target + 'lib/startup.o %s %s -L' + target + 'lib -lvc -o %s',
        '-ldnodb=-s -Rshort', '-ul=-l%s', '-cf=-F%s', '-ml=500', '-hunkdebug', '-amiga-softfloat', ''])
    for name, text in [
        ('native-build.config', config), ('Setup', 'Stack 262144\n'),
        ('Config/last-workspace', fixture + '/Projects/fixture'),
        ('Projects/fixture/build-command.txt', binpath + 'vc +' + fixture + '/native-build.config -cpu=68020 -O1 main.c -o Program.new -lamiga\n'),
        ('Projects/fixture/main.c', 'int main(void) { return 0; }\n'),
    ]:
        local = out / name.replace('/', '-')
        local.write_text(text)
        await push(local, fixture + '/' + name)
    await push(root / 'amiga/native/BuildProject', fixture + '/BuildProject')
    await push(root / '.context/inspect-ui', fixture + '/ClipboardProbe')
    await shell('Protect ' + fixture + '/ClipboardProbe +e')


async def exercise(root, out, fixture, shell, push, call, state, click, capture):
    project = fixture + '/Projects/fixture'
    version = re.search(r'#define TERMIC_VERSION "([^"]+)"', (root / 'amiga/native/version.h').read_text())[1]
    title = 'Termic Amiga ' + version
    report = out / 'editor-workflow-passed.json'
    report.unlink(missing_ok=True)

    async def key(code, mods=()):
        await state(front=True)
        try:
            for mod in mods:
                await call('amiga_input_key', {'rawkey': mod, 'direction': 'down'})
            await call('amiga_input_key', {'rawkey': code, 'direction': 'down'})
        finally:
            await call('amiga_input_key', {'rawkey': code, 'direction': 'up'})
            for mod in reversed(mods):
                await call('amiga_input_key', {'rawkey': mod, 'direction': 'up'})
        await asyncio.sleep(.3)

    async def pull(name, base=project):
        remote = base + '/' + name
        local = out / ('read-' + name)
        await call('amiga_pull_file', {'amiga_path': remote, 'local_path': str(local)})
        data = local.read_bytes()
        assert decode_checksum(await call('amiga_checksum', {'path': remote}), remote) == (zlib.crc32(data), len(data))
        return data

    async def clipboard(text):
        data = text.encode('ascii')
        chunk = b'CHRS' + struct.pack('>I', len(data)) + data + b'\0' * (len(data) % 2)
        local = out / 'input.iff'
        local.write_bytes(b'FORM' + struct.pack('>I', len(chunk) + 4) + b'FTXT' + chunk)
        await push(local, fixture + '/input.iff')
        await shell(fixture + '/ClipboardProbe restore-clip ' + fixture + '/input.iff')

    async def source(text, compact_save=False):
        await key('51')  # F2 selects and focuses the source editor.
        await key('20', ('67',))  # Edit > Select All.
        await clipboard(text)
        await key('34', ('67',))  # Edit > Paste.
        if compact_save:
            p = await state(front=True)
            # Current fixed-size MUI icon row: Build, Run, Save, Stop.
            await click(p['x'] + p['width'] - 56, p['y'] + p['border'] + 17)
        else:
            await key('57')  # F8 Save, while TextEditor owns focus.
        assert (await pull('main.c')).decode() == text, 'Editor did not save exact source bytes'

    async def job(action, expected, code=0):
        await shell('If EXISTS ' + project + '/.result\n Delete ' + project + '/.result\nEndIf')
        await action()
        for _ in range(90):
            flags = await shell('If EXISTS ' + project + '/.result\n Echo RESULT\nEndIf\n'
                                'If EXISTS ' + project + '/.busy\n Echo BUSY\nEndIf\n'
                                'If EXISTS ' + project + '/.launch\n Echo LAUNCH\nEndIf')
            if 'RESULT' in flags and 'BUSY' not in flags and 'LAUNCH' not in flags:
                log = (await pull('job.log')).decode()
                assert expected in log and 'Finished with exit code ' + str(code) in log, log
                print('Verified ' + expected, flush=True)
                return log
            await asyncio.sleep(1)
        raise AssertionError('GUI action did not complete: ' + expected)

    async def toolbar(offset):
        p = await state(front=True)
        assert p['width'] >= 800 and p['height'] >= 500 and p['border'] > 0
        # Buttons are 32x30; these offsets match the native reference header.
        await click(p['x'] + p['width'] - offset, p['y'] + p['border'] + 17)

    async def copied_output():
        await clipboard('COPY_NOT_PERFORMED')
        await key('20', ('67',))
        await key('33', ('67',))
        await shell(fixture + '/ClipboardProbe save-clip ' + fixture + '/output.iff')
        data = await pull('output.iff', fixture)
        assert data[:4] == b'FORM' and data[8:12] == b'FTXT'
        pos, chunks = 12, []
        while pos + 8 <= len(data):
            size = struct.unpack('>I', data[pos + 4:pos + 8])[0]
            if data[pos:pos + 4] == b'CHRS':
                chunks.append(data[pos + 8:pos + 8 + size])
            pos += 8 + size + size % 2
        return b''.join(chunks).decode('latin1')

    async def resize(width, height):
        p = await state(front=True)
        await call('amiga_window_move', {'window': format(p['address'], '08x'), 'x': 0, 'y': 0})
        for _ in range(4):
            await call('amiga_window_resize', {'window': format(p['address'], '08x'), 'width': width, 'height': height})
            await asyncio.sleep(1)
            p = await state(front=True)
            if (p['width'], p['height']) == (width, height):return p
        raise AssertionError('Live resize did not reach requested dimensions: ' + str(p))

    async def return_to_editor():
        # The cached tab is already Editor. The shortcut must reveal it even
        # when setting the same ActivePage would not generate a notification.
        await key('51')
        for code in ('52', '51'):  # Shift F3 CLI; Shift F2 Files.
            await key(code, ('60',))
            await key('51')
            p = await state(front=True)
            await click(p['x'] + 60, p['y'] + 140)
            copied = await copied_output()
            if copied != SOURCE:
                await capture(title, 'navigation-failed.png')
                raise AssertionError('F2 did not reveal the cached Editor after a sidebar pane')
            await key('4e')
        print('Verified compact sidebar-to-editor shortcuts', flush=True)

    await shell(fixture + '/ClipboardProbe save-clip ' + fixture + '/clipboard-backup')
    try:
        await resize(1024, 800)
        await source(SOURCE)
        await resize(340, 280)
        await return_to_editor()
        await resize(1024, 800)
        await job(lambda: toolbar(162), 'Native Amiga build succeeded.')
        program = await pull('Program')
        await job(lambda: toolbar(128), 'MUI_WORKFLOW_OK')
        await capture(title, 'build-run-1024x800.png')

        await source(BROKEN)
        await job(lambda: key('55'), 'Finished with exit code 20', 20)  # F6.
        assert await pull('Program') == program, 'Failed build replaced the working program'
        flags = await shell('If EXISTS ' + project + '/.build-ok\n Echo BUILD_OK\nEndIf')
        assert 'BUILD_OK' not in flags, 'Failed build retained the success marker'
        await capture(title, 'build-failure-1024x800.png')

        await resize(340, 280)
        await source(SOURCE, compact_save=True)
        await job(lambda: key('55'), 'Native Amiga build succeeded.')
        await job(lambda: key('56'), 'MUI_WORKFLOW_OK')  # F7.
        await key('52', ('60',))  # Shift F3: CLI command, compact CLI pane.
        await clipboard('Echo MUI_CLI_OK')
        await key('41', ('60',))
        await key('46', ('60',))
        await key('34', ('67',))
        await job(lambda: key('44'), 'MUI_CLI_OK')  # Return executes the CLI.
        p = await state(front=True)
        await click(p['x'] + 60, p['y'] + 100)
        assert 'MUI_CLI_OK' in await copied_output(), 'CLI output was not displayed/selectable'
        await key('4e')  # Clear selection before the screenshot.
        await capture(title, 'cli-340x280.png')
        await key('51')
        await capture(title, 'editor-340x280.png')
        assert (await pull('main.c')).decode() == SOURCE
        report.write_text(json.dumps({'fixture': fixture, 'toolbar_build_run': True,
            'save_exact_bytes': True, 'failed_build_preserves_program': True,
            'compact_save_build_run': True, 'cli_return_and_copy': True,
            'sidebar_to_cached_editor': True,
            'native_window_sizes': [[1024, 800], [340, 280]]}, indent=2))
        print('Native MUI editor workflow passed', flush=True)
    finally:
        await shell(fixture + '/ClipboardProbe restore-clip ' + fixture + '/clipboard-backup\nDelete ' + fixture + '/clipboard-backup')
