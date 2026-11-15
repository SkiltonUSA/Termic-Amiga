"""Host Git worktrees for the Amiga. No cloud model and no shell parsing.

Repositories and worktrees live under the proxy's private state directory.
Only explicit sync/PR actions transfer files. Existing dirty worktrees are refused.
"""
import asyncio
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
from codex_relay import Mailbox


def request(text):
    value=json.loads(text)
    if not isinstance(value,dict) or set(value)!={'action','repository','branch','base','title','body'}:
        raise ValueError('Invalid Git request fields')
    if value['action'] not in ('status','sync','pr'):
        raise ValueError('Unsupported Git action')
    if not isinstance(value['repository'],str) or not re.fullmatch(r'[A-Za-z0-9_-][A-Za-z0-9_.-]{0,99}/[A-Za-z0-9_-][A-Za-z0-9_.-]{0,99}',value['repository']):
        raise ValueError('Use a GitHub owner/repository name')
    for key in ('branch','base'):
        ref=value[key]
        if not isinstance(ref,str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_./-]{0,79}',ref) or any(x in ref for x in ('..','//','/.')) or ref.endswith(('/','.','.lock')):
            raise ValueError('Invalid Git branch name')
    if value['branch']==value['base']:
        raise ValueError('Use a task branch different from the base branch')
    for key,limit in [('title',200),('body',4096)]:
        if not isinstance(value[key],str) or len(value[key])>limit or '\0' in value[key]:
            raise ValueError('Invalid Git description')
    if value['action']!='status' and not value['title'].strip():
        raise ValueError('Enter a commit / PR title')
    return value


def source_path(name):
    p=PurePosixPath(name)
    if not re.fullmatch(r'[A-Za-z0-9_./-]{1,180}',name) or any(part.startswith('.') for part in p.parts) or str(p)!=name or p.is_absolute():
        raise ValueError('Invalid source filename')
    if p.suffix not in ('.c','.h','.py','.rexx','.rx','.txt','.md'):
        raise ValueError('Unsupported source file')
    if p.name.lower() in ('task.txt','state.txt','name.txt','command.txt','commit.txt','branch.txt','build-command.txt','session.txt','transcript.txt','last-prompt.txt') or p.name.lower().startswith(('session-','usage-')):
        raise ValueError('Internal task state is not a source file')
    return p


def executable(name):
    found=shutil.which(name)
    if found:return found
    suffix='.exe' if os.name=='nt' else ''
    directories=[Path.home()/'.local/bin',Path('/opt/homebrew/bin'),Path('/usr/local/bin')]
    if os.name=='nt':
        base=Path(os.environ.get('ProgramFiles','C:/Program Files'))
        directories += [base/'Git/cmd',base/'GitHub CLI']
    for directory in directories:
        candidate=directory/(name+suffix)
        if candidate.is_file():return str(candidate)
    return None


async def command(args,cwd=None):
    from cli_relay import process_options,terminate
    args=list(args);env=dict(os.environ)
    if args[0] in ('git','gh'):
        found=executable(args[0])
        if not found:raise RuntimeError('Install '+args[0]+' on this host')
        args[0]=found
        env['PATH']=os.pathsep.join([str(Path(p).parent) for p in (executable('git'),executable('gh')) if p]+[env.get('PATH','')])
        env['GIT_TERMINAL_PROMPT']='0';env['GH_PROMPT_DISABLED']='1'
    process=await asyncio.create_subprocess_exec(*map(str,args),cwd=cwd,env=env,start_new_session=os.name!='nt',**process_options(),
        stdout=asyncio.subprocess.PIPE,stderr=asyncio.subprocess.STDOUT)
    try:
        raw,_=await asyncio.wait_for(process.communicate(),180)
    except BaseException:
        await terminate(process);raise
    text=raw.decode('utf-8','replace')[-16384:]
    if process.returncode:raise RuntimeError(text.strip() or 'Git operation failed')
    return text


class GitWorkspace:
    def __init__(self,state,config,runner=command):
        self.config=config;self.runner=runner
        self.root=Path(state).resolve()/'git'
        self.root.mkdir(parents=True,exist_ok=True)
        self.hooks=self.root/'no-hooks';self.hooks.mkdir(exist_ok=True)
        key=hashlib.sha256(config['repository'].lower().encode()).hexdigest()[:24]
        branch=hashlib.sha256(config['branch'].encode()).hexdigest()[:24]
        self.repo=self.root/'repositories'/key
        self.tree=self.root/'worktrees'/key/branch
        self.url='https://github.com/'+config['repository']+'.git'
        for path in (self.hooks,self.repo,self.tree):
            if not path.resolve().is_relative_to(self.root):raise ValueError('Git storage escapes proxy state')
    async def git(self,*args,tree=False):
        return await self.runner(['git','-c','core.hooksPath='+str(self.hooks),'-c','protocol.file.allow=never',
            '-C',str(self.tree if tree else self.repo),*args])
    async def prepare(self):
        if not self.repo.exists():
            self.repo.parent.mkdir(parents=True,exist_ok=True)
            await self.runner(['git','-c','core.hooksPath='+str(self.hooks),'-c','protocol.file.allow=never',
                'clone','--no-checkout','--',self.url,str(self.repo)])
        actual=(await self.git('remote','get-url','origin')).strip()
        if actual.lower()!=self.url.lower():raise ValueError('Repository remote does not match the requested project')
        if not self.tree.exists():
            self.tree.parent.mkdir(parents=True,exist_ok=True)
            # New isolated branch from the requested remote base. Never switch the main checkout.
            await self.git('worktree','add','-b',self.config['branch'],str(self.tree),'origin/'+self.config['base'])
        common=Path((await self.git('rev-parse','--git-common-dir',tree=True)).strip())
        if not common.is_absolute():common=self.tree/common
        if common.resolve()!=(self.repo/'.git').resolve():raise ValueError('Unexpected worktree repository')
        branch=(await self.git('branch','--show-current',tree=True)).strip()
        if branch!=self.config['branch']:raise ValueError('Worktree branch has been changed externally')
    async def sync(self,files):
        if (await self.git('status','--porcelain','--untracked-files=all','--ignored=matching',tree=True)).strip():
            raise ValueError('Host worktree has uncommitted changes. Review or commit them on the host first')
        if not files:raise ValueError('No source files returned by the Amiga')
        paths=[];folded=set()
        for name,text in files.items():
            relative=source_path(name);target=self.tree/relative
            if name.casefold() in folded:raise ValueError('Case-colliding source paths')
            folded.add(name.casefold())
            if not target.resolve().is_relative_to(self.tree.resolve()):raise ValueError('Source path follows a link outside the worktree')
            if not isinstance(text,str) or len(text.encode('latin1'))>32768 or '\0' in text:
                raise ValueError('Invalid or oversized Amiga source file')
            paths.append((name,target,text))
        for _,path,text in paths:
            path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(text.encode('latin1'))
        await self.git('add','--',*[name for name,_,_ in paths],tree=True)
        if (await self.git('diff','--cached','--name-only',tree=True)).strip():
            await self.git('commit','-m',self.config['title'],tree=True)
        return await self.git('status','--short','--branch',tree=True)
    async def pull_request(self):
        await self.git('push','--set-upstream','origin',self.config['branch'],tree=True)
        # Reuse an existing PR when available; never create duplicates on retries.
        found=await self.runner(['gh','pr','list','--repo',self.config['repository'],'--head',self.config['branch'],
            '--state','open','--json','url'])
        rows=json.loads(found)
        if rows:return rows[0]['url']
        return await self.runner(['gh','pr','create','--repo',self.config['repository'],'--head',self.config['branch'],
            '--base',self.config['base'],'--title',self.config['title'],'--body',self.config['body']])


async def run_git_job(bridge,job,state):
    box=Mailbox(bridge,job);status='failed';parent=asyncio.current_task()
    async def keep_alive():
        try:
            while True:
                await asyncio.sleep(5);await box.send('ping')
        except asyncio.CancelledError:raise
        except Exception:parent.cancel()
    heartbeat=asyncio.create_task(keep_alive())
    try:
        config=request(job['prompt'])
        if not executable('git'):raise RuntimeError('Install Git on this host before using Git worktrees')
        if config['action']=='pr' and not executable('gh'):raise RuntimeError('Install GitHub CLI and run gh auth login on the host')
        workspace=GitWorkspace(state,config)
        await box.text('Preparing host Git worktree...\n');await workspace.prepare()
        if config['action'] in ('sync','pr'):
            listing=await box.send('tool',name='list_sources',arguments={})
            names=[name for name in listing.splitlines() if name.strip()]
            if len(names)>128:raise ValueError('Maximum 128 source files per sync')
            files={}
            for name in names:
                source_path(name)
                files[name]=await box.send('tool',name='read_file',arguments={'path':name})
                if files[name].startswith(('Cannot read file:', 'Invalid path.', 'Unsupported tool')):
                    raise ValueError('Amiga could not read '+name)
            await box.text(await workspace.sync(files))
            if config['action']=='pr':await box.text((await workspace.pull_request()).strip()+'\n')
        else:
            await box.text(await workspace.git('status','--short','--branch',tree=True))
            await box.text(await workspace.git('worktree','list'))
        await box.text('Host worktree: '+str(workspace.tree)+'\n');status='completed'
    except asyncio.CancelledError:
        status='interrupted'
    except Exception as error:
        await box.text('Git stopped: '+str(error)+'\n')
    finally:
        heartbeat.cancel();await asyncio.gather(heartbeat,return_exceptions=True)
        try:await box.send('done',status=status)
        except (ConnectionError,TimeoutError):pass
    return status=='completed'
