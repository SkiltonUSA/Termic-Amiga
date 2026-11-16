#ifndef TERMIC_REVIEW_DIFF_H
#define TERMIC_REVIEW_DIFF_H
/* Full-context unified review. Bound quadratic work on the 68k; a large
 * changed region is represented as a replacement, without dropping bytes. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define REVIEW_DIFF_CELLS 262144UL
struct DiffLine {const char *text;size_t length;unsigned long hash;};
static struct DiffLine *diff_lines(const char *s,size_t *count)
{
 const char *p=s,*start=s;size_t n=0,i=0;struct DiffLine *lines;unsigned long hash=2166136261UL;
 while(*p)if(*p++=='\n')++n;
 if(p>s && p[-1]!='\n')++n;
 lines=malloc((n+1)*sizeof(*lines));if(!lines)return NULL;
 for(p=s;*p;++p) {
  hash=(hash^(unsigned char)*p)*16777619UL;
  if(*p=='\n') {
   lines[i].text=start;lines[i].length=p+1-start;lines[i++].hash=hash;
   start=p+1;hash=2166136261UL;
  }
 }
 if(p>start){lines[i].text=start;lines[i].length=p-start;lines[i++].hash=hash;}
 *count=i;return lines;
}
static int diff_equal(const struct DiffLine *a,const struct DiffLine *b)
{
 return a->hash==b->hash && a->length==b->length && !memcmp(a->text,b->text,a->length);
}
static void diff_emit(char **out,char mark,const struct DiffLine *line)
{
 char *p=*out;*p++=mark;memcpy(p,line->text,line->length);p+=line->length;
 if(line->length && line->text[line->length-1]!='\n') {
  const char *note="\n\\ No newline at end of file\n";
  strcpy(p,note);p+=strlen(note);
 }
 *out=p;
}
static char *review_unified(const char *before,const char *after,const char *name,int *coarse)
{
 struct DiffLine *a=NULL,*b=NULL;unsigned short *grid=NULL;
 size_t na=0,nb=0,prefix=0,suffix=0,an,bn,i,j,stride,size;char *out=NULL,*w;
 *coarse=0;
 if(!before || !after || !name || strlen(before)>32768 || strlen(after)>32768 || strlen(name)>183)return NULL;
 a=diff_lines(before,&na);b=diff_lines(after,&nb);if(!a || !b)goto done;
 size=2*(strlen(before)+strlen(after))+1024;out=malloc(size);if(!out)goto done;
 if(!strcmp(before,after)){strcpy(out,"No changes in this file.\n");goto done;}
 while(prefix<na && prefix<nb && diff_equal(a+prefix,b+prefix))++prefix;
 while(suffix<na-prefix && suffix<nb-prefix && diff_equal(a+na-1-suffix,b+nb-1-suffix))++suffix;
 an=na-prefix-suffix;bn=nb-prefix-suffix;stride=bn+1;
 if((an+1)<=REVIEW_DIFF_CELLS/stride)grid=calloc((an+1)*stride,sizeof(*grid));
 if(!grid && an && bn)*coarse=1;
 if(grid)for(i=an;i>0;--i)for(j=bn;j>0;--j)
  grid[(i-1)*stride+j-1]=diff_equal(a+prefix+i-1,b+prefix+j-1)?
   grid[i*stride+j]+1:(grid[i*stride+j-1]>=grid[(i-1)*stride+j]?grid[i*stride+j-1]:grid[(i-1)*stride+j]);
 w=out;w+=snprintf(w,size,"--- a/%s\n+++ b/%s\n@@ -%lu,%lu +%lu,%lu @@\n",name,name,
  na?1UL:0UL,(unsigned long)na,nb?1UL:0UL,(unsigned long)nb);
 for(i=0;i<prefix;++i)diff_emit(&w,' ',a+i);
 i=j=0;
 while(i<an || j<bn) {
  if(i<an && j<bn && diff_equal(a+prefix+i,b+prefix+j)) {diff_emit(&w,' ',a+prefix+i);++i;++j;}
  else if(i<an && (j==bn || !grid || grid[(i+1)*stride+j]>=grid[i*stride+j+1]))diff_emit(&w,'-',a+prefix+i++);
  else diff_emit(&w,'+',b+prefix+j++);
 }
 for(i=na-suffix;i<na;++i)diff_emit(&w,' ',a+i);
 *w=0;
done:free(a);free(b);free(grid);return out;
}
#endif
