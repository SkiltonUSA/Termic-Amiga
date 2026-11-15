enum {SP_CLOSE=5401,SP_SELECT,SP_NEW,SP_SAVE,SP_REFRESH};
static Object *pad_window,*pad_list,*pad_text,*pad_name,*pad_note;
static char pad_root[PATH_SIZE],pad_names[MAX_ITEMS][80];
static int pad_count,pad_selected=-1,pad_loading;
static int pad_id(const char *s)
{
    unsigned n=0;if(!s || !*s)return 0;
    for(;*s;s++)if(++n>=80 || (!isalnum((unsigned char)*s) && *s!='-' && *s!='_'))return 0;
    return 1;
}
static int pad_save(void)
{
    char path[PATH_SIZE],dir[PATH_SIZE],*text;int ok;
    if(pad_selected<0 || pad_selected>=pad_count || !value(pad_text,MUIA_TextEditor_HasChanged))return 1;
    if(busy(pad_root)){set(pad_note,MUIA_Text_Contents,"Task is queued or running. Notes will remain open until you can save.");return 0;}
    join(dir,pad_root,".scratchpads");join(path,dir,pad_names[pad_selected]);
    text=(char *)DoMethod(pad_text,MUIM_TextEditor_ExportText);if(!text)return 0;
    ok=save_text(path,text,SOURCE_LIMIT);FreeVec(text);
    if(ok)set(pad_text,MUIA_TextEditor_HasChanged,FALSE);
    set(pad_note,MUIA_Text_Contents,ok?"Note saved. Previous contents retained in .bak.":"Note could not be saved (32 KiB limit). Keep this window open.");return ok;
}
static void pad_refresh(void)
{
    BPTR lock;struct FileInfoBlock *fib;char dir[PATH_SIZE];
    pad_loading=1;pad_count=0;pad_selected=-1;join(dir,pad_root,".scratchpads");
    DoMethod(pad_list,MUIM_List_Clear);lock=Lock((STRPTR)dir,ACCESS_READ);fib=AllocDosObject(DOS_FIB,NULL);
    if(lock && fib && Examine(lock,fib))while(ExNext(lock,fib) && pad_count<MAX_ITEMS)
        if(fib->fib_DirEntryType==ST_FILE && pad_id(fib->fib_FileName)) {
            strcpy(pad_names[pad_count++],fib->fib_FileName);
            DoMethod(pad_list,MUIM_List_InsertSingle,fib->fib_FileName,MUIV_List_Insert_Bottom);
        }
    if(fib)FreeDosObject(DOS_FIB,fib);if(lock)UnLock(lock);
    set(pad_text,MUIA_TextEditor_Contents,"");set(pad_text,MUIA_TextEditor_HasChanged,FALSE);
    editor_readonly(pad_text,TRUE);set(pad_list,MUIA_List_Active,MUIV_List_Active_Off);pad_loading=0;
    set(pad_note,MUIA_Text_Contents,pad_root);
}
static int pad_build_window(void)
{
    pad_window=WindowObject,MUIA_Window_Title,"Termic: Scratchpads",
        MUIA_Window_ID,MAKE_ID('T','P','A','D'),MUIA_Window_Width,800,MUIA_Window_Height,460,
        WindowContents,VGroup,Child,pad_note=label("Task notes, shared with the scratchpad MCP tools"),
        Child,HGroup,Child,VGroup,MUIA_Weight,25,Child,list(&pad_list),Child,pad_name=entry(80),
        Child,button("New note",SP_NEW),End,Child,VGroup,MUIA_Weight,75,Child,editor(&pad_text,0),End,End,
        Child,HGroup,Child,button("Save",SP_SAVE),Child,button("Refresh",SP_REFRESH),Child,HVSpace,
        Child,button("Close",SP_CLOSE),End,End,End;
    if(!pad_window)return 0;DoMethod(app,OM_ADDMEMBER,pad_window);
    notify(pad_window,MUIA_Window_CloseRequest,TRUE,SP_CLOSE);
    notify(pad_list,MUIA_List_Active,MUIV_EveryTime,SP_SELECT);
    notify(pad_name,MUIA_String_Acknowledge,MUIV_EveryTime,SP_NEW);return 1;
}
static void pad_event(ULONG id)
{
    char path[PATH_SIZE],dir[PATH_SIZE],*text;const char *name;int n;
    switch(id) {
    case SP_OPEN:
        if(!pad_save())break;strcpy(pad_root,current);pad_refresh();set(pad_window,MUIA_Window_Open,TRUE);break;
    case SP_CLOSE:if(pad_save())set(pad_window,MUIA_Window_Open,FALSE);break;
    case SP_SAVE:pad_save();break;
    case SP_REFRESH:if(pad_save())pad_refresh();break;
    case SP_SELECT:
        if(pad_loading)break;n=value(pad_list,MUIA_List_Active);
        if(n<0 || n>=pad_count || n==pad_selected)break;
        if(!pad_save()) {SetAttrs(pad_list,MUIA_NoNotify,TRUE,MUIA_List_Active,pad_selected,TAG_DONE);break;}
        join(dir,pad_root,".scratchpads");join(path,dir,pad_names[n]);text=read_text(path,SOURCE_LIMIT);
        if(!text) {SetAttrs(pad_list,MUIA_NoNotify,TRUE,MUIA_List_Active,pad_selected,TAG_DONE);set(pad_note,MUIA_Text_Contents,"Cannot read note (binary, missing or larger than 32 KiB).");break;}
        pad_selected=n;set(pad_text,MUIA_TextEditor_Contents,text);free(text);
        set(pad_text,MUIA_TextEditor_HasChanged,FALSE);editor_readonly(pad_text,busy(pad_root));break;
    case SP_NEW:
        name=(char *)value(pad_name,MUIA_String_Contents);
        if(!pad_id(name) || busy(pad_root) || !pad_save()) {
            set(pad_note,MUIA_Text_Contents,"Use a new note name containing letters, numbers, - or _. Task must be idle.");break;
        }
        join(dir,pad_root,".scratchpads");join(path,dir,name);
        if(exists(path) || !mkdir_safe(dir) || !save_text(path,"",SOURCE_LIMIT)) {
            set(pad_note,MUIA_Text_Contents,"Note already exists, or its drawer cannot be written.");break;
        }
        {char selected[80];strcpy(selected,name);pad_refresh();
         for(n=0;n<pad_count;n++)if(!strcmp(pad_names[n],selected))set(pad_list,MUIA_List_Active,n);}
        set(pad_name,MUIA_String_Contents,"");break;
    }
}
