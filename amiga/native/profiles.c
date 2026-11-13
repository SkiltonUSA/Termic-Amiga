#include "profiles.h"
#include "native.h"
#include "vendor/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void init(struct AgentProfile *p, const char *id, const char *name, int kind)
{
    memset(p, 0, sizeof(*p));
    strcpy(p->id, id);
    strcpy(p->name, name);
    p->kind = kind;
    p->visible = 1;
    p->resume = 1;
    p->steps = 8;
    p->tokens = 4096;
    p->usage = 1;
}
void profiles_defaults(struct AgentProfiles *p)
{
    memset(p, 0, sizeof(*p));
    p->count = 2;
    p->next_id = 1;
    init(&p->items[0], "openai", "OpenAI API", 0);
    init(&p->items[1], "shell", "Amiga CLI", 2);
    strcpy(p->items[1].command, "C:NewShell");
}
int profile_builtin(const struct AgentProfile *p)
{
    return !strcmp(p->id, "openai") || !strcmp(p->id, "shell");
}
static int single(const char *s)
{
    while (*s)
    {
        if ((unsigned char)*s < 32 || (unsigned char)*s == 127)
            return 0;
        s++;
    }
    return 1;
}
int profile_valid(const struct AgentProfile *p)
{
    const char *s = p->id, *e;
    if (!*s || !*p->name || p->kind < 0 || p->kind > 5 || p->steps < 1 || p->steps > 32 ||
        p->tokens < 256 || p->tokens > 16384 || p->visible < 0 || p->visible > 1 ||
        p->readonly < 0 || p->readonly > 1 || p->resume < 0 || p->resume > 1 || p->usage < 0 ||
        p->usage > 1)
        return 0;
    while (*s)
    {
        if (!isalnum((unsigned char)*s) && *s != '-' && *s != '_')
            return 0;
        s++;
    }
    if (!single(p->name) || !single(p->model) || !single(p->command) || !single(p->args) ||
        !single(p->yolo) || !single(p->resume_args) || strpbrk(p->command, "\"*`$;<>|"))
        return 0;
    for (s = p->model; *s; s++)
        if (isspace((unsigned char)*s))
            return 0;
    /* One NAME=value per line; values never become shell source. */
    for (s = p->env; *s; s = *e ? e + 1 : e)
    {
        const char *eq;
        e = strchr(s, '\n');
        if (!e)
            e = s + strlen(s);
        if (e == s)
            continue;
        eq = memchr(s, '=', e - s);
        if (!eq || eq == s || !(isalpha((unsigned char)*s) || *s == '_'))
            return 0;
        while (s < eq)
        {
            if (!isalnum((unsigned char)*s) && *s != '_')
                return 0;
            s++;
        }
        while (++s < e)
            if ((unsigned char)*s < 32 || (unsigned char)*s == 127)
                return 0;
    }
    return 1;
}
int profile_find(const struct AgentProfiles *p, const char *id)
{
    int i;
    for (i = 0; i < p->count; i++)
        if (!strcmp(id, p->items[i].id))
            return i;
    return -1;
}
int profile_add(struct AgentProfiles *p, int kind, int copy)
{
    char id[32];
    int n = p->next_id, i = p->count;
    if (i >= PROFILE_MAX)
        return -1;
    do
    {
        sprintf(id, "custom-%d", n++);
    } while (profile_find(p, id) >= 0);
    if (copy >= 0 && copy < p->count)
    {
        p->items[i] = p->items[copy];
        strcpy(p->items[i].id, id);
    }
    else
        init(&p->items[i], id,
             kind == 0   ? "New API agent"
             : kind == 1 ? "New Amiga CLI"
                         : kind == 2 ? "New CLI"
             : kind == 3 ? "New ARexx script" : kind == 4 ? "New MicroPython script" : "Codex on Mac",
             kind);
    if (copy < 0) {
        if (kind == 2) strcpy(p->items[i].command, "C:NewShell");
        if (kind == 3) { strcpy(p->items[i].command, "SYS:Rexxc/RX"); strcpy(p->items[i].args, "Termic:Examples/hello.rexx"); }
        if (kind == 4) { strcpy(p->items[i].command, "Termic:Tools/MicroPython/micropython"); strcpy(p->items[i].args, "Termic:Examples/hello.py"); }
    }
    p->next_id = n;
    p->count++;
    return i;
}
int profile_remove(struct AgentProfiles *p, int index)
{
    if (index < 0 || index >= p->count || profile_builtin(&p->items[index]))
        return 0;
    memmove(p->items + index, p->items + index + 1,
            (p->count - index - 1) * sizeof(p->items[0]));
    p->count--;
    return 1;
}
#define STR_FIELDS(X) X(id) X(name) X(model) X(command) X(args) X(yolo) X(resume_args) X(env)
#define INT_FIELDS(X) X(kind) X(visible) X(readonly) X(resume) X(steps) X(tokens) X(usage)
int profiles_parse(struct AgentProfiles *p, const char *json)
{
    cJSON *root = cJSON_Parse(json), *list, *v, *row;
    struct AgentProfiles *tmp = calloc(1, sizeof(*tmp));
    int ok = 0, i = 0;
    if (!tmp || !cJSON_IsObject(root))
        goto done;
    v = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (!cJSON_IsNumber(v) || v->valuedouble != 1)
        goto done;
    v = cJSON_GetObjectItemCaseSensitive(root, "next_id");
    if (!cJSON_IsNumber(v) || v->valuedouble != v->valueint || v->valueint < 1 ||
        v->valueint > 1000000000)
        goto done;
    tmp->next_id = v->valueint;
    list = cJSON_GetObjectItemCaseSensitive(root, "profiles");
    if (!cJSON_IsArray(list) || cJSON_GetArraySize(list) < 2 ||
        cJSON_GetArraySize(list) > PROFILE_MAX)
        goto done;
    cJSON_ArrayForEach(row, list)
    {
        struct AgentProfile *a = &tmp->items[i];
        if (!cJSON_IsObject(row))
            goto done;
#define READSTR(f)                                                                             \
    v = cJSON_GetObjectItemCaseSensitive(row, #f);                                             \
    if (!cJSON_IsString(v) || strlen(v->valuestring) >= sizeof(a->f))                          \
        goto done;                                                                             \
    strcpy(a->f, v->valuestring);
        STR_FIELDS(READSTR)
#undef READSTR
#define READINT(f)                                                                             \
    v = cJSON_GetObjectItemCaseSensitive(row, #f);                                             \
    if (!cJSON_IsNumber(v) || v->valuedouble != v->valueint)                                   \
        goto done;                                                                             \
    a->f = v->valueint;
        INT_FIELDS(READINT)
#undef READINT
        if (!profile_valid(a) || profile_find(tmp, a->id) >= 0)
            goto done;
        tmp->count = ++i;
    }
    i = profile_find(tmp, "openai");
    if (i < 0 || tmp->items[i].kind != 0)
        goto done;
    i = profile_find(tmp, "shell");
    if (i < 0 || tmp->items[i].kind != 2)
        goto done;
    *p = *tmp;
    ok = 1;
done:
    free(tmp);
    cJSON_Delete(root);
    return ok;
}
char *profiles_json(const struct AgentProfiles *p)
{
    cJSON *r = cJSON_CreateObject(), *list = cJSON_CreateArray();
    char *text;
    int i;
    if (!r || !list)
    {
        cJSON_Delete(r);
        cJSON_Delete(list);
        return NULL;
    }
    cJSON_AddNumberToObject(r, "version", 1);
    cJSON_AddNumberToObject(r, "next_id", p->next_id);
    cJSON_AddItemToObject(r, "profiles", list);
    for (i = 0; i < p->count; i++)
    {
        const struct AgentProfile *a = &p->items[i];
        cJSON *row = cJSON_CreateObject();
        if (!profile_valid(a) || !row)
        {
            cJSON_Delete(row);
            cJSON_Delete(r);
            return NULL;
        }
#define WRITESTR(f) cJSON_AddStringToObject(row, #f, a->f);
        STR_FIELDS(WRITESTR)
#undef WRITESTR
#define WRITEINT(f) cJSON_AddNumberToObject(row, #f, a->f);
        INT_FIELDS(WRITEINT)
#undef WRITEINT
        cJSON_AddItemToArray(list, row);
    }
    text = cJSON_PrintUnformatted(r);
    cJSON_Delete(r);
    return text;
}
int profiles_load(struct AgentProfiles *p)
{
    char *s = read_preference("agents.json", PROFILE_LIMIT);
    int ok;
    profiles_defaults(p);
    if (!s)
        return 1;
    ok = profiles_parse(p, s);
    free(s);
    if (ok) {
        int cli = profile_find(p, "shell");
        if (cli >= 0 && !strcmp(p->items[cli].name, "Amiga Shell"))
            strcpy(p->items[cli].name, "Amiga CLI");
    }
    return ok;
}
char *profile_key(const struct AgentProfile *p)
{
    char path[512], *s;
    if (!profile_valid(p))
        return NULL;
    if (!strcmp(p->id, "openai"))
        return read_preference("api-key", 512);
    sprintf(path, TERMIC_ENV "key-%s", p->id);
    s = read_text(path, 512);
    if (!s)
    {
        sprintf(path, TERMIC_CONFIG "key-%s", p->id);
        s = read_text(path, 512);
    }
    return s;
}
int profile_selected(struct AgentProfiles *p)
{
    char *id = read_text(TERMIC_PROJECT "agent-profile", 31);
    int i;
    if (!id)
        return profile_find(p, "openai");
    id[strcspn(id, "\r\n")] = 0;
    i = profile_find(p, id);
    free(id);
    return i;
}
int profile_command(const struct AgentProfile *p, int yolo, int resume, char *out,
                    unsigned size)
{
    int n;
    if (!profile_valid(p) || (p->kind == 0 || p->kind == 5) || !*p->command)
        return 0;
    n = snprintf(out, size, "\"%s\" %s %s %s", p->command, p->args, yolo ? p->yolo : "",
                 resume ? p->resume_args : "");
    return n >= 0 && (unsigned)n < size;
}
int script_command(int kind, const char *args, char *out, unsigned size)
{
    int n;
    const char *runtime = kind == 3 ? "SYS:Rexxc/RX" : "Termic:Tools/MicroPython/micropython";
    if ((kind != 3 && kind != 4) || !args || !*args || !single(args))
        return 0;
    n = snprintf(out, size, "\"%s\" %s", runtime, args);
    return n >= 0 && (unsigned)n < size;
}
