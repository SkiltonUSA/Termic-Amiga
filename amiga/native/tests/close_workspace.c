/* Deployment/acceptance helper: request the GUI's normal save-and-quit path.
 * Never signals workers or other applications. The caller must wait for the
 * public port to disappear before replacing the installed executable. */
#include <proto/exec.h>
#include <dos/dos.h>
#include <exec/ports.h>
int main(void)
{
    struct MsgPort *port;
    Forbid();
    port=FindPort((STRPTR)"TermicWorkspaceUI");
    if(port)Signal(port->mp_SigTask,SIGBREAKF_CTRL_C);
    Permit();
    return 0;
}
