/* Real MUI layout/resize acceptance. Uses synthetic, unsaved editor content;
 * no production files, settings, queue or credentials are written. */
#define TERMIC_APP_BASE "TERMICSCALECHECK"
#define TERMIC_APP_TITLE "Termic scaling acceptance"
#define main termic_workspace_main
#include "../gui.c"
#undef main
#include <dos/dosextens.h>
static int failed;
#define CHECK(x) do{if(!(x)){failed=__LINE__;goto done;}}while(0)
static const char *fixture_source="/* Resize demo: unsaved source */\n#include <stdio.h>\nint main(void)\n{\n    puts(\"Hello Amiga\");\n    return 0;\n}\n";
static void pump(void)
{
 ULONG sigs,id;int i;
 for(i=0;i<5;i++) {
  sigs=0;while((id=DoMethod(app,MUIM_Application_NewInput,&sigs))!=0){workspace_event(id);termic_monitor_event(id);}
  workspace_layout();termic_monitor_poll();Delay(2);
 }
}
static int same_source(void)
{
 char *s=(char *)DoMethod(source,MUIM_TextEditor_ExportText);int ok=s && !strcmp(s,fixture_source);
 if(!ok || !value(source,MUIA_TextEditor_HasChanged)) {
  char p[PATH_SIZE],t[120];snprintf(t,sizeof(t),"same=%d dirty=%lu length=%lu view=%d",ok,value(source,MUIA_TextEditor_HasChanged),s?(unsigned long)strlen(s):0,layout_view);
  join(p,project_root,"source-check.txt");save_text(p,t,128);
 }
 if(s)FreeVec(s);
 s=(char *)DoMethod(prompt,MUIM_TextEditor_ExportText);ok=ok && s && !strcmp(s,"Build a small Amiga program.\n");if(s)FreeVec(s);
 return ok && value(source,MUIA_TextEditor_HasChanged) && value(prompt,MUIA_TextEditor_HasChanged) &&
     value(source,MUIA_TextEditor_CursorY)==2 && value(source,MUIA_TextEditor_CursorX)==3;
}
static int geometry(Object *window,int width,int height)
{
 struct Window *w=(struct Window *)value(window,MUIA_Window_Window);
 if(!w)return 0;
 ChangeWindowBox(w,0,w->WScreen->BarHeight+1,width,height);pump();
 if(w->Width!=width || w->Height!=height){ChangeWindowBox(w,0,w->WScreen->BarHeight+1,width,height);pump();}
 {char p[PATH_SIZE],s[512];snprintf(s,sizeof(s),"wanted=%dx%d actual=%dx%d min=%dx%d max=%dx%d border=%d flags=%08lx mode=%d",width,height,(int)w->Width,(int)w->Height,(int)w->MinWidth,(int)w->MinHeight,(int)w->MaxWidth,(int)w->MaxHeight,(int)w->BorderTop,(unsigned long)w->Flags,layout_compact);join(p,project_root,"geometry.txt");save_text(p,s,512);}
 return w->Width==width && w->Height==height && w->BorderTop>0 && (w->Flags&WFLG_SIZEGADGET);
}
static int icon_row_valid(Object *row,Object *pane,int expected)
{
 struct List *children=(struct List *)value(row,MUIA_Group_ChildList);
 APTR childstate=children->lh_Head;Object *child;int count=0;LONG top=-1,right=-1;
 while((child=NextObject(&childstate)))if(value(child,MUIA_UserData)&AI_SMALL) {
  if(_mwidth(child)>24 || _mheight(child)>24)return 0;
  if(count && (_mtop(child)!=top || _mleft(child)<=right))return 0;
  if(!(char *)value(child,MUIA_ShortHelp))return 0;
  top=_mtop(child);right=_mleft(child)+_mwidth(child)-1;count++;
 }
 return count==expected && right<_mleft(pane)+_mwidth(pane);
}
int main(int argc,char **argv)
{
 char path[PATH_SIZE],report[2048],*text;int i,j,n=0,ticks=0;Object *shown=NULL;
 static const int sizes[][2]={{320,240},{340,280},{480,320},{640,480},{800,600},{1024,800},{340,280},{1024,800}};
 if(argc!=2 || strlen(argv[1])>120 || !strstr(argv[1],"/Acceptance/"))return 20;
 /* Optional Termic preferences may be unavailable when the workspace is
  * closed. A fixture must fail/fall back instead of opening volume requesters. */
 ((struct Process *)FindTask(NULL))->pr_WindowPtr=(APTR)-1;
 strcpy(project_root,argv[1]);strcpy(current,argv[1]);strcpy(project,argv[1]);strcpy(opened,"main.c");
 MUIMasterBase=OpenLibrary("muimaster.library",21);IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37);
 GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",37);DiskfontBase=OpenLibrary("diskfont.library",37);
 CHECK(MUIMasterBase && IntuitionBase && GfxBase && DiskfontBase);
 i=build_ui();
 snprintf(report,sizeof(report),"class=%ld app=%ld win=%ld profiles=%ld access=%ld board=%ld queue=%ld review=%ld pads=%ld git=%ld body=%ld center=%ld pages=%ld",(long)!!workspace_class,(long)!!app,(long)!!win,(long)!!profile_window,(long)!!access_window,(long)!!board_window,(long)!!queue_window,(long)!!change_window,(long)!!pad_window,(long)!!git_window,(long)!!workspace_body,(long)!!workspace_center,(long)!!pages);
 join(path,project_root,"diagnostics.txt");save_text(path,report,2048);
 CHECK(i);set(win,MUIA_Window_Title,"Termic: MUI scaling demo");
 profiles_defaults(&profile_live);profiles_picker_refresh();
 set(model,MUIA_String_Contents,"example-model");set(projectdrawer,MUIA_String_Contents,"Work:Projects");
 set(buildcommand,MUIA_String_Contents,"vc main.c -o Program.new");
 set(heading,MUIA_Text_Contents,"Demo project / Resize task");
 DoMethod(projects,MUIM_List_InsertSingle,"Demo project",MUIV_List_Insert_Bottom);
 DoMethod(tasks,MUIM_List_InsertSingle,"Resize task",MUIV_List_Insert_Bottom);
 DoMethod(files,MUIM_List_InsertSingle,"main.c",MUIV_List_Insert_Bottom);
 set(source,MUIA_TextEditor_Contents,fixture_source);set(source,MUIA_TextEditor_HasChanged,TRUE);
 set(prompt,MUIA_TextEditor_Contents,"Build a small Amiga program.\n");set(prompt,MUIA_TextEditor_HasChanged,TRUE);
 set(transcript,MUIA_TextEditor_Contents,"\33P[79C8FF]Agent: Ready to build.\n\33P[F2B66D]Log: Native tools available.\n");
 set(cli_output,MUIA_TextEditor_Contents,"Demo CLI output\n");set(command,MUIA_String_Contents,"Version");
 set(win,MUIA_Window_Open,TRUE);CHECK(value(win,MUIA_Window_Open));pump();workspace_select(1);pump();
 colour_pending=1;colour_delay=0;editor_recolour();
 SetAttrs(source,MUIA_TextEditor_HasChanged,TRUE,MUIA_TextEditor_CursorY,2,MUIA_TextEditor_CursorX,3,TAG_DONE);set(prompt,MUIA_TextEditor_HasChanged,TRUE);CHECK(same_source());
 for(i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++) {
  CHECK(geometry(win,sizes[i][0],sizes[i][1]));
  for(j=0;j<9;j++) {
   workspace_select(j);pump();CHECK(same_source());
   {struct Window *w=(struct Window *)value(win,MUIA_Window_Window);
    if(j==1)CHECK(_mtop(findtext)+_mheight(findtext)<w->Height-w->BorderBottom);
    if(j==8)CHECK(_mtop(command)+_mheight(command)<w->Height-w->BorderBottom);
   }
   if(j<5) {
    /* Exercise the notification used by native tab clicks, not just the
     * cycle/shortcut path. Both navigation controls must stay in sync. */
    set(pages,MUIA_Group_ActivePage,(j+1)%5);pump();
    CHECK(value(workspace_view,MUIA_Cycle_Active)==(j+1)%5);CHECK(same_source());
    workspace_select(j);pump();
   }
   CHECK(value(workspace_tabs,MUIA_ShowMe));
   CHECK(value(workspace_view,MUIA_ShowMe)==layout_compact);
   if(!layout_compact) {
    CHECK(_mleft(workspace_left)<_mleft(workspace_center) && _mleft(workspace_center)<_mleft(workspace_files));
    if(j==0) {
     CHECK(_mleft(prompt)<_mleft(profile_picker));
     if(sizes[i][0]>=1024)
      CHECK(_mleft(profile_fast)+_mwidth(profile_fast)<=_mleft(workspace_center)+_mwidth(workspace_center));
    }
   }
   if(!layout_compact || j==5)CHECK(icon_row_valid(project_actions,project_pane,3));
   if(!layout_compact || j==6)CHECK(icon_row_valid(task_actions,task_pane,5));
   CHECK(value(workspace_center,MUIA_ShowMe)==(!layout_compact || j<5));
   CHECK(value(workspace_files,MUIA_ShowMe)==(!layout_compact || j==7 || j==8));
   CHECK(value(workspace_left,MUIA_ShowMe)==(!layout_compact || j==5 || j==6));
  }
 }
 join(path,project_root,"ready.txt");CHECK(save_text(path,"MATRIX_PASS",128));
 for(ticks=0;ticks<18000;ticks++) {
  pump();join(path,project_root,"control.json");text=read_text(path,2048);
  if(text) {
   cJSON *r=cJSON_Parse(text),*mode=cJSON_GetObjectItemCaseSensitive(r,"panel");
   cJSON *width=cJSON_GetObjectItemCaseSensitive(r,"width"),*height=cJSON_GetObjectItemCaseSensitive(r,"height");
   cJSON *view=cJSON_GetObjectItemCaseSensitive(r,"view");const char *which=cJSON_IsString(mode)?mode->valuestring:"main";
   free(text);DeleteFile((STRPTR)path);
   if(!strcmp(which,"close")){cJSON_Delete(r);break;}
   if(shown && shown!=win)set(shown,MUIA_Window_Open,FALSE);
   shown=win;
   if(!strcmp(which,"agents"))shown=profile_window;
   else if(!strcmp(which,"access"))shown=access_window;
   else if(!strcmp(which,"board"))shown=board_window;
   else if(!strcmp(which,"queue"))shown=queue_window;
   else if(!strcmp(which,"review"))shown=change_window;
   else if(!strcmp(which,"scratchpad"))shown=pad_window;
   else if(!strcmp(which,"git"))shown=git_window;
   else if(!strcmp(which,"monitor")) {
    struct List *windows;APTR state;Object *o;
    CHECK(termic_monitor_open(app,1));
    windows=(struct List *)value(app,MUIA_Application_WindowList);state=windows->lh_Head;
    while((o=NextObject(&state))) {
     char *title=(char *)value(o,MUIA_Window_Title);
     if(title && !strcmp(title,"Termic: Activity")){shown=o;break;}
    }
    CHECK(shown!=win);
   }
   CHECK(cJSON_IsNumber(width) && cJSON_IsNumber(height) && width->valueint>=340 && height->valueint>=240);
   if(cJSON_IsNumber(view) && view->valueint>=0 && view->valueint<9)workspace_select(view->valueint);
   set(shown,MUIA_Window_Open,TRUE);CHECK(geometry(shown,width->valueint,height->valueint));
   set(shown,MUIA_Window_Activate,TRUE);WindowToFront((struct Window *)value(shown,MUIA_Window_Window));pump();
   CHECK(same_source());
   snprintf(report,sizeof(report),"{\"panel\":\"%s\",\"width\":%d,\"height\":%d,\"view\":%d,\"compact\":%d,\"editor\":[%ld,%ld,%ld,%ld],\"row\":[%ld,%ld,%ld,%ld],\"preserved\":true}",which,width->valueint,height->valueint,layout_view,layout_compact,
    (long)_mleft(source),(long)_mtop(source),(long)_mwidth(source),(long)_mheight(source),
    (long)_mleft(workspace_row),(long)_mtop(workspace_row),(long)_mwidth(workspace_row),(long)_mheight(workspace_row));
   join(path,project_root,"state.json");CHECK(save_text(path,report,2048));cJSON_Delete(r);n++;
  }
 }
 CHECK(n>0 && ticks<18000);
done:
 termic_monitor_close();
 if(app)MUI_DisposeObject(app);
 if(keyeditor_class)__termic_mui_unclass(MUIMasterBase,keyeditor_class);
 if(workspace_class)__termic_mui_unclass(MUIMasterBase,workspace_class);
 if(board_list_class)__termic_mui_unclass(MUIMasterBase,board_list_class);
 if(action_icon_class)__termic_mui_unclass(MUIMasterBase,action_icon_class);
 if(bodyfont)CloseFont(bodyfont);if(codefont)CloseFont(codefont);
 if(DiskfontBase)CloseLibrary(DiskfontBase);if(GfxBase)CloseLibrary((struct Library *)GfxBase);
 if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);if(MUIMasterBase)CloseLibrary(MUIMasterBase);
 snprintf(report,sizeof(report),"%s line %d",failed?"SCALING_FAIL":"SCALING_PASS",failed);
 join(path,project_root,"result.txt");save_text(path,report,128);return failed?20:0;
}
