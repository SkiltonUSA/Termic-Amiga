"""Portable checks for untrusted service definitions and 32-bit accounting."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class ProcessMonitorTests(unittest.TestCase):
    def test_service_input_and_counter_boundaries(self):
        source = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
typedef uint32_t ULONG;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#include "model.h"
static int parse(const char *text,Service *s){char line[1024];strcpy(line,text);return parse_service(line,s);}
int main(void){
 Service s;char line[1024],name[65];uint32_t x=12345,p,w;int i;
 assert(percent(0,0)==0 && percent(1,0)==0);
 assert(percent(UINT32_MAX-1,UINT32_MAX)==99);
 assert(percent(UINT32_MAX,UINT32_MAX)==100);
 for(i=0;i<100000;++i){
  x=x*1664525u+1013904223u;w=x?x:1;
  x=x*1664525u+1013904223u;p=x%w;
  assert(percent(p,w)==(uint64_t)p*100/w);
 }
 assert(parse(" # comment",&s)==0);
 assert(parse("Example | EXAMPLE.PORT | Run Example:Service | REQUIRED.ONE, REQUIRED.TWO\r\n",&s)==1);
 assert(!strcmp(s.name,"Example") && !strcmp(s.port,"EXAMPLE.PORT"));
 assert(validdeps(s.deps));
 assert(parse("Name|PORT|Command|",&s)==1);
 assert(parse("Name|PORT|Command|A,",&s)==-1);
 assert(parse("Name|PORT|Command|,A",&s)==-1);
 assert(parse("Name|PORT|Command|A,,B",&s)==-1);
 assert(parse("Name|PORT|Command|A|B",&s)==-1);
 assert(parse("Name|PORT||",&s)==-1);
 assert(parse("Name|PORT|Command",&s)==-1);
 assert(parse("Name|PORT|Command\nInjected|",&s)==-1);
 memset(name,'a',63);name[63]=0;snprintf(line,sizeof(line),"%s|PORT|Command|",name);assert(parse(line,&s)==1);
 name[63]='a';name[64]=0;snprintf(line,sizeof(line),"%s|PORT|Command|",name);assert(parse(line,&s)==-1);
 assert(contains("Background TERMIC task","termic"));
 assert(!contains("Example service","termic"));assert(!contains(NULL,"termic"));
 puts("process monitor model passed");return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            c=Path(d)/'test.c';binary=Path(d)/'test';c.write_text(source)
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-I',str(ROOT/'amiga/apps/process-monitor'),
                str(c),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

    def test_restart_lifecycle_and_stream_ownership(self):
        source = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
typedef uint32_t ULONG;
typedef int BOOL;
typedef long LONG;
typedef uintptr_t BPTR;
#define TRUE 1
#define FALSE 0
#define MODE_OLDFILE 1
#define MODE_NEWFILE 2
#define SYS_Asynch 3
#define SYS_Input 4
#define SYS_Output 5
#define NP_StackSize 6
#define TAG_DONE 0
#include "model.h"
typedef struct {int id;} Row;
static int present,oldalive,required,stops,launches,closes,opens,failopen,launchrc;
static Row current;
static void copystr(char *d,const char *s,int n){snprintf(d,(size_t)n,"%s",s);}
static BOOL portexists(const char *p){return !strcmp(p,"PORT") ? present : required;}
static BOOL portrow(const char *p,Row *r){if(!portexists(p))return FALSE;*r=current;return TRUE;}
static BOOL same(Row *a,Row *b){return a->id==b->id;}
static BOOL stoprow(Row *r){assert(r->id==current.id);++stops;return TRUE;}
static BOOL alive(Row *r){(void)r;return oldalive;}
static void Forbid(void){}
static void Permit(void){}
static BPTR GetProgramDir(void){return 0;}
static BOOL NameFromLock(BPTR l,char *p,int n){(void)l;(void)p;(void)n;return FALSE;}
static BOOL AddPart(char *p,const char *s,int n){(void)p;(void)s;(void)n;return FALSE;}
static BPTR Open(const char *p,int mode){(void)p;(void)mode;++opens;return opens==failopen ? 0 : (BPTR)opens;}
static void Close(BPTR b){assert(b);++closes;}
static LONG SystemTags(const char *command,...){assert(!strcmp(command,"Example:Service"));++launches;return launchrc;}
#include "services.h"
static void reset(void){
 loadservices();assert(!nservices);assert(contains("PORT","port"));assert(percent(1,2)==50);
 assert(parse_service((char[]){'N','|','P','|','C','|',0},&services[0])==1);
 nservices=1;strcpy(services[0].port,"PORT");strcpy(services[0].command,"Example:Service");
 strcpy(services[0].deps," DEPEND ");memset(&pending,0,sizeof(pending));
 present=oldalive=required=1;current.id=1;stops=launches=closes=opens=failopen=launchrc=0;
}
int main(void){Row old={1},stale={2};int i;
 reset();assert(!beginrestart(0,&stale) && stops==0);
 required=0;assert(!beginrestart(0,&old) && stops==0);required=1;
 assert(beginrestart(0,&old));assert(stops==1);assert(!beginrestart(0,&old));assert(stops==1);
 present=0;assert(pollrestart()==0 && launches==0); /* old task still alive */
 oldalive=0;present=1;assert(pollrestart()==0 && launches==0); /* old port remains */
 present=0;assert(pollrestart()==0 && launches==1 && closes==0);
 assert(pending.phase==2);assert(pollrestart()==0 && launches==1);
 current.id=2;present=1;assert(pollrestart()==1 && pending.phase==0);
 for(i=0;i<20;++i)assert(pollrestart()==0);assert(launches==1);
 reset();assert(beginrestart(0,&old));pending.remaining=1;
 assert(pollrestart()==-1 && launches==0); /* stop timeout does not spawn */
 reset();assert(beginrestart(0,&old));present=oldalive=0;
 assert(pollrestart()==0 && launches==1);pending.remaining=1;
 assert(pollrestart()==-1 && launches==1);assert(pollrestart()==0 && launches==1);
 reset();assert(beginrestart(0,&old));present=oldalive=required=0;
 assert(pollrestart()==-1 && launches==0); /* dependency disappears while stopping */
 reset();present=0;launchrc=-1;assert(!launch(&services[0]));assert(closes==2);
 reset();present=0;failopen=2;assert(!launch(&services[0]));assert(closes==1 && launches==0);
 reset();assert(!launch(&services[0]) && opens==0); /* existing service */
 puts("process monitor restart invariants passed");return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            c=Path(d)/'test.c';binary=Path(d)/'test';c.write_text(source)
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-I',str(ROOT/'amiga/apps/process-monitor'),
                str(c),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

    def test_ui_control_uses_selected_service_and_rechecks_identity(self):
        # Compile the actual UI handler with only its platform boundary mocked.
        # A separate process selection must not become the service selection.
        main=(ROOT/'amiga/apps/process-monitor/main.c').read_text()
        handler=main.split('static void control(BOOL restart)',1)[1].split('static BOOL child(',1)[0]
        source=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef int BOOL;
typedef int32_t LONG;
typedef uint32_t ULONG;
#define TRUE 1
#define FALSE 0
#define MUIA_List_Active 1
typedef struct {int id;} Row;
typedef struct {char port[16];} Service;
static int synthetic,nservices=2,rowcount=2,list=1,servlist=2;
static int selected_process=0,selected_service=1,confirmed=1,required=1,stale;
static int restarted=-1,stopped=-1,queries,confirmations;
static Row rows[2]={{10},{20}};
static Service services[2]={{{'A',0}},{{'B',0}}};
static char message[256];
static void GetAttr(int attr,int object,ULONG *value){assert(attr==1);*value=(ULONG)(object==servlist ? selected_service : selected_process);}
static void statusmsg(const char *s){snprintf(message,sizeof(message),"%s",s);}
static BOOL portrow(const char *p,Row *r){assert(!strcmp(p,"B"));r->id=20+(stale && queries>0);++queries;return TRUE;}
static BOOL dependencies(Service *s){assert(s==&services[1]);return required;}
static BOOL confirm(char *s){assert(*s);++confirmations;return confirmed;}
static BOOL same(Row *a,Row *b){return a->id==b->id;}
static BOOL beginrestart(int i,Row *r){assert(r->id==20);restarted=i;return TRUE;}
static BOOL stoprow(Row *r){stopped=r->id;return TRUE;}
'''+'static void control(BOOL restart)'+handler+r'''
static void reset(void){restarted=stopped=-1;queries=confirmations=stale=synthetic=0;confirmed=required=1;selected_service=1;}
int main(void){
 reset();control(TRUE);assert(restarted==1 && stopped==-1 && queries==2 && confirmations==1);
 reset();selected_service=-1;control(TRUE);assert(restarted==-1 && queries==0 && confirmations==0);
 reset();confirmed=0;control(TRUE);assert(restarted==-1 && queries==1);
 reset();required=0;control(TRUE);assert(restarted==-1 && confirmations==0);
 reset();stale=1;control(TRUE);assert(restarted==-1 && queries==2);
 reset();control(FALSE);assert(stopped==10 && restarted==-1);
 reset();synthetic=1;control(TRUE);control(FALSE);assert(stopped==-1 && restarted==-1 && confirmations==0);
 puts("process monitor UI control guards passed");return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            c=Path(d)/'test.c';binary=Path(d)/'test';c.write_text(source)
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined',str(c),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
