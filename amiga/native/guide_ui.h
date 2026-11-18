/* Native MUI help, backed by the same local guide used by MultiView. */
#include "guide_document.h"
enum { HG_CLOSE=6000,HG_CHAPTER,HG_CONTENTS,HG_INDEX,HG_BACK,HG_PREV,HG_NEXT,HG_LINK,HG_FIND,HG_FINDNEXT };
static Object *guide_window,*guide_chapters,*guide_title,*guide_links,*guide_search,*guide_note,*guide_back,*guide_prev,*guide_next,*guide_go;
static struct GuideDocument guide_doc;
static struct GuideLink guide_targets[GUIDE_LINKS];
static const char *guide_names[GUIDE_NODES+1],*guide_link_names[GUIDE_LINKS+1];
static int guide_current=-1,guide_link_count,guide_history[64],guide_history_count;
static int guide_show(int chapter,int remember)
{
 char *plain,*styled;int i;
 plain=guide_render(&guide_doc,chapter,guide_targets,&guide_link_count);
 if(!plain){set(guide_note,MUIA_Text_Contents,"Cannot display this chapter: invalid local guide markup or link.");return 0;}
 styled=syntax_colours(plain,0);
 if(!styled){free(plain);set(guide_note,MUIA_Text_Contents,"Not enough memory to display this chapter.");return 0;}
 if(remember && guide_current>=0 && guide_current!=chapter) {
  if(guide_history_count==64){memmove(guide_history,guide_history+1,63*sizeof(int));guide_history_count--;}
  guide_history[guide_history_count++]=guide_current;
 }
 guide_current=chapter;
 set(guide_title,MUIA_Text_Contents,guide_doc.nodes[chapter].title);
 SetAttrs(guide_chapters,MUIA_NoNotify,TRUE,MUIA_Cycle_Active,chapter,TAG_DONE);
 set(guide_text,MUIA_TextEditor_Contents,styled);free(styled);free(plain);
 SetAttrs(guide_text,MUIA_TextEditor_CursorX,0,MUIA_TextEditor_CursorY,0,MUIA_TextEditor_Prop_First,0,TAG_DONE);
 for(i=0;i<guide_link_count;i++)guide_link_names[i]=guide_targets[i].title;
 if(!guide_link_count)guide_link_names[i++]="No related topics";
 guide_link_names[i]=NULL;
 SetAttrs(guide_links,MUIA_Cycle_Entries,guide_link_names,MUIA_Cycle_Active,0,MUIA_Disabled,!guide_link_count,TAG_DONE);
 set(guide_go,MUIA_Disabled,!guide_link_count);set(guide_back,MUIA_Disabled,!guide_history_count);
 set(guide_prev,MUIA_Disabled,chapter==0);set(guide_next,MUIA_Disabled,chapter==guide_doc.count-1);
 set(guide_note,MUIA_Text_Contents,"Local manual. Choose a chapter or a related topic. Search applies to this chapter.");
 return 1;
}
static void open_help(void)
{
 int i;char *text;Object *contents,*index,*close,*find,*next;
 if(!guide_window) {
  text=read_text("Termic:Termic.guide",GUIDE_LIMIT);
  if(!text || !guide_parse(&guide_doc,text)){free(text);message("Cannot read Termic.guide. Install a valid manual beside TermicNative.");return;}
  free(text);
  for(i=0;i<guide_doc.count;i++)guide_names[i]=guide_doc.nodes[i].title;
  guide_names[i]=NULL;guide_link_names[0]="No related topics";guide_link_names[1]=NULL;
  guide_window=ResponsiveWindowObject,MUIA_Window_Title,"Termic: User Guide (MUI 5)",
   MUIA_Window_ID,MAKE_ID('T','H','L','P'),MUIA_Window_NoMenus,TRUE,
   MUIA_Window_Width,900,MUIA_Window_Height,650,
   WindowContents,VGroup,MUIA_Group_Spacing,4,
    Child,HGroup,MUIA_Group_Spacing,3,
     Child,contents=button("Contents",HG_CONTENTS),Child,index=button("Index",HG_INDEX),
     Child,guide_back=button("Back",HG_BACK),Child,guide_prev=button("Up",HG_PREV),
     Child,guide_next=button("Down",HG_NEXT),Child,HVSpace,Child,close=button("Close",HG_CLOSE),End,
    Child,HGroup,Child,label("Chapter"),Child,guide_chapters=cycle(guide_names),End,
    Child,guide_title=elastic_label(""),Child,editor(&guide_text,1),
    Child,HGroup,Child,guide_links=cycle(guide_link_names),Child,guide_go=button("Open",HG_LINK),End,
    Child,HGroup,Child,guide_search=entry(120),Child,find=button("Find",HG_FIND),Child,next=button("Find next",HG_FINDNEXT),End,
    Child,guide_note=elastic_label(""),End,End;
  if(!guide_window){guide_free(&guide_doc);message("Cannot open the MUI guide window.");return;}
  SetAttrs(guide_text,MUIA_Background,"2:r00000000,00000000,00000000",
    MUIA_TextEditor_Pen,0xffe6edf3UL,MUIA_TextEditor_WrapMode,MUIV_TextEditor_WrapMode_SoftWrap,TAG_DONE);
  set(guide_search,MUIA_ShortHelp,"Find text in the displayed chapter. Return finds the next match.");
  set(guide_links,MUIA_ShortHelp,"Local links from the displayed chapter. Choose a topic and press Open.");
  set(contents,MUIA_ShortHelp,"Manual contents");set(index,MUIA_ShortHelp,"Alphabetical index");
  set(guide_back,MUIA_ShortHelp,"Return to the previous topic");
  set(guide_prev,MUIA_ShortHelp,"Previous chapter");set(guide_next,MUIA_ShortHelp,"Next chapter");
  set(guide_go,MUIA_ShortHelp,"Open the selected related topic");set(close,MUIA_ShortHelp,"Close help (Escape)");
  DoMethod(app,OM_ADDMEMBER,guide_window);
  notify(guide_chapters,MUIA_Cycle_Active,MUIV_EveryTime,HG_CHAPTER);
  notify(guide_window,MUIA_Window_CloseRequest,TRUE,HG_CLOSE);
  notify(guide_window,MUIA_Window_InputEvent,(ULONG)"-repeat esc",HG_CLOSE);
  notify(guide_search,MUIA_String_Acknowledge,MUIV_EveryTime,HG_FINDNEXT);
  guide_show(guide_find(&guide_doc,"MAIN"),0);
 }
 set(guide_window,MUIA_Window_CloseRequest,FALSE);set(guide_window,MUIA_Window_Open,TRUE);
 set(guide_window,MUIA_Window_Activate,TRUE);
}
static int guide_event(ULONG id)
{
 int target;
 if(id<HG_CLOSE || id>HG_FINDNEXT)return 0;
 if(!guide_window)return 1;
 switch(id) {
 case HG_CLOSE:set(guide_window,MUIA_Window_Open,FALSE);break;
 case HG_CHAPTER:guide_show(value(guide_chapters,MUIA_Cycle_Active),1);break;
 case HG_CONTENTS:guide_show(guide_find(&guide_doc,"MAIN"),1);break;
 case HG_INDEX:target=guide_find(&guide_doc,"Index");if(target>=0)guide_show(target,1);break;
 case HG_BACK:if(guide_history_count)guide_show(guide_history[--guide_history_count],0);break;
 case HG_PREV:if(guide_current>0)guide_show(guide_current-1,1);break;
 case HG_NEXT:if(guide_current+1<guide_doc.count)guide_show(guide_current+1,1);break;
 case HG_LINK:target=value(guide_links,MUIA_Cycle_Active);if(target>=0 && target<guide_link_count)guide_show(guide_targets[target].target,1);break;
 case HG_FIND:case HG_FINDNEXT:
  if(!*(char *)value(guide_search,MUIA_String_Contents)){set(guide_note,MUIA_Text_Contents,"Enter text to find in this chapter.");break;}
  set(guide_note,MUIA_Text_Contents,DoMethod(guide_text,MUIM_TextEditor_Search,
     (STRPTR)value(guide_search,MUIA_String_Contents),id==HG_FIND?MUIF_TextEditor_Search_FromTop:MUIF_TextEditor_Search_Next)
     ?"Match selected. Find next continues through this chapter.":"No further match in this chapter. Find searches again from the top.");break;
 }
 return 1;
}
