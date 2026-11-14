#include "syntax.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
static int word(int c)
{
    return isalnum((unsigned char)c) || c == '_';
}
static int keyword(const char *s, size_t n)
{
    static const char *words[] = {
        "auto",     "break",  "case",   "char",     "const",    "continue", "default",  "do",
        "double",   "else",   "enum",   "extern",   "float",    "for",      "goto",     "if",
        "inline",   "int",    "long",   "register", "restrict", "return",   "short",    "signed",
        "sizeof",   "static", "struct", "switch",   "typedef",  "union",    "unsigned", "void",
        "volatile", "while",  "_Bool",  NULL};
    int i;
    for (i = 0; words[i]; i++)
        if (strlen(words[i]) == n && !strncmp(s, words[i], n))
            return 1;
    return 0;
}
char *syntax_colours(const char *text, int csyntax)
{
    /* Bright accents on a black editor surface.
     * RGB escapes let TextEditor own screen pens across show/hide cycles. */
    static const char *colour[] = {"E6EDF3", "FF7B8B", "DDA0FF", "FFB86C",
                                   "8FE3A4", "A6AFBD", "79C8FF"};
    const char *p = text, *end, *q;
    char *out, *w;
    size_t n;
    int kind, first = 1;
    if (!text || (n = strlen(text)) > 32768 || strchr(text, 27))
        return NULL;
    out = malloc((n + 1) * 32);
    if (!out)
        return NULL;
    w = out;
    memcpy(w, "\033P[E6EDF3]", 10);
    w += 10;
    while (*p)
    {
        end = p + 1;
        kind = 0;
        if (*p == '\n')
        {
            *w++ = *p++;
            memcpy(w, "\033P[E6EDF3]", 10);
            w += 10;
            first = 1;
            continue;
        }
        if (*p == ' ' || *p == '\t' || *p == '\r')
        {
            *w++ = *p++;
            continue;
        }
        if (!csyntax)
        {
            while (*end && *end != '\n')
                end++;
        }
        else if (first && *p == '#')
        {
            kind = 6;
            while (*end && *end != '\n')
                end++;
        }
        else if (p[0] == '/' && p[1] == '/')
        {
            kind = 5;
            while (*end && *end != '\n')
                end++;
        }
        else if (p[0] == '/' && p[1] == '*')
        {
            kind = 5;
            end = p + 2;
            while (*end && !(end[0] == '*' && end[1] == '/'))
                end++;
            if (*end)
                end += 2;
        }
        else if (*p == '"' || *p == '\'')
        {
            kind = 4;
            while (*end)
            {
                if (*end == '\\' && end[1])
                    end += 2;
                else if (*end++ == *p)
                    break;
            }
        }
        else if (isalpha((unsigned char)*p) || *p == '_')
        {
            while (word(*end))
                end++;
            if (keyword(p, end - p))
                kind = 1;
            else
            {
                q = end;
                while (isspace((unsigned char)*q))
                    q++;
                if (*q == '(')
                    kind = 2;
            }
        }
        else if (isdigit((unsigned char)*p))
        {
            kind = 3;
            while (word(*end) || *end == '.')
                end++;
        }
        first = 0;
        /* Colour is reset at each newline by TextEditor, including in comments. */
        do
        {
            memcpy(w, "\033P[", 3);
            w += 3;
            memcpy(w, colour[kind], 6);
            w += 6;
            *w++ = ']';
            while (p < end && *p != '\n')
                *w++ = *p++;
            memcpy(w, "\033P[E6EDF3]", 10);
            w += 10;
            if (p < end && *p == '\n')
            {
                *w++ = *p++;
                first = 1;
            }
        } while (p < end);
    }
    *w = 0;
    return out;
}

void activity_clear(struct ActivityView *v)
{
    free(v->agent); free(v->log);
    memset(v, 0, sizeof(*v));
}
static int activity_prefix(const char *old, const char *text)
{
    return old && strlen(text) >= strlen(old) && !strncmp(old, text, strlen(old));
}
static char *activity_copy(const char *s)
{
    char *p = malloc(strlen(s) + 1);
    if (p) strcpy(p, s);
    return p;
}
char *activity_update(struct ActivityView *v, const char *agent, const char *log, int *replace)
{
    const char *parts[2], *p, *colour;
    char *a, *l, *out, *w;
    size_t size = 64;
    int i, kind, reset;
    if (!agent || !log || strlen(agent) > 262144 || strlen(log) > 262144) return NULL;
    reset = !activity_prefix(v->agent, agent) || !activity_prefix(v->log, log) || v->bytes > 4194304UL;
    parts[0] = agent + (reset ? 0 : strlen(v->agent));
    parts[1] = log + (reset ? 0 : strlen(v->log));
    for (i = 0; i < 2; i++) {
        size += strlen(parts[i]) + 40;
        for (p = parts[i]; *p; p++) if (*p == '\n') size += 10;
    }
    out = malloc(size); a = activity_copy(agent); l = activity_copy(log);
    if (!out || !a || !l) { free(out); free(a); free(l); return NULL; }
    w = out; kind = reset ? 0 : v->kind;
    for (i = 0; i < 2; i++) {
        p = parts[i]; if (!*p) continue;
        colour = i ? "\033P[FFCC80]" : "\033P[8BD5FF]";
        memcpy(w, colour, 10); w += 10;
        if (kind != i + 1) {
            const char *label = i ? "[Log]\n" : "[Agent]\n";
            if (kind) { *w++ = '\n'; memcpy(w, colour, 10); w += 10; }
            while (*label) *w++ = *label++;
            memcpy(w, colour, 10); w += 10;
        }
        while (*p) {
            *w++ = *p == 27 ? '?' : *p;
            if (*p++ == '\n') { memcpy(w, colour, 10); w += 10; }
        }
        kind = i + 1;
    }
    *w = 0;
    free(v->agent); free(v->log); v->agent = a; v->log = l;
    v->kind = kind; v->bytes = (reset ? 0 : v->bytes) + (w - out);
    *replace = reset;
    return out;
}
