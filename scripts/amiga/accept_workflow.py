"""Written hardware checks for instructions, prompt library and keyboard UI.
Called inside accept_agents.py's isolated installation and restoration guard.
"""
import asyncio,json,subprocess
async def stage(root,out,fixture,shell,push,call):
    for directory in ['Template','Template/.agents','Template/.agents/skills',
        'Template/.agents/skills/termic','Template/.agents/skills/termic/references','Prompts']:
        await shell('MakeDir '+fixture+'/'+directory)
    template=root/'amiga/native/Project'
    for local in template.rglob('*'):
        if local.is_file() and (local.name in ['AGENTS.md','CLAUDE.md'] or '.agents' in local.parts):
            await push(local,fixture+'/Template/'+str(local.relative_to(template)))
    for name in ['defaults.json','provenance.json']:
        await push(root/'amiga/native/Prompts'/name,fixture+'/Prompts/'+name)
    selected=out/'last-workspace';selected.write_text(fixture+'/Projects/fixture')
    await push(selected,fixture+'/Config/last-workspace')
    custom=out/'fixture-agents.md';custom.write_text('Fixture project instructions: preserve this file.\n')
    await push(custom,fixture+'/Projects/fixture/AGENTS.md')
    for _ in range(2):
        log=await shell('Termic:TermicAgent --instructions --project '+fixture+'/Projects/fixture')
        assert 'existing files preserved' in log,log
    await shell('Delete '+fixture+'/Projects/fixture/.result')
    copy=out/'guidance.md'
    await call('amiga_pull_file',{'amiga_path':fixture+'/Projects/fixture/AGENTS.md','local_path':str(copy)})
    assert copy.read_bytes()==custom.read_bytes()
    await call('amiga_pull_file',{'amiga_path':fixture+'/Projects/fixture/.agents/skills/termic/SKILL.md','local_path':str(copy)})
    assert copy.read_bytes()==(template/'.agents/skills/termic/SKILL.md').read_bytes()

async def exercise(root,out,fixture,shell,push,call,state,click,capture):
    async def key(code,mods=()):
        try:
            for mod in mods:await call('amiga_input_key',{'rawkey':mod,'direction':'down'})
            await call('amiga_input_key',{'rawkey':code,'direction':'down'})
        finally:
            await call('amiga_input_key',{'rawkey':code,'direction':'up'})
            for mod in reversed(mods):await call('amiga_input_key',{'rawkey':mod,'direction':'up'})
        await asyncio.sleep(.2)
    async def pull(name):
        local=out/('keyboard-'+name.replace('/','-'));local.unlink(missing_ok=True)
        await call('amiga_pull_file',{'amiga_path':fixture+'/Projects/fixture/'+name,'local_path':str(local)})
        return local.read_text()
    await key('50') # F1: prompt
    await key('32') # x
    await key('26',('63',)) # Ctrl J: newline
    await key('15') # y
    await key('42') # Tab: send, deliberately no credentials in fixture
    for _ in range(15):
        result=await shell('If EXISTS '+fixture+'/Projects/fixture/.result\n Echo RESULT_READY\nEndIf')
        if 'RESULT_READY' in result:break
        await asyncio.sleep(1)
    else:raise AssertionError('Tab did not launch the worker')
    assert (await pull('last-prompt.txt')).strip()=='x\ny'
    assert 'Configure Config/api-key' in await pull('job.log') # stopped before network
    await asyncio.sleep(2)
    await key('50');await key('31') # edit with z
    # Double Escape is sent as one script-free key burst; bridge overhead can
    # exceed the system double-click time, so restoration is also unit-scoped.
    await key('51');await key('42');await key('57') # editor Tab, F8 save
    assert 'Configure Config/api-key' in await pull('job.log') # no new agent job
    await key('50');await key('20',('67',));await key('41') # select all/delete prompt
    await key('3a') # slash: commands
    a=await state('Termic: Commands');assert a['address'],a
    await capture('Termic: Commands','commands.png');await key('45')
    await key('3a',('60',)) # ?: shortcut reference
    a=await state('Termic: Keyboard shortcuts');assert a['address'],a
    await capture('Termic: Keyboard shortcuts','keyboard-shortcuts.png');await key('45')
    assert not (await state('Termic: Keyboard shortcuts'))['address']
    await key('14',('63',));await key('52') # Ctrl T and F3 override native F3 in transcript
    assert (await state())['address'], 'Main closed during transcript search shortcuts'
    await key('50')
    assert (await state())['address'], 'Main closed returning to prompt'
    await key('19',('67',)) # Right Amiga P: library
    title='Termic: Prompts';a=await state(title);assert a['address'],a
    await capture(title,'prompts.png')
    await click(a['x']+108,a['y']+a['height']-24,title) # Insert + Use
    assert not (await state(title))['address'],'Insert + Use did not close'
    await key('57')
    body=await pull('task.txt')
    assert '# Review guidelines:' in body and 'Amiga execution context:' in body,body[:160]
    await key('19',('67',));a=await state(title)
    await click(a['x']+32,a['y']+a['height']-24,title) # Save
    local=out/'prompts-saved.json'
    await call('amiga_pull_file',{'amiga_path':fixture+'/Config/prompts.json','local_path':str(local)})
    delta=json.loads(local.read_text());assert len(delta['order'])==17 and not delta['deletedBuiltins'] and not delta['overrides'],delta
    await key('19',('67',));a=await state(title);assert a['address']
    await click(a['x']+70,a['y']+75,title) # Duplicate Review into a custom prompt
    await click(a['x']+32,a['y']+a['height']-24,title)
    assert not (await state(title))['address'], 'Save after duplicate did not close'
    await call('amiga_pull_file',{'amiga_path':fixture+'/Config/prompts.json','local_path':str(local)})
    delta=json.loads(local.read_text());assert len(delta['order'])==18 and len(delta['customs'])==1,delta
    await key('19',('67',));a=await state(title)
    await click(a['x']+108,a['y']+75,title) # Remove Review in the draft
    await key('45');assert not (await state(title))['address']
    await call('amiga_pull_file',{'amiga_path':fixture+'/Config/prompts.json','local_path':str(local)})
    assert not json.loads(local.read_text())['deletedBuiltins'],'Cancel persisted a draft removal'
    # End with main window active; the caller resumes the existing profile suite.
    p=await state(front=True)
    await capture('', 'workspace-icons-desktop.png')
    image=out/'workspace-icons-desktop.png'
    if image.exists():
        subprocess.run(['sips','--cropToHeightWidth',str(p['height']),str(p['width']),
            '--cropOffset',str(p['y']),str(p['x']),str(image),'--out',str(out/'workspace-icons.png')],check=True,capture_output=True)
    (out/'workflow-passed.json').write_text(json.dumps({'guidance':True,'tab_send':True,
        'editor_tab_does_not_send':True,'shortcut_dialog':True,'prompt_insert_save':True},indent=2))
