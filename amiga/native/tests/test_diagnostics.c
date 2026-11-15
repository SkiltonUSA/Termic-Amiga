#include "diagnostics.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    struct Diagnostic rows[4];
    const char *log="warning 12 in line 4 of \"main.c\": fixture\n"
        "src/util.h:8:3: error: fixture\n../../secret.c:2:bad\n"
        "main.c:0:error\nmain.c:abc:error\nProgram ran\n";
    assert(diagnostics_parse(log,rows,4)==2);
    assert(rows[0].line==4 && !strcmp(rows[0].path,"main.c"));
    assert(rows[1].line==8 && !strcmp(rows[1].path,"src/util.h"));
    assert(diagnostics_parse(log,rows,1)==1);
    assert(diagnostics_parse("error in line 4 of \"main.c\"\n",rows,4)==1);
    assert(diagnostics_parse("ignored in line 4\n\"main.c\"",rows,4)==0);
    puts("VBCC and GCC diagnostic locations and invalid paths passed.");return 0;
}
