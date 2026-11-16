/* Broadcast creates separate task copies, then queues them paused for review. */
enum { BC_CLOSE=5800,BC_QUEUE };
static Object *broadcast_window,*broadcast_profiles,*broadcast_note;
static char broadcast_ids[PROFILE_MAX][32],broadcast_source[PATH_SIZE];
static int broadcast_count;
static void broadcast_open(void)
{
 int i;char *text,*styled;Object *view;
 if(!flow_valid || busy(current) || !save_current()) {message("Save and finish the source task before broadcasting.");return;}
 if(!broadcast_window) {
  view=ListviewObject,MUIA_Listview_MultiSelect,MUIV_Listview_MultiSelect_Default,
   MUIA_Listview_List,broadcast_profiles=ListObject,InputListFrame,
   MUIA_Font,bodyfont?(ULONG)bodyfont:MUIV_Font_List,
   MUIA_List_ConstructHook,MUIV_List_ConstructHook_String,
   MUIA_List_DestructHook,MUIV_List_DestructHook_String,End,End;
  broadcast_window=ResponsiveWindowObject,MUIA_Window_Title,"Termic: Broadcast to agents",
   MUIA_Window_ID,MAKE_ID('T','B','C','S'),MUIA_Window_Width,820,MUIA_Window_Height,500,
   WindowContents,VGroup,Child,label("Select agents (Shift-click for several). Each gets a new, independent copy of this task."),
   Child,HGroup,Child,VGroup,MUIA_Weight,35,Child,label("Agent profiles"),Child,view,End,
   Child,VGroup,MUIA_Weight,65,Child,label("Shared instruction"),Child,editor(&broadcast_text,0),End,End,
   Child,broadcast_note=label(""),
   Child,label("Queue copies pauses dispatch. Review them, set the worker limit, then Resume. YOLO is off."),
   Child,HGroup,Child,button("Queue copies",BC_QUEUE),Child,HVSpace,Child,button("Cancel",BC_CLOSE),End,End,End;
  if(!broadcast_window){message("Cannot open broadcast.");return;}
  set(broadcast_text,MUIA_TextEditor_Pen,0xffe6edf3UL);
  DoMethod(app,OM_ADDMEMBER,broadcast_window);notify(broadcast_window,MUIA_Window_CloseRequest,TRUE,BC_CLOSE);
 }
 strcpy(broadcast_source,current);broadcast_count=0;DoMethod(broadcast_profiles,MUIM_List_Clear);
 for(i=0;i<profile_live.count;i++)if(profile_live.items[i].visible &&
    (profile_live.items[i].kind<=1 || profile_is_relay(profile_live.items[i].kind))) {
  strcpy(broadcast_ids[broadcast_count++],profile_live.items[i].id);
  DoMethod(broadcast_profiles,MUIM_List_InsertSingle,profile_live.items[i].name,MUIV_List_Insert_Bottom);
 }
 text=(char *)DoMethod(prompt,MUIM_TextEditor_ExportText);
 styled=syntax_colours(text?text:"",0);
 set(broadcast_text,MUIA_TextEditor_Contents,styled?styled:(text?text:""));
 free(styled);if(text)FreeVec(text);
 set(broadcast_text,MUIA_TextEditor_Pen,0xffe6edf3UL);
 set(broadcast_note,MUIA_Text_Contents,broadcast_source);set(broadcast_window,MUIA_Window_Open,TRUE);
}
static int broadcast_copy(const char *source,const char *base,const struct AgentProfile *profile,
                          const char *instruction,char *target)
{
 char drawer[PATH_SIZE],path[PATH_SIZE],name[120];BPTR lock;int i;
 if(strlen(base)+sizeof("/Tasks/broadcast-0000/last-prompt.txt")>PATH_SIZE)return 0;
 join(drawer,base,"Tasks");if(!mkdir_safe(drawer))return 0;
 for(i=1;i<=9999;i++) {
  snprintf(target,PATH_SIZE,"%s/broadcast-%04d",drawer,i);
  if(exists(target))continue;
  lock=CreateDir((STRPTR)target);if(!lock)return 0;UnLock(lock);break;
 }
 if(i>9999)return 0;
 snprintf(name,sizeof(name),"Broadcast: %s",profile->name);
 if(!clone_sources(source,target,0))goto fail;
 join(path,target,"name.txt");if(!save_text(path,name,120))goto fail;
 join(path,target,"task.txt");if(!save_text(path,instruction,8192))goto fail;
 join(path,target,"last-prompt.txt");if(!save_text(path,instruction,8192))goto fail;
 join(path,target,"agent-profile");if(!save_text(path,profile->id,31))goto fail;
 join(path,target,"agent-yolo");if(!save_text(path,"0",1))goto fail;
 join(path,target,"state.txt");if(!save_text(path,"Ready",32))goto fail;
 return 1;
fail:
 join(path,target,"state.txt");save_text(path,"Needs attention",32);return 0;
}
static void broadcast_queue(void)
{
 LONG row=MUIV_List_NextSelected_Start;int chosen[PROFILE_MAX],count=0,i,n,added=0,ok=1;
 char target[PATH_SIZE],path[PATH_SIZE],notice[200],*text,*p;
 if(!flow_valid || strcmp(current,broadcast_source) || busy(current) || !save_current()) {
  set(broadcast_note,MUIA_Text_Contents,"Source changed or is busy. Close and reopen Broadcast from the intended task.");return;
 }
 for(;;) {
  DoMethod(broadcast_profiles,MUIM_List_NextSelected,&row);if(row==MUIV_List_NextSelected_End)break;
  if(row<0 || row>=broadcast_count || count>=PROFILE_MAX)return;
  n=profile_find(&profile_live,broadcast_ids[row]);if(n<0 || (profile_live.items[n].kind>1 && !profile_is_relay(profile_live.items[n].kind))){set(broadcast_note,MUIA_Text_Contents,"Profile changed. Reopen Broadcast.");return;}
  chosen[count++]=n;
 }
 if(!count || task_flow.count+count>FLOW_MAX) {
  set(broadcast_note,MUIA_Text_Contents,!count?"Select at least one agent.":"Not enough queue slots. Remove finished entries first.");return;
 }
 text=(char *)DoMethod(broadcast_text,MUIM_TextEditor_ExportText);if(!text)return;
 for(p=text;isspace((unsigned char)*p);p++);
 if(!*p || strlen(text)>8192) {FreeVec(text);set(broadcast_note,MUIA_Text_Contents,"Enter an instruction of 1 to 8192 bytes.");return;}
 task_flow.paused=1;if(!flow_persist()){FreeVec(text);return;}
 for(i=0;i<count;i++) {
  *target=0;
  if(!broadcast_copy(current,project,&profile_live.items[chosen[i]],text,target)){ok=0;break;}
  n=task_flow.count;
  if(!flow_add(&task_flow,target,0) || !flow_persist()) {task_flow.count=n;ok=0;break;}
  queue_marker(target,1);join(path,target,".queued");
  if(!exists(path)) {task_flow.items[n].state=FLOW_ATTENTION;flow_persist();ok=0;break;}
  added++;
 }
 FreeVec(text);refresh_mcp_tasks();flow_changed();
 snprintf(notice,sizeof(notice),ok?"%d task copies queued, paused. Open Queue, inspect the copies, then Resume.":
     "%d copies queued, paused. Storage failed; inspect created tasks before resuming. No jobs started.",added);
 set(broadcast_note,MUIA_Text_Contents,notice);
 /* Keep partial failure visible. Success exposes the queue for review. */
 if(ok){set(broadcast_window,MUIA_Window_Open,FALSE);flow_event(Q_OPEN);message(notice);}
}
static int broadcast_event(ULONG id)
{
 switch(id) {
 case BC_CLOSE:set(broadcast_window,MUIA_Window_Open,FALSE);return 1;
 case BC_QUEUE:broadcast_queue();return 1;
 }
 return 0;
}
