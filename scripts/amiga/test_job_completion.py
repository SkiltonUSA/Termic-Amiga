"""A GUI observer must never see an unregistered successful job without its result."""
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]

class CompletionTests(unittest.TestCase):
    def test_observer_and_metadata_failures(self):
        source=r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
typedef char *STRPTR;
struct MsgPort {int unused;};
static int registered,observed_missing,expected_result,state_failure;
static void observe(void){
 FILE *f;int rc=-1;char state[64]={0};
 if(registered)return;
 f=fopen(".result","r");if(!f){observed_missing=1;return;}
 assert(fscanf(f,"%d",&rc)==1);fclose(f);assert(rc==expected_result);
 f=fopen("state.txt","r");if(f){assert(fgets(state,sizeof(state),f));fclose(f);}
 if(!state_failure)assert(!strcmp(state,rc ? "Needs attention" : "Review"));
}
static void RemPort(struct MsgPort *p){(void)p;registered=0;observe();}
static int DeleteFile(STRPTR p){observe();return rmdir(p)==0;}
static int save_text(const char *p,const char *s,unsigned long limit){
 FILE *f;int ok;(void)limit;observe();if(state_failure)return 0;
 f=fopen(p,"w");if(!f)return 0;ok=fputs(s,f)>=0;if(fclose(f))ok=0;return ok;
}
#include "job_finish.h"
static void setup(void){
 remove(".result");remove(".result.new");remove("state.txt");mkdir(".busy",0700);
 registered=1;observed_missing=state_failure=0;
}
int main(void){struct MsgPort port;
 setup();expected_result=0;assert(finish_job(&port,0)==0);assert(!registered && !observed_missing);
 setup();expected_result=20;assert(finish_job(&port,20)==20);assert(!observed_missing);
 setup();state_failure=1;expected_result=20;assert(finish_job(&port,0)==20);assert(!observed_missing);
 setup();mkdir(".result.new",0700);assert(finish_job(&port,0)==20);
 assert(!registered && observed_missing);rmdir(".result.new");
 puts("worker completion observer passed");return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            c=Path(d)/'test.c';binary=Path(d)/'test';c.write_text(source)
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-I',str(ROOT/'amiga/native'),str(c),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],cwd=d,check=True)

    def test_long_jobs_and_exit_grace(self):
        source=r'''
#include <assert.h>
#include "job_watch.h"
int main(void){int polls=0,i;
 for(i=0;i<1000;++i)assert(job_watch(1,0,&polls)==0);
 assert(polls==0);
 assert(job_watch(0,0,&polls)==0); /* late publication after a long job */
 assert(job_watch(0,1,&polls)==1);
 assert(job_watch(1,1,&polls)==0 && polls==0); /* still owns resources */
 for(i=0;i<20;++i)assert(job_watch(0,0,&polls)==0);
 assert(job_watch(0,0,&polls)==-1);
 assert(job_watch(1,0,&polls)==0 && polls==0);
 assert(job_watch(0,0,&polls)==0);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            c=Path(d)/'test.c';binary=Path(d)/'test';c.write_text(source)
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-I',str(ROOT/'amiga/native'),str(c),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],cwd=d,check=True)
