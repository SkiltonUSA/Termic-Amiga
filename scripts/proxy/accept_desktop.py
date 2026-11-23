"""Drive the packaged Start/Stop/restart/close flow against a synthetic MCP bridge."""
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

if len(sys.argv) > 1 and sys.argv[1] == '--server':
    from mcp.server.fastmcp import FastMCP
    mcp = FastMCP('Amiga-MCP', host='127.0.0.1', port=int(sys.argv[2]))
    @mcp.tool()
    def amiga_list_dir(path: str) -> str:
        if (Path(sys.argv[3]) / 'test-bridge-down').exists():
            raise ConnectionError('Synthetic Amiga disconnect')
        return 'Empty directory: '+path
    mcp.run(transport='streamable-http')
else:
    binary = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix='termic-ui-') as temp:
        state = Path(temp)
        (state/'test-bridge-down').touch()
        with socket.socket() as s:
            s.bind(('127.0.0.1', 0)); port = s.getsockname()[1]
        server = subprocess.Popen([sys.executable, __file__, '--server', str(port), temp],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            url = f'http://127.0.0.1:{port}/mcp'
            for _ in range(100):
                try:
                    urllib.request.urlopen(url, timeout=.2); break
                except urllib.error.HTTPError: break
                except urllib.error.URLError: time.sleep(.1)
            else: raise RuntimeError('Fixture bridge did not start')
            (state/'proxy.json').write_text(json.dumps({'bridge':url, 'autostart':False}))
            subprocess.run([binary, '--ui-test', temp], check=True, timeout=50)
            evidence = json.loads((state/'ui-passed.json').read_text())
            assert all(evidence[k] for k in ('start','stop','restart','close','startup_retry','disconnect_retry'))
            print('Packaged reconnect, Start, Stop, restart and window-close passed.')
        finally:
            server.terminate()
            try: server.wait(timeout=5)
            except subprocess.TimeoutExpired: server.kill(); server.wait()
