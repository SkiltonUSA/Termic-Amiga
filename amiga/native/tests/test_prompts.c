#include "prompts.h"
#include "native.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv)
{
    char *text; cJSON *defaults,*items,*delta,*stored,*again,*p,*q;
    assert(argc==2);text=read_text(argv[1],PROMPTS_LIMIT);assert(text);
    defaults=cJSON_Parse(text);free(text);assert(cJSON_GetArraySize(defaults)==17);
    items=prompts_resolve(defaults,NULL);assert(cJSON_GetArraySize(items)==17);
    delta=prompts_delta(defaults,items);assert(delta);
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(delta,"customs"))==0);
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(delta,"overrides"))==0);
    cJSON_Delete(delta);
    p=cJSON_GetArrayItem(items,0);
    cJSON_ReplaceItemInObjectCaseSensitive(p,"body",cJSON_CreateString("Custom review body"));
    cJSON_ReplaceItemInObjectCaseSensitive(p,"enabled",cJSON_CreateBool(0));
    cJSON_DeleteItemFromArray(items,1);
    p=cJSON_CreateObject();cJSON_AddStringToObject(p,"id","custom:1");
    cJSON_AddStringToObject(p,"title","Fixture prompt");cJSON_AddStringToObject(p,"body","Fixture task");
    cJSON_AddBoolToObject(p,"enabled",1);cJSON_InsertItemInArray(items,0,p);
    delta=prompts_delta(defaults,items);assert(delta);
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(delta,"overrides"))==1);
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(delta,"deletedBuiltins"))==1);
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(delta,"disabled"))==1);
    text=cJSON_PrintUnformatted(delta);stored=cJSON_Parse(text);free(text);
    /* An upgraded unedited default changes; an override remains user's text. */
    p=prompt_find(defaults,"builtin:write-tests");
    cJSON_ReplaceItemInObjectCaseSensitive(p,"body",cJSON_CreateString("Updated shipped test guidance"));
    again=prompts_resolve(defaults,stored);assert(again);
    assert(!strcmp(prompt_string(cJSON_GetArrayItem(again,0),"id"),"custom:1"));
    assert(!prompt_find(again,"builtin:deep-review"));
    p=prompt_find(again,"builtin:review");assert(!strcmp(prompt_string(p,"body"),"Custom review body"));
    assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(p,"enabled")));
    assert(!strcmp(prompt_string(prompt_find(again,"builtin:write-tests"),"body"),"Updated shipped test guidance"));
    cJSON_Delete(again);
    q=cJSON_GetObjectItemCaseSensitive(stored,"customs");cJSON_AddItemToArray(q,cJSON_Duplicate(cJSON_GetArrayItem(q,0),1));
    assert(!prompts_resolve(defaults,stored));
    cJSON_DeleteItemFromArray(q,1);p=cJSON_GetArrayItem(q,0);
    cJSON_ReplaceItemInObjectCaseSensitive(p,"body",cJSON_CreateString("bad\033control"));
    assert(!prompts_resolve(defaults,stored));
    cJSON_Delete(delta);cJSON_Delete(stored);cJSON_Delete(items);cJSON_Delete(defaults);
    return 0;
}
