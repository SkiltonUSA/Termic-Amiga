"""Written Kanban/queue acceptance against isolated synthetic Amiga tasks."""
import asyncio
import json
import subprocess


async def stage(root, out, fixture, shell, push, call):
    async def write(name, text, target):
        local=out/('kanban-'+name);local.write_text(text);await push(local,target)
    await write('last-workspace',fixture+'/Projects/fixture',fixture+'/Config/last-workspace')
    base=fixture+'/Projects/fixture/Tasks'
    await shell('If NOT EXISTS '+base+'\n MakeDir '+base+'\nEndIf')
    for name, state in [('backlog','Ready'),('attention','Needs attention'),
                        ('review','Review'),('done','Done'),('archived','Archived'),
                        ('first','Ready'),('failure','Ready'),('last','Ready')]:
        await shell('If NOT EXISTS '+base+'/'+name+'\n MakeDir '+base+'/'+name+'\nEndIf\nEcho \"'+name.title()+'\" > '+base+'/'+name+'/name.txt\nEcho \"'+state+'\" > '+base+'/'+name+'/state.txt')
    await write('fail','1',base+'/failure/fail')
    # Queue test exercises real worker completion using a deterministic local build script.
    await write('build','Echo "Synthetic queue build started"\nWait 3\nIf EXISTS fail\n Quit 20\nEndIf\nEcho "Synthetic queue build passed"\nQuit 0\n',fixture+'/BuildProject')
    queue={'version':1,'paused':False,'items':[{'path':base+'/'+name,'build':True,'state':0}
                                            for name in ['first','failure','last']]}
    await write('queue',json.dumps(queue),fixture+'/Config/queue.json')


async def exercise(root,out,fixture,shell,push,call,state,click,capture,menu):
    report=out/'kanban-passed.json';report.unlink(missing_ok=True)
    board='Termic: Kanban board';queue='Termic: Task queue'
    async def key(code):
        try:await call('amiga_input_key',{'rawkey':code,'direction':'down'})
        finally:await call('amiga_input_key',{'rawkey':code,'direction':'up'})
        await asyncio.sleep(.4)
    async def read(path):
        local=out/'kanban-read'
        # queue.json is atomically replaced while jobs finish. A bridge transfer
        # can retain the preceding size; restart this read-only snapshot.
        for attempt in range(5):
            local.unlink(missing_ok=True)
            try:
                await call('amiga_pull_file',{'amiga_path':fixture+'/'+path,'local_path':str(local)})
                return local.read_text()
            except (AssertionError,FileNotFoundError):
                if attempt==4:raise
                await asyncio.sleep(1)

    async def persisted():return json.loads(await read('Config/queue.json'))
    async def wait_states(expected):
        for _ in range(120):
            value=await persisted()
            if [row['state'] for row in value['items']]==expected:return value
            await asyncio.sleep(1)
        raise AssertionError((expected,value))
    async def snap(title,name):
        # The screenshot endpoint already crops named windows.
        await asyncio.sleep(5)
        await capture(title,name+'.png')
    initial=await persisted()
    assert initial['paused'] and [r['state'] for r in initial['items']]==[0,0,0],initial
    await menu('15') # Right Amiga Y
    p=await state(board);assert p['address'],p
    await snap(board,'kanban-initial')
    # One Review fixture starts in this column. Its selection follows each move.
    await click(p['x']+630,p['y']+84,board)
    await snap(board,'kanban-review-selected')
    for x,target in [(604,'Done'),(690,'Archived'),(830,'Ready')]:
        await click(p['x']+x,p['y']+p['height']-34,board)
        for _ in range(20):
            value=(await read('Projects/fixture/Tasks/review/state.txt')).strip()
            if value==target:break
            await asyncio.sleep(1)
        else:
            await snap(board,'kanban-transition-failed')
            raise AssertionError((target,value))

    # Board close button remains right aligned.
    await click(p['x']+p['width']-38,p['y']+p['height']-24,board)
    await menu('26') # Right Amiga J: task queue
    p=await state(queue);assert p['address'],p
    await snap(queue,'queue-paused')
    await click(p['x']+130,p['y']+65,queue)
    await click(p['x']+675,p['y']+p['height']-68,queue)
    value=await persisted();assert [r['path'].rsplit('/',1)[-1] for r in value['items']]==['first','last','failure'],value
    await click(p['x']+130,p['y']+82,queue)
    await click(p['x']+640,p['y']+p['height']-68,queue)
    value=await persisted();assert [r['path'].rsplit('/',1)[-1] for r in value['items']]==['first','failure','last'],value

    await key('13') # R: Resume
    value=await wait_states([3,2,0]);assert value['paused'],value
    assert 'Synthetic queue build passed' in await read('Projects/fixture/Tasks/first/job.log')
    failed=await read('Projects/fixture/Tasks/failure/.result');assert failed.strip()=='20'
    await snap(queue,'queue-failure')
    # Explicit Resume continues the remaining queued task, without replaying the failed one.
    await key('13');await wait_states([3,2,3])
    p=await state(queue,front=True)
    await click(p['x']+250,p['y']+p['height']-34,queue) # Pause
    for _ in range(10):
        if (await persisted())['paused']:break
        await asyncio.sleep(.5)
    else:raise AssertionError('Pause did not persist')
    # Removing a failed entry also clears a stale reservation left by an interrupted dispatch.
    failed=fixture+'/Projects/fixture/Tasks/failure/.queued'
    await shell('Echo "synthetic stale reservation" > '+failed)
    await click(p['x']+130,p['y']+65,queue)
    await click(p['x']+p['width']-28,p['y']+p['height']-68,queue)
    value=await persisted();assert len(value['items'])==2,value
    assert 'STALE' not in await shell('If EXISTS '+failed+'\n Echo STALE\nEndIf')
    await click(p['x']+435,p['y']+p['height']-34,queue)
    assert not (await persisted())['items']
    await key('33') # C: Close queue
    await menu('15');p=await state(board);assert p['address'],p
    await snap(board,'kanban-completed')
    await click(p['x']+p['width']-38,p['y']+p['height']-24,board)
    report.write_text(json.dumps({'fixture':fixture,'startup_paused':True,'serial_order':True,
        'failure_paused':True,'resume_after_failure':True,'no_failure_replay':True,
        'board_open':True,'done_archive_restore':True,'queue_reorder_remove_clear':True,'screenshots_synthetic':True},indent=2))
