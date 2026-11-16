/* Native integration test. No process controls or configured service commands
 * are executed. Run with Stack 262144 on an Amiga with MUI 5. */
#define TERMIC_EMBEDDED
#include "../../apps/process-monitor/main.c"
#define assert(test) do { if(!(test)) { printf("ACTIVITY_CHECK_FAIL line %d\n",__LINE__); goto done; } } while(0)
struct Library *MUIMasterBase,*DiskfontBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
int main(void)
{
 Object *parent=NULL,*first;ULONG count,index;Row selected;int result=20;
 MUIMasterBase=OpenLibrary("muimaster.library",21);
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37);
 GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",37);
 DiskfontBase=OpenLibrary("diskfont.library",37);
 assert(MUIMasterBase && IntuitionBase && GfxBase && DiskfontBase);
 parent=MUI_NewObject(MUIC_Application,MUIA_Application_Title,"Activity checks",
  MUIA_Application_Base,"TERMICACTIVITYCHECK",TAG_DONE);assert(parent);
 assert(termic_monitor_open(parent,1));first=win;
 assert(termic_monitor_open(parent,1) && win==first);
 GetAttr(MUIA_List_Entries,list,&count);assert(count==3);
 assert(!zzvisible);GetAttr(MUIA_ShowMe,arminfo,&count);assert(!count);
 assert(termic_monitor_event(TM_EVENT+10) && zzvisible);
 GetAttr(MUIA_ShowMe,arminfo,&count);assert(count);
 assert(strstr(armtext,"SYNTHETIC ZZ9000"));
 assert(termic_monitor_event(TM_EVENT+10) && !zzvisible);
 GetAttr(MUIA_ShowMe,arminfo,&count);assert(!count);
 selected=rows[0];SetAttrs(list,MUIA_List_Active,0,TAG_DONE);
 assert(termic_monitor_event(TM_EVENT+7)); /* CPU descending */
 assert(rows[0].hits==30 && rows[2].hits==10);
 GetAttr(MUIA_List_Active,list,&index);assert(index==2 && same(&selected,&rows[index]));
 assert(termic_monitor_event(TM_EVENT+8));assert(rows[0].tracked==196608);
 assert(termic_monitor_event(TM_EVENT+9));assert(strstr(displayname(&rows[0]),"Demo1"));
 assert(termic_monitor_event(TM_EVENT+3) && paused && !sample.installed);
 Delay(60);termic_monitor_poll();assert(paused && timerpending);
 assert(termic_monitor_event(TM_EVENT+3) && !paused);
 assert(!termic_monitor_event(1)); /* parent Quit must remain the parent's event */
 termic_monitor_event(TM_EVENT+4);
 assert(!zzvisible && !arminfo && !zzbutton);
 assert(!win && !sample.installed && !timerport && !timerio && !termic_monitor_signals());
 assert(termic_monitor_event(TM_EVENT+5)); /* harmless queued selection */
 assert(termic_monitor_open(parent,1));GetAttr(MUIA_List_Entries,list,&count);assert(count==3);
 termic_monitor_close();termic_monitor_close();
 assert(termic_monitor_open(parent,0) && sample.installed);
 Delay(60);termic_monitor_poll();assert(totalhits>0 && rowcount>0);
 assert(termic_monitor_event(TM_EVENT+3) && paused && !sample.installed);
 termic_monitor_event(TM_EVENT+3);assert(sample.installed);
 termic_monitor_close();assert(!sample.installed && !pending.phase);
 result=0;
done:
 termic_monitor_close();
 if(parent)MUI_DisposeObject(parent);
 if(DiskfontBase)CloseLibrary(DiskfontBase);if(GfxBase)CloseLibrary((struct Library *)GfxBase);
 if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);if(MUIMasterBase)CloseLibrary(MUIMasterBase);
 if(!result)puts("ACTIVITY_INTEGRATION_PASS");return result;
}
