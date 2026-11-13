/* Application-local window chrome. No Workbench font or Intuition patching. */
#ifndef TERMIC_CHROME_H
#define TERMIC_CHROME_H
#include "chrome_geometry.h"
#include <devices/inputevent.h>
#include <intuition/screens.h>

struct MUI_CustomClass *__termic_mui_class(__reg("a6") struct Library *,
                                           __reg("a0") struct Library *, __reg("a1") char *,
                                           __reg("a2") struct MUI_CustomClass *, __reg("d0") ULONG,
                                           __reg("a3") APTR) = "\tjsr\t-108(a6)";
void __termic_mui_unclass(__reg("a6") struct Library *,
                          __reg("a0") struct MUI_CustomClass *) = "\tjsr\t-114(a6)";
static struct MUI_CustomClass *chrome_class;
struct ChromeData
{
    struct MUI_EventHandlerNode events;
    LONG dragging, resize, mx, my, left, top, width, height;
};
static ULONG chrome_dispatch(__reg("a0") struct IClass *cl, __reg("a2") Object *obj,
                             __reg("a1") Msg msg)
{
    struct ChromeData *d = obj ? INST_DATA(cl, obj) : NULL;
    switch (msg->MethodID)
    {
    case MUIM_Setup:
        if (!DoSuperMethodA(cl, obj, msg))
            return FALSE;
        memset(d, 0, sizeof(*d));
        get(obj, MUIA_UserData, &d->resize);
        d->events.ehn_Object = obj;
        d->events.ehn_Class = cl;
        d->events.ehn_Events =
            IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW;
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, &d->events);
        return TRUE;
    case MUIM_Cleanup:
        d->dragging = FALSE;
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, &d->events);
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_HandleEvent:
    {
        struct IntuiMessage *im = ((struct MUIP_HandleEvent *)msg)->imsg;
        struct Window *w = _window(obj);
        struct Screen *s;
        if (!im || !w)
            break;
        s = w->WScreen;
        if (im->Class == IDCMP_INACTIVEWINDOW || im->Class == IDCMP_ACTIVEWINDOW)
        {
            d->dragging = FALSE;
            if (!d->resize)
            {
                set(obj, MUIA_Background,
                    im->Class == IDCMP_ACTIVEWINDOW ? "2:r2d2d2d2d,5d5d5d5d,9d9d9d9d"
                                                    : "2:r60606060,6c6c6c6c,80808080");
                if (MUIMasterBase->lib_Version >= 21)
                    set(obj, MUIA_Text_PreParse, im->Class == IDCMP_ACTIVEWINDOW
                        ? "\33P[FFFFFF]\33p[2D5D9D]\33B"
                        : "\33P[FFFFFF]\33p[606C80]\33B");
            }
        }
        else if (im->Class == IDCMP_MOUSEBUTTONS)
        {
            if (im->Code == SELECTUP && d->dragging)
            {
                d->dragging = FALSE;
                return MUI_EventHandlerRC_Eat;
            }
            if (im->Code == SELECTDOWN && im->MouseX >= _left(obj) && im->MouseX <= _right(obj) &&
                im->MouseY >= _top(obj) && im->MouseY <= _bottom(obj))
            {
                d->dragging = TRUE;
                d->mx = s->MouseX;
                d->my = s->MouseY;
                d->left = w->LeftEdge;
                d->top = w->TopEdge;
                d->width = w->Width;
                d->height = w->Height;
                return MUI_EventHandlerRC_Eat;
            }
        }
        else if (im->Class == IDCMP_MOUSEMOVE && d->dragging)
        {
            LONG dx = s->MouseX - d->mx, dy = s->MouseY - d->my;
            if (!(im->Qualifier & IEQUALIFIER_LEFTBUTTON))
            {
                d->dragging = FALSE;
                break;
            }
            {
                struct ChromeRect start, next;
                start.x = d->left;
                start.y = d->top;
                start.width = d->width;
                start.height = d->height;
                next = chrome_motion(start, dx, dy, d->resize, s->Width, s->Height,
                                     s->BarHeight + 1, w->MinWidth, w->MinHeight);
                if (d->resize)
                    ChangeWindowBox(w, next.x, next.y, next.width, next.height);
                else
                    MoveWindow(w, next.x - w->LeftEdge, next.y - w->TopEdge);
            }
            return MUI_EventHandlerRC_Eat;
        }
        break;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}
static Object *chrome_area(struct TextFont *font, int resize)
{
    if (!chrome_class)
        chrome_class = __termic_mui_class(MUIMasterBase, NULL, MUIC_Text, NULL,
                                          sizeof(struct ChromeData), (APTR)chrome_dispatch);
    if (!chrome_class)
        return NULL;
    return NewObject(chrome_class->mcc_Class, NULL, MUIA_UserData, resize, MUIA_Font,
                     font ? (ULONG)font : MUIV_Font_Normal, MUIA_Text_Contents,
                     resize ? " /" : "\33bTermic Amiga Workspace", MUIA_Text_PreParse,
                     MUIMasterBase->lib_Version >= 21
                         ? "\33P[FFFFFF]\33p[2D5D9D]\33B" : "\33" "3",
                     MUIA_Background, "2:r2d2d2d2d,5d5d5d5d,9d9d9d9d", MUIA_FixHeight,
                     resize ? 18 : 32, MUIA_Text_SetVMax, FALSE,
                     resize ? MUIA_FixWidth : TAG_IGNORE, 24, MUIA_InnerLeft, 10, MUIA_InnerRight,
                     6, MUIA_ShortHelp, resize ? "Drag to resize" : "Drag to move the window",
                     TAG_DONE);
}
static void chrome_cleanup(void)
{
    if (chrome_class)
        __termic_mui_unclass(MUIMasterBase, chrome_class);
    chrome_class = NULL;
}
#endif
