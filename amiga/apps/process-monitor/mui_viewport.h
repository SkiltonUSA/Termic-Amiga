/* Native MUI scrolling keeps large forms/tables reachable on small screens.
 * MUI owns the scrollers, font metrics, borders and minimum-size calculation. */
#ifndef TERMIC_MUI_VIEWPORT_H
#define TERMIC_MUI_VIEWPORT_H
#include <intuition/screens.h>
/* MUI 5 attribute, documented by amiga-mui/muidev MUI_Scrollgroup.
 * Keep the OS3 ABI header usable with older SDK archives. */
#ifndef MUIA_Scrollgroup_AutoBars
#define MUIA_Scrollgroup_AutoBars 0x8042f50eUL
#endif
static Object *mui_viewport(Object *contents)
{
    Object *virtual_group;
    if(!contents)return NULL;
    virtual_group=VGroupV,InnerSpacing(0,0),Child,contents,End;
    if(!virtual_group)return NULL;
    return ScrollgroupObject,MUIA_Scrollgroup_AutoBars,TRUE,MUIA_Scrollgroup_Contents,virtual_group,End;
}
static Object *mui_scroll_window(ULONG first,...)
{
    struct TagItem tags[64],*input=(struct TagItem *)&first;
    struct Screen *screen=LockPubScreen(NULL);
    LONG width=screen?screen->Width:1024,height=screen?screen->Height-screen->BarHeight-1:780;
    int n=0;
    if(screen)UnlockPubScreen(NULL,screen);
    while(input->ti_Tag!=TAG_DONE && n<61) {
        tags[n]=*input++;
        if(tags[n].ti_Tag==MUIA_Window_RootObject) {
            tags[n].ti_Data=(ULONG)mui_viewport((Object *)tags[n].ti_Data);
            if(!tags[n].ti_Data)return NULL;
        }
        /* MUI's requested dimensions are inner dimensions. Leave room for
         * the screen title, native window borders and sizing gadget. */
        if(tags[n].ti_Tag==MUIA_Window_Width && (LONG)tags[n].ti_Data>width-24)tags[n].ti_Data=width-24;
        if(tags[n].ti_Tag==MUIA_Window_Height && (LONG)tags[n].ti_Data>height-36)tags[n].ti_Data=height-36;
        n++;
    }
    tags[n].ti_Tag=TAG_DONE;tags[n].ti_Data=0;
    return __termic_mui_new(MUIMasterBase,MUIC_Window,tags);
}
#define ResponsiveWindowObject mui_scroll_window(TAG_IGNORE,0
#endif
