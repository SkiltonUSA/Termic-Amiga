"""Verify packaged task pickup and mailbox replies against a synthetic MCP server.
The provider executable deliberately does not exist; no login or model is used.
"""
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

if len(sys.argv)>1 and sys.argv[1]=='--server':
    import zlib
    from mcp.server.fastmcp import FastMCP
    state=Path(sys.argv[3]); root='Termic:Relay'
    job={'protocol':2,'provider':'claude','id':'12345678-12345678-1234',
         'prompt':'Synthetic mailbox pickup check.','instructions':'Use only supplied tools.',
         'model':'','thread':'','readonly':True,'limit':2,'guidance':[]}
    files={root+'/request.json':json.dumps(job).encode()}; frames=[]; first=[True]; checksum_error=[True]
    mcp=FastMCP('fixture-mailbox',host='127.0.0.1',port=int(sys.argv[2]))
    @mcp.tool()
    def amiga_list_dir(path:str)->str:
        assert path==root
        if first[0]:
            first[0]=False
            return '[OK] T:fixture-capture'  # must be rejected, not treated as absence
        if not files:return 'Empty directory: '+root
        return f'Directory: {root} ({len(files)} entries)\n'+'\n'.join(
            f'  FILE {p.rsplit("/",1)[-1]:30s} {len(raw):>8}  ' for p,raw in files.items())
    @mcp.tool()
    def amiga_read_file(path:str,offset:int=0,size:int=4096)->str:
        raw=files[path][offset:offset+size]
        return '\n'.join(f'{offset+i:08x}  '+raw[i:i+16].hex(' ').ljust(47)+'   fixture' for i in range(0,len(raw),16))
    @mcp.tool()
    def amiga_write_file(path:str,offset:int,hex_data:str)->str:
        assert path.startswith(root+'/') and path.endswith('.in.tmp')
        files[path]=files.get(path,b'')[:offset]+bytes.fromhex(hex_data)
        return 'Write command sent'
    @mcp.tool()
    def amiga_checksum(path:str)->str:
        if checksum_error[0]:
            checksum_error[0]=False
            return 'Error: T:fixture-unrelated-reader'
        raw=files[path]
        return f'CRC32: {zlib.crc32(raw):08X}\nSize: {len(raw)} bytes'
    @mcp.tool()
    def amiga_run_script(script:str,timeout:int=10)->str:
        assert script.startswith('FailAt 10\nDelete '+root+'/')
        files.pop(script.split('Delete ',1)[1],None)
        return '[OK]'
    @mcp.tool()
    def amiga_rename(old_path:str,new_path:str)->str:
        assert new_path==root+'/'+job['id']+'.in' and old_path==new_path+'.tmp'
        frame=json.loads(files.pop(old_path)); frames.append(frame)
        assert frame['id']==job['id'] and frame['seq']==len(frames)
        files[root+'/'+job['id']+'.out']=json.dumps({'id':job['id'],'seq':frame['seq'],'ok':True,'text':'OK'}).encode()
        if frame['kind']=='done':
            assert frame['status']=='failed'
            files.pop(root+'/request.json')
            (state/'mailbox-passed.json').write_text(json.dumps({'kinds':[f['kind'] for f in frames],
                'task_pickup':True,'ordered_replies':True,'provider_failure_reported':True}))
        return f'Renamed: {old_path} -> {new_path}'
    mcp.run(transport='streamable-http')
else:
    binary=str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix='termic-mailbox-') as temp:
        state=Path(temp)
        with socket.socket() as s:
            s.bind(('127.0.0.1',0));port=s.getsockname()[1]
        server=subprocess.Popen([sys.executable,__file__,'--server',str(port),temp],
                                stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        worker=None
        try:
            url=f'http://127.0.0.1:{port}/mcp'
            for _ in range(100):
                try:urllib.request.urlopen(url,timeout=.2);break
                except urllib.error.HTTPError:break
                except urllib.error.URLError:time.sleep(.1)
            else:raise RuntimeError('Fixture MCP server did not start')
            (state/'proxy.json').write_text(json.dumps({'bridge':url,'autostart':False,
                'claude':str(state/'missing-agent-executable')}))
            worker=subprocess.Popen([binary,'--relay-worker',temp],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            for _ in range(100):
                if (state/'mailbox-passed.json').exists():break
                if worker.poll() is not None:raise AssertionError((state/'proxy.log').read_text())
                time.sleep(.2)
            else:raise AssertionError('Packaged proxy did not consume the fixture task: '+(state/'proxy.log').read_text())
            (state/'proxy.stop').touch();worker.wait(timeout=15)
            evidence=json.loads((state/'mailbox-passed.json').read_text())
            assert evidence['task_pickup'] and evidence['ordered_replies'] and evidence['provider_failure_reported']
            assert evidence['kinds'][0]=='ping' and evidence['kinds'][-1]=='done' and 'text' in evidence['kinds']
            assert 'Processing claude task.' in (state/'proxy.log').read_text()
            print('Packaged task pickup, ordered mailbox replies and provider-failure completion passed.')
        finally:
            for process in (worker,server):
                if process and process.poll() is None:
                    process.terminate()
                    try:process.wait(timeout=5)
                    except subprocess.TimeoutExpired:process.kill();process.wait()
