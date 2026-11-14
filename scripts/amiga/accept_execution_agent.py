"""Authenticated Codex check of execution tools in the prior isolated fixture.
Requires accept_execution.py to pass first and AI Proxy to be stopped.
Uses an existing host sign-in, never copies credentials to the Amiga.
"""
import asyncio
import json
import time
from pathlib import Path
from codex_relay import Bridge, RelayLock, default_state, validate_job, dispatch_job, parser
ROOT=Path(__file__).resolve().parents[2]
async def main():
    out=ROOT/'.context/execution-acceptance'
    (out/'agent-passed.json').unlink(missing_ok=True)
    evidence=json.loads((out/'passed.json').read_text())
    fixture=evidence['fixture']
    assert fixture.startswith('SD032G:Dev/Termic/Acceptance/execution-') and fixture.rsplit('-',1)[-1].isdigit()
    lock=RelayLock(default_state())
    try:
        async with Bridge('http://127.0.0.1:55010/mcp') as bridge:
            assert await bridge.read('request.json') is None
            parent=fixture
            fixture += '/agent-check-'+str(int(time.time()))
            await bridge.shell('MakeDir '+fixture+'\nCopy '+parent+'/TermicAgent '+fixture+'/TermicAgent\nCopy '+parent+'/agent-profile '+fixture+'/agent-profile\nProtect '+fixture+'/TermicAgent +e')
            local=out/'agent-task.txt'
            local.write_text('Use system_info to inspect the actual Amiga. Use write_file to save detected.rexx: an ARexx script that prints AGENT_EXECUTION_OK followed by the CPU and total Chip/Fast memory bytes returned by system_info. Execute it with run_arexx and report the actual output and exit code. Do not build C or use other tools. Do not ask the user to run the script.')
            await bridge.call('amiga_push_file',{'local_path':str(local),'amiga_path':fixture+'/task.txt'})
            launch=out/'agent-launch'
            launch.write_text('Stack 262144\n'+fixture+'/TermicAgent --agent --project '+fixture+' >'+fixture+'/agent-check.log\n')
            await bridge.call('amiga_push_file',{'local_path':str(launch),'amiga_path':fixture+'/agent-launch'})
            await bridge.shell('Run >NIL: <NIL: Execute '+fixture+'/agent-launch')
            for _ in range(30):
                raw=await bridge.read('request.json')
                if raw:break
                await asyncio.sleep(.5)
            else: raise AssertionError('Worker did not publish a request')
            job=validate_job(json.loads(raw))
            assert job['provider']=='codex' and not job['readonly'] and not job['thread']
            args=parser().parse_args([])
            assert await dispatch_job(bridge,job,args), 'Authenticated agent task failed'
            for _ in range(30):
                text=await bridge.shell('If EXISTS '+fixture+'/.busy\n Echo BUSY\nEndIf')
                if 'BUSY' not in text:break
                await asyncio.sleep(.5)
            else:raise AssertionError('Worker still running')
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
            measured=json.loads(json.loads((out/'results.json').read_text())[0]['result'])
            assert measured['cpu'] in script,script
            for key in ('chip_total','fast_total'):
                assert str(measured['memory_bytes'][key]) in script.replace(',', ''),script
            await pull('transcript.txt')
            await bridge.remove(job['id']+'.out')
            (out/'agent-passed.json').write_text(json.dumps({'fixture':fixture,'authenticated_codex':True,'hardware_script_execution':True},indent=2))
            print('Authenticated Codex hardware -> ARexx save -> execution passed.')
    finally:lock.close()
if __name__=='__main__':asyncio.run(main())
