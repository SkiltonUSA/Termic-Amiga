/* Host-only acceptance against the upstream AmiPkg parser/resolver/recipe core. */
#include "aindex.h"
#include "ajson.h"
#include "arecipe.h"
#include "arun.h"
#include "aver.h"
#include "resolve.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char *read_all(const char *name)
{
    FILE *f=fopen(name,"rb");long n;char *s;assert(f);
    assert(!fseek(f,0,SEEK_END));n=ftell(f);assert(n>=0);rewind(f);
    s=malloc((size_t)n+1);assert(s);assert(fread(s,1,(size_t)n,f)==(size_t)n);s[n]=0;fclose(f);return s;
}
int main(int argc,char **argv)
{
    aidx_index index;ares_result result={0},missing={0};const aidx_entry *root;
    char *json,*paths,*line,*next;const char *entries[512];int dirs[512]={0};size_t n=0,i,j;
    aj_node *doc;const aj_node *entry;arecipe recipe;arun_plan *plan;
    assert(argc==3);json=read_all(argv[1]);assert(!aidx_parse(json,&index));
    root=aidx_find(&index,"termic-amiga");assert(root && root->dep_count==2);
    ares_resolve(&index,"termic-amiga","68060",&result);
    assert(!result.missing_count && result.count==3 && result.ordered[2]==root);
    for(i=0;i<root->dep_count;i++) {
        const aidx_entry *dep=aidx_find(&index,root->deps[i].id);assert(dep);
        /* Resolver orders IDs; explicitly check each requested minimum too. */
        assert(aver_compare(aidx_comparable_version(dep),root->deps[i].min)>=0);
        for(j=0;j<result.count && result.ordered[j]!=dep;j++) {}
        assert(j<2);
    }
    assert(ares_cpu_satisfies("68060",root->min_cpu));
    assert(!ares_cpu_satisfies("68000",root->min_cpu));
    assert(!strcmp(root->min_ks,"3.2"));
    /* The root is first in our generated index. Removing deps must be detected. */
    index.count=1;ares_resolve(&index,"termic-amiga","68060",&missing);
    assert(missing.missing_count==2);index.count=3;
    doc=ajson_parse(json);assert(doc);entry=ajson_get(doc,"packages")->child;
    assert(!arecipe_parse(entry,&recipe) && recipe.schema==2 && !recipe.has_unknown);
    assert(recipe.ops[0].type==AROP_PRE_SCRIPT);
    assert(strstr(recipe.ops[0].script,"Version muimaster.library 21"));
    paths=read_all(argv[2]);line=paths;
    while(*line) {
        next=strchr(line,'\n');if(next)*next++=0;
        if(*line){assert(n<512);entries[n++]=line;}
        if(!next)break;line=next;
    }
    plan=calloc(1,sizeof(*plan));assert(plan);
    assert(!arun_plan_build(&recipe,entries,dirs,n,plan) && !plan->overflow && plan->count==n);
    for(i=0;i<plan->count;i++) {
        assert(plan->ops[i].kind==ARUN_COPY);
        assert(!strncmp(plan->ops[i].dest,"Programs/Termic-Amiga/",21) ||
               !strcmp(plan->ops[i].dest,"Programs/Termic-Amiga.info"));
        printf("COPY\t%s\t%s\n",plan->ops[i].src,plan->ops[i].dest);
    }
    puts("AmiPkg parser, dependency order/minima, CPU floor, missing-dependency rejection and file plan: PASS");
    free(plan);free(paths);ajson_free(doc);aidx_free(&index);free(json);return 0;
}
