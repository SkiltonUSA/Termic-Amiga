#include "taskflow.h"
#include "native.h"
#include "workspace.h"
#include "vendor/cJSON.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
static int valid_path(const char *p)
{
    unsigned n=0;
    if(!p || !*p)return 0;
    while(*p) {if(++n>=FLOW_PATH || (!isalnum((unsigned char)*p) && !strchr(":/_-",*p)))return 0;p++;}
    return 1;
}
int flow_find(const struct TaskFlow *f,const char *path)
{
    int i;for(i=0;i<f->count;i++)if(f->items[i].state<=FLOW_RUNNING && !strcmp(f->items[i].path,path))return i;
    return -1;
}
int flow_add(struct TaskFlow *f,const char *path,int build)
{
    struct FlowItem *item;
    if(!valid_path(path) || f->count>=FLOW_MAX || flow_find(f,path)>=0)return 0;
    item=&f->items[f->count++];strcpy(item->path,path);item->build=!!build;item->state=FLOW_QUEUED;return 1;
}
int flow_move(struct TaskFlow *f,int index,int direction)
{
    struct FlowItem temp;int other=index+direction;
    if((direction!=1 && direction!=-1) || index<0 || index>=f->count || other<0 || other>=f->count ||
       f->items[index].state!=FLOW_QUEUED || f->items[other].state!=FLOW_QUEUED)return 0;
    temp=f->items[index];f->items[index]=f->items[other];f->items[other]=temp;return 1;
}
int flow_remove(struct TaskFlow *f,int index)
{
    if(index<0 || index>=f->count || f->items[index].state==FLOW_RUNNING)return 0;
    memmove(f->items+index,f->items+index+1,(f->count-index-1)*sizeof(f->items[0]));f->count--;return 1;
}
int flow_next(const struct TaskFlow *f)
{
    int i;if(f->paused)return -1;
    for(i=0;i<f->count;i++)if(f->items[i].state==FLOW_RUNNING)return -1;
    for(i=0;i<f->count;i++)if(f->items[i].state==FLOW_QUEUED)return i;
    return -1;
}
void flow_finish(struct TaskFlow *f,const char *path,int result)
{
    int i=flow_find(f,path);
    if(i>=0 && f->items[i].state==FLOW_RUNNING) {
        f->items[i].state=result?FLOW_ATTENTION:FLOW_COMPLETE;
        if(result)f->paused=1;
    }
}
int flow_save(const struct TaskFlow *f,const char *path)
{
    cJSON *root=cJSON_CreateObject(),*items;char *text=NULL;int i,ok=0;
    if(!root)return 0;
    items=cJSON_AddArrayToObject(root,"items");
    if(!items || !cJSON_AddNumberToObject(root,"version",1) ||
       !cJSON_AddBoolToObject(root,"paused",f->paused))goto done;
    for(i=0;i<f->count;i++) {
        cJSON *item=cJSON_CreateObject();if(!item)goto done;
        cJSON_AddItemToArray(items,item);
        if(!cJSON_AddStringToObject(item,"path",f->items[i].path) ||
           !cJSON_AddBoolToObject(item,"build",f->items[i].build) ||
           !cJSON_AddNumberToObject(item,"state",f->items[i].state))goto done;
    }
    text=cJSON_PrintUnformatted(root);
    if(text)ok=save_text(path,text,65536);
done:free(text);cJSON_Delete(root);return ok;
}
int flow_load(struct TaskFlow *f,const char *path)
{
    char *text=read_text(path,65536);const char *end; cJSON *root,*items,*item,*v;struct TaskFlow *next;int ok=0,i;
    memset(f,0,sizeof(*f));f->paused=1;
    if(!text)return 0;
    root=cJSON_ParseWithOpts(text,&end,1);free(text);if(!root)return 0;
    next=calloc(1,sizeof(*next));if(!next){cJSON_Delete(root);return 0;}
    v=cJSON_GetObjectItem(root,"version");items=cJSON_GetObjectItem(root,"items");
    if(!cJSON_IsNumber(v) || v->valuedouble!=1 || !cJSON_IsArray(items) || cJSON_GetArraySize(items)>FLOW_MAX)goto done;
    next->paused=1; /* Restart never silently resumes pending or uncertain work. */
    cJSON_ArrayForEach(item,items) {
        struct FlowItem *row=&next->items[next->count];
        cJSON *p=cJSON_GetObjectItem(item,"path"),*b=cJSON_GetObjectItem(item,"build"),*s=cJSON_GetObjectItem(item,"state");
        if(!cJSON_IsString(p) || !valid_path(p->valuestring) || !cJSON_IsBool(b) || !cJSON_IsNumber(s) ||
           s->valuedouble!=s->valueint || s->valueint<0 || s->valueint>FLOW_CANCELLED)goto done;
        if(s->valueint<=FLOW_RUNNING)for(i=0;i<next->count;i++)
            if(next->items[i].state<=FLOW_RUNNING && !strcmp(next->items[i].path,p->valuestring))goto done;
        strcpy(row->path,p->valuestring);row->build=cJSON_IsTrue(b);row->state=s->valueint;next->count++;
    }
    *f=*next;ok=1;
done:free(next);cJSON_Delete(root);return ok;
}
int board_column(const char *state,int running,int queued)
{
    if(running)return BOARD_WORKING;
    if(queued)return BOARD_BACKLOG;
    if(!state)return BOARD_BACKLOG;
    if(!strcmp(state,"Archived"))return BOARD_ARCHIVED;
    if(!strcmp(state,"Needs attention") || !strcmp(state,"Running"))return BOARD_ATTENTION;
    if(!strcmp(state,"Review"))return BOARD_REVIEW;
    if(!strcmp(state,"Done"))return BOARD_DONE;
    return BOARD_BACKLOG;
}
