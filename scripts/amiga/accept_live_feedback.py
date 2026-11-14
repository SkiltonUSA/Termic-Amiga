"""Written live transcript/selection/tool feedback checks on an isolated Amiga.
Uses synthetic streamed text, not cloud inference or user project data.
"""
import asyncio
import json
import struct
import time
import subprocess
from codex_relay import Bridge, Mailbox, LiveText, validate_job
from cli_relay import ToolServer


async def exercise(root, out, fixture, shell, push, call, state, click, capture, menu):
    project = fixture + '/Projects/fixture'
    report = out / 'live-feedback-passed.json'
    report.unlink(missing_ok=True)
    tag = str(int(time.time()))
    first_marker = 'LIVE_FIRST_' + tag + ':'
    second_marker = 'LIVE_SECOND_' + tag + ':'
    ready_marker = 'LIVE_READY_' + tag + ':'
    async def write(name, text, target):
        local = out / name
        local.write_text(text)
        await push(local, target)
    async def key(code, mods=()):
        try:
            for mod in mods: await call('amiga_input_key', {'rawkey':mod,'direction':'down'})
            await call('amiga_input_key', {'rawkey':code,'direction':'down'})
        finally:
            await call('amiga_input_key', {'rawkey':code,'direction':'up'})
            for mod in reversed(mods): await call('amiga_input_key', {'rawkey':mod,'direction':'up'})
    async def pull(remote, name):
        local = out / name
        await call('amiga_pull_file', {'amiga_path':remote,'local_path':str(local)})
        return local.read_bytes()
    def profile(ident, kind):
        p = dict.fromkeys(('model','command','args','yolo','resume_args','env'), '')
        p.update(id=ident, name=ident, kind=kind, visible=1, readonly=0, resume=1, steps=8, tokens=4096, usage=1)
        return p
    registry = {'version':1, 'next_id':1, 'profiles':[profile('openai',0),profile('shell',2),profile('live-fixture',5)]}
    await write('live-agents.json',json.dumps(registry),'ENV:Termic/agents.json')
    await write('live-selected','live-fixture',project+'/agent-profile')
    await menu('12')
    p = await state('Termic: Agents, CLI and Scripts')
    await click(p['x']+100,p['y']+170,'Termic: Agents, CLI and Scripts')
    await click(p['x']+68,p['y']+p['height']-24,'Termic: Agents, CLI and Scripts')
    await push(root/'.context/inspect-ui',fixture+'/ClipboardProbe')
    await shell('Protect '+fixture+'/ClipboardProbe +e\n'+fixture+'/ClipboardProbe save-clip '+fixture+'/clipboard-backup')
    async def clipboard_text():
        await shell(fixture+'/ClipboardProbe save-clip '+fixture+'/live-copy.iff')
        raw = await pull(fixture+'/live-copy.iff','live-copy.iff')
        assert raw[:4] == b'FORM' and raw[8:12] == b'FTXT'
        pos=12; text=[]
        while pos+8 <= len(raw):
            size=struct.unpack('>I',raw[pos+4:pos+8])[0]
            if raw[pos:pos+4]==b'CHRS':text.append(raw[pos+8:pos+8+size])
            pos+=8+size+size%2
        return b''.join(text).decode('latin1')
    async def copy_transcript():
        p=await state()
        await click(p['x']+p['width']//2,p['y']+160)
        await key('20',('67',)); await key('33',('67',))
        return await clipboard_text()
    async def busy():
        return 'BUSY' in await shell('If EXISTS '+project+'/.busy\n Echo BUSY\nEndIf')
    try:
        await key('50'); await key('20',('67',)); await key('41')
        # Synthetic instruction; the test acts as the host, with no inference.
        await key('10'); await key('42')
        async with Bridge('http://127.0.0.1:55010/mcp',fixture+'/Relay') as bridge:
            for _ in range(30):
                raw=await bridge.read('request.json')
                if raw:break
                await asyncio.sleep(.5)
            else:raise AssertionError('Native relay did not start')
            job=validate_job(json.loads(raw)); box=Mailbox(bridge,job)
            live=LiveText(box, interval=.05)
            tools=ToolServer(box,job,live)
            try:
                live.add(first_marker + ' The agent is responding before completion.')
                await live.finish()
                assert await busy()
                first=await copy_transcript()
                assert '[Agent]' in first and '[Log]' in first and 'Waiting for the host agent.' in first, first
                assert first_marker in first and '[Mac relay completed]' not in first[first.index(first_marker):], first
                # Hold the selection while another chunk arrives. Copy stays stable.
                live.add(' ' + second_marker + ' More text arrived while the first part was selected.\n')
                await live.finish()
                await key('33',('67',))
                assert await clipboard_text()==first, 'Streaming replaced a clipboard selection'
                await key('4e') # release selection; next GUI poll catches up
                await asyncio.sleep(1)
                second=await copy_transcript()
                assert second.count(first_marker)==1 and second.count(second_marker)==1,second
                assert 'before completion. ' + second_marker in second, 'Chunk boundary inserted a newline: ' + second
                await key('4e')
                result=await tools.dispatch({'token':tools.token,'op':'call','name':'list_sources','arguments':{}})
                assert 'main.c' in result
                live.add(ready_marker + ' Tool returned; the worker is still running.\n')
                await live.finish()
                assert await busy()
                text=await copy_transcript()
                assert '[Amiga: list_sources]' in text and '[Amiga tool returned: list_sources]' in text
                assert 'Mac agent tool: list_sources' in text and 'Mac agent tool returned: list_sources' in text, text
                assert ready_marker in text
                await key('4e'); await key('50')
                await capture('','live-feedback-desktop.png')
                p=await state(); image=out/'live-feedback-desktop.png'
                if image.exists():
                    subprocess.run(['sips','--cropToHeightWidth',str(p['height']),str(p['width']),
                        '--cropOffset',str(p['y']),str(p['x']),str(image),'--out',str(out/'live-feedback.png')],check=True,capture_output=True)
                held = await copy_transcript()
                await box.send('done',status='completed')
                for _ in range(30):
                    if not await busy():break
                    await asyncio.sleep(.5)
                assert not await busy()
                await key('33',('67',))
                assert await clipboard_text()==held, 'Completion replaced selected text'
                await key('4e')
                for _ in range(5):
                    await asyncio.sleep(1)
                    final=await copy_transcript()
                    if '[Mac relay completed]' in final and 'Finished with exit code 0.' in final:break
                    await key('4e')
                assert 'Finished with exit code 0.' in final, final
                assert '[Mac relay completed]' in final and final.count(first_marker)==1,final
                await key('4e')
                await bridge.remove(job['id']+'.out')
                # Stop after partial output and verify no later text is accepted.
                await key('50'); await key('20',('67',)); await key('41')
                await key('10'); await key('42')
                for _ in range(30):
                    raw=await bridge.read('request.json')
                    if raw:break
                    await asyncio.sleep(.5)
                else:raise AssertionError('Second native relay did not start')
                stopped=validate_job(json.loads(raw)); stopped_box=Mailbox(bridge,stopped)
                await stopped_box.text('Partial response before Stop.\n')
                await key('59',('60',))
                for _ in range(30):
                    if not await busy():break
                    await asyncio.sleep(.5)
                assert not await busy()
                assert await bridge.read(stopped['id']+'.stopped')=='stopped'
                try:
                    await stopped_box.text('MUST_NOT_APPEAR')
                except ConnectionError:
                    pass
                else:raise AssertionError('Stopped task accepted more text')
                stopped_view=await copy_transcript()
                assert 'Mac relay cancelled.' in stopped_view and 'Finished with exit code 20.' in stopped_view, stopped_view
                assert 'Mac agent tool: list_sources' not in stopped_view, 'Previous worker log remained after restart'
                await key('4e')
                saved=(await pull(project+'/transcript.txt','live-final-transcript.txt')).decode()
                assert 'MUST_NOT_APPEAR' not in saved
                await bridge.remove(stopped['id']+'.out',stopped['id']+'.stopped')
                report.write_text(json.dumps({'fixture':fixture,'before_completion':True,'incremental':True,
                    'selection_preserved':True,'completion_selection_preserved':True,'tool_activity':True,'final_no_duplicates':True,'stop_midstream':True,'combined_log_live':True,'combined_log_completion':True,'combined_log_reset':True},indent=2))
            finally:
                await live.close(); await tools.close()
                if await busy():
                    await key('59',('60',))
                    for _ in range(30):
                        if not await busy():break
                        await asyncio.sleep(.5)
                    assert not await busy(),'Worker did not stop during test cleanup'
    finally:
        await shell(fixture+'/ClipboardProbe restore-clip '+fixture+'/clipboard-backup\nDelete '+fixture+'/clipboard-backup')
