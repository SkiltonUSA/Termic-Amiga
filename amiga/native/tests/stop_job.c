/* Written hardware acceptance: send the same cooperative stop as GUI/MCP. */
#include <proto/exec.h>
#include <dos/dos.h>
#include <exec/ports.h>
#include <stdio.h>
int main(int argc, char **argv)
{
    unsigned long hash = 5381;
    const char *p;
    char name[40];
    struct MsgPort *port;
    int found;
    if (argc != 2) return 20;
    for (p=argv[1]; *p; p++) hash=hash*33+(unsigned char)*p;
    sprintf(name,"TermicJob.%08lx",hash);
    Forbid(); port=FindPort((STRPTR)name); found=port!=NULL;
    if(port)Signal(port->mp_SigTask,SIGBREAKF_CTRL_C|SIGBREAKF_CTRL_E);
    Permit();
    return found ? 0 : 20;
}
