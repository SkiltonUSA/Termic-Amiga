"""Fresh fixture SDK extraction and on-Amiga build, inside accept_agents isolation."""
import asyncio
import json
import time
import zlib


async def stage(root,out,fixture,kit,shell,push,call):
    # Backup before any operation that can fail or change SDK assignments.
    await shell('If EXISTS ENV:VBCC\n Copy ENV:VBCC '+fixture+'/env-vbcc\nEndIf\nEcho saved > '+fixture+'/env-vbcc-saved')
    downloads=root/'../../.context/native-downloads'
    lock=json.loads((root/'amiga/native/sdk-lock.json').read_text())
    await push(kit,fixture+'/install-kit.lha')
    await shell('lha -q x '+fixture+'/install-kit.lha '+fixture+'/\nMakeDir '+fixture+'/SDK',timeout=60)
    # Large bridge CHECKSUM calls have a fixed ten-second timeout. Run the
    # streaming checker as a separate DOS process, then read its small result.
    checker=root/'.context/check-archive'
    await push(checker,fixture+'/CheckArchive')
    await shell('Protect '+fixture+'/CheckArchive +e')
    script=['FailAt 21','Stack 65536']
    expected=[];sources={}
    import hashlib
    for name in lock:
        remote='SD032G:Dev/Termic/SDK/'+name
        raw=(downloads/name).read_bytes()
        assert hashlib.sha256(raw).hexdigest()==lock[name]['sha256']
        if 'ARCHIVE_PRESENT' not in await shell('If EXISTS '+remote+'\n Echo ARCHIVE_PRESENT\nEndIf'):
            remote=fixture+'/SDK/'+name
            await push(downloads/name,remote)
        sources[name]=remote
        expected.append(f'{zlib.crc32(raw):08x} {len(raw)} {remote}')
        script += [fixture+'/CheckArchive '+remote+' >> '+fixture+'/archive-check.log',
                   'If WARN',' Echo 20 > '+fixture+'/archive-check.result',' Quit 20','EndIf']
    script+=['Echo 0 > '+fixture+'/archive-check.result']
    local=out/'archive-check-script';local.write_text('\n'.join(script)+'\n')
    await push(local,fixture+'/archive-check-script')
    await shell('Run >NIL: <NIL: Execute '+fixture+'/archive-check-script')
    for _ in range(300):
        if 'CHECK_DONE' in await shell('If EXISTS '+fixture+'/archive-check.result\n Echo CHECK_DONE\nEndIf'):break
        await asyncio.sleep(1)
    else:raise AssertionError('Archive checker did not finish; no installer was launched')
    local=out/'archive-check.log'
    await call('amiga_pull_file',{'amiga_path':fixture+'/archive-check.log','local_path':str(local)})
    assert local.read_text().splitlines()==expected,'SDK archive CRC/size mismatch'
    for name,remote in sources.items():
        if remote!=fixture+'/SDK/'+name:await shell('Copy '+remote+' '+fixture+'/SDK/'+name,timeout=60)
    script=['FailAt 21','Stack 262144']
    for name in ('InstallSDK','InstallUI','BuildNative','TermicCheck'):
        command=('Execute ' if name!='TermicCheck' else '')+'Termic:'+name
        script += [command+' > '+fixture+'/'+name+'.log','If WARN',
                   ' Echo 20 > '+fixture+'/install.result',' Quit 20','EndIf']
    script+=['Echo 0 > '+fixture+'/install.result']
    local=out/'install-script';local.write_text('\n'.join(script)+'\n')
    await push(local,fixture+'/install-script')
    await shell('Echo running > '+fixture+'/install-started\nRun >NIL: <NIL: Execute '+fixture+'/install-script')
    deadline=time.monotonic()+7200
    while time.monotonic()<deadline:
        if 'INSTALL_DONE' in await shell('If EXISTS '+fixture+'/install.result\n Echo INSTALL_DONE\nEndIf'):break
        await asyncio.sleep(1)
    else:raise AssertionError('Native installation/build did not finish; retain fixture for inspection')
    result=out/'install.result';await call('amiga_pull_file',{'amiga_path':fixture+'/install.result','local_path':str(result)})
    for name in ('InstallSDK','InstallUI','BuildNative','TermicCheck'):
        await call('amiga_pull_file',{'amiga_path':fixture+'/'+name+'.log','local_path':str(out/(name+'.log'))})
    assert result.read_text().strip()=='0', 'Inspect installation/build logs'
    assert 'dependency check: PASS' in (out/'TermicCheck.log').read_text()
    # Exercise the GUI and worker built natively in the rest of the acceptance suite.
    await shell('Copy '+fixture+'/TermicNative '+fixture+'/TermicProfilesTest')
    (out/'install-passed.json').write_text(json.dumps({'fresh_sdk_extraction':True,'native_build':True,'dependencies':True,'fixture':fixture},indent=2))
