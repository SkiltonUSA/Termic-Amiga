/* Bounded, local-only reader for the AmigaGuide subset in Termic.guide.
 * Link targets are node IDs, never commands, files or URLs. */
#ifndef TERMIC_GUIDE_DOCUMENT_H
#define TERMIC_GUIDE_DOCUMENT_H
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#define GUIDE_LIMIT 131072
#define GUIDE_NODES 64
#define GUIDE_LINKS 64
struct GuideNode {char id[64],title[128];char *body;};
struct GuideDocument {char *storage;int count;struct GuideNode nodes[GUIDE_NODES];};
struct GuideLink {char title[128];int target;};
static int guide_equal(const char *a,const char *b)
{
 while(*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b)){a++;b++;}
 return !*a && !*b;
}
static int guide_find(const struct GuideDocument *doc,const char *id)
{
 int i;for(i=0;i<doc->count;i++)if(guide_equal(doc->nodes[i].id,id))return i;return -1;
}
static void guide_free(struct GuideDocument *doc)
{
 free(doc->storage);memset(doc,0,sizeof(*doc));
}
static int guide_token(const char **input,char *out,size_t size)
{
 const char *p=*input;size_t n=0;int quoted;
 while(*p==' ' || *p=='\t')p++;
 quoted=*p=='"';if(quoted)p++;
 while(*p && (quoted?*p!='"':!isspace((unsigned char)*p))) {
  if((unsigned char)*p<32 || n+1>=size)return 0;
  out[n++]=*p++;
 }
 if(quoted && *p++!='"')return 0;
 out[n]=0;*input=p;return n!=0;
}
static int guide_parse(struct GuideDocument *doc,const char *text)
{
 struct GuideDocument next;char *p,*end,*line;int active=-1;size_t size;
 memset(&next,0,sizeof(next));if(!text || (size=strlen(text))>GUIDE_LIMIT)return 0;
 next.storage=malloc(size+1);if(!next.storage)return 0;memcpy(next.storage,text,size+1);
 p=next.storage;
 while(*p) {
  const char *s;char command[32],id[64],title[128],header[256];size_t n;
  line=p;end=strchr(p,'\n');p=end?end+1:p+strlen(p);n=(size_t)((end?end:p)-line);
  if(n && line[n-1]=='\r')n--;
  if(!n || *line!='@' || (n>1 && line[1]=='{'))continue;
  if(n>=sizeof(header))goto fail;
  memcpy(header,line,n);header[n]=0;s=header;
  if(!guide_token(&s,command,sizeof(command)))goto fail;
  if(guide_equal(command,"@NODE")) {
   if(active>=0 || next.count==GUIDE_NODES || !end ||
      !guide_token(&s,id,sizeof(id)) || !guide_token(&s,title,sizeof(title)) || guide_find(&next,id)>=0)goto fail;
   active=next.count++;strcpy(next.nodes[active].id,id);strcpy(next.nodes[active].title,title);next.nodes[active].body=p;
  } else if(guide_equal(command,"@ENDNODE")) {
   if(active<0)goto fail;
   *line=0;active=-1;
  } else if(active>=0)goto fail;
 }
 if(active>=0 || guide_find(&next,"MAIN")<0)goto fail;
 guide_free(doc);*doc=next;return 1;
fail:guide_free(&next);return 0;
}
/* The rendered body is at most as long as its source. Formatting directives
 * disappear; local link labels remain, with targets exposed separately. */
static char *guide_render(const struct GuideDocument *doc,int node,struct GuideLink *links,int *count)
{
 const char *p,*end;char *result,*out;int n=0;
 *count=0;if(node<0 || node>=doc->count)return NULL;
 p=doc->nodes[node].body;result=malloc(strlen(p)+1);if(!result)return NULL;out=result;
 while(*p) {
  if(p[0]=='@' && p[1]=='{') {
   char tag[256],title[128],kind[32],target[64];const char *s;size_t size;
   end=strchr(p+2,'}');if(!end || (size=(size_t)(end-p-2))>=sizeof(tag))goto fail;
   memcpy(tag,p+2,size);tag[size]=0;s=tag;
   if(*s=='"') {
    int index;
    if(!guide_token(&s,title,sizeof(title)) || !guide_token(&s,kind,sizeof(kind)) ||
       !guide_equal(kind,"LINK") || !guide_token(&s,target,sizeof(target)) || (index=guide_find(doc,target))<0 || n==GUIDE_LINKS)goto fail;
    while(isspace((unsigned char)*s))s++;if(*s)goto fail;
    strcpy(links[n].title,title);links[n++].target=index;
    size=strlen(title);memcpy(out,title,size);out+=size;
   } else if(!guide_equal(tag,"b") && !guide_equal(tag,"ub") &&
             !guide_equal(tag,"i") && !guide_equal(tag,"ui") &&
             !guide_equal(tag,"u") && !guide_equal(tag,"uu"))goto fail;
   p=end+1;
  } else {
   unsigned char c=(unsigned char)*p++;
   if(c!='\r')*out++=(c<32 && c!='\n' && c!='\t') || c==127?'?':c;
  }
 }
 *out=0;*count=n;return result;
fail:free(result);return NULL;
}
#endif
