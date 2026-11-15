"""Opt-in native branch/dependency + host Git relay checks in a synthetic task.

Stop AI Proxy first. No cloud request, GitHub connection, push or PR is made.
The production assign/application stays unchanged; the host repository is local.
"""
import argparse
import asyncio
import json
from pathlib import Path
import tempfile
import time
import zlib
from codex_relay import Bridge, RelayLock, default_state, validate_job, dispatch_job, parser
from git_service import GitWorkspace, command

ROOT=Path(__file__).resolve().parents[2]


async def main(args):
    out=ROOT/'.context/task-services';out.mkdir(exist_ok=True)
    (out/'passed.json').unlink(missing_ok=True)
    fixture=args.projects_root.rstrip('/')+'/accept-services-'+str(int(time.time()))
    lock=RelayLock(default_state())
    try:
        async with Bridge('http://127.0.0.1:55010/mcp') as b:
            assert await b.read('request.json') is None
            async def write(name,text):
                local=out/name;local.write_text(text)
                await b.call('amiga_push_file',{'local_path':str(local),'amiga_path':fixture+'/'+name})
            async def read(name):
                local=out/('read-'+name.replace('/','-'))
                await b.call('amiga_pull_file',{'amiga_path':fixture+'/'+name,'local_path':str(local)})
                return local.read_text()
            async def wait_done():
                for _ in range(60):
                    check=await b.shell('If EXISTS '+fixture+'/.result\n Echo RESULT_READY\nEndIf')
                    if 'RESULT_READY' in check:return
                    await asyncio.sleep(.5)
                raise AssertionError('Native worker did not finish')
            await b.shell('MakeDir '+fixture)
            for name,binary in [('TermicAgent',args.worker),('TermicCheck',args.check),('ReviewFixture',args.review)]:
                await b.call('amiga_push_file',{'local_path':str(binary.resolve()),'amiga_path':fixture+'/'+name})
                raw=binary.read_bytes();checksum=await b.call('amiga_checksum',{'path':fixture+'/'+name})
                assert f'{zlib.crc32(raw):08X}' in checksum
            await b.shell('Protect '+fixture+'/TermicAgent +e\nProtect '+fixture+'/TermicCheck +e\nProtect '+fixture+'/ReviewFixture +e')
            await b.call('amiga_run_script',{'script':'FailAt 21\nStack 262144\n'+fixture+'/ReviewFixture '+fixture+' > '+fixture+'/review-test.log','timeout':60})
            assert 'Review fixture: PASS' in await read('review-test.log')
            print('Native review journal creation: PASS',flush=True)
            await b.call('amiga_run_script',{'script':'Stack 262144\n'+fixture+'/TermicCheck > '+fixture+'/dependencies.log','timeout':60})
            deps=await read('dependencies.log');assert 'dependency check: PASS' in deps,deps
            print(deps,flush=True)
            async def native(action):
                await write('service-run','Stack 262144\n'+fixture+'/TermicAgent '+action+' --project '+fixture+' > '+fixture+'/job.log\n')
                await b.shell('If EXISTS '+fixture+'/.result\n Delete '+fixture+'/.result\nEndIf\nRun >NIL: <NIL: Execute '+fixture+'/service-run')
                await wait_done()
                result=await read('.result');log=await read('job.log')
                assert result.strip()=='0',log
                return log
            await write('main.c','int main(void) { return 0; }\n')
            await native('--init')
            await b.call('amiga_run_script',{'script':'FailAt 10\nStack 262144\nCD '+fixture+'\nTermic:Tools/amiga-git/git config user.name Fixture\nTermic:Tools/amiga-git/git config user.email user@example.com','timeout':60})
            await write('commit.txt','Fixture baseline');await native('--commit')
            original=await native('--branches')
            branch=next(line.split('* ',1)[1].strip() for line in original.splitlines() if line.startswith('* '))
            await write('branch.txt','termic-fixture');await native('--branch')
            assert '* termic-fixture' in await native('--branches')
            await write('branch.txt',branch);await native('--checkout')
            assert '* '+branch in await native('--branches')
            print('Native branch create/list/switch: PASS',flush=True)
            config=dict(action='sync',repository='acme/fixture',branch='termic/fixture',base='main',title='Fixture sync',body='Synthetic test')
            with tempfile.TemporaryDirectory() as directory:
                state=Path(directory);workspace=GitWorkspace(state,config)
                repo=workspace.repo;repo.mkdir(parents=True)
                await command(['git','init','-b','main',repo])
                await workspace.git('config','user.name','Fixture')
                await workspace.git('config','user.email','user@example.com')
                (repo/'main.c').write_text('original host fixture\n')
                await workspace.git('add','main.c');await workspace.git('commit','-m','Fixture baseline')
                await workspace.git('remote','add','origin',workspace.url)
                await workspace.git('update-ref','refs/remotes/origin/main','HEAD')
                await write('.host-git-request',json.dumps(config))
                await write('notes.md','Synthetic task documentation\n')
                await write('service-run','Stack 262144\n'+fixture+'/TermicAgent --host-git --project '+fixture+' > '+fixture+'/job.log\n')
                await b.shell('Delete '+fixture+'/.result\nRun >NIL: <NIL: Execute '+fixture+'/service-run')
                job=None
                try:
                    for _ in range(40):
                        raw=await b.read('request.json')
                        if raw:break
                        await asyncio.sleep(.5)
                    else:raise AssertionError('Host Git request was not published')
                    job=validate_job(json.loads(raw));assert job['provider']=='git' and job['readonly']
                    options=parser().parse_args([]);options.state_dir=state
                    assert await dispatch_job(b,job,options),'Git relay failed'
                    await wait_done();assert (await read('.result')).strip()=='0',await read('job.log')
                    assert (workspace.tree/'main.c').read_text()=='int main(void) { return 0; }\n'
                    assert (workspace.tree/'notes.md').read_text()=='Synthetic task documentation\n'
                    assert (repo/'main.c').read_text()=='original host fixture\n'
                    assert not (await workspace.git('status','--porcelain',tree=True)).strip()
                finally:
                    if job and await b.read('request.json') is not None:
                        from codex_relay import Mailbox
                        await Mailbox(b,job).send('done',status='failed')
            (out/'passed.json').write_text(json.dumps({'native_dependencies':True,'native_branches':True,'native_to_host_git_sync':True,'fixture':fixture},indent=2))
            print('Native request -> host dispatcher -> Amiga source reads -> isolated worktree commit: PASS',flush=True)
    finally:lock.close()


if __name__=='__main__':
    options=argparse.ArgumentParser(description=__doc__)
    options.add_argument('--projects-root',default='TMGIT:Projects',help='Existing mounted long-filename fixture volume; never formatted by this test')
    options.add_argument('--worker',type=Path,required=True)
    options.add_argument('--check',type=Path,required=True)
    options.add_argument('--review',type=Path,required=True)
    asyncio.run(main(options.parse_args()))
