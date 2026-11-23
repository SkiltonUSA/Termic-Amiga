/* Shared-palette colours for TextEditor: no exclusive RGB pen is required. */
#ifndef TERMIC_EDITOR_COLORS_H
#define TERMIC_EDITOR_COLORS_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define EDITOR_COLOR_COUNT 10
static const unsigned long editor_rgb[EDITOR_COLOR_COUNT]={
 0xe6edf3UL,0xa6afbdUL,0x79c8ffUL,0xf2b66dUL,0xff7b8bUL,
 0x8fe3a4UL,0xdda0ffUL,0xffb86cUL,0x8bd5ffUL,0xffcc80UL
};
static unsigned editor_color_index(unsigned long rgb)
{
 unsigned i,best=0;unsigned long distance=~0UL;
 for(i=0;i<EDITOR_COLOR_COUNT;i++) {
  long r=(long)((rgb>>16)&255)-(long)((editor_rgb[i]>>16)&255);
  long g=(long)((rgb>>8)&255)-(long)((editor_rgb[i]>>8)&255);
  long b=(long)(rgb&255)-(long)(editor_rgb[i]&255);
  unsigned long d=(unsigned long)(r*r+g*g+b*b);
  if(d<distance){best=i;distance=d;}
 }
 return best+1; /* TextEditor colour-map indices start at one. */
}
static int editor_hex(char c)
{
 if(c>='0' && c<='9')return c-'0';
 if(c>='a' && c<='f')return c-'a'+10;
 if(c>='A' && c<='F')return c-'A'+10;
 return -1;
}
static char *editor_color_text(const char *input,int dark)
{
 size_t n,i=0,j=0;char *out;
 if(!input)return NULL;
 n=strlen(input);if(n>((size_t)-1-16)/7)return NULL;
 out=malloc(n*7+16);if(!out)return NULL;
 if(dark){memcpy(out,"\033p[1]",5);j=5;}
 while(i<n) {
  if(n-i>=10 && input[i]==27 && input[i+1]=='P' && input[i+2]=='[' && input[i+9]==']') {
   unsigned k;unsigned long rgb=0;
   for(k=0;k<6;k++){int h=editor_hex(input[i+3+k]);if(h<0)break;rgb=(rgb<<4)|(unsigned)h;}
   if(k==6){j+=(size_t)snprintf(out+j,n*7+16-j,"\033p[%u]",editor_color_index(rgb));i+=10;continue;}
  }
  out[j++]=input[i];
  if(input[i++]=='\n' && dark && i<n){memcpy(out+j,"\033p[1]",5);j+=5;}
 }
 out[j]=0;return out;
}
#endif
