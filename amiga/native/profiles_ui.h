/* Included by gui.c after its common UI and preference helpers. */
static struct AgentProfiles profile_live, profile_draft;
static Object *profile_window, *profile_list, *profile_filter, *profile_tabs, *profile_info;
static Object *profile_name, *profile_model, *profile_keyfield, *profile_commandfield,
    *profile_args;
static Object *profile_yolo, *profile_resume_args, *profile_env, *profile_visible,
    *profile_role;
static Object *profile_resume, *profile_steps, *profile_tokens, *profile_usage, *profile_kind;
static Object *profile_picker, *profile_fast;
static char profile_secrets[PROFILE_MAX][512];
static int profile_row[PROFILE_MAX], profile_rows, profile_edit = -1, profile_loading,
                                                   profile_invalid;
static int picker_row[PROFILE_MAX], picker_count;
static const char *picker_names[PROFILE_MAX + 1];
static const char *yesno[] = {"Off", "On", NULL};
static const char *launcher_yolo[] = {"YOLO Off", "YOLO On", NULL};
static const char *kinds[] = {"OpenAI API", "Amiga agent CLI", "Amiga CLI", "ARexx", "MicroPython", "Codex on Mac", "Claude on Mac", "Cursor on Mac", NULL};
static const char *roles[] = {"Edit, build and run", "Read files and hardware only", NULL};
static const char *filters[] = {"All", "API / CLI agents", "CLI / Scripts", NULL};
static const char *profile_tabnames[] = {"General", "Launch / Sessions", "Environment",
                                         "Status", NULL};
enum
{
    PF_OPEN = 2000,
    PF_SELECT,
    PF_FILTER,
    PF_ADDAPI,
    PF_ADDCLI,
    PF_ADDTERM,
    PF_COPY,
    PF_REMOVE,
    PF_RESET,
    PF_SAVE,
    PF_USE,
    PF_CANCEL,
    PF_KIND,
    PF_ADDREXX,
    PF_ADDPYTHON,
    PF_ADDCODEX,
    PF_ADDCLAUDE,
    PF_ADDCURSOR
};
static Object *cycle(const char **choices)
{
    return CycleObject, MUIA_Cycle_Entries, choices, MUIA_Font,
           bodyfont ? (ULONG)bodyfont : MUIV_Font_Normal, MUIA_CycleChain, 1, End;
}
static Object *launcher_cycle(const char **choices)
{
    return CycleObject, MUIA_Cycle_Entries, choices, MUIA_Font,
           bodyfont ? (ULONG)bodyfont : MUIV_Font_Normal,
           MUIA_InnerLeft,0,MUIA_InnerRight,0,MUIA_CycleChain,1,End;
}
static void profiles_picker_refresh(void)
{
    int i, selected = 0;
    char path[PATH_SIZE], *id = NULL;
    if (*current)
    {
        join(path, current, "agent-profile");
        id = read_text(path, 31);
    }
    picker_count = 0;
    for (i = 0; i < profile_live.count; i++)
        if (profile_live.items[i].visible || (id && !strcmp(id, profile_live.items[i].id)))
        {
            picker_row[picker_count] = i;
            picker_names[picker_count] = profile_live.items[i].name;
            if (id && !strcmp(id, profile_live.items[i].id))
                selected = picker_count;
            picker_count++;
        }
    if (!picker_count)
    {
        picker_row[0] = 0;
        picker_names[0] = "No visible profiles";
        picker_names[1] = NULL;
    }
    if(picker_count)picker_names[picker_count] = NULL;
    SetAttrs(profile_picker, MUIA_Cycle_Entries, picker_names, MUIA_Cycle_Active, selected,
             TAG_DONE);
    free(id);
}
static void profile_store_fields(void)
{
    struct AgentProfile *p;
    char *env;
    if (profile_edit < 0 || profile_edit >= profile_draft.count || profile_loading)
        return;
    profile_invalid = 0;
    p = &profile_draft.items[profile_edit];
#define GET(f, o) snprintf(p->f, sizeof(p->f), "%s", (char *)value(o, MUIA_String_Contents))
    GET(name, profile_name);
    GET(model, profile_model);
    GET(command, profile_commandfield);
    GET(args, profile_args);
    GET(yolo, profile_yolo);
    GET(resume_args, profile_resume_args);
#undef GET
    p->kind = value(profile_kind, MUIA_Cycle_Active);
    p->visible = value(profile_visible, MUIA_Cycle_Active);
    p->readonly = value(profile_role, MUIA_Cycle_Active);
    p->resume = value(profile_resume, MUIA_Cycle_Active);
    p->usage = value(profile_usage, MUIA_Cycle_Active);
    p->steps = atoi((char *)value(profile_steps, MUIA_String_Contents));
    p->tokens = atoi((char *)value(profile_tokens, MUIA_String_Contents));
    snprintf(profile_secrets[profile_edit], 512, "%s",
             (char *)value(profile_keyfield, MUIA_String_Contents));
    env = (char *)DoMethod(profile_env, MUIM_TextEditor_ExportText);
    if (env)
    {
        if (strlen(env) >= sizeof(p->env))
        {
            profile_invalid = 1;
            set(profile_info, MUIA_Text_Contents,
                "Environment must be shorter than 1024 bytes. Edits have not been applied.");
        }
        else
            strcpy(p->env, env);
        FreeVec(env);
    }
}
static void profile_show(int index)
{
    struct AgentProfile *p;
    char n[16];
    if (index < 0 || index >= profile_draft.count)
        return;
    profile_loading = 1;
    profile_edit = index;
    p = &profile_draft.items[index];
#define PUT(f, o) set(o, MUIA_String_Contents, p->f)
    PUT(name, profile_name);
    PUT(model, profile_model);
    PUT(command, profile_commandfield);
    PUT(args, profile_args);
    PUT(yolo, profile_yolo);
    PUT(resume_args, profile_resume_args);
#undef PUT
    set(profile_keyfield, MUIA_String_Contents, profile_secrets[index]);
    SetAttrs(profile_kind, MUIA_NoNotify, TRUE, MUIA_Cycle_Active, p->kind, MUIA_Disabled,
             profile_builtin(p), TAG_DONE);
    set(profile_visible, MUIA_Cycle_Active, p->visible);
    set(profile_role, MUIA_Cycle_Active, p->readonly);
    set(profile_resume, MUIA_Cycle_Active, p->resume);
    set(profile_usage, MUIA_Cycle_Active, p->usage);
    sprintf(n, "%d", p->steps);
    set(profile_steps, MUIA_String_Contents, n);
    sprintf(n, "%d", p->tokens);
    set(profile_tokens, MUIA_String_Contents, n);
    set(profile_env, MUIA_TextEditor_Contents, p->env);
    set(profile_model, MUIA_Disabled, p->kind != 0 && !profile_is_relay(p->kind));
    set(profile_keyfield, MUIA_Disabled, p->kind != 0);
    set(profile_role, MUIA_Disabled, p->kind != 0 && !profile_is_relay(p->kind));
    set(profile_steps, MUIA_Disabled, p->kind != 0 && !profile_is_relay(p->kind));
    set(profile_tokens, MUIA_Disabled, p->kind != 0);
    set(profile_usage, MUIA_Disabled, p->kind != 0);
    set(profile_commandfield, MUIA_Disabled, p->kind == 0 || profile_is_relay(p->kind));
    set(profile_args, MUIA_Disabled, p->kind == 0 || profile_is_relay(p->kind));
    set(profile_yolo, MUIA_Disabled, p->kind == 0 || profile_is_relay(p->kind));
    set(profile_resume_args, MUIA_Disabled, p->kind == 0 || profile_is_relay(p->kind));
    set(profile_env, MUIA_Disabled, p->kind == 0 || profile_is_relay(p->kind));
    set(profile_info, MUIA_Text_Contents,
        p->kind == 0
            ? "Direct OpenAI API. Blank model uses the main Settings model.\nBlank key retains "
              "this profile's credential. Copies need their own key."
            : profile_is_relay(p->kind) ? "Mac agent through the MCP relay. Sign in on the Mac; no Amiga API key.\nBlank model uses the Mac default. Steps limits tool calls. Start the Mac relay before sending."
            : p->kind == 3 ? "ARexx: enter the script path and arguments under Launch / Sessions.\nRequires SYS:Rexxc/RX and a running RexxMast. No Termic-specific ARexx commands are added."
            : p->kind == 4 ? "MicroPython: enter a script path or -c expression under Launch / Sessions.\nRuntime: Termic:Tools/MicroPython/micropython. Interactive use is through CLI."
            : "Native AmigaDOS command. Install the executable before launching.\nCLI consoles open separately; desktop agent executables need a different host.");
    profile_loading = 0;
}
static void profile_refresh(int selected)
{
    int i, active = 0, filter = value(profile_filter, MUIA_Cycle_Active);
    profile_loading = 1;
    set(profile_list, MUIA_List_Quiet, TRUE);
    DoMethod(profile_list, MUIM_List_Clear);
    profile_rows = 0;
    for (i = 0; i < profile_draft.count; i++)
        if (filter == 0 || (filter == 1 && (profile_draft.items[i].kind < 2 || profile_is_relay(profile_draft.items[i].kind))) ||
            (filter == 2 && (profile_draft.items[i].kind >= 2 && profile_draft.items[i].kind <= 4)))
        {
            if (i == selected)
                active = profile_rows;
            profile_row[profile_rows++] = i;
            DoMethod(profile_list, MUIM_List_InsertSingle, profile_draft.items[i].name,
                     MUIV_List_Insert_Bottom);
        }
    SetAttrs(profile_list, MUIA_NoNotify, TRUE, MUIA_List_Active,
             profile_rows ? active : MUIV_List_Active_Off, MUIA_List_Quiet, FALSE, TAG_DONE);
    profile_loading = 0;
    if (profile_rows)
        profile_show(profile_row[active]);
}
static void profile_open(void)
{
    if (!profile_window)
        return;
    if (value(profile_window, MUIA_Window_Open))
    {
        set(profile_window, MUIA_Window_Activate, TRUE);
        return;
    }
    if (!profiles_load(&profile_draft))
    {
        message("Invalid agents.json. Repair it before editing profiles.");
        return;
    }
    memset(profile_secrets, 0, sizeof(profile_secrets));
    profile_edit = -1;
    set(profile_filter, MUIA_Cycle_Active, 0);
    profile_refresh(0);
    set(profile_window, MUIA_Window_Open, TRUE);
}
static void profile_apply(int permanent)
{
    char *json, path[PATH_SIZE], keyname[48];
    int i, ok = 1;
    profile_store_fields();
    if (profile_invalid)
        return;
    for (i = 0; i < profile_draft.count; i++)
        if (!profile_valid(&profile_draft.items[i]))
        {
            set(profile_info, MUIA_Text_Contents,
                "Invalid profile: use a name, single-line fields, steps 1-32,\ntokens "
                "256-16384 and environment lines NAME=value.");
            return;
        }
    json = profiles_json(&profile_draft);
    if (!json)
        return;
    mkdir_safe("ENV:Termic");
    if (permanent && !write_preference("Termic:Config", "agents.json", json, PROFILE_LIMIT))
        ok = 0;
    if (!write_preference("ENV:Termic", "agents.json", json, PROFILE_LIMIT))
        ok = 0;
    free(json);
    for (i = 0; i < profile_draft.count; i++)
        if (*profile_secrets[i] && profile_draft.items[i].kind == 0)
        {
            if (!strcmp(profile_draft.items[i].id, "openai"))
                strcpy(keyname, "api-key");
            else
                sprintf(keyname, "key-%s", profile_draft.items[i].id);
            if (permanent &&
                !write_preference("Termic:Config", keyname, profile_secrets[i], 512))
                ok = 0;
            if (!write_preference("ENV:Termic", keyname, profile_secrets[i], 512))
                ok = 0;
            sprintf(path, "Termic:Config/%s.bak", keyname);
            DeleteFile((STRPTR)path);
            sprintf(path, "ENV:Termic/%s.bak", keyname);
            DeleteFile((STRPTR)path);
        }
    if (!ok)
    {
        set(profile_info, MUIA_Text_Contents,
            "Some settings could not be saved. Check free space and protection; retry.");
        return;
    }
    memset(profile_secrets, 0, sizeof(profile_secrets));
    set(profile_keyfield, MUIA_String_Contents, "");
    profile_live = profile_draft;
    profiles_picker_refresh();
    set(profile_window, MUIA_Window_Open, FALSE);
    message(permanent ? "Agent profiles saved." : "Agent profiles in use until reboot.");
}
static void profile_event(int id)
{
    int index;
    struct AgentProfiles defaults;
    if (profile_loading)
        return;
    if (id == PF_OPEN)
    {
        profile_open();
        return;
    }
    if (id == PF_CANCEL)
    {
        memset(profile_secrets, 0, sizeof(profile_secrets));
        set(profile_keyfield, MUIA_String_Contents, "");
        set(profile_window, MUIA_Window_Open, FALSE);
        return;
    }
    if (id == PF_SAVE || id == PF_USE)
    {
        profile_apply(id == PF_SAVE);
        return;
    }
    profile_store_fields();
    if (profile_invalid)
        return;
    if (id == PF_SELECT)
    {
        index = value(profile_list, MUIA_List_Active);
        if (index >= 0 && index < profile_rows)
            profile_show(profile_row[index]);
        return;
    }
    if (id == PF_KIND)
    {
        profile_show(profile_edit);
        return;
    }
    if (id == PF_FILTER)
    {
        profile_refresh(profile_edit);
        return;
    }
    if (id == PF_RESET)
    {
        profiles_defaults(&defaults);
        for (index = 0; index < defaults.count; index++)
        {
            int n = profile_find(&profile_draft, defaults.items[index].id);
            if (n >= 0)
                profile_draft.items[n] = defaults.items[index];
        }
    }
    else if (id == PF_REMOVE)
    {
        if (!profile_remove(&profile_draft, profile_edit))
        {
            set(profile_info, MUIA_Text_Contents,
                "Built-in profiles can be edited, but cannot be removed.");
            return;
        }
        memmove(profile_secrets[profile_edit], profile_secrets[profile_edit + 1],
                (profile_draft.count - profile_edit) * 512);
        memset(profile_secrets[profile_draft.count], 0, 512);
        profile_edit = 0;
    }
    else
    {
        index = profile_add(&profile_draft,
                            id == PF_ADDCLAUDE ? 6
                            : id == PF_ADDCURSOR ? 7
                            : id == PF_ADDCODEX ? 5
                            : id == PF_ADDREXX ? 3
                            : id == PF_ADDPYTHON ? 4
                            : id == PF_ADDTERM  ? 2
                            : id == PF_ADDCLI ? 1
                                              : 0,
                            id == PF_COPY ? profile_edit : -1);
        if (index < 0)
        {
            set(profile_info, MUIA_Text_Contents, "The profile limit is 32.");
            return;
        }
        profile_edit = index;
        set(profile_filter, MUIA_Cycle_Active, 0);
    }
    profile_refresh(profile_edit);
}
static Object *build_profiles_window(void)
{
    Object *titles = NULL, *o;
    if (MUIMasterBase->lib_Version >= 21)
    {
        titles = MUI_NewObject("Title.mui", Child, label(profile_tabnames[0]), Child,
                               label(profile_tabnames[1]), Child, label(profile_tabnames[2]),
                               Child, label(profile_tabnames[3]), TAG_DONE);
        if (!titles)
            return NULL;
    }
    o=ResponsiveWindowObject,MUIA_Window_Title,"Termic: Agents, CLI and Scripts",MUIA_Window_ID,MAKE_ID('T','A','G','T'),
    MUIA_Window_Width,1060,MUIA_Window_Height,600,WindowContents,VGroup,InnerSpacing(10,10),
    Child,HGroup,Child,label("\33bAgents / CLI / Scripts"),Child,HVSpace,Child,button("Add Codex",PF_ADDCODEX),Child,button("Add Claude",PF_ADDCLAUDE),Child,button("Add Cursor",PF_ADDCURSOR),Child,button("Reset built-ins",PF_RESET),End,
    Child,label("Configure API agents, CLI commands, ARexx and MicroPython scripts. Save keeps changes; Use lasts until reboot."),
    Child,HGroup,Child,button("Add API",PF_ADDAPI),Child,button("Add CLI",PF_ADDTERM),Child,button("Add ARexx",PF_ADDREXX),Child,button("Add MicroPython",PF_ADDPYTHON),Child,button("Duplicate",PF_COPY),Child,button("Remove",PF_REMOVE),End,
    Child,HGroup,Child,VGroup,MUIA_Weight,24,Child,profile_filter=cycle(filters),Child,list(&profile_list),End,
    Child,VGroup,MUIA_Weight,76,Child,profile_tabs=MUI_NewObject(titles?MUIC_Group:MUIC_Register,
    titles?TAG_IGNORE:MUIA_Register_Titles,profile_tabnames,MUIA_Group_PageMode,TRUE,
    MUIA_Font,bodyfont?(ULONG)bodyfont:MUIV_Font_Normal,titles?Child:TAG_IGNORE,titles,
    Child,VGroup,InnerSpacing(8,8),Child,ColGroup(2),Child,label("Name"),Child,profile_name=entry(64),
    Child,label("Type"),Child,profile_kind=cycle(kinds),Child,label("Show in launcher"),Child,profile_visible=cycle(yesno),
    Child,label("Model"),Child,profile_model=entry(128),Child,label("API key"),Child,profile_keyfield=StringObject,StringFrame,MUIA_String_Secret,TRUE,MUIA_String_MaxLen,512,MUIA_CycleChain,1,End,
    Child,label("File access"),Child,profile_role=cycle(roles),End,
    Child,label("API keys stay in local Config / ENV files, outside the profile registry.\nClassic AmigaOS stores these credentials as plaintext."),Child,HVSpace,End,
    Child,VGroup,InnerSpacing(8,8),Child,ColGroup(2),Child,label("Command"),Child,profile_commandfield=entry(256),
    Child,label("Default arguments"),Child,profile_args=entry(384),Child,label("YOLO arguments"),Child,profile_yolo=entry(256),
    Child,label("Resume arguments"),Child,profile_resume_args=entry(256),Child,label("Resume session"),Child,profile_resume=cycle(yesno),
    Child,label("API steps / relay calls"),Child,profile_steps=entry(4),Child,label("Output token limit"),Child,profile_tokens=entry(6),End,
    Child,label("YOLO arguments apply only when selected in the launcher.\nAPI conversation history is isolated by task and profile.\nNative resume arguments apply after a successful launch of that profile."),Child,HVSpace,End,
    Child,VGroup,InnerSpacing(8,8),Child,label("\33bNative command environment"),Child,label("One NAME=value per line. Values apply only to the launched worker and its children."),
    Child,editor(&profile_env,0),Child,label("Native commands have ordinary AmigaDOS access.\nmacOS Keychain roles, Docker and desktop sandbox rules are not available.\nAgent file tools access task C/H and ARexx. Read-only disables builds and execution."),End,
    Child,VGroup,InnerSpacing(8,8),Child,ColGroup(2),Child,label("Report API token usage"),Child,profile_usage=cycle(yesno),End,
    Child,label("\33bAgent state"),Child,label("The native worker reports Running, completion and failure in the task list.\nBuild / CLI contains its output. Cancel requests an orderly stop.\nAPI usage reports actual input/output tokens for each response when supplied.\nSubscription plan quotas and total context-window capacity are not reported.\nExternal CLI hook installation and live YOLO switching need an interactive host.\nNative CLI consoles open their own console; their lifetime is not tracked here."),Child,HVSpace,End,End,
    Child,profile_info=label(""),End,End,
    Child,HGroup,MUIA_Group_Spacing,6,Child,button("Save",PF_SAVE),Child,button("Use",PF_USE),Child,button("Cancel",PF_CANCEL),Child,HVSpace,End,End,End;
    if(o) {
        DoMethod(app, OM_ADDMEMBER, o);
        notify(o, MUIA_Window_CloseRequest, TRUE, PF_CANCEL);
        notify(profile_list, MUIA_List_Active, MUIV_EveryTime, PF_SELECT);
        notify(profile_filter, MUIA_Cycle_Active, MUIV_EveryTime, PF_FILTER);
        notify(profile_kind, MUIA_Cycle_Active, MUIV_EveryTime, PF_KIND);
    }
    return o;
}
static int profile_prepare_launch(void)
{
    char path[PATH_SIZE];
    int n = value(profile_picker, MUIA_Cycle_Active);
    if (n < 0 || n >= picker_count || busy(current))
        return 0;
    join(path, current, "agent-profile");
    if (!save_text(path, profile_live.items[picker_row[n]].id, 31))
        return 0;
    join(path, current, "agent-yolo");
    return save_text(path, value(profile_fast, MUIA_Cycle_Active) ? "1" : "0", 1);
}
