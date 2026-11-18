"""Authenticated host-agent check of execution tools in the prior isolated fixture.
Requires accept_execution.py to pass first and AI Proxy to be stopped.
Uses an existing host sign-in, never copies credentials to the Amiga.
"""
import argparse
import asyncio
import re
import json
import time
import zlib
from pathlib import Path
from codex_relay import Bridge, RelayLock, default_state, validate_job, dispatch_job, parser
ROOT=Path(__file__).resolve().parents[2]
async def main(options):
    provider=options.provider
    if options.profile and not re.fullmatch(r'[A-Za-z0-9_-]{1,31}',options.profile):
        raise ValueError('Invalid profile ID')
    if provider!='codex' and not options.profile:
        raise ValueError('Supply an editable native profile with --profile')
    base=ROOT/'.context/execution-acceptance'
    out=base if provider=='codex' else base/('arexx-'+provider)
    out.mkdir(exist_ok=True)
    (out/'agent-passed.json').unlink(missing_ok=True)
    evidence=json.loads((base/'passed.json').read_text())
    fixture=evidence['fixture']
    assert fixture.startswith('SD032G:Dev/Termic/Acceptance/execution-') and fixture.rsplit('-',1)[-1].isdigit()
    lock=RelayLock(default_state())
    try:
        async with Bridge('http://127.0.0.1:55110/mcp') as bridge:
            assert await bridge.read('request.json') is None
            parent=fixture
            fixture += '/arexx-'+provider+'-'+str(int(time.time()))
            await bridge.shell('MakeDir '+fixture+'\nCopy Termic:TermicAgent '+fixture+'/TermicAgent\nCopy '+parent+'/agent-profile '+fixture+'/agent-profile\nProtect '+fixture+'/TermicAgent +e')
            if options.worker:
                raw=options.worker.read_bytes()
                assert raw[:4]==b'\0\0\3\xf3', 'Candidate must be an Amiga Hunk'
                await bridge.call('amiga_push_file',{'local_path':str(options.worker.resolve()),'amiga_path':fixture+'/TermicAgent'})
                await bridge.shell('Protect '+fixture+'/TermicAgent +e')
                checksum=await bridge.call('amiga_checksum',{'path':fixture+'/TermicAgent'})
                assert f'{zlib.crc32(raw):08X}' in checksum and f'{len(raw)} bytes' in checksum
            if options.profile:
                selected=out/'agent-profile';selected.write_text(options.profile)
                await bridge.call('amiga_push_file',{'local_path':str(selected),'amiga_path':fixture+'/agent-profile'})
            local=out/'agent-task.txt'
            local.write_text('Use system_info to inspect the actual Amiga. Use write_file to save detected.rexx: an ARexx script that prints AGENT_EXECUTION_OK followed by the CPU and total Chip/Fast memory bytes returned by system_info. Execute it with run_arexx and report the actual output and exit code. Do not build C or use other tools. Do not ask the user to run the script.')
            await bridge.call('amiga_push_file',{'local_path':str(local),'amiga_path':fixture+'/task.txt'})
            launch=out/'agent-launch'
            launch.write_text('Stack 262144\n'+fixture+'/TermicAgent --agent --project '+fixture+' >'+fixture+'/agent-check.log\n')
            await bridge.call('amiga_push_file',{'local_path':str(launch),'amiga_path':fixture+'/agent-launch'})
            started=True
            try:
                await bridge.shell('Run >NIL: <NIL: Execute '+fixture+'/agent-launch')
                for _ in range(30):
                    raw=await bridge.read('request.json')
                    if raw:break
                    await asyncio.sleep(.5)
                else: raise AssertionError('Worker did not publish a request')
                job=validate_job(json.loads(raw))
                assert job['provider']==provider and not job['readonly'] and not job['thread']
                args=parser().parse_args([])
                assert await dispatch_job(bridge,job,args), 'Authenticated agent task failed'
                for _ in range(30):
                    text=await bridge.shell('If EXISTS '+fixture+'/.busy\n Echo BUSY\nEndIf')
                    if 'BUSY' not in text:break
                    await asyncio.sleep(.5)
                else:raise AssertionError('Worker still running')
                started=False
                async def pull(name):
                    path=out/name
                    await bridge.call('amiga_pull_file',{'amiga_path':fixture+'/'+name,'local_path':str(path)})
                    return path.read_text()
                log=await pull('agent-check.log')
                for name in ('system_info','write_file','run_arexx'):
                    assert 'Mac agent tool: '+name in log,log
                assert 'AGENT_EXECUTION_OK' in log and 'arexx exit code: 0' in log,log
                assert (await pull('.result')).strip()=='0'
                script=await pull('detected.rexx')
                measured=json.loads(json.loads((base/'results.json').read_text())[0]['result'])
                assert measured['cpu'] in script,script
                for key in ('chip_total','fast_total'):
                    assert str(measured['memory_bytes'][key]) in script.replace(',', ''),script
                await pull('transcript.txt')
                await bridge.remove(job['id']+'.out')
                (out/'agent-passed.json').write_text(json.dumps({'fixture':fixture,'worker_source':'candidate' if options.worker else 'installed','authenticated_codex':provider=='codex','authenticated_provider':provider,'hardware_script_execution':True},indent=2))
                print(provider.title()+' hardware -> ARexx save -> execution passed.')
            finally:
                if started:
                    await bridge.shell('If EXISTS '+fixture+'/.busy\n '+parent+'/StopJob '+fixture+'\nEndIf')
                    for _ in range(30):
                        state=await bridge.shell('If EXISTS '+fixture+'/.busy\n Echo BUSY\nEndIf')
                        if 'BUSY' not in state:break
                        await asyncio.sleep(.5)
                    else:raise AssertionError('Fixture worker did not stop')
    finally:lock.close()
if __name__=='__main__':
    options=argparse.ArgumentParser(description=__doc__)
    options.add_argument('--provider',choices=['codex','claude','cursor'],default='codex')
    options.add_argument('--profile',help='Existing editable Amiga profile ID for this provider')
    options.add_argument('--worker',type=Path,help='Optional candidate Hunk worker to stage instead of the installed worker')
    asyncio.run(main(options.parse_args()))
