/* Run only in a new Acceptance drawer. Configuration and tasks stay there.
 * Uses production UI, broadcast, dispatcher and installed native compiler.
 * No model request is made: broadcast copies are converted to build jobs. */
static int trace_queries,trace_drops,trace_source,trace_dest;
static void drag_trace(int drop,int index,int dest){if(drop)trace_drops++;else trace_queries++;trace_source=index;trace_dest=dest;}
#define TERMIC_BOARD_DRAG_TRACE(drop,index,dest) drag_trace(drop,index,dest)
static char fixture_queue[384],fixture_keymap[384];
#define QUEUE_FILE fixture_queue
#define KEYMAP_FILE fixture_keymap
#define main termic_workspace_main
#include "../gui.c"
#undef main
#define CHECK(x) do{if(!(x)){printf("WORKFLOW_FAIL line %d\n",__LINE__);goto done;}}while(0)
static int check_state(const char *root,const char *expected)
{
 char path[PATH_SIZE],*text;int ok;join(path,root,"state.txt");text=read_value(path,32);
 ok=text && !strcmp(text,expected);free(text);return ok;
}
static int selected_column(int column)
{
 int n;
 if(!board_counts[column])return 0;
 n=board_rows[column][0];strcpy(board_selection,board_paths[n]);board_selected=n;
 set(board_lists[column],MUIA_List_Active,0);return 1;
}
static int fixture_show(const char *mode)
{
 char path[PATH_SIZE],geometry[512];int i;size_t used=0;
 set(queue_window,MUIA_Window_Open,FALSE);set(board_window,MUIA_Window_Open,FALSE);board_visible=0;
 if(keymap_window)set(keymap_window,MUIA_Window_Open,FALSE);
 if(broadcast_window)set(broadcast_window,MUIA_Window_Open,FALSE);
 if(shortcut_window)set(shortcut_window,MUIA_Window_Open,FALSE);
 if(!strcmp(mode,"keymap"))keymap_open();
 else if(!strcmp(mode,"broadcast")){set(prompt,MUIA_TextEditor_Contents,"Build an independent demo application.\nReport the build result and changed files.\n");set(prompt,MUIA_TextEditor_HasChanged,FALSE);broadcast_open();set(broadcast_note,MUIA_Text_Contents,"Source: Demo project / Main workspace");DoMethod(broadcast_profiles,MUIM_List_Select,MUIV_List_Select_All,MUIV_List_Select_On,NULL);}
 else if(!strcmp(mode,"queue"))flow_event(Q_OPEN);
 else if(!strcmp(mode,"shortcuts"))show_shortcuts();
 else {
  board_visible=1;*board_selection=0;board_refresh();set(board_window,MUIA_Window_Open,TRUE);
  for(i=0;i<6;i++)used+=snprintf(geometry+used,sizeof(geometry)-used,"%d %ld %ld %ld\n",i,
    (long)value(board_lists[i],MUIA_LeftEdge),(long)value(board_lists[i],MUIA_TopEdge),(long)value(board_lists[i],MUIA_Width));
  trace_queries=trace_drops=0;
  join(path,project_root,"geometry.txt");if(!save_text(path,geometry,512))return 0;
 }
 join(path,project_root,"ready.txt");return save_text(path,mode,32);
}
int main(int argc,char **argv)
{
 char path[PATH_SIZE],*text;ULONG sigs,id;int result=20,i,show=argc==3,overlap=0,ticks=0;char mode[32];
 struct ShortcutBinding remembered[SHORTCUT_COUNT];
 if(argc<2 || strlen(argv[1])>120 || (!strstr(argv[1],"/Acceptance/") && !strstr(argv[1],":Acceptance/")))return 20;
 strcpy(project_root,argv[1]);join(project,project_root,"Demo");
 if(exists(project)){puts("Refusing existing fixture");return 20;}
 CHECK(mkdir_safe(project));strcpy(current,project);
 join(fixture_queue,project_root,"queue.json");join(fixture_keymap,project_root,"keymap.json");
 join(path,project,"name.txt");CHECK(save_text(path,"Demo project",120));
 join(path,project,"main.c");CHECK(save_text(path,"#include <stdio.h>\nint main(void){puts(\"Demo build\");return 0;}\n",SOURCE_LIMIT));
 join(path,project,"build-command.txt");CHECK(save_text(path,"Wait 3\nvc +aos68k -cpu=68020 -O1 main.c -o Program.new -lamiga\n",512));
 MUIMasterBase=OpenLibrary("muimaster.library",21);
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37);
 GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",37);DiskfontBase=OpenLibrary("diskfont.library",37);
 CHECK(MUIMasterBase && IntuitionBase && GfxBase && DiskfontBase);
 {struct TextAttr a={"helvetica.font",13,0,FPF_DISKFONT},b={"courier.font",13,0,FPF_DISKFONT};bodyfont=OpenDiskFont(&a);codefont=OpenDiskFont(&b);}
 app=ApplicationObject,MUIA_Application_Title,"Termic workflow acceptance",MUIA_Application_Base,"TERMICFLOWCHECK",End;CHECK(app);
 win=WindowObject,WindowContents,VGroup,Child,list(&tasks),Child,status=label("Fixture"),
  Child,projectname=entry(120),Child,editor(&prompt,0),Child,editor(&source,0),End,End;CHECK(win);
 DoMethod(app,OM_ADDMEMBER,win);
 CHECK(flow_build_windows());task_flow.paused=1;task_flow.parallel=2;SetAttrs(queue_limit,MUIA_NoNotify,TRUE,MUIA_Cycle_Active,1,TAG_DONE);project_count=1;strcpy(project_paths[0],project);
 profiles_defaults(&profile_live);profile_live.count=2;
 strcpy(profile_live.items[0].id,"demo-codex");strcpy(profile_live.items[0].name,"Demo Codex");profile_live.items[0].kind=5;
 profile_live.items[1]=profile_live.items[0];strcpy(profile_live.items[1].id,"demo-claude");strcpy(profile_live.items[1].name,"Demo Claude");profile_live.items[1].kind=6;
 keymap_load();keymap_open();CHECK(keymap_window && value(keymap_window,MUIA_Window_Open));
 memcpy(remembered,shortcut_map,sizeof(remembered));
 set(keymap_rows,MUIA_List_Active,0);set(keymap_key,MUIA_Cycle_Active,4);set(keymap_mod,MUIA_Cycle_Active,KEY_CTRL);keymap_event(KM_ASSIGN);
 CHECK(keymap_draft[0].code==0x51 && keymap_draft[0].mods==KEY_CTRL);
 keymap_event(KM_USE);CHECK(shortcut_action(0x51,KEY_CTRL,1,0,0)==KEY_SEND);
 keymap_open();keymap_event(KM_DEFAULTS);keymap_event(KM_CLOSE);CHECK(shortcut_map[0].code==0x51);
 keymap_open();keymap_event(KM_SAVE);CHECK(exists(fixture_keymap));shortcut_defaults(shortcut_map);keymap_load();CHECK(shortcut_map[0].code==0x51);
 keymap_open();keymap_event(KM_DEFAULTS);keymap_event(KM_USE);CHECK(!memcmp(shortcut_map,remembered,sizeof(remembered)));
 set(prompt,MUIA_TextEditor_Contents,"Build an independent demo application.\n");set(prompt,MUIA_TextEditor_HasChanged,FALSE);
 broadcast_open();CHECK(broadcast_window && broadcast_count==2);
 DoMethod(broadcast_profiles,MUIM_List_Select,MUIV_List_Select_All,MUIV_List_Select_On,NULL);
 {char longbase[PATH_SIZE];memset(longbase,'x',sizeof(longbase)-1);longbase[sizeof(longbase)-1]=0;
  CHECK(!broadcast_copy(current,longbase,&profile_live.items[0],"Demo",path));}
 broadcast_queue();CHECK(task_flow.count==2 && task_flow.paused && !any_jobs());
 CHECK(strcmp(task_flow.items[0].path,task_flow.items[1].path));
 for(i=0;i<2;i++) {
  join(path,task_flow.items[i].path,"task.txt");text=read_text(path,8192);CHECK(text && strstr(text,"independent demo"));free(text);
  join(path,task_flow.items[i].path,"main.c");text=read_text(path,SOURCE_LIMIT);CHECK(text && strstr(text,"Demo build"));free(text);
 }
 /* Relay ownership blocks the next host task even before its port appears. */
 jobs[0].active=jobs[0].host=1;strcpy(jobs[0].path,"RAM:ReservedFixture");task_flow.paused=0;flow_tick();
 CHECK(task_flow.items[0].state==FLOW_QUEUED);jobs[0].active=0;
 /* Exercise real independent builds; never launch the synthetic AI profiles. */
 task_flow.items[0].build=task_flow.items[1].build=1;flow_tick();flow_tick();
 CHECK(task_flow.items[0].state==FLOW_RUNNING && task_flow.items[1].state==FLOW_RUNNING);
 task_flow.paused=1;CHECK(flow_persist());
 for(i=0;i<200;i++){if(worker_registered(task_flow.items[0].path) && worker_registered(task_flow.items[1].path)){overlap=1;break;}Delay(1);}
 CHECK(overlap);puts("PARALLEL_WORKERS_REGISTERED");
 for(i=0;i<180 && any_jobs();i++){Delay(25);poll_jobs();}
 CHECK(!any_jobs() && task_flow.items[0].state==FLOW_COMPLETE && task_flow.items[1].state==FLOW_COMPLETE);
 for(i=0;i<2;i++){join(path,task_flow.items[i].path,"Program");CHECK(exists(path));}
 board_visible=1;board_refresh();set(board_window,MUIA_Window_Open,TRUE);CHECK(value(board_window,MUIA_Window_Open));
 CHECK(selected_column(BOARD_REVIEW));
 {struct BoardListData drag;struct IntuiMessage pointer;
  memset(&drag,0,sizeof(drag));memset(&pointer,0,sizeof(pointer));
  pointer.Class=IDCMP_MOUSEBUTTONS;pointer.Code=SELECTDOWN;
  pointer.MouseX=_left(board_lists[BOARD_REVIEW])+20;pointer.MouseY=_top(board_lists[BOARD_REVIEW])+8;
  board_pointer(&drag,board_lists[BOARD_REVIEW],&pointer);CHECK(drag.tracking);
  pointer.Class=IDCMP_MOUSEMOVE;pointer.Qualifier=IEQUALIFIER_LEFTBUTTON;pointer.MouseX+=20;
  board_pointer(&drag,board_lists[BOARD_REVIEW],&pointer);CHECK(drag.moved);
  pointer.Class=IDCMP_RAWKEY;pointer.Code=0x45;
  board_pointer(&drag,board_lists[BOARD_REVIEW],&pointer);CHECK(!drag.tracking && !*board_drop_path);
 }
 CHECK(DoMethod(board_lists[BOARD_WORKING],MUIM_DragQuery,board_lists[BOARD_REVIEW])==MUIV_DragQuery_Refuse);
 CHECK(DoMethod(board_lists[BOARD_DONE],MUIM_DragQuery,board_lists[BOARD_REVIEW])==MUIV_DragQuery_Accept);
 DoMethod(board_lists[BOARD_DONE],MUIM_DragDrop,board_lists[BOARD_REVIEW],0,0);
 strcpy(path,board_drop_path);flow_event(KB_DROP);CHECK(check_state(path,"Done"));
 queue_marker(path,1);board_set_state(path,BOARD_ARCHIVED);CHECK(check_state(path,"Done"));queue_marker(path,0);
 board_set_state(path,BOARD_REVIEW);CHECK(check_state(path,"Review"));
 puts("WORKFLOW_CHECKS_PASS");fflush(stdout);
 if(show){strncpy(mode,argv[2],31);mode[31]=0;CHECK(fixture_show(mode));}
 if(show)for(;;) {
  sigs=0;id=DoMethod(app,MUIM_Application_NewInput,&sigs);
  if(id==KB_CLOSE || id==KM_CLOSE || id==BC_CLOSE || id==Q_CLOSE || id==SHORTCUTSCLOSE)break;
  if(id==KB_DROP || (id>=KB_SELECT0 && id<=KB_SELECT5)) {
   flow_event(id);set(board_detail,MUIA_Text_Contents,"Demo tasks: drag Review to Done, or restore to Backlog.");
   if(id==KB_DROP && board_counts[BOARD_DONE]) {join(path,project_root,"drop.txt");CHECK(save_text(path,"DROP_DONE_PASS",32));}
  }
  if(id==KM_SELECT || id==KM_ASSIGN || id==KM_DEFAULTS || id==KM_USE || id==KM_SAVE || id==KM_CLOSE)keymap_event(id);
  if(id==Q_CLOSE)set(queue_window,MUIA_Window_Open,FALSE);
  if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C)break;
  if(++ticks>=25) {
   {char info[160];snprintf(info,sizeof(info),"queries=%d drops=%d source=%d dest=%d selected=%d active=%ld loading=%d pending=%d dragtype=%ld\n",
     trace_queries,trace_drops,trace_source,trace_dest,board_selected,(long)value(board_lists[BOARD_REVIEW],MUIA_List_Active),board_loading,!!*board_drop_path,(long)value(board_lists[BOARD_REVIEW],MUIA_Listview_DragType));
    join(path,project_root,"trace.txt");save_text(path,info,160);}
   ticks=0;join(path,project_root,"control.txt");text=read_value(path,32);
   if(text && strcmp(text,mode)) {
    if(!strcmp(text,"close")){free(text);break;}
    strncpy(mode,text,31);mode[31]=0;free(text);CHECK(fixture_show(mode));
   }else free(text);
  }
  Delay(1); /* Short-lived fixture controller only; production uses event waits. */
 }
 result=0;
done:
 if(any_jobs()) {
  /* Finish isolated builds before disposing their UI; never kill live tasks. */
  task_flow.paused=1;for(i=0;i<180 && any_jobs();i++){Delay(25);poll_jobs();}
 }
 if(app)MUI_DisposeObject(app);
 if(keyeditor_class)__termic_mui_unclass(MUIMasterBase,keyeditor_class);
 if(board_list_class)__termic_mui_unclass(MUIMasterBase,board_list_class);
 if(action_icon_class)__termic_mui_unclass(MUIMasterBase,action_icon_class);
 if(bodyfont)CloseFont(bodyfont);if(codefont)CloseFont(codefont);
 if(DiskfontBase)CloseLibrary(DiskfontBase);if(GfxBase)CloseLibrary((struct Library *)GfxBase);
 if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);if(MUIMasterBase)CloseLibrary(MUIMasterBase);
 join(path,project_root,"result.txt");save_text(path,result?"FAIL":"WORKFLOW_WINDOW_PASS",128);
 if(!result)puts("WORKFLOW_WINDOW_PASS");return result;
}
