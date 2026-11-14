#ifndef TERMIC_ACTIVITY_SCROLL_H
#define TERMIC_ACTIVITY_SCROLL_H
/* Keep the editor's cursor and viewport together. Scrolling only the prop
 * leaves the cursor at the top, so MUI can jump back there on show/layout. */
static void activity_scroll_bottom(Object *editor)
{
    ULONG line = 0;
    DoMethod(editor, MUIM_TextEditor_ARexxCmd, "POSITION EOF");
    GetAttr(MUIA_TextEditor_CursorY, editor, &line);
    /* Reapply even when EOF was already the cursor position: the user may
     * have moved only the scrollbar. TextEditor then reveals that cursor. */
    set(editor, MUIA_TextEditor_CursorY, line);
}
#endif
