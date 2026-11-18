"""Written Amiga acceptance for Claude/Cursor profiles, with synthetic MCP clients.
No cloud credentials or paid inference required. Runs inside accept_agents fixture.
"""
import asyncio
import json
import struct
from pathlib import Path
import codex_relay
from cli_relay import ToolServer

async def exercise(root, out, fixture, shell, push, call, state, click, capture, menu, args):
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
        local = out / ('providers-' + name.replace('/', '-'))
        await call('amiga_pull_file', {'amiga_path': project + '/' + name, 'local_path': str(local)})
        return local.read_bytes()
    async def wait_done():
        for _ in range(30):
            if 'BUSY' not in await shell('If EXISTS ' + project + '/.busy\n Echo BUSY\nEndIf'): return
            await asyncio.sleep(.5)
        raise AssertionError('Provider worker did not stop')
    registry_path = out / 'providers-registry.json'
    await call('amiga_pull_file', {'amiga_path': 'ENV:Termic/agents.json', 'local_path': str(registry_path)})
    registry = json.loads(registry_path.read_text())
    registry['profiles'] = [p for p in registry['profiles'] if not p['id'].startswith('fixture-')]
    await push(out / 'build-command.txt', project + '/build-command.txt')
    await push(root / '.context/inspect-ui', fixture + '/ClipboardProbe')
    await shell('Protect ' + fixture + '/ClipboardProbe +e\n' + fixture + '/ClipboardProbe save-clip ' + fixture + '/clipboard-backup')
    async def prompt(text):
        raw = text.encode('ascii'); chunk = b'CHRS' + struct.pack('>I', len(raw)) + raw + (b'\0' if len(raw) % 2 else b'')
        local = out / 'providers-prompt.iff'; local.write_bytes(b'FORM' + struct.pack('>I', len(chunk)+4) + b'FTXT' + chunk)
        await push(local, fixture + '/prompt.iff')
        await shell(fixture + '/ClipboardProbe restore-clip ' + fixture + '/prompt.iff')
        await state(front=True); await key('50')
        await key('20', ('67',)); await key('41'); await key('34', ('67',)); await key('42')
    results = {}
    try:
        async with codex_relay.Bridge('http://127.0.0.1:55110/mcp', fixture + '/Relay') as bridge:
            async def request(text):
                await prompt(text)
                for _ in range(30):
                    raw = await bridge.read('request.json')
                    if raw: return codex_relay.validate_job(json.loads(raw))
                    await asyncio.sleep(.5)
                raise AssertionError('Provider worker did not publish request')
            for provider, kind, model in [('claude', 6, 'sonnet'), ('cursor', 7, 'auto')]:
                profile = dict(registry['profiles'][0])
                profile.update(id='fixture-' + provider, name=provider.title() + ' on Mac', kind=kind,
                               model=model, steps=12, readonly=0, resume=1)
                registry['profiles'].append(profile)
                registry_path.write_text(json.dumps(registry)); await push(registry_path, 'ENV:Termic/agents.json')
                selected = out / 'providers-selected'; selected.write_text(profile['id']); await push(selected, project + '/agent-profile')
                await shell('If EXISTS ' + project + f'/session-{kind}-fixture-{provider}.txt\n Delete ' + project + f'/session-{kind}-fixture-{provider}.txt\nEndIf')
                await menu('12'); a = await state(title)
                await click(a['x']+100, a['y']+140+(len(registry['profiles'])-1)*15, title)
                await capture(title, provider + '-profile.png')
                await click(a['x']+68, a['y']+a['height']-24, title)
                job = await request('Synthetic ' + provider + ' profile: read, edit and compile fixture.')
                assert job['provider'] == provider and job['protocol'] == 2 and job['model'] == model and not job['thread'], job
                mailbox = codex_relay.Mailbox(bridge, job); tools = ToolServer(mailbox, job)
                async def tool(name, **arguments):
                    return await tools.dispatch({'token': tools.token, 'op':'call', 'name': name, 'arguments': arguments})
                assert 'main.c' in await tool('list_sources')
                await tool('read_source')
                marker = 'TERMIC_' + provider.upper() + '_FIXTURE'
                source = '#include <stdio.h>\nint main(void) { puts("' + marker + '"); return 0; }\n'
                await tool('write_source', source=source)
                await tool('build')
                assert (await pull('main.c')).decode() == source
                assert (await pull('Program'))[:4] == b'\0\0\x03\xf3'
                thread = 'abcdef-0006' if kind == 6 else 'abcdef-0007'
                await mailbox.send('session', thread=thread)
                await mailbox.text('Synthetic MCP acceptance complete. No cloud inference.\n')
                await mailbox.send('done', status='completed'); await wait_done()
                await bridge.remove(job['id']+'.out')
                second = await request('Synthetic conversation resume fixture.')
                assert second['thread'] == thread and second['provider'] == provider
                box = codex_relay.Mailbox(bridge, second); await box.send('done', status='completed'); await wait_done()
                await bridge.remove(second['id']+'.out')
                # Native permission boundary, independent of what an AI requests.
                profile['readonly'] = 1
                registry_path.write_text(json.dumps(registry)); await push(registry_path, 'ENV:Termic/agents.json')
                readonly = await request('Synthetic read-only boundary fixture.')
                assert readonly['readonly']
                box = codex_relay.Mailbox(bridge, readonly)
                assert 'Read-only profile' in await box.send('tool', name='write_source', arguments={'source':'invalid'})
                assert 'Read-only profile' in await box.send('tool', name='build', arguments={})
                await box.send('done', status='completed'); await wait_done()
                await bridge.remove(readonly['id']+'.out')
                assert (await pull('main.c')).decode() == source
                results[provider] = {'gui_send':True,'model':True,'native_build':True,'resume':True,'readonly':True,'cloud_authenticated':False}
            stopped = await request('Synthetic Stop fixture.')
            await key('59', ('60',)); await wait_done()
            assert 'cancelled' in (await pull('job.log')).decode()
            await bridge.remove(stopped['id']+'.stopped')
        (out/'providers-passed.json').write_text(json.dumps({'providers':results,'stop_waiting':True}, indent=2))
    finally:
        await shell(fixture + '/ClipboardProbe restore-clip ' + fixture + '/clipboard-backup\nDelete ' + fixture + '/clipboard-backup')
