"""Opt-in authenticated host-agent relay acceptance inside accept_agents' fixture.
Uses synthetic prompts/project data and restores the system clipboard on Amiga.
"""
import asyncio
import json
import struct
import subprocess
from pathlib import Path
import codex_relay

async def exercise(root, out, fixture, shell, push, call, state, click, capture, menu, args, provider="codex"):
    kind = {"codex":5, "claude":6, "cursor":7}[provider]
    profile_id = "fixture-" + provider
    session_file = ("session-" if kind==5 else "session-" + str(kind) + "-") + profile_id + ".txt"
    project = fixture + '/Projects/fixture'
    title = 'Termic: Agents, CLI and Scripts'
    async def key(code, mods=()):
        try:
            for mod in mods: await call('amiga_input_key', {'rawkey': mod, 'direction': 'down'})
            await call('amiga_input_key', {'rawkey': code, 'direction': 'down'})
        finally:
            await call('amiga_input_key', {'rawkey': code, 'direction': 'up'})
            for mod in reversed(mods): await call('amiga_input_key', {'rawkey': mod, 'direction': 'up'})
    async def pull(name):
        local = out / ('relay-' + name.replace('/', '-'))
        await call('amiga_pull_file', {'amiga_path': project + '/' + name, 'local_path': str(local)})
        return local.read_bytes()
    async def wait_done():
        for _ in range(30):
            status = await shell('If EXISTS ' + project + '/.busy\n Echo BUSY\nEndIf')
            if 'BUSY' not in status: return
            await asyncio.sleep(1)
        raise AssertionError('Relay worker did not stop')
    # Existing configuration in this isolated installation contains only fixtures.
    registry_path = out / 'relay-registry.json'
    await call('amiga_pull_file', {'amiga_path': 'ENV:Termic/agents.json', 'local_path': str(registry_path)})
    registry = json.loads(registry_path.read_text())
    profile = dict(registry['profiles'][0])
    profile.update(id=profile_id, name=provider.title()+' on Mac', kind=kind, model='sonnet' if provider=='claude' else '', steps=12, readonly=0, resume=1)
    registry['profiles'] = [p for p in registry['profiles'] if p['id'] != profile_id]
    registry['profiles'].append(profile)
    registry_path.write_text(json.dumps(registry))
    await push(registry_path, 'ENV:Termic/agents.json')
    selected = out / 'relay-profile'; selected.write_text(profile_id)
    await push(selected, project + '/agent-profile')
    await shell('If EXISTS ' + project + '/' + session_file + '\n Delete ' + project + '/' + session_file + '\nEndIf')
    await menu('12'); a = await state(title)
    # Opening reads the updated registry; select the last row, then inspect it.
    await click(a['x'] + 100, a['y'] + 140 + (len(registry['profiles']) - 1) * 15, title)
    await capture(title, provider+'-live-profile.png')
    await click(a['x'] + 68, a['y'] + a['height'] - 24, title) # Use and refresh launcher
    await push(out / 'build-command.txt', project + '/build-command.txt')
    await push(root / '.context/inspect-ui', fixture + '/ClipboardProbe')
    await shell('Protect ' + fixture + '/ClipboardProbe +e\n' + fixture + '/ClipboardProbe save-clip ' + fixture + '/clipboard-backup')
    async def prompt(text):
        raw = text.encode('ascii')
        chunk = b'CHRS' + struct.pack('>I', len(raw)) + raw + (b'\0' if len(raw) % 2 else b'')
        iff = b'FORM' + struct.pack('>I', len(chunk) + 4) + b'FTXT' + chunk
        local = out / 'relay-prompt.iff'; local.write_bytes(iff)
        await push(local, fixture + '/prompt.iff')
        await shell(fixture + '/ClipboardProbe restore-clip ' + fixture + '/prompt.iff')
        await state(front=True)
        await key('50') # F1 prompt
        await key('20', ('67',)); await key('41'); await key('34', ('67',)) # select all/delete/paste
        await key('42') # Tab launches selected host-agent profile
    relay_args = codex_relay.parser().parse_args([])
    try:
        async with codex_relay.Bridge(args.bridge if hasattr(args, 'bridge') else 'http://127.0.0.1:55110/mcp', fixture + '/Relay') as bridge:
            async def run(text):
                await prompt(text)
                for _ in range(30):
                    raw = await bridge.read('request.json')
                    if raw: break
                    await asyncio.sleep(1)
                else: raise AssertionError('GUI did not start the host relay worker')
                job = codex_relay.validate_job(json.loads(raw))
                assert job.get('provider','codex') == provider
                ok = await codex_relay.dispatch_job(bridge, job, relay_args)
                await wait_done()
                log = (await pull('job.log')).decode()
                transcript = (await pull('transcript.txt')).decode()
                assert ok and 'Finished with exit code 0' in log, (log, transcript)
                return job, transcript
            await shell('If EXISTS ' + project + '/Program\n Delete ' + project + '/Program\nEndIf')
            first, transcript = await run('Read main.c using read_source. Replace it using write_source with a tiny C89 console program that prints AMIGA_RELAY_OK and returns zero. Compile it with build and report the actual result. Do not run it.')
            first_log = (await pull('job.log')).decode()
            (out / (provider+'-live-build.log')).write_text(first_log)
            for tool_name in ('read_source','write_source','build'):
                assert 'Mac agent tool: ' + tool_name in first_log or 'Codex tool: ' + tool_name in first_log
            assert 'AMIGA_RELAY_OK' in (await pull('main.c')).decode()
            assert (await pull('Program'))[:4] == b'\0\0\x03\xf3'
            session = (await pull(session_file)).decode()
            assert not first['thread'] and session
            first_transcript_length = len(transcript)
            second, transcript = await run('From our previous turn, what marker did the program print? Do not use any tools. Reply briefly.')
            assert second['thread'] == session and 'AMIGA_RELAY_OK' in transcript[first_transcript_length:]
            assert (await pull(session_file)).decode() == session
            await capture('', provider+'-live-desktop.png')
            p = await state()
            if (out / (provider+'-live-desktop.png')).exists():
                subprocess.run(['sips', '--cropToHeightWidth', str(p['height']), str(p['width']),
                    '--cropOffset', str(p['y']), str(p['x']), str(out / (provider+'-live-desktop.png')),
                    '--out', str(out / (provider+'-live-relay.png'))], check=True, capture_output=True)
            # Exercise native Stop while waiting; no model inference is started.
            await prompt('Cancellation fixture. Do not perform any work.')
            for _ in range(20):
                raw = await bridge.read('request.json')
                if raw: break
                await asyncio.sleep(.5)
            assert raw
            await key('59', ('60',)) # Shift F10: Stop
            await wait_done()
            assert 'cancelled' in (await pull('job.log')).decode()
            assert await bridge.read('request.json') is None
            # Remove only this synthetic cancellation's leftover marker.
            stopped = json.loads(raw)['id']
            await bridge.remove(stopped + '.stopped')
            # Native worker enforces read-only even if a client sends write/build.
            profile['readonly'] = 1
            registry_path.write_text(json.dumps(registry))
            await push(registry_path, 'ENV:Termic/agents.json')
            before = await pull('main.c')
            await prompt('Read-only boundary fixture; no model inference required.')
            for _ in range(20):
                raw = await bridge.read('request.json')
                if raw: break
                await asyncio.sleep(.5)
            assert raw
            readonly = codex_relay.validate_job(json.loads(raw))
            assert readonly['readonly']
            mailbox = codex_relay.Mailbox(bridge, readonly)
            response = await mailbox.send('tool', name='write_source', arguments={'source': 'invalid fixture'})
            assert 'Read-only profile' in response
            response = await mailbox.send('tool', name='build', arguments={})
            assert 'Read-only profile' in response
            response = await mailbox.send('tool', name='read_file', arguments={'path': '../../Config/api-key'})
            assert 'Invalid path' in response
            await mailbox.send('done', status='completed')
            await wait_done()
            assert await pull('main.c') == before
            await bridge.remove(readonly['id'] + '.out')
        (out / ('codex-passed.json' if provider=='codex' else provider+'-live-passed.json')).write_text(json.dumps({'authenticated': True, 'gui_send': True,
            'native_read_write_build': True, 'resume': True, 'stop_waiting': True, 'native_readonly': True, 'traversal_rejected': True}, indent=2))
    finally:
        await shell(fixture + '/ClipboardProbe restore-clip ' + fixture + '/clipboard-backup\nDelete ' + fixture + '/clipboard-backup')
