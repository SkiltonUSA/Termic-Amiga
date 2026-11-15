#include "diagnostics.h"
#include "workspace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int diagnostics_parse(const char *log,struct Diagnostic *rows,int limit)
{
    int count=0;const char *p=log,*end,*where,*quote,*stop,*colon;char path[184];long line;
    if(!p || !rows || limit<1)return 0;
    while(*p && count<limit) {
        size_t n;end=strchr(p,'\n');if(!end)end=p+strlen(p);line=0;*path=0;
        where=strstr(p," in line ");
        if(where && where<end) {
            char *after;line=strtol(where+9,&after,10);
            quote=strchr(after,'"');stop=quote?strchr(quote+1,'"'):NULL;
            if(quote && stop && stop<end && (n=stop-quote-1)<sizeof(path)) {memcpy(path,quote+1,n);path[n]=0;}
        } else {
            colon=memchr(p,':',end-p);
            if(colon && (n=colon-p)<sizeof(path)) {
                char *after;memcpy(path,p,n);path[n]=0;line=strtol(colon+1,&after,10);
                if(after>=end || *after!=':')line=0;
            }
        }
        if(line>0 && line<=1000000 && source_name(path)) {
            struct Diagnostic *row=&rows[count++];strcpy(row->path,path);row->line=line;
            n=end-p;if(n>=sizeof(row->message))n=sizeof(row->message)-1;
            memcpy(row->message,p,n);row->message[n]=0;
            for(n=0;row->message[n];n++)if((unsigned char)row->message[n]<32)row->message[n]=' ';
        }
        p=*end?end+1:end;
    }
    return count;
}
