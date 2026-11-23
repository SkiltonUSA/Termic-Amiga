/* Per-editor shared pens follow the screen through MUI Setup/Cleanup. */
#include "editor_colors.h"
#define MUIA_Termic_DarkEditor (TAG_USER+0x54e001)
struct KeyEditorData {
 struct MUI_EventHandlerNode events;
 struct ColorMap *colormap;
 ULONG pens[EDITOR_COLOR_COUNT];
 UBYTE owned[EDITOR_COLOR_COUNT];
 int dark;
};
static struct TagItem *editor_next_tag(struct TagItem **cursor)
{
 struct TagItem *t;
 while((t=*cursor)!=NULL) {
  if(t->ti_Tag==TAG_DONE)return NULL;
  if(t->ti_Tag==TAG_MORE){*cursor=(struct TagItem *)t->ti_Data;continue;}
  if(t->ti_Tag==TAG_SKIP){*cursor=t+1+t->ti_Data;continue;}
  *cursor=t+1;if(t->ti_Tag!=TAG_IGNORE)return t;
 }
 return NULL;
}
static void editor_palette_setup(Object *obj,struct KeyEditorData *d)
{
 unsigned i;struct Screen *s=_screen(obj);struct DrawInfo *di;
 ULONG fallback=2;
 d->colormap=s->ViewPort.ColorMap;
 di=GetScreenDrawInfo(s);if(di){fallback=di->dri_Pens[SHINEPEN];FreeScreenDrawInfo(s,di);}
 for(i=0;i<EDITOR_COLOR_COUNT;i++) {
  ULONG rgb=editor_rgb[i];
  LONG pen=ObtainBestPen(d->colormap,((rgb>>16)&255)*0x01010101UL,
    ((rgb>>8)&255)*0x01010101UL,(rgb&255)*0x01010101UL,
    OBP_Precision,PRECISION_GUI,OBP_FailIfBad,FALSE,TAG_DONE);
  d->owned[i]=pen>=0;d->pens[i]=pen>=0?(ULONG)pen:fallback;
 }
}
static void editor_palette_cleanup(struct KeyEditorData *d)
{
 unsigned i;
 if(d->colormap)for(i=0;i<EDITOR_COLOR_COUNT;i++)if(d->owned[i])ReleasePen(d->colormap,d->pens[i]);
 memset(d->owned,0,sizeof(d->owned));d->colormap=NULL;
}
static ULONG editor_palette_set(struct IClass *cl,Object *obj,struct opSet *msg,struct KeyEditorData *d)
{
 struct TagItem *scan=msg->ops_AttrList,*t,*tags;struct opSet copy=*msg;
 unsigned count=0,i=0;int needed=0;ULONG result;
 while((t=editor_next_tag(&scan))!=NULL){count++;if(t->ti_Tag==MUIA_TextEditor_Contents || t->ti_Tag==MUIA_TextEditor_Pen)needed=1;}
 if(!needed)return DoSuperMethodA(cl,obj,(Msg)msg);
 tags=calloc(count+1,sizeof(*tags));if(!tags)return FALSE;
 scan=msg->ops_AttrList;
 while((t=editor_next_tag(&scan))!=NULL) {
  tags[i]=*t;
  if(t->ti_Tag==MUIA_TextEditor_Contents && t->ti_Data) {
   char *text=editor_color_text((char *)t->ti_Data,d->dark);
   if(!text)goto failed;
   tags[i].ti_Data=(ULONG)text;
  } else if(t->ti_Tag==MUIA_TextEditor_Pen && (t->ti_Data&0xff000000UL)==0xff000000UL)
   tags[i].ti_Data=editor_color_index(t->ti_Data);
  i++;
 }
 copy.ops_AttrList=tags;result=DoSuperMethodA(cl,obj,(Msg)&copy);
 goto cleanup;
failed:
 result=FALSE;
cleanup:
 while(i--)if(tags[i].ti_Tag==MUIA_TextEditor_Contents && tags[i].ti_Data)free((void *)tags[i].ti_Data);
 free(tags);return result;
}
