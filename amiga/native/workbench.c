/* Workbench launcher and classic .info artwork. No PNG assets are installed. */
#include "version.h"
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/wb.h>
#include <stdio.h>
#include <string.h>
struct Library *IconBase,*WorkbenchBase;
const char version_tag[]=TERMIC_VERSION_TAG("Termic Amiga");
const char stack_size[]="$STACK:262144";
#define ICON_W 48
#define ICON_H 40
static UWORD image_data[2][ICON_H*3*2];
static void pixel(unsigned which,int x,int y,unsigned pen)
{
 unsigned plane;
 for(plane=0;plane<2;plane++)if(pen&(1U<<plane))
  image_data[which][plane*ICON_H*3+y*3+x/16]|=(UWORD)(0x8000U>>(x%16));
}
static void artwork(unsigned which)
{
 int x,y;
 for(y=0;y<ICON_H;y++)for(x=0;x<ICON_W;x++) {
  unsigned pen=0;
  if(x>=3 && x<45 && y>=4 && y<34)pen=1;
  if(x>=4 && x<44 && y>=5 && y<9)pen=3;
  if(x>=5 && x<43 && y>=10 && y<32)pen=which?3:1;
  if((y==4 || x==3) && x>=3 && x<45 && y>=4 && y<34)pen=2;
  if(y>=34 && y<37 && x>=20 && x<28)pen=1;
  if(y>=37 && y<39 && x>=13 && x<35)pen=3;
  /* CLI chevron and cursor; three editor lines in Workbench pens. */
  if((x==10+y-14 && y>=14 && y<=18) || (x==14-(y-18) && y>18 && y<=22))pen=2;
  if(x>=19 && x<=25 && y>=21 && y<=22)pen=2;
  if(x>=28 && x<=38 && (y==14 || y==18 || y==22))pen=which?2:3;
  pixel(which,x,y,pen);
 }
}
static int write_tool_icon(const char *path)
{
 struct DiskObject *d=NewDiskObject(WBTOOL);struct Image normal,selected;
 APTR old_render,old_select;UWORD old_flags;int ok=0;
 if(!d)return 0;
 memset(&normal,0,sizeof(normal));normal.Width=ICON_W;normal.Height=ICON_H;
 normal.Depth=2;normal.PlanePick=3;normal.ImageData=image_data[0];
 selected=normal;selected.ImageData=image_data[1];
 old_render=d->do_Gadget.GadgetRender;old_select=d->do_Gadget.SelectRender;old_flags=d->do_Gadget.Flags;
 d->do_Gadget.GadgetRender=&normal;d->do_Gadget.SelectRender=&selected;
 d->do_Gadget.Flags=(old_flags&~GFLG_GADGHIGHBITS)|GFLG_GADGIMAGE|GFLG_GADGHIMAGE;
 d->do_Gadget.Width=ICON_W;d->do_Gadget.Height=ICON_H;
 d->do_Gadget.Activation=GACT_RELVERIFY;d->do_Gadget.GadgetType=GTYP_BOOLGADGET;
 d->do_StackSize=262144;d->do_CurrentX=NO_ICON_POSITION;d->do_CurrentY=NO_ICON_POSITION;
 ok=PutDiskObject((STRPTR)path,d);
 d->do_Gadget.GadgetRender=old_render;d->do_Gadget.SelectRender=old_select;d->do_Gadget.Flags=old_flags;
 FreeDiskObject(d);return ok;
}
int main(int argc,char **argv)
{
 char root[384],target[420],command[460];BPTR lock;int ok;
 if(argc==2 && !strcmp(argv[1],"--version")){puts(version_tag+7);return 0;}
 if(!NameFromLock(GetProgramDir(),root,sizeof(root)))return 20;
 if(argc==2 && !strcmp(argv[1],"--show-drawer")) {
  WorkbenchBase=OpenLibrary("workbench.library",44);if(!WorkbenchBase)return 20;
  CloseWorkbenchObjectA(root,NULL);
  ok=OpenWorkbenchObjectA(root,NULL);CloseLibrary(WorkbenchBase);return ok?0:20;
 }
 if(argc==2 && !strcmp(argv[1],"--check-icons")) {
  struct DiskObject *d;
  IconBase=OpenLibrary("icon.library",44);if(!IconBase)return 20;
  snprintf(target,sizeof(target),"%s/Termic Amiga",root);d=GetDiskObject(target);
  if(d)printf("Tool icon: type=%u stack=%ld gadget=%dx%d image=%dx%d\n",d->do_Type,d->do_StackSize,
      d->do_Gadget.Width,d->do_Gadget.Height,d->do_Gadget.GadgetRender?((struct Image *)d->do_Gadget.GadgetRender)->Width:0,
      d->do_Gadget.GadgetRender?((struct Image *)d->do_Gadget.GadgetRender)->Height:0);
  ok=d && d->do_Type==WBTOOL && d->do_StackSize==262144 &&
     d->do_Gadget.Width==ICON_W && d->do_Gadget.Height==ICON_H;
  if(d)FreeDiskObject(d);d=GetDiskObject(root);
  ok=ok && d && d->do_Type==WBDRAWER;if(d)FreeDiskObject(d);
  CloseLibrary(IconBase);puts(ok?"Workbench icon validation: PASS":"Workbench icon validation: FAIL");return ok?0:20;
 }
 if(argc==2 && !strcmp(argv[1],"--make-icons")) {
  struct DiskObject *d;
  IconBase=OpenLibrary("icon.library",44);if(!IconBase)return 20;
  artwork(0);artwork(1);snprintf(target,sizeof(target),"%s/Termic Amiga",root);
  ok=write_tool_icon(target);d=GetDefDiskObject(WBDRAWER);
  if(d){
   if(d->do_DrawerData){d->do_DrawerData->dd_NewWindow.Width=640;d->do_DrawerData->dd_NewWindow.Height=320;}
   ok=PutDiskObject(root,d)&&ok;FreeDiskObject(d);
  }else ok=0;
  CloseLibrary(IconBase);puts(ok?"Workbench launcher and drawer icons created.":"Icon creation failed.");
  return ok?0:20;
 }
 snprintf(target,sizeof(target),"%s/TermicNative",root);
 lock=Lock(target,ACCESS_READ);if(!lock){puts("TermicNative is missing beside this launcher.");return 20;}UnLock(lock);
 if(argc==2 && !strcmp(argv[1],"--check")){printf("Launcher target: %s\n",target);return 0;}
 snprintf(command,sizeof(command),"Stack 262144\nRun >NIL: <NIL: \"%s\"",target);
 return SystemTags(command,TAG_DONE)?20:0;
}
