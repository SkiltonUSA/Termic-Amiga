/* Native palette-mapped Workbench artwork; index zero is transparent. */
#ifndef TERMIC_WORKBENCH_ART_H
#define TERMIC_WORKBENCH_ART_H
#include <string.h>
#define ICON_W 64
#define ICON_H 56
#define ICON_COLORS 24
static unsigned char icon_palette[ICON_COLORS][3]={
 {0,0,0},{31,39,54},{61,76,94},{109,132,150},
 {183,204,215},{239,248,250},{14,24,38},{22,40,58},
 {31,54,74},{43,72,93},{48,106,139},{63,150,188},
 {105,205,224},{173,236,245},{241,157,73},{255,201,113},
 {124,210,171},{159,170,192},{125,35,49},{190,45,58},
 {234,67,77},{255,127,132},{203,212,225},{255,255,255}
};
static unsigned char icon_pixels[2][ICON_W*ICON_H];
static void icon_dot(unsigned state,int x,int y,unsigned color)
{
 if(x>=0 && x<ICON_W && y>=0 && y<ICON_H)
  icon_pixels[state][y*ICON_W+x]=(unsigned char)color;
}
static void icon_rect(unsigned state,int x,int y,int w,int h,unsigned color)
{
 int a,b;for(b=y;b<y+h;b++)for(a=x;a<x+w;a++)icon_dot(state,a,b,color);
}
static void artwork(unsigned state)
{
 int x,y,dx,dy,d;
 memset(icon_pixels[state],0,sizeof(icon_pixels[state]));
 /* Offset back sheet and a bevelled, frameless terminal card. */
 icon_rect(state,9,9,51,37,1);icon_rect(state,8,6,50,38,2);
 icon_rect(state,7,5,49,1,4);icon_rect(state,6,6,1,36,3);
 icon_rect(state,4,7,49,38,1);icon_rect(state,3,8,51,36,2);
 icon_rect(state,4,7,48,1,state?13:5);icon_rect(state,3,8,1,35,state?12:4);
 icon_rect(state,5,9,46,32,6);icon_rect(state,5,9,46,7,state?11:10);
 icon_rect(state,6,10,44,1,state?13:12);
 icon_rect(state,5,16,46,1,1);icon_rect(state,5,17,46,2,7);
 icon_rect(state,5,37,46,4,7);icon_rect(state,5,41,46,2,3);
 icon_rect(state,5,41,46,1,4);
 /* Small window controls and a restrained title mark. */
 icon_rect(state,8,12,3,2,5);icon_rect(state,14,12,15,1,13);
 icon_rect(state,43,12,5,2,8);
 /* Orange command prompt, pale cursor, and coloured source lines. */
 for(y=0;y<6;y++) {
  icon_rect(state,10+y,21+y,2,2,15);
  icon_rect(state,10+y,31-y,2,2,14);
 }
 icon_rect(state,20,31,7,2,5);
 icon_rect(state,31,22,10,2,12);icon_rect(state,43,22,4,2,17);
 icon_rect(state,31,27,5,2,16);icon_rect(state,38,27,9,2,4);
 icon_rect(state,31,32,13,2,11);
 /* Shaded red/white Amiga checker sphere, offset over the card corner. */
 for(y=31;y<55;y++)for(x=38;x<63;x++) {
  dx=x-50;dy=y-42;d=dx*dx+dy*dy;
  if(d<=137)icon_dot(state,x+1,y+1,1);
 }
 for(y=30;y<54;y++)for(x=37;x<62;x++) {
  unsigned color;int red;
  dx=x-49;dy=y-41;d=dx*dx+dy*dy;
  if(d>132)continue;
  if(d>112){icon_dot(state,x,y,dy<0?4:2);continue;}
  /* Narrower checks near the sphere's edges imply curved longitude. */
  red=(((dx+11+(dy*dy)/32)/5+(dy+11)/5)&1);
  if(red)color=(dx+dy<-4)?21:((dx+dy>8)?18:((dx+dy>2)?19:20));
  else color=(dx+dy<-4)?23:((dx+dy>8)?3:((dx+dy>2)?22:5));
  icon_dot(state,x,y,color);
 }
 icon_rect(state,44,34,2,1,23);
}
#endif
