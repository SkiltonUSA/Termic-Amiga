/* Test-only native input sequence; coordinates are supplied by the UI test. */
#include <exec/types.h>
#include <exec/io.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <stdlib.h>
#include <string.h>
struct IntuitionBase *IntuitionBase;
static struct IOStdReq *io;
static int send(UWORD code,UWORD qualifier,WORD x,WORD y)
{
 struct InputEvent event;memset(&event,0,sizeof(event));
 event.ie_Class=IECLASS_RAWMOUSE;event.ie_Code=code;
 event.ie_Qualifier=qualifier|IEQUALIFIER_RELATIVEMOUSE;
 event.ie_X=x;event.ie_Y=y;
 io->io_Command=IND_WRITEEVENT;io->io_Data=&event;io->io_Length=sizeof(event);
 return DoIO((struct IORequest *)io)==0;
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
 if(OpenDevice("input.device",0,(struct IORequest *)io,0))goto done;
 if(!send(IECODE_NOBUTTON,0,tx-x,ty-y))goto close;
 Delay(10);if(!send(IECODE_RBUTTON,IEQUALIFIER_RBUTTON,0,0))goto close;
 Delay(20);send(IECODE_NOBUTTON,IEQUALIFIER_RBUTTON,ix-tx,iy-ty);
 Delay(30);send(IECODE_RBUTTON|IECODE_UP_PREFIX,0,0,0);result=0;
close:
 send(IECODE_RBUTTON|IECODE_UP_PREFIX,0,0,0);CloseDevice((struct IORequest *)io);
done:
 if(io)DeleteIORequest((struct IORequest *)io);if(port)DeleteMsgPort(port);
 CloseLibrary((struct Library *)IntuitionBase);return result;
}
