#include "native.h"
#include "profiles.h"
#include "workspace.h"
#include "relay.h"
#include "vendor/cJSON.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *field(cJSON *o, const char *name)
{
    cJSON *v=cJSON_GetObjectItemCaseSensitive(o,name);
    return cJSON_IsString(v)?v->valuestring:NULL;
}
static int thread_id(const char *id)
{
    unsigned n=0;
    if(!id || !*id)return 0;
    while(*id) {if(!isxdigit((unsigned char)*id) && *id!='-')return 0;id++;n++;}
    return n<=64;
}
int relay_message(struct RelayState *s,const char *json,char **reply)
{
    cJSON *m=cJSON_Parse(json),*seq,*out;
    const char *id,*kind,*text,*name;
    char *args=NULL,*result=NULL;
    int done=0,ok=1;
    *reply=NULL;
    if(!m)return -1;
    id=field(m,"id");kind=field(m,"kind");seq=cJSON_GetObjectItemCaseSensitive(m,"seq");
    if(!id || strcmp(id,s->id) || !kind || !cJSON_IsNumber(seq) ||
       seq->valuedouble!=s->seq+1 || s->seq>=10000) {cJSON_Delete(m);return -1;}
    if(!strcmp(kind,"ping")) {}
    else if(!strcmp(kind,"text")) {
        text=field(m,"text");
        if(!text || strlen(text)>8192)ok=0;
        else if(!s->append(text))ok=0;
    } else if(!strcmp(kind,"session")) {
        text=field(m,"thread");
        ok=thread_id(text) && save_text(s->session,text,64);
    } else if(!strcmp(kind,"tool")) {
        name=field(m,"name");args=cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(m,"arguments"));
        if(!name || strlen(name)>40 || !args || ++s->calls>s->limit)ok=0;
        else {
            printf("Mac agent tool: %s\n",name);
            fflush(stdout);
            result=agent_tool_result(name,args,s->readonly);
            ok=result!=NULL;
            printf("Mac agent tool returned: %s\n",name);
            fflush(stdout);
        }
    } else if(!strcmp(kind,"done")) {
        text=field(m,"status");
        if(!text || (strcmp(text,"completed") && strcmp(text,"failed") && strcmp(text,"interrupted")))ok=0;
        else {s->result=!strcmp(text,"completed")?0:20;done=1;}
    } else ok=0;
    out=cJSON_CreateObject();
    if(out) {
        cJSON_AddStringToObject(out,"id",s->id);
        cJSON_AddNumberToObject(out,"seq",++s->seq);
        cJSON_AddBoolToObject(out,"ok",ok);
        cJSON_AddStringToObject(out,"text",result?result:ok?"OK":"Invalid relay operation or tool-call limit reached.");
        *reply=cJSON_PrintUnformatted(out);cJSON_Delete(out);
    }
    free(args);free(result);cJSON_Delete(m);
    return *reply?done:-1;
}

#if defined(__VBCC__) || defined(__amigaos__)
#include <dos/dos.h>
#include <exec/ports.h>
#include <proto/dos.h>
#include <proto/exec.h>
#define RELAY "Termic:Relay/"
/* This target's C runtime fails fopen(a/r+) on the tested filesystem.
 * Use bounded backup-and-replace writes and close after each message so the
 * GUI can read progress. A full/unreadable transcript is never truncated. */
static int append_transcript(const char *text)
{
    char *old=read_text("transcript.txt",RESPONSE_LIMIT),*combined;
    size_t n=old?strlen(old):0,m=strlen(text);int ok;BPTR l;
    if(!old) {
        l=Lock((STRPTR)"transcript.txt",ACCESS_READ);
        if(l) {UnLock(l);puts("Transcript is unreadable or exceeds 256 KiB.");return 0;}
        if(IoErr()!=ERROR_OBJECT_NOT_FOUND)return 0;
    }
    if(n+m>RESPONSE_LIMIT) {free(old);puts("Transcript limit is 256 KiB. Archive it before sending again.");return 0;}
    combined=malloc(n+m+1);if(!combined) {free(old);return 0;}
    if(n)memcpy(combined,old,n);memcpy(combined+n,text,m+1);
    ok=save_text("transcript.txt",combined,RESPONSE_LIMIT);
    free(combined);free(old);return ok;
}
static int publish(const char *path,const char *text)
{
    char temp[160];FILE *f;int ok;size_t n=strlen(text),prefix;
    const char *dot=strrchr(path,'.');
    /* Keep the longest mailbox basename below the classic 30-byte limit:
     * job.stopped.tmp would exceed it. Only this worker publishes replies. */
    prefix=dot?(size_t)(dot-path):strlen(path);
    if(prefix+5>sizeof(temp))return 0;
    memcpy(temp,path,prefix);strcpy(temp+prefix,".tmp");
    f=fopen(temp,"wb");if(!f) {printf("Cannot open relay output: %s (DOS %ld).\n",temp,(long)IoErr());return 0;}
    ok=fwrite(text,1,n,f)==n;if(fclose(f))ok=0;
    if(ok) {DeleteFile((STRPTR)path);ok=Rename((STRPTR)temp,(STRPTR)path)!=0;}
    if(!ok) {printf("Cannot publish relay output: %s (DOS %ld).\n",path,(long)IoErr());DeleteFile((STRPTR)temp);}
    return ok;
}
int run_relay(void)
{
    struct AgentProfiles *registry=calloc(1,sizeof(*registry));
    struct AgentProfile *p;struct RelayState s;
    struct MsgPort port;struct DateStamp stamp;BPTR l;
    char in[128],out[128],stopped[128],*task=NULL,*json=NULL,*previous=NULL;
    cJSON *request=NULL,*guides=NULL;int selected,i,owned=0,idle=0,total=0,done=0;
    memset(&s,0,sizeof(s));s.result=20;
    if(!registry || !profiles_load(registry) || (selected=profile_selected(registry))<0) {puts("Cannot load Mac relay profile.");goto end;}
    p=&registry->items[selected];if(!profile_is_relay(p->kind)) {puts("Select a Mac agent profile.");goto end;}
    task=read_text("task.txt",8192);if(!task || !*task) {puts("Enter task instructions first.");goto end;}
    memset(&port,0,sizeof(port));port.mp_Node.ln_Name="TermicCodexRelay";
    port.mp_Node.ln_Type=NT_MSGPORT;port.mp_Flags=PA_IGNORE;port.mp_SigTask=FindTask(NULL);
    Forbid();if(!FindPort((STRPTR)"TermicCodexRelay")) {AddPort(&port);owned=1;}Permit();
    if(!owned) {puts("Another Mac relay task is running. Wait or stop it first.");goto end;}
    l=Lock((STRPTR)"Termic:Relay",ACCESS_READ);
    if(!l)l=CreateDir((STRPTR)"Termic:Relay");
    if(!l) {puts("Cannot create Termic:Relay drawer.");goto end;}UnLock(l);
    DateStamp(&stamp);
    sprintf(s.id,"%08lx-%08lx-%04lx",(unsigned long)stamp.ds_Days,
        (unsigned long)(stamp.ds_Minute*3000+stamp.ds_Tick),((unsigned long)FindTask(NULL))&0xffffUL);
    sprintf(in,RELAY "%s.in",s.id);sprintf(out,RELAY "%s.out",s.id);sprintf(stopped,RELAY "%s.stopped",s.id);
    profile_session(p,s.session);s.readonly=p->readonly;s.limit=p->steps;
    s.append=append_transcript;
    {char *heading=malloc(strlen(task)+80);int ok;
     if(!heading)goto end;sprintf(heading,"\nTask (%s on Mac):\n%s\n\n",profile_provider(p->kind),task);
     ok=s.append(heading);free(heading);
     if(!ok) {puts("Cannot update task transcript.");goto end;}}

    request=cJSON_CreateObject();guides=cJSON_CreateArray();
    if(!request || !guides) {puts("Not enough memory for relay request.");goto end;}
    cJSON_AddNumberToObject(request,"protocol",p->kind==5?1:2);
    cJSON_AddStringToObject(request,"provider",profile_provider(p->kind));
    cJSON_AddStringToObject(request,"id",s.id);
    cJSON_AddStringToObject(request,"prompt",task);
    cJSON_AddStringToObject(request,"model",p->model);
    cJSON_AddStringToObject(request,"instructions",agent_instructions);
    cJSON_AddBoolToObject(request,"readonly",p->readonly);
    cJSON_AddNumberToObject(request,"limit",p->steps);
    cJSON_AddItemToObject(request,"tools",cJSON_Parse(agent_tools_json));
    if(p->resume)previous=read_text(s.session,64);
    cJSON_AddStringToObject(request,"thread",thread_id(previous)?previous:"");
    for(i=0;i<=2;i+=2)if(project_guidance_allowed(guidance_files[i])) {
        char *g=read_text(guidance_files[i],8192);
        if(g) {cJSON *item=cJSON_CreateObject();cJSON_AddStringToObject(item,"path",guidance_files[i]);
            cJSON_AddStringToObject(item,"text",g);cJSON_AddItemToArray(guides,item);free(g);}
    }
    cJSON_AddItemToObject(request,"guidance",guides);guides=NULL;
    json=cJSON_PrintUnformatted(request);
    if(!json || strlen(json)>65536 || !publish(RELAY "request.json",json)) {puts("Cannot publish relay request.");goto end;}
    puts("Waiting for the host agent. Open Termic AI Proxy on the Mac or PC.");
    fflush(stdout);
    while(!done && total++<18000) {
        char *message,*reply=NULL;
        if(cancel_requested()) {puts("Mac relay cancelled.");break;}
        message=read_text(in,65536);
        if(message) {
            int result;DeleteFile((STRPTR)in);
            result=relay_message(&s,message,&reply);free(message);
            if(result<0) {free(reply);puts("Invalid relay frame; stopping.");break;}
            if(!publish(out,reply)) {free(reply);break;}
            free(reply);idle=0;done=result;
        } else if(++idle>(s.seq?1200:600)) {
            puts("Mac relay timed out. Check the Mac relay and MCP bridge, then send again.");break;
        }
        if(!done)Delay(5);
    }
    if(!done) {s.result=20;publish(stopped,"stopped");}
    if(!s.append(s.result==0?"\n[Mac relay completed]\n":"\n[Mac relay stopped or failed]\n"))s.result=20;
    DeleteFile((STRPTR)in);
end:
    if(owned) {DeleteFile((STRPTR)RELAY "request.json");RemPort(&port);}
    cJSON_Delete(request);cJSON_Delete(guides);free(previous);free(task);free(json);free(registry);
    return s.result;
}
#endif
