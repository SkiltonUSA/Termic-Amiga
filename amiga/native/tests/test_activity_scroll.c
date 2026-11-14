/* Opt-in regression on the installed MUI/TextEditor classes. Synthetic text
 * only; no production task files or settings are opened. */
#include "../mui_compat.h"
#include <mui/TextEditor_mcc.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <string.h>
#include "../activity_scroll.h"
struct Library *MUIMasterBase;
struct IntuitionBase *IntuitionBase;
const char stack_size[] = "$STACK:262144";
static ULONG attr(Object *object, ULONG name)
{
    ULONG result = 0;
    GetAttr(name, object, &result);
    return result;
}
static int bottom(Object *editor, const char *stage)
{
    LONG first = attr(editor, MUIA_TextEditor_Prop_First);
    LONG total = attr(editor, MUIA_TextEditor_Prop_Entries);
    LONG visible = attr(editor, MUIA_TextEditor_Prop_Visible);
    LONG height = attr(editor, MUIA_TextEditor_Prop_DeltaFactor);
    printf("%s first=%ld total=%ld visible=%ld\n", stage, first, total, visible);
    return visible > 0 && total > visible && first > 0 && first + visible * height >= total * height;
}
int main(void)
{
    Object *app, *window, *editor, *input;
    char text[8192]; int i, ok = 1;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    if (!IntuitionBase) return 20;
    MUIMasterBase = OpenLibrary("muimaster.library", 19);
    if (!MUIMasterBase) {CloseLibrary((struct Library *)IntuitionBase); return 20;}
    text[0] = 0;
    for (i = 0; i < 100; i++) strcat(text, "Synthetic activity line for scrolling.\n");
    app = ApplicationObject,
        MUIA_Application_Title, "Termic activity acceptance",
        SubWindow, window = WindowObject,
            MUIA_Window_Title, "Termic activity acceptance",
            MUIA_Window_Width, 480, MUIA_Window_Height, 240,
            WindowContents, VGroup,
                Child, input = StringObject, StringFrame, End,
                Child, editor = MUI_NewObject("TextEditor.mcc",
                    MUIA_TextEditor_ReadOnly, TRUE,
                    MUIA_TextEditor_Contents, text, TAG_DONE),
            End,
        End,
    End;
    if (!app) {CloseLibrary(MUIMasterBase); CloseLibrary((struct Library *)IntuitionBase); return 20;}
    /* The application follows after initial MUI setup, then on updates. */
    activity_scroll_bottom(editor);
    set(window, MUIA_Window_Open, TRUE);
    set(window, MUIA_Window_ActiveObject, input);
    activity_scroll_bottom(editor);
    ok &= bottom(editor, "load");
    set(editor, MUIA_TextEditor_Prop_First, 0);
    activity_scroll_bottom(editor);
    ok &= bottom(editor, "same cursor after scroll-up");
    set(editor, MUIA_TextEditor_Prop_First, 0);
    DoMethod(editor, MUIM_TextEditor_InsertText, "New agent output\n",
             MUIV_TextEditor_InsertText_Bottom);
    activity_scroll_bottom(editor);
    ok &= bottom(editor, "append after scroll-up");
    set(editor, MUIA_TextEditor_Contents, text);
    activity_scroll_bottom(editor);
    ok &= bottom(editor, "replace");
    set(window, MUIA_Window_Open, FALSE);
    set(window, MUIA_Window_Open, TRUE);
    ok &= bottom(editor, "reopen");
    ok &= attr(window, MUIA_Window_ActiveObject) == (ULONG)input;
    set(editor, MUIA_TextEditor_Contents, "Short activity\n");
    activity_scroll_bottom(editor);
    ok &= attr(editor, MUIA_TextEditor_Prop_First) == 0;
    set(window, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    CloseLibrary((struct Library *)IntuitionBase);
    printf("Activity follow-bottom: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 20;
}
