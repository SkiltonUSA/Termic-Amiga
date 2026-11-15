"""Synthetic native acceptance for review, notes and live editor colours."""
import asyncio
import json
import struct


async def stage(root,out,fixture,shell,push,call):
    selected=out/'feature-workspace';selected.write_text(fixture+'/Projects/fixture')
    await push(selected,fixture+'/Config/last-workspace')
    for local,name in [('.context/test-editor-colours','EditorCheck'),('.context/review-fixture','ReviewFixture'),('.context/inspect-ui','ClipboardProbe')]:
        await push(root/local,fixture+'/'+name)
        await shell('Protect '+fixture+'/'+name+' +e')
    # These helpers use production implementation code; no user files or cloud calls.
    await shell(fixture+'/EditorCheck > '+fixture+'/editor-check.log',timeout=60)
    local=out/'editor-check.log';await call('amiga_pull_file',{'amiga_path':fixture+'/editor-check.log','local_path':str(local)})
    assert 'Live editor colour preservation: PASS' in local.read_text()
    # Restaging this synthetic fixture must discard the preceding run's journal.
    # Keep this scoped to the isolated fixture root, never a user's workspace.
    base=fixture+'/Projects/fixture'
    await shell('Delete '+base+'/.review#? QUIET\nDelete '+base+'/notes.txt QUIET\nIf EXISTS '+base+'/.scratchpads/fixture\n Delete '+base+'/.scratchpads/fixture\nEndIf')
    result=await shell('FailAt 21\n'+fixture+'/ReviewFixture '+fixture+'/Projects/fixture',timeout=60)
    assert 'Review fixture: PASS' in result,result


async def exercise(root,out,fixture,shell,push,call,state,click,capture,menu):
    project=fixture+'/Projects/fixture';review='Termic: Review agent changes';pads='Termic: Scratchpads'
    async def read(path):
        local=out/'feature-read';await call('amiga_pull_file',{'amiga_path':project+'/'+path,'local_path':str(local)})
        return local.read_text()
    async def key(code,mods=()):
        try:
            for mod in mods:await call('amiga_input_key',{'rawkey':mod,'direction':'down'})
            await call('amiga_input_key',{'rawkey':code,'direction':'down'})
        finally:
            await call('amiga_input_key',{'rawkey':code,'direction':'up'})
            for mod in reversed(mods):await call('amiga_input_key',{'rawkey':mod,'direction':'up'})
        await asyncio.sleep(.5)
    async def snap(title,name):
        await asyncio.sleep(5);await capture(title,name+'.png')
    await menu('16');p=await state(review);assert p['address'],p
    await snap(review,'review-agent-edits')
    # The first two footer controls are Accept file and Revert file.
    await click(p['x']+320,p['y']+p['height']-24,review)
    confirm=await state('Revert agent edit');assert confirm['address'],confirm
    await click(confirm['x']+confirm['width']//4,confirm['y']+confirm['height']-20,'Revert agent edit')
    for _ in range(10):
        if 'original fixture' in await read('main.c'):break
        await asyncio.sleep(.5)
    else:raise AssertionError('Revert did not restore original source')
    assert 'generated fixture' in await read('main.c.bak')
    p=await state(review,front=True);await click(p['x']+65,p['y']+p['height']-24,review)
    for _ in range(10):
        if not json.loads(await read('.review.json'))['files']:break
        await asyncio.sleep(.5)
    else:raise AssertionError('Accept did not clear remaining review entry')
    await snap(review,'review-complete')
    await click(p['x']+p['width']-24,p['y']+p['height']-24,review)
    await shell(fixture+'/ClipboardProbe save-clip '+fixture+'/clipboard-backup')
    async def paste(text):
        content=text.encode('ascii');chunk=b'CHRS'+struct.pack('>I',len(content))+content+b'\0'*(len(content)%2)
        raw=b'FORM'+struct.pack('>I',4+len(chunk))+b'FTXT'+chunk
        local=out/'fixture-clipboard.iff';local.write_bytes(raw);await push(local,fixture+'/clipboard-fixture')
        await shell(fixture+'/ClipboardProbe restore-clip '+fixture+'/clipboard-fixture')
        await key('34',('67',))
    try:
        await menu('28');p=await state(pads);assert p['address'],p
        await snap(pads,'scratchpads-empty')
        await click(p['x']+75,p['y']+p['height']-92,pads);await paste('fixture')
        await click(p['x']+75,p['y']+p['height']-55,pads)
        await click(p['x']+p['width']//2,p['y']+90,pads);await paste('Synthetic task notes\n')
        await click(p['x']+25,p['y']+p['height']-24,pads)
        assert (await read('.scratchpads/fixture')).strip()=='Synthetic task notes'
        await snap(pads,'scratchpad-saved')
        await click(p['x']+p['width']-24,p['y']+p['height']-24,pads)
        await menu('24');title='Termic: Git branches and worktrees';p=await state(title);assert p['address'],p
        await snap(title,'git-worktrees')
        await click(p['x']+p['width']-24,p['y']+p['height']-24,title)
    finally:
        await shell(fixture+'/ClipboardProbe restore-clip '+fixture+'/clipboard-backup\nDelete '+fixture+'/clipboard-backup')
    (out/'features-passed.json').write_text(json.dumps({'review_revert':True,'review_accept':True,
        'new_note_saved':True,'git_controls':True,'live_colours_preserve_text_cursor_undo':True},indent=2))
