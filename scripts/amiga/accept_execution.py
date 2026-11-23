"""Opt-in execution-tool acceptance on an isolated task, using the real mailbox.
Stop AI Proxy first. Uses an existing editable host-agent profile without changing
it, assigning volumes, touching user tasks, or making a cloud request.
"""
import argparse
import asyncio
import json
import time
import zlib
from pathlib import Path
from codex_relay import Bridge, Mailbox, RelayLock, default_state, validate_job

ROOT = Path(__file__).resolve().parents[2]

class EvidenceBridge(Bridge):
    async def call(self, name, fields):
        result = await asyncio.wait_for(self.session.call_tool(name, fields), 45)
        text = '\n'.join(getattr(c, 'text', '') for c in result.content)
        if result.isError or any(marker in text for marker in ('[ERROR]', 'ERROR:', '[STILL RUNNING]', 'Cannot read remote file')):
            (ROOT/'.context/execution-acceptance/bridge-error.txt').write_text(name+'\n'+text)
            raise ConnectionError('Acceptance bridge operation failed: '+name)
        return text

async def main(args):
    out = ROOT / '.context/execution-acceptance'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'passed.json').unlink(missing_ok=True)
    for binary in (args.worker, args.stop_probe):
        assert binary.is_file() and binary.read_bytes()[:4] == b'\0\0\3\xf3', 'Missing or invalid Hunk: '+str(binary)
    lock = RelayLock(default_state())
    fixture = 'SD032G:Termic-Amiga/Acceptance/execution-' + str(int(time.time()))
    try:
        async with EvidenceBridge(args.bridge) as bridge:
            async def call(name, fields):
                return await bridge.call(name, fields)
            async def shell(script):
                return await bridge.call('amiga_run_script', {'script':'FailAt 10\nStack 262144\n'+script, 'timeout':15})
            async def write(name, text):
                local = out / name
                local.write_text(text)
                await call('amiga_push_file', {'local_path': str(local), 'amiga_path': fixture + '/' + name})
            async def busy():
                return 'BUSY' in await shell('If EXISTS '+fixture+'/.busy\n Echo BUSY\nEndIf')
            async def completed():
                for _ in range(30):
                    if not await busy(): return
                    await asyncio.sleep(.5)
                raise AssertionError('Native worker did not complete')
            assert await bridge.read('request.json') is None, 'A task already owns the relay'
            await shell('MakeDir ' + fixture)
            await call('amiga_push_file', {'local_path':str(args.worker.resolve()), 'amiga_path':fixture+'/TermicAgent'})
            await shell('Protect '+fixture+'/TermicAgent +e')
            await call('amiga_push_file', {'local_path':str(args.stop_probe.resolve()), 'amiga_path':fixture+'/StopJob'})
            for name, binary in [('TermicAgent', args.worker), ('StopJob', args.stop_probe)]:
                raw=binary.read_bytes(); checksum=await call('amiga_checksum', {'path':fixture+'/'+name})
                assert f'{zlib.crc32(raw):08X}' in checksum and f'{len(raw)} bytes' in checksum,checksum
            await shell('Protect '+fixture+'/StopJob +e')
            await write('agent-profile', args.profile)
            await write('main.c', 'int main(void) { return 0; }\n')
            await write('task.txt', 'Isolated execution framework acceptance. No cloud inference.')
            # Keep compiler settings local to the fixture.
            sdk='SD032G:Termic-Amiga/SDK'; binpath=sdk+'/vbcc_bin_amigaos68k/bin/'
            target=sdk+'/vbcc_target_m68k-amigaos/targets/m68k-amigaos/'
            config='\n'.join([
                '-cc='+binpath+'vbccm68k -quiet %s -o= %s %s -O=%ld -I'+target+'include -I'+sdk+'/NDK/Include_H',
                '-as='+binpath+'vasmm68k_mot -quiet -Fhunk -nowarn=62 %s -o %s',
                '-rm=delete quiet %s',
                '-ld='+binpath+'vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -mrel '+target+'lib/startup.o %s %s -L'+target+'lib -lvc -o %s',
                '-ldnodb=-s -Rshort','-ul=-l%s','-cf=-F%s','-ml=500','-hunkdebug','-amiga-softfloat',''])
            await write('native-build.config', config)
            await write('build-command.txt', binpath+'vc +'+fixture+'/native-build.config -cpu=68020 -O1 main.c -o Program.new -lamiga\n')
            box = None
            async def start():
                await shell('Run >NIL: <NIL: '+fixture+'/TermicAgent --agent --project '+fixture)
                for _ in range(30):
                    raw = await bridge.read('request.json')
                    if raw: break
                    await asyncio.sleep(.5)
                else: raise AssertionError('Native request not published')
                job = validate_job(json.loads(raw))
                assert not job['readonly'] and job['limit'] >= 8, 'Use an editable relay profile with at least eight calls'
                return Mailbox(bridge, job)
            results=[]
            async def tool(name, **fields):
                value = await box.send('tool', name=name, arguments=fields)
                results.append({'tool':name, 'result':value})
                (out/'results.json').write_text(json.dumps(results,indent=2))
                print(name+': '+value[:1200], flush=True)
                return value
            async def finish():
                await box.send('done', status='completed')
                await completed()
                await bridge.remove(box.job['id']+'.out')
            try:
                box=await start()
                info=json.loads(await tool('system_info'))
                assert info['cpu'] and info['memory_bytes']['fast_total'] > 0 and info['rexxmast_running'],info
                assert info['cpu_clock_mhz'] is None and info['machine_model'] is None
                assert 'saved' in await tool('write_file', path='hello.rexx', source='/* synthetic execution fixture */\nsay "AREXX_EXECUTION_OK"\nexit 0\n')
                assert 'AREXX_EXECUTION_OK' in await tool('read_file', path='hello.rexx')
                run=json.loads(await tool('run_arexx', path='hello.rexx'))
                assert run['success'] and run['exit_code']==0 and 'AREXX_EXECUTION_OK' in run['output'],run
                assert 'saved' in await tool('write_source', source='#include <stdio.h>\nint main(void) { puts("NATIVE_EXECUTION_OK"); return 0; }\n')
                assert json.loads(await tool('build'))['success']
                run=json.loads(await tool('run_program'))
                assert run['success'] and run['exit_code']==0 and 'NATIVE_EXECUTION_OK' in run['output'],run
                assert 'Invalid path' in await tool('read_file', path='../outside.c')
                await finish()
                box=await start()
                assert 'saved' in await tool('write_file', path='hello.py', source='print("MICROPYTHON_AGENT_OK")\n')
                run=json.loads(await tool('run_python', path='hello.py'))
                assert run['success'] and run['exit_code']==0 and 'MICROPYTHON_AGENT_OK' in run['output'],run
                assert 'Invalid MicroPython path' in await tool('run_python', path='SYS:outside.py')
                assert 'Invalid path' in await tool('write_file', path='State.txt', source='must not change state')
                await finish()
                checksum=await call('amiga_checksum', {'path':fixture+'/Program'})
                box=await start()
                assert 'saved' in await tool('write_file', path='failure.rexx', source='/* synthetic syntax failure */\nsay (1 +\n')
                run=json.loads(await tool('run_arexx', path='failure.rexx'))
                assert not run['success'] and run['exit_code']!=0,run
                assert 'Build required' in await tool('run_program')
                assert 'Invalid ARexx path' in await tool('run_arexx', path='SYS:outside.rexx')
                assert 'saved' in await tool('write_source', source='#error intentional acceptance failure\n')
                failed=json.loads(await tool('build'))
                assert not failed['success'] and failed['exit_code']!=0,failed
                assert 'intentional acceptance failure' in failed['output'],failed
                assert 'Build required' in await tool('run_program')
                assert 'Invalid ARexx path' in await tool('run_arexx', path='main.c')
                await finish()
                assert await call('amiga_checksum', {'path':fixture+'/Program'}) == checksum, 'Failed build replaced previous Program'
                box=await start()
                assert 'saved' in await tool('write_source', source='int main(void) { return 7; }\n')
                assert json.loads(await tool('build'))['success']
                run=json.loads(await tool('run_program'))
                assert not run['success'] and run['exit_code']==7,run
                assert 'saved' in await tool('write_source', source='#include <proto/exec.h>\n#include <proto/dos.h>\nint main(void) { int i; for(i=0;i<1500;i++) { if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C) return 20; Delay(1); } return 0; }\n')
                assert json.loads(await tool('build'))['success']
                running=asyncio.create_task(tool('run_program'))
                await asyncio.sleep(4)
                async with bridge.lock:
                    await shell(fixture+'/StopJob '+fixture)
                try: await running
                except ConnectionError: pass # Stop may be seen before the final reply is downloaded.
                await completed()
                assert await bridge.read(box.job['id']+'.stopped')=='stopped'
                reply=box.job['id']+('.r'+str(box.seq & 1) if box.job.get('reply_slots') else '.out')
                output=json.loads(await bridge.read(reply))
                run=json.loads(output['text'])
                assert not run['success'] and run['cancelled'] and run['exit_code']==20,run
                await bridge.remove(box.job['id']+'.out',box.job['id']+'.r0',box.job['id']+'.r1',box.job['id']+'.stopped')
                (out/'passed.json').write_text(json.dumps({'fixture':fixture,'hardware':True,'arexx_run':True,
                    'micropython_run':True,'protected_metadata_rejected':True,'native_build_run':True,'actual_output_exit':True,'failed_build_preserves_program':True,
                    'stale_run_blocked':True,'paths_rejected':True,'program_failure':True,'cooperative_stop':True},indent=2))
            finally:
                if box and await busy():
                    # Finish the test session; never replay an uncertain execution.
                    try: await box.send('done', status='failed')
                    except Exception: pass
                    await completed()
    finally:
        lock.close()

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker', type=Path, required=True)
    parser.add_argument('--profile', required=True)
    parser.add_argument('--stop-probe', type=Path, required=True)
    parser.add_argument('--bridge', default='http://127.0.0.1:55110/mcp')
    asyncio.run(main(parser.parse_args()))
