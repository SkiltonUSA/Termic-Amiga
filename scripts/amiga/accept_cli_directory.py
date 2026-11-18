"""Verify native CLI file resolution, displayed directory and caller restoration.
Uses a fresh synthetic project and task; never reads or edits a user workspace.
"""
import argparse,asyncio,json,time
from pathlib import Path
from codex_relay import Bridge
from bridge_client import DEFAULT_BRIDGE

async def main(args):
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    fixture='SD032G:Dev/Termic/Acceptance/cli-directory-'+str(int(time.time()))
    (out/'fixture.json').write_text(json.dumps({'fixture':fixture}))
    async with Bridge(DEFAULT_BRIDGE,fixture) as b:
        assert 'TermicJob.' not in await b.call('amiga_list_ports',{})
        await b.shell('MakeDir '+fixture+'\nMakeDir '+fixture+'/Project\nMakeDir '+fixture+'/Project/Tasks\nMakeDir '+fixture+'/Project/Tasks/TaskA')
        async def put(relative,data):
            p=out/relative.replace('/','-');p.write_bytes(data.encode() if isinstance(data,str) else data)
            await b.call('amiga_push_file',{'local_path':str(p),'amiga_path':fixture+'/'+relative})
        await put('TermicAgent',args.worker.read_bytes());await b.verify_file('TermicAgent',args.worker.read_bytes())
        await b.shell('Protect '+fixture+'/TermicAgent +e')
        results=[]
        for sub,marker in [('Project','PROJECT_DIRECTORY_OK'),('Project/Tasks/TaskA','TASK_DIRECTORY_OK')]:
            root=fixture+'/'+sub;v=Bridge(DEFAULT_BRIDGE,root);v.session=b.session
            await put(sub+'/marker.txt',marker+'\n')
            for index,command in enumerate(['CD','Type marker.txt']):
                await put(sub+'/command.txt',command)
                launch=('CD "SD032G:Dev/Termic"\nStack 262144\n'+fixture+'/TermicAgent --shell --project "'+root+'" >'+root+'/job.log\n'
                        'CD >'+root+'/caller-directory.txt\nEcho complete >'+root+'/done\n')
                await put(sub+'/launch',launch)
                await b.shell('If EXISTS '+root+'/done\n Delete '+root+'/done\nEndIf\nRun >NIL: <NIL: Execute '+root+'/launch')
                for _ in range(80):
                    if await v.read('done'):break
                    await asyncio.sleep(.5)
                else:raise AssertionError('CLI worker did not finish')
                values=await v.read_many('job.log','.result','caller-directory.txt','.command.log')
                assert values['.result'].strip()=='0',values
                assert '> '+command in values['job.log'],values
                assert values['caller-directory.txt'].strip().lower().endswith(':dev/termic'),values
                header=next(line[len('Working directory: '):] for line in values['job.log'].splitlines() if line.startswith('Working directory: '))
                assert header.lower().endswith(sub.lower()),values
                if index==0:assert values['.command.log'].strip().lower()==header.lower(),values
                else:assert marker in values['.command.log'],values
                results.append({'workspace':sub,'command':command,'result':values})
                print(sub,command,'PASS',flush=True)
        (out/'passed.json').write_text(json.dumps({'fixture':fixture,'checks':results},indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--worker',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    asyncio.run(main(p.parse_args()))
