"""Opt-in regression check: Stop during a native build must release the worker.
Requires accept_execution.py evidence and a stopped, idle AI Proxy. Uses a
fresh fixture and the mailbox directly; no cloud inference or user task edits.
"""
import argparse
import asyncio
import json
from pathlib import Path
import time
import zlib
from codex_relay import Bridge, Mailbox, RelayLock, default_state, validate_job

ROOT = Path(__file__).resolve().parents[2]

async def main(options):
    out = ROOT / '.context/execution-acceptance/stop-build'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'passed.json').unlink(missing_ok=True)
    parent = json.loads((out.parent / 'passed.json').read_text())['fixture']
    assert parent.startswith('SD032G:Dev/Termic/Acceptance/execution-')
    assert parent.rsplit('-', 1)[-1].isdigit()
    fixture = parent + '/stop-build-' + str(int(time.time()))
    lock = RelayLock(default_state())
    try:
        async with Bridge(options.bridge) as bridge:
            assert await bridge.read('request.json') is None, 'A task owns the relay'
            await bridge.shell('MakeDir ' + fixture)
            raw = options.worker.read_bytes()
            assert raw[:4] == b'\0\0\3\xf3'
            await bridge.call('amiga_push_file', {'local_path':str(options.worker.resolve()),
                                                  'amiga_path':fixture+'/TermicAgent'})
            checksum = await bridge.call('amiga_checksum', {'path':fixture+'/TermicAgent'})
            assert f'{zlib.crc32(raw):08X}' in checksum and f'{len(raw)} bytes' in checksum
            async def write(name, value):
                local = out / name; local.write_text(value)
                await bridge.call('amiga_push_file', {'local_path':str(local), 'amiga_path':fixture+'/'+name})
            async def pull(name):
                local = out / name
                await bridge.call('amiga_pull_file', {'local_path':str(local), 'amiga_path':fixture+'/'+name})
                return local.read_text()
            await write('agent-profile', options.profile)
            await write('task.txt', 'Synthetic Stop-during-build acceptance. No cloud inference.')
            await write('build-command.txt', 'Echo STARTED >build-started\nWait 15 SECS\nEcho BUILD_STOP_FIXTURE\nQuit 20\n')
            await write('launch-test', 'Stack 262144\n'+fixture+'/TermicAgent --agent --project '+fixture+' >'+fixture+'/job.log\n')
            await bridge.shell('Protect '+fixture+'/TermicAgent +e\nRun >NIL: <NIL: Execute '+fixture+'/launch-test')
            for _ in range(30):
                request = await bridge.read('request.json')
                if request: break
                await asyncio.sleep(.5)
            else: raise AssertionError('Worker did not start')
            box = Mailbox(bridge, validate_job(json.loads(request)))
            assert not box.job['readonly']
            running = asyncio.create_task(box.send('tool', name='build', arguments={}))
            try:
                for _ in range(30):
                    async with bridge.lock:
                        state = await bridge.shell('If EXISTS '+fixture+'/build-started\n Echo STARTED\nEndIf')
                    if 'STARTED' in state: break
                    await asyncio.sleep(.25)
                else: raise AssertionError('Delayed build did not start')
                async with bridge.lock:
                    await bridge.shell(parent+'/StopJob '+fixture)
                try: await running
                except ConnectionError: pass  # Stop can arrive before its result download.
                for _ in range(40):
                    state = await bridge.shell('If EXISTS '+fixture+'/.busy\n Echo BUSY\nEndIf')
                    if 'BUSY' not in state: break
                    await asyncio.sleep(.5)
                else: raise AssertionError('Stop left a stale task lock')
                assert await bridge.read('request.json') is None, 'Stop left a stale relay request'
                assert await bridge.read(box.job['id']+'.stopped') == 'stopped'
                result = json.loads(json.loads(await bridge.read(box.job['id']+'.out'))['text'])
                assert result['cancelled'] and not result['success'] and result['exit_code']==20, result
                assert (await pull('.result')).strip() == '20'
                log = await pull('job.log')
                assert 'BUILD_STOP_FIXTURE' in log and 'Finished with exit code 20.' in log
                ports = await bridge.call('amiga_list_ports', {})
                digest = 5381
                for byte in fixture.encode('ascii'): digest = (digest*33+byte)&0xffffffff
                assert 'TermicCodexRelay' not in ports and f'TermicJob.{digest:08x}' not in ports
                await bridge.remove(box.job['id']+'.out', box.job['id']+'.stopped')
                (out/'passed.json').write_text(json.dumps({'fixture':fixture,
                    'worker_checksum':checksum.splitlines()[1:], 'stop_during_build':True,
                    'cancelled_result':True, 'ports_and_task_lock_released':True}, indent=2))
                print('Stop during native build returned cancellation and released ports, mailbox and task lock.')
            finally:
                if not running.done():
                    running.cancel()
                    await asyncio.gather(running, return_exceptions=True)
    finally:
        lock.close()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker', type=Path, required=True)
    parser.add_argument('--profile', required=True)
    parser.add_argument('--bridge', default='http://127.0.0.1:55110/mcp')
    asyncio.run(main(parser.parse_args()))
