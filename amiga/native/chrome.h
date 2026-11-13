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
static struct MUI_CustomClass *chrome_class, *chrome_icon_class;
static struct ChromeMetrics chrome_sizes;
static struct TextFont *chrome_font;
static void chrome_init_scale(struct TextFont *body_font)
{
    struct Screen *screen = LockPubScreen(NULL);
    struct TextAttr attr = {(STRPTR)"helvetica.font", 13, 0, FPF_DISKFONT};
    chrome_sizes = chrome_metrics(screen ? screen->Height : 800);
    if (screen)
        UnlockPubScreen(NULL, screen);
    if (DiskfontBase && (!body_font || body_font->tf_YSize != chrome_sizes.font))
    {
        attr.ta_YSize = chrome_sizes.font;
        chrome_font = OpenDiskFont(&attr);
    }
    /* Respect the actual fallback font if the requested size is unavailable. */
    if (chrome_font || body_font)
    {
        struct TextFont *font = chrome_font ? chrome_font : body_font;
        if (chrome_sizes.bar < font->tf_YSize + 6)
            chrome_sizes.bar = font->tf_YSize + 6;
    }
}
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
                     (chrome_font ? (ULONG)chrome_font : font ? (ULONG)font : MUIV_Font_Normal), MUIA_Text_Contents,
                     resize ? " /" : "\33bTermic Amiga Workspace", MUIA_Text_PreParse,
                     MUIMasterBase->lib_Version >= 21
                         ? "\33P[FFFFFF]\33p[2D5D9D]\33B" : "\33" "3",
                     MUIA_Background, "2:r2d2d2d2d,5d5d5d5d,9d9d9d9d", MUIA_FixHeight,
                     resize ? chrome_sizes.grip : chrome_sizes.bar, MUIA_Text_SetVMax, FALSE,
                     resize ? MUIA_FixWidth : TAG_IGNORE, chrome_sizes.bar, MUIA_InnerLeft, 10, MUIA_InnerRight,
                     6, MUIA_ShortHelp, resize ? "Drag to resize" : "Drag to move the window",
                     TAG_DONE);
}
/* Small pixel-aligned symbols, rendered using the current MUI text pen.
 * Button frames, focus, pressed state and input stay with MUI's Area class. */
/* Map the icon's 18-unit design grid to integer display pixels. */
static void chrome_icon_rect(struct RastPort *rp, LONG x, LONG y,
                             LONG left, LONG top, LONG right, LONG bottom)
{
    LONG size = chrome_sizes.icon;
    RectFill(rp, x + left * size / 18, y + top * size / 18,
             x + (right + 1) * size / 18 - 1, y + (bottom + 1) * size / 18 - 1);
}
static void chrome_square(struct RastPort *rp, LONG x, LONG y, LONG left, LONG top, LONG size)
{
    chrome_icon_rect(rp, x, y, left, top, left + size - 1, top + 1);
    chrome_icon_rect(rp, x, y, left, top + size - 2, left + size - 1, top + size - 1);
    chrome_icon_rect(rp, x, y, left, top + 2, left + 1, top + size - 3);
    chrome_icon_rect(rp, x, y, left + size - 2, top + 2, left + size - 1, top + size - 3);
}
static ULONG chrome_icon_dispatch(__reg("a0") struct IClass *cl,
                                  __reg("a2") Object *obj, __reg("a1") Msg msg)
{
    ULONG result = DoSuperMethodA(cl, obj, msg);
    if (msg->MethodID == MUIM_AskMinMax)
    {
        struct MUI_MinMax *mm = ((struct MUIP_AskMinMax *)msg)->MinMaxInfo;
        /* Area contributes frame/insets; the custom drawing must supply its size. */
        mm->MinWidth += chrome_sizes.bar;
        mm->DefWidth += chrome_sizes.bar;
        mm->MaxWidth += chrome_sizes.bar;
        mm->MinHeight += chrome_sizes.bar;
        mm->DefHeight += chrome_sizes.bar;
        mm->MaxHeight += chrome_sizes.bar;
        return 0;
    }
    if (msg->MethodID == MUIM_Draw &&
        (((struct MUIP_Draw *)msg)->flags & MADF_DRAWOBJECT))
    {
        struct RastPort *rp = _rp(obj);
        ULONG kind = 0, selected = 0, pen = GetAPen(rp), mode = GetDrMd(rp);
        LONG x, y, i;
        get(obj, MUIA_UserData, &kind);
        get(obj, MUIA_Selected, &selected);
        x = _mleft(obj) + (_mwidth(obj) - chrome_sizes.icon) / 2 + !!selected;
        y = _mtop(obj) + (_mheight(obj) - chrome_sizes.icon) / 2 + !!selected;
        SetAPen(rp, _pens(obj)[MPEN_TEXT]);
        SetDrMd(rp, JAM1);
        if (kind == 0)
        {
            for (i = 2; i < 14; i++)
            {
                chrome_icon_rect(rp, x, y, i, i, i + 2, i + 2);
                chrome_icon_rect(rp, x, y, 16 - i, i, 18 - i, i + 2);
            }
        }
        else if (kind == 1)
        {
            /* Only the visible edges of the rear square are drawn. */
            chrome_icon_rect(rp, x, y, 0, 0, 12, 1);
            chrome_icon_rect(rp, x, y, 0, 2, 1, 12);
            chrome_icon_rect(rp, x, y, 2, 11, 4, 12);
            chrome_icon_rect(rp, x, y, 11, 2, 12, 4);
            chrome_square(rp, x, y, 5, 5, 13);
        }
        else
            chrome_square(rp, x, y, 1, 1, 16);
        SetAPen(rp, pen);
        SetDrMd(rp, mode);
    }
    return result;
}
static Object *chrome_icon(ULONG kind, const char *help)
{
    if (!chrome_icon_class)
        chrome_icon_class = __termic_mui_class(MUIMasterBase, NULL, MUIC_Area, NULL,
                                               0, (APTR)chrome_icon_dispatch);
    if (!chrome_icon_class)
        return NULL;
    return NewObject(chrome_icon_class->mcc_Class, NULL, ButtonFrame,
        MUIA_UserData, kind,
        MUIA_Background, MUII_ButtonBack, MUIA_InputMode, MUIV_InputMode_RelVerify,
        MUIA_CycleChain, 1, MUIA_ShortHelp, help, TAG_DONE);
}
static void chrome_cleanup(void)
{
    if (chrome_class)
        __termic_mui_unclass(MUIMasterBase, chrome_class);
    chrome_class = NULL;
    if (chrome_icon_class)
        __termic_mui_unclass(MUIMasterBase, chrome_icon_class);
    chrome_icon_class = NULL;
    if (chrome_font)
        CloseFont(chrome_font);
    chrome_font = NULL;
}
#endif
