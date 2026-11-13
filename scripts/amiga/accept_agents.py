#!/usr/bin/env python3
"""Opt-in MUI profile/MCP acceptance in an isolated installation on the Amiga.

Closes production normally, saves its window geometry, keeps ENV backup on the
Amiga only, and restores ENV/assign/application in finally. Never enters a key.
"""
import argparse, asyncio, json, re, subprocess, sys, time, urllib.parse, urllib.request
from pathlib import Path
from mcp import ClientSession
from mcp.client.streamable_http import streamablehttp_client
ROOT=Path(__file__).resolve().parents[2]
PROD='SD032G:Dev/Termic'
async def main(args):
    out=ROOT/'.context/agents-acceptance';out.mkdir(parents=True,exist_ok=True)
    (out/'passed.json').unlink(missing_ok=True)
    fixture=PROD+'/Acceptance/agents-'+str(int(time.time()))
    async with streamablehttp_client('http://127.0.0.1:55010/mcp') as (r,w,_):
        async with ClientSession(r,w) as s:
            await s.initialize()
            async def call(n,a):
                r=await s.call_tool(n,a);t='\n'.join(getattr(x,'text','') for x in r.content)
                with (out/'evidence.log').open('a') as f:f.write(n+' '+t+'\n')
                assert not r.isError and 'ERROR:' not in t and '[ERROR]' not in t and '[STILL RUNNING]' not in t,t
                await asyncio.sleep(.2);return t
            async def shell(script):return await call('amiga_run_script',{'script':'FailAt 10\nStack 262144\n'+script,'timeout':15})
            async def push(local,remote):await call('amiga_push_file',{'local_path':str(Path(local).resolve()),'amiga_path':remote})
            async def state(title='',front=False):
                t=await shell(PROD+'/ChromeProbe '+('front' if front else 'query')+(' "'+title+'"' if title else ''))
                p=dict(re.findall(r'(\w+)=([0-9a-f]+)',t));assert 'address' in p,t
                return {k:int(v,16 if k=='address' else 10) for k,v in p.items()}
            async def click(x,y,title=''):
                for _ in range(4):
                    p=await state(title);assert p['address'] and p['active'] and p['front'],p
                    if abs(p['mx']-x)<=1 and abs(p['my']-y)<=1:break
                    await call('amiga_input_mouse_move',{'dx':x-p['mx'],'dy':y-p['my']})
                else:raise AssertionError('Pointer did not reach target')
                try:await call('amiga_input_click',{'button':'left','direction':'down'})
                finally:await call('amiga_input_click',{'button':'left','direction':'up'})
                await asyncio.sleep(.6)
            async def capture(title,name):
                query='?'+urllib.parse.urlencode({'window':title}) if title else ''
                for attempt in range(2):
                    try:
                        with urllib.request.urlopen('http://127.0.0.1:55010/api/screenshot'+query,timeout=60) as r:d=json.load(r)
                        break
                    except TimeoutError:
                        if attempt:raise
                        await asyncio.sleep(2)
                subprocess.run(['sips','-s','format','png',d['path'],'--out',str(out/name)],check=True,capture_output=True)
            async def wait_main():
                for _ in range(20):
                    p=await state()
                    if p['address']:return p
                    await asyncio.sleep(1)
                raise AssertionError('Main window did not open')
            async def stop_candidate():
                # Exercise normal MUI cleanup rather than an external break signal.
                for title in ['Termic: Agents & Terminals','Termic: Agent access']:
                    p=await state(title)
                    if p['address']:
                        await state(title,front=True)
                        await click(p['x']+10,p['y']+8,title)
                p=await state(front=True)
                if p['address']:await click(p['x']+20,p['y']+20)
                for _ in range(15):
                    if not (await state())['address']:return
                    await asyncio.sleep(1)
                raise AssertionError('Candidate did not close; retain fixture assign')
            await push(args.probe,PROD+'/ChromeProbe');await shell('Protect '+PROD+'/ChromeProbe +e')
            await state(front=True);original=await state();assert original['address'] and original['active'],original
            (out/'original.json').write_text(json.dumps(original))
            await click(original['x']+20,original['y']+20)
            assert not (await state())['address'],'Production did not close normally'
            env=False;prepared=False;launched=False
            try:
                await shell('If NOT EXISTS '+PROD+'/Acceptance\n MakeDir '+PROD+'/Acceptance\nEndIf\nMakeDir '+fixture+'\nMakeDir '+fixture+'/Config\nMakeDir '+fixture+'/Projects\nMakeDir '+fixture+'/Projects/fixture')
                await shell('MakeDir '+fixture+'/Project')
                env='ENV_EXISTS' in await shell('If EXISTS ENV:Termic\n Echo ENV_EXISTS\n Copy ENV:Termic '+fixture+'/EnvBackup ALL\nEndIf')
                prepared=True
                await shell('If EXISTS ENV:Termic\n Delete ENV:Termic ALL\nEndIf\nMakeDir ENV:Termic\nAssign Termic: '+fixture)
                for name,txt in [('projects-drawer',fixture+'/Projects'),('model','fixture-model')]:
                    local=out/name;local.write_text(txt);await push(local,fixture+'/Config/'+name)
                await push(ROOT/'amiga/native/Project/main.c',fixture+'/Projects/fixture/main.c')
                await push(ROOT/'amiga/native/Project/main.c',fixture+'/Project/main.c')
                await push(args.gui,fixture+'/TermicProfilesTest')
                await push(args.worker,fixture+'/TermicAgent')
                await push(args.mcp,fixture+'/TermicMCP')
                await shell('Protect '+fixture+'/TermicProfilesTest +e\nProtect '+fixture+'/TermicAgent +e\nProtect '+fixture+'/TermicMCP +e\nRun >NIL: <NIL: '+fixture+'/TermicProfilesTest')
                launched=True;p=await wait_main()
                await call('amiga_window_move',{'window':format(p['address'],'08x'),'x':260,'y':75})
                await call('amiga_window_resize',{'window':format(p['address'],'08x'),'width':1400,'height':650})
                await state(front=True);p=await state()
                await click(p['x']+890,p['y']+54)
                # Positions pinned to the 1400x650 MUI acceptance layout.
                await click(p['x']+480,p['y']+130)
                title='Termic: Agents & Terminals';a=await state(title)
                assert a['address'] and a['active'],a
                await capture(title,'agents-general.png')
                # Native titlebar + header + add row. Save is at the lower left.
                await click(a['x']+100,a['y']+80,title)
                await click(a['x']+160,a['y']+a['height']-24,title)
                saved=out/'agents.json';await call('amiga_pull_file',{'amiga_path':fixture+'/Config/agents.json','local_path':str(saved)})
                profiles=json.loads(saved.read_text());assert len(profiles['profiles'])==3,profiles
                await state(front=True);p=await state();await click(p['x']+480,p['y']+130)
                a=await state(title)
                await click(a['x']+740,a['y']+80,title) # duplicate into a separate account/profile
                await click(a['x']+530,a['y']+a['height']-24,title) # Use only
                temporary=out/'agents-env.json';await call('amiga_pull_file',{'amiga_path':'ENV:Termic/agents.json','local_path':str(temporary)})
                assert len(json.loads(temporary.read_text())['profiles'])==4
                await call('amiga_pull_file',{'amiga_path':fixture+'/Config/agents.json','local_path':str(saved)})
                assert len(json.loads(saved.read_text())['profiles'])==3
                await state(front=True);p=await state();await click(p['x']+480,p['y']+130)
                a=await state(title)
                await click(a['x']+530,a['y']+80,title) # unsaved terminal
                await click(a['x']+a['width']-150,a['y']+a['height']-24,title) # Cancel
                await call('amiga_pull_file',{'amiga_path':'ENV:Termic/agents.json','local_path':str(temporary)})
                assert len(json.loads(temporary.read_text())['profiles'])==4
                await state(front=True);p=await state();await click(p['x']+480,p['y']+130)
                a=await state(title);await click(a['x']+550,a['y']+105,title);await capture(title,'agents-launch.png')
                await click(a['x']+a['width']-150,a['y']+a['height']-24,title)
                await state(front=True);p=await state();await click(p['x']+890,p['y']+130)
                title='Termic: Agent access';a=await state(title);assert a['address'],a
                await capture(title,'agent-access.png')
                await click(a['x']+464,a['y']+92,title)
                await click(a['x']+150,a['y']+a['height']-34,title)
                assert not (await state(title))['address'],'Access Save did not close the window'
                enabled=out/'mcp-enabled';await call('amiga_pull_file',{'amiga_path':fixture+'/Config/mcp-enabled','local_path':str(enabled)})
                assert enabled.read_text()=='1'
                # Exercise real native MCP without an API request or reading credentials.
                local=out/'enabled';local.write_text('1');await push(local,fixture+'/Config/mcp-enabled')
                async def rpc(tool,arguments):
                    local=out/'request.json';local.write_text(json.dumps({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':tool,'arguments':arguments}})+'\n')
                    await push(local,fixture+'/request.json')
                    await shell('Termic:TermicMCP --request '+fixture+'/request.json --reply '+fixture+'/reply.json')
                    reply=out/'reply.json';await call('amiga_pull_file',{'amiga_path':fixture+'/reply.json','local_path':str(reply)})
                    data=json.loads(reply.read_text());assert 'result' in data,data;return data['result']
                result=await rpc('task_new',{'project':'fixture','name':'mcp-test'});assert not result['isError'],result
                result=await rpc('file_write',{'project':'fixture','task':'mcp-test','path':'main.c','content':'int main(void) { return 0; }\n'});assert not result['isError'],result
                result=await rpc('scratchpad_write',{'project':'fixture','task':'mcp-test','pad':'notes','content':'Hardware MCP fixture'});assert not result['isError'],result
                result=await rpc('scratchpad_read',{'project':'fixture','task':'mcp-test','pad':'notes'});assert result['content'][0]['text']=='Hardware MCP fixture',result
                result=await rpc('file_read',{'project':'fixture','task':'mcp-test','path':'../../Config/api-key'});assert result['isError'],result
                # Run the native command profile through the same worker used by the launcher.
                registry=json.loads(temporary.read_text());cli=dict(registry['profiles'][0])
                cli.update(id='fixture-cli',name='Fixture CLI',kind=1,command='C:Get',args='TERMIC_FIXTURE',env='TERMIC_FIXTURE=amiga-cli-fixture',resume_args='',yolo='')
                registry['profiles'].append(cli)
                cli_registry=out/'cli-agents.json';cli_registry.write_text(json.dumps(registry))
                await push(cli_registry,'ENV:Termic/agents.json')
                selected=out/'agent-profile';selected.write_text('fixture-cli');await push(selected,fixture+'/Projects/fixture/Tasks/mcp-test/agent-profile')
                await shell('Termic:TermicAgent --agent --project '+fixture+'/Projects/fixture/Tasks/mcp-test >'+fixture+'/cli.log')
                cli_log=out/'cli.log';await call('amiga_pull_file',{'amiga_path':fixture+'/cli.log','local_path':str(cli_log)})
                assert 'amiga-cli-fixture' in cli_log.read_text() and 'Finished with exit code 0' in cli_log.read_text()
                # Real stdio connector, including upstream MCP handshake and native dispatch.
                child=await asyncio.create_subprocess_exec(sys.executable,str(ROOT/'scripts/amiga/mcp_bridge.py'),stdin=asyncio.subprocess.PIPE,stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.PIPE)
                requests=[{'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-11-25','capabilities':{}}},
                          {'jsonrpc':'2.0','id':2,'method':'tools/list','params':{}},
                          {'jsonrpc':'2.0','id':3,'method':'tools/call','params':{'name':'project_list','arguments':{}}}]
                stdout,stderr=await asyncio.wait_for(child.communicate((''.join(json.dumps(r)+'\n' for r in requests)).encode()),90)
                assert child.returncode==0,stderr.decode()
                replies=[json.loads(line) for line in stdout.splitlines()]
                assert len(replies)==3 and len(replies[1]['result']['tools'])==15,replies
                assert not replies[2]['result']['isError'],replies
                (out/'bridge-replies.json').write_bytes(stdout)
                (out/'passed.json').write_text(json.dumps({'fixture':fixture,'profiles_saved':3,'native_mcp':True},indent=2))
            finally:
                if launched:await stop_candidate()
                if prepared:
                    await shell('If EXISTS ENV:Termic\n Delete ENV:Termic ALL\nEndIf')
                    if env:await shell('Copy '+fixture+'/EnvBackup ENV:Termic ALL\nDelete '+fixture+'/EnvBackup ALL')
                await shell('Assign Termic: '+PROD+'\nRun >NIL: <NIL: '+PROD+'/TermicNative')
                p=await wait_main()
                await call('amiga_window_move',{'window':format(p['address'],'08x'),'x':original['x'],'y':original['y']})
                await call('amiga_window_resize',{'window':format(p['address'],'08x'),'width':original['width'],'height':original['height']})
                await state(front=True)
                restored=await state();assert all(restored[k]==original[k] for k in ['x','y','width','height']),restored
                (out/'restored.json').write_text(json.dumps(restored))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['gui','worker','mcp','probe']:p.add_argument('--'+name,type=Path,required=True)
    asyncio.run(main(p.parse_args()))
