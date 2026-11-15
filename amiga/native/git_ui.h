enum {GT_CLOSE=5501,GT_LIST,GT_BRANCH,GT_CHECKOUT,GT_STATUS,GT_SYNC,GT_PR};
static Object *git_window,*git_branch,*git_repo,*git_base,*git_title,*git_body,*git_note;
static char git_root[PATH_SIZE];
static int git_build_window(void)
{
    git_window=WindowObject,MUIA_Window_Title,"Termic: Git branches and worktrees",
        MUIA_Window_ID,MAKE_ID('T','G','I','T'),MUIA_Window_Width,780,MUIA_Window_Height,420,
        WindowContents,VGroup,Child,git_note=label("Native branches; host worktrees and GitHub pull requests"),
        Child,ColGroup(2),Child,label("Task branch"),Child,git_branch=entry(81),
        Child,label("GitHub owner/repository"),Child,git_repo=entry(202),
        Child,label("Base branch"),Child,git_base=entry(81),
        Child,label("Commit / PR title"),Child,git_title=entry(201),End,
        Child,label("Pull request description"),Child,editor(&git_body,0),
        Child,label("Native Git"),Child,HGroup,Child,button("List branches",GT_LIST),
        Child,button("Create branch",GT_BRANCH),Child,button("Switch branch",GT_CHECKOUT),End,
        Child,label("Mac / Windows proxy (Git and gh must be installed there)"),Child,HGroup,
        Child,button("Worktree status",GT_STATUS),Child,button("Sync and commit",GT_SYNC),
        Child,button("Create PR",GT_PR),Child,button("Close",GT_CLOSE),End,End,End;
    if(!git_window)return 0;DoMethod(app,OM_ADDMEMBER,git_window);
    notify(git_window,MUIA_Window_CloseRequest,TRUE,GT_CLOSE);return 1;
}
static void git_event(ULONG id)
{
    char path[PATH_SIZE],*text,*body;const char *branch;cJSON *config,*field;
    if(id==GT_OPEN) {
        if(!save_current())return;strcpy(git_root,current);join(path,current,".host-git.json");
        text=read_text(path,8192);config=text?cJSON_Parse(text):NULL;free(text);
        field=config?cJSON_GetObjectItem(config,"repository"):NULL;
        set(git_repo,MUIA_String_Contents,cJSON_IsString(field)?field->valuestring:"");
        field=config?cJSON_GetObjectItem(config,"branch"):NULL;
        set(git_branch,MUIA_String_Contents,cJSON_IsString(field)?field->valuestring:"termic/task");
        field=config?cJSON_GetObjectItem(config,"base"):NULL;
        set(git_base,MUIA_String_Contents,cJSON_IsString(field)?field->valuestring:"main");
        field=config?cJSON_GetObjectItem(config,"title"):NULL;
        set(git_title,MUIA_String_Contents,cJSON_IsString(field)?field->valuestring:"");
        field=config?cJSON_GetObjectItem(config,"body"):NULL;
        set(git_body,MUIA_TextEditor_Contents,cJSON_IsString(field)?field->valuestring:"");
        cJSON_Delete(config);set(git_window,MUIA_Window_Open,TRUE);return;
    }
    if(id==GT_CLOSE){set(git_window,MUIA_Window_Open,FALSE);return;}
    if(strcmp(git_root,current) || busy(current) || !save_current()) {
        set(git_note,MUIA_Text_Contents,"Return to this idle task, then reopen Git before starting an operation.");return;
    }
    branch=(char *)value(git_branch,MUIA_String_Contents);
    if(id==GT_LIST || id==GT_BRANCH || id==GT_CHECKOUT) {
        join(path,current,"branch.txt");
        if(id!=GT_LIST && !save_text(path,branch,80))return;
        set(git_window,MUIA_Window_Open,FALSE);
        launch(id==GT_LIST?"--branches":id==GT_BRANCH?"--branch":"--checkout",3);return;
    }
    if(id==GT_PR && !__termic_mui_request(MUIMasterBase,app,git_window,0,"Create pull request","_Create PR|_Cancel",
        "Sync task sources to the host worktree, commit them,\npush this branch to GitHub and create a pull request?",NULL))return;
    body=(char *)DoMethod(git_body,MUIM_TextEditor_ExportText);if(!body)return;
    config=cJSON_CreateObject();if(!config){FreeVec(body);return;}
    cJSON_AddStringToObject(config,"action",id==GT_PR?"pr":id==GT_SYNC?"sync":"status");
    cJSON_AddStringToObject(config,"repository",(char *)value(git_repo,MUIA_String_Contents));
    cJSON_AddStringToObject(config,"branch",branch);
    cJSON_AddStringToObject(config,"base",(char *)value(git_base,MUIA_String_Contents));
    cJSON_AddStringToObject(config,"title",(char *)value(git_title,MUIA_String_Contents));
    cJSON_AddStringToObject(config,"body",body);FreeVec(body);
    text=cJSON_PrintUnformatted(config);cJSON_Delete(config);if(!text)return;
    join(path,current,".host-git.json");
    if(save_text(path,text,8192)) {
        join(path,current,".host-git-request");
        if(save_text(path,text,8192)) {set(git_window,MUIA_Window_Open,FALSE);launch("--host-git",0);}
    }
    free(text);
}
