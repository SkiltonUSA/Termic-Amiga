#!/usr/bin/env python3
"""Portable MCP contract and filesystem-boundary tests, using synthetic projects."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/'amiga/native'
class MCPTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory(prefix='termic-mcp-')
        cls.work=Path(cls.tmp.name);cls.binary=cls.work/'mcp'
        subprocess.run(['cc','-std=c99','-D_DARWIN_C_SOURCE','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations','-Wno-misleading-indentation','-fsanitize=address,undefined','-I',str(SRC),'-DTERMIC_CONFIG="Config/"','-DTERMIC_ENV="Env/"']+[str(SRC/f) for f in ['mcp.c','profiles.c','files.c','workspace.c','vendor/cJSON.c']]+['-o',str(cls.binary)],check=True)
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def setUp(self):
        self.fixture=tempfile.TemporaryDirectory(prefix='fixture-',dir=self.work)
        self.cwd=Path(self.fixture.name)
        (self.cwd/'Config').mkdir();(self.cwd/'Env').mkdir()
        (self.cwd/'Projects/demo').mkdir(parents=True)
        (self.cwd/'Projects/demo/main.c').write_text('int main(void) { return 0; }\n')
        (self.cwd/'Config/projects-drawer').write_text('Projects')
        (self.cwd/'Config/mcp-enabled').write_text('1')
    def tearDown(self):self.fixture.cleanup()
    def rpc(self,method,params=None,ident=1):
        q={'jsonrpc':'2.0','method':method,'params':params or {}}
        if ident is not None:q['id']=ident
        p=subprocess.run([str(self.binary),'--stdio'],input=json.dumps(q)+'\n',text=True,capture_output=True,cwd=self.cwd,check=True)
        return json.loads(p.stdout) if p.stdout else None
    def call(self,tool,**args):return self.rpc('tools/call',{'name':tool,'arguments':args})['result']
    def create(self):
        result=self.call('task_new',project='demo',name='fixture')
        self.assertFalse(result['isError'],result)
        return self.cwd/'Projects/demo/Tasks/fixture'
    def test_protocol(self):
        self.assertEqual(self.rpc('initialize')['result']['protocolVersion'],'2025-11-25')
        self.assertIn('2026-07-28',self.rpc('server/discover')['result']['supportedVersions'])
        tools=self.rpc('tools/list')['result']['tools'];self.assertEqual(len(tools),15)
        self.assertEqual(len({x['name'] for x in tools}),15)
        modern=self.rpc('tools/list',{'_meta':{'io.modelcontextprotocol/protocolVersion':'2026-07-28'}})['result']
        self.assertEqual(modern['resultType'],'complete')
        self.assertEqual(modern['_meta']['io.modelcontextprotocol/serverInfo']['name'],'termic-amiga')
        self.assertEqual(modern['cacheScope'],'public')
        self.assertEqual(self.rpc('tools/list',{'_meta':{'io.modelcontextprotocol/protocolVersion':'unsupported'}})['error']['code'],-32022)
        self.assertEqual(self.rpc('missing')['error']['code'],-32601)
        self.assertFalse(self.rpc('tools/call',{'name':'project_list'})['result']['isError'])
        self.assertEqual(self.rpc('tools/call',{'name':'project_list','arguments':{'unexpected':1}})['error']['code'],-32602)
        self.assertIsNone(self.rpc('tools/call',{'name':'task_new','arguments':{'project':'demo','name':'notification'}},ident=None))
        self.assertFalse((self.cwd/'Projects/demo/Tasks/notification').exists())
        oversized=' '*65536+'\n'
        valid=json.dumps({'jsonrpc':'2.0','id':2,'method':'ping'})+'\n'
        p=subprocess.run([str(self.binary),'--stdio'],input=oversized+valid,text=True,capture_output=True,cwd=self.cwd,check=True)
        replies=[json.loads(line) for line in p.stdout.splitlines()]
        self.assertEqual(len(replies),2)
        self.assertEqual(replies[0]['error']['code'],-32600)
        self.assertEqual(replies[1]['id'],2)
    def test_tasks_files_scratchpads(self):
        task=self.create()
        self.assertEqual((task/'main.c').read_text(),(self.cwd/'Projects/demo/main.c').read_text())
        self.assertFalse(self.call('file_write',project='demo',task='fixture',path='main.c',content='/* changed */')['isError'])
        self.assertTrue((task/'main.c.bak').exists())
        for path in ['script.py','script.rexx','README.md','notes.txt']:
            self.assertFalse(self.call('file_write',project='demo',task='fixture',path=path,content='fixture')['isError'])
            self.assertEqual(self.call('file_read',project='demo',task='fixture',path=path)['content'][0]['text'],'fixture')
        self.assertFalse(self.call('scratchpad_write',project='demo',task='fixture',pad='notes',content='Fixture notes')['isError'])
        self.assertEqual(self.call('scratchpad_read',project='demo',task='fixture',pad='notes')['content'][0]['text'],'Fixture notes')
        self.assertTrue(self.call('task_new',project='demo',name='fixture')['isError'])
        self.assertTrue(self.call('task_build',project='demo',task='fixture')['isError']) # no host command execution
    def test_boundaries(self):
        task=self.create();(self.cwd/'Config/api-key').write_text('fixture-secret')
        for path in ['../../../../Config/api-key','DH0:Config/api-key','.mcp-owned','task.txt']:
            self.assertTrue(self.call('file_read',project='demo',task='fixture',path=path)['isError'])
        for path in ('State.txt','TASK.txt','Session-123.txt','agents.md'):
            self.assertTrue(self.call('file_write',project='demo',task='fixture',path=path,content='blocked')['isError'])
        (task/'escape.c').symlink_to(self.cwd/'Config/api-key')
        self.assertTrue(self.call('file_read',project='demo',task='fixture',path='escape.c')['isError'])
        self.assertTrue(self.call('file_write',project='demo',task='fixture',path='escape.c',content='oops')['isError'])
        (task/'main.c.new').symlink_to(self.cwd/'Config/api-key')
        self.assertTrue(self.call('file_write',project='demo',task='fixture',path='main.c',content='oops')['isError'])
        (task/'main.c.new').unlink()
        (task/'.busy').mkdir()
        self.assertTrue(self.call('file_write',project='demo',task='fixture',path='main.c',content='oops')['isError'])
        (task/'.busy').rmdir()
        (task/'.queued').write_text('pending')
        self.assertTrue(self.call('file_write',project='demo',task='fixture',path='main.c',content='oops')['isError'])
        (task/'.queued').unlink()
        (self.cwd/'Config/last-workspace').write_text('Projects/demo/Tasks/fixture')
        self.assertTrue(self.call('file_write',project='demo',task='fixture',path='main.c',content='oops')['isError'])
        (self.cwd/'Config/last-workspace').write_text(str(task.resolve()))
        self.assertTrue(self.call('file_write',project='demo',task='fixture',path='main.c',content='oops')['isError'])
        self.assertEqual((self.cwd/'Config/api-key').read_text(),'fixture-secret')
    def test_guidance_copy_read_and_write_boundaries(self):
        project=self.cwd/'Projects/demo'
        for name in ['AGENTS.md','CLAUDE.md','.agents/skills/termic/SKILL.md',
                     '.agents/skills/termic/.termic-version',
                     '.agents/skills/termic/references/development.md',
                     '.agents/skills/termic/references/mcp.md']:
            path=project/name;path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text('Fixture guidance: '+name)
        task=self.create()
        for original in [p for p in project.rglob('*') if p.is_file() and 'Tasks' not in p.parts and p.name!='main.c']:
            name=str(original.relative_to(project))
            self.assertEqual((task/name).read_text(),original.read_text())
            self.assertEqual(self.call('file_read',project='demo',task='fixture',path=name)['content'][0]['text'],original.read_text())
            self.assertTrue(self.call('file_write',project='demo',task='fixture',path=name,content='overwrite')['isError'])
        (task/'AGENTS.md').unlink()
        (self.cwd/'Config/private').write_text('fixture-private')
        (task/'AGENTS.md').symlink_to(self.cwd/'Config/private')
        self.assertTrue(self.call('file_read',project='demo',task='fixture',path='AGENTS.md')['isError'])
        (project/'AGENTS.md').unlink()
        (project/'AGENTS.md').symlink_to(self.cwd/'Config/private')
        self.assertFalse(self.call('task_new',project='demo',name='linked-guidance')['isError'])
        self.assertFalse((project/'Tasks/linked-guidance/AGENTS.md').exists())
        self.assertTrue(self.call('file_read',project='demo',task='fixture',path='.agents/skills/termic/../../private')['isError'])

    def test_disable_and_no_credentials(self):
        r=self.call('agent_list');self.assertNotIn('api-key',json.dumps(r))
        (self.cwd/'Env/mcp-enabled').write_text('0')
        self.assertTrue(self.call('project_list')['isError'])
        (self.cwd/'Env/mcp-enabled').unlink()
        self.assertFalse(self.call('project_list')['isError'])
if __name__=='__main__':unittest.main()
