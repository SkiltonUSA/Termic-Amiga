#include "review_diff.h"
#include <assert.h>
static void check(const char *before,const char *after,int expected_coarse)
{
 int coarse;char *diff=review_unified(before,after,"fixture.c",&coarse),*p,*end;
 char old[32770],newtext[32770],previous=0;size_t on=0,nn=0,n;int i;
 assert(diff);if(expected_coarse>=0)assert(coarse==expected_coarse);
 if(!strcmp(before,after)){assert(!strcmp(diff,"No changes in this file.\n"));free(diff);return;}
 p=diff;for(i=0;i<3;i++){p=strchr(p,'\n');assert(p);++p;}
 while(*p) {
  end=strchr(p,'\n');assert(end);n=end-p;
  if(*p=='\\') {
   assert(!strncmp(p,"\\ No newline at end of file",n));
   if(previous!='+'){assert(on && old[on-1]=='\n');--on;}
   if(previous!='-'){assert(nn && newtext[nn-1]=='\n');--nn;}
  } else {
   assert(*p==' ' || *p=='+' || *p=='-');previous=*p;
   if(*p!='+'){assert(on+n<sizeof(old));memcpy(old+on,p+1,n);on+=n;}
   if(*p!='-'){assert(nn+n<sizeof(newtext));memcpy(newtext+nn,p+1,n);nn+=n;}
  }
  p=end+1;
 }
 old[on]=newtext[nn]=0;
 assert(!strcmp(old,before));assert(!strcmp(newtext,after));free(diff);
}
int main(void)
{
 char a[32769],b[32769];unsigned seed=19;int i,j,k,coarse;char *s;
 check("","",0);check("","new\n",0);check("old\n","",0);
 check("first\nold\nlast\n","first\nnew\nlast\n",0);
 check("same\n","same",0);check("same","same\n",0);check("a","b",0);
 check("\n\n","\n",0);check("a\r\nb\n","a\nb\n",0);
 s=review_unified("keep\nremove\nlast\n","keep\nadd\nlast\n","fixture.c",&coarse);
 assert(s && strstr(s," keep\n-remove\n+add\n last\n"));free(s);
 for(i=0;i<250;i++) {
  for(k=0;k<2;k++) {
   char *w=k?b:a;int lines=(i*17+k*7)%90;
   for(j=0;j<lines;j++){seed=seed*1664525U+1013904223U;*w++='a'+(seed%7);*w++='\n';}
   if(i%3==0 && w>(k?b:a))--w;*w=0;
  }
  check(a,b,0);
 }
 for(i=0;i<32768;i++){a[i]=(i&1)?'\n':'a';b[i]=(i&1)?'\n':'b';}
 a[32768]=b[32768]=0;check(a,b,1);
 memset(a,'x',32768);memset(b,'y',32768);a[32768]=b[32768]=0;check(a,b,0);
 assert(!review_unified(NULL,b,"fixture.c",&coarse));
 puts("Unified review: reconstruction, repeated lines, missing newlines and bounded large changes PASS");
 return 0;
}
