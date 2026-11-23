#include "../workbench_art.h"
#include <assert.h>
#include <stdio.h>
int main(int argc,char **argv)
{
 unsigned state,i,opaque=0;FILE *f;
 artwork(0);artwork(1);
 assert(memcmp(icon_pixels[0],icon_pixels[1],sizeof(icon_pixels[0])));
 for(state=0;state<2;state++)for(i=0;i<ICON_W*ICON_H;i++) {
  assert(icon_pixels[state][i]<ICON_COLORS);
  if(icon_pixels[state][i])opaque++;
 }
 assert(opaque>ICON_W*ICON_H && opaque<2*ICON_W*ICON_H);
 assert(!icon_pixels[0][0] && !icon_pixels[1][ICON_W*ICON_H-1]);
 if(argc==2) {
  f=fopen(argv[1],"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",ICON_W*2,ICON_H);
  for(i=0;i<ICON_H;i++)for(state=0;state<2;state++) {
   unsigned x;for(x=0;x<ICON_W;x++) {
    unsigned color=icon_pixels[state][i*ICON_W+x];
    const unsigned char bg[]={170,170,170};
    assert(fwrite(color?icon_palette[color]:bg,1,3,f)==3);
   }
  }
  assert(!fclose(f));
 }
 puts("Workbench colour artwork: normal/selected bounds and transparency PASS");return 0;
}
