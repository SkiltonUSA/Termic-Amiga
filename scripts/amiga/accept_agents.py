#!/usr/bin/env python3
"""Opt-in MUI profile/MCP acceptance in an isolated installation on the Amiga.

Closes production normally, saves its window geometry, keeps ENV backup on the
Amiga only, and restores ENV/assign/application in finally. Never enters a key.
"""
import argparse, asyncio, json, re, shutil, subprocess, sys, time, zlib
from codex_relay import Bridge, decode_checksum
from pathlib import Path
import accept_workflow
from bridge_client import ClientSession
from mcp.client.streamable_http import streamablehttp_client
ROOT=Path(__file__).resolve().parents[2]
PROD='SD032G:Termic-Amiga'
async def main(args):
    for name in ('gui', 'worker', 'mcp', 'probe', 'micropython'):
        if not getattr(args, name).is_file():
            raise FileNotFoundError('Missing acceptance input: ' + str(getattr(args, name)))
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    for name in ('passed.json','editor-workflow-passed.json','restored.json'):
        (out/name).unlink(missing_ok=True)
    fixture=PROD+'/Acceptance/agents-'+str(int(time.time()))
    async with streamablehttp_client('http://127.0.0.1:55110/mcp') as (r,w,_):
        async with ClientSession(r,w) as s:
            await s.initialize()
            async def call(n,a):
                r=await s.call_tool(n,a);t='\n'.join(getattr(x,'text','') for x in r.content)
                with (out/'evidence.log').open('a') as f:f.write(n+' '+t+'\n')
                assert not r.isError and 'ERROR:' not in t and '[ERROR]' not in t and '[STILL RUNNING]' not in t and 'Cannot read remote file' not in t,t
                await asyncio.sleep(.2);return t
            async def shell(script,timeout=15):return await call('amiga_run_script',{'script':'FailAt 10\nStack 262144\n'+script,'timeout':timeout})
            async def push(local,remote):
                await call('amiga_push_file',{'local_path':str(Path(local).resolve()),'amiga_path':remote})
                data=Path(local).read_bytes()
                actual=decode_checksum(await call('amiga_checksum',{'path':remote}),remote)
                assert actual==(zlib.crc32(data),len(data)),'Upload checksum mismatch: '+remote
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
            async def menu(rawkey):
                await state(front=True)
                try:
                    await call('amiga_input_key',{'rawkey':'67','direction':'down'})
                    await call('amiga_input_key',{'rawkey':rawkey,'direction':'down'})
                finally:
                    await call('amiga_input_key',{'rawkey':rawkey,'direction':'up'})
                    await call('amiga_input_key',{'rawkey':'67','direction':'up'})
                title={'15':'Termic: Kanban board','26':'Termic: Task queue',
                       '16':'Termic: Review agent changes','28':'Termic: Scratchpads',
                       '24':'Termic: Git branches and worktrees'}.get(rawkey)
                if title:
                    # Native disk-backed dialogs may need several seconds to
                    # populate. Wait for the window, not a host-speed delay.
                    for _ in range(30):
                        if (await state(title))['address']:break
                        await asyncio.sleep(1)
                    else:raise AssertionError('Menu did not open '+title)
                await asyncio.sleep(.5)
            async def capture(title,name):
                if args.skip_screenshots:return
                for _ in range(3):
                    await state(title,front=True)
                    if not (await state(title))['front']:continue
                    text=await call('amiga_screenshot',{'window':title} if title else {})
                    match=re.search(r'Screenshot saved: (.+) \((\d+)x(\d+), (RLE|truecolor), (\d+) rows received\)',text)
                    if match and match[3]==match[5] and (await state(title))['front']:
                        source=Path(match[1]);target=out/name
                        if source.suffix=='.png':shutil.copyfile(source,target)
                        else:subprocess.run(['sips','-s','format','png',str(source),'--out',str(target)],check=True,capture_output=True)
                        return
                raise AssertionError('Incomplete or obscured screenshot: '+name)
            async def close_main():
                p=await state(front=True)
                if p['address']:
                    if p['border']>0:await click(p['x']+8,p['y']+p['border']//2)
                    else:await click(p['x']+p['width']-26,p['y']+57)
                for _ in range(45):
                    p=await state()
                    if not p['address'] and not p['control']:return
                    await asyncio.sleep(1)
                raise AssertionError('Workspace did not finish saving and closing')
            async def wait_main():
                for _ in range(20):
                    p=await state()
                    if p['address']:return p
                    await asyncio.sleep(1)
                raise AssertionError('Main window did not open')
            async def stop_candidate():
                # Exercise normal MUI cleanup rather than an external break signal.
                for title in ['Termic: Review agent changes','Termic: Scratchpads','Termic: Git branches and worktrees','Termic: Kanban board','Termic: Task queue','Termic: Commands','Termic: Keyboard shortcuts','Termic: Prompts','Termic: Agents, CLI and Scripts','Termic: Agent access']:
                    p=await state(title)
                    if p['address']:
                        await state(title,front=True)
                        await click(p['x']+10,p['y']+8,title)
                await close_main()
            assert 'TermicJob.' not in await call('amiga_list_ports',{}),'Worker active'
            relay=Bridge('',PROD+'/Relay');relay.session=s
            assert await relay.read('request.json') is None,'Relay busy'
            await push(args.probe,PROD+'/ChromeProbe');await shell('Protect '+PROD+'/ChromeProbe +e')
            await state(front=True);original=await state();assert original['address'] and original['active'],original
            (out/'original.json').write_text(json.dumps(original))
            env=False;prepared=False;launched=False
            try:
                await close_main()
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
                await shell('MakeDir '+fixture+'/Tools\nMakeDir '+fixture+'/Tools/MicroPython\nMakeDir '+fixture+'/Examples')
                runtime=await call('amiga_checksum',{'path':PROD+'/Tools/MicroPython/micropython'})
                if f'{zlib.crc32(args.micropython.read_bytes()):08X}' in runtime:
                    await shell('Copy '+PROD+'/Tools/MicroPython/micropython '+fixture+'/Tools/MicroPython/micropython')
                else:await push(args.micropython,fixture+'/Tools/MicroPython/micropython')
                await push(ROOT/'amiga/native/Examples/hello.py',fixture+'/Examples/hello.py')
                await push(ROOT/'amiga/native/Examples/hello.rexx',fixture+'/Examples/hello.rexx')
                await shell('Protect '+fixture+'/Tools/MicroPython/micropython +e')
                if args.install_kit:
                    import accept_install
                    await accept_install.stage(ROOT,out,fixture,args.install_kit,shell,push,call)
                if not (args.kanban_only or args.features_only or args.editor_workflow_only):
                    await accept_workflow.stage(ROOT,out,fixture,shell,push,call)
                if args.kanban_only or args.features_only:
                    import accept_kanban
                    if not args.features_only:
                        await accept_kanban.stage(ROOT,out,fixture,shell,push,call)
                    if args.features:
                        import accept_features
                        await accept_features.stage(ROOT,out,fixture,shell,push,call)
                if args.monitor_task:
                    import accept_monitor_task
                    await accept_monitor_task.stage(ROOT,out,fixture,shell,push,call)
                if args.editor_workflow_only:
                    import accept_editor_workflow
                    await accept_editor_workflow.stage(ROOT,out,fixture,shell,push,call)
                launched=True
                await shell('Protect '+fixture+'/TermicProfilesTest +e\nProtect '+fixture+'/TermicAgent +e\nProtect '+fixture+'/TermicMCP +e\nRun >NIL: <NIL: '+fixture+'/TermicProfilesTest')
                p=await wait_main()
                await call('amiga_window_move',{'window':format(p['address'],'08x'),'x':260,'y':75})
                await call('amiga_window_resize',{'window':format(p['address'],'08x'),'width':1400,'height':650})
                await state(front=True);p=await state()
                if args.kanban_only or args.features_only:
                    import accept_singleton
                    await accept_singleton.exercise(ROOT,out,fixture,shell,push,call,state,args.gui)
                if args.kanban_only or args.features_only:
                    import accept_kanban
                    if not args.features_only:
                        await accept_kanban.exercise(ROOT,out,fixture,shell,push,call,state,click,capture,menu)
                    if args.features:
                        import accept_features
                        await accept_features.exercise(ROOT,out,fixture,shell,push,call,state,click,capture,menu)
                    if args.monitor_task:
                        import accept_monitor_task
                        await accept_monitor_task.exercise(ROOT,out,fixture,shell,push,call,state,click,capture,menu,args)
                    return
                if args.editor_workflow_only:
                    await accept_editor_workflow.exercise(ROOT,out,fixture,shell,push,call,state,click,capture)
                    return
                if args.live_feedback_only:
                    import accept_live_feedback
                    await accept_live_feedback.exercise(ROOT,out,fixture,shell,push,call,state,click,capture,menu)
                    return
                if args.cli_panel_only:
                    import accept_cli_panel
                    await accept_cli_panel.exercise(ROOT,out,fixture,shell,push,call,state,click,capture)
                    return
                await accept_workflow.exercise(ROOT,out,fixture,shell,push,call,state,click,capture)
                await menu('12') # Right Amiga E: agent profiles
                title='Termic: Agents, CLI and Scripts';a=await state(title)
                assert a['address'] and a['active'],a
                await capture(title,'agents-general.png')
                # Native titlebar + header + add row. Save is at the lower left.
                await click(a['x']+100,a['y']+80,title)
                await click(a['x']+30,a['y']+a['height']-24,title)
                saved=out/'agents.json';await call('amiga_pull_file',{'amiga_path':fixture+'/Config/agents.json','local_path':str(saved)})
                profiles=json.loads(saved.read_text());assert len(profiles['profiles'])==3,profiles
                await menu('12')
                a=await state(title)
                await click(a['x']+740,a['y']+80,title) # duplicate into a separate account/profile
                await click(a['x']+68,a['y']+a['height']-24,title) # Use only
                temporary=out/'agents-env.json';await call('amiga_pull_file',{'amiga_path':'ENV:Termic/agents.json','local_path':str(temporary)})
                assert len(json.loads(temporary.read_text())['profiles'])==4
                await call('amiga_pull_file',{'amiga_path':fixture+'/Config/agents.json','local_path':str(saved)})
                assert len(json.loads(saved.read_text())['profiles'])==3
                await menu('12')
                a=await state(title)
                await click(a['x']+530,a['y']+80,title) # unsaved MicroPython profile
                await click(a['x']+106,a['y']+a['height']-24,title) # Cancel
                await call('amiga_pull_file',{'amiga_path':'ENV:Termic/agents.json','local_path':str(temporary)})
                assert len(json.loads(temporary.read_text())['profiles'])==4
                await menu('12')
                a=await state(title);await click(a['x']+550,a['y']+105,title);await capture(title,'agents-launch.png')
                await click(a['x']+106,a['y']+a['height']-24,title)
                await menu('37')
                title='Termic: Agent access';a=await state(title);assert a['address'],a
                await capture(title,'agent-access.png')
                await click(a['x']+464,a['y']+92,title)
                await click(a['x']+32,a['y']+a['height']-34,title)
                assert not (await state(title))['address'],'Access Save did not close the window'
                enabled=out/'mcp-enabled';await call('amiga_pull_file',{'amiga_path':fixture+'/Config/mcp-enabled','local_path':str(enabled)})
                assert enabled.read_text()=='1'
                # Exercise real native MCP without an API request or reading credentials.
                local=out/'enabled';local.write_text('1');await push(local,fixture+'/Config/mcp-enabled')
                async def rpc(tool,arguments):
                    local=out/'request.json';local.write_text(json.dumps({'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':tool,'arguments':arguments}})+'\n')
                    await push(local,fixture+'/request.json')
                    await shell('Termic:TermicMCP --request '+fixture+'/request.json --reply '+fixture+'/reply.json')
                    reply=out/'reply.json';reply.unlink(missing_ok=True)
                    await call('amiga_pull_file',{'amiga_path':fixture+'/reply.json','local_path':str(reply)})
                    data=json.loads(reply.read_text());assert 'result' in data,data;return data['result']
                result=await rpc('task_new',{'project':'fixture','name':'mcp-test'});assert not result['isError'],result
                result=await rpc('file_write',{'project':'fixture','task':'mcp-test','path':'main.c','content':'int main(void) { return 0; }\n'});assert not result['isError'],result
                result=await rpc('scratchpad_write',{'project':'fixture','task':'mcp-test','pad':'notes','content':'Hardware MCP fixture'});assert not result['isError'],result
                result=await rpc('scratchpad_read',{'project':'fixture','task':'mcp-test','pad':'notes'});assert result['content'][0]['text']=='Hardware MCP fixture',result
                result=await rpc('file_read',{'project':'fixture','task':'mcp-test','path':'../../Config/api-key'});assert result['isError'],result
                # Compile on the real Amiga with absolute SDK paths. The test
                # config avoids changing VBCC/vinclude/vlib system assigns.
                sdk=PROD+'/SDK';binpath=sdk+'/vbcc_bin_amigaos68k/bin/'
                target=sdk+'/vbcc_target_m68k-amigaos/targets/m68k-amigaos/'
                config='\n'.join([
                    '-cc='+binpath+'vbccm68k -quiet %s -o= %s %s -O=%ld -I'+target+'include',
                    '-as='+binpath+'vasmm68k_mot -quiet -Fhunk -nowarn=62 %s -o %s',
                    '-rm=delete quiet %s',
                    '-ld='+binpath+'vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -mrel '+target+'lib/startup.o %s %s -L'+target+'lib -lvc -o %s',
                    '-ldnodb=-s -Rshort','-ul=-l%s','-cf=-F%s','-ml=500','-hunkdebug','-amiga-softfloat',''])
                config_file=out/'native-build.config';config_file.write_text(config)
                await push(config_file,fixture+'/native-build.config')
                setup=out/'Setup';setup.write_text('Stack 262144\n')
                await push(setup,fixture+'/Setup')
                await push(ROOT/'amiga/native/BuildProject',fixture+'/BuildProject')
                command=out/'build-command.txt';command.write_text(binpath+'vc +'+fixture+'/native-build.config -cpu=68020 -O1 main.c -o Program.new -lamiga\n')
                await push(command,fixture+'/Projects/fixture/Tasks/mcp-test/build-command.txt')
                async def build(expected):
                    r=await rpc('task_build',{'project':'fixture','task':'mcp-test'});assert not r['isError'],r
                    for _ in range(30):
                        r=await rpc('task_status',{'project':'fixture','task':'mcp-test'});assert not r['isError'],r
                        status=json.loads(r['content'][0]['text'])
                        if not status['busy'] and status['exit'].strip():break
                        await asyncio.sleep(1)
                    else:raise AssertionError('MCP build did not finish')
                    log=await rpc('task_log',{'project':'fixture','task':'mcp-test'})
                    assert int(status['exit'])==expected,(status,log)
                    (out/('build-success.json' if expected==0 else 'build-failure.json')).write_text(json.dumps(log))
                await build(0)
                program=out/'Program';await call('amiga_pull_file',{'amiga_path':fixture+'/Projects/fixture/Tasks/mcp-test/Program','local_path':str(program)})
                executable=program.read_bytes();assert executable[:4]==b'\x00\x00\x03\xf3'
                result=await rpc('file_write',{'project':'fixture','task':'mcp-test','path':'main.c','content':'#error intentional acceptance failure\n'});assert not result['isError'],result
                await build(20)
                program.unlink()
                await call('amiga_pull_file',{'amiga_path':fixture+'/Projects/fixture/Tasks/mcp-test/Program','local_path':str(program)})
                assert program.read_bytes()==executable,'Failed build replaced the previous executable'
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
                # Exercise script profiles and command-panel actions on native workers.
                for kind,action,executable,script,expected in [
                    (3,'--rexx','SYS:Rexxc/RX','hello.rexx','Hello from Termic ARexx'),
                    (4,'--python','Termic:Tools/MicroPython/micropython','hello.py','Hello from Termic MicroPython')]:
                    profile=dict(cli);profile.update(id='fixture-script',name='Fixture script',kind=kind,command=executable,args='Termic:Examples/'+script)
                    registry['profiles'][-1]=profile
                    cli_registry.write_text(json.dumps(registry));await push(cli_registry,'ENV:Termic/agents.json')
                    selected.write_text('fixture-script');await push(selected,fixture+'/Projects/fixture/Tasks/mcp-test/agent-profile')
                    for mode in ['--agent',action]:
                        cmd=out/'command.txt';cmd.write_text('Termic:Examples/'+script)
                        await push(cmd,fixture+'/Projects/fixture/Tasks/mcp-test/command.txt')
                        await shell('Termic:TermicAgent '+mode+' --project '+fixture+'/Projects/fixture/Tasks/mcp-test >'+fixture+'/script.log')
                        log=out/(script+mode+'.log');log.unlink(missing_ok=True)
                        await call('amiga_pull_file',{'amiga_path':fixture+'/script.log','local_path':str(log)})
                        assert expected in log.read_text() and 'Finished with exit code 0' in log.read_text(),log.read_text()
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
                if args.providers:
                    import accept_providers
                    await accept_providers.exercise(ROOT,out,fixture,shell,push,call,state,click,capture,menu,args)
                for provider in args.host_agent:
                    import accept_codex
                    await accept_codex.exercise(ROOT,out,fixture,shell,push,call,state,click,capture,menu,args,provider)
                if args.codex_relay:
                    import accept_codex
                    await accept_codex.exercise(ROOT,out,fixture,shell,push,call,state,click,capture,menu,args)
                (out/'passed.json').write_text(json.dumps({'fixture':fixture,'profiles_saved':3,'native_mcp':True,'native_build':True,'failed_build_preserves_program':True,'stdio_bridge':True,'arexx':True,'micropython':True,'workflow':True},indent=2))
            finally:
                if args.install_kit:
                    pending=await shell('If EXISTS '+fixture+'/install-started\n If NOT EXISTS '+fixture+'/install.result\n  Echo INSTALL_ACTIVE\n EndIf\nEndIf')
                    if 'INSTALL_ACTIVE' in pending:
                        raise RuntimeError('Installation may still be active; retain fixture assignments until inspected')
                if launched:
                    await stop_candidate()
                    # Closing the GUI does not kill an active native worker.
                    # Keep its installation assigned until all test jobs exit.
                    for _ in range(900):
                        if 'TermicJob.' not in await call('amiga_list_ports',{}):break
                        await asyncio.sleep(1)
                    else:raise RuntimeError('Test worker remains active; retain fixture assignments')
                if prepared:
                    await shell('If EXISTS ENV:Termic\n Delete ENV:Termic ALL\nEndIf')
                    if env:await shell('Copy '+fixture+'/EnvBackup ENV:Termic ALL\nDelete '+fixture+'/EnvBackup ALL')
                if args.install_kit:
                    await shell('Assign Termic: '+PROD+'\nIf EXISTS '+fixture+'/env-vbcc-saved\n Execute Termic:Setup\n If EXISTS '+fixture+'/env-vbcc\n  Copy '+fixture+'/env-vbcc ENV:VBCC\n Else\n  Delete ENV:VBCC\n EndIf\nEndIf')
                await shell('Assign Termic: '+PROD+'\nRun >NIL: <NIL: '+PROD+'/TermicNative')
                p=await wait_main()
                await call('amiga_window_move',{'window':format(p['address'],'08x'),'x':original['x'],'y':original['y']})
                await call('amiga_window_resize',{'window':format(p['address'],'08x'),'width':original['width'],'height':original['height']})
                await state(front=True)
                restored=await state();assert all(restored[k]==original[k] for k in ['x','y','width','height']),restored
                (out/'restored.json').write_text(json.dumps(restored))
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['gui','worker','mcp','probe','micropython']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--output',type=Path,default=ROOT/'.context/agents-acceptance')
    p.add_argument('--editor-workflow-only',action='store_true',help='Exercise native MUI editing, save/build/run, failure recovery and compact shortcuts')
    p.add_argument('--host-agent', action='append', choices=['claude','cursor'], default=[], help='Run an authenticated host agent read/write/build and resume test')
    p.add_argument('--providers', action='store_true', help='Exercise Claude/Cursor native profiles with synthetic MCP clients')
    p.add_argument('--live-feedback-only',action='store_true',help='Exercise live transcript, selection and tool feedback without cloud inference')
    p.add_argument('--cli-panel-only', action='store_true', help='Exercise the Files/CLI split, commands, output, focus and resizing only')
    p.add_argument('--codex-relay',action='store_true',help='Run an authenticated synthetic Codex read/write/build and resume test')
    p.add_argument('--monitor-task',action='store_true',help='Run authenticated process-monitor development through Kanban, then wait for local source audit')
    p.add_argument('--install-kit',type=Path,help='Optional fixture-only LHA without SDK archives, to validate fresh extraction and native builds')
    p.add_argument('--features-only',action='store_true',help='Stage the fixture but skip the already-tested queue execution')
    p.add_argument('--features',action='store_true',help='Also exercise native editor colouring, change review and scratchpads')
    p.add_argument('--kanban-only',action='store_true',help='Exercise queue persistence, failure recovery and the Kanban board without cloud inference')
    p.add_argument('--skip-screenshots',action='store_true')
    asyncio.run(main(p.parse_args()))
