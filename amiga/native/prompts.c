/* Adapted from Termic src/store/prompts.ts (AGPL-3.0-or-later).
 * See Prompts/provenance.json for the upstream revision and default bodies. */
#include "prompts.h"
#include <string.h>
const char *prompt_string(const cJSON *item,const char *name)
{
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(item,name);
    return cJSON_IsString(v)?v->valuestring:"";
}
cJSON *prompt_find(cJSON *items,const char *id)
{
    cJSON *p;
    cJSON_ArrayForEach(p,items) if(!strcmp(prompt_string(p,"id"),id)) return p;
    return NULL;
}
static int contains(cJSON *array,const char *id)
{
    cJSON *p;cJSON_ArrayForEach(p,array)
        if(cJSON_IsString(p) && !strcmp(p->valuestring,id))return 1;
    return 0;
}
static int printable(const char *s,int multiline)
{
    for(;*s;s++) if(((unsigned char)*s<32 && (!multiline || (*s!='\n' && *s!='\r' && *s!='\t'))) || (unsigned char)*s==127)return 0;
    return 1;
}
int prompt_valid(const cJSON *p)
{
    const char *id=prompt_string(p,"id"),*title=prompt_string(p,"title"),*body=prompt_string(p,"body");
    return *id && strlen(id)<64 && printable(id,0) && *title && strlen(title)<96 &&
        printable(title,0) && *body && strlen(body)<=8192 && printable(body,1);
}
cJSON *prompts_resolve(cJSON *defaults,cJSON *stored)
{
    cJSON *items=cJSON_CreateArray(), *result, *p, *q, *overrides;
    if(!items || !cJSON_IsArray(defaults) || (stored && !cJSON_IsObject(stored)))goto fail;
    overrides=cJSON_GetObjectItemCaseSensitive(stored,"overrides");
    cJSON_ArrayForEach(p,defaults) {
        const char *id=prompt_string(p,"id");
        if(!prompt_valid(p) || prompt_find(items,id))goto fail;
        if(contains(cJSON_GetObjectItemCaseSensitive(stored,"deletedBuiltins"),id))continue;
        q=cJSON_Duplicate(p,1);if(!q)goto fail;
        if(cJSON_IsObject(overrides)) {
            cJSON *o=cJSON_GetObjectItemCaseSensitive(overrides,id);
            if(o) {
                cJSON_ReplaceItemInObjectCaseSensitive(q,"title",cJSON_CreateString(prompt_string(o,"title")));
                cJSON_ReplaceItemInObjectCaseSensitive(q,"body",cJSON_CreateString(prompt_string(o,"body")));
            }
        }
        if(!prompt_valid(q)) {cJSON_Delete(q);goto fail;}
        cJSON_AddItemToArray(items,q);
    }
    cJSON_ArrayForEach(p,cJSON_GetObjectItemCaseSensitive(stored,"customs")) {
        if(!prompt_valid(p) || !strncmp(prompt_string(p,"id"),"builtin:",8) || prompt_find(items,prompt_string(p,"id")))goto fail;
        q=cJSON_Duplicate(p,1);if(!q)goto fail;cJSON_AddItemToArray(items,q);
    }
    if(cJSON_GetArraySize(items)>PROMPTS_MAX)goto fail;
    cJSON_ArrayForEach(p,items) {
        cJSON_DeleteItemFromObjectCaseSensitive(p,"enabled");
        cJSON_AddBoolToObject(p,"enabled",!contains(cJSON_GetObjectItemCaseSensitive(stored,"disabled"),prompt_string(p,"id")));
    }
    result=cJSON_CreateArray();if(!result)goto fail;
    cJSON_ArrayForEach(p,cJSON_GetObjectItemCaseSensitive(stored,"order")) {
        if(cJSON_IsString(p) && (q=prompt_find(items,p->valuestring))!=NULL)
            cJSON_AddItemToArray(result,cJSON_DetachItemViaPointer(items,q));
    }
    while((q=cJSON_GetArrayItem(items,0))!=NULL)cJSON_AddItemToArray(result,cJSON_DetachItemViaPointer(items,q));
    cJSON_Delete(items);return result;
fail: cJSON_Delete(items);return NULL;
}
cJSON *prompts_delta(cJSON *defaults,cJSON *items)
{
    cJSON *root=cJSON_CreateObject(),*customs,*overrides,*deleted,*disabled,*order,*p;
    if(!root || !cJSON_IsArray(items) || cJSON_GetArraySize(items)>PROMPTS_MAX)goto fail;
    customs=cJSON_AddArrayToObject(root,"customs");overrides=cJSON_AddObjectToObject(root,"overrides");
    deleted=cJSON_AddArrayToObject(root,"deletedBuiltins");disabled=cJSON_AddArrayToObject(root,"disabled");
    order=cJSON_AddArrayToObject(root,"order");
    if(!customs || !overrides || !deleted || !disabled || !order)goto fail;
    cJSON_ArrayForEach(p,items) {
        const char *id=prompt_string(p,"id");cJSON *base=prompt_find(defaults,id),*copy;
        if(!prompt_valid(p) || contains(order,id))goto fail;
        cJSON_AddItemToArray(order,cJSON_CreateString(id));
        if(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(p,"enabled")))cJSON_AddItemToArray(disabled,cJSON_CreateString(id));
        if(!base) {if(!strncmp(id,"builtin:",8))goto fail;cJSON_AddItemToArray(customs,cJSON_Duplicate(p,1));}
        else if(strcmp(prompt_string(p,"title"),prompt_string(base,"title")) || strcmp(prompt_string(p,"body"),prompt_string(base,"body"))) {
            copy=cJSON_CreateObject();cJSON_AddStringToObject(copy,"title",prompt_string(p,"title"));
            cJSON_AddStringToObject(copy,"body",prompt_string(p,"body"));cJSON_AddItemToObject(overrides,id,copy);
        }
    }
    cJSON_ArrayForEach(p,defaults)if(!prompt_find(items,prompt_string(p,"id")))cJSON_AddItemToArray(deleted,cJSON_CreateString(prompt_string(p,"id")));
    return root;
fail:cJSON_Delete(root);return NULL;
}
