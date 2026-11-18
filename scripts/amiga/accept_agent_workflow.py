#!/usr/bin/env python3
"""Opt-in real host-agent Kanban acceptance on an idle Amiga.

Requires the installed AI Proxy running with a signed-in Codex account, an
editable native profile named codex, and a working installed TermicAgent/VBCC.
Compile tests/test_agent_workflow.c with the GUI dependencies. This drives the
production queue/review functions in an isolated native MUI harness; it is not
a mouse-click test. Generated files remain in a fresh Acceptance drawer.

At Review, inspect output/main.c and job.log, then create output/approve-review
containing the SHA256 of main.c to accept the generated source. The test never
accepts a model's success claim in place of native build/execution evidence.
"""
import argparse
import asyncio
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import time

from codex_relay import Bridge, PublicationUncertain, decode_checksum

MARKER = 'SYSTEM_SUMMARY_OK'
TITLES = {'board': 'Termic: Kanban board', 'review': 'Termic: Review agent changes'}


async def accept(args):
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    stage = args.drawer.rstrip('/') + '/agent-' + str(int(time.time()))
    task = stage + '/Project/Tasks/system-summary'
    (out / 'stage.json').write_text(json.dumps({'stage': stage, 'task': task}, indent=2))
    async with Bridge(args.server) as bridge:
        assert 'TermicJob.' not in await bridge.call('amiga_list_ports', {}), 'A worker is active'
        assert await bridge.read('request.json') is None, 'A request is pending'
        root = Bridge(args.server, stage)
        root.session = bridge.session
        project = Bridge(args.server, task)
        project.session = bridge.session

        async def push(local, remote):
            raw = local.read_bytes()
            # A push may complete despite a missing acknowledgement. Verify it;
            # never repeat a mutation merely because its response was lost.
            try:
                await bridge.call('amiga_push_file', {'local_path': str(local.resolve()), 'amiga_path': remote})
            except (ConnectionError, TimeoutError):
                pass
            reader = Bridge(args.server, remote.rsplit('/', 1)[0])
            reader.session = bridge.session
            await reader.verify_file(remote.rsplit('/', 1)[1], raw)

        async def write(name, text, remote):
            path = out / name
            path.write_text(text)
            await push(path, remote)

        async def read(name):
            return await root.read(name) or ''

        async def wait_phase(expected, seconds=1200):
            until = time.monotonic() + seconds
            while time.monotonic() < until:
                values = await root.read_many('phase.txt', 'result.txt')
                assert 'FAIL' not in (values['result.txt'] or ''), values['result.txt']
                if values['phase.txt'] == expected:
                    print('Native phase: ' + expected, flush=True)
                    return
                await asyncio.sleep(2)
            raise TimeoutError('Native phase did not reach ' + expected)

        async def control(command):
            try:
                await root.write('control.json', {'command': command})
            except PublicationUncertain:
                # Only the resulting phase/result may resolve an uncertain
                # publication. Do not publish the same command again.
                pass

        async def capture(mode, filename):
            # Cropping a synthetic fixture window avoids capturing real data
            # from other applications or the Workbench desktop.
            await bridge.shell(stage + '/ChromeProbe front "' + TITLES[mode] + '"')
            await asyncio.sleep(2)
            for _ in range(3):
                reply = await bridge.session.call_tool('amiga_screenshot', {'window': TITLES[mode]})
                text = '\n'.join(getattr(c, 'text', '') for c in reply.content)
                match = re.search(r'Screenshot saved: (.+) \((\d+)x(\d+), (RLE|truecolor), (\d+) rows received\)', text)
                if not reply.isError and match and match[3] == match[5]:
                    source = Path(match[1])
                    if source.suffix == '.png':
                        shutil.copyfile(source, out / filename)
                    else:
                        subprocess.run(['sips', '-s', 'format', 'png', str(source), '--out', str(out / filename)],
                                       check=True, capture_output=True)
                    print('Captured ' + filename, flush=True)
                    return
            raise AssertionError('Incomplete screenshot: ' + filename)

        await bridge.shell('MakeDir ' + stage + '\nMakeDir ' + stage + '/Project\nMakeDir '
                           + stage + '/Project/Tasks\nMakeDir ' + task)
        for name, local in [('AgentWorkflowCheck', args.binary), ('ChromeProbe', args.probe)]:
            assert local.read_bytes()[:4] == b'\0\0\3\xf3', 'Expected Amiga Hunk binary'
            await push(local, stage + '/' + name)
        await bridge.shell('Protect ' + stage + '/AgentWorkflowCheck +e\nProtect ' + stage + '/ChromeProbe +e')
        await write('project-name.txt', 'Agent workflow demo', stage + '/Project/name.txt')
        files = {
            'name.txt': 'System summary', 'state.txt': 'Ready', 'agent-profile': 'codex', 'agent-yolo': '0',
            'main.c': '#error AGENT_SOURCE_REQUIRED\n',
            'requirements.md': 'Create a small AmigaOS 3 CLI program in main.c. Print the Exec library version '
                'and available Chip/Fast RAM using public Exec library interfaces and MEMF_CHIP/MEMF_FAST. '
                'Use VBCC-compatible C and the installed NDK. Exit automatically with code 0 and print '
                + MARKER + ' as the final line. Do not change tasks, services, firmware, device state, or '
                'other files. Do not access undocumented memory or ARM hardware. No input or GUI.\n',
            'task.txt': 'Read requirements.md and main.c. Implement the requested CLI program by replacing '
                'main.c with the supplied Amiga file tools. Build with the native build tool and repair '
                'any compiler errors. After a successful build, execute Program exactly once with '
                'run_program. Report the actual output and exit code. Only edit main.c. Do not ask the '
                'user to run anything. Do not simulate success or use host shell/file tools.\n',
            'build-command.txt': 'SD032G:Dev/Termic/SDK/vbcc_bin_amigaos68k/bin/vc +'
                + task + '/native-build.config -cpu=68020 -O1 main.c -o Program.new -lamiga\n',
        }
        for name, text in files.items():
            await write(name, text, task + '/' + name)
        await push(args.compiler_config, task + '/native-build.config')
        await write('launch-test', 'Stack 262144\n' + stage + '/AgentWorkflowCheck ' + stage
                    + ' >' + stage + '/native.log\n', stage + '/launch-test')
        launched = True
        try:
            try:
                await bridge.call('amiga_launch', {'command': 'Execute ' + stage + '/launch-test'})
            except (ConnectionError, TimeoutError):
                pass  # Wait for the native phase; never launch twice.
            await wait_phase('backlog', 60)
            await capture('board', 'kanban-ready.png')
            await control('start')
            await wait_phase('working', 30)
            await wait_phase('review')
            values = await project.read_many('main.c', 'job.log', '.command.log', '.result', 'state.txt', '.review.json')
            for name, value in values.items():
                assert value is not None, 'Missing evidence: ' + name
                (out / name).write_text(value)
            log = values['job.log']
            position = 0
            for marker in ('Mac agent tool: read_', 'Mac agent tool: write_', 'Mac agent tool: build',
                           'build exit code: 0', 'Mac agent tool: run_program', MARKER,
                           'program exit code: 0', 'Finished with exit code 0.'):
                position = log.find(marker, position)
                assert position >= 0, 'Missing/out-of-order evidence: ' + marker
                position += len(marker)
            assert log.count('Mac agent tool: run_program\n') == 1, 'Expected exactly one execution'
            assert values['.result'].strip() == '0' and values['state.txt'] == 'Review'
            assert values['.command.log'].strip().endswith(MARKER)
            checksum = decode_checksum(await bridge.call('amiga_checksum', {'path': task + '/Program'}), task + '/Program')
            assert checksum[1] > 0
            await capture('review', 'agent-changes.png')
            digest = hashlib.sha256(values['main.c'].encode()).hexdigest()
            (out / 'review-ready.json').write_text(json.dumps({'sha256': digest, 'task': task}, indent=2))
            print('Review ready: inspect main.c and job.log; approve-review must contain ' + digest, flush=True)
            until = time.monotonic() + 1200
            while not (out / 'approve-review').exists():
                assert time.monotonic() < until, 'Source review approval timed out'
                await asyncio.sleep(2)
            assert (out / 'approve-review').read_text().strip() == digest, 'Approval does not match generated source'
            assert await project.read('main.c') == values['main.c'], 'Source changed during review'
            await control('accept')
            await wait_phase('done', 60)
            assert await project.read('state.txt') == 'Done'
            await capture('board', 'kanban-done.png')
            await control('close')
            for _ in range(30):
                if await read('result.txt') == 'AGENT_WORKFLOW_PASS':
                    launched = False
                    break
                await asyncio.sleep(1)
            assert not launched, 'Native harness did not finish cleanly'
            (out / 'passed.json').write_text(json.dumps({'fixture': stage, 'task': task,
                'provider': 'codex', 'installed_proxy': True, 'native_queue': True,
                'native_build_run': True, 'exactly_one_execution': True,
                'review_accepted': True, 'done': True, 'source_sha256': digest,
                'program_crc32': f'{checksum[0]:08X}', 'program_size': checksum[1]}, indent=2))
            print('Real Codex -> Amiga build/run -> review -> Done passed.', flush=True)
        finally:
            if launched:
                # Stop only this fixture; its harness cooperatively cancels
                # its own worker. Never signal other applications or workers.
                await control('close')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', default='http://127.0.0.1:55110/mcp')
    parser.add_argument('--drawer', default='SD032G:Dev/Termic/Acceptance')
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--compiler-config', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    asyncio.run(accept(parser.parse_args()))
