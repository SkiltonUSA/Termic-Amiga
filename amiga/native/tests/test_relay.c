#include "relay.h"
#include "native.h"
#include "vendor/cJSON.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int executed;
static FILE *transcript;
static int append(const char *text) {return fputs(text,transcript)!=EOF && !fflush(transcript);}
char *agent_tool_result(const char *name,const char *args,int readonly)
{
    char *s=malloc(64);(void)args;executed++;
    strcpy(s,readonly && !strcmp(name,"write_source")?"Read-only profile":"tool result");return s;
}
int save_text(const char *path,const char *text,size_t limit)
{(void)path;(void)text;(void)limit;return 1;}
static int send(struct RelayState *s,const char *id,int seq,const char *kind,const char *extra,int *ok)
{
    char frame[1024],*reply=NULL;int rc;cJSON *r;
    sprintf(frame,"{\"id\":\"%s\",\"seq\":%d,\"kind\":\"%s\"%s}",id,seq,kind,extra);
    rc=relay_message(s,frame,&reply);
    if(reply){r=cJSON_Parse(reply);assert(r);*ok=cJSON_IsTrue(cJSON_GetObjectItem(r,"ok"));cJSON_Delete(r);free(reply);}
    return rc;
}
int main(void)
{
    struct RelayState s;int ok=0;char *reply=NULL;
    memset(&s,0,sizeof(s));strcpy(s.id,"fixture");strcpy(s.session,"session-fixture.txt");s.limit=1;transcript=tmpfile();assert(transcript);s.append=append;
    assert(relay_message(&s,"broken",&reply)==-1 && !reply);
    assert(send(&s,"other",1,"tool",",\"name\":\"build\",\"arguments\":{}",&ok)==-1 && !executed);
    assert(send(&s,s.id,2,"ping","",&ok)==-1 && s.seq==0);
    assert(send(&s,s.id,1,"ping","",&ok)==0 && ok);
    assert(send(&s,s.id,1,"tool",",\"name\":\"build\",\"arguments\":{}",&ok)==-1 && !executed);
    assert(send(&s,s.id,2,"session",",\"thread\":\"../../Config/api-key\"",&ok)==0 && !ok);
    assert(send(&s,s.id,3,"session",",\"thread\":\"01234567-abcd\"",&ok)==0 && ok);
    s.readonly=1;
    assert(send(&s,s.id,4,"tool",",\"name\":\"write_source\",\"arguments\":{}",&ok)==0 && ok && executed==1);
    assert(send(&s,s.id,5,"tool",",\"name\":\"build\",\"arguments\":{}",&ok)==0 && !ok && executed==1);
    assert(send(&s,s.id,6,"text",",\"text\":\"Hello\\n\"",&ok)==0 && ok);
    assert(send(&s,s.id,7,"done",",\"status\":\"unknown\"",&ok)==0 && !ok);
    assert(send(&s,s.id,8,"done",",\"status\":\"completed\"",&ok)==1 && ok && !s.result);
    memset(&s,0,sizeof(s));strcpy(s.id,"fixture");s.append=append;s.limit=1;
    assert(send(&s,s.id,1,"tool",",\"reply_slot\":0,\"name\":\"build\",\"arguments\":{}",&ok)==-1 && executed==1);
    assert(send(&s,s.id,1,"ping",",\"reply_slot\":1",&ok)==0 && ok && s.reply_slot==1);
    assert(send(&s,s.id,2,"ping",",\"reply_slot\":0",&ok)==0 && ok && s.reply_slot==0);
    assert(send(&s,s.id,2,"tool",",\"reply_slot\":0,\"name\":\"build\",\"arguments\":{}",&ok)==-1 && executed==1);
    assert(send(&s,s.id,3,"ping","",&ok)==0 && ok && s.reply_slot==-1);
    fclose(transcript);puts("Relay: stale IDs, sequence, limits, session and completion passed.");return 0;
}
