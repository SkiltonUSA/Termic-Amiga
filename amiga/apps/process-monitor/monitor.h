#ifndef MONITOR_H
#define MONITOR_H
#define MAXROWS 256
#define HASHN 512
typedef struct {
 struct Task *task;
 APTR lower, upper;
 char name[80], command[256];
 ULONG tracked, stack, hits;
 UBYTE state, type;
 BYTE pri;
 BOOL clipped, has_cli, identity_clipped;
} Row;
typedef struct { struct Task *task; ULONG hits; } Bucket;
typedef struct {
 struct Interrupt irq;
 volatile ULONG total, idle, dropped;
 Bucket buckets[HASHN];
 BOOL installed;
} Sampler;
static Sampler sample;
static Bucket recent[HASHN];
static Row fixture_owner;
static BOOL fixture_owned;
static char fixture_token[80];
static Row rows[MAXROWS];
static int rowcount, memleft;
static ULONG totalhits, idlehits, losthits, ticks;
static BOOL snapshot_clipped;
static void copystr(char *d, const char *s, int n)
{
 int i=0;
 if(s) while(i<n-1 && s[i]) { d[i]=s[i]; ++i; }
 d[i]=0;
}
static ULONG __saveds __amigainterrupt sampler(__reg("a1") Sampler *s)
{
 struct Task *t;
 ULONG h, i;
 t=SysBase->ThisTask;
 ++s->total;
 if(!t || t->tc_State!=TS_RUN) { ++s->idle; return 0; }
 h=((ULONG)t>>4)&(HASHN-1);
 for(i=0;i<8;++i) {
  Bucket *b=&s->buckets[(h+i)&(HASHN-1)];
  if(!b->task || b->task==t) { b->task=t; ++b->hits; return 0; }
 }
 ++s->dropped;
 return 0;
}
static void sampling_start(void)
{
 memset(&sample,0,sizeof(sample));
 sample.irq.is_Node.ln_Type=NT_INTERRUPT;
 sample.irq.is_Node.ln_Pri=0;
 sample.irq.is_Node.ln_Name="Termic statistical sampler";
 sample.irq.is_Data=&sample;
 sample.irq.is_Code=(VOID (*)())sampler;
 AddIntServer(INTB_VERTB,&sample.irq);
 sample.installed=TRUE;
}
static void sampling_stop(void)
{
 if(sample.installed) { RemIntServer(INTB_VERTB,&sample.irq); sample.installed=FALSE; }
}
static void capture(Row *r,struct Task *t,BOOL memory)
{
 struct Process *p;
 struct CommandLineInterface *cli;
 UBYTE *b;
 struct MemList *ml;
 int i, walk=0;
 memset(r,0,sizeof(*r));
 r->task=t; r->lower=t->tc_SPLower; r->upper=t->tc_SPUpper;
 r->state=t->tc_State; r->pri=t->tc_Node.ln_Pri; r->type=t->tc_Node.ln_Type;
 copystr(r->name,t->tc_Node.ln_Name,sizeof(r->name));
 r->identity_clipped=t->tc_Node.ln_Name && strlen(r->name)==sizeof(r->name)-1;
 if((ULONG)r->upper>(ULONG)r->lower) r->stack=(ULONG)r->upper-(ULONG)r->lower;
 if(r->type==NT_PROCESS) {
  p=(struct Process *)t;
  if(p->pr_CLI) {
   r->has_cli=TRUE;
   cli=(struct CommandLineInterface *)BADDR(p->pr_CLI);
   if(cli->cli_CommandName) {
    b=(UBYTE *)BADDR(cli->cli_CommandName);
    i=b[0];
    memcpy(r->command,b+1,i); r->command[i]=0;
   }
  }
 }
 if(!memory) return;
 ml=(struct MemList *)t->tc_MemEntry.lh_Head;
 while(ml->ml_Node.ln_Succ && memleft>0 && walk++<256) {
  --memleft;
  for(i=0;i<ml->ml_NumEntries && memleft>0;++i) {
   ULONG n=ml->ml_ME[i].me_Length;
   --memleft;
   if(0xffffffffUL-r->tracked<n) { r->tracked=0xffffffffUL; r->clipped=TRUE; }
   else r->tracked+=n;
  }
  if(i<ml->ml_NumEntries) r->clipped=TRUE;
  ml=(struct MemList *)ml->ml_Node.ln_Succ;
 }
 if(ml->ml_Node.ln_Succ) r->clipped=TRUE;
}
static BOOL same(Row *a,Row *b)
{
 return a->task==b->task && a->lower==b->lower && a->upper==b->upper &&
 a->type==b->type && a->has_cli==b->has_cli &&
 !strcmp(a->name,b->name) && !strcmp(a->command,b->command);
}
static void addtask(struct Task *t)
{
 int i;
 if(!t) return;
 for(i=0;i<rowcount;++i) if(rows[i].task==t) return;
 if(rowcount==MAXROWS) { snapshot_clipped=TRUE; return; }
 capture(&rows[rowcount++],t,TRUE);
}
static int address_order(const void *a,const void *b)
{
 ULONG x=(ULONG)((const Row *)a)->task,y=(ULONG)((const Row *)b)->task;
 return x<y ? -1 : x>y;
}
static void snapshot(void)
{
 struct Node *n;
 struct List *lists[2];
 int j,k,walk;
 ULONG h,i,assigned=0;
 rowcount=0; memleft=4096; snapshot_clipped=FALSE;
 lists[0]=&SysBase->TaskReady; lists[1]=&SysBase->TaskWait;
 Forbid();
 addtask(SysBase->ThisTask);
 for(j=0;j<2;++j) {
  n=lists[j]->lh_Head; walk=0;
  while(n->ln_Succ && walk++<512) { addtask((struct Task *)n); n=n->ln_Succ; }
  if(n->ln_Succ) snapshot_clipped=TRUE;
 }
 Permit();
 /* Ready/Wait order changes whenever a task wakes. Keep rows stable across
  * those transitions so selection does not repeatedly scroll the viewport. */
 qsort(rows,rowcount,sizeof(*rows),address_order);
 Disable();
 totalhits=sample.total; idlehits=sample.idle; losthits=sample.dropped;
 memcpy(recent,sample.buckets,sizeof(recent));
 memset(sample.buckets,0,sizeof(sample.buckets));
 sample.total=sample.idle=sample.dropped=0;
 Enable();
 for(k=0;k<rowcount;++k) {
  h=((ULONG)rows[k].task>>4)&(HASHN-1);
  for(i=0;i<8;++i) {
   Bucket *b=&recent[(h+i)&(HASHN-1)];
   if(b->task==rows[k].task) { rows[k].hits=b->hits; assigned+=b->hits; break; }
  }
 }
 if(assigned<=totalhits-idlehits)losthits=totalhits-idlehits-assigned;
}

static BOOL findlive(Row *saved,Row *out)
{
 struct Node *n;
 struct List *lists[2];
 struct Task *t;
 int j,w;
 BOOL found=FALSE;
 lists[0]=&SysBase->TaskReady; lists[1]=&SysBase->TaskWait;
 t=SysBase->ThisTask;
 if(t==saved->task) { capture(out,t,FALSE); return same(saved,out); }
 for(j=0;j<2 && !found;++j) {
  n=lists[j]->lh_Head; w=0;
  while(n->ln_Succ && w++<512) {
   if((struct Task *)n==saved->task) {
    capture(out,(struct Task *)n,FALSE); found=same(saved,out); break;
   }
   n=n->ln_Succ;
  }
 }
 return found;
}
static BOOL critical(Row *r)
{
 static const char *guard[]={"termic","relay","bridge","handler","input",
 "exec","intuition","ramlib","filesystem","workbench","con-handler",
 "trackdisk","timer","sixies","debug","tcp/ip","tcpip","roadshow",
 "roadie","bsdsocket","inetd","superserver","netstack","rexxmast","iprefs",NULL};
 struct Node *n; struct MsgPort *p; int i,w=0;const char *base=r->command,*c;
 for(c=r->command;*c;++c)if(*c=='/' || *c==':')base=c+1;
 if(r->type!=NT_PROCESS || !r->has_cli || !*r->command || r->identity_clipped ||
    r->task==FindTask(NULL)) return TRUE;
 /* The sole exception requires the exact fixture port and task owner. */
 p=FindPort("Termic.Monitor.Fixture");
 if(fixture_owned && same(r,&fixture_owner) && p && p->mp_SigTask==r->task) {
  struct MsgPort *proof=FindPort(fixture_token);
  if(proof && proof->mp_SigTask==r->task)return FALSE;
 }
 for(i=0;guard[i];++i)
  if(contains(r->name,guard[i]) || contains(base,guard[i])) return TRUE;
 n=SysBase->PortList.lh_Head;
 while(n->ln_Succ && w++<1024) {
  p=(struct MsgPort *)n;
  if(p->mp_SigTask==r->task) {
   for(i=0;guard[i];++i)
    if(contains(p->mp_Node.ln_Name ? p->mp_Node.ln_Name : "",guard[i])) return TRUE;
  }
  n=n->ln_Succ;
 }
 return w>=1024;
}
static BOOL stoprow(Row *saved)
{
 Row live; BOOL ok=FALSE;
 Forbid();
 if(findlive(saved,&live) && !critical(&live)) {
  Signal(live.task,SIGBREAKF_CTRL_C); ok=TRUE;
 }
 Permit();
 return ok;
}
static BOOL alive(Row *r)
{
 Row temp; BOOL ok;
 Forbid(); ok=findlive(r,&temp); Permit(); return ok;
}
static BOOL portrow(const char *name,Row *r)
{
 struct MsgPort *p; Row key; BOOL ok=FALSE;
 Forbid();
 p=FindPort((STRPTR)name);
 if(p) {
  /* Compare pointer against the live lists before dereferencing owner. */
  struct Node *n; int j,w; struct List *ls[2];
  ls[0]=&SysBase->TaskReady; ls[1]=&SysBase->TaskWait;
  if(p->mp_SigTask==SysBase->ThisTask) {
   capture(r,SysBase->ThisTask,FALSE); ok=TRUE;
  }
  for(j=0;j<2 && !ok;++j) {
   n=ls[j]->lh_Head; w=0;
   while(n->ln_Succ && w++<512) {
    if((struct Task *)n==p->mp_SigTask) { capture(&key,(struct Task *)n,FALSE); *r=key; ok=TRUE; break; }
    n=n->ln_Succ;
   }
  }
 }
 Permit(); return ok;
}
static BOOL portexists(const char *name)
{
 BOOL yes; Forbid(); yes=FindPort((STRPTR)name)!=NULL; Permit(); return yes;
}
#endif
