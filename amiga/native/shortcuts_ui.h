/* Included after gui.c's shared UI, workspace and profile helpers. */
#include "shortcuts.h"
static Object *shortcut_window, *commands_window, *history_find;
static int history_marking;
static LONG history_anchor, history_line;
static void queue_action(ULONG id)
{
    DoMethod(app, MUIM_Application_ReturnID, id);
}
static Object *shortcut_column(const char *text)
{
    return TextObject, MUIA_Font, codefont ? (ULONG)codefont : MUIV_Font_Fixed,
        MUIA_Text_Contents, text, MUIA_Text_PreParse,
        MUIMasterBase->lib_Version >= 21 ? "\33P[EEEEEE]" : "\33" "3", End;
}
static void show_shortcuts(void)
{
    if (!shortcut_window) {
        shortcut_window = WindowObject, MUIA_Window_Title, "Termic: Keyboard shortcuts",
            MUIA_Window_NoMenus, TRUE, MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
            MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
            WindowContents, VGroup, InnerSpacing(20,16), MUIA_Group_Spacing, 18,
            MUIA_Background, "2:r00000000,00000000,00000000",
            Child, shortcut_column("\33bKeyboard shortcuts"),
            Child, HGroup, MUIA_Group_Spacing, 24,
            Child, shortcut_column(
                "\33bCompose\33n\n"
                "/          Commands (empty prompt)\n"
                "@          Mention open file\n"
                "!          CLI command\n"
                "Ctrl J     New line\n"
                "Ctrl G     Open built-in editor\n"
                "Ctrl R     Search history\n"
                "Esc Esc    Edit last message"),
            Child, shortcut_column(
                "\33bSession\33n\n"
                "Tab        Send message\n"
                "Shift Tab  Change agent profile\n"
                "F4         Process Monitor\n"
                "Ctrl C     Quit (saves edits)\n\n"
                "Prompt shortcuts apply only\n"
                "while the prompt has focus."),
            Child, shortcut_column(
                "\33bTranscript (open first)\33n\n"
                "Ctrl T          Open transcript\n"
                "F3              Find text\n"
                "PgUp / PgDn     Scroll\n"
                "Ctrl Space      Select lines\n"
                "Up / Down       Extend selection\n"
                "Ctrl Home / End Top / latest\n"
                "Shift Alt , / . Top / latest"), End,
            Child, shortcut_column(
                "/keymap  Show this reference       Right Amiga K  Open from any page\n"
                "Right Amiga I  Tools > Process Monitor (from any workspace page)\n\n"
                "Not available: image paste, reasoning adjustment, subagents, custom keymaps.\n"
                "Use Right Amiga V for text paste. Symbols use US keyboard positions.\n"
                "Selection: Ctrl Space to finish, Right Amiga C to copy. Esc returns to prompt."),
            Child, HGroup, Child, shortcut_column("? / Esc  Close"), Child, HVSpace,
            Child, button("Close", SHORTCUTSCLOSE), End, End, End;
        if (!shortcut_window) {message("Cannot open keyboard shortcuts.");return;}
        DoMethod(app, OM_ADDMEMBER, shortcut_window);
        notify(shortcut_window, MUIA_Window_CloseRequest, TRUE, SHORTCUTSCLOSE);
        notify(shortcut_window, MUIA_Window_InputEvent, (ULONG)"-repeat esc", SHORTCUTSCLOSE);
        notify(shortcut_window, MUIA_Window_InputEvent, (ULONG)"-repeat ?", SHORTCUTSCLOSE);
    }
    set(shortcut_window, MUIA_Window_CloseRequest, FALSE);
    set(shortcut_window, MUIA_Window_Open, TRUE);
    set(shortcut_window, MUIA_Window_Activate, TRUE);
}
static void show_commands(void)
{
    if (!commands_window) {
        commands_window = WindowObject, MUIA_Window_Title, "Termic: Commands",
            MUIA_Window_NoMenus, TRUE, MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
            MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
            WindowContents, VGroup, InnerSpacing(18,14), MUIA_Group_Spacing, 10,
            MUIA_Background, "2:r00000000,00000000,00000000",
            Child, shortcut_column("\33bCommands\33n\nChoose an action, or type a slash command and press Tab."),
            Child, button("/prompts    Prompt library", PL_OPEN),
            Child, button("/keymap    Keyboard shortcuts", SHORTCUTS),
            Child, button("/instructions    Install project instructions", INSTRUCTIONS),
            Child, button("/cli    CLI command panel", FOCUSCOMMAND),
            Child, button("/editor    Code editor", FOCUSEDITOR),
            Child, button("/history    Transcript", HISTORY),
            Child, button("Close", COMMANDSCLOSE), End, End;
        if (!commands_window) {message("Cannot open commands.");return;}
        DoMethod(app, OM_ADDMEMBER, commands_window);
        notify(commands_window, MUIA_Window_CloseRequest, TRUE, COMMANDSCLOSE);
        notify(commands_window, MUIA_Window_InputEvent, (ULONG)"-repeat esc", COMMANDSCLOSE);
    }
    set(commands_window, MUIA_Window_CloseRequest, FALSE);
    set(commands_window, MUIA_Window_Open, TRUE);
    set(commands_window, MUIA_Window_Activate, TRUE);
}
static int prompt_command(void)
{
    char *text = (char *)DoMethod(prompt, MUIM_TextEditor_ExportText), *end;
    int action = 0, handled = 0;
    if (!text) return 1;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) *--end = 0;
    if (!*text) {message("Enter a message before sending.");handled=1;}
    else if (*text == '/') {
        handled = 1;
        if (!strcmp(text,"/prompts")) action=PL_OPEN;
        else if (!strcmp(text,"/keymap") || !strcmp(text,"/help")) action=SHORTCUTS;
        else if (!strcmp(text,"/commands")) action=COMMANDS;
        else if (!strcmp(text,"/instructions")) action=INSTRUCTIONS;
        else if (!strcmp(text,"/cli")) action=FOCUSCOMMAND;
        else if (!strcmp(text,"/editor")) action=FOCUSEDITOR;
        else if (!strcmp(text,"/history")) action=HISTORY;
        else message("Unknown command. Use /commands or /keymap. Custom keymaps are unavailable.");
        if (action) {set(prompt,MUIA_TextEditor_Contents,"");queue_action(action);}
    }
    FreeVec(text);
    return handled;
}
static void history_scroll(LONG position)
{
    LONG total=value(transcript,MUIA_TextEditor_Prop_Entries);
    LONG visible=value(transcript,MUIA_TextEditor_Prop_Visible);
    LONG last=total>visible ? total-visible : 0;
    if(position<0) position=0;
    if(position>last) position=last;
    set(transcript,MUIA_TextEditor_Prop_First,position);
}
static ULONG termic_key_event(struct IntuiMessage *im)
{
    Object *active;
    unsigned mods=0;
    int compose, history, empty=0, action;
    static ULONG esc_seconds, esc_micros;
    static int escape_pending;
    if (!win || im->Code >= 0x80 || im->IDCMPWindow != (struct Window *)value(win,MUIA_Window_Window)) return 0;
    active=(Object *)value(win,MUIA_Window_ActiveObject);
    compose=active==prompt;
    history=active==transcript;
    if (im->Qualifier & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) mods|=KEY_SHIFT;
    if (im->Qualifier & IEQUALIFIER_CONTROL) mods|=KEY_CTRL;
    if (im->Qualifier & (IEQUALIFIER_LALT|IEQUALIFIER_RALT)) mods|=KEY_ALT;
    if (im->Qualifier & (IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND)) mods|=KEY_AMIGA;
    /* Route board/queue shortcuts before an editor handles its own bindings. */
    if((mods & KEY_AMIGA) && (im->Code==0x15 || im->Code==0x26 || im->Code==0x16 || im->Code==0x28 || im->Code==0x24)) {
        if(!(im->Qualifier & IEQUALIFIER_REPEAT))queue_action(im->Code==0x15?KB_OPEN:im->Code==0x26?Q_OPEN:im->Code==0x16?CR_OPEN:im->Code==0x28?SP_OPEN:GT_OPEN);
        return 1;
    }
    if (compose && (im->Code==0x3a || im->Code==0x01 || im->Code==0x02 || (mods&KEY_ALT))) {
        char *text=(char *)DoMethod(prompt,MUIM_TextEditor_ExportText), *p=text;
        if(p) {while(isspace((unsigned char)*p)) p++;empty=!*p;FreeVec(text);}
    }
    if (history && !mods && im->Code==0x45) {
        history_marking=0;queue_action(PAGE_AGENT);return 1;
    }
    action=shortcut_action(im->Code,mods,compose,history,empty);
    if (action!=KEY_ESCAPE) escape_pending=0;
    if (!action) return 0;
    /* Holding Tab must never start repeated jobs or move focus. */
    if ((im->Qualifier&IEQUALIFIER_REPEAT) && action!=KEY_UP && action!=KEY_DOWN &&
        action!=KEY_PAGEUP && action!=KEY_PAGEDOWN) return 1;
    switch(action) {
    case KEY_SEND: queue_action(AGENT);break;
    case KEY_MODE:
        if(picker_count) set(profile_picker,MUIA_Cycle_Active,
            (value(profile_picker,MUIA_Cycle_Active)+1)%picker_count);
        message("Agent profile changed. Tab sends using the selected profile.");break;
    case KEY_NEWLINE:
        DoMethod(prompt,MUIM_TextEditor_InsertText,"\n",MUIV_TextEditor_InsertText_Cursor);break;
    case KEY_EDITOR: queue_action(FOCUSEDITOR);break;
    case KEY_HISTORY: queue_action(HISTORYFIND);break;
    case KEY_TRANSCRIPT: queue_action(HISTORY);break;
    case KEY_QUIT: queue_action(QUIT);break;
    case KEY_ACTIVITY: queue_action(TM_ACTIVITY_OPEN);break;
    case KEY_COMMANDS: queue_action(im->Qualifier&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT) ? SHORTCUTS : COMMANDS);break;
    case KEY_MENTION: queue_action(MENTION);break;
    case KEY_CLI: set(command_mode,MUIA_Cycle_Active,0);queue_action(FOCUSCOMMAND);break;
    case KEY_ESCAPE:
        if(escape_pending && DoubleClick(esc_seconds,esc_micros,im->Seconds,im->Micros)) {
            escape_pending=0;queue_action(LASTPROMPT);
        } else {escape_pending=1;esc_seconds=im->Seconds;esc_micros=im->Micros;}
        break;
    case KEY_FIND: queue_action(HISTORYFIND);break;
    case KEY_TOP: history_marking=0;history_scroll(0);break;
    case KEY_BOTTOM: history_marking=0;history_scroll(0x7fffffff);break;
    case KEY_PAGEUP: case KEY_PAGEDOWN:
        history_marking=0;
        history_scroll((LONG)value(transcript,MUIA_TextEditor_Prop_First)+
            (action==KEY_PAGEUP ? -1 : 1)*(LONG)value(transcript,MUIA_TextEditor_Prop_Visible));break;
    case KEY_MARK:
        history_marking=!history_marking;
        history_anchor=history_line=value(transcript,MUIA_TextEditor_Prop_First)/(codefont?codefont->tf_YSize:13);
        message(history_marking ? "Selection starts at the top visible line. Up/Down extends; Ctrl Space finishes." : "Selection finished. Right Amiga C copies.");break;
    case KEY_UP: case KEY_DOWN:
        if(history_marking) {
            LONG lines=value(transcript,MUIA_TextEditor_Prop_Entries)/(codefont?codefont->tf_YSize:13);
            history_line+=action==KEY_UP?-1:1;
            if(history_line<0) history_line=0;
            if(history_line>lines) history_line=lines;
            DoMethod(transcript,MUIM_TextEditor_MarkText,0,
                history_line<history_anchor?history_line:history_anchor,0,
                history_line<history_anchor?history_anchor:history_line);
        } else DoMethod(transcript,MUIM_TextEditor_ARexxCmd,action==KEY_UP?"CURSOR UP":"CURSOR DOWN");
        break;
    case KEY_UNAVAILABLE: message("Image paste, reasoning controls and subagent switching are not available. Right Amiga V pastes text.");break;
    }
    return 1;
}
static int shortcut_event(ULONG id)
{
    char path[PATH_SIZE], *text;
    if(commands_window && value(commands_window,MUIA_Window_Open) &&
        (id==PL_OPEN || id==SHORTCUTS || id==INSTRUCTIONS || id==FOCUSCOMMAND || id==FOCUSEDITOR || id==HISTORY))
        set(commands_window,MUIA_Window_Open,FALSE);
    switch(id) {
    case SHORTCUTS: show_shortcuts();break;
    case COMMANDS: show_commands();break;
    case SHORTCUTSCLOSE: case COMMANDSCLOSE:
        set(id==SHORTCUTSCLOSE?shortcut_window:commands_window,MUIA_Window_Open,FALSE);
        set(win,MUIA_Window_Activate,TRUE);
        if(value(pages,MUIA_Group_ActivePage)==0)set(win,MUIA_Window_ActiveObject,prompt);
        break;
    case INSTRUCTIONS: launch("--instructions",2);break;
    case HISTORY: case HISTORYFIND:
        set(pages,MUIA_Group_ActivePage,0);
        set(win,MUIA_Window_ActiveObject,id==HISTORY?transcript:history_find);
        history_marking=0;break;
    case HISTORYNEXT:
        if(!DoMethod(transcript,MUIM_TextEditor_Search,(STRPTR)value(history_find,MUIA_String_Contents),
            MUIF_TextEditor_Search_Next))
            if(!DoMethod(transcript,MUIM_TextEditor_Search,(STRPTR)value(history_find,MUIA_String_Contents),
                MUIF_TextEditor_Search_FromTop)) message("Text not found in transcript.");
        set(win,MUIA_Window_ActiveObject,transcript);break;
    case LASTPROMPT:
        join(path,current,"last-prompt.txt");text=read_text(path,8192);
        if(text && save_current()) {
            set(prompt,MUIA_TextEditor_Contents,text);
            set(prompt,MUIA_TextEditor_HasChanged,TRUE);
            message("Last sent message restored for editing. Tab sends it again.");
        } else message("No previous message, or the current edits could not be saved.");
        free(text);break;
    case MENTION:
        if(*opened) {
            snprintf(path,sizeof(path),"[File: %s]\n",opened);
            DoMethod(prompt,MUIM_TextEditor_InsertText,path,MUIV_TextEditor_InsertText_Cursor);
            message("Open file mentioned. The agent can read it using its file tools.");
        } else message("Open a file in the editor first, then return to the prompt and press @.");
        break;
    default: return 0;
    }
    return 1;
}
