/* Opt-in native regression: preserve a sentence split across text updates. */
#include <libraries/mui.h>
#include <mui/TextEditor_mcc.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <string.h>
struct Library *MUIMasterBase;
const char stack_size[] = "$STACK:262144";
int main(void)
{
    Object *editor; char *text; int result=20;
    MUIMasterBase=OpenLibrary("muimaster.library",19);
    if(!MUIMasterBase)return 20;
    editor=MUI_NewObject("TextEditor.mcc",
        MUIA_TextEditor_ReadOnly,TRUE,
        MUIA_TextEditor_WrapMode,MUIV_TextEditor_WrapMode_SoftWrap,
        MUIA_TextEditor_WrapBorder,0,
        MUIA_TextEditor_ImportHook,MUIV_TextEditor_ImportHook_Plain,
        MUIA_TextEditor_ExportHook,MUIV_TextEditor_ExportHook_NoStyle,
        MUIA_TextEditor_Contents,"First fragment",TAG_DONE);
    if(editor) {
        DoMethod(editor,MUIM_TextEditor_InsertText," continues here.\n",MUIV_TextEditor_InsertText_Bottom);
        text=(char *)DoMethod(editor,MUIM_TextEditor_ExportText);
        if(text) {
            printf("Export: <%s>\n",text);
            if(!strcmp(text,"First fragment continues here.\n"))result=0;
            FreeVec(text);
        }
        MUI_DisposeObject(editor);
    }
    CloseLibrary(MUIMasterBase);
    printf("TextEditor chunk boundary: %s\n",result?"FAIL":"PASS");
    return result;
}
