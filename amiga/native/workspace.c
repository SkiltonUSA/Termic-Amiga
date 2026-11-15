#include "workspace.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
const char *guidance_files[GUIDANCE_COUNT] = {
    "AGENTS.md", "CLAUDE.md", ".agents/skills/termic/SKILL.md",
    ".agents/skills/termic/.termic-version",
    ".agents/skills/termic/references/development.md",
    ".agents/skills/termic/references/mcp.md"
};
int workspace_name_equal(const char *a,const char *b)
{
    if(!a || !b)return 0;
    while(*a && *b)if(tolower((unsigned char)*a++)!=tolower((unsigned char)*b++))return 0;
    return !*a && !*b;
}
int guidance_name(const char *name)
{
    int i;
    if (!name) return 0;
    for (i=0;i<GUIDANCE_COUNT;i++) if (workspace_name_equal(name,guidance_files[i])) return 1;
    return 0;
}
int source_name(const char *s)
{
    const char *p = s, *dot;
    unsigned n = 0;
    if (guidance_name(s)) return 1;
    if (!s || !*s || *s == '.' || *s == '/')
        return 0;
    for (; *p; ++p)
    {
        if (++n > 180)
            return 0;
        if (*p == '/')
        {
            if (p == s || p[-1] == '/' || p[1] == '.' || !p[1])
                return 0;
        }
        else if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-' && *p != '.')
            return 0;
    }
    dot = strrchr(s, '.');
    return dot && (!strcmp(dot, ".c") || !strcmp(dot, ".h") || !strcmp(dot, ".txt") ||
                   !strcmp(dot, ".md") || !strcmp(dot, ".py") ||
                   !strcmp(dot, ".rexx") || !strcmp(dot, ".rx"));
}
int agent_source_name(const char *path)
{
    const char *base;int i;
    static const char *reserved[]={"task.txt","name.txt","state.txt","session.txt","last-prompt.txt",
        "commit.txt","command.txt","branch.txt","build-command.txt","transcript.txt",NULL};
    if(!source_name(path) || guidance_name(path))return 0;
    base=strrchr(path,'/');base=base?base+1:path;
    for(i=0;reserved[i];i++)if(workspace_name_equal(base,reserved[i]))return 0;
    {char prefix[9];unsigned n;
     for(n=0;n<8 && base[n];n++)prefix[n]=tolower((unsigned char)base[n]);prefix[n]=0;
     if(!strncmp(prefix,"session-",8) || !strncmp(prefix,"usage-",6))return 0;}
    return 1;
}
int save_text(const char *path, const char *text, unsigned long limit)
{
    char tmp[512], bak[512];
    FILE *f;
    size_t n;
    int existed = 0, ok;
    if (!path || strlen(path) > 500 || !text || (n = strlen(text)) > limit)
        return 0;
    snprintf(tmp, sizeof(tmp), "%s.new", path);
    snprintf(bak, sizeof(bak), "%s.bak", path);
    f = fopen(tmp, "wb");
    if (!f)
        return 0;
    ok = fwrite(text, 1, n, f) == n;
    if (fclose(f))
        ok = 0;
    if (!ok)
    {
        remove(tmp);
        return 0;
    }
    f = fopen(path, "rb");
    if (f)
    {
        fclose(f);
        existed = 1;
        remove(bak);
        if (rename(path, bak))
        {
            remove(tmp);
            return 0;
        }
    }
    if (rename(tmp, path))
    {
        if (existed)
            rename(bak, path);
        remove(tmp);
        return 0;
    }
    return 1;
}
