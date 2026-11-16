/* Actual production review window, in a fresh Acceptance drawer only. */
#define main termic_workspace_main
#include "../gui.c"
#undef main
#define CHECK(x) do{if(!(x)){printf("REVIEW_WINDOW_FAIL line %d\n",__LINE__);goto done;}}while(0)
int main(int argc,char **argv)
{
 char path[PATH_SIZE],*text=NULL;ULONG sigs,id;int result=20,show=argc==3;
 if(argc<2 || strlen(argv[1])>=sizeof(change_root) || !strstr(argv[1],"/Acceptance/"))return 20;
 strcpy(change_root,argv[1]);join(path,change_root,"main.c");
 if(exists(path)){puts("Refusing an existing review fixture");return 20;}
 CHECK(save_text(path,"/* original fixture */\nint main(void)\n{\n    return 0;\n}\n",SOURCE_LIMIT));
 CHECK(review_write(change_root,"main.c","/* generated fixture */\nint main(void)\n{\n    return 42;\n}\n"));
 MUIMasterBase=OpenLibrary("muimaster.library",21);
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37);
 GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",37);
 DiskfontBase=OpenLibrary("diskfont.library",37);
 CHECK(MUIMasterBase && IntuitionBase && GfxBase && DiskfontBase);
 {struct TextAttr a={"helvetica.font",13,0,FPF_DISKFONT},b={"courier.font",13,0,FPF_DISKFONT};
 bodyfont=OpenDiskFont(&a);codefont=OpenDiskFont(&b);}
 app=ApplicationObject,MUIA_Application_Title,"Termic review acceptance",
  MUIA_Application_Base,"TERMICREVIEWCHECK",End;CHECK(app);
 CHECK(change_build_window());set(change_window,MUIA_Window_Open,TRUE);
 CHECK(value(change_window,MUIA_Window_Open));change_refresh();change_show();
 text=(char *)DoMethod(change_text,MUIM_TextEditor_ExportText);
 CHECK(text && strstr(text,"-    return 0;\n+    return 42;") && !strchr(text,27));FreeVec(text);text=NULL;
 set(change_mode,MUIA_Cycle_Active,1);change_event(CR_MODE);
 text=(char *)DoMethod(change_text,MUIM_TextEditor_ExportText);
 CHECK(text && strstr(text,"BEFORE") && strstr(text,"AFTER") && !strchr(text,27));FreeVec(text);text=NULL;
 set(change_mode,MUIA_Cycle_Active,0);change_event(CR_MODE);
 if(show)for(;;) {
  sigs=0;id=DoMethod(app,MUIM_Application_NewInput,&sigs);
  if(id==CR_CLOSE)break;
  if(id==CR_SELECT || id==CR_MODE || id==CR_REFRESH)change_event(id);
  /* No Accept/Revert actions in the visual fixture. */
  if(sigs && (Wait(sigs|SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C))break;
 }
 result=0;
done:
 if(text)FreeVec(text);
 if(app){if(change_window)set(change_window,MUIA_Window_Open,FALSE);MUI_DisposeObject(app);}
 if(keyeditor_class)__termic_mui_unclass(MUIMasterBase,keyeditor_class);
 if(action_icon_class)__termic_mui_unclass(MUIMasterBase,action_icon_class);
 if(bodyfont)CloseFont(bodyfont);if(codefont)CloseFont(codefont);
 if(DiskfontBase)CloseLibrary(DiskfontBase);if(GfxBase)CloseLibrary((struct Library *)GfxBase);
 if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);if(MUIMasterBase)CloseLibrary(MUIMasterBase);
 if(!result)puts("REVIEW_WINDOW_PASS");return result;
}
