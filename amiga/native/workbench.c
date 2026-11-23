/* Workbench launcher and native ColorIcon artwork. No PNG assets installed. */
#include "version.h"
#include <dos/dos.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/wb.h>
#include <stdio.h>
#include <string.h>
struct Library *IconBase,*WorkbenchBase;
const char version_tag[]=TERMIC_VERSION_TAG("Termic Amiga");
const char stack_size[]="$STACK:262144";
#include "workbench_art.h"
/* Classic two-plane fallback accompanies the native colour images. */
static UWORD image_data[2][ICON_H*4*2];
static void fallback_artwork(unsigned state)
{
 int x,y;unsigned plane;
 for(y=0;y<ICON_H;y++)for(x=0;x<ICON_W;x++) {
  unsigned color=icon_pixels[state][y*ICON_W+x],pen;
  pen=!color?0:((color==5 || color==13 || color==15 || color==23)?2:
       (color==4 || color==10 || color==11 || color==12 || color==14 || color>=19)?3:1);
  for(plane=0;plane<2;plane++)if(pen&(1U<<plane))
   image_data[state][plane*ICON_H*4+y*4+x/16]|=(UWORD)(0x8000U>>(x%16));
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
 IconControl(d,ICONCTRLA_SetWidth,ICON_W,ICONCTRLA_SetHeight,ICON_H,
  ICONCTRLA_SetPaletteSize1,ICON_COLORS,ICONCTRLA_SetPaletteSize2,ICON_COLORS,
  ICONCTRLA_SetPalette1,icon_palette,ICONCTRLA_SetPalette2,icon_palette,
  ICONCTRLA_SetImageData1,icon_pixels[0],ICONCTRLA_SetImageData2,icon_pixels[1],
  ICONCTRLA_SetTransparentColor1,0,ICONCTRLA_SetTransparentColor2,0,
  ICONCTRLA_SetFrameless,TRUE,ICONCTRLA_SetNewIconsSupport,FALSE,TAG_DONE);
 ok=PutIconTagList((STRPTR)path,d,NULL);
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
  if(d) {
   ULONG width=0,height=0,colors=0;LONG transparent=-1,frameless=0;UBYTE *pixels=NULL;
   IconControl(d,ICONCTRLA_GetWidth,&width,ICONCTRLA_GetHeight,&height,
    ICONCTRLA_GetPaletteSize1,&colors,ICONCTRLA_GetTransparentColor1,&transparent,
    ICONCTRLA_GetFrameless,&frameless,ICONCTRLA_GetImageData1,&pixels,TAG_DONE);
   printf("Colour icon: %lux%lu colours=%lu transparent=%ld frameless=%ld\n",width,height,colors,transparent,frameless);
   ok=ok && width==ICON_W && height==ICON_H && colors>1 && colors<=ICON_COLORS && transparent==0 && frameless && pixels;
   FreeDiskObject(d);
  }
  d=GetDiskObject(root);
  ok=ok && d && d->do_Type==WBDRAWER;if(d)FreeDiskObject(d);
  CloseLibrary(IconBase);puts(ok?"Workbench icon validation: PASS":"Workbench icon validation: FAIL");return ok?0:20;
 }
 if(argc==2 && !strcmp(argv[1],"--make-icons")) {
  struct DiskObject *d;
  IconBase=OpenLibrary("icon.library",44);if(!IconBase)return 20;
  artwork(0);artwork(1);fallback_artwork(0);fallback_artwork(1);snprintf(target,sizeof(target),"%s/Termic Amiga",root);
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
