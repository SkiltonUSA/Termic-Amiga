#include "prompts.h"
static Object *prompts_window, *prompts_list, *prompts_title, *prompts_enabled, *prompts_note;
static cJSON *prompts_defaults, *prompts_items;
static int prompts_selected=-1, prompts_loading;
enum { PL_OPEN=3000, PL_SELECT, PL_NEW, PL_DUPLICATE, PL_REMOVE, PL_RESET,
       PL_RESTORE, PL_UP, PL_DOWN, PL_SAVE, PL_USE, PL_CANCEL, PL_INSERT };
static int prompts_store(void)
{
    cJSON *p=cJSON_GetArrayItem(prompts_items,prompts_selected), *copy;
    char *body=NULL;const char *title,*body_text;
    if(!p || prompts_loading)return 1;
    if(value(prompts_body,MUIA_TextEditor_HasChanged))
        body=(char *)DoMethod(prompts_body,MUIM_TextEditor_ExportText);
    body_text=body?body:prompt_string(p,"body");
    title=(const char *)value(prompts_title,MUIA_String_Contents);
    if(value(prompts_body,MUIA_TextEditor_HasChanged) && !body)return 0;
    copy=cJSON_Duplicate(p,1);
    cJSON_ReplaceItemInObjectCaseSensitive(copy,"title",cJSON_CreateString(title));
    cJSON_ReplaceItemInObjectCaseSensitive(copy,"body",cJSON_CreateString(body_text));
    cJSON_ReplaceItemInObjectCaseSensitive(copy,"enabled",cJSON_CreateBool(value(prompts_enabled,MUIA_Cycle_Active)));
    if(body)FreeVec(body);
    if(!prompt_valid(copy)) {
        cJSON_Delete(copy);set(prompts_note,MUIA_Text_Contents,"Enter a title and body (maximum 8192 bytes, no control codes).");return 0;
    }
    cJSON_ReplaceItemInArray(prompts_items,prompts_selected,copy);return 1;
}
static void prompts_fields(void)
{
    cJSON *p=cJSON_GetArrayItem(prompts_items,prompts_selected);
    prompts_loading=1;
    set(prompts_title,MUIA_String_Contents,prompt_string(p,"title"));
    {
        char *styled=syntax_colours(prompt_string(p,"body"),0);
        set(prompts_body,MUIA_TextEditor_Contents,styled?styled:prompt_string(p,"body"));
        free(styled);
        set(prompts_body,MUIA_TextEditor_Pen,0xffe6edf3UL);
    }
    set(prompts_body,MUIA_TextEditor_HasChanged,FALSE);
    set(prompts_enabled,MUIA_Cycle_Active,!cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(p,"enabled")));
    set(prompts_title,MUIA_Disabled,p==NULL);set(prompts_body,MUIA_Disabled,p==NULL);
    set(prompts_note,MUIA_Text_Contents,p && prompt_find(prompts_defaults,prompt_string(p,"id")) ?
        "Termic default. Reset restores its shipped text; Save keeps your changes." : "Custom prompt. Save persists; Cancel discards this window's edits.");
    prompts_loading=0;
}
static void prompts_refresh(int selected)
{
    cJSON *p;char row[128];int count=cJSON_GetArraySize(prompts_items);
    /* ReturnID notifications are queued, so a synchronous loading flag
     * cannot suppress a Clear -> select -> refresh cycle on reopening. */
    prompts_loading=1;
    DoMethod(prompts_list,MUIM_KillNotify,MUIA_List_Active);
    DoMethod(prompts_list,MUIM_List_Clear);
    cJSON_ArrayForEach(p,prompts_items) {
        snprintf(row,sizeof(row),"%s%s",cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(p,"enabled"))?"[Off] ":"",prompt_string(p,"title"));
        DoMethod(prompts_list,MUIM_List_InsertSingle,row,MUIV_List_Insert_Bottom);
    }
    if(selected>=count)selected=count-1;
    prompts_selected=selected;
    SetAttrs(prompts_list,MUIA_NoNotify,TRUE,MUIA_List_Active,selected,TAG_DONE);
    prompts_loading=0;prompts_fields();
    notify(prompts_list,MUIA_List_Active,MUIV_EveryTime,PL_SELECT);
}
static void prompts_close(void)
{
    set(prompts_window,MUIA_Window_Open,FALSE);
    cJSON_Delete(prompts_items);prompts_items=NULL;
    cJSON_Delete(prompts_defaults);prompts_defaults=NULL;prompts_selected=-1;
    set(win,MUIA_Window_Activate,TRUE);
}
static void prompts_open(void)
{
    char *text;const char *stored_path; cJSON *stored=NULL;
    if(prompts_window && value(prompts_window,MUIA_Window_Open)) {
        set(prompts_window,MUIA_Window_Activate,TRUE);return;
    }
    text=read_text("Termic:Prompts/defaults.json",PROMPTS_LIMIT);
    prompts_defaults=text?cJSON_Parse(text):NULL;free(text);
    stored_path=exists("ENV:Termic/prompts.json")?"ENV:Termic/prompts.json":"Termic:Config/prompts.json";
    if(exists(stored_path)) {
        text=read_text(stored_path,PROMPTS_LIMIT);stored=text?cJSON_Parse(text):NULL;free(text);
        if(!stored) {message("Cannot read prompts.json. Repair it before editing the library.");goto fail;}
    }
    prompts_items=prompts_resolve(prompts_defaults,stored);cJSON_Delete(stored);stored=NULL;
    if(!prompts_items) {message("Install valid Termic:Prompts/defaults.json and check prompts.json.");goto fail;}
    if(!prompts_window) {
        prompts_window=ResponsiveWindowObject,MUIA_Window_Title,"Termic: Prompts",MUIA_Window_NoMenus,TRUE,
            MUIA_Window_Width,1100,MUIA_Window_Height,620,
            MUIA_Window_LeftEdge,MUIV_Window_LeftEdge_Centered,MUIA_Window_TopEdge,MUIV_Window_TopEdge_Centered,
            WindowContents,VGroup,InnerSpacing(12,10),MUIA_Group_Spacing,8,
            Child,label("\33bPrompts"),
            Child,label("Termic's 17 defaults. Insert into the task composer, choose an agent, then Tab to send."),
            Child,HGroup,MUIA_Group_Spacing,6,Child,button("New",PL_NEW),Child,button("Duplicate",PL_DUPLICATE),
            Child,button("Remove",PL_REMOVE),Child,button("Reset selected",PL_RESET),
            Child,button("Restore missing defaults",PL_RESTORE),Child,button("Up",PL_UP),Child,button("Down",PL_DOWN),Child,HVSpace,End,
            Child,HGroup,Child,VGroup,MUIA_Weight,32,Child,list(&prompts_list),End,
            Child,VGroup,MUIA_Weight,68,Child,HGroup,Child,label("Title"),Child,prompts_title=entry(96),
            Child,label("Enabled"),Child,prompts_enabled=cycle(yesno),End,
            Child,editor(&prompts_body,0),End,End,
            Child,prompts_note=label(""),
            Child,label("Native API tools cannot run Git, browse, push or dispatch subagents. Templates explain these limits."),
            Child,HGroup,MUIA_Group_Spacing,6,Child,button("Save",PL_SAVE),Child,button("Use",PL_USE),
            Child,button("Insert + Use",PL_INSERT),Child,button("Cancel",PL_CANCEL),Child,HVSpace,End,End,End;
        if(!prompts_window)goto fail;
        SetAttrs(prompts_body,MUIA_Background,"2:r00000000,00000000,00000000",
            MUIA_TextEditor_Pen,0xffe6edf3UL,MUIA_TextEditor_WrapMode,MUIV_TextEditor_WrapMode_SoftWrap,TAG_DONE);
        DoMethod(app,OM_ADDMEMBER,prompts_window);
        notify(prompts_list,MUIA_List_Active,MUIV_EveryTime,PL_SELECT);
        notify(prompts_window,MUIA_Window_CloseRequest,TRUE,PL_CANCEL);
        notify(prompts_window,MUIA_Window_InputEvent,(ULONG)"-repeat esc",PL_CANCEL);
    }
    prompts_refresh(0);set(prompts_window,MUIA_Window_CloseRequest,FALSE);
    set(prompts_window,MUIA_Window_Open,TRUE);set(prompts_window,MUIA_Window_Activate,TRUE);return;
fail:cJSON_Delete(stored);cJSON_Delete(prompts_defaults);prompts_defaults=NULL;
    cJSON_Delete(prompts_items);prompts_items=NULL;
}
static int prompts_save(int permanent)
{
    cJSON *delta=prompts_delta(prompts_defaults,prompts_items);char *text=delta?cJSON_PrintUnformatted(delta):NULL;int ok=0;
    if(text && mkdir_safe("ENV:Termic") && (!permanent || write_preference("Termic:Config","prompts.json",text,PROMPTS_LIMIT)))
        ok=write_preference("ENV:Termic","prompts.json",text,PROMPTS_LIMIT);
    cJSON_Delete(delta);free(text);
    if(!ok)set(prompts_note,MUIA_Text_Contents,"Save failed. Check free space and prompt size; this window retains your edits.");
    return ok;
}
static void prompts_event(ULONG id)
{
    cJSON *p,*q;int n=prompts_selected,count;
    if(id==PL_OPEN){prompts_open();return;}
    if(id==PL_CANCEL){prompts_close();return;}
    if(prompts_loading || !prompts_items)return;
    if(!prompts_store()) {
        SetAttrs(prompts_list,MUIA_NoNotify,TRUE,MUIA_List_Active,prompts_selected,TAG_DONE);return;
    }
    p=cJSON_GetArrayItem(prompts_items,n);count=cJSON_GetArraySize(prompts_items);
    switch(id) {
    case PL_SELECT:n=value(prompts_list,MUIA_List_Active);break;
    case PL_NEW:case PL_DUPLICATE:
        if(count>=PROMPTS_MAX) {set(prompts_note,MUIA_Text_Contents,"Library limit: 48 prompts.");return;}
        {
            char key[64];int i;
            for(i=1;i<100000;i++){sprintf(key,"custom:%d",i);if(!prompt_find(prompts_items,key))break;}
            q=cJSON_CreateObject();cJSON_AddStringToObject(q,"id",key);
            cJSON_AddStringToObject(q,"title",id==PL_DUPLICATE && p?prompt_string(p,"title"):"New prompt");
            cJSON_AddStringToObject(q,"body",id==PL_DUPLICATE && p?prompt_string(p,"body"):"Describe the work for the agent.");
            cJSON_AddBoolToObject(q,"enabled",1);cJSON_AddItemToArray(prompts_items,q);n=count;
        }break;
    case PL_REMOVE:if(p)cJSON_DeleteItemFromArray(prompts_items,n);break;
    case PL_RESET:
        q=p?prompt_find(prompts_defaults,prompt_string(p,"id")):NULL;
        if(q)cJSON_ReplaceItemInArray(prompts_items,n,cJSON_Duplicate(q,1));break;
    case PL_RESTORE:
        cJSON_ArrayForEach(q,prompts_defaults)if(!prompt_find(prompts_items,prompt_string(q,"id")) && cJSON_GetArraySize(prompts_items)<PROMPTS_MAX)
            cJSON_AddItemToArray(prompts_items,cJSON_Duplicate(q,1));break;
    case PL_UP:case PL_DOWN:
        if(p && (id==PL_UP?n>0:n<count-1)) {
            q=cJSON_DetachItemFromArray(prompts_items,n);n+=id==PL_UP?-1:1;cJSON_InsertItemInArray(prompts_items,n,q);
        }break;
    case PL_SAVE:case PL_USE:
        if(prompts_save(id==PL_SAVE)){prompts_close();message(id==PL_SAVE?"Prompt library saved.":"Prompt library in use until reboot.");}return;
    case PL_INSERT:
        if(p && !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(p,"enabled"))) {
            char *old=(char *)DoMethod(prompt,MUIM_TextEditor_ExportText);const char *body=prompt_string(p,"body");
            if(old && strlen(old)+strlen(body)+2<=8192 && !busy(current) && prompt_loaded) {
                if(!prompts_save(0)) {FreeVec(old);return;}
                /* Append without discarding the user's task; never send automatically. */
                if(*old)DoMethod(prompt,MUIM_TextEditor_InsertText,"\n",MUIV_TextEditor_InsertText_Bottom);
                DoMethod(prompt,MUIM_TextEditor_InsertText,body,MUIV_TextEditor_InsertText_Bottom);
                set(prompt,MUIA_TextEditor_HasChanged,TRUE);FreeVec(old);
                prompts_close();
                message("Prompt inserted. Library in use until reboot. Choose an agent and press Tab to send.");
                set(pages,MUIA_Group_ActivePage,0);set(win,MUIA_Window_ActiveObject,prompt);return;
            }
            FreeVec(old);
        }
        set(prompts_note,MUIA_Text_Contents,"Enable the prompt and use an idle task with space in its 8192-byte composer.");return;
    }
    prompts_refresh(n);
}
