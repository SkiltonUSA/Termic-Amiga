"""Authenticated Kanban development task; pauses locally for source audit before execution.
Invoked by accept_agents in its isolated installation. Does not execute generated code.
"""
import asyncio
import json
import shutil
from pathlib import Path
import codex_relay

async def stage(root,out,fixture,shell,push,call):
    # Stage before GUI loads its profile registry. Only this installation is changed.
    work=root/'.context/process-monitor';work.mkdir(exist_ok=True)
    for name in ('task.txt','requirements.md'):
        shutil.copyfile(root/'scripts/amiga/fixtures/process-monitor'/name,work/name)
    project=fixture+'/Projects/fixture/Tasks/monitor'
    await shell('If NOT EXISTS '+fixture+'/Relay\n MakeDir '+fixture+'/Relay\nEndIf')
    await shell('If NOT EXISTS '+fixture+'/Projects/fixture/Tasks\n MakeDir '+fixture+'/Projects/fixture/Tasks\nEndIf\nIf NOT EXISTS '+project+'\n MakeDir '+project+'\nEndIf')
    base={key:'' for key in ('id','name','model','command','args','yolo','resume_args','env')}
    base.update(kind=0,visible=1,readonly=0,resume=1,steps=32,tokens=16384,usage=1)
    profile={'version':1,'next_id':2,'profiles':[
        dict(base,id='openai',name='OpenAI API',visible=0),
        dict(base,id='shell',name='Amiga CLI',kind=2,command='C:NewShell',visible=0),
        dict(base,id='monitor-codex',name='Codex on Mac',kind=5)]}
    async def write(name,text,remote):
        local=work/name;local.write_text(text);await push(local,remote)
    await write('agents.json',json.dumps(profile),fixture+'/Config/agents.json')
    await write('env-agents.json',json.dumps(profile),'ENV:Termic/agents.json')
    await write('name.txt','Process monitor',project+'/name.txt')
    await write('state.txt','Ready',project+'/state.txt')
    await write('agent-profile','monitor-codex',project+'/agent-profile')
    for name in ['task.txt','requirements.md']:
        await push(work/name,project+'/'+name)
    await push(root/'amiga/native/mui_compat.h',project+'/mui_compat.h')
    await push(root/'amiga/native/Project/main.c',project+'/main.c')
    await write('build-command.txt','vc +aos68k -cpu=68020 -O1 -ITermic:SDK/MUI/Developer/C/Include main.c -o Program.new -lamiga -lmieee\n',project+'/build-command.txt')
    # The synthetic queue build script is replaced only after queue acceptance finishes.
    (work/'location.json').write_text(json.dumps({'fixture':fixture,'task':project}))

async def exercise(root,out,fixture,shell,push,call,state,click,capture,menu,args):
    work=root/'.context/process-monitor';project=fixture+'/Projects/fixture/Tasks/monitor'
    await push(root/'amiga/native/BuildProject',fixture+'/BuildProject')
    await shell('Execute '+fixture+'/Setup',timeout=60)
    async def read(name):
        local=work/('remote-'+name.replace('/','-'))
        await call('amiga_pull_file',{'amiga_path':project+'/'+name,'local_path':str(local)})
        return local.read_text()
    async def key(code):
        try:await call('amiga_input_key',{'rawkey':code,'direction':'down'})
        finally:await call('amiga_input_key',{'rawkey':code,'direction':'up'})
    board='Termic: Kanban board';queue='Termic: Task queue'
    # Search isolates the genuine task from the synthetic lifecycle fixtures.
    await menu('15');p=await state(board)
    # Use clipboard helper; preserve it on the target, never download clipboard content.
    import struct
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
    await asyncio.sleep(3);await capture(board,'monitor-backlog.png')
    column={'Ready':0,'Needs attention':1,'Review':3}.get((await read('state.txt')).strip())
    assert column is not None,'Monitor task is not ready for an explicit queued retry'
    await click(p['x']+90+180*column,p['y']+84,board)
    await asyncio.sleep(2)
    await click(p['x']+195,p['y']+p['height']-34,board) # Queue agent
    await click(p['x']+p['width']-38,p['y']+p['height']-24,board)
    await menu('26');p=await state(queue)
    await key('13') # Resume
    relay_args=codex_relay.parser().parse_args([])
    async with codex_relay.Bridge('http://127.0.0.1:55110/mcp',fixture+'/Relay') as bridge:
        for _ in range(40):
            try:raw=await bridge.read('request.json')
            except ConnectionError:raw=None
            if raw:break
            await asyncio.sleep(1)
        else:raise AssertionError('Kanban did not publish monitor agent request')
        job=codex_relay.validate_job(json.loads(raw));assert job['provider']=='codex' and not job['readonly']
        assert job['prompt'].strip()==(work/'task.txt').read_text().strip(), 'Queue selected an unexpected task'
        await asyncio.sleep(2);await capture(queue,'monitor-working.png')
        print('MONITOR_AGENT_STARTED '+project,flush=True)
        ok=await codex_relay.dispatch_job(bridge,job,relay_args)
        for _ in range(40):
            if 'BUSY' not in await shell('If EXISTS '+project+'/.busy\n Echo BUSY\nEndIf'):break
            await asyncio.sleep(1)
        else:raise AssertionError('Monitor worker did not finish')
        # Persist evidence even when inference/build failed, for inspection and recovery.
        for name in ['job.log','transcript.txt','state.txt','.review.json']:
            try:await read(name)
            except Exception:pass
        journal=json.loads((work/'remote-.review.json').read_text())
        sources=work/'generated';sources.mkdir(exist_ok=True)
        for item in journal['files']:
            name=item.get('path',item.get('name',''))
            assert name and '/' not in name and ':' not in name and '..' not in name,name
            await call('amiga_pull_file',{'amiga_path':project+'/'+name,'local_path':str(sources/name)})
        built=True
        try:await call('amiga_pull_file',{'amiga_path':project+'/Program','local_path':str(work/'Program')})
        except Exception:built=False
        task_state=(await read('state.txt')).strip()
        await key('33');await menu('15');await asyncio.sleep(3)
        await capture(board,'monitor-review.png')
        (work/'agent-built.json').write_text(json.dumps({'task':project,'agent':job['id'],'queued_via_ui':True,'state':task_state,'agent_ok':ok,'binary_exists':built},indent=2))
        print('MONITOR_AWAITING_SOURCE_AUDIT',flush=True)
        # The coordinator audits generated native code before any execution. The
        # marker is private local state, never an agent-generated remote file.
        for _ in range(7200):
            if (work/'audit-finished').exists():break
            await asyncio.sleep(1)
        else:raise AssertionError('Source audit not completed; leave task in Review')

        import accept_monitor_finish
        await accept_monitor_finish.exercise(root,out,fixture,project,shell,push,call,state,click,capture,menu)
