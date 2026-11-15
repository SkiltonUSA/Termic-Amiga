#include "taskflow.h"
#include "native.h"
#include "workspace.h"
#include "vendor/cJSON.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int allocations;
static void *limited_alloc(size_t size) {return allocations-- > 0?malloc(size):NULL;}
int main(void)
{
    struct TaskFlow f,g;memset(&f,0,sizeof(f));f.paused=1;
    assert(flow_add(&f,"DH0:Fixture/task-1",0));assert(flow_add(&f,"DH0:Fixture/task-2",1));
    assert(!flow_add(&f,"DH0:Fixture/task-1",0));assert(!flow_add(&f,"DH0:../bad",0));
    assert(flow_next(&f)==-1);f.paused=0;assert(flow_next(&f)==0);
    assert(flow_move(&f,1,-1));assert(f.items[0].build);
    f.items[0].state=FLOW_RUNNING;assert(flow_next(&f)==-1);assert(!flow_remove(&f,0));assert(!flow_move(&f,0,1));
    assert(flow_save(&f,"queue.json"));assert(flow_load(&g,"queue.json"));assert(g.paused && g.items[0].state==FLOW_RUNNING);
    flow_finish(&f,f.items[0].path,20);assert(f.paused && f.items[0].state==FLOW_ATTENTION);
    f.paused=0;assert(flow_next(&f)==1);f.items[1].state=FLOW_RUNNING;
    flow_finish(&f,f.items[1].path,0);assert(f.items[1].state==FLOW_COMPLETE && flow_next(&f)==-1);
    assert(flow_remove(&f,0));assert(f.count==1);
    assert(board_column("Done",1,0)==BOARD_WORKING);assert(board_column("Review",0,0)==BOARD_REVIEW);
    assert(board_column("Running",0,0)==BOARD_ATTENTION);assert(board_column("Done",0,0)==BOARD_DONE);
    assert(board_column("Archived",0,0)==BOARD_ARCHIVED);assert(board_column("Review",0,1)==BOARD_BACKLOG);
    assert(save_text("queue.json","{\"version\":1,\"items\":[{\"path\":\"DH0:bad;cmd\",\"build\":false,\"state\":0}]}",65536));
    assert(!flow_load(&g,"queue.json") && g.paused && !g.count);
    assert(!flow_load(&g,"missing.json") && g.paused && !g.count);
    assert(!flow_save(&f,"missing/queue.json"));
    {int n;cJSON_Hooks hooks={limited_alloc,free};
     for(n=0;n<20;n++) {
        assert(save_text("queue.json","existing queue",65536));
        allocations=n;cJSON_InitHooks(&hooks);
        if(!flow_save(&f,"queue.json")) {
            char *kept=read_text("queue.json",65536);assert(kept && !strcmp(kept,"existing queue"));free(kept);
        }
        cJSON_InitHooks(NULL);
     }}
    assert(save_text("queue.json","{\"version\":1,\"items\":[]} trailing",65536));
    assert(!flow_load(&g,"queue.json"));
    assert(save_text("queue.json","{\"version\":1,\"items\":[{\"path\":\"DH0:Task\",\"build\":false,\"state\":8}]}",65536));
    assert(!flow_load(&g,"queue.json"));
    memset(&g,0,sizeof(g));
    {int i;char path[40];for(i=0;i<FLOW_MAX;i++){sprintf(path,"DH0:Task/%d",i);assert(flow_add(&g,path,0));}}
    assert(!flow_add(&g,"DH0:Task/full",0));
    assert(flow_save(&g,"queue.json"));assert(flow_load(&f,"queue.json") && f.count==FLOW_MAX && f.paused);
    puts("Queue persistence, ordering, interruption and board state passed.");return 0;
}
