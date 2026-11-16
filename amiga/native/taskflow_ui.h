/* MUI owns all queue/board state. Pending work is durable; restart pauses it. */

static struct TaskFlow task_flow;
static int flow_valid=1,board_loading,board_selected=-1,board_count,board_visible;
static Object *board_window,*queue_window,*board_lists[6],*board_titles[6],
    *board_search,*board_project,*board_detail,*queue_list,*queue_detail,*queue_limit;
static char board_paths[MAX_ITEMS][PATH_SIZE],board_selection[PATH_SIZE];
static int board_rows[6][MAX_ITEMS],board_counts[6];
static const char *board_names[]={"Backlog","Needs attention","Working","Review","Done","Archived"};
static const char *board_colors[]={"234D70","7A3900","006070","593180","205E35","40474D"};
static const char *board_projects[]={"All projects","Current project",NULL};
static const char *queue_limits[]={"1 worker","2 workers","3 workers","4 workers",NULL};
static const char *queue_names[]={"Queued","Working","Needs attention","Finished","Cancelled"};
/* Board-local MUI pointer dragging and native drop validation.
 * A drop queues a path, never a volatile row number; disk work occurs in the
 * application event loop after MUI has finished its drag transaction. */
static struct MUI_CustomClass *board_list_class;
static char board_drop_path[PATH_SIZE];
static int board_drop_column;
struct BoardListData {
    struct MUI_EventHandlerNode events;
    int tracking,moved,column,index,target;
    LONG x,y;
    char path[PATH_SIZE];
};
static int board_pointer_column(LONG x,LONG y)
{
    int i;for(i=0;i<6;i++)if(x>=_left(board_lists[i]) && x<=_right(board_lists[i]) &&
        y>=_top(board_lists[i]) && y<=_bottom(board_lists[i]))return i;
    return -1;
}
static void board_pointer(struct BoardListData *d,Object *obj,struct IntuiMessage *im)
{
    int column;struct MUI_List_TestPos_Result hit;
    if(!im)return;
    if(im->Class==IDCMP_INACTIVEWINDOW || (im->Class==IDCMP_RAWKEY && im->Code==0x45)) {
        d->tracking=0;return;
    }
    if(im->Class==IDCMP_MOUSEBUTTONS && im->Code==SELECTDOWN) {
        d->tracking=0;
        if(board_loading || *board_drop_path)return;
        column=board_pointer_column(im->MouseX,im->MouseY);
        if(column<0 || board_lists[column]!=obj)return;
        memset(&hit,0,sizeof(hit));hit.entry=-1;
        DoMethod(obj,MUIM_List_TestPos,im->MouseX,im->MouseY,&hit);
        if(hit.entry<0 || hit.entry>=board_counts[column] || hit.column<0)return;
        d->index=board_rows[column][hit.entry];strcpy(d->path,board_paths[d->index]);
        d->column=column;d->target=-1;d->x=im->MouseX;d->y=im->MouseY;
        d->tracking=1;d->moved=0;return;
    }
    if(!d->tracking)return;
    if(im->Class==IDCMP_MOUSEMOVE) {
        if(!(im->Qualifier&IEQUALIFIER_LEFTBUTTON)){d->tracking=0;return;}
        if(abs(im->MouseX-d->x)>=4 || abs(im->MouseY-d->y)>=4)d->moved=1;
        column=board_pointer_column(im->MouseX,im->MouseY);
        if(d->moved && column!=d->target) {
            char hint[160];d->target=column;
            if(column>=0 && column!=d->column && board_drop_state(column,0))
                snprintf(hint,sizeof(hint),"Release to move task to %s. Esc cancels.",board_names[column]);
            else strcpy(hint,"Drag to another task column. Working is assigned by live jobs. Esc cancels.");
            set(board_detail,MUIA_Text_Contents,hint);
        }
    } else if(im->Class==IDCMP_MOUSEBUTTONS) {
        d->tracking=0;
        column=board_pointer_column(im->MouseX,im->MouseY);
        if(im->Code==SELECTUP && d->moved && column!=d->column && board_drop_state(column,0) && !*board_drop_path) {
            strcpy(board_drop_path,d->path);board_drop_column=column;
#ifdef TERMIC_BOARD_DRAG_TRACE
            TERMIC_BOARD_DRAG_TRACE(1,d->index,column);
#endif
            DoMethod(app,MUIM_Application_ReturnID,KB_DROP);
        } else set(board_detail,MUIA_Text_Contents,*board_selection?board_selection:"Select a task.");
    }
}
static int board_drag_source(Object *source)
{
    int i,n,index;
    if(board_loading || *board_drop_path)return -1;
    for(i=0;i<6;i++)if(source==board_lists[i]) {
        n=value(source,MUIA_List_Active);
        if(n<0 || n>=board_counts[i])return -1;
        index=board_rows[i][n];
        /* Read MUI's active source row directly, as in the SDK example.
         * Application selection notifications may still be queued. All board
         * rows and lists are owned by this same UI task. */
        return index;
    }
    return -1;
}
static ULONG board_list_dispatch(__reg("a0") struct IClass *cl,
                                 __reg("a2") Object *obj,__reg("a1") Msg msg)
{
    struct BoardListData *d=obj?INST_DATA(cl,obj):NULL;
    switch(msg->MethodID) {
    case MUIM_Setup:
        if(!DoSuperMethodA(cl,obj,msg))return FALSE;
        memset(d,0,sizeof(*d));d->events.ehn_Object=obj;d->events.ehn_Class=cl;
        d->events.ehn_Priority=20;
        d->events.ehn_Events=IDCMP_MOUSEBUTTONS|IDCMP_MOUSEMOVE|IDCMP_INACTIVEWINDOW|IDCMP_RAWKEY;
        DoMethod(_win(obj),MUIM_Window_AddEventHandler,&d->events);return TRUE;
    case MUIM_Cleanup:
        d->tracking=0;DoMethod(_win(obj),MUIM_Window_RemEventHandler,&d->events);break;
    case MUIM_HandleEvent:
        board_pointer(d,obj,((struct MUIP_HandleEvent *)msg)->imsg);return 0;
    }
    if(msg->MethodID==MUIM_DragQuery || msg->MethodID==MUIM_DragDrop) {
        Object *source=((struct MUIP_DragQuery *)msg)->obj;
        int dest,index=board_drag_source(source);
        for(dest=0;dest<6 && obj!=board_lists[dest];dest++);
#ifdef TERMIC_BOARD_DRAG_TRACE
        TERMIC_BOARD_DRAG_TRACE(msg->MethodID==MUIM_DragDrop,index,dest);
#endif
        if(source==obj || index<0 || !board_drop_state(dest,0))return MUIV_DragQuery_Refuse;
        if(msg->MethodID==MUIM_DragQuery)return MUIV_DragQuery_Accept;
        strcpy(board_drop_path,board_paths[index]);board_drop_column=dest;
        DoMethod(app,MUIM_Application_ReturnID,KB_DROP);return 0;
    }
    return DoSuperMethodA(cl,obj,msg);
}
static Object *board_list(Object **o)
{
    if(!board_list_class)board_list_class=__termic_mui_class(MUIMasterBase,NULL,MUIC_List,NULL,sizeof(struct BoardListData),(APTR)board_list_dispatch);
    if(!board_list_class)return NULL;
    return *o=NewObject(board_list_class->mcc_Class,NULL,InputListFrame,
        MUIA_Listview_DragType,MUIV_Listview_DragType_None,
        MUIA_Font,bodyfont?(ULONG)bodyfont:MUIV_Font_List,MUIA_List_ShowDropMarks,FALSE,
        MUIA_List_ConstructHook,MUIV_List_ConstructHook_String,
        MUIA_List_DestructHook,MUIV_List_DestructHook_String,TAG_DONE);
}
#ifndef QUEUE_FILE
#define QUEUE_FILE "Termic:Config/queue.json"
#endif
static int flow_persist(void)
{
    if(flow_valid && flow_save(&task_flow,QUEUE_FILE))return 1;
    task_flow.paused=1;
    message("Queue paused: cannot save queue.json. Check disk space and permissions.");
    return 0;
}
static void queue_marker(const char *task,int create)
{
    char p[PATH_SIZE];join(p,task,".queued");
    if(create)save_text(p,"Pending in Termic task queue. Remove using the Queue window.",128);
    else DeleteFile((STRPTR)p);
}
static int board_matches(const char *text,const char *needle)
{
    const char *a,*b;
    if(!*needle)return 1;
    for(;*text;text++) {
        for(a=text,b=needle;*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b);a++,b++);
        if(!*b)return 1;
    }
    return 0;
}
static void board_add(const char *path,const char *project_label)
{
    char p[PATH_SIZE],row[240],*name,*state;
    const char *query=(char *)value(board_search,MUIA_String_Contents);
    int col,q,running,index;
    if(board_count>=MAX_ITEMS || !managed_path(path) || !exists(path))return;
    join(p,path,"name.txt");name=read_value(p,120);
    join(p,path,"state.txt");state=read_value(p,32);
    if(!board_matches(name?name:path,query) && !board_matches(project_label,query))goto done;
    q=flow_find(&task_flow,path);running=worker_registered(path);
    if(!running) {
        int j;for(j=0;j<16;j++)if(jobs[j].active && !strcmp(jobs[j].path,path))running=1;
    }
    col=board_column(state,running,q>=0 && task_flow.items[q].state==FLOW_QUEUED);
    if(q>=0 && task_flow.items[q].state==FLOW_QUEUED)
        snprintf(row,sizeof(row),"#%d %.70s | %.60s",q+1,name?name:"Main workspace",project_label);
    else snprintf(row,sizeof(row),"%.80s | %.60s",name?name:"Main workspace",project_label);
    index=board_count++;strcpy(board_paths[index],path);
    board_rows[col][board_counts[col]++]=index;
    DoMethod(board_lists[col],MUIM_List_InsertSingle,row,MUIV_List_Insert_Bottom);
    if(!strcmp(board_selection,path)) {
        SetAttrs(board_lists[col],MUIA_NoNotify,TRUE,MUIA_List_Active,board_counts[col]-1,TAG_DONE);
        board_selected=index;
    }
done:free(name);free(state);
}
static void board_refresh(void)
{
    int i;char dir[PATH_SIZE],p[PATH_SIZE],title[64],*name;
    BPTR lock;struct FileInfoBlock *fib;
    if(!board_window || !board_visible)return;
    board_loading=1;board_count=0;board_selected=-1;
    for(i=0;i<6;i++) {
        board_counts[i]=0;set(board_lists[i],MUIA_List_Quiet,TRUE);
        DoMethod(board_lists[i],MUIM_List_Clear);
    }
    for(i=0;i<project_count && board_count<MAX_ITEMS;i++) {
        if(value(board_project,MUIA_Cycle_Active)==1 && strcmp(project,project_paths[i]))continue;
        join(p,project_paths[i],"name.txt");name=read_value(p,120);
        board_add(project_paths[i],name?name:"Project");
        join(dir,project_paths[i],"Tasks");lock=Lock((STRPTR)dir,ACCESS_READ);
        fib=AllocDosObject(DOS_FIB,NULL);
        if(lock && fib && Examine(lock,fib))while(ExNext(lock,fib) && board_count<MAX_ITEMS)
            if(fib->fib_DirEntryType>0 && fib->fib_DirEntryType!=ST_SOFTLINK &&
               strlen(dir)+strlen(fib->fib_FileName)+2<PATH_SIZE) {
                join(p,dir,fib->fib_FileName);board_add(p,name?name:"Project");
            }
        if(fib)FreeDosObject(DOS_FIB,fib);if(lock)UnLock(lock);free(name);
    }
    for(i=0;i<6;i++) {
        snprintf(title,sizeof(title),"\33P[%s]\33b%s (%d)",board_colors[i],board_names[i],board_counts[i]);
        set(board_titles[i],MUIA_Text_Contents,title);set(board_lists[i],MUIA_List_Quiet,FALSE);
    }
    set(board_detail,MUIA_Text_Contents,board_selected>=0?board_selection:
        board_count==MAX_ITEMS?"Showing first 128 matching workspaces. Filter by project to narrow the board.":
        "Select a task. Working is based on live jobs; Review waits for your decision.");
    board_loading=0;
}
static void queue_refresh(void)
{
    int i,active;char row[512],p[PATH_SIZE],*name;
    if(!queue_window || !value(queue_window,MUIA_Window_Open))return;
    active=value(queue_list,MUIA_List_Active);set(queue_list,MUIA_List_Quiet,TRUE);
    DoMethod(queue_list,MUIM_List_Clear);
    for(i=0;i<task_flow.count;i++) {
        join(p,task_flow.items[i].path,"name.txt");name=read_value(p,120);
        snprintf(row,sizeof(row),"%02d  %-15s  %-5s  %s",i+1,queue_names[task_flow.items[i].state],
                 task_flow.items[i].build?"Build":"Agent",name?name:task_flow.items[i].path);
        DoMethod(queue_list,MUIM_List_InsertSingle,row,MUIV_List_Insert_Bottom);free(name);
    }
    SetAttrs(queue_list,MUIA_List_Active,active<task_flow.count?active:MUIV_List_Active_Off,
             MUIA_List_Quiet,FALSE,TAG_DONE);
    /* Replacing equal-length lists under Quiet can retain visible cached rows
       in the installed MUI. Invalidate every row after the batch completes. */
    DoMethod(queue_list,MUIM_List_Redraw,MUIV_List_Redraw_All);
    set(queue_detail,MUIA_Text_Contents,!flow_valid?"Queue file is invalid. Restore Config/queue.json.bak and restart.":
        task_flow.paused?"Paused. Resume starts queued work. A restart or failure always pauses the queue.":
        "Enabled. Independent tasks use the worker limit; host relay requests run one at a time.");
}
static void flow_changed(void) {board_refresh();queue_refresh();}
static void flow_completed(const char *path,int result)
{
    if(flow_find(&task_flow,path)>=0) {flow_finish(&task_flow,path,result);flow_persist();}
    flow_changed();
}
static int flow_open_task(const char *path)
{
    char selected[PATH_SIZE],*sub;int i;
    if(!path || !managed_path(path) || !exists(path) || !save_current())return 0;
    strcpy(selected,path);strcpy(project,path);sub=strstr(project,"/Tasks/");if(sub)*sub=0;
    for(i=0;i<project_count;i++)if(!strcmp(project_paths[i],project))
        SetAttrs(projects,MUIA_NoNotify,TRUE,MUIA_List_Active,i,TAG_DONE);
    load_tasks();
    for(i=0;i<task_count;i++)if(!strcmp(task_paths[i],selected))
        SetAttrs(tasks,MUIA_NoNotify,TRUE,MUIA_List_Active,i,TAG_DONE);
    load_workspace(selected);set(win,MUIA_Window_Activate,TRUE);return 1;
}
static void flow_enqueue(int build)
{
    int n=task_flow.count;
    if(!flow_valid || busy(current) || !save_current()) {
        message("Cannot queue: task is busy, already queued, or could not be saved.");return;
    }
    if(!build && (!profile_prepare_launch() || !save_editor(prompt,"last-prompt.txt",8192)))return;
    if(!flow_add(&task_flow,current,build)) {message("Queue full or task already queued.");return;}
    if(!flow_persist()) {task_flow.count=n;return;}
    queue_marker(current,1);
    /* Verify the marker before permitting the dispatcher to proceed. */
    {char p[PATH_SIZE];join(p,current,".queued");if(!exists(p)) {
        task_flow.items[n].state=FLOW_ATTENTION;task_flow.paused=1;flow_persist();
        message("Queue paused: cannot reserve task drawer.");flow_changed();return;
    }}
    editor_readonly(source,TRUE);editor_readonly(prompt,TRUE);
    message(task_flow.paused?"Task queued. Open Queue and Resume when ready.":"Task queued.");
    flow_changed();flow_tick();
}
static void flow_tick(void)
{
    int n,i,running=0;struct FlowItem *item;
    if(!flow_valid || (n=flow_next(&task_flow))<0)return;
    for(i=0;i<16;i++)if(jobs[i].active)running++;
    if(running>=(task_flow.parallel>=1?task_flow.parallel:1)) {arm_timer();return;}
    item=&task_flow.items[n];
    if(!item->build && task_uses_relay(item->path) && host_job_active()) {arm_timer();return;}
    /* Persist intent before launch. A crash here must never replay uncertain work. */
    item->state=FLOW_RUNNING;
    if(!flow_persist()) {item->state=FLOW_QUEUED;return;}
    queue_marker(item->path,0);
    if(!managed_path(item->path) || !exists(item->path) || busy(item->path) ||
       !launch_at(item->path,item->build?"--build":"--agent",-1)) {
        flow_finish(&task_flow,item->path,20);flow_persist();
        if(managed_path(item->path) && exists(item->path) && !busy(item->path)) {
            char p[PATH_SIZE];join(p,item->path,"state.txt");save_text(p,"Needs attention",32);
        }
        message("Queue paused: task could not start. Check its drawer and existing workers.");
        flow_changed();
    }
}
static void flow_startup(void)
{
    int i;char p[PATH_SIZE];
    flow_valid=flow_load(&task_flow,QUEUE_FILE) || !exists(QUEUE_FILE);
    if(!flow_valid) {message("Queue file invalid. Restore Config/queue.json.bak and restart.");return;}
    for(i=0;i<task_flow.count;i++) {
        struct FlowItem *item=&task_flow.items[i];
        if(!managed_path(item->path) || !exists(item->path)) {item->state=FLOW_ATTENTION;continue;}
        if(item->state==FLOW_RUNNING) {
            if(worker_registered(item->path))attach_job(item->path);
            else {
                item->state=FLOW_ATTENTION;queue_marker(item->path,0);
                join(p,item->path,"state.txt");save_text(p,"Needs attention",32);
            }
        } else if(item->state==FLOW_QUEUED) {
            queue_marker(item->path,1);join(p,item->path,".queued");
            if(!exists(p))item->state=FLOW_ATTENTION;
        }
    }
    SetAttrs(queue_limit,MUIA_NoNotify,TRUE,MUIA_Cycle_Active,task_flow.parallel-1,TAG_DONE);
    if(task_flow.count)flow_persist();
    editor_readonly(source,!source_loaded || busy(current));
    editor_readonly(prompt,!prompt_loaded || busy(current));
    if(any_jobs())arm_timer();
}
static int flow_build_windows(void)
{
    Object *columns,*body;int i;
    columns=HGroup,MUIA_Group_Spacing,6,End;
    if(!columns)return 0;
    for(i=0;i<6;i++) {
        Object *column=VGroup,GroupFrame,MUIA_Weight,100,Child,
            board_titles[i]=label(board_names[i]),Child,board_list(&board_lists[i]),End;
        if(!column){MUI_DisposeObject(columns);return 0;}
        DoMethod(columns,OM_ADDMEMBER,column);
        notify(board_lists[i],MUIA_List_Active,MUIV_EveryTime,KB_SELECT0+i);
    }
    body=VGroup,Child,HGroup,Child,label("Search"),Child,board_search=entry(120),
        Child,board_project=CycleObject,MUIA_Cycle_Entries,board_projects,End,
        Child,button("Refresh",KB_REFRESH),End,Child,columns,
        Child,board_detail=label("Select a task"),Child,HGroup,
        Child,button("Open task",KB_TASK),Child,button("Queue agent",KB_AGENT),
        Child,button("Queue build",KB_BUILD),Child,button("Review",KB_REVIEW),
        Child,button("Mark done",KB_DONE),Child,button("Archive",KB_ARCHIVE),
        Child,button("Restore",KB_RESTORE),Child,button("Queue...",Q_OPEN),
        Child,button("Close",KB_CLOSE),End,End;
    board_window=WindowObject,MUIA_Window_Title,"Termic: Kanban board",
        MUIA_Window_ID,MAKE_ID('T','B','R','D'),MUIA_Window_Width,1100,
        MUIA_Window_Height,440,WindowContents,body,End;
    if(!board_window)return 0;
    DoMethod(app,OM_ADDMEMBER,board_window);
    notify(board_window,MUIA_Window_CloseRequest,TRUE,KB_CLOSE);
    notify(board_search,MUIA_String_Acknowledge,MUIV_EveryTime,KB_FILTER);
    notify(board_project,MUIA_Cycle_Active,MUIV_EveryTime,KB_FILTER);
    queue_window=WindowObject,MUIA_Window_Title,"Termic: Task queue",
        MUIA_Window_ID,MAKE_ID('T','Q','U','E'),MUIA_Window_Width,740,
        MUIA_Window_Height,360,WindowContents,VGroup,
        Child,queue_detail=label("Paused"),Child,HGroup,Child,label("Parallel work"),
        Child,queue_limit=CycleObject,MUIA_Cycle_Entries,queue_limits,End,
        Child,button("Broadcast...",Q_BROADCAST),End,Child,list(&queue_list),Child,HGroup,
        Child,button("Queue current agent",Q_AGENT),Child,button("Queue current build",Q_BUILD),
        Child,button("Open task",Q_TASK),Child,button("Up",Q_UP),Child,button("Down",Q_DOWN),
        Child,button("Remove",Q_REMOVE),End,Child,HGroup,
        Child,button("Resume",Q_RESUME),Child,button("Pause",Q_PAUSE),
        Child,button("Clear finished",Q_CLEAR),Child,HVSpace,Child,button("Close",Q_CLOSE),End,End,End;
    if(!queue_window)return 0;
    DoMethod(app,OM_ADDMEMBER,queue_window);
    notify(queue_window,MUIA_Window_CloseRequest,TRUE,Q_CLOSE);
    notify(queue_limit,MUIA_Cycle_Active,MUIV_EveryTime,Q_LIMIT);
    notify(queue_window,MUIA_Window_InputEvent,(ULONG)"-repeat r",Q_RESUME);
    notify(queue_window,MUIA_Window_InputEvent,(ULONG)"-repeat p",Q_PAUSE);
    notify(queue_window,MUIA_Window_InputEvent,(ULONG)"-repeat c",Q_CLOSE);return 1;
}
static void board_set_state(const char *task,int column)
{
    char path[PATH_SIZE];const char *state;
    if(!managed_path(task) || !exists(task)) {
        set(board_detail,MUIA_Text_Contents,"Task no longer exists in a managed project.");return;
    }
    state=board_drop_state(column,busy(task));
    if(!state) {
        set(board_detail,MUIA_Text_Contents,"Queued/running tasks cannot move. Working is assigned by a live job.");return;
    }
    join(path,task,"state.txt");
    if(save_text(path,state,32)) {
        strcpy(board_selection,task);task_row(task);board_refresh();
    }else set(board_detail,MUIA_Text_Contents,"Cannot save task state; task has not moved.");
}
static void flow_event(ULONG id)
{
    char path[PATH_SIZE];int n,i,changed=0;
    if(id>=KB_SELECT0 && id<=KB_SELECT5) {
        if(board_loading)return;
        i=id-KB_SELECT0;n=value(board_lists[i],MUIA_List_Active);
        if(n<0 || n>=board_counts[i])return;
        board_selected=board_rows[i][n];strcpy(board_selection,board_paths[board_selected]);
        board_loading=1;
        for(n=0;n<6;n++)if(n!=i)SetAttrs(board_lists[n],MUIA_NoNotify,TRUE,MUIA_List_Active,MUIV_List_Active_Off,TAG_DONE);
        board_loading=0;set(board_detail,MUIA_Text_Contents,board_selection);return;
    }
    switch(id) {
    case KB_OPEN:board_visible=1;board_refresh();set(board_window,MUIA_Window_Open,TRUE);break;
    case KB_CLOSE:board_visible=0;set(board_window,MUIA_Window_Open,FALSE);break;
    case KB_REFRESH:case KB_FILTER:board_refresh();break;
    case Q_OPEN:set(queue_window,MUIA_Window_Open,TRUE);queue_refresh();break;
    case Q_CLOSE:set(queue_window,MUIA_Window_Open,FALSE);break;
    case KB_TASK:case KB_AGENT:case KB_BUILD:case KB_REVIEW:
        if(board_selected<0){set(board_detail,MUIA_Text_Contents,"Select a task first.");break;}
        strcpy(path,board_paths[board_selected]);
        if(flow_open_task(path)) {
            if(id==KB_AGENT || id==KB_BUILD)flow_enqueue(id==KB_BUILD);
            else {
                board_visible=0;set(board_window,MUIA_Window_Open,FALSE);
                if(id==KB_REVIEW)change_open();
            }
        }break;
    case KB_DROP:
        if(*board_drop_path) {
            strcpy(path,board_drop_path);*board_drop_path=0;
            board_set_state(path,board_drop_column);
        }break;
    case KB_DONE:case KB_ARCHIVE:case KB_RESTORE:
        if(board_selected>=0)board_set_state(board_paths[board_selected],
            id==KB_DONE?BOARD_DONE:id==KB_ARCHIVE?BOARD_ARCHIVED:BOARD_BACKLOG);
        break;
    case Q_BROADCAST:broadcast_open();break;
    case Q_LIMIT:
        if(flow_valid) {
            int previous=task_flow.parallel;task_flow.parallel=value(queue_limit,MUIA_Cycle_Active)+1;
            if(!flow_persist())task_flow.parallel=previous;
            SetAttrs(queue_limit,MUIA_NoNotify,TRUE,MUIA_Cycle_Active,task_flow.parallel-1,TAG_DONE);
            flow_changed();flow_tick();
        }break;
    case Q_AGENT:case Q_BUILD:flow_enqueue(id==Q_BUILD);break;
    case Q_TASK:n=value(queue_list,MUIA_List_Active);
        if(n>=0 && n<task_flow.count && flow_open_task(task_flow.items[n].path))set(queue_window,MUIA_Window_Open,FALSE);break;
    case Q_RESUME:case Q_PAUSE:
        if(!flow_valid)break;
        task_flow.paused=id==Q_PAUSE;
        if(flow_persist()){flow_changed();flow_tick();}break;
    case Q_UP:case Q_DOWN:case Q_REMOVE:case Q_CLEAR:
        if(!flow_valid)break;
        {
            struct TaskFlow *previous=malloc(sizeof(*previous));
            if(!previous)break;*previous=task_flow;n=value(queue_list,MUIA_List_Active);
            if(id==Q_CLEAR) {
                for(i=task_flow.count-1;i>=0;i--)if(task_flow.items[i].state>FLOW_RUNNING) {
                    flow_remove(&task_flow,i);changed=1;
                }
            } else if(id==Q_REMOVE)changed=flow_remove(&task_flow,n);
            else changed=flow_move(&task_flow,n,id==Q_UP?-1:1);
            if(changed) {
                if(flow_persist()) {
                    for(i=0;i<previous->count;i++)if(previous->items[i].state!=FLOW_RUNNING &&
                        flow_find(&task_flow,previous->items[i].path)<0)queue_marker(previous->items[i].path,0);
                    editor_readonly(source,!source_loaded || busy(current));
                    editor_readonly(prompt,!prompt_loaded || busy(current));
                } else {task_flow=*previous;task_flow.paused=1;}
                flow_changed();
            }
            free(previous);
        }break;
    }
}
