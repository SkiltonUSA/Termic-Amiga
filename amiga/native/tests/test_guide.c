#include "../guide_document.h"
#include <stdio.h>
#include <assert.h>
static const char valid[]="@DATABASE Demo.guide\n@NODE MAIN \"Contents\"\n@{b}Hello@{ub}\n@{\"Next topic\" LINK next}\n@ENDNODE\n@NODE Next \"Second chapter\"\nSafe text\n@ENDNODE\n";
int main(int argc,char **argv)
{
 struct GuideDocument doc;struct GuideLink links[GUIDE_LINKS];char *text,*render;int n,i;FILE *f;long size;
 memset(&doc,0,sizeof(doc));assert(guide_parse(&doc,valid));assert(doc.count==2 && guide_find(&doc,"next")==1);
 render=guide_render(&doc,0,links,&n);assert(render && !strcmp(render,"Hello\nNext topic\n") && n==1 && links[0].target==1);free(render);
 assert(!guide_parse(&doc,"@NODE MAIN \"Missing end\"\nbody"));assert(doc.count==2);
 assert(!guide_parse(&doc,"@NODE MAIN \"A\"\n@NODE X \"B\"\n@ENDNODE\n"));
 assert(!guide_parse(&doc,"@NODE MAIN \"A\"\n@ENDNODE\n@NODE main \"B\"\n@ENDNODE\n"));
 assert(!guide_parse(&doc,"@NODE X \"No contents\"\n@ENDNODE\n"));
 assert(guide_parse(&doc,"@NODE MAIN \"A\"\n@{\"Unsafe\" SYSTEM execute}\n@ENDNODE\n"));assert(!guide_render(&doc,0,links,&n));
 assert(guide_parse(&doc,"@NODE MAIN \"A\"\n@{\"Missing\" LINK nope}\n@ENDNODE\n"));assert(!guide_render(&doc,0,links,&n));
 assert(guide_parse(&doc,"@NODE MAIN \"A\"\n@{\"Broken LINK MAIN}\n@ENDNODE\n"));assert(!guide_render(&doc,0,links,&n));
 assert(guide_parse(&doc,"@NODE MAIN \"A\"\ntext\33P[FF0000]\r\n@ENDNODE\n"));render=guide_render(&doc,0,links,&n);assert(render && !strchr(render,27) && !strchr(render,'\r'));free(render);
 text=malloc(GUIDE_LIMIT+2);memset(text,'x',GUIDE_LIMIT+1);text[GUIDE_LIMIT+1]=0;assert(!guide_parse(&doc,text));free(text);
 assert(argc==2);f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size=ftell(f);rewind(f);assert(size>0 && size<=GUIDE_LIMIT);
 text=malloc(size+1);assert(text && fread(text,1,size,f)==(size_t)size);text[size]=0;fclose(f);
 assert(guide_parse(&doc,text));free(text);assert(guide_find(&doc,"TINY")>=0 && guide_find(&doc,"activitymonitor")>=0);
 for(i=0;i<doc.count;i++){render=guide_render(&doc,i,links,&n);assert(render && !strstr(render,"@{") && n<=GUIDE_LINKS);free(render);}
 printf("GUIDE_PASS: %d shipped chapters, all local links resolved\n",doc.count);guide_free(&doc);assert(!doc.storage && !doc.count);return 0;
}
