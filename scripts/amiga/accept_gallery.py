#!/usr/bin/env python3
"""Capture the production Amiga UI with isolated documentation data.
Compile tests/test_gallery_window.c with the GUI units, then pass that binary
and ChromeProbe. Uses the shared reservation-aware bridge. Never loads user
profiles or projects; no AI requests or process controls are issued.
"""
import argparse,asyncio,json,re,shutil,subprocess,time
from pathlib import Path
from codex_relay import Bridge,PublicationUncertain
async def main(args):
 out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
 stage='SD032G:Dev/Termic/Acceptance/gallery-'+str(int(time.time()))
 (out/'stage.json').write_text(json.dumps({'stage':stage}))
 async with Bridge(args.server,'SD032G:Dev/Termic/Relay',reservation_path=args.reservation_file) as b:
  assert 'TermicJob.' not in await b.call('amiga_list_ports',{}),'Worker active'
  assert await b.read('request.json') is None,'Relay busy'
  root=Bridge(args.server,stage);root.session=b.session
  await b.shell('MakeDir '+stage)
  try:await b.call('amiga_push_file',{'local_path':str(args.binary.resolve()),'amiga_path':stage+'/GalleryCheck'})
  except (ConnectionError,TimeoutError):pass
  await root.verify_file('GalleryCheck',args.binary.read_bytes())
  await b.call('amiga_push_file',{'local_path':str(args.probe.resolve()),'amiga_path':stage+'/ChromeProbe'})
  await root.verify_file('ChromeProbe',args.probe.read_bytes())
  await b.call('amiga_push_file',{'local_path':str(Path('amiga/native/Prompts/defaults.json').resolve()),'amiga_path':stage+'/defaults.json'})
  await root.verify_file('defaults.json',Path('amiga/native/Prompts/defaults.json').read_bytes())
  await b.call('amiga_push_file',{'local_path':str(Path('amiga/native/Termic.guide').resolve()),'amiga_path':stage+'/Termic.guide'})
  await root.verify_file('Termic.guide',Path('amiga/native/Termic.guide').read_bytes())
  await b.shell('Protect '+stage+'/ChromeProbe +e\nProtect '+stage+'/GalleryCheck +e\nEcho "Stack 262144" >'+stage+'/launch\nEcho "'+stage+'/GalleryCheck '+stage+'" >>'+stage+'/launch')
  try:await b.call('amiga_launch',{'command':'Execute '+stage+'/launch'})
  except (ConnectionError,TimeoutError):pass
  async def wait(name,predicate,seconds=600):
   until=time.monotonic()+seconds
   while time.monotonic()<until:
    values=await root.read_many(name,'result.txt')
    assert not (values['result.txt'] or '').startswith('GALLERY_FAIL'),values['result.txt']
    if values[name] and predicate(values[name]):return values[name]
    await asyncio.sleep(1)
   raise TimeoutError('Waiting for '+name)
  async def control(frame):
   try:await root.write('control.json',frame)
   except PublicationUncertain:pass
  async def capture(title,name):
   for _ in range(3):
    try:
     await b.shell(stage+'/ChromeProbe front "'+title+'"')
     await asyncio.sleep(1)
     before=await b.shell(stage+'/ChromeProbe query "'+title+'"')
     if 'front=1' not in before:continue
     reply=await b.session.call_tool('amiga_screenshot',{'window':title})
     raw='\n'.join(getattr(c,'text','') for c in reply.content)
     match=re.search(r'Screenshot saved: (.+) \((\d+)x(\d+), (RLE|truecolor), (\d+) rows received\)',raw)
     after=await b.shell(stage+'/ChromeProbe query "'+title+'"')
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
   await wait('ready.txt',lambda text:text=='GALLERY_READY');print('Documentation fixture ready',flush=True)
   states=[]
   started=not args.start_at
   for panel,width,height,view,shot in [
    ('main',1024,800,1,'workspace-cli.png'),
    ('main',1024,800,0,'live-feedback.png'),
    ('tiny',900,650,0,'tiny-profile.png'),
    ('agents',900,650,0,'agents.png'),
    ('codex',900,650,0,'codex-profile.png'),
    ('claude',900,650,0,'claude-profile.png'),
    ('cursor',900,650,0,'cursor-profile.png'),
    ('access',900,600,4,'agent-access.png'),
    ('main',1024,800,4,'preferences.png'),
    ('about',0,0,1,'about.png'),
    ('prompts',1024,650,0,'prompts.png'),
    ('shortcuts',0,0,0,'keyboard-shortcuts.png'),
    ('keymap',900,600,0,'shortcut-preferences.png'),
    ('board',1024,600,0,'kanban.png'),
    ('queue',900,550,0,'parallel-queue.png'),
    ('review',900,550,1,'review-diff.png'),
    ('broadcast',1000,600,0,'broadcast.png'),
    ('main',1024,800,9,'processors.png'),
    ('zz9000',1024,800,9,'zz9000.png'),
    ('main',340,280,1,'editor-340x280.png'),
    ('main',340,280,0,'tiny-340x280.png'),
    ('main',340,280,9,'processors-340x280.png'),
    ('guide',900,650,0,'guide.png'),('guide-tiny',900,650,0,'guide-tiny.png'),
    ('guide',340,280,0,'guide-340x280.png')]:
    if shot==args.start_at:started=True
    if not started or (args.only and shot!=args.only):continue
    frame=dict(panel=panel,width=width,height=height,view=view)
    await control(frame)
    raw=await wait('state.json',lambda s:all(json.loads(s).get(k)==v for k,v in frame.items()))
    state=json.loads(raw);assert state['source_preserved'] and state['tiny_default'];states.append(state)
    print('Verified '+panel+' '+str(width)+'x'+str(height),flush=True)
    if shot:await capture(state['title'],shot)
   (out/'geometry.json').write_text(json.dumps(states,indent=2))
   await control({'panel':'close'})
   await wait('result.txt',lambda s:s.startswith('GALLERY_PASS'))
   (out/'passed.json').write_text(json.dumps({'production_widgets':True,
       'synthetic_data':True,'source_preserved':True,'tiny_default':True,
       'fixture':stage,'captures':len(states)},indent=2))
   print('Documentation gallery capture passed',flush=True)
  finally:
   if not (await root.read('result.txt')):await control({'panel':'close'})
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--binary',type=Path,required=True)
 p.add_argument('--probe',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--server',default='http://127.0.0.1:55110/mcp')
 p.add_argument('--only',help='Capture just this screenshot filename')
 p.add_argument('--start-at',help='Start at this screenshot filename after a completed/closed earlier fixture')
 p.add_argument('--reservation-file',type=Path,help='Private recovery file from this runner only')
 asyncio.run(main(p.parse_args()))
