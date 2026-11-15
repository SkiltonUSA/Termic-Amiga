"""Continue the Kanban acceptance after explicit local source-audit completion.
Only runs the reviewed app's bounded fixture service tests, then its synthetic UI.
"""
import asyncio
import hashlib
import json
from pathlib import Path
from codex_relay import Bridge,Mailbox,validate_job

async def exercise(root,out,fixture,project,shell,push,call,state,click,capture,menu):
    work=root/'.context/process-monitor';board='Termic: Kanban board';queue='Termic: Task queue'
    audit=json.loads((work/'audit-finished').read_text())
    assert audit['kernel_code_reviewed'] and audit['fixture_only_execution']
    source=root/'amiga/apps/process-monitor'
    for name,digest in audit['files'].items():
        assert name and '/' not in name and ':' not in name and '..' not in name
        assert hashlib.sha256((source/name).read_bytes()).hexdigest()==digest
    async def key(code):
        try:await call('amiga_input_key',{'rawkey':code,'direction':'down'})
        finally:await call('amiga_input_key',{'rawkey':code,'direction':'up'})
    async def read(name):
        local=work/('final-'+name.replace('/','-'))
        await call('amiga_pull_file',{'amiga_path':project+'/'+name,'local_path':str(local)})
        return local.read_text()
    async def write(name,text):
        local=work/('stage-'+name);local.write_text(text);await push(local,project+'/'+name)
    async def queue_current(build):
        # Board search and selection are preserved from the first agent run.
        p=await state(board,front=True)
        await click(p['x']+(365 if build else 195),p['y']+p['height']-34,board)
        await click(p['x']+p['width']-38,p['y']+p['height']-24,board)
        await menu('26');await key('13')
    async def wait_finished():
        for _ in range(240):
            value=await shell('If EXISTS '+project+'/.busy\n Echo BUSY\nEndIf')
            local=work/'queue-state.json'
            await call('amiga_pull_file',{'amiga_path':fixture+'/Config/queue.json','local_path':str(local)})
            rows=json.loads(local.read_text())['items']
            if rows and rows[-1]['path']==project and rows[-1]['state'] in (2,3) and 'BUSY' not in value:return
            await asyncio.sleep(1)
        raise AssertionError('Monitor queue entry did not complete')
    if (work/'fixture-restarted.json').exists():
        # A rebuilt native GUI may be installed at the local audit barrier.
        # Restore selection through the same search/list controls after restart.
        import struct
        await menu('15');p=await state(board,front=True)
        await shell(fixture+'/ClipboardProbe save-clip '+fixture+'/monitor-clipboard')
        try:
            content=b'Process monitor';chunk=b'CHRS'+struct.pack('>I',len(content))+content+b'\0'*(len(content)%2)
            local=work/'search.iff';local.write_bytes(b'FORM'+struct.pack('>I',4+len(chunk))+b'FTXT'+chunk)
            await push(local,fixture+'/monitor-search.iff')
            await shell(fixture+'/ClipboardProbe restore-clip '+fixture+'/monitor-search.iff')
            await click(p['x']+400,p['y']+31,board)
            await call('amiga_input_key',{'rawkey':'67','direction':'down'})
            try:await key('34')
            finally:await call('amiga_input_key',{'rawkey':'67','direction':'up'})
            await key('44')
        finally:await shell(fixture+'/ClipboardProbe restore-clip '+fixture+'/monitor-clipboard')
        await asyncio.sleep(3);await capture(board,'monitor-native-restart.png')
        column=1 if (await read('state.txt')).strip()=='Needs attention' else 3
        await click(p['x']+(270 if column==1 else 630),p['y']+84,board)
        await asyncio.sleep(3)
    # Audited corrections use the actual native file tools, preserving the review
    # journal and all read-only/queue protections. No host writes bypass it.
    await queue_current(False)
    async with Bridge('http://127.0.0.1:55010/mcp',fixture+'/Relay') as bridge:
        for _ in range(40):
            try:raw=await bridge.read('request.json')
            except ConnectionError:raw=None
            if raw:break
            await asyncio.sleep(1)
        else:raise AssertionError('Audited source update was not queued')
        job=validate_job(json.loads(raw));box=Mailbox(bridge,job)
        assert not job['readonly'] and len(audit['files'])+1<=job['limit']
        try:
            await box.text('Applying coordinator-reviewed source corrections through native file tools.\n')
            for name in audit['files']:
                text=(source/name).read_text(encoding='ascii')
                assert len(text)<=32768
                result=await box.send('tool',name='write_file',arguments={'path':name,'source':text})
                assert 'saved' in result.lower(),(name,result)
            await box.send('done',status='completed')
        except BaseException:
            try:await box.send('done',status='failed')
            except Exception:pass
            raise
    await wait_finished();assert (await read('state.txt')).strip()=='Review'
    await asyncio.sleep(5);await capture(queue,'monitor-queue-finished.png')
    await key('33');await menu('15')
    # This queue build compiles and tests before BuildProject promotes Program.new.
    build='\n'.join(['FailAt 21','Stack 262144',
        'vc +aos68k -cpu=68020 -O1 -ITermic:SDK/MUI/Developer/C/Include main.c -o Program.new -lamiga -lmieee',
        'If WARN',' Quit 20','EndIf','Program.new --self-test','If WARN',' Quit 20','EndIf','Quit 0',''])
    await write('build-checked',build)
    await write('build-command.txt','Execute build-checked\n')
    await queue_current(True);await wait_finished()
    log=await read('job.log');assert 'PROCESS_MONITOR_SELFTEST_PASS' in log,log
    assert (await read('.result')).strip()=='0'
    assert (await read('state.txt')).strip()=='Review'
    await key('33');await menu('15');await asyncio.sleep(3)
    await capture(board,'monitor-tests-passed.png')
    # Read-only physical report is private evidence, never a public fixture.
    await shell('Stack 262144\n'+project+'/Program --report > '+project+'/report.txt',timeout=60)
    report=await read('report.txt');assert report.strip()
    # The screenshot mode has synthetic rows and disables real process actions.
    title='Termic Process Monitor'
    await shell('Run >NIL: <NIL: '+project+'/Program --fixture-ui')
    try:
        for _ in range(30):
            p=await state(title)
            if p['address']:break
            await asyncio.sleep(1)
        else:raise AssertionError('Monitor MUI window did not open')
        await state(title,front=True);await asyncio.sleep(4)
        await capture(title,'process-monitor.png')
        await call('amiga_window_resize',{'window':format(p['address'],'08x'),'width':1000,'height':580})
        await asyncio.sleep(3);p=await state(title);assert p['address']
        await capture(title,'process-monitor-resized.png')
    finally:
        p=await state(title,front=True)
        if p['address']:await click(p['x']+10,p['y']+8,title)
    for _ in range(20):
        if not (await state(title))['address']:break
        await asyncio.sleep(1)
    else:raise AssertionError('Monitor did not close normally')
    # Return the task to a normal build command for subsequent user development.
    await write('build-command.txt','vc +aos68k -cpu=68020 -O1 -ITermic:SDK/MUI/Developer/C/Include main.c -o Program.new -lamiga -lmieee\n')
    p=await state(board,front=True)
    await click(p['x']+510,p['y']+p['height']-34,board) # Review
    review='Termic: Review agent changes';p=await state(review);assert p['address']
    for _ in range(64):
        if not json.loads(await read('.review.json'))['files']:break
        await click(p['x']+65,p['y']+p['height']-24,review)
    else:raise AssertionError('Audited edits did not resolve')
    await click(p['x']+p['width']-24,p['y']+p['height']-24,review)
    await menu('15');p=await state(board)
    await click(p['x']+604,p['y']+p['height']-34,board)
    assert (await read('state.txt')).strip()=='Done'
    await asyncio.sleep(3);await capture(board,'monitor-done.png')
    await call('amiga_pull_file',{'amiga_path':project+'/Program','local_path':str(work/'Program-tested')})
    (work/'passed.json').write_text(json.dumps({'task':project,'real_agent_queued':True,
        'audited_updates_through_native_tools':True,'queued_native_build':True,
        'disposable_service_stop_restart':True,'read_only_report':True,
        'review_accepted':True,'mui_open_resize_close':True,'kanban_done':True,'source_audit':audit},indent=2))
