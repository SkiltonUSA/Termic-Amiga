/* Opt-in hardware acceptance helper: inspect native menus and preserve/test
 * clipboard bytes. Not linked into Termic or shipped in installation kits. */
#include <devices/clipboard.h>
#include <exec/io.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct IntuitionBase *IntuitionBase;
const char stack_size[] = "$STACK:65536";
static int menus(void)
{
    struct Screen *screen;
    struct Window *window;
    struct Menu *menu;
    struct MenuItem *item;
    char lines[80][180];
    int count = 0, i;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
        return 20;
    screen = LockPubScreen(NULL);
    if (!screen)
    {
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }
    Forbid();
    for (window = screen->FirstWindow; window; window = window->NextWindow)
        if (window->Title && !strcmp(window->Title, "Termic Amiga Workspace"))
            break;
    if (window)
        for (menu = window->MenuStrip; menu && count < 79; menu = menu->NextMenu)
        {
            snprintf(lines[count++], 180, "MENU %s", menu->MenuName);
            for (item = menu->FirstItem; item && count < 79; item = item->NextItem)
            {
                const char *label = "---";
                if ((item->Flags & ITEMTEXT) && item->ItemFill)
                    label = ((struct IntuiText *)item->ItemFill)->IText;
                snprintf(lines[count++], 180, "ITEM %s enabled=%d key=%c", label ? label : "---",
                         !!(item->Flags & ITEMENABLED),
                         item->Flags & COMMSEQ ? item->Command : '-');
            }
        }
    Permit();
    UnlockPubScreen(NULL, screen);
    CloseLibrary((struct Library *)IntuitionBase);
    for (i = 0; i < count; i++)
        puts(lines[i]);
    return count ? 0 : 20;
}
static int clipboard(int restore, const char *path)
{
    struct MsgPort *port = CreateMsgPort();
    struct IOClipReq *io = port ? (struct IOClipReq *)CreateIORequest(port, sizeof(*io)) : NULL;
    FILE *file = NULL;
    char buffer[1024];
    int opened = 0, ok = 0;
    unsigned long total = 0;
    if (!io || OpenDevice("clipboard.device", 0, (struct IORequest *)io, 0))
        goto done;
    opened = 1;
    file = fopen(path, restore ? "rb" : "wb");
    if (!file)
        goto done;
    io->io_Offset = 0;
    io->io_ClipID = 0;
    do
    {
        io->io_Data = buffer;
        io->io_Length = restore ? fread(buffer, 1, sizeof(buffer), file) : sizeof(buffer);
        io->io_Command = restore ? CMD_WRITE : CMD_READ;
        if (DoIO((struct IORequest *)io))
            goto done;
        if (restore && io->io_Actual != io->io_Length)
            goto done;
        if (!restore && fwrite(buffer, 1, io->io_Actual, file) != io->io_Actual)
            goto done;
        total += io->io_Actual;
        if (total > 1048576)
            goto done;
    } while (io->io_Actual);
    if (ferror(file))
        goto done;
    if (restore)
    {
        io->io_Command = CMD_UPDATE;
        if (DoIO((struct IORequest *)io))
            goto done;
    }
    ok = 1;
done:
    if (file && fclose(file))
        ok = 0;
    if (opened)
        CloseDevice((struct IORequest *)io);
    if (io)
        DeleteIORequest((struct IORequest *)io);
    if (port)
        DeleteMsgPort(port);
    puts(ok ? "OK" : "FAILED");
    return ok ? 0 : 20;
}
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "menus"))
        return menus();
    if (argc == 3 && !strcmp(argv[1], "save-clip"))
        return clipboard(0, argv[2]);
    if (argc == 3 && !strcmp(argv[1], "restore-clip"))
        return clipboard(1, argv[2]);
    return 20;
}
