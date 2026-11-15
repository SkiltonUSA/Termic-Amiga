#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/interrupts.h>
#include <exec/ports.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <devices/timer.h>
#include <hardware/intbits.h>
#include <libraries/configvars.h>
#include <libraries/expansion.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/expansion.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mui_compat.h"
struct Library *MUIMasterBase;
struct IntuitionBase *IntuitionBase;
struct ExpansionBase *ExpansionBase;
struct GfxBase *GfxBase;
struct Library *DiskfontBase;
static struct TextFont *bodyfont,*codefont;
#include "model.h"
#include "monitor.h"
#include "services.h"
static Object *app,*win,*list,*detail,*summary,*status,*servlist;
static char summarytext[512],detailtext[1024],armtext[768],statustext[256];
static char entries[MAXROWS][160],serviceentries[NSERVICES][384];
static const char version[]="$VER: Termic Process Monitor 0.1.0 Beta (13.11.2026)";
static BOOL synthetic;
static struct MsgPort *timerport;
static struct timerequest *timerio;
static BOOL timeropen,timerpending;
static void statusmsg(const char *s)
{
 copystr(statustext,s,sizeof(statustext));
 if(status) SetAttrs(status,MUIA_Text_Contents,statustext,TAG_DONE);
}
static void armreport(void)
{
 struct ConfigDev *cd=NULL; struct MsgPort *p; Row owner;
 char boards[256],part[96],who[108];
 boards[0]=0; who[0]=0;
 ExpansionBase=(struct ExpansionBase *)OpenLibrary("expansion.library",37);
 if(ExpansionBase) {
  while((cd=FindConfigDev(cd,0x6d6e,-1))!=NULL) {
   if(cd->cd_Rom.er_Product==4 || cd->cd_Rom.er_Product==5) {
    sprintf(part,"%s base %08lx, %lu bytes; ",
      cd->cd_Rom.er_Product==4 ? "graphics" : "shared RAM",(ULONG)cd->cd_BoardAddr,cd->cd_BoardSize);
    if(strlen(boards)+strlen(part)<sizeof(boards)) strcat(boards,part);
   }
  }
  CloseLibrary((struct Library *)ExpansionBase); ExpansionBase=NULL;
 }
 if(portrow("Sixies.ARM.Debug.Owner",&owner)) copystr(who,owner.name,sizeof(who));
 else {
  Forbid(); p=FindPort("Sixies.ARM.Debug.Owner"); Permit();
  copystr(who,p ? "owner port present; task unavailable" : "no registered owner (not proof of idle)",sizeof(who));
 }
 sprintf(armtext,"ZZ9000: %s\nARM: load, private heap, priorities and dependencies unavailable: legacy firmware exposes no process/service table.\nConfigured firmware XX19c / XACP1.7 (not detected). Core0 firmware; Core1 custom payload possible. Owner: %s",
 boards[0] ? boards : "no matching AutoConfig boards",who);
}
static void makereport(void)
{
 int i; char cpu[16];
 snapshot();
 printf("Termic Process Monitor 0.1.0 Beta: statistical VBlank estimate\n");
 printf("CPU: %s detected. TF4060 is the configured board, not independently detected.\n",
  SysBase->AttnFlags & AFF_68060 ? "68060" : "not identified as 68060");
 printf("Window %lu seconds; samples %lu; idle %lu; unassigned %lu\n",ticks,totalhits,idlehits,losthits);
 if(totalhits) printf("Busy %lu%% idle %lu%%\n",percent(totalhits-idlehits,totalhits),percent(idlehits,totalhits));
 else puts("CPU N/A (zero samples)");
 printf("Chip free/total %lu/%lu; Fast %lu/%lu\n",AvailMem(MEMF_CHIP),AvailMem(MEMF_CHIP|MEMF_TOTAL),AvailMem(MEMF_FAST),AvailMem(MEMF_FAST|MEMF_TOTAL));
 for(i=0;i<rowcount;++i) {
  if(totalhits) sprintf(cpu,"%lu%%",percent(rows[i].hits,totalhits)); else strcpy(cpu,"N/A");
  printf("%08lx %s | %s state=%u pri=%d stack=%lu tracked(partial)=%lu%s CPU=%s\n",
   (ULONG)rows[i].task,rows[i].name,rows[i].command,rows[i].state,rows[i].pri,
   rows[i].stack,rows[i].tracked,rows[i].clipped ? "+" : "",cpu);
 }
 for(i=0;i<nservices;++i)printf("Service %s | port %s | %s | required ports: %s\n",services[i].name,services[i].port,portexists(services[i].port) ? "present" : "absent",*services[i].deps ? services[i].deps : "none declared");
 puts(armtext);
 puts("OS3 has no complete per-process heap accounting or dependency graph. Dependencies unknown unless declared.");
 if(snapshot_clipped) puts("Inventory clipped by bounded traversal.");
}
static BOOL timerstart(void)
{
 timerport=CreateMsgPort(); if(!timerport) return FALSE;
 timerio=(struct timerequest *)CreateIORequest(timerport,sizeof(*timerio));
 if(!timerio) return FALSE;
 if(OpenDevice(TIMERNAME,UNIT_VBLANK,(struct IORequest *)timerio,0)) return FALSE;
 timeropen=TRUE; return TRUE;
}
static void arm_timer(void)
{
 timerio->tr_node.io_Command=TR_ADDREQUEST;
 timerio->tr_time.tv_secs=1; timerio->tr_time.tv_micro=0;
 SendIO((struct IORequest *)timerio); timerpending=TRUE;
}
static void closetimer(void)
{
 if(timerpending) { AbortIO((struct IORequest *)timerio); WaitIO((struct IORequest *)timerio); }
 if(timeropen) CloseDevice((struct IORequest *)timerio);
 if(timerio) DeleteIORequest((struct IORequest *)timerio);
 if(timerport) DeleteMsgPort(timerport);
}
/* Fixture is the only process exercised by unattended control tests. */
static int fixture(const char *token)
{
 struct MsgPort *p,*proof;APTR memory;ULONG sig;volatile ULONG work=0;int i;
 if(!validport(token) || strncmp(token,"Termic.Monitor.Proof.",21))return 20;
 p=CreateMsgPort();proof=CreateMsgPort();
 if(!p || !proof){if(p)DeleteMsgPort(p);if(proof)DeleteMsgPort(proof);return 20;}
 memory=AllocVec(65536,MEMF_PUBLIC|MEMF_CLEAR);
 if(!memory){DeleteMsgPort(p);DeleteMsgPort(proof);return 20;}
 p->mp_Node.ln_Name="Termic.Monitor.Fixture";proof->mp_Node.ln_Name=(STRPTR)token;
 Forbid();
 if(FindPort(p->mp_Node.ln_Name) || FindPort(proof->mp_Node.ln_Name)) {
  Permit();FreeVec(memory);DeleteMsgPort(p);DeleteMsgPort(proof);return 10;
 }
 AddPort(p);AddPort(proof);Permit();
 do {
  for(i=0;i<4000;++i)work+=(ULONG)i;
  Delay(2);sig=SetSignal(0,SIGBREAKF_CTRL_C);
 }while(!(sig&SIGBREAKF_CTRL_C));
 Forbid();RemPort(proof);RemPort(p);Permit();
 FreeVec(memory);DeleteMsgPort(proof);DeleteMsgPort(p);return 0;
}
static BOOL fixturecommand(char *out,int size)
{
 BPTR lock;char path[512],name[256];int i;
 lock=GetProgramDir();if(!lock || !NameFromLock(lock,path,sizeof(path)))return FALSE;
 if(!GetProgramName(name,sizeof(name)) || !AddPart(path,FilePart(name),sizeof(path)))return FALSE;
 for(i=0;path[i];++i)
  if((unsigned char)path[i]<32 || strchr("\"*$`",path[i]))return FALSE;
 if(strlen(path)+strlen(fixture_token)+24>=(ULONG)size)return FALSE;
 sprintf(out,"\"%s\" --fixture-service %s",path,fixture_token);return TRUE;
}
static BOOL ownfixture(Row *out)
{
 Row proof;BOOL ok;
 Forbid();
 ok=portrow("Termic.Monitor.Fixture",out) && portrow(fixture_token,&proof) && same(out,&proof);
 if(ok){fixture_owner=*out;fixture_owned=TRUE;}
 Permit();return ok;
}
static BOOL waitfixture(Row *out)
{
 int i;for(i=0;i<100;++i){if(ownfixture(out))return TRUE;Delay(5);}return FALSE;
}
static BOOL waitgone(Row *old)
{
 int i;
 for(i=0;i<100;++i) {
  if(!portexists("Termic.Monitor.Fixture") && !portexists(fixture_token) && !alive(old))return TRUE;
  Delay(5);
 }
 return FALSE;
}
static int selftest(void)
{
 Service saved[NSERVICES];int savedcount=nservices;
 Row first,second,stale,me;int i,found=0,rc=0;BOOL ok=FALSE;
 struct DateStamp stamp;
 if(portexists("Termic.Monitor.Fixture")){puts("Fixture already exists; refused.");return 10;}
 DateStamp(&stamp);
 sprintf(fixture_token,"Termic.Monitor.Proof.%08lx.%08lx",(ULONG)FindTask(NULL),(ULONG)stamp.ds_Tick);
 if(portexists(fixture_token))return 10;
 memcpy(saved,services,sizeof(saved));nservices=1;memset(services,0,sizeof(services));
 strcpy(services[0].name,"Disposable test service");strcpy(services[0].port,"Termic.Monitor.Fixture");
 if(!fixturecommand(services[0].command,sizeof(services[0].command)))goto done;
 strcpy(services[0].deps,"Termic.Monitor.Missing.Required.Port");
 if(dependencies(&services[0]) || launch(&services[0]))goto done;
 services[0].deps[0]=0;
 Forbid();capture(&me,FindTask(NULL),FALSE);Permit();
 pending.old=me;pending.service=services[0];pending.phase=1;pending.remaining=1;
 if(pollrestart()!=-1 || portexists(services[0].port))goto done;
 if(!launch(&services[0]) || !waitfixture(&first))goto cleanup;
 Delay(150);ticks=3;snapshot();
 for(i=0;i<rowcount;++i)if(same(&first,&rows[i])) {
  found=1;if(!rows[i].stack || rows[i].hits>totalhits || rows[i].type!=NT_PROCESS)goto cleanup;
 }
 if(!found || !totalhits || idlehits>totalhits)goto cleanup;
 Forbid();capture(&me,FindTask(NULL),FALSE);Permit();
 if(stoprow(&me))goto cleanup;
 stale=first;stale.lower=(APTR)((ULONG)stale.lower+4);
 if(stoprow(&stale) || beginrestart(0,&stale))goto cleanup;
 strcpy(services[0].deps,"Termic.Monitor.Missing.Required.Port");
 if(beginrestart(0,&first) || !alive(&first))goto cleanup;
 services[0].deps[0]=0;
 if(!beginrestart(0,&first) || beginrestart(0,&first))goto cleanup;
 for(i=0;i<24 && !rc;++i){Delay(50);rc=pollrestart();}
 if(rc!=1 || !waitfixture(&second))goto cleanup;
 if(!stoprow(&second) || !waitgone(&second))goto cleanup;
 fixture_owned=FALSE;ok=TRUE;goto done;
cleanup:
 /* Never signal an unauthenticated owner, even if another task claimed the port. */
 pending.phase=0;
 if(ownfixture(&second)){stoprow(&second);waitgone(&second);}
done:
 pending.phase=0;fixture_owned=FALSE;memcpy(services,saved,sizeof(saved));nservices=savedcount;
 puts(ok ? "PROCESS_MONITOR_SELFTEST_PASS" : "PROCESS_MONITOR_SELFTEST_FAIL");
 return ok ? 0 : 20;
}
static Object *textobject(char *s)
{
 return MUI_NewObject(MUIC_Text,MUIA_Text_Contents,s,MUIA_Text_PreParse,"\33l",MUIA_Text_SetMin,FALSE,TAG_DONE);
}
static Object *button(char *s)
{
 return MUI_NewObject(MUIC_Text,MUIA_Frame,MUIV_Frame_Button,
 MUIA_Background,MUII_ButtonBack,MUIA_InputMode,MUIV_InputMode_RelVerify,
 MUIA_Text_Contents,s,MUIA_Text_PreParse,"\33c",TAG_DONE);
}
static const char *statename(UBYTE state)
{
 switch(state){case TS_RUN:return "Run";case TS_READY:return "Ready";
 case TS_WAIT:return "Wait";case TS_ADDED:return "Added";case TS_REMOVED:return "Exit";default:return "Other";}
}
static void servicedisplay(void)
{
 int i;static int displayed;char *state;
 SetAttrs(servlist,MUIA_List_Quiet,TRUE,TAG_DONE);
 for(i=0;i<nservices;++i) {
  state=synthetic ? "Example" : (portexists(services[i].port) ? "Running" : "Absent");
  sprintf(serviceentries[i],"%-24.24s %-8s  Requires: %.110s",services[i].name,state,
          *services[i].deps ? services[i].deps : "none declared");
  if(i>=displayed)DoMethod(servlist,MUIM_List_InsertSingle,serviceentries[i],MUIV_List_Insert_Bottom);
 }
 while(displayed>nservices){DoMethod(servlist,MUIM_List_Remove,MUIV_List_Remove_Last);--displayed;}
 displayed=nservices;
 SetAttrs(servlist,MUIA_List_Quiet,FALSE,TAG_DONE);
 DoMethod(servlist,MUIM_List_Redraw,MUIV_List_Redraw_All);
}
static const char *displayname(Row *r)
{
 const char *p,*base=r->command;
 if(!*base)return r->name;
 for(p=base;*p;++p)if(*p=='/' || *p==':')base=p+1;
 return *base ? base : r->name;
}
static void refresh(void)
{
 LONG active=-1;static int displayed;Row selected;BOOL have=FALSE;int i,newactive=-1;char cpu[16];
 ULONG chip,ctotal,fast,ftotal,rate;char duration[40];
 GetAttr(MUIA_List_Active,list,(ULONG *)&active);
 if(active>=0 && active<rowcount){selected=rows[active];have=TRUE;}
 if(!synthetic)snapshot();
 else {
  rowcount=3;memset(rows,0,sizeof(rows));
  for(i=0;i<3;++i) {
   sprintf(rows[i].name,"Example process %d",i+1);sprintf(rows[i].command,"Example:Programs/Demo%d",i+1);
   rows[i].task=(struct Task *)(0x1000UL+i*0x100);rows[i].stack=32768;
   rows[i].tracked=65536UL*(i+1);rows[i].hits=10*(i+1);rows[i].type=NT_PROCESS;
   rows[i].state=i==1 ? TS_READY : TS_WAIT;rows[i].pri=i-1;
  }
  totalhits=100;idlehits=40;
 }
 SetAttrs(list,MUIA_List_Quiet,TRUE,TAG_DONE);
 for(i=0;i<rowcount;++i) {
  if(totalhits)sprintf(cpu,"%lu%%",percent(rows[i].hits,totalhits));else strcpy(cpu,"N/A");
  sprintf(entries[i],"%-24.24s %08lx %-5s %4d %8lu %9lu%s %4s",displayname(&rows[i]),
   (ULONG)rows[i].task,statename(rows[i].state),rows[i].pri,rows[i].stack,
   rows[i].tracked,rows[i].clipped ? "+" : " ",cpu);
  if(i>=displayed)DoMethod(list,MUIM_List_InsertSingle,entries[i],MUIV_List_Insert_Bottom);
  if(have && same(&selected,&rows[i]))newactive=i;
 }
 if(synthetic && !displayed)newactive=0;
 while(displayed>rowcount){DoMethod(list,MUIM_List_Remove,MUIV_List_Remove_Last);--displayed;}
 displayed=rowcount;
 if(newactive!=active)SetAttrs(list,MUIA_NoNotify,TRUE,MUIA_List_Active,newactive,TAG_DONE);
 SetAttrs(list,MUIA_List_Quiet,FALSE,TAG_DONE);
 DoMethod(list,MUIM_List_Redraw,MUIV_List_Redraw_All);
 if(synthetic){chip=1048576;ctotal=2097152;fast=33554432;ftotal=67108864;}
 else {chip=AvailMem(MEMF_CHIP);ctotal=AvailMem(MEMF_CHIP|MEMF_TOTAL);
       fast=AvailMem(MEMF_FAST);ftotal=AvailMem(MEMF_FAST|MEMF_TOTAL);}
 rate=synthetic ? 50 : SysBase->VBlankFrequency;
 if(rate && totalhits)sprintf(duration,"%lu.%02lu s",totalhits/rate,(totalhits%rate)*100/rate);
 else strcpy(duration,"N/A");
 sprintf(summarytext,"%sChip free/total: %lu/%lu KiB    Fast: %lu/%lu KiB\nCPU %s, statistical %s: busy %lu%%, idle %lu%%\nSample interval: %s, samples: %lu, unassigned: %lu%s",
  synthetic ? "SYNTHETIC DATA. " : "",chip/1024,ctotal/1024,fast/1024,ftotal/1024,
  synthetic ? "example 68060" : (SysBase->AttnFlags & AFF_68060 ? "68060 (TF4060 target)" : "68k (model unspecified)"),
  totalhits ? "estimate" : "N/A",percent(totalhits-idlehits,totalhits),percent(idlehits,totalhits),
  duration,totalhits,losthits,snapshot_clipped ? "; list clipped" : "");
 SetAttrs(summary,MUIA_Text_Contents,summarytext,TAG_DONE);servicedisplay();
}
static void details(void)
{
 LONG a=-1;Row *r,owner;int i;char deps[256];
 GetAttr(MUIA_List_Active,list,(ULONG *)&a);
 if(a<0 || a>=rowcount)strcpy(detailtext,"Select a process to see its executable and allocation details.\nMemory is partial tracked allocation, not total heap usage.\nDependencies are unknown unless declared in services.txt.");
 else {
  r=&rows[a];strcpy(deps,"unknown (OS has no dependency graph)");
  if(!synthetic)for(i=0;i<nservices;++i)
   if(portrow(services[i].port,&owner) && same(r,&owner)) {
    copystr(deps,*services[i].deps ? services[i].deps : "none declared",sizeof(deps));break;
   }
  sprintf(detailtext,"Task: %.79s (%08lx)\nExecutable: %.255s\nStack: %lu bytes. Tracked allocation: %lu%s bytes (partial). State: %s; priority: %d.\nDeclared required ports: %.255s",
   r->name,(ULONG)r->task,*r->command ? r->command : "unavailable (Exec task or no CLI)",r->stack,r->tracked,
   r->clipped ? "+" : "",statename(r->state),r->pri,deps);
 }
 SetAttrs(detail,MUIA_Text_Contents,detailtext,TAG_DONE);
}
static BOOL confirm(char *message)
{
 return __termic_mui_request(MUIMasterBase,app,win,0,
 "Termic Process Monitor","_Proceed|_Cancel",message,NULL)==1;
}
static void control(BOOL restart)
{
 LONG a=-1; Row r; int i; Row owner;
 if(synthetic) { statusmsg("Synthetic mode: controls disabled."); return; }
 if(restart) {
  GetAttr(MUIA_List_Active,servlist,(ULONG *)&a);
  if(a<0 || a>=nservices) { statusmsg("Select an opt-in service."); return; }
  i=(int)a;
  if(!portrow(services[i].port,&r)) { statusmsg("Service port has no live task; restart refused."); return; }
  if(!dependencies(&services[i])) { statusmsg("Missing declared required port."); return; }
  if(!confirm("Request cooperative stop, wait for task and port exit, then start configured command?")) return;
  if(!portrow(services[i].port,&owner) || !same(&r,&owner) || !beginrestart(i,&r))
   statusmsg("Restart refused: stale, critical or pending task.");
  else statusmsg("Waiting for old task AND port to exit (10 second timeout).");
 } else {
  GetAttr(MUIA_List_Active,list,(ULONG *)&a);
  if(a<0 || a>=rowcount) { statusmsg("No live selection."); return; }
  r=rows[a];
  if(!confirm("Send cooperative CTRL-C? A process may ignore this request; no forced termination.")) return;
  statusmsg(stoprow(&r) ? "CTRL-C requested. Ignored breaks do not force termination." :
   "Stop refused: stale selection, self, Exec task or protected process.");
 }
}
static BOOL child(Object *group,Object *object)
{
 if(!object)return FALSE;DoMethod(group,OM_ADDMEMBER,object);return TRUE;
}
static int gui(void)
{
 Object *root=NULL,*buttons,*lv,*slv,*label,*b[5];ULONG sigs,id,got;int i,res=20,poll;
 static char *labels[]={"Refresh","Stop selected","Restart service","About","Quit"};
 MUIMasterBase=OpenLibrary("muimaster.library",21);
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37);
 if(!MUIMasterBase || !IntuitionBase || !timerstart())goto done;
 GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",37);
 DiskfontBase=OpenLibrary("diskfont.library",37);
 if(GfxBase && DiskfontBase) {
  struct TextAttr body={"helvetica.font",13,0,FPF_DISKFONT},code={"courier.font",13,0,FPF_DISKFONT};
  bodyfont=OpenDiskFont(&body);codefont=OpenDiskFont(&code);
 }
 root=MUI_NewObject(MUIC_Group,MUIA_Font,bodyfont ? (ULONG)bodyfont : MUIV_Font_Normal,TAG_DONE);if(!root)goto done;
 summary=textobject("");if(!child(root,summary))goto done;
 label=MUI_NewObject(MUIC_Text,MUIA_Text_Contents,"Executable / task        Address  State  Pri    Stack  Tracked*   CPU",
 MUIA_Text_SetMin,FALSE,MUIA_Font,codefont ? (ULONG)codefont : MUIV_Font_Fixed,TAG_DONE);
 if(!child(root,label))goto done;
 list=MUI_NewObject(MUIC_List,MUIA_Font,codefont ? (ULONG)codefont : MUIV_Font_Fixed,
 MUIA_ShortHelp,"Tracked bytes are partial tc_MemEntry accounting. CPU is statistical, per refresh interval.",TAG_DONE);
 if(!list)goto done;
 lv=MUI_NewObject(MUIC_Listview,MUIA_Listview_List,list,TAG_DONE);
 if(!lv){MUI_DisposeObject(list);list=NULL;goto done;}child(root,lv);
 detail=textobject("Select a process.\nTracked allocation is partial.\nDependencies may be unknown.");if(!child(root,detail))goto done;
 if(!child(root,textobject(armtext)))goto done;
 if(!child(root,textobject("Opt-in services: select a running service to restart. Commands are never started automatically.")))goto done;
 servlist=MUI_NewObject(MUIC_List,MUIA_Font,codefont ? (ULONG)codefont : MUIV_Font_Fixed,TAG_DONE);if(!servlist)goto done;
 slv=MUI_NewObject(MUIC_Listview,MUIA_Listview_List,servlist,MUIA_Weight,20,TAG_DONE);
 if(!slv){MUI_DisposeObject(servlist);servlist=NULL;goto done;}child(root,slv);
 status=textobject("Ready. No service is started automatically.");if(!child(root,status))goto done;
 buttons=MUI_NewObject(MUIC_Group,MUIA_Group_Horiz,TRUE,TAG_DONE);if(!child(root,buttons))goto done;
 for(i=0;i<5;++i){b[i]=button(labels[i]);if(!child(buttons,b[i]))goto done;}
 win=MUI_NewObject(MUIC_Window,MUIA_Window_Title,"Termic Process Monitor",MUIA_Window_ID,0x54504d4e,
 MUIA_Window_Width,1000,MUIA_Window_Height,580,MUIA_Window_RootObject,root,TAG_DONE);
 if(!win)goto done;root=NULL;
 app=MUI_NewObject(MUIC_Application,MUIA_Application_Title,"Termic Process Monitor",
 MUIA_Application_Base,"TERMICMON",MUIA_Application_Version,version,MUIA_Application_Window,win,TAG_DONE);
 if(!app){MUI_DisposeObject(win);win=NULL;goto done;}
 DoMethod(win,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,app,2,MUIM_Application_ReturnID,100);
 for(i=0;i<5;++i)DoMethod(b[i],MUIM_Notify,MUIA_Pressed,FALSE,app,2,MUIM_Application_ReturnID,i+1);
 DoMethod(list,MUIM_Notify,MUIA_List_Active,MUIV_EveryTime,app,2,MUIM_Application_ReturnID,6);
 if(synthetic){SetAttrs(b[1],MUIA_Disabled,TRUE,TAG_DONE);SetAttrs(b[2],MUIA_Disabled,TRUE,TAG_DONE);}
 if(badservices){sprintf(statustext,"Ignored %d invalid/duplicate service definitions.",badservices);statusmsg(statustext);}
 refresh();details();SetAttrs(win,MUIA_Window_Open,TRUE,TAG_DONE);
 GetAttr(MUIA_Window_Open,win,&got);if(!got)goto done;
 arm_timer();res=0;
 for(;;) {
  sigs=0;id=DoMethod(app,MUIM_Application_NewInput,&sigs);
  if(id==100 || id==5)break;
  if(id==1){refresh();details();}
  if(id==2)control(FALSE);
  if(id==3)control(TRUE);
  if(id==4)__termic_mui_request(MUIMasterBase,app,win,0,"About","_OK",
   "Termic Process Monitor 0.1.0 Beta\nNative AmigaOS 3.2 / MUI 5\nStatistical CPU estimates and partial memory accounting.\nLegacy ZZ9000 ARM telemetry is unavailable.\nSkiltonUSA / Termic-Amiga contributors, AGPL-3.0-or-later\ngithub.com/SkiltonUSA/Termic-Amiga",NULL);
  if(id==6)details();
  if(CheckIO((struct IORequest *)timerio)) {
   WaitIO((struct IORequest *)timerio);timerpending=FALSE;++ticks;
   poll=pollrestart();
   if(poll)statusmsg(poll>0 ? "Service port returned after restart." : "Restart failed or timed out. No automatic retry.");
   refresh();details();arm_timer();
  }
  if(sigs){got=Wait(sigs|(1UL<<timerport->mp_SigBit)|SIGBREAKF_CTRL_C);if(got&SIGBREAKF_CTRL_C)break;}
 }
done:
 closetimer();
 if(app){SetAttrs(win,MUIA_Window_Open,FALSE,TAG_DONE);MUI_DisposeObject(app);}
 if(root)MUI_DisposeObject(root);
 if(bodyfont)CloseFont(bodyfont);if(codefont)CloseFont(codefont);
 if(DiskfontBase)CloseLibrary(DiskfontBase);
 if(GfxBase)CloseLibrary((struct Library *)GfxBase);
 if(MUIMasterBase)CloseLibrary(MUIMasterBase);
 if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);
 return res;
}
int main(int argc,char **argv)
{
 int result=20;
 if(argc==2 && !strcmp(argv[1],"--version")){puts(version+7);return 0;}
 if(argc==2 && !strcmp(argv[1],"--help")){puts("Program [--report | --self-test | --fixture-ui | --version]\nRun without arguments for MUI. See ReadMe for service setup.");return 0;}
 if(((struct Library *)DOSBase)->lib_Version<47){puts("Requires AmigaOS 3.2 (dos.library 47).");return 20;}
 if(argc>1 && !strcmp(argv[1],"--fixture-service")) return argc==3 ? fixture(argv[2]) : 20;
 if(argc>1 && strcmp(argv[1],"--report") && strcmp(argv[1],"--self-test") &&
 strcmp(argv[1],"--fixture-ui")) { puts("Unknown option"); return 10; }
 synthetic=argc>1 && !strcmp(argv[1],"--fixture-ui");
 if(synthetic) {
  strcpy(armtext,"SYNTHETIC ZZ9000: graphics and shared RAM boards\nARM telemetry unavailable on legacy firmware. Owner: example payload\nShared board RAM is not a measurement of ARM private heap.");
  nservices=1;strcpy(services[0].name,"Example service");strcpy(services[0].port,"EXAMPLE.SERVICE");strcpy(services[0].deps,"EXAMPLE.REQUIRED");
 } else { armreport();loadservices(); }
 if(!synthetic) sampling_start();
 if(argc>1 && !strcmp(argv[1],"--report")) { Delay(150); ticks=3; makereport(); result=0; }
 else if(argc>1 && !strcmp(argv[1],"--self-test")) result=selftest();
 else result=gui();
 sampling_stop();
 return result;
}
