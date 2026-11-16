/* Explicit hardware acceptance using the real queue, worker and host proxy.
 * All project/queue/review state lives in a pre-staged Acceptance drawer.
 * Start/accept/close are controlled separately so generated sources can be
 * inspected before accepting their review entries or marking the task Done. */
static char fixture_queue[384];
#define QUEUE_FILE fixture_queue
#define main termic_workspace_main
#include "../gui.c"
#undef main
static int failure_line;
#define CHECK(x) do{if(!(x)){failure_line=__LINE__;goto done;}}while(0)
static int phase(const char *name)
{
 char path[PATH_SIZE];join(path,project_root,"phase.txt");return save_text(path,name,128);
}
int main(int argc,char **argv)
{
 char path[PATH_SIZE],task[PATH_SIZE],*text;ULONG sigs,id;int result=20,ticks=0,total=0,stage=0,i;
 if(argc!=2 || strlen(argv[1])>120 || !strstr(argv[1],"/Acceptance/"))return 20;
 strcpy(project_root,argv[1]);join(project,project_root,"Project");strcpy(current,project);
 join(task,project,"Tasks/system-summary");join(path,task,"task.txt");if(!exists(path))return 20;
 join(fixture_queue,project_root,"queue.json");if(exists(fixture_queue))return 20;
 MUIMasterBase=OpenLibrary("muimaster.library",21);
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37);
 GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",37);DiskfontBase=OpenLibrary("diskfont.library",37);
 CHECK(MUIMasterBase && IntuitionBase && GfxBase && DiskfontBase);
 {struct TextAttr a={"helvetica.font",13,0,FPF_DISKFONT},b={"courier.font",13,0,FPF_DISKFONT};bodyfont=OpenDiskFont(&a);codefont=OpenDiskFont(&b);}
 app=ApplicationObject,MUIA_Application_Title,"Termic agent workflow acceptance",MUIA_Application_Base,"TERMICAICHECK",End;CHECK(app);
 win=WindowObject,WindowContents,VGroup,Child,list(&tasks),Child,status=label("Acceptance"),
  Child,projectname=entry(120),Child,editor(&prompt,0),Child,editor(&source,0),End,End;CHECK(win);
 DoMethod(app,OM_ADDMEMBER,win);
 CHECK(profiles_load(&profile_live));i=profile_find(&profile_live,"codex");CHECK(i>=0 && profile_live.items[i].kind==5 && !profile_live.items[i].readonly);
 CHECK(flow_build_windows());task_flow.paused=1;task_flow.parallel=1;
 project_count=1;strcpy(project_paths[0],project);board_visible=1;board_refresh();
 set(board_window,MUIA_Window_Open,TRUE);CHECK(phase("backlog"));
 while(total++<18000) {
  sigs=0;id=DoMethod(app,MUIM_Application_NewInput,&sigs);
  if(id==KB_CLOSE || id==Q_CLOSE || id==CR_CLOSE)break;
  if(id>=KB_SELECT0 && id<=KB_SELECT5)flow_event(id);
  if(id==CR_SELECT || id==CR_MODE)change_event(id);
  if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C)break;
  if(++ticks>=5) {
   ticks=0;if(any_jobs())poll_jobs();
   if(stage==1 && !any_jobs()) {
    CHECK(task_flow.count==1 && task_flow.items[0].state==FLOW_COMPLETE);
    join(path,task,"state.txt");text=read_value(path,32);i=text && !strcmp(text,"Review");free(text);CHECK(i);
    join(path,task,".command.log");text=read_text(path,32768);i=text && strstr(text,"SYSTEM_SUMMARY_OK");free(text);CHECK(i);
    strcpy(change_root,task);CHECK(change_build_window());change_refresh();CHECK(change_set.count>0);
    set(board_window,MUIA_Window_Open,FALSE);set(change_window,MUIA_Window_Open,TRUE);change_show();
    stage=2;CHECK(phase("review"));
   }
   join(path,project_root,"control.json");text=read_text(path,1024);
   if(text) {
    cJSON *control=cJSON_Parse(text),*command=cJSON_GetObjectItemCaseSensitive(control,"command");
    free(text);DeleteFile((STRPTR)path);
    if(cJSON_IsString(command) && !strcmp(command->valuestring,"start") && stage==0) {
     CHECK(flow_add(&task_flow,task,0));CHECK(flow_persist());queue_marker(task,1);
     task_flow.paused=0;CHECK(flow_persist());flow_tick();CHECK(any_jobs());stage=1;CHECK(phase("working"));
    } else if(cJSON_IsString(command) && !strcmp(command->valuestring,"accept") && stage==2) {
     while(change_set.count) {
      int before=change_set.count;set(change_files,MUIA_List_Active,0);change_event(CR_ACCEPT);CHECK(change_set.count<before);
     }
     set(change_window,MUIA_Window_Open,FALSE);set(board_window,MUIA_Window_Open,TRUE);
     board_set_state(task,BOARD_DONE);join(path,task,"state.txt");text=read_value(path,32);
     i=text && !strcmp(text,"Done");free(text);CHECK(i);
     set(board_detail,MUIA_Text_Contents,"System summary: generated, built, executed and reviewed on the Amiga.");
     stage=3;CHECK(phase("done"));
    } else if(cJSON_IsString(command) && !strcmp(command->valuestring,"close")) {
     cJSON_Delete(control);if(stage==3)result=0;break;
    }
    cJSON_Delete(control);
   }
  }
  Delay(5);
 }
done:
 if(any_jobs()) {
  task_flow.paused=1;strcpy(current,task);cancel();strcpy(current,project);
  for(i=0;i<600 && any_jobs();i++){Delay(25);poll_jobs();}
 }
 if(app)MUI_DisposeObject(app);
 if(keyeditor_class)__termic_mui_unclass(MUIMasterBase,keyeditor_class);
 if(board_list_class)__termic_mui_unclass(MUIMasterBase,board_list_class);
 if(action_icon_class)__termic_mui_unclass(MUIMasterBase,action_icon_class);
 if(bodyfont)CloseFont(bodyfont);if(codefont)CloseFont(codefont);
 if(DiskfontBase)CloseLibrary(DiskfontBase);if(GfxBase)CloseLibrary((struct Library *)GfxBase);
 if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);if(MUIMasterBase)CloseLibrary(MUIMasterBase);
 {char report[80];snprintf(report,sizeof(report),result?"AGENT_WORKFLOW_FAIL line %d stage %d":"AGENT_WORKFLOW_PASS",failure_line,stage);
  join(path,project_root,"result.txt");save_text(path,report,128);}
 return result;
}
