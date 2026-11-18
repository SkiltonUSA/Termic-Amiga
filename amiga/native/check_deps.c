/* Read-only runtime/build preflight; never installs or replaces libraries. */
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "version.h"
extern struct ExecBase *SysBase;
const char stack_size[]="$STACK:65536";
const char version_tag[]=TERMIC_VERSION_TAG("TermicCheck");
static int missing;
static void check(const char *name,int ok)
{
    printf("%s: %s\n",ok?"OK":"MISSING",name);if(!ok)missing++;
}
static int file(const char *path)
{
    BPTR lock=Lock((STRPTR)path,ACCESS_READ);if(!lock)return 0;UnLock(lock);return 1;
}
static void library(const char *name,unsigned version,unsigned revision)
{
    struct Library *lib=OpenLibrary((STRPTR)name,version);
    int ok=lib && (lib->lib_Version>version || lib->lib_Revision>=revision);
    if(lib)printf("  detected %u.%u\n",lib->lib_Version,lib->lib_Revision);
    check(name,ok);if(lib)CloseLibrary(lib);
}
int main(int argc,char **argv)
{
    int gui_only=argc==2 && !strcmp(argv[1],"--gui");
    if(argc==2 && !strcmp(argv[1],"--version")){puts(version_tag+7);return 0;}
    check("68020 or newer CPU",(SysBase->AttnFlags & (AFF_68020|AFF_68030|AFF_68040))!=0);
    library("dos.library",47,0);
    library("muimaster.library",21,0);
    library("MUI:Libs/mui/TextEditor.mcc",15,56);
    if(!gui_only) {
        check("VBCC driver",file("Termic:SDK/vbcc_bin_amigaos68k/bin/vc"));
        check("VBCC Amiga target",file("Termic:SDK/vbcc_target_m68k-amigaos/config/aos68k"));
        check("NDK headers",file("Termic:SDK/NDK/Include_H/exec/types.h"));
        check("Offline Tiny runtime",file("Termic:Tools/Tiny/tiny68k"));
        check("Offline Tiny model",file("Termic:Tools/Tiny/tiny.t68"));
        check("MicroPython",file("Termic:Tools/MicroPython/micropython"));
        check("Native Git",file("Termic:Tools/amiga-git/git"));
        library("amisslmaster.library",5,27);
        check("ARexx interpreter",file("SYS:Rexxc/RX"));
    }
    printf("Termic dependency check: %s (%d missing)\n",missing?"FAIL":"PASS",missing);
    return missing?20:0;
}
