/* Integrated AmigaOS workspace. All MUI calls stay on the UI task; long
 * operations run in TermicAgent processes with per-workspace locking. */
#include "mui_compat.h"
#include "native.h"
#include "profiles.h"
#include "syntax.h"
#include "version.h"
#include "workspace.h"
#include "taskflow.h"
#include "job_watch.h"
#include "change_review.h"
#include "diagnostics.h"
#include <ctype.h>
#include <devices/timer.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <exec/memory.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <mui/TextEditor_mcc.h>
#include <proto/asl.h>
#include <proto/diskfont.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <workbench/workbench.h>

struct Library *MUIMasterBase, *IconBase, *DiskfontBase, *AslBase;
struct GfxBase *GfxBase;
static struct TextFont *bodyfont, *codefont;
struct IntuitionBase *IntuitionBase;
static ULONG termic_key_event(struct IntuiMessage *im);
#include "chrome.h"
const char version_tag[] = TERMIC_VERSION_TAG("TermicNative");
const char stack_size[] = "$STACK:262144";
#define MAX_ITEMS 128
#define PATH_SIZE 384
static Object *app, *win, *projects, *tasks, *files, *pages, *source, *prompt, *output, *review,
    *transcript, *cli_output;
static Object *prompts_body,*diagnostic_list,*diagnostic_view;
static struct Diagnostic diagnostic_rows[DIAGNOSTIC_MAX];
static int diagnostic_count;
static Object *status, *heading, *filename, *newname, *projectname, *taskname, *model, *keyfield,
    *command, *command_mode, *commit, *findtext, *buildcommand, *projectdrawer;
static char project_paths[MAX_ITEMS][PATH_SIZE], task_paths[MAX_ITEMS][PATH_SIZE],
    file_names[MAX_ITEMS][184];
static char current[PATH_SIZE], opened[184], project[PATH_SIZE];
static char project_root[PATH_SIZE] = "Termic:Projects";
static int project_count, task_count, file_count, loading, source_loaded = 1, prompt_loaded = 1;
static const char *command_modes[] = {"CLI", "ARexx", "MicroPython", NULL};
static const char *tabs[] = {"Agent", "Editor", "Build / CLI", "Changes", "Settings", NULL};
enum
{
    QUIT = 1,
    PROJECT,
    TASK,
    FILESEL,
    NEWPROJECT,
    NEWTASK,
    SAVE,
    BUILD,
    RUN,
    AGENT,
    CANCEL,
    NEWFILE,
    REFRESH,
    PROBE,
    SETTINGS,
    SAVECONFIG,
    SHELL,
    DIFF,
    GITSTATUS,
    GITINIT,
    GITCOMMIT,
    UNDO,
    REDO,
    CUT,
    COPY,
    PASTE,
    SAVETASK,
    RENAMEPROJECT,
    RENAMETASK,
    MARKDONE,
    PAGE_AGENT,
    PAGE_EDITOR,
    PAGE_OUTPUT,
    PAGE_CHANGES,
    PAGE_SETTINGS,
    FIND,
    FINDNEXT,
    SAVEBUILD,
    NEWSESSION,
    IMPORTPROJECT,
    FOCUSFILE,
    FOCUSEDITOR,
    FOCUSCOMMIT,
    FOCUSCOMMAND,
    ABOUT,
    ABOUTCLOSE,
    HELP,
    USECONFIG,
    CANCELCONFIG,
    MUIPREFS,
    UPDATE_MENUS,
    SELECTALL,
    ERASE,
    WINDOW_DEPTH,
    WINDOW_ZOOM,
    SHORTCUTS, SHORTCUTSCLOSE, INSTRUCTIONS, HISTORY, HISTORYFIND, HISTORYNEXT,
    LASTPROMPT, MENTION, COMMANDS, COMMANDSCLOSE, CR_OPEN=5200, SP_OPEN=5400, GT_OPEN=5500
};
struct Job
{
    char path[PATH_SIZE];
    int active, polls;
    long bytes;
} jobs[16];
#include "activity_scroll.h"

static struct ActivityView activity;
static int transcript_pending;
static void editor_recolour(void);
static int colour_pending,colour_delay;
static void flow_tick(void), flow_changed(void), flow_completed(const char *, int);
static struct MsgPort *timer_port;
static struct timerequest *timer_io;
static int timer_open, timer_pending, control_registered;
static struct MsgPort control_port;
static ULONG value(Object *o, ULONG a)
{
    ULONG v = 0;
    get(o, a, &v);
    return v;
}
static void notify(Object *o, ULONG a, ULONG v, ULONG id)
{
    DoMethod(o, MUIM_Notify, a, v, app, 2, MUIM_Application_ReturnID, id);
}
static Object *label(const char *s)
{
    return TextObject, MUIA_Font, bodyfont ? (ULONG)bodyfont : MUIV_Font_Normal, MUIA_Text_Contents,
           s, End;
}
#include "action_icons.h"
static Object *button(const char *s, int id)
{
    int kind=action_icon_kind(s,id);
    const char *help=id==CANCEL?"Stop current job (Shift F10)":id==RUN?"Run program (F7)":
        id==BUILD?"Build (F6)":id==SAVE?"Save (F8 / Right Amiga S)":id==AGENT?"Start agent (Tab in prompt)":s;
    Object *o;
    if(kind>=0) o=action_icon(kind,help);
    else o = TextObject, ButtonFrame, MUIA_Font, bodyfont ? (ULONG)bodyfont : MUIV_Font_Button,
           MUIA_InnerTop, 4, MUIA_InnerBottom, 4, MUIA_Background, MUII_ButtonBack, MUIA_InputMode,
           MUIV_InputMode_RelVerify, MUIA_Text_Contents, s, MUIA_Text_PreParse, "\33c",
           MUIA_CycleChain, 1, End;
    if (!o) return NULL;
    if (id == SAVECONFIG || id == USECONFIG || id == CANCELCONFIG)
    {
        char key = id == SAVECONFIG ? 's' : id == USECONFIG ? 'u' : 'c';
        SetAttrs(o, MUIA_ControlChar, key, MUIA_Text_HiChar, key, TAG_DONE);
    }
    notify(o, MUIA_Pressed, FALSE, id);
    return o;
}
static Object *chrome_button(int id)
{
    Object *o = chrome_icon(id == WINDOW_ZOOM ? 2 : id == WINDOW_DEPTH ? 1 : 0,
        id == WINDOW_ZOOM ? "Zoom: fit screen / restore window"
            : id == WINDOW_DEPTH ? "Back: send window behind other windows" : "Close: save and close");
    if (o)
        notify(o, MUIA_Pressed, FALSE, id);
    return o;
}
static Object *entry(unsigned max)
{
    return StringObject, StringFrame, MUIA_Font, bodyfont ? (ULONG)bodyfont : MUIV_Font_Normal,
           MUIA_String_MaxLen, max, MUIA_CycleChain, 1, End;
}
static Object *list(Object **o)
{
    return ListviewObject, MUIA_Listview_List,
           *o = ListObject, InputListFrame, MUIA_Font, bodyfont ? (ULONG)bodyfont : MUIV_Font_List,
           o == &files ? MUIA_Background : TAG_IGNORE, "2:r00000000,00000000,00000000",
           o == &files ? MUIA_List_Format : TAG_IGNORE,
           MUIMasterBase->lib_Version >= 21 ? "P=\33P[EEEEEE]" : "P=\33" "3",
           MUIA_List_ConstructHook, MUIV_List_ConstructHook_String, MUIA_List_DestructHook,
           MUIV_List_DestructHook_String, End, End;
}
/* Register on the active editor itself: MUI delivers active-object handlers
 * before other objects, regardless of a title bar handler's priority.
 * Each class owns its handler; unhandled events pass to TextEditor's node. */
static struct MUI_CustomClass *keyeditor_class;
struct KeyEditorData { struct MUI_EventHandlerNode events; };
static ULONG keyeditor_dispatch(__reg("a0") struct IClass *cl,
                                __reg("a2") Object *obj, __reg("a1") Msg msg)
{
    struct KeyEditorData *d=obj?INST_DATA(cl,obj):NULL;
    switch(msg->MethodID) {
    case MUIM_Setup:
        if(!DoSuperMethodA(cl,obj,msg))return FALSE;
        memset(d,0,sizeof(*d));
        d->events.ehn_Object=obj;d->events.ehn_Class=cl;
        d->events.ehn_Priority=20;d->events.ehn_Events=IDCMP_RAWKEY;
        DoMethod(_win(obj),MUIM_Window_AddEventHandler,&d->events);return TRUE;
    case MUIM_Cleanup:
        DoMethod(_win(obj),MUIM_Window_RemEventHandler,&d->events);break;
    case MUIM_HandleEvent:
        {
            struct IntuiMessage *im=((struct MUIP_HandleEvent *)msg)->imsg;
            return im && termic_key_event(im)?MUI_EventHandlerRC_Eat:0;
        }
    }
    return DoSuperMethodA(cl,obj,msg);
}
static Object *editor(Object **o, int readonly)
{
    Object *slider;
    Object *g;
    if(!keyeditor_class)keyeditor_class=__termic_mui_class(MUIMasterBase,NULL,MUIC_TextEditor,NULL,
        sizeof(struct KeyEditorData),(APTR)keyeditor_dispatch);
    if(!keyeditor_class)return NULL;
    g = HGroup, Child, *o = NewObject(keyeditor_class->mcc_Class, NULL, InputListFrame, MUIA_Font,
           codefont ? (ULONG)codefont : MUIV_Font_Fixed,
           (o == &source || o == &prompts_body || o == &transcript) ? MUIA_Background : TAG_IGNORE, "2:r00000000,00000000,00000000",
           MUIA_TextEditor_ReadOnly, readonly, MUIA_TextEditor_ActiveObjectOnClick, TRUE,
           MUIA_TextEditor_ImportHook,
           MUIV_TextEditor_ImportHook_Plain, MUIA_TextEditor_ExportHook,
           MUIV_TextEditor_ExportHook_NoStyle, MUIA_TextEditor_PasteColors, FALSE,
           MUIA_TextEditor_WrapMode, o == &transcript ? MUIV_TextEditor_WrapMode_SoftWrap : MUIV_TextEditor_WrapMode_NoWrap,
           MUIA_TextEditor_WrapBorder, 0, MUIA_CycleChain, 1, End,
           Child, slider = ScrollbarObject, End, End;
    set(*o, MUIA_TextEditor_Slider, slider);
    return g;
}
static void editor_readonly(Object *o, int readonly)
{
    /* TextEditor clears click-to-focus when ReadOnly is set, but does not
     * restore it when unlocked. Logs also need focus for selection/copy. */
    SetAttrs(o, MUIA_TextEditor_ReadOnly, readonly,
             MUIA_TextEditor_ActiveObjectOnClick, TRUE, TAG_DONE);
}
static void message(const char *s)
{
    set(status, MUIA_Text_Contents, s);
}
static void job_output(const char *text)
{
    set(output, MUIA_TextEditor_Contents, text);
    set(cli_output, MUIA_TextEditor_Contents, text);
    if(diagnostic_list) {
        int i;set(diagnostic_list,MUIA_List_Quiet,TRUE);DoMethod(diagnostic_list,MUIM_List_Clear);
        diagnostic_count=diagnostics_parse(text,diagnostic_rows,DIAGNOSTIC_MAX);
        for(i=0;i<diagnostic_count;i++)DoMethod(diagnostic_list,MUIM_List_InsertSingle,
            diagnostic_rows[i].message,MUIV_List_Insert_Bottom);
        set(diagnostic_list,MUIA_List_Quiet,FALSE);
    }
}
static int exists(const char *p)
{
    BPTR l = Lock((STRPTR)p, ACCESS_READ);
    if (!l)
        return 0;
    UnLock(l);
    return 1;
}
static int mkdir_safe(const char *p)
{
    BPTR l = CreateDir((STRPTR)p);
    if (l)
    {
        UnLock(l);
        return 1;
    }
    return exists(p);
}
static void join(char *out, const char *dir, const char *name)
{
    snprintf(out, PATH_SIZE, "%s/%s", dir, name);
}
static int busy(const char *p)
{
    char path[PATH_SIZE];
    int i;
    for (i = 0; i < 16; i++)
        if (jobs[i].active && !strcmp(jobs[i].path, p))
            return 1;
    join(path, p, ".busy");
    if(exists(path)) return 1;
    join(path,p,".launch");if(exists(path))return 1;
    join(path,p,".mcp-lock");if(exists(path))return 1;
    join(path,p,".queued");return exists(path);
}
static void content(Object *o, const char *p, const char *fallback)
{
    char *s = read_text(p, o == source ? SOURCE_LIMIT : o == prompt ? 8192 : RESPONSE_LIMIT);
    int valid = s != NULL || !exists(p);
    if (o == source && s && strchr(s, 27))
    {
        free(s);
        s = NULL;
        valid = 0;
    }
    if (o == source)
        source_loaded = valid;
    if (o == prompt)
        prompt_loaded = valid;
    if (!valid && (o == source || o == prompt))
    {
        fallback = "File could not be loaded (binary/escape codes, unreadable or above the editor "
                   "size limit).";
        editor_readonly(o, TRUE);
    }
    if (o == source)
    {
        char *styled = syntax_colours(s ? s : fallback, syntax_language(p));
        set(o, MUIA_TextEditor_Contents, styled ? styled : (s ? s : fallback));
        free(styled);
        set(o, MUIA_TextEditor_Pen, 0xffe6edf3UL);
    }
    else if (o == transcript)
    {
        char logpath[PATH_SIZE], *log, *styled;
        int replace;
        join(logpath, current, "job.log");
        log = read_text(logpath, RESPONSE_LIMIT);
        activity_clear(&activity);
        styled = activity_update(&activity, s ? s : fallback, log ? log : "", &replace);
        set(o, MUIA_TextEditor_Contents, styled ? styled : "");
        free(styled); free(log);
    }
    else if (o == output)
        job_output(s ? s : fallback);
    else
        set(o, MUIA_TextEditor_Contents, s ? s : fallback);
    if (o == transcript)
    {
        transcript_pending = 0;
        activity_scroll_bottom(o);
    }
    free(s);
    set(o, MUIA_TextEditor_HasChanged, FALSE);
}
static int save_editor(Object *o, const char *name, unsigned long limit)
{
    char path[PATH_SIZE], *s;
    int ok;
    if (!*current || busy(current) || (o == source && !source_loaded) ||
        (o == prompt && !prompt_loaded))
    {
        message("Workspace is busy. Wait or cancel before saving.");
        return 0;
    }
    join(path, current, name);
    s = (char *)DoMethod(o, MUIM_TextEditor_ExportText);
    if (!s)
        return 0;
    ok = save_text(path, s, limit);
    FreeVec(s);
    if (ok)
        set(o, MUIA_TextEditor_HasChanged, FALSE);
    else
        message("Save failed. Previous file is preserved; check size and free disk space.");
    return ok;
}
static int save_current(void)
{
    if (*opened && value(source, MUIA_TextEditor_HasChanged) &&
        !save_editor(source, opened, SOURCE_LIMIT))
        return 0;
    if (value(prompt, MUIA_TextEditor_HasChanged) && !save_editor(prompt, "task.txt", 8192))
        return 0;
    return 1;
}
static void arm_timer(void);
static int worker_registered(const char *path)
{
    unsigned long hash = 5381;
    char portname[40];
    const char *p;
    struct MsgPort *port;
    for (p = path; *p; p++)
        hash = hash * 33 + (unsigned char)*p;
    sprintf(portname, "TermicJob.%08lx", hash);
    Forbid();
    port = FindPort((STRPTR)portname);
    Permit();
    return port != NULL;
}
static void attach_job(const char *path)
{
    int i, free_slot = -1;
    if (!worker_registered(path))
        return;
    for (i = 0; i < 16; i++)
    {
        if (jobs[i].active && !strcmp(jobs[i].path, path))
            return;
        if (!jobs[i].active)
            free_slot = i;
    }
    if (free_slot >= 0)
    {
        strcpy(jobs[free_slot].path, path);
        jobs[free_slot].active = 1;
        jobs[free_slot].polls = 0;
        jobs[free_slot].bytes = -1;
        arm_timer();
    }
}
static int internal_name(const char *n)
{
    return !strcmp(n, "name.txt") || !strcmp(n, "state.txt") || !strcmp(n, "session.txt") ||
           !strcmp(n, "last-prompt.txt") || !strcmp(n, "commit.txt") || !strcmp(n, "command.txt") || !strcmp(n, "transcript.txt");
}
static int managed_path(const char *p)
{
    const char *s = p;
    if ((strncmp(p, project_root, strlen(project_root)) || p[strlen(project_root)] != '/') &&
        strncmp(p, "Termic:Project", 14))
        return 0;
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') ||
              strchr(":/_-", *s)))
            return 0;
    return strlen(p) < PATH_SIZE - 184;
}
static char *read_value(const char *path, unsigned limit)
{
    char *s = read_text(path, limit);
    unsigned i;
    if (s)
    {
        s[strcspn(s, "\r\n")] = 0;
        for (i = 0; s[i]; i++)
            if ((unsigned char)s[i] < 32 || (unsigned char)s[i] == 155)
                s[i] = ' ';
    }
    return s;
}
static void task_label(const char *p, int index, char *labeltext, unsigned size)
{
    char path[PATH_SIZE], *name, *state;
    join(path, p, "name.txt");
    name = read_value(path, 120);
    join(path, p, "state.txt");
    state = read_value(path, 32);
    snprintf(labeltext, size, "%s [%s]", name ? name : (index ? "Task" : "Main workspace"),
             state ? state : "Ready");
    free(name);
    free(state);
}
static void task_row(const char *p)
{
    char labeltext[180];
    int i, n = value(tasks, MUIA_List_Active);
    for (i = 0; i < task_count; i++)
        if (!strcmp(task_paths[i], p))
            break;
    if (i == task_count)
        return;
    task_label(p, i, labeltext, sizeof(labeltext));
    DoMethod(tasks, MUIM_KillNotify, MUIA_List_Active);
    DoMethod(tasks, MUIM_List_Remove, i);
    DoMethod(tasks, MUIM_List_InsertSingle, labeltext, i);
    set(tasks, MUIA_List_Active, n);
    notify(tasks, MUIA_List_Active, MUIV_EveryTime, TASK);
}
static void scan_files(const char *dir, const char *prefix, int depth)
{
    BPTR l;
    struct FileInfoBlock *f;
    char path[PATH_SIZE], rel[184];
    if (depth > 6 || file_count >= MAX_ITEMS)
        return;
    l = Lock((STRPTR)dir, ACCESS_READ);
    if (!l)
        return;
    f = AllocDosObject(DOS_FIB, NULL);
    if (f && Examine(l, f))
        while (ExNext(l, f) && file_count < MAX_ITEMS)
        {
            if ((f->fib_FileName[0] == '.' && strcmp(f->fib_FileName, ".agents") && strcmp(f->fib_FileName, ".termic-version")) || internal_name(f->fib_FileName) ||
                !strcmp(f->fib_FileName, "task.txt") ||
                !strcmp(f->fib_FileName, "build-command.txt") || !strcmp(f->fib_FileName, "Tasks"))
                continue;
            if (strlen(prefix) + strlen(f->fib_FileName) + 2 >= sizeof(rel))
                continue;
            sprintf(rel, "%s%s", prefix, f->fib_FileName);
            if (f->fib_DirEntryType == ST_USERDIR)
            {
                char sub[184];
                join(path, dir, f->fib_FileName);
                if (strlen(rel) > 180)
                    continue;
                sprintf(sub, "%s/", rel);
                scan_files(path, sub, depth + 1);
            }
            else if (f->fib_DirEntryType == ST_FILE && source_name(rel))
            {
                strcpy(file_names[file_count++], rel);
                DoMethod(files, MUIM_List_InsertSingle, rel, MUIV_List_Insert_Bottom);
            }
        }
    if (f)
        FreeDosObject(DOS_FIB, f);
    UnLock(l);
}
static void refresh_files(void)
{
    loading = 1;
    DoMethod(files, MUIM_KillNotify, MUIA_List_Active);
    DoMethod(files, MUIM_List_Clear);
    file_count = 0;
    scan_files(current, "", 0);
    notify(files, MUIA_List_Active, MUIV_EveryTime, FILESEL);
    loading = 0;
}
static void open_file(const char *name)
{
    char path[PATH_SIZE];
    if (!save_current())
        return;
    strcpy(opened, name);
    join(path, current, name);
    content(source, path, "");
    editor_readonly(source, busy(current) || !source_loaded);
    set(filename, MUIA_Text_Contents, name);
}
static void profiles_picker_refresh(void);
static void load_workspace(const char *p)
{
    char path[PATH_SIZE], title[420];
    strcpy(current, p);
    attach_job(p);
    profiles_picker_refresh();
    save_text("Termic:Config/last-workspace", p, PATH_SIZE);
    *opened = 0;
    refresh_files();
    open_file("main.c");
    join(path, current, "task.txt");
    content(prompt, path, "");
    join(path, current, "transcript.txt");
    content(
        transcript, path,
        "Describe a task below, then choose Start agent.\nThe agent can read source, edit it, and "
        "invoke the local compiler.\nConfigure your API key in Settings before the first request.");
    join(path, current, "build-command.txt");
    {
        char *s = read_text(path, 512);
        set(buildcommand, MUIA_String_Contents,
            s ? s : "vc +aos68k -cpu=68020 -O1 main.c -o Program.new -lamiga");
        free(s);
    }
    join(path, current, "job.log");
    content(output, path, "Build output and explicit shell commands appear here.");
    join(path, current, "name.txt");
    {
        char *s = read_value(path, 120);
        set(taskname, MUIA_String_Contents, s ? s : "Main workspace");
        free(s);
    }
    task_row(current);
    sprintf(title, "\33b%s", current);
    set(heading, MUIA_Text_Contents, title);
    editor_readonly(source, busy(current) || !source_loaded);
    editor_readonly(prompt, busy(current) || !prompt_loaded);
    message(busy(current) ? "Running in this workspace. Other workspaces remain available."
                          : "Ready. Changes are saved when switching files or workspaces.");
}
static void scan_drawers(Object *listobj, const char *base, char paths[][PATH_SIZE], int *count)
{
    BPTR l;
    struct FileInfoBlock *f;
    char path[PATH_SIZE], namepath[PATH_SIZE], *name;
    *count = 0;
    DoMethod(listobj, MUIM_List_Clear);
    l = Lock((STRPTR)base, ACCESS_READ);
    if (!l)
        return;
    f = AllocDosObject(DOS_FIB, NULL);
    if (f && Examine(l, f))
        while (ExNext(l, f) && *count < MAX_ITEMS)
        {
            if (f->fib_DirEntryType != ST_USERDIR || f->fib_FileName[0] == '.')
                continue;
            join(path, base, f->fib_FileName);
            if (!managed_path(path))
                continue;
            strcpy(paths[*count], path);
            ++*count;
            join(namepath, path, "name.txt");
            name = read_value(namepath, 120);
            DoMethod(listobj, MUIM_List_InsertSingle, name && *name ? name : f->fib_FileName,
                     MUIV_List_Insert_Bottom);
            free(name);
        }
    if (f)
        FreeDosObject(DOS_FIB, f);
    UnLock(l);
}
static void load_tasks(void)
{
    char path[PATH_SIZE], *project_label;
    join(path, project, "name.txt");
    project_label = read_value(path, 120);
    set(projectname, MUIA_String_Contents, project_label ? project_label : "Starter project");
    free(project_label);
    loading = 1;
    DoMethod(tasks, MUIM_KillNotify, MUIA_List_Active);
    join(path, project, "Tasks");
    scan_drawers(tasks, path, task_paths, &task_count);
    if (task_count < MAX_ITEMS)
    {
        memmove(task_paths + 1, task_paths, task_count * PATH_SIZE);
        strcpy(task_paths[0], project);
        task_count++;
        DoMethod(tasks, MUIM_List_InsertSingle, "Main workspace", MUIV_List_Insert_Top);
    }
    set(tasks, MUIA_List_Active, 0);
    notify(tasks, MUIA_List_Active, MUIV_EveryTime, TASK);
    loading = 0;
    load_workspace(project);
}
/* MCP notifies through an Exec signal; only this UI task touches MUI.
 * Rebuild the task list without selecting/reloading the current editor. */
static void refresh_mcp_tasks(void)
{
    char path[PATH_SIZE], labeltext[180];int i,selected=-1;
    if(!*project)return;
    DoMethod(tasks,MUIM_KillNotify,MUIA_List_Active);
    set(tasks,MUIA_List_Quiet,TRUE);
    join(path,project,"Tasks");scan_drawers(tasks,path,task_paths,&task_count);
    if(task_count<MAX_ITEMS) {
        memmove(task_paths+1,task_paths,task_count*PATH_SIZE);strcpy(task_paths[0],project);task_count++;
        DoMethod(tasks,MUIM_List_InsertSingle,"Main workspace",MUIV_List_Insert_Top);
    }
    for(i=0;i<task_count;i++) {
        if(!strcmp(task_paths[i],current))selected=i;
        task_label(task_paths[i],i,labeltext,sizeof(labeltext));
        DoMethod(tasks,MUIM_List_Remove,i);
        DoMethod(tasks,MUIM_List_InsertSingle,labeltext,i);
        attach_job(task_paths[i]);
    }
    SetAttrs(tasks,MUIA_NoNotify,TRUE,MUIA_List_Active,selected,MUIA_List_Quiet,FALSE,TAG_DONE);
    notify(tasks,MUIA_List_Active,MUIV_EveryTime,TASK);
}
static void load_projects(void)
{
    char path[PATH_SIZE];
    char *name;
    loading = 1;
    DoMethod(projects, MUIM_KillNotify, MUIA_List_Active);
    scan_drawers(projects, project_root, project_paths, &project_count);
    if (project_count < MAX_ITEMS)
    {
        memmove(project_paths + 1, project_paths, project_count * PATH_SIZE);
        strcpy(project_paths[0], "Termic:Project");
        project_count++;
        DoMethod(projects, MUIM_List_InsertSingle, "Starter project", MUIV_List_Insert_Top);
    }
    set(projects, MUIA_List_Active, 0);
    notify(projects, MUIA_List_Active, MUIV_EveryTime, PROJECT);
    loading = 0;
    strcpy(project, project_paths[0]);
    join(path, project, "name.txt");
    name = read_value(path, 120);
    set(projectname, MUIA_String_Contents, name ? name : "Starter project");
    free(name);
    load_tasks();
}
/* Clone only regular editable files. Never follow links or copy credentials,
 * build products, Git internals or active job state into a new task. */
static int clone_sources(const char *from, const char *to, int depth)
{
    BPTR l;
    struct FileInfoBlock *f;
    char src[PATH_SIZE], dst[PATH_SIZE];
    int ok = 1;
    char *text;
    if (depth > 6)
        return 0;
    l = Lock((STRPTR)from, ACCESS_READ);
    if (!l)
        return 0;
    f = AllocDosObject(DOS_FIB, NULL);
    if (!f || !Examine(l, f))
        ok = 0;
    else
        while (ok && ExNext(l, f))
        {
            if ((f->fib_FileName[0] == '.' && strcmp(f->fib_FileName, ".agents") && strcmp(f->fib_FileName, ".termic-version")) || internal_name(f->fib_FileName) ||
                !strcmp(f->fib_FileName, "Tasks") || !strcmp(f->fib_FileName, "name.txt") ||
                !strcmp(f->fib_FileName, "transcript.txt") ||
                !strcmp(f->fib_FileName, "command.txt"))
                continue;
            if (strlen(from) + strlen(f->fib_FileName) + 2 >= PATH_SIZE ||
                strlen(to) + strlen(f->fib_FileName) + 2 >= PATH_SIZE)
            {
                ok = 0;
                break;
            }
            join(src, from, f->fib_FileName);
            join(dst, to, f->fib_FileName);
            if (f->fib_DirEntryType == ST_USERDIR)
                ok = mkdir_safe(dst) && clone_sources(src, dst, depth + 1);
            else if (f->fib_DirEntryType == ST_FILE && (source_name(f->fib_FileName) || (depth==3 && !strcmp(f->fib_FileName,".termic-version"))))
            {
                text = read_text(src, SOURCE_LIMIT);
                ok = text && save_text(dst, text, SOURCE_LIMIT);
                free(text);
            }
        }
    if (f)
        FreeDosObject(DOS_FIB, f);
    UnLock(l);
    return ok;
}
static void create_workspace(int task)
{
    char base[PATH_SIZE], path[PATH_SIZE], namepath[PATH_SIZE], name[80];
    int n;
    if (task && busy(current))
    {
        message("Wait for this workspace job before copying its source into a new task.");
        return;
    }
    if (!save_current())
        return;
    if (task)
    {
        join(base, project, "Tasks");
        mkdir_safe(base);
    }
    else
        strcpy(base, project_root);
    for (n = 1; n < 10000; n++)
    {
        snprintf(path, sizeof(path), "%s/%s-%04d", base, task ? "task" : "project", n);
        if (!exists(path))
            break;
    }
    if (n == 10000 || !mkdir_safe(path))
    {
        message("Cannot create workspace.");
        return;
    }
    if (!clone_sources(task ? current : "Termic:Template", path, 0))
    {
        message("Copy failed. Partial drawer retained for recovery.");
        return;
    }
    sprintf(name, "%s %d", task ? "Task" : "Project", n);
    join(namepath, path, "name.txt");
    save_text(namepath, name, 120);
    if (task)
    {
        load_tasks();
        {
            int i;
            for (i = 0; i < task_count; i++)
                if (!strcmp(task_paths[i], path))
                    SetAttrs(tasks, MUIA_NoNotify, TRUE, MUIA_List_Active, i, TAG_DONE);
        }
        load_workspace(path);
    }
    else
    {
        load_projects();
        {
            int i;
            for (i = 0; i < project_count; i++)
                if (!strcmp(project_paths[i], path))
                    SetAttrs(projects, MUIA_NoNotify, TRUE, MUIA_List_Active, i, TAG_DONE);
        }
        strcpy(project, path);
        load_tasks();
    }
    message("Workspace created. Rename it below the list. Task files are independent snapshots.");
}
static void arm_timer(void)
{
    if (!timer_io || timer_pending)
        return;
    timer_io->tr_node.io_Command = TR_ADDREQUEST;
    timer_io->tr_time.tv_secs = 0;
    timer_io->tr_time.tv_micro = 500000;
    SendIO((struct IORequest *)timer_io);
    timer_pending = 1;
}
static int any_jobs(void)
{
    int i;
    for (i = 0; i < 16; i++)
        if (jobs[i].active)
            return 1;
    return 0;
}
static int launch_at(const char *target, const char *action, int page)
{
    char path[PATH_SIZE], line[512];
    BPTR in, out;
    int i;
    if (busy(target))
    {
        message("A job is already running in this workspace.");
        return 0;
    }
    for (i = 0; i < 16 && jobs[i].active; i++)
        ;
    if (i == 16)
    {
        message("All 16 worker slots are in use.");
        return 0;
    }
    join(path, target, ".result");
    DeleteFile((STRPTR)path);
    join(path, target, "job.log");
    out = Open((STRPTR)path, MODE_READWRITE);
    in = Open((STRPTR) "NIL:", MODE_OLDFILE);
    if (!out || !in)
    {
        if (out)
            Close(out);
        if (in)
            Close(in);
        message("Cannot open job log.");
        return 0;
    }
    SetFileSize(out, 0, OFFSET_BEGINNING);
    sprintf(line, "Stack 262144\nTermic:TermicAgent %s --project \"%s\"\n", action, target);
    join(path, target, ".job");
    if (!save_text(path, line, 1024))
    {
        Close(in);
        Close(out);
        message("Cannot write worker script.");
        return 0;
    }
    sprintf(line, "Execute \"%s\"", path);
    join(path,target,".launch");
    {BPTR reservation=CreateDir((STRPTR)path);
     if(!reservation) {Close(in);Close(out);message("Cannot reserve task launch.");return 0;}
     UnLock(reservation);}
    if (SystemTags((STRPTR)line, SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE, TAG_DONE) == -1)
    {
        Close(in);
        Close(out);
        DeleteFile((STRPTR)path);
        message("Cannot launch worker.");
        return 0;
    }
    strcpy(jobs[i].path, target);
    jobs[i].active = 1;
    jobs[i].polls = 0;
    jobs[i].bytes = -1;
    join(path, target, "state.txt");
    save_text(path, "Running", 32);
    task_row(target);
    if (!strcmp(target,current)) {
    editor_readonly(source, TRUE);
    editor_readonly(prompt, TRUE);
    if (page >= 0)
        set(pages, MUIA_Group_ActivePage, page);
    job_output("Starting native worker...\n");
    message(page == 0 ? "Agent running. Cyan responses and amber logs appear together."
                      : "Running. You can switch tasks while this worker continues.");
    }
    arm_timer();
    flow_changed();
    return 1;
}
static void launch(const char *action, int page)
{
    if(save_current()) launch_at(current,action,page);
}
/* Follow the latest activity without disturbing a clipboard selection.
 * Catch up to the bottom when the selection is released. */
static void refresh_transcript(void)
{
    char path[PATH_SIZE], *agent, *log, *styled;
    int replace;
    if (value(transcript, MUIA_TextEditor_AreaMarked))
    {
        transcript_pending = 1;
        return;
    }
    transcript_pending = 0;
    join(path, current, "transcript.txt");
    agent = read_text(path, RESPONSE_LIMIT);
    if (!agent && exists(path)) return;
    join(path, current, "job.log");
    log = read_text(path, RESPONSE_LIMIT);
    if (!log && exists(path)) { free(agent); return; }
    /* Backup-and-replace writes briefly remove a file. Keep its last
     * snapshot during that gap, so the combined history is not reset. */
    styled = activity_update(&activity, agent ? agent : (activity.agent ? activity.agent : ""),
                             log ? log : (activity.log ? activity.log : ""), &replace);
    free(agent); free(log);
    if (!styled) return;
    if (!*styled && !replace) { free(styled); return; }
    if (replace) set(transcript, MUIA_TextEditor_Contents, styled);
    else DoMethod(transcript, MUIM_TextEditor_InsertText, styled,
                  MUIV_TextEditor_InsertText_Bottom);
    set(transcript, MUIA_TextEditor_HasChanged, FALSE);
    activity_scroll_bottom(transcript);
    free(styled);
}
static void poll_jobs(void)
{
    int i,completion;
    char path[PATH_SIZE], *s;
    for (i = 0; i < 16; i++)
        if (jobs[i].active)
        {
            join(path, jobs[i].path, "job.log");
            if (!strcmp(current, jobs[i].path))
            {
                refresh_transcript();
                s = read_text(path, RESPONSE_LIMIT);
                if (s)
                {
                    long n = strlen(s);
                    if (n != jobs[i].bytes)
                    {
                        job_output(s);
                        if (value(pages, MUIA_Group_ActivePage) == 3)
                            set(review, MUIA_TextEditor_Contents, s);
                        jobs[i].bytes = n;
                    }
                    free(s);
                }
            }
            join(path, jobs[i].path, ".result");
            s = read_text(path, 32);
            completion=job_watch(worker_registered(jobs[i].path),s!=NULL,&jobs[i].polls);
            if (completion<0)
            {
                jobs[i].active = 0;
                join(path,jobs[i].path,"state.txt");save_text(path,"Needs attention",32);
                flow_completed(jobs[i].path,20);
                if (!strcmp(current, jobs[i].path))
                    message("Worker exited without a result. Check its log and any stale .busy "
                            "drawer.");
            }
            if (s && completion>0)
            {
                jobs[i].active = 0;
                join(path, jobs[i].path, "state.txt");
                save_text(path, atoi(s) == 0 ? "Review" : "Needs attention", 32);
                flow_completed(jobs[i].path,atoi(s));
                task_row(jobs[i].path);
                if (!strcmp(current, jobs[i].path))
                {
                    /* The worker may finish between our log read and result
                     * read. Refresh once more after observing completion. */
                    join(path, current, "job.log");
                    content(output, path, "No output was captured for this job.");
                    editor_readonly(source, !source_loaded);
                    editor_readonly(prompt, !prompt_loaded);
                    message(atoi(s) == 0
                                ? "Job completed successfully. Review changes before running."
                                : "Job failed or was cancelled. See the amber log on Agent.");
                    refresh_transcript();
                    refresh_files();
                    if (*opened && !value(source, MUIA_TextEditor_HasChanged))
                    {
                        join(path, current, opened);
                        content(source, path, "");
                    }
                }
                free(s);
            }
            else free(s);
        }
    editor_recolour();
    flow_tick();
    if (any_jobs() || colour_pending)
        arm_timer();
}
static void cancel(void)
{
    unsigned long hash = 5381;
    const char *p;
    char name[40];
    struct MsgPort *port;
    for (p = current; *p; p++)
        hash = hash * 33 + (unsigned char)*p;
    sprintf(name, "TermicJob.%08lx", hash);
    Forbid();
    port = FindPort((STRPTR)name);
    if (port)
        Signal(port->mp_SigTask, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E);
    Permit();
    message(port ? "Cancellation requested. Compiler or network operation will finish unwinding."
                 : "No worker is currently registered for this workspace.");
}
static void restore_preferences(void)
{
    char *s = read_preference("model", 128);
    if (s)
        s[strcspn(s, "\r\n")] = 0;
    set(model, MUIA_String_Contents, s ? s : "gpt-5-mini");
    free(s);
    s = read_preference("projects-drawer", 180);
    if (s)
        s[strcspn(s, "\r\n")] = 0;
    set(projectdrawer, MUIA_String_Contents, s ? s : project_root);
    free(s);
    set(keyfield, MUIA_String_Contents, "");
}
static int write_preference(const char *base, const char *name, const char *text, unsigned limit)
{
    char path[PATH_SIZE];
    int ok;
    join(path, base, name);
    ok = save_text(path, text, limit);
    if (ok && !strcmp(name, "api-key"))
    {
        strcat(path, ".bak");
        DeleteFile((STRPTR)path);
    }
    return ok;
}
static void configure(int permanent)
{
    const char *s = (char *)value(model, MUIA_String_Contents);
    const char *k = (char *)value(keyfield, MUIA_String_Contents);
    const char *drawer = (char *)value(projectdrawer, MUIA_String_Contents);
    char *retained = NULL;
    unsigned i;
    int ok = 1, valid = *drawer && *s;
    for (i = 0; drawer[i]; i++)
        if (!isalnum((unsigned char)drawer[i]) && !strchr(":/_-", drawer[i]))
            valid = 0;
    for (i = 0; s[i]; i++)
        if (isspace((unsigned char)s[i]))
            valid = 0;
    if (!valid)
    {
        message(
            "Enter a model without spaces and a project drawer using letters, digits, : / _ -.");
        return;
    }
    if (!*k)
        k = retained = read_preference("api-key", 512);
    mkdir_safe("ENV:Termic");
    /* Validate all fields before writing. Report partial I/O failures honestly. */
    if (permanent)
    {
        ok = write_preference("Termic:Config", "model", s, 128);
        if (!write_preference("Termic:Config", "projects-drawer", drawer, 180))
            ok = 0;
        if (k && !write_preference("Termic:Config", "api-key", k, 512))
            ok = 0;
    }
    if (!write_preference("ENV:Termic", "model", s, 128))
        ok = 0;
    if (!write_preference("ENV:Termic", "projects-drawer", drawer, 180))
        ok = 0;
    if (k && !write_preference("ENV:Termic", "api-key", k, 512))
        ok = 0;
    free(retained);
    set(keyfield, MUIA_String_Contents, "");
    if (ok)
    {
        set(pages, MUIA_Group_ActivePage, 0);
        set(win, MUIA_Window_ActiveObject, prompt);
    }
    message(
        !ok ? "Some preferences could not be written. Check free space and protection, then retry."
        : permanent
            ? "Preferences saved. Restart Termic to use a changed project drawer."
            : "Preferences in use until reboot. Restart Termic for a changed project drawer.");
}
#include "profiles_ui.h"
#include "access_ui.h"
static void open_help(void)
{
    BPTR in, out;
    if (!exists("Termic:Termic.guide") || !exists("SYS:Utilities/MultiView"))
    {
        message("Install Termic.guide beside TermicNative and MultiView in SYS:Utilities.");
        return;
    }
    in = Open((STRPTR) "NIL:", MODE_OLDFILE);
    out = Open((STRPTR) "NIL:", MODE_NEWFILE);
    if (!in || !out ||
        SystemTags((STRPTR) "SYS:Utilities/MultiView Termic:Termic.guide", SYS_Input, in,
                   SYS_Output, out, SYS_Asynch, TRUE, TAG_DONE) == -1)
    {
        if (in)
            Close(in);
        if (out)
            Close(out);
        message("Unable to open AmigaGuide manual.");
    }
}
static int icon(const char *p)
{
    struct DiskObject *d;
    int ok = 0;
    IconBase = OpenLibrary((STRPTR) "icon.library", 37);
    if (!IconBase)
        return 20;
    d = GetDefDiskObject(WBTOOL);
    if (d)
    {
        d->do_StackSize = 262144;
        d->do_CurrentX = d->do_CurrentY = NO_ICON_POSITION;
        ok = PutDiskObject((STRPTR)p, d);
        FreeDiskObject(d);
    }
    CloseLibrary(IconBase);
    return ok ? 0 : 20;
}
/* Same lazy, application-owned MUI window pattern as the ZZ8-Bit launcher. */
static Object *about_window;
static void open_about(void)
{
    Object *close_button;
    if (!about_window)
    {
        about_window = WindowObject, MUIA_Window_Title, "About Termic Amiga",
        MUIA_Window_SizeGadget, FALSE, MUIA_Window_NoMenus, TRUE, MUIA_Window_LeftEdge,
        MUIV_Window_LeftEdge_Centered, MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
        WindowContents, VGroup, InnerSpacing(16, 14), MUIA_Group_Spacing, 10, MUIA_Font,
        bodyfont ? (ULONG)bodyfont : MUIV_Font_Normal, Child,
        label("\33c\33bTermic Amiga " TERMIC_VERSION), Child,
        label("\33cNative development workspace for AmigaOS"), Child, TextObject, TextFrame,
        MUIA_Text_Contents,
        "Version: " TERMIC_VERSION "   |   Build date: " TERMIC_VERSION_DATE "\n"
        "Platform: AmigaOS 3.2 / 68020+ / MUI\n\n"
        "Amiga adaptation: Dominic Skilton\n"
        "Development assistance: Codex\n"
        "Original Termic: Simion Agavriloaei and contributors\n\n"
        "Interface: MUI (Stefan Stuntz), TextEditor.mcc contributors\n"
        "Toolchain: VBCC, vasm, vlink and Amiga NDK\n"
        "Libraries and tools: AmiSSL, cJSON, native Git\n"
        "Amiga MCP bridge: geekychris and contributors\n"
        "github.com/geekychris/amiga_mcp\n\n"
        "Source and support: github.com/SkiltonUSA/Termic-Amiga\n"
        "Original project: termic.dev\n"
        "Licence: GNU AGPL v3 or later; dependencies retain their licences.",
        End, Child, HGroup, Child, HVSpace, Child, close_button = button("Close", ABOUTCLOSE),
        Child, HVSpace, End, End, End;
        if (!about_window)
        {
            message("Unable to open About window.");
            return;
        }
        DoMethod(app, OM_ADDMEMBER, about_window);
        notify(about_window, MUIA_Window_CloseRequest, TRUE, ABOUTCLOSE);
        notify(about_window, MUIA_Window_InputEvent, (ULONG) "-repeat esc", ABOUTCLOSE);
        notify(about_window, MUIA_Window_InputEvent, (ULONG) "-repeat return", ABOUTCLOSE);
        set(about_window, MUIA_Window_DefaultObject, close_button);
    }
    set(about_window, MUIA_Window_CloseRequest, FALSE);
    set(about_window, MUIA_Window_Open, TRUE);
    set(about_window, MUIA_Window_Activate, TRUE);
}
#include "prompts_ui.h"
#include "shortcuts_ui.h"
#include "editor_tools_ui.h"
#include "change_review_ui.h"
#include "taskflow_ui.h"
#include "scratchpads_ui.h"
#include "git_ui.h"
static Object *menu_items[COMMANDSCLOSE + 1];
static Object *edit_target(void)
{
    Object *active = (Object *)value(win, MUIA_Window_ActiveObject);
    if (active == source || active == prompt || active == output || active == review ||
        active == transcript || active == cli_output)
        return active;
    return NULL;
}
static void menu_enabled(int id, int enabled)
{
    enabled = !!enabled;
    if (value(menu_items[id], MUIA_Menuitem_Enabled) != enabled)
        set(menu_items[id], MUIA_Menuitem_Enabled, enabled);
}
static void update_menus(void)
{
    Object *o = edit_target();
    int writable = o && !value(o, MUIA_TextEditor_ReadOnly);
    int marked = o && value(o, MUIA_TextEditor_AreaMarked);
    int running = *current && busy(current);
    int editable = *current && !running;
    menu_enabled(CUT, writable && marked);
    menu_enabled(COPY, marked);
    menu_enabled(PASTE, writable);
    menu_enabled(ERASE, writable && marked);
    menu_enabled(SELECTALL, o != NULL);
    menu_enabled(UNDO, writable && value(o, MUIA_TextEditor_UndoAvailable));
    menu_enabled(REDO, writable && value(o, MUIA_TextEditor_RedoAvailable));
    menu_enabled(SAVE, editable);
    menu_enabled(BUILD, editable);
    menu_enabled(RUN, editable);
    menu_enabled(AGENT, editable);
    menu_enabled(CANCEL, running);
    menu_enabled(SAVECONFIG, value(pages, MUIA_Group_ActivePage) == 4);
    menu_enabled(USECONFIG, value(pages, MUIA_Group_ActivePage) == 4);
    menu_enabled(CANCELCONFIG, value(pages, MUIA_Group_ActivePage) == 4);
}
static void menu_item(Object *menu, const char *title, const char *shortcut, int id)
{
    Object *item = MenuitemObject, MUIA_Menuitem_Title, title, MUIA_Menuitem_Shortcut, shortcut,
           End;
    DoMethod(menu, OM_ADDMEMBER, item);
    if (id)
    {
        if (id <= COMMANDSCLOSE) menu_items[id] = item;
        notify(item, MUIA_Menuitem_Trigger, MUIV_EveryTime, id);
    }
}
static Object *new_menu(Object *strip, const char *title)
{
    Object *menu = MenuObject, MUIA_Menu_Title, title, End;
    DoMethod(strip, OM_ADDMEMBER, menu);
    return menu;
}
static Object *build_menus(void)
{
    Object *strip = MenustripObject, End;
    Object *menu = new_menu(strip, "Project");
    menu_item(menu, "New", "N", NEWPROJECT);
    menu_item(menu, "Open...", "O", IMPORTPROJECT);
    menu_item(menu, NM_BARLABEL, NULL, 0);
    menu_item(menu, "Save", "S", SAVE);
    menu_item(menu, NM_BARLABEL, NULL, 0);
    menu_item(menu, "About...", "?", ABOUT);
    menu_item(menu, NM_BARLABEL, NULL, 0);
    menu_item(menu, "Quit", "Q", QUIT);
    menu = new_menu(strip, "Edit");
    menu_item(menu, "Cut", "X", CUT);
    menu_item(menu, "Copy", "C", COPY);
    menu_item(menu, "Paste", "V", PASTE);
    menu_item(menu, NM_BARLABEL, NULL, 0);
    menu_item(menu, "Erase", NULL, ERASE);
    menu_item(menu, "Select All", "A", SELECTALL);
    menu_item(menu, NM_BARLABEL, NULL, 0);
    menu_item(menu, "Undo", "Z", UNDO);
    menu_item(menu, "Redo", NULL, REDO);
    menu = new_menu(strip, "Workspace");
    menu_item(menu, "New Task", NULL, NEWTASK);
    menu_item(menu, "Install agent instructions", NULL, INSTRUCTIONS);
    menu_item(menu, "Mark Done", NULL, MARKDONE);
    menu_item(menu, NM_BARLABEL, NULL, 0);
    menu_item(menu, "Build", "B", BUILD);
    menu_item(menu, "Run", "R", RUN);
    menu_item(menu, "Start Agent", NULL, AGENT);
    menu_item(menu, "Cancel Job", NULL, CANCEL);
    menu_item(menu, NM_BARLABEL, NULL, 0);
    menu_item(menu, "Git Status", NULL, GITSTATUS);
    menu_item(menu, "Git Diff", NULL, DIFF);
    menu = new_menu(strip, "View");
    menu_item(menu, "Kanban board...", "Y", KB_OPEN);
    menu_item(menu, "Task queue...", "J", Q_OPEN);
    menu_item(menu, "Scratchpads...", "L", SP_OPEN);
    menu_item(menu, "Review agent edits...", "U", CR_OPEN);
    menu_item(menu, "Git branches and worktrees...", "G", GT_OPEN);
    menu_item(menu, "Agent", "1", PAGE_AGENT);
    menu_item(menu, "Editor", "2", PAGE_EDITOR);
    menu_item(menu, "Build / CLI", "3", PAGE_OUTPUT);
    menu_item(menu, "Changes", "4", PAGE_CHANGES);
    menu = new_menu(strip, "Settings");
    menu_item(menu, "Preferences", ",", SETTINGS);
    menu_item(menu, "Prompts...", "P", PL_OPEN);
    menu_item(menu, "Agents / CLI / Scripts...", "E", PF_OPEN);
    menu_item(menu, "Agent access / MCP...", "M", AC_OPEN);
    menu_item(menu, "MUI Settings...", NULL, MUIPREFS);
    menu_item(menu, NM_BARLABEL, NULL, 0);
    menu_item(menu, "Save Settings", NULL, SAVECONFIG);
    menu_item(menu, "Use Settings", NULL, USECONFIG);
    menu_item(menu, "Cancel Edits", NULL, CANCELCONFIG);
    menu = new_menu(strip, "Help");
    menu_item(menu, "Contents...", NULL, HELP);
    menu_item(menu, "Keyboard shortcuts...", "K", SHORTCUTS);
    return strip;
}
/* Icon-only controls must not set a vertical pane's maximum width.
 * Keep the two navigation panes readable while allowing balance resizing. */
static struct MUI_CustomClass *pane_class;
static ULONG pane_dispatch(__reg("a0") struct IClass *cl,
                           __reg("a2") Object *obj,__reg("a1") Msg msg)
{
    ULONG result=DoSuperMethodA(cl,obj,msg);
    if(msg->MethodID==MUIM_AskMinMax) {
        struct MUI_MinMax *mm=((struct MUIP_AskMinMax *)msg)->MinMaxInfo;
        if(mm->MinWidth<230)mm->MinWidth=230;
        if(mm->DefWidth<mm->MinWidth)mm->DefWidth=mm->MinWidth;
        mm->MaxWidth=MUI_MAXMAX;
    }
    return result;
}
#define PaneGroup NewObject(pane_class->mcc_Class,NULL
static int build_ui(void)
{
    Object *body, *left, *right, *center, *titlebar, *grip, *tabtitles = NULL;
    pane_class=__termic_mui_class(MUIMasterBase,NULL,MUIC_Group,NULL,0,(APTR)pane_dispatch);
    if(!pane_class)return 0;
    chrome_init_scale(bodyfont);
    profiles_load(&profile_live);
    app = ApplicationObject, MUIA_Application_Title, "Termic", MUIA_Application_Base, "TERMIC",
    MUIA_Application_Version, version_tag + 1, MUIA_Application_SingleTask, TRUE,
    MUIA_Application_Description, "Native Amiga development workspace", End;
    if (!app)
        return 0;
    if (MUIMasterBase->lib_Version >= 21)
    {
        /* MUI 5's Title class gives each tab its own font-aware text object. */
        tabtitles =
            MUI_NewObject("Title.mui", Child, label(tabs[0]), Child, label(tabs[1]), Child,
                          label(tabs[2]), Child, label(tabs[3]), Child, label(tabs[4]), TAG_DONE);
        if (!tabtitles)
            return 0;
    }
    left = VGroup, MUIA_Group_Spacing, 8, MUIA_Weight, 22, Child, PaneGroup, GroupFrame, Child,
    label("\33bProjects"), InnerSpacing(6, 6), MUIA_Group_Spacing, 6, Child, list(&projects), Child,
    projectname = entry(120), Child, HGroup, Child, button("New project", NEWPROJECT), Child,
    button("Rename", RENAMEPROJECT), Child, HVSpace, End, Child, HGroup, Child, button("Import source drawer...", IMPORTPROJECT), Child, HVSpace, End,
    End, Child, PaneGroup, GroupFrame, Child, label("\33bTasks"), InnerSpacing(6, 6),
    MUIA_Group_Spacing, 6, Child, list(&tasks), Child, taskname = entry(120), Child, HGroup, Child,
    button("New task", NEWTASK), Child, button("Rename", RENAMETASK), Child, HVSpace, End, Child,
    HGroup, Child, button("Kanban", KB_OPEN), Child, button("Queue", Q_OPEN), Child,
    button("Mark done", MARKDONE), Child, HVSpace, End, End, End;
    center = VGroup, MUIA_Weight, 65, Child, heading = label("Termic workspace"), Child,
    pages = MUI_NewObject(tabtitles ? MUIC_Group : MUIC_Register,
    tabtitles ? TAG_IGNORE : MUIA_Register_Titles, tabs, MUIA_Group_PageMode, TRUE,
    MUIA_Font, bodyfont ? (ULONG)bodyfont : MUIV_Font_Normal,
    tabtitles ? Child : TAG_IGNORE, tabtitles, Child, VGroup, Child,
    VGroup, MUIA_Weight, 80, Child, label("\33bActivity: Agent (cyan) / Log (amber)"), Child,
    editor(&transcript, 1), End, Child, HGroup, Child, history_find=entry(120),
    Child, button("Find history", HISTORYNEXT), Child, button("Prompts", PL_OPEN), Child, button("Shortcuts", SHORTCUTS), End,
    Child, HGroup, MUIA_Weight, 20, Child, VGroup, MUIA_Weight, 65,
    Child, label("\33bTask instructions"), Child, editor(&prompt, 0), End, Child,
    VGroup, MUIA_Weight, 35, Child, label("Agent profile"), Child, profile_picker=cycle(kinds),
    Child, HGroup, Child, label("YOLO"), Child, profile_fast=cycle(yesno), End, Child,
    HGroup, Child, button("Save task", SAVETASK), Child, button("Start agent", AGENT), Child,
    button("Cancel", CANCEL), Child, button("New conversation", NEWSESSION), Child, HVSpace, End,
    Child, HVSpace, End, End, End, Child,
    VGroup, Child, filename = label("main.c"), Child, editor(&source, 0), Child, HGroup, Child,
    findtext = entry(120), Child, button("Find", FIND), Child, button("Next", FINDNEXT), End, End,
    Child, VGroup, Child, VGroup,MUIA_Weight,80,Child,editor(&output, 1),End,Child,
    VGroup,MUIA_Weight,20,Child,label("Compiler locations (double-click to open)"),Child,diagnostic_view=list(&diagnostic_list),End,Child,
    label("Full job output. Run CLI commands from the panel below Files."), End, Child, VGroup,
    Child, HGroup, Child, button("Status", GITSTATUS), Child, button("Diff", DIFF), Child,
    button("Init repository", GITINIT), Child, button("Review agent edits", CR_OPEN), Child,button("Git branches...",GT_OPEN),End, Child, editor(&review, 1), Child,
    label("Commit message (configure Git identity in the shell first)"), Child, commit = entry(256),
    Child, button("Stage source and commit", GITCOMMIT), End, Child, VGroup, Child, VGroup,
    GroupFrame, Child, HGroup, Child, button("Agents / CLI / Scripts...", PF_OPEN), Child, button("Agent access / MCP...", AC_OPEN), End, Child, label("\33bDefault API connection"), InnerSpacing(8, 8), MUIA_Group_Spacing, 6, Child,
    ColGroup(2), MUIA_Group_HorizSpacing, 10, MUIA_Group_VertSpacing, 6, Child, label("Model"),
    Child, model = entry(128), Child, label("API key"), Child, keyfield = StringObject, StringFrame,
    MUIA_Font, bodyfont ? (ULONG)bodyfont : MUIV_Font_Normal, MUIA_String_Secret, TRUE,
    MUIA_String_MaxLen, 512, MUIA_CycleChain, 1, End, End, Child,
    label("Leave the key blank to retain it. The stored key is never displayed."), Child,
    button("Test verified HTTPS", PROBE), End, Child, VGroup, GroupFrame, Child,
    label("\33bDevelopment"), InnerSpacing(8, 8), MUIA_Group_Spacing, 6, Child, ColGroup(2),
    MUIA_Group_HorizSpacing, 10, MUIA_Group_VertSpacing, 6, Child, label("Project drawer"), Child,
    projectdrawer = entry(180), Child, label("Build command"), Child, buildcommand = entry(512),
    End, Child,
    label("Restart after changing the project drawer. Builds must produce Program.new."), Child,
    HGroup, Child, button("Save build command", SAVEBUILD), Child, HVSpace, End, End, Child,
    label("Builds run locally with VBCC / NDK. Tasks have independent source copies.\n"
          "Cloud inference needs an API key and internet access.\n"
          "ZZ9000 supplies the RTG display; ARM compute is not enabled."),
    Child, HVSpace, Child, HGroup, Child, button("Save", SAVECONFIG), Child,
    button("Use", USECONFIG), Child, button("Cancel", CANCELCONFIG), Child, HVSpace, End, End, End, End;
    right = VGroup, MUIA_Weight, 23, MUIA_Group_Spacing, 4,
    Child, PaneGroup, GroupFrame, MUIA_Weight, 50, Child, label("\33bFiles"), InnerSpacing(6, 6), MUIA_Group_Spacing,
    6, Child, list(&files), Child, newname = entry(180), Child,
    HGroup, Child, button("Create file", NEWFILE), Child, button("Refresh files", REFRESH), Child, HVSpace, End, Child,
    label("C/H, Python, ARexx,\ntext and Markdown.\nSaves keep .bak copies."), End,
    Child, BalanceObject, End,
    Child, PaneGroup, GroupFrame, MUIA_Weight, 50, InnerSpacing(6, 6), MUIA_Group_Spacing, 6,
    Child, HGroup, Child, label("\33bCLI"), Child, command_mode=cycle(command_modes), End,
    Child, editor(&cli_output, 1), Child, command = entry(1024),
    Child, HGroup, Child, button("Execute", SHELL), Child, button("Cancel job", CANCEL),
    Child, HVSpace, End, End, End;
    set(command, MUIA_ShortHelp, "Command or script arguments in the selected workspace. Return executes; Shift F10 stops.");
    body = VGroup, MUIA_Font, bodyfont ? (ULONG)bodyfont : MUIV_Font_Normal, MUIA_Group_Spacing, 7, InnerSpacing(8, 6), Child, HGroup, Child,
    label("\33bTERMIC / AMIGA " TERMIC_VERSION), Child, HVSpace, Child, button("Build", BUILD),
    Child, button("Run", RUN), Child, button("Settings", SETTINGS), Child, button("About", ABOUT),
    Child, button("Quit", QUIT), End, Child, HGroup, Child, left, Child, BalanceObject, End, Child,
    center, Child, BalanceObject, End, Child, right, End, Child, status = label("Ready"), End;
    titlebar = chrome_area(bodyfont, 0);
    grip = chrome_area(bodyfont, 1);
    if (!titlebar || !grip) return 0;
    body = VGroup, MUIA_Group_Spacing, 0, InnerSpacing(2, 2), Child,
        HGroup, MUIA_Group_Spacing, 2, MUIA_FixHeight, chrome_sizes.bar,
        MUIA_Background, "2:r2d2d2d2d,5d5d5d5d,9d9d9d9d",
        Child, chrome_button(QUIT), Child, titlebar,
        Child, chrome_button(WINDOW_DEPTH), Child, chrome_button(WINDOW_ZOOM), End,
        Child, body, Child, HGroup, Child, HVSpace, Child, grip, End, End;
    win = WindowObject, MUIA_Window_Title, NULL,
    MUIA_Window_ScreenTitle, "Termic Amiga Workspace",
    MUIA_Window_Borderless, TRUE, MUIA_Window_DragBar, FALSE,
    MUIA_Window_CloseGadget, FALSE, MUIA_Window_DepthGadget, FALSE,
    MUIA_Window_SizeGadget, FALSE, MUIA_Window_ID,
    MAKE_ID('T', 'R', 'M', 'C'), MUIA_Window_Width, 1400, MUIA_Window_Height, 650,
    MUIA_Window_Menustrip, build_menus(), WindowContents, body, End;
    if (!win)
        return 0;
    DoMethod(app, OM_ADDMEMBER, win);
    profile_window=build_profiles_window();
    if(!profile_window) return 0;
    access_window=build_access_window();
    if(!access_window) return 0;
    if(!flow_build_windows() || !change_build_window() || !pad_build_window() || !git_build_window())return 0;
    profiles_picker_refresh();
    notify(win, MUIA_Window_CloseRequest, TRUE, QUIT);
    notify(win, MUIA_Window_InputEvent, (ULONG) "-repeat help", HELP);
    notify(win, MUIA_Window_InputEvent, (ULONG) "shift help", ABOUT);
    notify(win, MUIA_Window_ActiveObject, MUIV_EveryTime, UPDATE_MENUS);
    notify(projects, MUIA_List_Active, MUIV_EveryTime, PROJECT);
    notify(tasks, MUIA_List_Active, MUIV_EveryTime, TASK);
    notify(files, MUIA_List_Active, MUIV_EveryTime, FILESEL);
    {
        const char *keys[] = {"-repeat f1", "-repeat f2", "-repeat f3", "-repeat f4",
                              "-repeat f5", "-repeat f6", "-repeat f7", "-repeat f8",
                              "-repeat f9", "-repeat f10"};
        int ids[] = {PAGE_AGENT, PAGE_EDITOR, PAGE_OUTPUT, PAGE_CHANGES, PAGE_SETTINGS,
                     BUILD,      RUN,         SAVE,        NEWTASK,      QUIT};
        int i;
        for (i = 0; i < 10; i++)
            notify(win, MUIA_Window_InputEvent, (ULONG)keys[i], ids[i]);
    }
    {
        const char *keys[] = {"shift f1", "shift f2", "shift f3", "shift f4", "shift f5",
                              "shift f6", "shift f7", "shift f8", "shift f9", "shift f10"};
        int ids[] = {AGENT,     FOCUSFILE, FOCUSCOMMAND, FOCUSCOMMIT, GITINIT,
                     GITSTATUS, DIFF,      MARKDONE,     NEWPROJECT,  CANCEL};
        int i;
        for (i = 0; i < 10; i++)
            notify(win, MUIA_Window_InputEvent, (ULONG)keys[i], ids[i]);
    }
    notify(diagnostic_view,MUIA_Listview_DoubleClick,TRUE,ED_DIAGNOSTIC);
    notify(source,MUIA_TextEditor_ContentsChanged,TRUE,ED_CHANGED);
    notify(history_find, MUIA_String_Acknowledge, MUIV_EveryTime, HISTORYNEXT);
    notify(newname, MUIA_String_Acknowledge, MUIV_EveryTime, NEWFILE);
    notify(commit, MUIA_String_Acknowledge, MUIV_EveryTime, GITCOMMIT);
    notify(command, MUIA_String_Acknowledge, MUIV_EveryTime, SHELL);
    restore_preferences();
    {
        Object *editors[] = {source, prompt, transcript, output, review, cli_output};
        int i;
        for (i = 0; i < 6; i++)
        {
            notify(editors[i], MUIA_TextEditor_AreaMarked, MUIV_EveryTime, UPDATE_MENUS);
            notify(editors[i], MUIA_TextEditor_UndoAvailable, MUIV_EveryTime, UPDATE_MENUS);
            notify(editors[i], MUIA_TextEditor_RedoAvailable, MUIV_EveryTime, UPDATE_MENUS);
        }
    }
    set(source, MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_NoStyle);
    set(source, MUIA_TextEditor_PasteColors, FALSE);
    return 1;
}
int main(int argc, char **argv)
{
    ULONG id, signals, received;
    int done = 0, result = 20, storage_failed = 0;
    BPTR root;
    char path[PATH_SIZE];
    if (argc == 2 && !strcmp(argv[1], "--version"))
    {
        puts(version_tag + 7);
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "--icon"))
        return icon(argv[2]);
    /* Claim the singleton before touching the shared assign or preferences.
       A second copy must not redirect the already running workspace's files. */
    memset(&control_port, 0, sizeof(control_port));
    control_port.mp_Node.ln_Name = (STRPTR)"TermicWorkspaceUI";
    control_port.mp_Node.ln_Type = NT_MSGPORT;
    control_port.mp_Flags = PA_IGNORE;
    control_port.mp_SigTask = FindTask(NULL);
    Forbid();
    if (!FindPort((STRPTR)"TermicWorkspaceUI"))
    {
        AddPort(&control_port);
        control_registered = 1;
    }
    Permit();
    if (!control_registered)
    {
        puts("Termic is already running. Close its window before starting another copy.");
        return 20;
    }
    root = DupLock(GetProgramDir());
    if (!root)
    {
        RemPort(&control_port);
        return 20;
    }
    if (!AssignLock((STRPTR) "Termic", root))
    {
        UnLock(root);
        RemPort(&control_port);
        return 20;
    }
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR) "intuition.library", 37);
    MUIMasterBase = OpenLibrary((STRPTR) "muimaster.library", 21);
    if (!IntuitionBase || !MUIMasterBase)
        goto cleanup;
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR) "graphics.library", 37);
    AslBase = OpenLibrary((STRPTR) "asl.library", 37);
    DiskfontBase = OpenLibrary((STRPTR) "diskfont.library", 37);
    if (GfxBase && DiskfontBase)
    {
        struct TextAttr a = {(STRPTR) "helvetica.font", 13, 0, FPF_DISKFONT},
                        b = {(STRPTR) "courier.font", 13, 0, FPF_DISKFONT};
        bodyfont = OpenDiskFont(&a);
        codefont = OpenDiskFont(&b);
    }
    {
        char *configured = read_preference("projects-drawer", 180);
        if (configured)
            configured[strcspn(configured, "\r\n")] = 0;
        if (configured && *configured)
        {
            unsigned i;
            int valid = 1;
            for (i = 0; configured[i]; i++)
                if (!isalnum((unsigned char)configured[i]) && !strchr(":/_-", configured[i]))
                    valid = 0;
            if (valid)
                strcpy(project_root, configured);
        }
        free(configured);
        if (!strncmp(project_root, "TMGIT:", 6) &&
            SystemTags((STRPTR) "Termic:TermicStorage", TAG_DONE))
        {
            strcpy(project_root, "Termic:Projects");
            storage_failed = 1;
        }
    }
    if (!build_ui())
    {
        puts("MUI 5.0 and TextEditor.mcc 15.56 are required. Run TermicCheck.");
        goto cleanup;
    }
    mkdir_safe(project_root);
    mkdir_safe("Termic:Config");
    {
        char *last = argc == 3 && !strcmp(argv[1], "--workspace")
                         ? NULL
                         : read_text("Termic:Config/last-workspace", PATH_SIZE);
        char desired[PATH_SIZE];
        const char *p = argc == 3 && !strcmp(argv[1], "--workspace") ? argv[2] : last;
        *desired = 0;
        if (p && managed_path(p) && exists(p))
            strcpy(desired, p);
        free(last);
        load_projects();
        if (*desired)
        {
            char *sub;
            int i;
            strcpy(project, desired);
            sub = strstr(project, "/Tasks/");
            if (sub)
                *sub = 0;
            for (i = 0; i < project_count; i++)
                if (!strcmp(project_paths[i], project))
                    SetAttrs(projects, MUIA_NoNotify, TRUE, MUIA_List_Active, i, TAG_DONE);
            load_tasks();
            for (i = 0; i < task_count; i++)
                if (!strcmp(task_paths[i], desired))
                    SetAttrs(tasks, MUIA_NoNotify, TRUE, MUIA_List_Active, i, TAG_DONE);
            load_workspace(desired);
        }
    }
    profiles_picker_refresh();
    timer_port = CreateMsgPort();
    if (timer_port)
        timer_io = (struct timerequest *)CreateIORequest(timer_port, sizeof(*timer_io));
    if (!timer_io || OpenDevice((STRPTR)TIMERNAME, UNIT_VBLANK, (struct IORequest *)timer_io, 0))
    {
        message("Cannot open timer.device.");
        goto cleanup;
    }
    timer_open = 1;
    if (any_jobs())
        arm_timer();
    if (storage_failed)
        message(
            "Project volume could not mount. Using Termic:Projects; check TermicStorage in Shell.");
    set(win, MUIA_Window_Open, TRUE);
    if (!value(win, MUIA_Window_Open))
        goto cleanup;
    activity_scroll_bottom(transcript);
    flow_startup();
    while (!done)
    {
        update_menus();
        signals = 0;
        while (!done && (id = DoMethod(app, MUIM_Application_NewInput, &signals)) != 0)
        {
            int n;
            char *name;
            if(id>=AC_OPEN && id<=AC_CANCEL) {access_event(id);continue;}
            if(id>=PF_OPEN && id<=PF_ADDCURSOR) {profile_event(id);continue;}
            if (shortcut_event(id)) continue;
            if(id>=PL_OPEN && id<=PL_INSERT) {prompts_event(id);continue;}
            if(id==ED_DIAGNOSTIC) {
                int row=value(diagnostic_list,MUIA_List_Active);
                if(row>=0 && row<diagnostic_count && save_current()) {
                    open_file(diagnostic_rows[row].path);set(pages,MUIA_Group_ActivePage,1);
                    set(win,MUIA_Window_ActiveObject,source);
                    SetAttrs(source,MUIA_TextEditor_CursorY,diagnostic_rows[row].line-1,MUIA_TextEditor_CursorX,0,TAG_DONE);
                }
                continue;
            }
            if(id==ED_CHANGED) {
                set(source,MUIA_TextEditor_ContentsChanged,FALSE);
                colour_pending=1;colour_delay=1;arm_timer();continue;
            }
            if(id>=GT_OPEN && id<=GT_PR) {git_event(id);continue;}
            if(id>=SP_OPEN && id<=SP_REFRESH) {pad_event(id);continue;}
            if(id>=CR_OPEN && id<=CR_REFRESH) {change_event(id);continue;}
            if(id>=KB_OPEN && id<=Q_LAST) {flow_event(id);continue;}
            switch (id)
            {
            case WINDOW_DEPTH:
            {
                struct Window *w = (struct Window *)value(win, MUIA_Window_Window);
                if (w) WindowToBack(w);
                break;
            }
            case WINDOW_ZOOM:
            {
                static LONG left, top, width, height;
                static int zoomed;
                struct Window *w = (struct Window *)value(win, MUIA_Window_Window);
                if (w)
                {
                    if (!zoomed)
                    {
                        left = w->LeftEdge; top = w->TopEdge;
                        width = w->Width; height = w->Height;
                        ChangeWindowBox(w, 0, w->WScreen->BarHeight + 1,
                            w->WScreen->Width, w->WScreen->Height - w->WScreen->BarHeight - 1);
                    }
                    else ChangeWindowBox(w, left, top, width, height);
                    zoomed = !zoomed;
                }
                break;
            }
            case UPDATE_MENUS:
                if (transcript_pending && !value(transcript, MUIA_TextEditor_AreaMarked))
                    refresh_transcript();
                update_menus();
                break;
            case HELP:
                open_help();
                break;
            case MUIPREFS:
                DoMethod(app, MUIM_Application_OpenConfigWindow, 0);
                break;
            case ABOUT:
                open_about();
                break;
            case ABOUTCLOSE:
                set(about_window, MUIA_Window_Open, FALSE);
                set(win, MUIA_Window_Activate, TRUE);
                break;
            case FOCUSFILE:
                set(win, MUIA_Window_ActiveObject, newname);
                break;
            case FOCUSEDITOR:
                set(pages, MUIA_Group_ActivePage, 1);
                set(win, MUIA_Window_ActiveObject, source);
                break;
            case FOCUSCOMMIT:
                set(pages, MUIA_Group_ActivePage, 3);
                set(win, MUIA_Window_ActiveObject, commit);
                break;
            case FOCUSCOMMAND:
                set(win, MUIA_Window_ActiveObject, command);
                break;
            case PAGE_AGENT:
            case PAGE_EDITOR:
            case PAGE_OUTPUT:
            case PAGE_CHANGES:
            case PAGE_SETTINGS:
                set(win, MUIA_Window_ActiveObject, NULL);
                set(pages, MUIA_Group_ActivePage, id - PAGE_AGENT);
                if (id == PAGE_AGENT)
                    set(win, MUIA_Window_ActiveObject, prompt);
                if (id == PAGE_EDITOR)
                    set(win, MUIA_Window_ActiveObject, source);
                if (id == PAGE_SETTINGS)
                    set(win, MUIA_Window_ActiveObject, model);
                break;
            case QUIT:
            case MUIV_Application_ReturnID_Quit:
                if (save_current() && pad_save())
                {
                    done = 1;
                }
                break;
            case PROJECT:
                if (!loading && !save_current())
                {
                    int i;
                    for (i = 0; i < project_count; i++)
                        if (!strcmp(project_paths[i], project))
                            SetAttrs(projects, MUIA_NoNotify, TRUE, MUIA_List_Active, i, TAG_DONE);
                    break;
                }
                if (!loading)
                {
                    n = value(projects, MUIA_List_Active);
                    if (n >= 0 && n < project_count)
                    {
                        strcpy(project, project_paths[n]);
                        join(path, project, "name.txt");
                        name = read_value(path, 120);
                        set(projectname, MUIA_String_Contents, name ? name : "Starter project");
                        free(name);
                        load_tasks();
                    }
                }
                break;
            case TASK:
                if (!loading && !save_current())
                {
                    int i;
                    for (i = 0; i < task_count; i++)
                        if (!strcmp(task_paths[i], current))
                            SetAttrs(tasks, MUIA_NoNotify, TRUE, MUIA_List_Active, i, TAG_DONE);
                    break;
                }
                if (!loading)
                {
                    n = value(tasks, MUIA_List_Active);
                    if (n >= 0 && n < task_count)
                        load_workspace(task_paths[n]);
                }
                break;
            case FILESEL:
                if (loading)
                    break;
                n = value(files, MUIA_List_Active);
                if (n >= 0 && n < file_count)
                {
                    open_file(file_names[n]);
                    set(pages, MUIA_Group_ActivePage, 1);
                    set(win, MUIA_Window_ActiveObject, source);
                }
                break;
            case IMPORTPROJECT:
                if (save_current() && AslBase)
                {
                    struct FileRequester *r = AllocAslRequestTags(
                        ASL_FileRequest, ASLFR_TitleText, "Import source drawer", ASLFR_DrawersOnly,
                        TRUE, TAG_DONE);
                    if (r && AslRequestTags(r, TAG_DONE))
                    {
                        char dest[PATH_SIZE], np[PATH_SIZE];
                        int i;
                        for (i = 1; i < 10000; i++)
                        {
                            snprintf(dest, sizeof(dest), "%s/import-%04d", project_root, i);
                            if (!exists(dest))
                                break;
                        }
                        if (i < 10000 && mkdir_safe(dest) && clone_sources(r->fr_Drawer, dest, 0))
                        {
                            join(np, dest, "name.txt");
                            save_text(np, "Imported project", 120);
                            load_projects();
                            strcpy(project, dest);
                            load_tasks();
                            message("Source drawer imported into an independent project.");
                        }
                        else
                            message("Import failed. Check the selected drawer and disk space.");
                    }
                    if (r)
                        FreeAslRequest(r);
                }
                break;
            case NEWPROJECT:
                create_workspace(0);
                break;
            case NEWTASK:
                create_workspace(1);
                break;
            case SAVE:
                if (save_current())
                    message("Saved. Previous contents are retained in .bak files.");
                break;
            case SAVETASK:
                if (save_editor(prompt, "task.txt", 8192))
                    message("Task instructions saved.");
                break;
            case NEWFILE:
                name = (char *)value(newname, MUIA_String_Contents);
                if (!source_name(name) || strchr(name, '/'))
                {
                    message("Use .c, .h, .py, .rexx, .rx, .txt or .md filenames.");
                    break;
                }
                join(path, current, name);
                if (busy(current) || exists(path))
                {
                    message("Workspace busy or file already exists.");
                    break;
                }
                if (save_text(path, "", SOURCE_LIMIT))
                {
                    refresh_files();
                    open_file(name);
                    set(pages, MUIA_Group_ActivePage, 1);
                    set(win, MUIA_Window_ActiveObject, source);
                    set(newname, MUIA_String_Contents, "");
                }
                break;
            case BUILD:
                launch("--build", 2);
                break;
            case RUN:
                launch("--run", 2);
                break;
            case AGENT:
                if (prompt_command()) break;
                if (save_editor(prompt, "task.txt", 8192) && profile_prepare_launch() &&
                    save_editor(prompt, "last-prompt.txt", 8192))
                    launch("--agent", 0);
                break;
            case CANCEL:
                cancel();
                break;
            case FIND:
            case FINDNEXT:
                if (!DoMethod(source, MUIM_TextEditor_Search,
                              (STRPTR)value(findtext, MUIA_String_Contents),
                              id == FIND ? MUIF_TextEditor_Search_FromTop
                                         : MUIF_TextEditor_Search_Next))
                    message("Text not found.");
                break;
            case NEWSESSION:
                join(path, current, "session.txt");
                if (!busy(current))
                {
                    int n=value(profile_picker,MUIA_Cycle_Active);
                    char sessionfile[64];
                    if(n<0 || n>=picker_count) { message("Select an agent profile first.");break; }
                    profile_session(&profile_live.items[picker_row[n]],sessionfile);
                    join(path,current,sessionfile);
                    DeleteFile((STRPTR)path);
                    join(path,current,"cli-started");DeleteFile((STRPTR)path);
                    message("Next agent request starts a new conversation. Transcript retained.");
                }
                break;
            case SAVEBUILD:
                join(path, current, "build-command.txt");
                if (!busy(current) &&
                    save_text(path, (char *)value(buildcommand, MUIA_String_Contents), 512))
                    message("Build command saved for this workspace.");
                break;
            case REFRESH:
                refresh_files();
                break;
            case SETTINGS:
                set(pages, MUIA_Group_ActivePage, 4);
                set(win, MUIA_Window_ActiveObject, model);
                break;
            case SAVECONFIG:
            case USECONFIG:
                configure(id == SAVECONFIG);
                break;
            case CANCELCONFIG:
                restore_preferences();
                set(pages, MUIA_Group_ActivePage, 0);
                set(win, MUIA_Window_ActiveObject, prompt);
                message(
                    "Preference edits discarded. Saved workspace build commands are unchanged.");
                break;
            case PROBE:
                launch("--probe", 2);
                break;
            case SHELL:
                name = (char *)value(command, MUIA_String_Contents);
                join(path, current, "command.txt");
                if (!busy(current) && save_text(path, name, 1024))
                    launch(value(command_mode,MUIA_Cycle_Active)==1 ? "--rexx" :
                           value(command_mode,MUIA_Cycle_Active)==2 ? "--python" : "--shell", -1);
                break;
            case GITSTATUS:
                launch("--status", 3);
                break;
            case DIFF:
                launch("--diff", 3);
                break;
            case GITINIT:
                launch("--init", 3);
                break;
            case GITCOMMIT:
                name = (char *)value(commit, MUIA_String_Contents);
                join(path, current, "commit.txt");
                if (*name && !busy(current) && save_text(path, name, 256))
                    launch("--commit", 3);
                else
                    message("Enter a commit message and wait for running jobs.");
                break;
            case UNDO:
            case REDO:
            case CUT:
            case COPY:
            case PASTE:
            case SELECTALL:
            case ERASE:
            {
                Object *target = edit_target();
                const char *cmd = id == UNDO        ? "UNDO"
                                  : id == REDO      ? "REDO"
                                  : id == CUT       ? "CUT"
                                  : id == COPY      ? "COPY"
                                  : id == SELECTALL ? "SELECTALL"
                                  : id == ERASE     ? "DELETE"
                                                    : "PASTE";
                if (target)
                {
                    DoMethod(target, MUIM_TextEditor_ARexxCmd, cmd);
                    /* Undo may return to the imported buffer and clear the
                     * gadget's dirty flag even after a newer version was saved. */
                    if ((id == UNDO || id == REDO) &&
                        !value(target, MUIA_TextEditor_ReadOnly))
                        set(target, MUIA_TextEditor_HasChanged, TRUE);
                }
                break;
            }
            case RENAMEPROJECT:
            case RENAMETASK:
                name = (char *)value(id == RENAMEPROJECT ? projectname : taskname,
                                     MUIA_String_Contents);
                join(path, id == RENAMEPROJECT ? project : current, "name.txt");
                if (!busy(current) && *name && save_text(path, name, 120))
                {
                    task_row(current);
                    if (id == RENAMEPROJECT)
                    {
                        int i = value(projects, MUIA_List_Active);
                        DoMethod(projects, MUIM_KillNotify, MUIA_List_Active);
                        DoMethod(projects, MUIM_List_Remove, i);
                        DoMethod(projects, MUIM_List_InsertSingle, name, i);
                        set(projects, MUIA_List_Active, i);
                        notify(projects, MUIA_List_Active, MUIV_EveryTime, PROJECT);
                    }
                    message("Name saved.");
                }
                break;
            case MARKDONE:
                join(path, current, "state.txt");
                if (!busy(current) && save_text(path, "Done", 32))
                {
                    task_row(current);
                    message("Task marked done. Files and history are retained.");
                }
                break;
            }
            if(id==NEWTASK || id==NEWPROJECT || id==MARKDONE || id==RENAMETASK ||
               id==RENAMEPROJECT || id==IMPORTPROJECT)flow_changed();
            if (done)
                break;
        }
        if (done)
            break;
        if (signals)
        {
            received = Wait(signals | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | (1UL << timer_port->mp_SigBit));
            if (received & SIGBREAKF_CTRL_D) {refresh_mcp_tasks();flow_changed();flow_tick();}
            if (received & SIGBREAKF_CTRL_C)
            {
                if (save_current() && pad_save())
                    done = 1;
            }
            if (timer_pending && CheckIO((struct IORequest *)timer_io))
            {
                WaitIO((struct IORequest *)timer_io);
                timer_pending = 0;
                poll_jobs();
            }
        }
    }
    result = 0;
cleanup:
    if(control_registered)RemPort(&control_port);
    if (timer_pending)
    {
        AbortIO((struct IORequest *)timer_io);
        WaitIO((struct IORequest *)timer_io);
    }
    if (timer_open)
        CloseDevice((struct IORequest *)timer_io);
    if (timer_io)
        DeleteIORequest((struct IORequest *)timer_io);
    if (timer_port)
        DeleteMsgPort(timer_port);
    activity_clear(&activity);
    if (app)
        MUI_DisposeObject(app);
    cJSON_Delete(prompts_items);
    cJSON_Delete(prompts_defaults);
    if(keyeditor_class)__termic_mui_unclass(MUIMasterBase,keyeditor_class);
    if(pane_class)__termic_mui_unclass(MUIMasterBase,pane_class);
    if(action_icon_class)__termic_mui_unclass(MUIMasterBase,action_icon_class);
    chrome_cleanup();
    if (codefont)
        CloseFont(codefont);
    if (bodyfont)
        CloseFont(bodyfont);
    if (DiskfontBase)
        CloseLibrary(DiskfontBase);
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (AslBase)
        CloseLibrary(AslBase);
    if (MUIMasterBase)
        CloseLibrary(MUIMasterBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    return result;
}
