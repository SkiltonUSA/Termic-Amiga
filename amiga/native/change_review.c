#include "change_review.h"
#include "workspace.h"
#include "native.h"
#include "vendor/cJSON.h"
#include <stdio.h>
#include <errno.h>
#if defined(__AMIGA__) || defined(__VBCC__)
#include <proto/dos.h>
#endif
#include <stdlib.h>
#include <string.h>
static int path_at(char *out,const char *root,const char *name)
{
    if(!root || strlen(root)+strlen(name)+2>=500)return 0;
    sprintf(out,"%s%s%s",root,*root && root[strlen(root)-1]!='/' && root[strlen(root)-1]!=':'?"/":"",name);
    return 1;
}
static int file_exists(const char *path)
{
#if defined(__AMIGA__) || defined(__VBCC__)
    BPTR lock=Lock((STRPTR)path,ACCESS_READ);
    if(lock){UnLock(lock);return 1;}
    return IoErr()!=ERROR_OBJECT_NOT_FOUND && IoErr()!=ERROR_DIR_NOT_FOUND;
#else
    FILE *f=fopen(path,"rb");if(!f)return errno!=ENOENT;fclose(f);return 1;
#endif
}
static int snapshot_path(char *out,const char *root,int slot,int after)
{
    char name[40];sprintf(name,".review-%02d.%s",slot,after?"after":"before");return path_at(out,root,name);
}
int review_load(struct ChangeReview *review,const char *root)
{
    char path[512],*text;const char *end;cJSON *doc,*items,*item;int i,ok=0;
    memset(review,0,sizeof(*review));if(!path_at(path,root,".review.json"))return 0;
    text=read_text(path,32768);if(!text)return !file_exists(path);
    doc=cJSON_ParseWithOpts(text,&end,1);free(text);if(!doc)return 0;
    items=cJSON_GetObjectItem(doc,"files");
    if(!cJSON_IsArray(items) || cJSON_GetArraySize(items)>REVIEW_MAX)goto done;
    cJSON_ArrayForEach(item,items) {
        cJSON *name=cJSON_GetObjectItem(item,"name"),*slot=cJSON_GetObjectItem(item,"slot"),
            *existed=cJSON_GetObjectItem(item,"existed");struct ReviewEntry *row=&review->entries[review->count];
        if(!cJSON_IsString(name) || !agent_source_name(name->valuestring) || strlen(name->valuestring)>=sizeof(row->name) ||
           !cJSON_IsNumber(slot) || slot->valuedouble!=slot->valueint || slot->valueint<0 ||
           slot->valueint>=REVIEW_MAX || !cJSON_IsBool(existed))goto done;
        for(i=0;i<review->count;i++)if(review->entries[i].slot==slot->valueint ||
            workspace_name_equal(review->entries[i].name,name->valuestring))goto done;
        strcpy(row->name,name->valuestring);row->slot=slot->valueint;row->existed=cJSON_IsTrue(existed);review->count++;
    }
    ok=1;
done:cJSON_Delete(doc);if(!ok)review->count=0;return ok;
}
static int review_save(const struct ChangeReview *review,const char *root)
{
    char path[512],*text;cJSON *doc=cJSON_CreateObject(),*items;int i,ok=0;
    if(!doc || !path_at(path,root,".review.json")){cJSON_Delete(doc);return 0;}
    items=cJSON_AddArrayToObject(doc,"files");if(!items){cJSON_Delete(doc);return 0;}
    for(i=0;i<review->count;i++) {
        const struct ReviewEntry *row=&review->entries[i];cJSON *item=cJSON_CreateObject();
        if(!item){cJSON_Delete(doc);return 0;}
        cJSON_AddItemToArray(items,item);
        if(!cJSON_AddStringToObject(item,"name",row->name) || !cJSON_AddNumberToObject(item,"slot",row->slot) ||
           !cJSON_AddBoolToObject(item,"existed",row->existed)){cJSON_Delete(doc);return 0;}
    }
    text=cJSON_PrintUnformatted(doc);if(text)ok=save_text(path,text,32768);
    free(text);cJSON_Delete(doc);return ok;
}
int review_write(const char *root,const char *name,const char *text)
{
    struct ChangeReview review;struct ReviewEntry *row;char path[512],before[512],after[512],*original,*expected;
    int i,j,slot=0,ok=0;
    if(!agent_source_name(name) || !text || strlen(text)>SOURCE_LIMIT ||
       !path_at(path,root,name) || !review_load(&review,root))return 0;
    original=read_text(path,SOURCE_LIMIT);if(!original && file_exists(path))return 0;
    for(i=0;i<review.count;i++)if(workspace_name_equal(review.entries[i].name,name))break;
    if(i==review.count) {
        if(review.count>=REVIEW_MAX)goto done;
        for(slot=0;slot<REVIEW_MAX;slot++) {
            for(j=0;j<review.count;j++)if(review.entries[j].slot==slot)break;
            if(j==review.count)break;
        }
        row=&review.entries[review.count++];strcpy(row->name,name);row->slot=slot;row->existed=original!=NULL;
        if(!snapshot_path(before,root,slot,0) || !save_text(before,original?original:"",SOURCE_LIMIT))goto done;
    } else {
        row=&review.entries[i];
        if(!snapshot_path(after,root,row->slot,1))goto done;
        expected=read_text(after,SOURCE_LIMIT);
        ok=expected && original && !strcmp(expected,original);free(expected);
        if(!ok)goto done;ok=0;
    }
    /* Publish expected result and journal first. Interrupted writes become conflicts,
     * never permission to replace a later user edit during Revert. */
    if(!snapshot_path(after,root,row->slot,1) || !save_text(after,text,SOURCE_LIMIT) || !review_save(&review,root))goto done;
    ok=save_text(path,text,SOURCE_LIMIT);
done:free(original);return ok;
}
int review_resolve(const char *root,int index,int revert)
{
    struct ChangeReview review;struct ReviewEntry row;char path[512],snapshot[512],*current,*expected,*before;
    int ok=0;
    if(!review_load(&review,root) || index<0 || index>=review.count)return 0;
    row=review.entries[index];if(!path_at(path,root,row.name) || !snapshot_path(snapshot,root,row.slot,1))return 0;
    current=read_text(path,SOURCE_LIMIT);expected=read_text(snapshot,SOURCE_LIMIT);
    if(!expected)goto done;
    if(revert) {
        int already_restored;
        if(!snapshot_path(snapshot,root,row.slot,0))goto done;
        before=read_text(snapshot,SOURCE_LIMIT);if(!before)goto done;
        already_restored=row.existed?current && !strcmp(current,before):!current && !file_exists(path);
        if(already_restored) {
            /* A previous revert may have restored the file but failed to save
             * the journal. Verify its backup before completing that operation. */
            char backup[512],*saved;snprintf(backup,sizeof(backup),"%s.bak",path);
            saved=read_text(backup,SOURCE_LIMIT);ok=saved && !strcmp(saved,expected);free(saved);
        } else if(current && !strcmp(current,expected)) {
            if(row.existed)ok=save_text(path,before,SOURCE_LIMIT);
            else {
                char backup[512];snprintf(backup,sizeof(backup),"%s.bak",path);
                remove(backup);ok=rename(path,backup)==0;
            }
        }
        free(before);if(!ok)goto done;ok=0;
    } else if(!current || strcmp(current,expected))goto done;
    memmove(review.entries+index,review.entries+index+1,(review.count-index-1)*sizeof(row));review.count--;
    ok=review_save(&review,root);
done:free(current);free(expected);return ok;
}
char *review_preview(const char *root,int index)
{
    struct ChangeReview review;struct ReviewEntry row;char path[512],*before,*after,*result;size_t size;
    if(!review_load(&review,root) || index<0 || index>=review.count)return NULL;
    row=review.entries[index];if(!snapshot_path(path,root,row.slot,0))return NULL;
    before=read_text(path,SOURCE_LIMIT);if(!snapshot_path(path,root,row.slot,1)){free(before);return NULL;}
    after=read_text(path,SOURCE_LIMIT);if(!before || !after){free(before);free(after);return NULL;}
    size=strlen(before)+strlen(after)+512;result=malloc(size);
    if(result)snprintf(result,size,"File: %s\n\nBEFORE%s\n%s\n\nAFTER\n%s",row.name,row.existed?"":" (new file)",before,after);
    free(before);free(after);return result;
}
