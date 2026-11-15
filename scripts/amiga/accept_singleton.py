"""Check that a second GUI cannot redirect the running installation's assign.

Run by the isolated hardware acceptance, with a newer candidate in a separate
folder. The already running window must retain both its identity and Termic:.
"""
import json

async def exercise(root,out,fixture,shell,push,call,state,candidate):
    before=await state()
    assert before['address'] and before['control']
    await shell('If NOT EXISTS '+fixture+'/Duplicate\n MakeDir '+fixture+'/Duplicate\nEndIf')
    await push(candidate,fixture+'/Duplicate/TermicNative')
    await shell('Protect '+fixture+'/Duplicate/TermicNative +e')
    path_before=await shell('CD Termic:\nCD')
    # The expected exit 20 is below FailAt 21; preserve it in a private result.
    result=await call('amiga_run_script',{'script':'FailAt 21\nStack 262144\n'+fixture+'/Duplicate/TermicNative\nEcho $RC > '+fixture+'/duplicate.result','timeout':30})
    assert 'Termic is already running' in result,result
    local=out/'duplicate.result'
    await call('amiga_pull_file',{'amiga_path':fixture+'/duplicate.result','local_path':str(local)})
    assert local.read_text().strip()=='20'
    assert await shell('CD Termic:\nCD')==path_before
    after=await state()
    assert after['address']==before['address'] and after['control']
    (out/'singleton-passed.json').write_text(json.dumps({'duplicate_rejected':True,'assign_preserved':True,'running_window_preserved':True}))
