"""Opt-in authenticated Codex build/repair/run check on the installed Amiga worker.
Requires accept_execution.py evidence and an idle, stopped AI Proxy. Uses a
fresh fixture task and the existing host login; never edits a user project.
"""
import asyncio
import json
import time
from pathlib import Path

from codex_relay import Bridge, RelayLock, default_state, dispatch_job, parser, validate_job

ROOT = Path(__file__).resolve().parents[2]
MARKER = 'AGENT_BUILD_EXECUTION_OK'


def verify_log(log):
    """Require native outcomes in order, independent of the model's claims."""
    expected = ('Mac agent tool: read_', 'Mac agent tool: build',
                'intentional_build_failure', 'build exit code: 20',
                'Mac agent tool: write_', 'Mac agent tool: build',
                'build exit code: 0', 'Mac agent tool: run_program',
                MARKER, 'program exit code: 0', 'Finished with exit code 0.')
    position = 0
    for value in expected:
        found = log.find(value, position)
        assert found >= 0, 'Missing/out-of-order native evidence: ' + value
        position = found + len(value)
    assert log.count('Mac agent tool: run_program\n') == 1, 'Expected exactly one execution'


async def main():
    evidence_dir = ROOT / '.context/execution-acceptance'
    parent = json.loads((evidence_dir / 'passed.json').read_text())['fixture']
    assert parent.startswith('SD032G:Dev/Termic/Acceptance/execution-')
    assert parent.rsplit('-', 1)[-1].isdigit()
    out = evidence_dir / 'build-agent'
    out.mkdir(exist_ok=True)
    (out / 'passed.json').unlink(missing_ok=True)
    fixture = parent + '/build-agent-' + str(int(time.time()))
    lock = RelayLock(default_state())
    try:
        async with Bridge('http://127.0.0.1:55010/mcp') as bridge:
            assert await bridge.read('request.json') is None, 'A task owns the relay'
            await bridge.shell('MakeDir ' + fixture + '\nCopy Termic:TermicAgent ' + fixture +
                               '/TermicAgent\nCopy ' + parent + '/agent-profile ' + fixture +
                               '/agent-profile\nCopy ' + parent + '/native-build.config ' + fixture +
                               '/native-build.config\nProtect ' + fixture + '/TermicAgent +e')
            installed = await bridge.call('amiga_checksum', {'path': 'Termic:TermicAgent'})
            staged = await bridge.call('amiga_checksum', {'path': fixture + '/TermicAgent'})
            assert installed.splitlines()[1:] == staged.splitlines()[1:]

            async def write(name, text):
                local = out / name
                local.write_text(text)
                await bridge.call('amiga_push_file', {'local_path': str(local),
                                                      'amiga_path': fixture + '/' + name})

            await write('main.c', '#include <stdio.h>\n#error intentional_build_failure\n'
                        'int main(void) { puts("' + MARKER + '"); return 0; }\n')
            await write('build-command.txt', 'SD032G:Dev/Termic/SDK/vbcc_bin_amigaos68k/bin/vc +'
                        + fixture + '/native-build.config -cpu=68020 -O1 main.c -o Program.new -lamiga\n')
            await write('task.txt', 'Read main.c. Build it unchanged once so you receive actual compiler '
                        'diagnostics. After that failure, repair the source while preserving its greeting '
                        + MARKER + '. Build again and, only after success, execute Program exactly once '
                        'using run_program. Report the real compiler failure, correction, final output '
                        'and exit code. Use only the supplied Amiga tools. Do not ask me to run commands.')
            await write('launch-test', 'Stack 262144\n' + fixture + '/TermicAgent --agent --project '
                        + fixture + ' >' + fixture + '/job.log\n')
            started = False
            try:
                await bridge.shell('Run >NIL: <NIL: Execute ' + fixture + '/launch-test')
                started = True
                for _ in range(30):
                    raw = await bridge.read('request.json')
                    if raw:
                        break
                    await asyncio.sleep(.5)
                else:
                    raise AssertionError('Worker did not publish a request')
                job = validate_job(json.loads(raw))
                assert job['provider'] == 'codex' and not job['readonly'] and not job['thread']
                assert job['limit'] >= 5
                assert await dispatch_job(bridge, job, parser().parse_args([])), 'Agent task failed'
                for _ in range(30):
                    state = await bridge.shell('If EXISTS ' + fixture + '/.busy\n Echo BUSY\nEndIf')
                    if 'BUSY' not in state:
                        break
                    await asyncio.sleep(.5)
                else:
                    raise AssertionError('Worker still running')
                started = False

                async def pull(name):
                    path = out / name
                    await bridge.call('amiga_pull_file', {'amiga_path': fixture + '/' + name,
                                                          'local_path': str(path)})
                    return path.read_bytes()

                verify_log((await pull('job.log')).decode('ascii'))
                assert (await pull('.result')).strip() == b'0'
                assert (await pull('.command.log')).strip() == MARKER.encode()
                source = (await pull('main.c')).decode('ascii')
                assert MARKER in source
                assert (await pull('Program'))[:4] == b'\0\0\3\xf3'
                await pull('transcript.txt')
                await bridge.remove(job['id'] + '.out')
                (out / 'passed.json').write_text(json.dumps({
                    'fixture': fixture, 'installed_worker_checksum': installed.splitlines()[1:],
                    'authenticated_codex': True, 'compiler_failure_repaired': True,
                    'native_build_and_execution': True, 'exactly_one_execution': True,
                }, indent=2))
                print('Authenticated Codex read -> failed build -> repair -> build -> run passed.')
            finally:
                if started:
                    # This helper addresses only the unique fixture's worker port.
                    # Never replay a run whose acknowledgement may have been lost.
                    await bridge.shell('If EXISTS ' + fixture + '/.busy\n ' + parent +
                                       '/StopJob ' + fixture + '\nEndIf')
    finally:
        lock.close()


if __name__ == '__main__':
    asyncio.run(main())
