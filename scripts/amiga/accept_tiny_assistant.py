"""Physical Amiga Tiny assistant acceptance, in a fresh isolated fixture.

The fixture worker must be built with TERMIC_CONFIG and TERMIC_ENV set to
FIXTURE/Config/ and TERMIC_RELAY_ROOT set to FIXTURE/Relay. This keeps all
synthetic provider messages away from the production AI Proxy and settings.
No provider login or paid inference is used. Production worker supplied with
--worker is tested for local commands; handoffs use --fixture-worker.
"""
import argparse
import asyncio
import json
from pathlib import Path
import re
from codex_relay import Bridge, Mailbox, validate_job
from bridge_client import DEFAULT_BRIDGE

async def main(args):
    fixture=args.fixture
    assert re.fullmatch(r'TMGIT:Projects/tiny-features-[0-9]+',fixture)
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    worker=args.worker.resolve();test_worker=args.fixture_worker.resolve()
    assert (fixture+'/Config/').encode() in test_worker.read_bytes()
    assert (fixture+'/Relay').encode() in test_worker.read_bytes()
    profiles=[]
    for ident,kind in [('openai',0),('shell',2),('tiny',8),('test-codex',5),('test-claude',6),('test-cursor',7)]:
        p={key:'' for key in ['id','name','model','command','args','yolo','resume_args','env']}
        p.update(id=ident,name=ident,kind=kind,visible=1,readonly=1,resume=0,steps=8,tokens=4096,usage=0)
        if kind==2:p['command']='C:NewShell'
        if kind==8:p.update(model='tiny-2.1',command='Termic:Tools/Tiny/tiny68k')
        profiles.append(p)
    results=[]
    async with Bridge(DEFAULT_BRIDGE,fixture,reservation_path=args.reservation) as b:
        ports=await b.call('amiga_list_ports',{})
        assert 'TermicJob.' not in ports and 'TermicCodexRelay' not in ports
        await b.shell('MakeDir '+fixture+'\nMakeDir '+fixture+'/Config\nMakeDir '+fixture+'/Relay')
        async def put(name,data):
            local=out/name.replace('/','-');local.write_bytes(data.encode() if isinstance(data,str) else data)
            await b.call('amiga_push_file',{'local_path':str(local),'amiga_path':fixture+'/'+name})
            await b.verify_file(name,local.read_bytes())
        await put('TermicAgent',worker.read_bytes());await put('FixtureAgent',test_worker.read_bytes())
        await put('Config/agents.json',json.dumps({'version':1,'next_id':1,'profiles':profiles}))
        await put('agent-profile','tiny');await put('main.c','/* TINY_READ_ONLY_FIXTURE */\nint main(void) { return 0; }\n')
        await b.shell('Protect '+fixture+'/TermicAgent +e\nProtect '+fixture+'/FixtureAgent +e')
        relay=Bridge(DEFAULT_BRIDGE,fixture+'/Relay');relay.session=b.session
        async def run(command,expected=0,provider=None,status='completed'):
            await put('task.txt',command)
            await put('launch','Stack 262144\n'+fixture+('/FixtureAgent' if provider else '/TermicAgent')+' --agent --project '+fixture+' >'+fixture+'/job.log\n')
            await b.shell('If EXISTS '+fixture+'/.result\n Delete '+fixture+'/.result\nEndIf\nRun >NIL: <NIL: Execute '+fixture+'/launch')
            if provider:
                for _ in range(40):
                    raw=await relay.read('request.json')
                    if raw:break
                    await asyncio.sleep(.25)
                else:raise AssertionError('No handoff request')
                job=validate_job(json.loads(raw))
                assert job['provider']==provider and job['readonly']
                assert 'Use MUI 5' in job['prompt'] and 'TINY_READ_ONLY_FIXTURE' in job['prompt']
                assert 'Current user request:' in job['prompt'] and len(job['prompt'])<=8192
                box=Mailbox(relay,job)
                await box.text('Synthetic '+provider+' handoff verified; no cloud inference.\n')
                await box.send('done',status=status)
            for _ in range(90):
                result=await b.read('.result')
                if result is not None:break
                await asyncio.sleep(.5)
            else:raise AssertionError('Worker did not finish')
            log=await b.read('job.log');assert int(result)==expected,(command,result,log)
            assert await b.read('agent-profile')=='tiny'
            assert await b.read('task.txt')==command
            assert await b.read('main.c')=='/* TINY_READ_ONLY_FIXTURE */\nint main(void) { return 0; }\n'
            results.append({'command':command,'exit':int(result),'log':log})
            print(command,'exit',result.strip(),flush=True)
            return log
        await run('/help')
        assert 'Offline guide:' in await run('/docs yolo')
        assert 'main.c' in await run('/files')
        assert 'TINY_READ_ONLY_FIXTURE' in await run('/read main.c')
        await run('/remember Use MUI 5')
        assert 'TINY_READ_ONLY_FIXTURE' in await run('/history')
        assert 'Use MUI 5' in await run('/recall MUI')
        for command in ['/read ../main.c','/read SYS:secret.txt','/read transcript.txt','/write main.c replacement','/handoff tiny build']:
            await run(command,20)
        # Refresh the source excerpt after negative tests have consumed turns.
        await run('/read main.c')
        for provider in ['codex','claude','cursor']:
            await run('/handoff test-'+provider+' Review the source without editing it.',provider=provider)
        await run('/handoff test-codex Review without changes.',20,provider='codex',status='failed')
        await run('/forget')
        assert 'No matching saved context' in await run('/history')
        assert 'TINY_READ_ONLY_FIXTURE' in await b.read('transcript.txt')
        (out/'passed.json').write_text(json.dumps({'fixture':fixture,'synthetic_handoffs':True,'results':results},indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reservation',type=Path);p.add_argument('--fixture',required=True);p.add_argument('--worker',type=Path,required=True)
    p.add_argument('--fixture-worker',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    asyncio.run(main(p.parse_args()))
