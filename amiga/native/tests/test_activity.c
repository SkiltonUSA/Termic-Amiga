/* Processor page lifecycle test: one parent window, no process signals. */
#define TERMIC_EMBEDDED
#include "../../apps/process-monitor/main.c"
#define assert(test) do { if(!(test)) { printf("ACTIVITY_CHECK_FAIL line %d\n",__LINE__); goto done; } } while(0)
struct Library *MUIMasterBase,*DiskfontBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
int main(void)
{
 Object *parent=NULL,*parentwin=NULL,*page;ULONG count,index;Row selected;int result=20,fixture;
 MUIMasterBase=OpenLibrary("muimaster.library",21);
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37);
 GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",37);
 DiskfontBase=OpenLibrary("diskfont.library",37);
 assert(MUIMasterBase && IntuitionBase && GfxBase && DiskfontBase);
 for(fixture=1;fixture>=0;--fixture) {
  parent=MUI_NewObject(MUIC_Application,MUIA_Application_Title,"Processor page checks",
   MUIA_Application_Base,"TERMICACTIVITYCHECK",TAG_DONE);assert(parent);
  page=termic_monitor_page(parent,fixture);assert(page);
  assert(termic_monitor_page(parent,fixture)==page && !win && !sample.installed);
  parentwin=mui_scroll_window(MUIA_Window_Title,"Termic processor page checks",
   MUIA_Window_RootObject,page,TAG_DONE);assert(parentwin);
  DoMethod(parent,OM_ADDMEMBER,parentwin);termic_monitor_attach(parentwin);
  SetAttrs(parentwin,MUIA_Window_Open,TRUE,TAG_DONE);
  GetAttr(MUIA_Window_Open,parentwin,&count);assert(count);
  termic_monitor_visible(1);
  if(fixture) {
   GetAttr(MUIA_List_Entries,list,&count);assert(count==3);
   assert(!zzvisible);assert(termic_monitor_event(TM_EVENT+10) && zzvisible);
   assert(strstr(armtext,"SYNTHETIC ZZ9000"));
   assert(termic_monitor_event(TM_EVENT+10) && !zzvisible);
   selected=rows[0];SetAttrs(list,MUIA_List_Active,0,TAG_DONE);
   assert(termic_monitor_event(TM_EVENT+7));assert(rows[0].hits==30 && rows[2].hits==10);
   GetAttr(MUIA_List_Active,list,&index);assert(index==2 && same(&selected,&rows[index]));
   assert(termic_monitor_event(TM_EVENT+8));assert(rows[0].tracked==196608);
   assert(termic_monitor_event(TM_EVENT+9));assert(strstr(displayname(&rows[0]),"Demo1"));
  } else {
   assert(sample.installed);Delay(60);termic_monitor_poll();assert(totalhits>0 && rowcount>0);
  }
  termic_monitor_visible(0);assert(!sample.installed);
  Delay(60);termic_monitor_poll();assert(timerpending);
  termic_monitor_visible(1);assert(fixture || sample.installed);
  assert(termic_monitor_event(TM_EVENT+3) && paused && !sample.installed);
  termic_monitor_visible(0);termic_monitor_visible(1);assert(!sample.installed);
  assert(termic_monitor_event(TM_EVENT+3) && !paused);
  assert(!termic_monitor_event(1));
  termic_monitor_event(TM_EVENT+4);assert(win==parentwin); /* no parent disposal */
  termic_monitor_close();termic_monitor_close();
  assert(!win && !monitor_page && !sample.installed && !timerport && !termic_monitor_signals());
  GetAttr(MUIA_Window_Open,parentwin,&count);assert(count);
  MUI_DisposeObject(parent);parent=parentwin=NULL;
 }
 result=0;
done:
 termic_monitor_close();if(parent)MUI_DisposeObject(parent);
 if(DiskfontBase)CloseLibrary(DiskfontBase);if(GfxBase)CloseLibrary((struct Library *)GfxBase);
 if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);if(MUIMasterBase)CloseLibrary(MUIMasterBase);
 if(!result)puts("ACTIVITY_INTEGRATION_PASS");return result;
}
