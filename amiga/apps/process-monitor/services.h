#ifndef SERVICES_H
#define SERVICES_H
static Service services[NSERVICES];
static int nservices,badservices;
static BOOL dependencies(Service *s)
{
 char buf[256],*p,*q;
 copystr(buf,s->deps,sizeof(buf));p=trim(buf);
 while(*p) {
  q=strchr(p,',');if(q)*q=0;
  if(!portexists(trim(p)))return FALSE;
  if(!q)break;p=q+1;
 }
 return TRUE;
}
static void loadservices(void)
{
 FILE *f;char path[512],line[768];int i,rc,ch;Service s;
 nservices=badservices=0;
 if(!GetProgramDir() || !NameFromLock(GetProgramDir(),path,sizeof(path)) ||
    !AddPart(path,"services.txt",sizeof(path)))return;
 f=fopen(path,"r");if(!f)return;
 while(fgets(line,sizeof(line),f)) {
  if(!strchr(line,'\n') && !feof(f)) {
   while((ch=fgetc(f))!=EOF && ch!='\n'){}++badservices;continue;
  }
  rc=parse_service(line,&s);if(!rc)continue;
  if(rc<0 || nservices==NSERVICES){++badservices;continue;}
  for(i=0;i<nservices;++i)
   if(!strcmp(services[i].port,s.port) || !strcmp(services[i].name,s.name))break;
  if(i<nservices){++badservices;continue;}
  services[nservices++]=s;
 }
 fclose(f);
}
static BOOL launch(Service *s)
{
 BPTR in,out;LONG rc;
 if(!dependencies(s) || portexists(s->port))return FALSE;
 in=Open("NIL:",MODE_OLDFILE);if(!in)return FALSE;
 out=Open("NIL:",MODE_NEWFILE);if(!out){Close(in);return FALSE;}
 /* On OS3.2 (DOS47), asynchronous success transfers both handles. */
 rc=SystemTags(s->command,SYS_Asynch,TRUE,SYS_Input,in,SYS_Output,out,
               NP_StackSize,262144,TAG_DONE);
 if(rc==-1){Close(in);Close(out);return FALSE;}
 return TRUE;
}
typedef struct { int phase;Row old;Service service;ULONG remaining; } Pending;
static Pending pending;
static BOOL beginrestart(int index,Row *r)
{
 Row owner;
 if(index<0 || index>=nservices || pending.phase || !dependencies(&services[index]))return FALSE;
 /* Recheck ownership immediately before Signal, within the same scheduling lock. */
 Forbid();
 if(!portrow(services[index].port,&owner) || !same(r,&owner) || !stoprow(r)) {
  Permit();return FALSE;
 }
 pending.old=*r;pending.service=services[index];pending.remaining=10;pending.phase=1;
 Permit();return TRUE;
}
/* Once per second: 1=new live service, -1=failed, 0=waiting. Never retries. */
static int pollrestart(void)
{
 Row owner;
 if(!pending.phase)return 0;
 if(pending.phase==1 && !portexists(pending.service.port) && !alive(&pending.old)) {
  if(!launch(&pending.service)){pending.phase=0;return -1;}
  pending.phase=2;pending.remaining=10;return 0;
 }
 if(pending.phase==2 && portrow(pending.service.port,&owner)) {
  pending.phase=0;return 1;
 }
 if(!--pending.remaining){pending.phase=0;return -1;}
 return 0;
}
#endif
