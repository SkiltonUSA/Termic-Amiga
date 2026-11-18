#include "../cli_console.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    struct CliConsole c={0};char text[1200],*styled,*longlog;size_t n;
    assert(cli_console_log(&c,"done\n"));assert(c.row==1);
    assert(cli_console_capture(&c,"done\n> Echo hello\n"));
    assert(!strcmp(c.draft,"Echo hello"));
    assert(!cli_console_capture(&c,"edited\n> Delete ALL\n"));
    assert(!cli_console_capture(&c,"done\n> Echo one\nEcho two\n"));
    assert(!cli_console_capture(&c,"done\n> Echo \033oops\n"));
    assert(!strcmp(c.draft,"Echo hello"));
    assert(cli_console_log(&c,"new output\nsecond line"));
    assert(!strcmp(c.draft,"Echo hello") && c.row==2);
    styled=cli_console_display(&c);assert(styled);
    assert(!strcmp(styled,"\033P[E6EDF3]new output\n\033P[E6EDF3]second line\n\033P[E6EDF3]> Echo hello"));free(styled);
    assert(cli_console_log(&c,""));assert(c.row==0);
    strcpy(text,"> ");memset(text+2,'x',1023);text[1025]=0;
    assert(cli_console_capture(&c,text));assert(strlen(c.draft)==1023);
    text[1025]='x';text[1026]=0;assert(!cli_console_capture(&c,text));
    assert(cli_console_capture(&c,"> \n"));assert(!*c.draft);
    longlog=malloc(70000);assert(longlog);memset(longlog,'\n',69999);longlog[69999]=0;
    assert(cli_console_log(&c,longlog));free(longlog);
    styled=cli_console_display(&c);assert(styled);free(styled);
    n=strlen(c.prefix);assert(n==32770 && c.row==32768);
    assert(cli_console_log(&c,"\033P[000000]\r\n\001"));
    assert(!strchr(c.prefix,27) && !strchr(c.prefix,1));
    cli_console_clear(&c);assert(!c.prefix && !*c.draft && !c.row);
    puts("CLI console draft, history protection, bounds and styling: PASS");return 0;
}
