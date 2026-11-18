#!/usr/bin/env python3
"""Opt-in native MUI size matrix, buffer preservation and synthetic captures.
Run with test_responsive_window.c compiled against the GUI dependencies.
The production workspace may remain open; no user files or preferences change.
"""
import argparse,asyncio,json,re,shutil,subprocess,time
from pathlib import Path
from codex_relay import Bridge,PublicationUncertain
TITLES={'main':'Termic: MUI scaling demo','agents':'Termic: Agents, CLI and Scripts',
 'access':'Termic: Agent access','board':'Termic: Kanban board','queue':'Termic: Task queue','monitor':'Termic: Activity',
 'review':'Termic: Review agent changes','scratchpad':'Termic: Scratchpads','git':'Termic: Git branches and worktrees'}
async def main(args):
 out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
 stage='SD032G:Dev/Termic/Acceptance/scaling-'+str(int(time.time()))
 (out/'stage.json').write_text(json.dumps({'stage':stage}))
 async with Bridge(args.server,'SD032G:Dev/Termic/Relay') as b:
  assert 'TermicJob.' not in await b.call('amiga_list_ports',{}),'Worker active'
  assert await b.read('request.json') is None,'Relay busy'
  root=Bridge(args.server,stage);root.session=b.session
  await b.shell('MakeDir '+stage)
  try:await b.call('amiga_push_file',{'local_path':str(args.binary.resolve()),'amiga_path':stage+'/ScalingCheck'})
  except (ConnectionError,TimeoutError):pass
  await root.verify_file('ScalingCheck',args.binary.read_bytes())
  await b.call('amiga_push_file',{'local_path':str(args.probe.resolve()),'amiga_path':stage+'/ChromeProbe'})
  await root.verify_file('ChromeProbe',args.probe.read_bytes())
  await b.shell('Protect '+stage+'/ChromeProbe +e\nProtect '+stage+'/ScalingCheck +e\nEcho "Stack 262144" >'+stage+'/launch\nEcho "'+stage+'/ScalingCheck '+stage+'" >>'+stage+'/launch')
  try:await b.call('amiga_launch',{'command':'Execute '+stage+'/launch'})
  except (ConnectionError,TimeoutError):pass
  async def wait(name,predicate,seconds=600):
   until=time.monotonic()+seconds
   while time.monotonic()<until:
    values=await root.read_many(name,'result.txt')
    assert not (values['result.txt'] or '').startswith('SCALING_FAIL'),values['result.txt']
    if values[name] and predicate(values[name]):return values[name]
    await asyncio.sleep(1)
   raise TimeoutError('Waiting for '+name)
  async def control(frame):
   try:await root.write('control.json',frame)
   except PublicationUncertain:pass
  async def capture(panel,name):
   for _ in range(3):
    try:
     await b.shell(stage+'/ChromeProbe front "'+TITLES[panel]+'"')
     await asyncio.sleep(1)
     before=await b.shell(stage+'/ChromeProbe query "'+TITLES[panel]+'"')
     if 'front=1' not in before:continue
     reply=await b.session.call_tool('amiga_screenshot',{'window':TITLES[panel]})
     raw='\n'.join(getattr(c,'text','') for c in reply.content)
     match=re.search(r'Screenshot saved: (.+) \((\d+)x(\d+), (RLE|truecolor), (\d+) rows received\)',raw)
     after=await b.shell(stage+'/ChromeProbe query "'+TITLES[panel]+'"')
     if not reply.isError and match and match[3]==match[5] and 'front=1' in after:
      source=Path(match[1]);target=out/name
      if source.suffix=='.png':shutil.copyfile(source,target)
      else:subprocess.run(['sips','-s','format','png',str(source),'--out',str(target)],check=True,capture_output=True)
      print('Captured '+name,flush=True);return
    except (ConnectionError,TimeoutError) as error:
     print('Retrying capture '+name+': '+str(error),flush=True)
     await asyncio.sleep(1)
   raise AssertionError('Incomplete capture '+name)
  try:
   await wait('ready.txt',lambda text:text=='MATRIX_PASS');print('Native size/page matrix passed',flush=True)
   states=[]
   for panel,width,height,view,shot in [
    ('main',340,280,1,'editor-340x280.png'),('main',340,280,0,'agent-340x280.png'),
    ('main',340,280,5,'projects-340x280.png'),('main',340,280,6,'tasks-340x280.png'),('main',340,280,7,''),('main',340,280,8,'cli-340x280.png'),('main',340,280,4,'settings-340x280.png'),
    ('main',640,480,1,''),('main',1024,800,1,'workspace-1024x800.png'),
    ('main',1024,800,0,'agent-1024x800.png'),
    *[(p,340,280,1,'') for p in ('agents','access','board','queue','review','scratchpad','git','monitor')]]:
    frame=dict(panel=panel,width=width,height=height,view=view)
    await control(frame)
    raw=await wait('state.json',lambda s:all(json.loads(s).get(k)==v for k,v in frame.items()))
    state=json.loads(raw);assert state['preserved'];states.append(state)
    print('Verified '+panel+' '+str(width)+'x'+str(height),flush=True)
    if shot:await capture(panel,shot)
   (out/'geometry.json').write_text(json.dumps(states,indent=2))
   await control({'panel':'close'})
   await wait('result.txt',lambda s:s.startswith('SCALING_PASS'))
   (out/'passed.json').write_text(json.dumps({'size_page_matrix':True,'unsaved_source_preserved':True,
       'native_window_controls':True,'auxiliary_windows_shrink':True,'fixture':stage},indent=2))
   print('Responsive MUI acceptance passed',flush=True)
  finally:
   if not (await root.read('result.txt')):await control({'panel':'close'})
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True)
 p.add_argument('--probe',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--server',default='http://127.0.0.1:55010/mcp')
 asyncio.run(main(p.parse_args()))
