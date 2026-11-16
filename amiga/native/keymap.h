#ifndef TERMIC_KEYMAP_H
#define TERMIC_KEYMAP_H
#include "shortcuts.h"
#include "vendor/cJSON.h"
#include <stdio.h>
static int shortcut_decode(const char *text,struct ShortcutBinding *out)
{
 cJSON *root,*version,*bindings,*item,*code,*mods;int i,j,ok=0;unsigned seen=0;
 struct ShortcutBinding next[SHORTCUT_COUNT];
 if(!text)return 0;
 root=cJSON_ParseWithOpts(text,NULL,1);if(!root)return 0;
 version=cJSON_GetObjectItemCaseSensitive(root,"version");bindings=cJSON_GetObjectItemCaseSensitive(root,"bindings");
 if(!cJSON_IsNumber(version) || version->valuedouble!=1 || !cJSON_IsObject(bindings) ||
    cJSON_GetArraySize(bindings)!=SHORTCUT_COUNT)goto done;
 cJSON_ArrayForEach(item,bindings) {
  for(i=0;i<SHORTCUT_COUNT && strcmp(item->string,shortcut_defs[i].id);i++);
  if(i==SHORTCUT_COUNT || (seen&(1UL<<i)) || !cJSON_IsArray(item) || cJSON_GetArraySize(item)!=2)goto done;
  code=cJSON_GetArrayItem(item,0);mods=cJSON_GetArrayItem(item,1);
  if(!cJSON_IsNumber(code) || !cJSON_IsNumber(mods) || code->valuedouble<0 || code->valuedouble>255 ||
     code->valuedouble!=code->valueint || mods->valuedouble<0 || mods->valuedouble>7 || mods->valuedouble!=mods->valueint)goto done;
  next[i].code=code->valueint;next[i].mods=mods->valueint;seen|=1UL<<i;
 }
 if(!shortcut_valid(next))goto done;
 for(j=0;j<SHORTCUT_COUNT;j++)out[j]=next[j];ok=1;
done:cJSON_Delete(root);return ok;
}
static int shortcut_encode(const struct ShortcutBinding *map,char *out,size_t size)
{
 int i,n;size_t used=0;
 if(!shortcut_valid(map) || size<32)return 0;
 n=snprintf(out,size,"{\"version\":1,\"bindings\":{");if(n<0 || (size_t)n>=size)return 0;used=n;
 for(i=0;i<SHORTCUT_COUNT;i++) {
  n=snprintf(out+used,size-used,"%s\"%s\":[%u,%u]",i?",":"",shortcut_defs[i].id,map[i].code,map[i].mods);
  if(n<0 || (size_t)n>=size-used)return 0;used+=n;
 }
 if(size-used<4)return 0;strcpy(out+used,"}}\n");return 1;
}
#endif
