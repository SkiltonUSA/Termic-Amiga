"""Real local Git worktree tests, with synthetic repositories and no network."""
import asyncio
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch
import os
from git_service import GitWorkspace,command,request,source_path,executable
from codex_relay import validate_job


def config(action='sync'):
    return dict(action=action,repository='acme/fixture',branch='termic/fixture',base='main',title='Fixture change',body='Synthetic test')


class GitRequests(unittest.TestCase):
    def test_finder_path_fallback(self):
        expected=Path.home()/'.local/bin'/('gh.exe' if os.name=='nt' else 'gh')
        with patch('git_service.shutil.which',return_value=None),patch.object(Path,'is_file',lambda p:p==expected):
            self.assertEqual(executable('gh'),str(expected))
            self.assertIsNone(executable('git'))
    def test_request_boundaries(self):
        self.assertEqual(request(json.dumps(config()))['action'],'sync')
        for key,value in [('action','shell'),('repository','../../outside'),('branch','-bad'),
                          ('branch','a/../b'),('branch','bad.lock'),('branch','main'),('body','\0')]:
            c=config();c[key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):request(json.dumps(c))
        for name in ('../a.c','.git/config','name.txt','state.txt','State.txt','src/Session-123.txt','a//b.py','SYS:file.c'):
            with self.subTest(name=name),self.assertRaises(ValueError):source_path(name)
        self.assertEqual(str(source_path('src/task.py')),'src/task.py')
    def test_git_relay_is_readonly_and_bounded(self):
        job=dict(protocol=2,id='12345678-12345678-1234',provider='git',prompt=json.dumps(config()),
                 instructions='',model='',thread='',readonly=True,limit=129,guidance=[])
        self.assertEqual(validate_job(job)['provider'],'git')
        job['readonly']=False
        with self.assertRaises(ValueError):validate_job(job)
        job['readonly']=True;job['provider']='claude'
        with self.assertRaises(ValueError):validate_job(job)


@unittest.skipUnless(shutil.which('git'),'Git executable required')
class GitWorktrees(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.temp=tempfile.TemporaryDirectory();self.state=Path(self.temp.name)
        self.workspace=GitWorkspace(self.state,config());repo=self.workspace.repo;repo.mkdir(parents=True)
        await command(['git','init','-b','main',repo])
        await self.workspace.git('config','user.name','Fixture')
        await self.workspace.git('config','user.email','user@example.com')
        (repo/'main.c').write_text('original\n')
        await self.workspace.git('add','main.c');await self.workspace.git('commit','-m','Fixture baseline')
        await self.workspace.git('remote','add','origin',self.workspace.url)
        await self.workspace.git('update-ref','refs/remotes/origin/main','HEAD')
        await self.workspace.prepare()
    async def asyncTearDown(self):self.temp.cleanup()
    async def test_sync_commits_only_task_worktree(self):
        await self.workspace.sync({'main.c':'changed\n','src/task.py':'print(42)\n'})
        self.assertEqual((self.workspace.repo/'main.c').read_text(),'original\n')
        self.assertEqual((self.workspace.tree/'main.c').read_text(),'changed\n')
        self.assertEqual((await self.workspace.git('log','-1','--format=%s',tree=True)).strip(),'Fixture change')
        self.assertFalse((await self.workspace.git('status','--porcelain',tree=True)).strip())
        await self.workspace.prepare() # reuse the registered worktree
    async def test_dirty_worktree_is_preserved(self):
        (self.workspace.tree/'main.c').write_text('host edit\n')
        with self.assertRaisesRegex(ValueError,'uncommitted'):
            await self.workspace.sync({'main.c':'must not overwrite\n'})
        self.assertEqual((self.workspace.tree/'main.c').read_text(),'host edit\n')
    async def test_remote_mismatch_is_refused(self):
        await self.workspace.git('remote','set-url','origin','https://github.com/acme/other.git')
        with self.assertRaisesRegex(ValueError,'remote'):
            await self.workspace.prepare()
    async def test_existing_pr_is_reused_without_creation(self):
        calls=[]
        async def runner(args,cwd=None):
            calls.append(args)
            if args[:3]==['gh','pr','list']:return '[{"url":"https://github.com/acme/fixture/pull/7"}]'
            return ''
        self.workspace.runner=runner
        result=await self.workspace.pull_request()
        self.assertTrue(result.endswith('/7'))
        self.assertFalse(any(args[:3]==['gh','pr','create'] for args in calls))
        self.assertTrue(any('push' in args for args in calls))
