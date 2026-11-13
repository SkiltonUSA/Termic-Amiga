/* Termic's Amiga MCP/CLI adapter. Stdout is exclusively JSON-RPC. */
#include "native.h"
#include "profiles.h"
#include "workspace.h"
#include "version.h"
#include "vendor/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__AMIGA__) || defined(__VBCC__)
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif
#define MCP_LIMIT 65536
const char version_tag[] = TERMIC_VERSION_TAG("TermicMCP");
const char stack_size[] = "$STACK:262144";
static char root[384];
struct Tool
{
    const char *name, *description, *args;
    int readonly;
};
static const struct Tool tools[] = {
    {"project_list", "List Amiga source projects.", "", 1},
    {"task_list", "List a project's independent source-copy tasks.", "project", 1},
    {"task_new",
     "Create an isolated task by copying project C/H sources. Does not start an agent.",
     "project name agent?", 0},
    {"task_status", "Read native worker state and exit result.", "project task", 1},
    {"task_send", "Start the task's direct API profile with a prompt. Refuses busy tasks.",
     "project task prompt", 0},
    {"task_stop", "Request cancellation of the native task worker.", "project task", 0},
    {"task_result", "Read the task's saved agent transcript.", "project task", 1},
    {"task_log", "Read native build/worker output.", "project task", 1},
    {"task_build", "Build an MCP-created task with its native build command.", "project task",
     0},
    {"file_read", "Read a C/H source in a task. Path is relative to the task.",
     "project task path", 1},
    {"file_write", "Save C/H source in an idle MCP-created task, retaining a backup.",
     "project task path content", 0},
    {"scratchpad_write", "Write a task note in its .scratchpads drawer, outside source files.",
     "project task pad content", 0},
    {"scratchpad_read", "Read a saved task scratchpad.", "project task pad", 1},
    {"scratchpad_list", "List saved task scratchpads.", "project task", 1},
    {"agent_list", "List configured profiles without credentials or environment values.", "",
     1}};
static const char *str(cJSON *o, const char *name)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, name);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}
static int segment(const char *s)
{
    unsigned n = 0;
    if (!s || !*s)
        return 0;
    while (*s)
    {
        if (!isalnum((unsigned char)*s) && *s != '-' && *s != '_')
            return 0;
        s++;
        n++;
    }
    return n < 80;
}
static int pathjoin(char *out, unsigned size, const char *base, const char *name)
{
    int n = snprintf(out, size, "%s%s%s", base,
                     *base && base[strlen(base) - 1] != ':' ? "/" : "", name);
    return n >= 0 && (unsigned)n < size;
}
/* Only real directories/files, never links. Every traversed component is checked. */
static int kind(const char *path)
{
#if defined(__AMIGA__) || defined(__VBCC__)
    struct FileInfoBlock *f = AllocDosObject(DOS_FIB, NULL);
    BPTR l = Lock((STRPTR)path, ACCESS_READ);
    int result = 0;
    if (f && l && Examine(l, f))
        result = f->fib_DirEntryType == ST_USERDIR ? 2 : f->fib_DirEntryType == ST_FILE ? 1 : 0;
    if (l)
        UnLock(l);
    if (f)
        FreeDosObject(DOS_FIB, f);
    return result;
#else
    struct stat st;
    if (lstat(path, &st))
        return 0;
    return S_ISDIR(st.st_mode) ? 2 : S_ISREG(st.st_mode) ? 1 : 0;
#endif
}
static int within(const char *base, const char *path)
{
#if defined(__AMIGA__) || defined(__VBCC__)
    char a[512], b[512];
    BPTR rootlock = Lock((STRPTR)base, ACCESS_READ), target = Lock((STRPTR)path, ACCESS_READ);
    int ok = 0;
    unsigned i, n;
    if (rootlock && target && NameFromLock(rootlock, (STRPTR)a, sizeof(a)) &&
        NameFromLock(target, (STRPTR)b, sizeof(b)))
    {
        n = strlen(a);
        if (strlen(b) == n)
        {
            ok = 1;
            for (i = 0; i < n; i++)
                if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
                    ok = 0;
            if (ok)
            {
                UnLock(rootlock);
                UnLock(target);
                return 1;
            }
        }
        if (n && a[n - 1] != ':')
            a[n++] = '/';
        a[n] = 0;
        if (strlen(b) >= n)
        {
            ok = 1;
            for (i = 0; i < n; i++)
                if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
                    ok = 0;
        }
    }
    if (rootlock)
        UnLock(rootlock);
    if (target)
        UnLock(target);
    return ok;
#else
    char a[1024], b[1024];
    size_t n;
    if (!realpath(base, a) || !realpath(path, b))
        return 0;
    n = strlen(a);
    return !strncmp(a, b, n) && (b[n] == '/' || b[n] == 0);
#endif
}
static int inside_root(const char *path) { return within(root, path); }
static int make_dir(const char *path)
{
#if defined(__AMIGA__) || defined(__VBCC__)
    BPTR l = CreateDir((STRPTR)path);
    if (!l)
        return 0;
    UnLock(l);
    return 1;
#else
    return mkdir(path, 0700) == 0;
#endif
}
static cJSON *listing(const char *dir, int dirs)
{
    cJSON *a = cJSON_CreateArray();
#if defined(__AMIGA__) || defined(__VBCC__)
    struct FileInfoBlock *f = AllocDosObject(DOS_FIB, NULL);
    BPTR l = Lock((STRPTR)dir, ACCESS_READ);
    if (l && f && Examine(l, f))
        while (ExNext(l, f))
        {
            if ((dirs ? f->fib_DirEntryType == ST_USERDIR : f->fib_DirEntryType == ST_FILE) &&
                f->fib_FileName[0] != '.')
                cJSON_AddItemToArray(a, cJSON_CreateString(f->fib_FileName));
        }
    if (l)
        UnLock(l);
    if (f)
        FreeDosObject(DOS_FIB, f);
#else
    DIR *d = opendir(dir);
    struct dirent *e;
    char path[512];
    if (d)
    {
        while ((e = readdir(d)) != NULL)
            if (e->d_name[0] != '.' && pathjoin(path, sizeof(path), dir, e->d_name) &&
                kind(path) == (dirs ? 2 : 1))
                cJSON_AddItemToArray(a, cJSON_CreateString(e->d_name));
        closedir(d);
    }
#endif
    return a;
}
static int project_path(const char *project, char *out)
{
    return segment(project) && kind(root) == 2 && pathjoin(out, 512, root, project) &&
           kind(out) == 2 && inside_root(out);
}
static int task_path(cJSON *args, char *out)
{
    char project[512], tasks[512];
    const char *t = str(args, "task");
    return project_path(str(args, "project"), project) &&
           pathjoin(tasks, sizeof(tasks), project, "Tasks") && kind(tasks) == 2 &&
           inside_root(tasks) && segment(t) && pathjoin(out, 512, tasks, t) && kind(out) == 2 &&
           inside_root(out);
}
static int source_path(const char *base, const char *name, char *out)
{
    const char *ext;
    char part[184], walk[512], *s, *next;
    if (!name || strlen(name) >= sizeof(part) || !source_name(name))
        return 0;
    ext = strrchr(name, '.');
    if (!ext || (strcmp(ext, ".c") && strcmp(ext, ".h")))
        return 0;
    strcpy(part, name);
    strcpy(walk, base);
    s = part;
    while ((next = strchr(s, '/')) != NULL)
    {
        *next = 0;
        if (!segment(s) || !pathjoin(out, 512, walk, s) || kind(out) != 2)
            return 0;
        strcpy(walk, out);
        s = next + 1;
    }
    if (!pathjoin(out, 512, walk, s))
        return 0;
    if (kind(out) == 1)
        return within(base, out);
    if (!within(base, walk))
        return 0;
    /* Existing non-regular objects must not be treated as a missing file. */
#if defined(__AMIGA__) || defined(__VBCC__)
    {
        BPTR l = Lock((STRPTR)out, ACCESS_READ);
        if (l)
        {
            UnLock(l);
            return 0;
        }
        return IoErr() == ERROR_OBJECT_NOT_FOUND;
    }
#else
    {
        struct stat st;
        return lstat(out, &st) != 0;
    }
#endif
}
static int mcp_save(const char *path, const char *text, unsigned limit)
{
    char temp[520], parent[512], *slash;
    const char *suffixes[] = {"", ".new", ".bak"};
    int i;
    if (strlen(path) > 500)
        return 0;
    strcpy(parent, path);
    slash = strrchr(parent, '/');
    if (!slash)
        return 0;
    *slash = 0;
    for (i = 0; i < 3; i++)
    {
        sprintf(temp, "%s%s", path, suffixes[i]);
#if defined(__AMIGA__) || defined(__VBCC__)
        {
            BPTR l = Lock((STRPTR)temp, ACCESS_READ);
            if (l)
            {
                UnLock(l);
                if (kind(temp) != 1 || !within(parent, temp))
                    return 0;
            }
        }
#else
        {
            struct stat st;
            if (!lstat(temp, &st) && (!S_ISREG(st.st_mode) || !within(parent, temp)))
                return 0;
        }
#endif
    }
    return save_text(path, text, limit);
}
static int copy_sources(const char *from, const char *to, int depth)
{
    cJSON *files = listing(from, 0), *dirs = listing(from, 1), *v;
    int ok = 1;
    char a[512], b[512], *text;
    if (depth > 6)
    {
        cJSON_Delete(files);
        cJSON_Delete(dirs);
        return 0;
    }
    cJSON_ArrayForEach(v, files) if (source_path(from, v->valuestring, a))
    {
        pathjoin(b, sizeof(b), to, v->valuestring);
        text = read_text(a, SOURCE_LIMIT);
        if (!text || !mcp_save(b, text, SOURCE_LIMIT))
            ok = 0;
        free(text);
    }
    cJSON_ArrayForEach(v, dirs) if (segment(v->valuestring) && strcmp(v->valuestring, "Tasks"))
    {
        pathjoin(a, sizeof(a), from, v->valuestring);
        pathjoin(b, sizeof(b), to, v->valuestring);
        if (!make_dir(b) || !copy_sources(a, b, depth + 1))
            ok = 0;
    }
    cJSON_Delete(files);
    cJSON_Delete(dirs);
    return ok;
}
static int busy_task(const char *task)
{
    char p[512];
    pathjoin(p, sizeof(p), task, ".busy");
    if (kind(p) != 0)
        return 1;
    pathjoin(p, sizeof(p), task, ".launch");
    return kind(p) != 0;
}
static int mutable_task(const char *task)
{
    char p[512], *open;
    int ok;
    pathjoin(p, sizeof(p), task, ".mcp-owned");
    if (kind(p) != 1 || busy_task(task))
        return 0;
    open = read_text(TERMIC_CONFIG "last-workspace", 384);
    if (open)
        open[strcspn(open, "\r\n")] = 0;
    /* Assigns and absolute device paths can name the same drawer. */
    ok = !open || (strcmp(open, task) && !(within(open, task) && within(task, open)));
    free(open);
    return ok;
}
static int launch_task(const char *task, const char *action)
{
#if defined(__AMIGA__) || defined(__VBCC__)
    char line[700], log[512], result[512];
    BPTR in, out;
    pathjoin(log, sizeof(log), task, "job.log");
    pathjoin(result, sizeof(result), task, ".result");
    in = Open((STRPTR) "NIL:", MODE_OLDFILE);
    out = Open((STRPTR)log, MODE_NEWFILE);
    if (!in || !out)
    {
        if (in)
            Close(in);
        if (out)
            Close(out);
        return 0;
    }
    DeleteFile((STRPTR)result);
    pathjoin(result, sizeof(result), task, ".launch");
    if (!mcp_save(result, "1", 1))
    {
        Close(in);
        Close(out);
        return 0;
    }
    sprintf(line, "Stack 262144\nTermic:TermicAgent %s --project \"%s\"", action, task);
    if (SystemTags((STRPTR)line, SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE, TAG_DONE) ==
        -1)
    {
        Close(in);
        Close(out);
        DeleteFile((STRPTR)result);
        return 0;
    }
    return 1;
#else
    (void)task;
    (void)action;
    return 0; /* Portable tests never launch commands. */
#endif
}
static cJSON *content(const char *text, int error)
{
    cJSON *r = cJSON_CreateObject(), *a = cJSON_AddArrayToObject(r, "content"),
          *v = cJSON_CreateObject();
    cJSON_AddStringToObject(v, "type", "text");
    cJSON_AddStringToObject(v, "text", text ? text : "");
    cJSON_AddItemToArray(a, v);
    cJSON_AddBoolToObject(r, "isError", error);
    return r;
}
static cJSON *data_result(cJSON *data)
{
    char *s = cJSON_PrintUnformatted(data);
    cJSON *r = content(s, 0);
    free(s);
    cJSON_Delete(data);
    return r;
}
static cJSON *run_tool(const struct Tool *tool, cJSON *args)
{
    const char *name = tool->name, *s;
    char task[512], path[512], base[512], *text = NULL;
    cJSON *result;
    s = read_preference("mcp-enabled", 8);
    if (!s || strcmp(s, "1"))
    {
        free((void *)s);
        return content("Amiga MCP is disabled in Agent access.", 1);
    }
    free((void *)s);
    if (!strcmp(name, "agent_list"))
    {
        struct AgentProfiles *p = calloc(1, sizeof(*p));
        int i;
        cJSON *list = cJSON_CreateArray();
        if (!p || !profiles_load(p))
        {
            free(p);
            cJSON_Delete(list);
            return content("Invalid agent registry.", 1);
        }
        for (i = 0; i < p->count; i++)
        {
            cJSON *v = cJSON_CreateObject();
            cJSON_AddStringToObject(v, "id", p->items[i].id);
            cJSON_AddStringToObject(v, "name", p->items[i].name);
            cJSON_AddNumberToObject(v, "kind", p->items[i].kind);
            cJSON_AddItemToArray(list, v);
        }
        free(p);
        return data_result(list);
    }
    if (!strcmp(name, "project_list"))
        return data_result(listing(root, 1));
    if (!strcmp(name, "task_list"))
    {
        if (!project_path(str(args, "project"), base) ||
            !pathjoin(path, sizeof(path), base, "Tasks"))
            return content("Unknown project.", 1);
        return data_result(listing(path, 1));
    }
    if (!strcmp(name, "task_new"))
    {
        const char *id = str(args, "name"), *agent = str(args, "agent");
        struct AgentProfiles *p = calloc(1, sizeof(*p));
        int i;
        if (!agent)
            agent = "openai";
        if (!p || !profiles_load(p) || (i = profile_find(p, agent)) < 0 ||
            p->items[i].kind != 0)
        {
            free(p);
            return content("Choose a direct API agent profile.", 1);
        }
        free(p);
        if (!segment(id) || !project_path(str(args, "project"), base))
            return content(
                "Use an existing project and a task name containing letters, digits, - or _.",
                1);
        pathjoin(path, sizeof(path), base, "Tasks");
        if (kind(path) != 2 && !make_dir(path))
            return content("Cannot create Tasks drawer.", 1);
        if (!inside_root(path))
            return content("Tasks drawer is outside project storage.", 1);
        if (!pathjoin(task, sizeof(task), path, id) || !make_dir(task))
            return content("Task already exists or cannot be created.", 1);
        if (!copy_sources(base, task, 0))
            return content(
                "Source copy incomplete. Task left for inspection; agent not started.", 1);
        pathjoin(path, sizeof(path), base, "build-command.txt");
        text = kind(path) == 1 && within(base, path) ? read_text(path, 512) : NULL;
        if (text)
        {
            pathjoin(path, sizeof(path), task, "build-command.txt");
            if (!mcp_save(path, text, 512))
            {
                free(text);
                return content("Cannot copy build command.", 1);
            }
            free(text);
            text = NULL;
        }
        pathjoin(path, sizeof(path), task, "name.txt");
        if (!mcp_save(path, id, 79))
            return content("Cannot save task name.", 1);
        pathjoin(path, sizeof(path), task, "agent-profile");
        if (!mcp_save(path, agent, 31))
            return content("Cannot save profile.", 1);
        pathjoin(path, sizeof(path), task, ".gitignore");
        if (!mcp_save(path, TERMIC_WORKSPACE_IGNORE, 2048))
            return content("Cannot save task ignore rules.", 1);
        pathjoin(path, sizeof(path), task, ".mcp-owned");
        if (!mcp_save(path, "1", 1))
            return content("Cannot save task ownership.", 1);
        return content(id, 0);
    }
    if (!task_path(args, task))
        return content("Unknown project/task.", 1);
    if (!tool->readonly && strcmp(name, "task_stop") && !mutable_task(task))
        return content(
            "Writes require an idle MCP-created task that is not selected in the editor.", 1);
    if (!strcmp(name, "task_status"))
    {
        result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "busy", busy_task(task));
        pathjoin(path, sizeof(path), task, ".result");
        text = kind(path) == 1 && within(task, path) ? read_text(path, 32) : NULL;
        cJSON_AddStringToObject(result, "exit", text ? text : "");
        free(text);
        return data_result(result);
    }
    if (!strcmp(name, "task_stop"))
    {
        pathjoin(path, sizeof(path), task, ".mcp-owned");
        if (kind(path) != 1)
            return content("Only MCP-created task workers can be cancelled here.", 1);
#if defined(__AMIGA__) || defined(__VBCC__)
        unsigned long hash = 5381;
        struct MsgPort *port;
        const char *c;
        char portname[40];
        for (c = task; *c; c++)
            hash = hash * 33 + (unsigned char)*c;
        sprintf(portname, "TermicJob.%08lx", hash);
        Forbid();
        port = FindPort((STRPTR)portname);
        if (port)
            Signal(port->mp_SigTask, SIGBREAKF_CTRL_C);
        Permit();
        return content(port ? "Cancellation requested." : "No worker registered.", !port);
#else
        return content("No native worker on this platform.", 1);
#endif
    }
    if (!strcmp(name, "task_send"))
    {
        struct AgentProfiles *p = calloc(1, sizeof(*p));
        char *id;
        int n;
        pathjoin(path, sizeof(path), task, "agent-profile");
        id = read_text(path, 31);
        if (!p || !id || !profiles_load(p) || (n = profile_find(p, id)) < 0 ||
            p->items[n].kind != 0)
        {
            free(p);
            free(id);
            return content("Task requires a direct API profile.", 1);
        }
        free(p);
        free(id);
        s = str(args, "prompt");
        pathjoin(path, sizeof(path), task, "task.txt");
        if (!mcp_save(path, s, 8192))
            return content("Cannot save prompt (limit 8192 bytes).", 1);
        {
            int ok = launch_task(task, "--agent");
            return content(ok ? "Agent started. Read task_status and task_result for progress."
                              : "Could not launch worker.",
                           !ok);
        }
    }
    if (!strcmp(name, "task_build"))
    {
        int ok = launch_task(task, "--build");
        return content(ok ? "Build started." : "Could not launch worker.", !ok);
    }
    if (!strcmp(name, "file_read") || !strcmp(name, "file_write"))
    {
        if (!source_path(task, str(args, "path"), path))
            return content("Only relative C/H files inside this task are accessible.", 1);
        if (!strcmp(name, "file_write"))
        {
            int ok = mcp_save(path, str(args, "content"), SOURCE_LIMIT);
            return content(ok ? "Source saved with backup." : "Source save failed.", !ok);
        }
    }
    else if (!strncmp(name, "scratchpad_", 11))
    {
        pathjoin(base, sizeof(base), task, ".scratchpads");
        if (!strcmp(name, "scratchpad_list"))
        {
            if (kind(base) != 2 || !within(task, base))
                return content("Scratchpad drawer unavailable.", 1);
            return data_result(listing(base, 0));
        }
        if (!segment(str(args, "pad")))
            return content("Invalid scratchpad id.", 1);
        if (kind(base) != 2 && (!strcmp(name, "scratchpad_read") || !make_dir(base)))
            return content("Scratchpad drawer unavailable.", 1);
        if (!within(task, base))
            return content("Scratchpad drawer outside project storage.", 1);
        pathjoin(path, sizeof(path), base, str(args, "pad"));
        if (kind(path) && (!within(base, path) || kind(path) != 1))
            return content("Invalid scratchpad file.", 1);
        if (!strcmp(name, "scratchpad_write"))
        {
            int ok = mcp_save(path, str(args, "content"), SOURCE_LIMIT);
            return content(ok ? "Scratchpad saved." : "Scratchpad save failed.", !ok);
        }
    }
    else if (!strcmp(name, "task_result"))
        pathjoin(path, sizeof(path), task, "transcript.txt");
    else if (!strcmp(name, "task_log"))
        pathjoin(path, sizeof(path), task, "job.log");
    else
        return content("Unsupported routine.", 1);
    if (!within(task, path) || kind(path) != 1)
        return content("File missing or outside project storage.", 1);
    text = read_text(path, SOURCE_LIMIT);
    result = content(text ? text : "File missing or exceeds 32 KiB.", !text);
    free(text);
    return result;
}
static cJSON *rpc_error(cJSON *id, int code, const char *message)
{
    cJSON *r = cJSON_CreateObject(), *e;
    cJSON_AddStringToObject(r, "jsonrpc", "2.0");
    cJSON_AddItemToObject(r, "id", id ? cJSON_Duplicate(id, 1) : cJSON_CreateNull());
    e = cJSON_AddObjectToObject(r, "error");
    cJSON_AddNumberToObject(e, "code", code);
    cJSON_AddStringToObject(e, "message", message);
    return r;
}
static cJSON *tool_schema(const struct Tool *t)
{
    cJSON *v = cJSON_CreateObject(), *schema, *props, *required, *annotations;
    char fields[128], *s, *next;
    cJSON_AddStringToObject(v, "name", t->name);
    cJSON_AddStringToObject(v, "description", t->description);
    schema = cJSON_AddObjectToObject(v, "inputSchema");
    cJSON_AddStringToObject(schema, "type", "object");
    props = cJSON_AddObjectToObject(schema, "properties");
    required = cJSON_AddArrayToObject(schema, "required");
    cJSON_AddBoolToObject(schema, "additionalProperties", 0);
    strcpy(fields, t->args);
    s = fields;
    while (*s)
    {
        int optional;
        next = strchr(s, ' ');
        if (next)
            *next = 0;
        optional = s[strlen(s) - 1] == '?';
        if (optional)
            s[strlen(s) - 1] = 0;
        cJSON_AddStringToObject(cJSON_AddObjectToObject(props, s), "type", "string");
        if (!optional)
            cJSON_AddItemToArray(required, cJSON_CreateString(s));
        if (!next)
            break;
        s = next + 1;
    }
    annotations = cJSON_AddObjectToObject(v, "annotations");
    cJSON_AddBoolToObject(annotations, "readOnlyHint", t->readonly);
    cJSON_AddBoolToObject(annotations, "openWorldHint", !strcmp(t->name, "task_send"));
    return v;
}
static int valid_args(const struct Tool *t, cJSON *a)
{
    if (!a)
        return !*t->args;
    cJSON *schema = tool_schema(t),
          *props = cJSON_GetObjectItemCaseSensitive(schema, "inputSchema"), *required, *v;
    int ok = 1;
    required = cJSON_GetObjectItemCaseSensitive(props, "required");
    props = cJSON_GetObjectItemCaseSensitive(props, "properties");
    if (!cJSON_IsObject(a))
        ok = 0;
    cJSON_ArrayForEach(v, a) if (!cJSON_IsString(v) ||
                                 !cJSON_GetObjectItemCaseSensitive(props, v->string)) ok = 0;
    cJSON_ArrayForEach(v, required) if (!str(a, v->valuestring)) ok = 0;
    cJSON_Delete(schema);
    return ok;
}
static cJSON *dispatch(const char *line)
{
    cJSON *q = cJSON_ParseWithOpts(line, NULL, 1), *id, *params, *result = NULL, *response;
    const char *method, *version;
    unsigned i;
    if (!q)
        return rpc_error(NULL, -32700, "Invalid JSON.");
    id = cJSON_GetObjectItemCaseSensitive(q, "id");
    method = str(q, "method");
    params = cJSON_GetObjectItemCaseSensitive(q, "params");
    if (!cJSON_IsObject(q) || !str(q, "jsonrpc") || strcmp(str(q, "jsonrpc"), "2.0") ||
        !method || (id && !cJSON_IsString(id) && !cJSON_IsNumber(id)))
    {
        response = rpc_error(NULL, -32600, "Invalid JSON-RPC request.");
        goto done;
    }
    if (!id)
    {
        response = NULL;
        goto done;
    } /* Never perform tools from a notification. */
    version = str(cJSON_GetObjectItemCaseSensitive(params, "_meta"),
                  "io.modelcontextprotocol/protocolVersion");
    if (version && strcmp(version, "2026-07-28"))
    {
        cJSON *data, *supported;
        response = rpc_error(id, -32022, "Unsupported protocol version");
        data = cJSON_AddObjectToObject(cJSON_GetObjectItemCaseSensitive(response, "error"),
                                       "data");
        supported = cJSON_AddArrayToObject(data, "supported");
        cJSON_AddItemToArray(supported, cJSON_CreateString("2026-07-28"));
        cJSON_AddStringToObject(data, "requested", version);
        goto done;
    }
    if (!strcmp(method, "server/discover") || !strcmp(method, "initialize"))
    {
        cJSON *info;
        result = cJSON_CreateObject();
        if (!strcmp(method, "initialize"))
            cJSON_AddStringToObject(result, "protocolVersion", "2025-11-25");
        else
        {
            cJSON *a = cJSON_AddArrayToObject(result, "supportedVersions");
            cJSON_AddItemToArray(a, cJSON_CreateString("2026-07-28"));
        }
        info = cJSON_AddObjectToObject(result, "serverInfo");
        cJSON_AddStringToObject(info, "name", "termic-amiga");
        cJSON_AddStringToObject(info, "version", TERMIC_VERSION);
        cJSON_AddObjectToObject(cJSON_AddObjectToObject(result, "capabilities"), "tools");
        cJSON_AddStringToObject(
            result, "instructions",
            "Native Amiga source-copy tasks. Create isolated tasks before writing; use "
            "task_status to poll workers. Credentials and arbitrary shell commands are not "
            "exposed. No desktop peer-pairing protocol.");
    }
    else if (!strcmp(method, "ping"))
        result = cJSON_CreateObject();
    else if (!strcmp(method, "tools/list"))
    {
        cJSON *a;
        result = cJSON_CreateObject();
        a = cJSON_AddArrayToObject(result, "tools");
        for (i = 0; i < sizeof(tools) / sizeof(tools[0]); i++)
            cJSON_AddItemToArray(a, tool_schema(&tools[i]));
    }
    else if (!strcmp(method, "tools/call"))
    {
        const char *name = str(params, "name");
        cJSON *a = cJSON_GetObjectItemCaseSensitive(params, "arguments");
        for (i = 0; name && i < sizeof(tools) / sizeof(tools[0]); i++)
            if (!strcmp(name, tools[i].name))
            {
                if (valid_args(&tools[i], a))
                {
                    char lock[512], task[512];
                    int locked = 0;
                    if (!tools[i].readonly && strcmp(tools[i].name, "task_stop") &&
                        task_path(a, task))
                    {
                        pathjoin(lock, sizeof(lock), task, ".mcp-lock");
                        locked = make_dir(lock);
                        if (!locked)
                            result =
                                content("Task is being updated by another MCP request.", 1);
                    }
                    if (!result)
                        result = run_tool(&tools[i], a);
                    if (locked)
                    {
#if defined(__AMIGA__) || defined(__VBCC__)
                        DeleteFile((STRPTR)lock);
#else
                        rmdir(lock);
#endif
                    }
#if defined(__AMIGA__) || defined(__VBCC__)
                    if (!tools[i].readonly)
                    {
                        struct MsgPort *ui;
                        Forbid();
                        ui = FindPort((STRPTR) "TermicWorkspaceUI");
                        if (ui)
                            Signal(ui->mp_SigTask, SIGBREAKF_CTRL_D);
                        Permit();
                    }
#endif
                }
                break;
            }
        if (!result)
        {
            response = rpc_error(id, -32602, "Unknown tool or invalid arguments.");
            goto done;
        }
    }
    else
    {
        response = rpc_error(id, -32601, "Method not supported.");
        goto done;
    }
    if (version || !strcmp(method, "server/discover"))
    {
        cJSON *meta, *info;
        cJSON_DeleteItemFromObjectCaseSensitive(result, "serverInfo");
        cJSON_AddStringToObject(result, "resultType", "complete");
        meta = cJSON_AddObjectToObject(result, "_meta");
        info = cJSON_AddObjectToObject(meta, "io.modelcontextprotocol/serverInfo");
        cJSON_AddStringToObject(info, "name", "termic-amiga");
        cJSON_AddStringToObject(info, "version", TERMIC_VERSION);
        if (!strcmp(method, "server/discover") || !strcmp(method, "tools/list"))
        {
            cJSON_AddNumberToObject(result, "ttlMs", 60000);
            cJSON_AddStringToObject(result, "cacheScope", "public");
        }
    }
    response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "jsonrpc", "2.0");
    cJSON_AddItemToObject(response, "id", cJSON_Duplicate(id, 1));
    cJSON_AddItemToObject(response, "result", result);
done:
    cJSON_Delete(q);
    return response;
}
int main(int argc, char **argv)
{
    char *line = malloc(MCP_LIMIT + 2), *configured = read_preference("projects-drawer", 383),
         *out;
    FILE *in = stdin, *dest = stdout;
    int one = 0, rc = 0;
    if (argc == 2 && !strcmp(argv[1], "--version"))
    {
        puts(version_tag + 7);
        free(line);
        free(configured);
        return 0;
    }
    strcpy(root, configured ? configured : "Termic:Projects");
    free(configured);
    root[strcspn(root, "\r\n")] = 0;
    {
        const char *c;
        for (c = root; *c; c++)
            if (!isalnum((unsigned char)*c) && !strchr(":/_-", *c))
            {
                fprintf(stderr, "Invalid project drawer.\n");
                free(line);
                return 20;
            }
    }
    if (argc == 5 && !strcmp(argv[1], "--request") && !strcmp(argv[3], "--reply"))
    {
        one = 1;
        in = fopen(argv[2], "r");
        dest = fopen(argv[4], "w");
    }
    else if (argc != 1 && !(argc == 2 && !strcmp(argv[1], "--stdio")))
    {
        fprintf(stderr, "Usage: TermicMCP [--stdio | --request file --reply file]\n");
        free(line);
        return 20;
    }
    if (!line || !in || !dest)
    {
        if (in && in != stdin)
            fclose(in);
        if (dest && dest != stdout)
            fclose(dest);
        free(line);
        return 20;
    }
    while (fgets(line, MCP_LIMIT + 2, in))
    {
        cJSON *r;
        if (strlen(line) > MCP_LIMIT)
        {
            int c;
            if (!strchr(line, '\n'))
                while ((c = fgetc(in)) != EOF && c != '\n')
                {
                }
            r = rpc_error(NULL, -32600, "Request exceeds 64 KiB.");
        }
        else
            r = dispatch(line);
        if (r)
        {
            out = cJSON_PrintUnformatted(r);
            if (out)
            {
                fprintf(dest, "%s\n", out);
                fflush(dest);
                free(out);
            }
            else
                rc = 20;
            cJSON_Delete(r);
        }
        if (one)
            break;
    }
    if (in != stdin)
        fclose(in);
    if (dest != stdout)
        fclose(dest);
    free(line);
    return rc;
}
