/* Test-only native left-button gesture; coordinates are supplied by the UI test. */
#include <exec/types.h>
#include <exec/io.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
struct IntuitionBase *IntuitionBase;
static struct IOStdReq *io;
static int send(UWORD code,UWORD qualifier,WORD x,WORD y)
{
 struct InputEvent event;memset(&event,0,sizeof(event));
 event.ie_Class=IECLASS_RAWMOUSE;event.ie_Code=code;
 /* RAWMOUSE coordinates are already relative. RELATIVEMOUSE belongs to
  * NEWPOINTERPOS pixel events; it must not leak into MUI modifier matching. */
 event.ie_Qualifier=qualifier;
 event.ie_X=x;event.ie_Y=y;
 CurrentTime(&event.ie_TimeStamp.tv_secs,&event.ie_TimeStamp.tv_micro);
 io->io_Command=IND_WRITEEVENT;io->io_Data=&event;io->io_Length=sizeof(event);
 {int rc=DoIO((struct IORequest *)io);if(rc)printf("Input event failed: %d\n",rc);return rc==0;}
}
int main(int argc,char **argv)
{
 struct MsgPort *port=NULL;struct Screen *screen;int result=20;WORD x,y,tx,ty,ix,iy;
 if(argc!=5)return 20;
 tx=atoi(argv[1]);ty=atoi(argv[2]);ix=atoi(argv[3]);iy=atoi(argv[4]);
 IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37);
 if(!IntuitionBase)return 20;
 screen=LockPubScreen(NULL);if(!screen)goto done;
 x=screen->MouseX;y=screen->MouseY;UnlockPubScreen(NULL,screen);
 port=CreateMsgPort();if(!port)goto done;
 io=(struct IOStdReq *)CreateIORequest(port,sizeof(*io));if(!io)goto done;
 if(OpenDevice("input.device",0,(struct IORequest *)io,0)){puts("Cannot open input.device");goto done;}
 if(!send(IECODE_NOBUTTON,0,tx-x,ty-y))goto close;
 Delay(10);if(!send(IECODE_LBUTTON,IEQUALIFIER_LEFTBUTTON,0,0))goto close;
 Delay(20);
 {int step;WORD px=tx,py=ty,nx,ny;
  for(step=1;step<=12;step++) {
   nx=tx+(ix-tx)*step/12;ny=ty+(iy-ty)*step/12;
   send(IECODE_NOBUTTON,IEQUALIFIER_LEFTBUTTON,nx-px,ny-py);px=nx;py=ny;Delay(2);
  }}
 Delay(30);send(IECODE_LBUTTON|IECODE_UP_PREFIX,0,0,0);result=0;
close:
 send(IECODE_LBUTTON|IECODE_UP_PREFIX,0,0,0);CloseDevice((struct IORequest *)io);
done:
 if(io)DeleteIORequest((struct IORequest *)io);if(port)DeleteMsgPort(port);
 CloseLibrary((struct Library *)IntuitionBase);puts(result?"INPUT_GESTURE_FAIL":"INPUT_GESTURE_PASS");return result;
}
