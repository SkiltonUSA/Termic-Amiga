#ifndef TERMIC_CLI_CONSOLE_H
#define TERMIC_CLI_CONSOLE_H
#include <stdlib.h>
#include <string.h>
/* The displayed history is never interpreted as a command. Only a single
 * editable tail after the exact canonical prefix can be submitted. */
struct CliConsole { char *prefix; char draft[1024]; unsigned long row; };
static void cli_console_clear(struct CliConsole *c)
{
    free(c->prefix);memset(c,0,sizeof(*c));
}
static int cli_console_capture(struct CliConsole *c,const char *text)
{
    size_t n,len,i;
    if(!c->prefix || !text)return 0;
    n=strlen(c->prefix);
    if(strncmp(text,c->prefix,n))return 0;
    text+=n;len=strlen(text);
    /* TextEditor exports a final line terminator even for a one-line draft. */
    if(len && text[len-1]=='\n')len--;
    if(len>=sizeof(c->draft))return 0;
    for(i=0;i<len;i++)if((unsigned char)text[i]<32 || text[i]==127)return 0;
    memcpy(c->draft,text,len);c->draft[len]=0;return 1;
}
static int cli_console_log(struct CliConsole *c,const char *log)
{
    size_t n=strlen(log),i,w=0;char *p;
    /* Bound the editable view; the full log remains in the Output page/file. */
    if(n>32768){log+=n-32768;n=32768;}
    p=malloc(n+4);if(!p)return 0;
    c->row=0;
    for(i=0;i<n;i++) {
        unsigned char ch=(unsigned char)log[i];
        if(ch=='\r')continue;
        if(ch<32 && ch!='\n' && ch!='\t')ch='?';
        if(ch==127)ch='?';
        p[w++]=(char)ch;if(ch=='\n')c->row++;
    }
    if(w && p[w-1]!='\n'){p[w++]='\n';c->row++;}
    p[w++]='>';p[w++]=' ';p[w]=0;
    free(c->prefix);c->prefix=p;return 1;
}
static char *cli_console_display(const struct CliConsole *c)
{
    const char *p;char *out,*w;size_t n;
    if(!c->prefix)return NULL;
    n=strlen(c->prefix)+strlen(c->draft);
    out=malloc(n+11*(c->row+1)+1);if(!out)return NULL;
    w=out;memcpy(w,"\033P[E6EDF3]",10);w+=10;
    for(p=c->prefix;*p;p++) {
        *w++=*p;
        if(*p=='\n'){memcpy(w,"\033P[E6EDF3]",10);w+=10;}
    }
    strcpy(w,c->draft);return out;
}
#endif
