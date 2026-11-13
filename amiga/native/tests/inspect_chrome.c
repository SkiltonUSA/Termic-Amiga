/* Opt-in acceptance helper. Window identity uses ScreenTitle because the
 * custom main window intentionally has no native title. */
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <stdio.h>
#include <string.h>
struct IntuitionBase *IntuitionBase;
struct Library *LayersBase;
int main(int argc, char **argv)
{
    struct Screen *s;
    struct Window *w;
    LONG x = 0, y = 0, width = 0, height = 0, top = 0, active = 0, mx = 0, my = 0, front = 0;
    ULONG address = 0;
    const char *wanted = argc > 2 ? argv[2] : NULL;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
        return 20;
    LayersBase = OpenLibrary("layers.library", 37);
    if (!LayersBase)
    {
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }
    s = LockPubScreen(NULL);
    if (!s)
    {
        CloseLibrary(LayersBase);
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }
    LockLayerInfo(&s->LayerInfo);
    Forbid();
    for (w = s->FirstWindow; w; w = w->NextWindow)
        if (wanted ? (w->Title && !strcmp(w->Title, wanted))
                   : (w->ScreenTitle && !strcmp(w->ScreenTitle, "Termic Amiga Workspace")))
        {
            address = (ULONG)w;
            x = w->LeftEdge;
            y = w->TopEdge;
            width = w->Width;
            height = w->Height;
            top = w->BorderTop;
            active = !!(w->Flags & WFLG_WINDOWACTIVE);
            /* Test point is on the title bar, covered by the bridge window
             * when this 1400x650 acceptance window is sent backwards. */
            front = WhichLayer(&s->LayerInfo, w->LeftEdge + 20, w->TopEdge + 20) == w->WLayer;
            break;
        }
    mx = s->MouseX;
    my = s->MouseY;
    Permit();
    UnlockLayerInfo(&s->LayerInfo);
    if (w && argc >= 2 && !strcmp(argv[1], "front"))
    {
        WindowToFront(w);
        ActivateWindow(w);
    }
    UnlockPubScreen(NULL, s);
    CloseLibrary(LayersBase);
    CloseLibrary((struct Library *)IntuitionBase);
    printf("CHROME address=%08lx x=%ld y=%ld width=%ld height=%ld border=%ld active=%ld mx=%ld "
           "my=%ld front=%ld\n",
           address, x, y, width, height, top, active, mx, my, front);
    return 0;
}
