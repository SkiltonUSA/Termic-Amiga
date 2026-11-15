/* Written native MUI acceptance: recolouring preserves text, undo and cursor. */
#include "../mui_compat.h"
#include "../syntax.h"
#include <mui/TextEditor_mcc.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct Library *MUIMasterBase;
struct IntuitionBase *IntuitionBase;
const char stack_size[]="$STACK:262144";
static Object *source;
static int colour_pending,colour_delay,source_loaded=1;
static char opened[184]="main.c";
static ULONG value(Object *o,ULONG a){ULONG v=0;GetAttr(a,o,&v);return v;}
#include "../editor_tools_ui.h"
int main(void)
{
    Object *app,*window;char *before,*after,*styled;ULONG x,y;int ok=1;
    IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37);
    MUIMasterBase=OpenLibrary("muimaster.library",19);
    if(!IntuitionBase || !MUIMasterBase)return 20;
    app=ApplicationObject,MUIA_Application_Title,"Termic editor acceptance",
        SubWindow,window=WindowObject,MUIA_Window_Title,"Termic editor acceptance",
        MUIA_Window_Width,500,MUIA_Window_Height,240,WindowContents,
        source=MUI_NewObject("TextEditor.mcc",MUIA_TextEditor_Contents,"int x;\n",
            MUIA_TextEditor_ExportHook,MUIV_TextEditor_ExportHook_NoStyle,TAG_DONE),End,End;
    if(!app)return 20;set(window,MUIA_Window_Open,TRUE);
    DoMethod(source,MUIM_TextEditor_InsertText,"return 42;\n",MUIV_TextEditor_InsertText_Bottom);
    x=value(source,MUIA_TextEditor_CursorX);y=value(source,MUIA_TextEditor_CursorY);
    before=(char *)DoMethod(source,MUIM_TextEditor_ExportText);
    colour_pending=1;editor_recolour();
    after=(char *)DoMethod(source,MUIM_TextEditor_ExportText);
    printf("text=%d\n",before && after && !strcmp(before,after));
    ok &= before && after && !strcmp(before,after);
    printf("cursor=%d undo=%lu dirty=%lu\n",x==value(source,MUIA_TextEditor_CursorX) && y==value(source,MUIA_TextEditor_CursorY),value(source,MUIA_TextEditor_UndoAvailable),value(source,MUIA_TextEditor_HasChanged));
    ok &= x==value(source,MUIA_TextEditor_CursorX) && y==value(source,MUIA_TextEditor_CursorY);
    ok &= value(source,MUIA_TextEditor_UndoAvailable)!=0 && value(source,MUIA_TextEditor_HasChanged)!=0;
    set(source,MUIA_TextEditor_ExportHook,MUIV_TextEditor_ExportHook_Plain);
    styled=(char *)DoMethod(source,MUIM_TextEditor_ExportText);
    printf("styled=%s\n",styled?styled:"NULL");
    ok &= styled && (strstr(styled,"ff7b8b") || strstr(styled,"FF7B8B"));
    if(styled)FreeVec(styled);
    set(source,MUIA_TextEditor_ExportHook,MUIV_TextEditor_ExportHook_NoStyle);
    DoMethod(source,MUIM_TextEditor_ARexxCmd,"UNDO");
    styled=(char *)DoMethod(source,MUIM_TextEditor_ExportText);
    printf("undo-text=%sEND\n",styled?styled:"NULL");
    ok &= styled && !strcmp(styled,"int x;\n");
    if(styled)FreeVec(styled);
    set(source,MUIA_TextEditor_HasChanged,FALSE);colour_pending=1;editor_recolour();
    printf("clean=%lu\n",value(source,MUIA_TextEditor_HasChanged));
    ok &= !value(source,MUIA_TextEditor_HasChanged);
    if(before)FreeVec(before);if(after)FreeVec(after);
    set(window,MUIA_Window_Open,FALSE);MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);CloseLibrary((struct Library *)IntuitionBase);
    printf("Live editor colour preservation: %s\n",ok?"PASS":"FAIL");return ok?0:20;
}
