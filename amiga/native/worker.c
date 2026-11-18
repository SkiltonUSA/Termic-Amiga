#include "native.h"
#include "profiles.h"
#include <dos/var.h>
#include "version.h"
#include "workspace.h"
#include "vendor/cJSON.h"
#include <ctype.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <exec/ports.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "job_finish.h"
#include "tiny_profile.h"
const char version_tag[] = TERMIC_VERSION_TAG("TermicAgent");
const char stack_size[] = "$STACK:262144";
int cancel_requested(void)
{
    static int cancelled;
    /* E retains the UI/MCP stop request if a RunCommand child consumes C. */
    if (SetSignal(0, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E) &
        (SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E)) cancelled = 1;
    return cancelled;
}
#ifdef __VBCC__
/* The default stdio Ctrl-C hook exits immediately, leaving public ports and
 * .busy behind. Latch cancellation and let the worker release its resources.
 * Commands loaded with RunCommand retain their own runtime break handling. */
void _chkabort(void)
{
    (void)cancel_requested();
}
#endif
/* Shell V47 merges diagnostics when stdout is explicitly redirected.
 * SYS_Output alone does not provide that shell-level redirection. */
static LONG run_command(const char *command)
{
    char redirected[1200], *errors;
    LONG rc;
    BPTR capture;
    if (strlen(command) + 20 >= sizeof(redirected))
        return 20;
    capture = Open((STRPTR) ".command.log", MODE_NEWFILE);
    if (!capture)
        return 20;
    Close(capture);
    snprintf(redirected, sizeof(redirected), "%s > .command.log\n", command);
    fflush(stdout);
    rc = SystemTags((STRPTR)redirected, TAG_DONE);
    errors = read_text(".command.log", RESPONSE_LIMIT);
    if (errors && *errors)
        printf("%s", errors);
    free(errors);
    return rc;
}
static const char *last_build_error;
static LONG run_build(void)
{
    LONG rc;
    char *receipt;
    const char *message = "Build exited before Program.new was promoted. Let successful nested scripts reach EOF instead of Quit 0.\n";
    last_build_error=NULL;
    if (!DeleteFile((STRPTR)".build-ok") && IoErr()!=ERROR_OBJECT_NOT_FOUND) {
        last_build_error="Cannot clear the previous build completion marker.";
        puts(last_build_error);
        return 20;
    }
    rc=run_command("Execute Termic:BuildProject");
    if(rc==0) {
        receipt=read_text(".build-ok",32);
        if(!receipt || strcmp(receipt,"complete\n")) {
            last_build_error=message;
            fputs(message,stdout);rc=20;
        }
        free(receipt);
    }
    return rc;
}
int build_project(void)
{
    LONG r = run_build();
    printf("Local compiler exit code: %ld\n", (long)r);
    return r == 0;
}
/* Resolve existing files (or a new file's parent) and reject links outside
 * the current workspace before handing content to the cloud agent. */
static int same_prefix(const char *a, const char *b, unsigned n)
{
    while (n--)
    {
        if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++))
            return 0;
    }
    return 1;
}
static int project_inside(const char *path)
{
    char root[512], resolved[512], parent[384];
    BPTR l;
    char *slash;
    int ok;
    if (!path || strlen(path) >= sizeof(parent)) return 0;
    l = Lock((STRPTR) "", ACCESS_READ);
    if (!l)
        return 0;
    ok = NameFromLock(l, (STRPTR)root, sizeof(root));
    UnLock(l);
    if (!ok)
        return 0;
    l = Lock((STRPTR)path, ACCESS_READ);
    if (!l)
    {
        strcpy(parent, path);
        slash = strrchr(parent, '/');
        if (slash)
            *slash = 0;
        else
            *parent = 0;
        l = Lock((STRPTR)parent, ACCESS_READ);
    }
    if (!l)
        return 0;
    ok = NameFromLock(l, (STRPTR)resolved, sizeof(resolved));
    UnLock(l);
    if (!ok)
        return 0;
    if (strlen(root) == strlen(resolved) && same_prefix(root, resolved, strlen(root)))
        return 1;
    if (strlen(root) + 2 > sizeof(root)) return 0;
    strcat(root, "/");
    return strlen(resolved) >= strlen(root) && same_prefix(root, resolved, strlen(root));
}
int project_source_allowed(const char *path)
{
    char sibling[192];
    if (!agent_source_name(path) || !project_inside(path)) return 0;
    snprintf(sibling, sizeof(sibling), "%s.new", path);
    if (!project_inside(sibling)) return 0;
    snprintf(sibling, sizeof(sibling), "%s.bak", path);
    return project_inside(sibling);
}
/* Execute a fixed binary in this worker process. RunCommand delivers a
 * cooperative Ctrl-C to the actual command and returns its real exit code.
 * No shell parses an agent-supplied script name. */
static LONG run_binary_input(const char *binary, const char *arguments, const char *input_path)
{
    BPTR segment, input, output, old_input, old_output, old_error, cwd, home, file_lock, old_home;
    char old_name[256];
    struct Process *process = (struct Process *)FindTask(NULL);
    LONG rc;
    char *text;
    segment = LoadSeg((STRPTR)binary);
    if (!segment) {
        printf("Cannot load %s (DOS error %ld).\n", binary, (long)IoErr());
        return -1;
    }
    input = Open((STRPTR)input_path, MODE_OLDFILE);
    output = Open((STRPTR)".command.log", MODE_NEWFILE);
    if (!input || !output) {
        if (input) Close(input);
        if (output) Close(output);
        UnLoadSeg(segment); return -1;
    }
    cwd = Lock((STRPTR)"", ACCESS_READ);
    file_lock = Lock((STRPTR)binary, ACCESS_READ);
    home = file_lock ? ParentDir(file_lock) : 0;
    if (file_lock) UnLock(file_lock);
    if (!cwd || !home) {
        if (cwd) UnLock(cwd);
        if (home) UnLock(home);
        Close(input); Close(output); UnLoadSeg(segment); return -1;
    }
    old_name[0] = 0; GetProgramName(old_name, sizeof(old_name));
    SetProgramName((STRPTR)binary); old_home = SetProgramDir(home);
    fflush(stdout);
    old_input = SelectInput(input); old_output = SelectOutput(output);
    old_error = process->pr_CES; process->pr_CES = output;
    rc = RunCommand(segment, 262144, (STRPTR)arguments, strlen(arguments));
    Flush(output);
    UnLock(CurrentDir(cwd));
    UnLock(SetProgramDir(old_home)); SetProgramName(old_name);
    process->pr_CES = old_error;
    SelectOutput(old_output); SelectInput(old_input);
    Close(output); Close(input); UnLoadSeg(segment);
    text = read_text(".command.log", RESPONSE_LIMIT);
    if (text) { fputs(text, stdout); free(text); }
    fflush(stdout);
    return rc;
}
static LONG run_binary(const char *binary,const char *arguments)
{
    return run_binary_input(binary,arguments,"NIL:");
}
static int run_tiny(const struct AgentProfile *profile)
{
    char question[74],*task=read_text("task.txt",8192),*answer;
    FILE *history;LONG result=20;
    if(strcmp(profile->model,"tiny-2.1")) {
        puts("Tiny supports model tiny-2.1. Choose it in Agents settings.");goto done;
    }
    if(!tiny_question(task,question)) {
        puts("Tiny needs one short question, up to 72 printable ASCII characters. Use another agent for coding tasks.");goto done;
    }
    if(cancel_requested())goto done;
    if(!save_text(".tiny-input",question,73))goto done;
    puts("Tiny 2.1 by WollyMonjac, running locally on the 68k CPU. No network or file tools.");
    fflush(stdout);
    result=run_binary_input(profile->command,"\"Termic:Tools/Tiny/tiny.t68\" -ask\n",".tiny-input");
    if(cancel_requested()){puts("Tiny stopped.");result=20;goto done;}
    if(result) {
        puts("Tiny could not finish. Install tiny68k and tiny.t68 in Termic:Tools/Tiny; see docs/tiny.md.");goto done;
    }
    answer=read_text(".command.log",8192);
    if(!answer || !*answer){free(answer);result=20;puts("Tiny returned no answer.");goto done;}
    history=fopen("transcript.txt","a");
    if(!history){free(answer);result=20;goto done;}
    fprintf(history,"Task:\n%s\nTiny 2.1 (68k):\n%s\n",task,answer);
    if(fclose(history))result=20;
    free(answer);
    puts("Tiny finished. Its answer is in Agent activity; no project files were changed.");
done:
    DeleteFile((STRPTR)".tiny-input");
    DeleteFile((STRPTR)".tiny-input.new");
    DeleteFile((STRPTR)".tiny-input.bak");
    free(task);return (int)result;
}
char *project_execute(const char *action, const char *path)
{
    char args[200], output[16385], *result, *script;
    const char *error = NULL;
    LONG rc = -1;
    FILE *file;
    size_t n = 0, i;
    int truncated = 0, cancelled;
    cJSON *object;
    /* Never return stale output after a failed preflight/load. */
    DeleteFile((STRPTR)".command.log");
    if (cancel_requested()) error = "Execution cancelled before launch.";
    else if (!strcmp(action, "build")) rc = run_build();
    else if (!strcmp(action, "arexx")) {
        int rexx;
        const char *ext = path ? strrchr(path, '.') : NULL;
        if (!ext || (strcmp(ext, ".rexx") && strcmp(ext, ".rx")) || !project_source_allowed(path))
            error = "Invalid ARexx path.";
        else {
            script = read_text(path, SOURCE_LIMIT);
            if (!script) error = "Script missing, binary or larger than 32 KiB.";
            free(script);
            Forbid(); rexx = FindPort((STRPTR)"REXX") != NULL; Permit();
            if (!error && !rexx) error = "RexxMast is not running. Start RexxMast and retry.";
            if (!error) {
                /* RX treats a quoted first argument as inline ARexx. The
                 * validated task path has no spaces or shell metacharacters. */
                snprintf(args, sizeof(args), "%s\n", path);
                rc = run_binary("SYS:Rexxc/RX", args);
            }
        }
    } else if(!strcmp(action,"python")) {
        const char *ext=path?strrchr(path,'.'):NULL;
        if(!ext || strcmp(ext,".py") || !project_source_allowed(path))error="Invalid MicroPython path.";
        else {
            script=read_text(path,SOURCE_LIMIT);
            if(!script)error="Script missing, binary or larger than 32 KiB.";
            free(script);
            if(!error) {
                snprintf(args,sizeof(args),"%s\n",path);
                rc=run_binary("Termic:Tools/MicroPython/micropython",args);
            }
        }
    } else if (!strcmp(action, "program")) {
        unsigned char header[4];
        file = project_inside("Program") ? fopen("Program", "rb") : NULL;
        if (!file) error = "Program is missing or outside the workspace. Build first.";
        else {
            if (fread(header, 1, 4, file) != 4 || memcmp(header, "\0\0\3\363", 4))
                error = "Program is not an Amiga Hunk executable.";
            fclose(file);
        }
        if (!error) rc = run_binary("Program", "\n");
    } else error = "Unknown execution action.";
    file = fopen(".command.log", "rb");
    if (file) {
        n = fread(output, 1, sizeof(output)-1, file);
        truncated = fgetc(file) != EOF;
        fclose(file);
    }
    for (i = 0; i < n; i++)
        if ((unsigned char)output[i] < 32 && output[i] != '\n' && output[i] != '\r' && output[i] != '\t') output[i] = '?';
    output[n] = 0;
    printf("%s exit code: %ld%s\n", action, (long)rc, error ? " (not executed)" : "");
    if (error) puts(error);
    fflush(stdout);
    object = cJSON_CreateObject();
    if (!object) return NULL;
    cJSON_AddStringToObject(object, "action", action);
    cancelled = cancel_requested();
    cJSON_AddBoolToObject(object, "success", !error && rc == 0 && !cancelled);
    cJSON_AddBoolToObject(object, "cancelled", cancelled);
    cJSON_AddNumberToObject(object, "exit_code", rc);
    cJSON_AddStringToObject(object, "output", output);
    cJSON_AddBoolToObject(object, "output_truncated", truncated);
    if (error) cJSON_AddStringToObject(object, "error", error);
    else if(!strcmp(action,"build") && last_build_error)
        cJSON_AddStringToObject(object,"error",last_build_error);
    else if (rc == -1) cJSON_AddStringToObject(object, "error", "Command could not be loaded or launched.");
    result = cJSON_PrintUnformatted(object); cJSON_Delete(object);
    return result;
}
int project_guidance_allowed(const char *path)
{
    return guidance_name(path) && project_inside(path);
}
static int install_guidance(void)
{
    const char *dirs[]={".agents",".agents/skills",".agents/skills/termic",".agents/skills/termic/references"};
    int i,ok=1; BPTR l;
    char path[384],temp[384],*text;
    for(i=0;i<4;i++) {
        l=Lock((STRPTR)dirs[i],ACCESS_READ);
        if(l) UnLock(l);
        else {l=CreateDir((STRPTR)dirs[i]);if(!l)return 20;UnLock(l);}
        if(!project_inside(dirs[i])) {puts("Instruction drawer is outside the workspace.");return 20;}
    }
    for(i=0;i<GUIDANCE_COUNT;i++) {
        l=Lock((STRPTR)guidance_files[i],ACCESS_READ);
        if(l) {UnLock(l);continue;} /* Preserve project-owned files. */
        if(!project_guidance_allowed(guidance_files[i])) {ok=0;continue;}
        snprintf(temp,sizeof(temp),"%s.new",guidance_files[i]);
        l=Lock((STRPTR)temp,ACCESS_READ);
        if(l) {UnLock(l);ok=0;continue;}
        snprintf(path,sizeof(path),"Termic:Template/%s",guidance_files[i]);
        text=read_text(path,8192);
        if(!text || !save_text(guidance_files[i],text,8192))ok=0;
        free(text);
    }
    puts(ok?"Project instruction files installed; existing files preserved.":"Some instruction files could not be installed.");
    return ok?0:20;
}
static void collect_sources(const char *dir, const char *prefix, char *out, unsigned *used,
                            int depth)
{
    BPTR l;
    struct FileInfoBlock *f;
    char rel[184], sub[184];
    unsigned n;
    if (depth > 6)
        return;
    l = Lock((STRPTR)dir, ACCESS_READ);
    if (!l)
        return;
    f = AllocDosObject(DOS_FIB, NULL);
    if (f && Examine(l, f))
        while (ExNext(l, f))
        {
            if (f->fib_FileName[0] == '.' || !strcmp(f->fib_FileName, "Tasks") ||
                strlen(prefix) + strlen(f->fib_FileName) > 178)
                continue;
            sprintf(rel, "%s%s", prefix, f->fib_FileName);
            if (f->fib_DirEntryType == ST_USERDIR)
            {
                sprintf(sub, "%s/", rel);
                collect_sources(rel, sub, out, used, depth + 1);
            }
            else if (f->fib_DirEntryType == ST_FILE && project_source_allowed(rel))
            {
                n = strlen(rel);
                if (*used + n + 2 < 16384)
                {
                    strcpy(out + *used, rel);
                    *used += n;
                    out[(*used)++] = '\n';
                    out[*used] = 0;
                }
            }
        }
    if (f)
        FreeDosObject(DOS_FIB, f);
    UnLock(l);
}
char *list_project_sources(void)
{
    char *out = calloc(1, 16384);
    unsigned n = 0;
    if (out)
        collect_sources("", "", out, &n, 0);
    return out;
}
static int run_profile(void)
{
    struct AgentProfiles *r=calloc(1,sizeof(*r));
    struct AgentProfile *p;
    char line[1200],*mode,*previous,*env,*end,*eq; int i,rc=20;
    if(!r || !profiles_load(r) || (i=profile_selected(r))<0) {
        puts("Agent profile missing or invalid."); free(r); return 20;
    }
    p=&r->items[i];
    if(p->kind==0) { free(r); return run_agent(); }
    if(p->kind==PROFILE_TINY) {rc=run_tiny(p);free(r);return rc;}
    if(profile_is_relay(p->kind)) { free(r); return run_relay(); }
    if ((p->kind == 3 || p->kind == 4) && !*p->args) {
        puts("Enter a script path and arguments in Launch / Sessions. Use a CLI for interactive input.");
        free(r);return 20;
    }
    mode=read_text("agent-yolo",8); previous=read_text("cli-started",64);
    if(!profile_command(p,mode && !strcmp(mode,"1"),p->resume && previous && !strcmp(previous,p->id),line,sizeof(line))) {
        puts("Configure an Amiga executable and valid arguments in Agents / CLI / Scripts.");
        goto done;
    }
    /* Per-process local variables are inherited by the command; ENV: is untouched. */
    env=p->env;
    while(*env) {
        end=strchr(env,'\n'); if(end) *end=0;
        if(*env) { eq=strchr(env,'='); *eq++=0;
            if(!SetVar((STRPTR)env,(STRPTR)eq,-1,GVF_LOCAL_ONLY)) { puts("Cannot set command environment."); goto done; }
        }
        if(!end) break; env=end+1;
    }
    printf("Starting %s. Native commands have ordinary AmigaDOS access.\n",p->name);
    rc=run_command(line);
    if(!rc) save_text("cli-started",p->id,31);
done: free(mode);free(previous);free(r);return rc;
}
int main(int argc, char **argv)
{
    BPTR lock, old;
    int result = 20;
    FILE *f;
    struct MsgPort port;
    char portname[40];
    unsigned long hash = 5381;
    const char *p;
    const char *path = "Termic:Project", *action = argc > 1 ? argv[1] : "--agent";
    if (argc == 2 && !strcmp(argv[1], "--version"))
    {
        puts(version_tag + 7);
        return 0;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc == 4 && !strcmp(argv[2], "--project"))
        path = argv[3];
    else if (argc > 2)
    {
        puts("Usage: TermicAgent "
             "--agent|--build|--run|--probe|--status|--diff|--init|--commit|--shell|--rexx|--python|--instructions|--branches|--branch|--checkout|--host-git [--project "
             "drawer]");
        return 20;
    }
    if (strcmp(action, "--agent") && strcmp(action, "--build") && strcmp(action, "--run") &&
        strcmp(action, "--probe") && strcmp(action, "--status") && strcmp(action, "--diff") &&
        strcmp(action, "--init") && strcmp(action, "--commit") && strcmp(action, "--shell") &&
        strcmp(action, "--rexx") && strcmp(action, "--python") && strcmp(action,"--instructions") && strcmp(action,"--host-git") &&
        strcmp(action,"--branches") && strcmp(action,"--branch") && strcmp(action,"--checkout"))
        return 20;
    lock = Lock((STRPTR)path, ACCESS_READ);
    if (!lock)
        return 20;
    old = CurrentDir(lock);
    lock = CreateDir((STRPTR) ".busy");
    if (!lock)
    {
        puts("Workspace busy or not writable.");
        UnLock(CurrentDir(old));
        return 20;
    }
    UnLock(lock);
    DeleteFile((STRPTR)".launch");
    DeleteFile((STRPTR)".result");
    DeleteFile((STRPTR)".result.new");
    for (p = path; *p; ++p)
        hash = hash * 33 + (unsigned char)*p;
    sprintf(portname, "TermicJob.%08lx", hash);
    memset(&port, 0, sizeof(port));
    port.mp_Node.ln_Name = portname;
    port.mp_Node.ln_Type = NT_MSGPORT;
    port.mp_Flags = PA_IGNORE;
    port.mp_SigTask = FindTask(NULL);
    AddPort(&port);
    save_text("state.txt","Running",32);
    Forbid();
    {struct MsgPort *ui=FindPort((STRPTR)"TermicWorkspaceUI");if(ui)Signal(ui->mp_SigTask,SIGBREAKF_CTRL_D);}
    Permit();
    if (!strcmp(action,"--instructions")) result=install_guidance();
    else if (!strcmp(action, "--probe"))
    {
        char *reply = NULL;
        int status = cloud_request(NULL, NULL, &reply);
        free(reply);
        printf("Verified HTTPS probe: HTTP %d (401 expected without a key).\n", status);
        result = status == 401 ? 0 : 20;
    }
    else if(!strcmp(action,"--host-git"))result=run_git_relay();
    else if (!strcmp(action, "--build"))
        result = build_project() ? 0 : 20;
    else if (!strcmp(action, "--run"))
        result = run_command("Program");
    else if (!strcmp(action, "--agent"))
        result = run_profile();
    else if (!strcmp(action, "--shell") || !strcmp(action, "--rexx") || !strcmp(action, "--python"))
    {
        char *command = read_text("command.txt", 1024);
        if (command && *command)
        {
            size_t n = strlen(command);
            while (n && isspace((unsigned char)command[n - 1]))
                command[--n] = 0;
            if (n) {
                char script[1200];
                if (!strcmp(action,"--shell")) result = run_command(command);
                else if (script_command(!strcmp(action,"--rexx") ? 3 : 4,command,script,sizeof(script)))
                    result = run_command(script);
                else puts("Enter a script path and arguments on one line.");
            }
        }
        free(command);
    }
    else if(!strcmp(action,"--branches"))result=run_command("Termic:Tools/amiga-git/git branch -a");
    else if(!strcmp(action,"--branch") || !strcmp(action,"--checkout")) {
        char *branch=read_text("branch.txt",80),cmd[256];int valid=branch && *branch;
        const char *p=branch;
        if(valid && (!isalnum((unsigned char)*branch) || strstr(branch,"..") || strstr(branch,"//") || strstr(branch,"/.")))valid=0;
        while(valid && *p) {if(!isalnum((unsigned char)*p) && !strchr("_-/.",*p))valid=0;p++;}
        if(valid && (branch[strlen(branch)-1]=='/' || branch[strlen(branch)-1]=='.' || strstr(branch,".lock")))valid=0;
        if(valid) {
            snprintf(cmd,sizeof(cmd),"Termic:Tools/amiga-git/git checkout %s%s",!strcmp(action,"--branch")?"-b ":"",branch);
            result=run_command(cmd);
        }else puts("Invalid branch name. Use letters, digits, _, -, / and single dots.");
        free(branch);
    }
    else if (!strcmp(action, "--status"))
        result = run_command("Termic:Tools/amiga-git/git status");
    else if (!strcmp(action, "--diff"))
        result = run_command("Termic:Tools/amiga-git/git diff");
    else if (!strcmp(action, "--init"))
    {
        char root[384], cmd[512];
        BPTR here = Lock((STRPTR) "", ACCESS_READ);
        if (here)
        {
            if (NameFromLock(here, (STRPTR)root, sizeof(root)))
            {
                snprintf(cmd, sizeof(cmd), "Termic:Tools/amiga-git/git init \"%s\"", root);
                result = run_command(cmd);
            }
            UnLock(here);
        }
        if (!result)
        {
            FILE *existing = fopen(".gitignore", "rb");
            if (existing)
                fclose(existing);
            else if (!save_text(".gitignore", TERMIC_WORKSPACE_IGNORE, 2048))
                result = 20;
        }
    }
    else if (!strcmp(action, "--commit"))
    {
        char *names = list_project_sources(), *name, *next, *msg = read_text("commit.txt", 256);
        char cmd[512];
        int valid = msg && *msg;
        if (valid)
            for (name = msg; *name; ++name)
                if (!isalnum((unsigned char)*name) && !strchr(" .,:/()_-", *name))
                {
                    valid = 0;
                    break;
                }
        if (!valid)
            puts("Use letters, numbers, spaces and . , : / ( ) _ - in the commit message.");
        else if (names)
        {
            result = 0;
            for (name = names; *name && !result; name = next)
            {
                next = strchr(name, '\n');
                if (!next)
                    break;
                *next++ = 0;
                sprintf(cmd, "Termic:Tools/amiga-git/git add \"%s\"", name);
                result = run_command(cmd);
                if (cancel_requested())
                {
                    result = 20;
                    break;
                }
            }
            if (!result)
            {
                FILE *ignore = fopen(".gitignore", "rb");
                if (ignore)
                {
                    fclose(ignore);
                    result = run_command("Termic:Tools/amiga-git/git add .gitignore");
                }
            }
            if (!result)
            {
                sprintf(cmd, "Termic:Tools/amiga-git/git commit -m \"%s\"", msg);
                result = run_command(cmd);
            }
        }
        free(names);
        free(msg);
    }
    if (cancel_requested()) result = 20;
    result = finish_job(&port,result);
    Forbid();
    {struct MsgPort *ui=FindPort((STRPTR)"TermicWorkspaceUI");if(ui)Signal(ui->mp_SigTask,SIGBREAKF_CTRL_D);}
    Permit();
    UnLock(CurrentDir(old));
    return result;
}
