/* Documentation captures of the production widgets with synthetic data.
 * No live preferences, credentials, workspaces or process rows are loaded.
 * Compile with the same GUI units as test_responsive_window.c. */
#define TERMIC_APP_BASE "TERMICGALLERY"
#define TERMIC_APP_TITLE "Termic documentation gallery"
#define profiles_load gallery_profiles_load
#define read_text gallery_read_text
#define main termic_workspace_main
#include "../gui.c"
#undef main
#undef read_text
#undef profiles_load
char *read_text(const char *,size_t);
static const char *demo_source="/* Hello Amiga: demonstration project */\n#include <stdio.h>\n\nint main(void)\n{\n    puts(\"Hello Amiga\");\n    return 0;\n}\n";
int gallery_profiles_load(struct AgentProfiles *p) {profiles_defaults(p);return 1;}
char *gallery_read_text(const char *path,size_t limit)
{
 char mapped[PATH_SIZE],*s;
 if(!strcmp(path,"Termic:Termic.guide")) {
  join(mapped,project_root,"Termic.guide");return read_text(mapped,limit);
 }
 if(!strcmp(path,"Termic:Prompts/defaults.json")) {
  join(mapped,project_root,"defaults.json");return read_text(mapped,limit);
 }
 if(!strcmp(path,"ENV:Termic/prompts.json") || !strcmp(path,"Termic:Config/prompts.json")) {
  s=malloc(3);if(s)strcpy(s,"{}");return s;
 }
 if(strncmp(path,project_root,strlen(project_root)) || path[strlen(project_root)]!='/')return NULL;
 return read_text(path,limit);
}
static void gallery_pump(void)
{
 ULONG sigs,id;int i;
 for(i=0;i<6;i++) {
  while((id=DoMethod(app,MUIM_Application_NewInput,&sigs))!=0) {
   workspace_event(id);guide_event(id);termic_monitor_event(id);
   if(id==ED_STATUS)source_editor_status();
  }
  workspace_layout();termic_monitor_poll();Delay(2);
 }
}
static void gallery_rows(Object *o,const char **rows)
{
 DoMethod(o,MUIM_List_Clear);
 while(*rows)DoMethod(o,MUIM_List_InsertSingle,*rows++,MUIV_List_Insert_Bottom);
}
static int gallery_seed(void)
{
 int i;char title[100];
 static const char *pr[]={"Hello Amiga","Process tools",NULL};
 static const char *ta[]={"Offline questions","Build greeting","Review changes",NULL};
 static const char *fi[]={"main.c","ReadMe","AGENTS.md","SKILL.md",NULL};
 static const char *cards[]={"Plan demo | Hello Amiga","Check headers | Hello Amiga","Build greeting | Hello Amiga","Review main.c | Hello Amiga","Add ReadMe | Hello Amiga","Old example | Hello Amiga"};
 static const char *qu[]={"01  Queued          Agent  Review greeting","02  Queued          Build  Build greeting","03  Finished        Build  Compile example",NULL};
 set(win,MUIA_Window_Title,"Termic Amiga " TERMIC_VERSION " - Demo");
 gallery_rows(projects,pr);gallery_rows(tasks,ta);gallery_rows(files,fi);
 set(heading,MUIA_Text_Contents,"Hello Amiga / Offline questions");
 set(projectname,MUIA_String_Contents,"Hello Amiga");set(taskname,MUIA_String_Contents,"Offline questions");
 set(source,MUIA_TextEditor_Contents,demo_source);set(prompt,MUIA_TextEditor_Contents,"what is the capital of peru\n");
 set(transcript,MUIA_TextEditor_Contents,"\33P[79C8FF]Tiny 2.1: offline question answering\n\n\33P[F2B66D]Demo: Tiny is selected for this task.\n\33P[F2B66D]Runs locally on the Amiga 68k CPU.\n\33P[F2B66D]No API key or Mac proxy is required.\n\n\33P[79C8FF]Enter a short question below to start.\n\33P[F2B66D]Maximum: 72 printable ASCII characters.\n\33P[F2B66D]No file/build tools or conversation memory.\n");
 set(cli_output,MUIA_TextEditor_Contents,"Demo CLI transcript\n\n> Echo Hello Amiga\nHello Amiga\n\nCLI, ARexx and MicroPython\nuse the mode selector above.\n");
 set(command,MUIA_String_Contents,"Echo Hello Amiga");
 set(output,MUIA_TextEditor_Contents,"Demonstration build log\n\nvc +aos68k -cpu=68020 -O1 main.c\n  -o Program.new -lamiga\n\nBuild completed.\nProgram.new promoted to Program.\n");
 set(review,MUIA_TextEditor_Contents,"Demo change summary\n\nModified: main.c\nAdded: ReadMe\n\nOpen Review edits to inspect the diff.\n");
 set(model,MUIA_String_Contents,"Default API model");set(projectdrawer,MUIA_String_Contents,"Work:Projects");
 set(buildcommand,MUIA_String_Contents,"vc +aos68k -cpu=68020 -O1 main.c -o Program.new -lamiga");
 set(keyfield,MUIA_String_Contents,"");set(status,MUIA_Text_Contents,"Documentation demo. Projects, activity and process data are synthetic.");
 profile_add(&profile_live,5,-1);profile_add(&profile_live,6,-1);profile_add(&profile_live,7,-1);
 profile_add(&profile_live,3,-1);profile_add(&profile_live,4,-1);
 strcpy(profile_live.items[4].model,"sonnet");strcpy(profile_live.items[5].model,"auto");
 profile_draft=profile_live;profiles_picker_refresh();profile_refresh(2);
 if(value(profile_picker,MUIA_Cycle_Active)!=2)return 0;
 set(access_enabled,MUIA_Cycle_Active,1);
 for(i=0;i<6;i++) {
  DoMethod(board_lists[i],MUIM_List_InsertSingle,cards[i],MUIV_List_Insert_Bottom);
  snprintf(title,sizeof(title),"\33P[%s]\33b%s (1)",board_colors[i],board_names[i]);
  set(board_titles[i],MUIA_Text_Contents,title);
 }
 set(board_detail,MUIA_Text_Contents,"Demonstration tasks. Drag between columns to track progress.");
 gallery_rows(queue_list,qu);set(queue_limit,MUIA_Cycle_Active,1);
 set(queue_detail,MUIA_Text_Contents,"Demo queue paused. Two workers selected; no jobs are running.");task_flow.paused=1;
 DoMethod(change_files,MUIM_List_InsertSingle,"main.c",MUIV_List_Insert_Bottom);
 SetAttrs(change_files,MUIA_NoNotify,TRUE,MUIA_List_Active,0,TAG_DONE);
 {char *s=change_colours("--- main.c (before)\n+++ main.c (after)\n@@ -3,5 +3,5 @@\n int main(void)\n {\n-    puts(\"Hello\");\n+    puts(\"Hello Amiga\");\n     return 0;\n }\n",1);
 if(!s)return 0;set(change_text,MUIA_TextEditor_Contents,s);free(s);}
 set(change_note,MUIA_Text_Contents,"Demonstration diff. Green + added; red - removed; blue headers.");
 shortcut_defaults(shortcut_map);shortcut_ready=1;
 return 1;
}
int main(int argc,char **argv)
{
 char path[PATH_SIZE],report[512],*text;Object *shown=NULL;int fail=0,ticks=0,captures=0;
 if(argc!=2 || strlen(argv[1])>120 || !strstr(argv[1],"/Acceptance/"))return 20;
 ((struct Process *)FindTask(NULL))->pr_WindowPtr=(APTR)-1;
 strcpy(project_root,argv[1]);strcpy(project,argv[1]);strcpy(current,argv[1]);strcpy(opened,"main.c");
 MUIMasterBase=OpenLibrary("muimaster.library",21);IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37);
 GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",37);DiskfontBase=OpenLibrary("diskfont.library",37);
#define REQUIRE(x) do {if(!(x)){fail=__LINE__;goto done;}}while(0)
 REQUIRE(MUIMasterBase && IntuitionBase && GfxBase && DiskfontBase);
 monitor_fixture=1;REQUIRE(build_ui());REQUIRE(gallery_seed());
 set(win,MUIA_Window_Open,TRUE);gallery_pump();workspace_select(1);gallery_pump();
 colour_pending=1;colour_delay=0;editor_recolour();
 set(source,MUIA_TextEditor_HasChanged,FALSE);set(prompt,MUIA_TextEditor_HasChanged,FALSE);
 join(path,project_root,"ready.txt");REQUIRE(save_text(path,"GALLERY_READY",128));
 for(ticks=0;ticks<36000;ticks++) {
  gallery_pump();join(path,project_root,"control.json");text=read_text(path,2048);
  if(text) {
   cJSON *r=cJSON_Parse(text),*p=cJSON_GetObjectItemCaseSensitive(r,"panel");
   const char *mode=cJSON_IsString(p)?p->valuestring:"main";int width,height,view;
   struct Window *w;
   free(text);DeleteFile((STRPTR)path);REQUIRE(r);
   if(!strcmp(mode,"close")){cJSON_Delete(r);break;}
   if(shown && shown!=win)set(shown,MUIA_Window_Open,FALSE);
   shown=win;
   if(!strcmp(mode,"agents") || !strcmp(mode,"tiny") || !strcmp(mode,"claude") || !strcmp(mode,"cursor") || !strcmp(mode,"codex")) {
    profile_refresh(!strcmp(mode,"claude")?4:!strcmp(mode,"cursor")?5:!strcmp(mode,"codex")?3:2);shown=profile_window;
   } else if(!strcmp(mode,"access"))shown=access_window;
   else if(!strcmp(mode,"board"))shown=board_window;
   else if(!strcmp(mode,"queue"))shown=queue_window;
   else if(!strcmp(mode,"review"))shown=change_window;
   else if(!strncmp(mode,"guide",5)) {
    int link;
    open_help();REQUIRE(guide_window && value(guide_window,MUIA_Window_Open));
    guide_event(HG_CONTENTS);REQUIRE(guide_current==guide_find(&guide_doc,"MAIN"));
    guide_event(HG_INDEX);REQUIRE(guide_current==guide_find(&guide_doc,"Index"));
    guide_event(HG_BACK);REQUIRE(guide_current==guide_find(&guide_doc,"MAIN"));
    for(link=0;link<guide_link_count;link++)if(guide_targets[link].target==guide_find(&guide_doc,"Start"))break;
    REQUIRE(link<guide_link_count);set(guide_links,MUIA_Cycle_Active,link);guide_event(HG_LINK);
    REQUIRE(guide_current==guide_find(&guide_doc,"Start"));guide_event(HG_BACK);
    set(guide_chapters,MUIA_Cycle_Active,guide_find(&guide_doc,"TINY"));gallery_pump();
    REQUIRE(guide_current==guide_find(&guide_doc,"TINY"));
    guide_event(HG_PREV);guide_event(HG_NEXT);REQUIRE(guide_current==guide_find(&guide_doc,"TINY"));
    set(guide_search,MUIA_String_Contents,"Tiny");guide_event(HG_FIND);
    REQUIRE(strstr((char *)value(guide_note,MUIA_Text_Contents),"Match selected"));
    set(guide_search,MUIA_String_Contents,"no-such-fixture-phrase");guide_event(HG_FIND);
    REQUIRE(strstr((char *)value(guide_note,MUIA_Text_Contents),"No further match"));
    set(guide_search,MUIA_String_Contents,"");
    guide_event(HG_CLOSE);REQUIRE(!value(guide_window,MUIA_Window_Open));open_help();
    REQUIRE(value(guide_window,MUIA_Window_Open) && value(guide_text,MUIA_TextEditor_ReadOnly));
    REQUIRE(guide_show(guide_find(&guide_doc,!strcmp(mode,"guide-tiny")?"TINY":"MAIN"),0));
    shown=guide_window;
   }
   else if(!strcmp(mode,"about")){open_about();shown=about_window;}
   else if(!strcmp(mode,"prompts")){prompts_open();shown=prompts_window;}
   else if(!strcmp(mode,"shortcuts")){show_shortcuts();shown=shortcut_window;}
   else if(!strcmp(mode,"keymap")){keymap_open();shown=keymap_window;}
   else if(!strcmp(mode,"broadcast")) {
    broadcast_open();shown=broadcast_window;
    if(shown){set(broadcast_text,MUIA_TextEditor_Contents,"\33P[E6EDF3]Review the Hello Amiga example.\n\33P[E6EDF3]Suggest clear comments and a short ReadMe.\n\33P[E6EDF3]Report changed files and build results.\n");
     set(broadcast_note,MUIA_Text_Contents,"Demo source: Hello Amiga / Build greeting");
     DoMethod(broadcast_profiles,MUIM_List_Select,MUIV_List_Select_All,MUIV_List_Select_On,NULL);}
   }
   REQUIRE(shown);
   p=cJSON_GetObjectItemCaseSensitive(r,"view");view=cJSON_IsNumber(p)?p->valueint:1;REQUIRE(view>=0 && view<10);
   workspace_select(view);gallery_pump();
   if(!strcmp(mode,"zz9000"))termic_monitor_event(5811);
   p=cJSON_GetObjectItemCaseSensitive(r,"width");width=cJSON_IsNumber(p)?p->valueint:0;
   p=cJSON_GetObjectItemCaseSensitive(r,"height");height=cJSON_IsNumber(p)?p->valueint:0;
   set(shown,MUIA_Window_Open,TRUE);gallery_pump();
   w=(struct Window *)value(shown,MUIA_Window_Window);REQUIRE(w);
   if(width && height) {ChangeWindowBox(w,0,w->WScreen->BarHeight+1,width,height);gallery_pump();
    ChangeWindowBox(w,0,w->WScreen->BarHeight+1,width,height);gallery_pump();REQUIRE(w->Width==width && w->Height==height);}
   WindowToFront(w);set(shown,MUIA_Window_Activate,TRUE);gallery_pump();
   text=(char *)DoMethod(source,MUIM_TextEditor_ExportText);REQUIRE(text && !strcmp(text,demo_source));FreeVec(text);
   REQUIRE(value(profile_picker,MUIA_Cycle_Active)==2);
   snprintf(report,sizeof(report),"{\"panel\":\"%s\",\"width\":%d,\"height\":%d,\"view\":%d,\"actual_width\":%d,\"actual_height\":%d,\"title\":\"%s\",\"tiny_default\":true,\"source_preserved\":true}",mode,width,height,view,(int)w->Width,(int)w->Height,w->Title);
   join(path,project_root,"state.json");REQUIRE(save_text(path,report,sizeof(report)));cJSON_Delete(r);captures++;
  }
 }
 REQUIRE(captures>0 && ticks<36000);
done:
 termic_monitor_close();if(app)MUI_DisposeObject(app);guide_free(&guide_doc);
 if(keyeditor_class)__termic_mui_unclass(MUIMasterBase,keyeditor_class);
 if(workspace_class)__termic_mui_unclass(MUIMasterBase,workspace_class);
 if(board_list_class)__termic_mui_unclass(MUIMasterBase,board_list_class);
 if(action_icon_class)__termic_mui_unclass(MUIMasterBase,action_icon_class);
 if(bodyfont)CloseFont(bodyfont);if(codefont)CloseFont(codefont);
 if(DiskfontBase)CloseLibrary(DiskfontBase);if(GfxBase)CloseLibrary((struct Library *)GfxBase);
 if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);if(MUIMasterBase)CloseLibrary(MUIMasterBase);
 snprintf(report,sizeof(report),"GALLERY_%s line %d captures %d",fail?"FAIL":"PASS",fail,captures);
 join(path,project_root,"result.txt");save_text(path,report,128);return fail?20:0;
}
