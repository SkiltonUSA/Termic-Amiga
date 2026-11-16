/* Included by shortcuts_ui.h after its reference window helpers. */
#ifndef KEYMAP_FILE
#define KEYMAP_FILE "Termic:Config/keymap.json"
#endif
enum { KM_OPEN=5700,KM_CLOSE,KM_SELECT,KM_ASSIGN,KM_DEFAULTS,KM_SAVE,KM_USE };
static Object *keymap_window,*keymap_rows,*keymap_key,*keymap_mod,*keymap_note;
static struct ShortcutBinding keymap_draft[SHORTCUT_COUNT];
struct KeymapRow {const char *action;char key[64];};
static struct KeymapRow keymap_entries[SHORTCUT_COUNT];
static ULONG keymap_display(__reg("a2") char **columns,__reg("a1") struct KeymapRow *row)
{
 columns[0]=(char *)(row?row->action:"Action");columns[1]=row?row->key:"Shortcut";return 0;
}
static struct Hook keymap_display_hook={{NULL,NULL},(HOOKFUNC)keymap_display,NULL,NULL};
static const char *keymap_key_names[SHORTCUT_KEYS+1];
static const char *keymap_mod_names[]={"None","Shift","Ctrl","Shift Ctrl","Alt","Shift Alt","Ctrl Alt","Shift Ctrl Alt",NULL};
static void keymap_select(void)
{
 int i,n=value(keymap_rows,MUIA_List_Active);if(n<0 || n>=SHORTCUT_COUNT)return;
 for(i=0;i<(int)SHORTCUT_KEYS && shortcut_keys[i].code!=keymap_draft[n].code;i++);
 set(keymap_key,MUIA_Cycle_Active,i<(int)SHORTCUT_KEYS?i:0);
 set(keymap_mod,MUIA_Cycle_Active,keymap_draft[n].mods);
}
static void keymap_refresh(void)
{
 int i,n=value(keymap_rows,MUIA_List_Active);
 set(keymap_rows,MUIA_List_Quiet,TRUE);DoMethod(keymap_rows,MUIM_List_Clear);
 for(i=0;i<SHORTCUT_COUNT;i++) {
  keymap_entries[i].action=shortcut_defs[i].title;
  shortcut_key_label(keymap_entries[i].key,sizeof(keymap_entries[i].key),keymap_draft+i);
  DoMethod(keymap_rows,MUIM_List_InsertSingle,keymap_entries+i,MUIV_List_Insert_Bottom);
 }
 SetAttrs(keymap_rows,MUIA_List_Quiet,FALSE,MUIA_List_Active,n>=0?n:0,TAG_DONE);
 DoMethod(keymap_rows,MUIM_List_Redraw,MUIV_List_Redraw_All);keymap_select();
}
static void keymap_load(void)
{
 char *text=read_text(KEYMAP_FILE,2048);
 shortcut_defaults(shortcut_map);shortcut_ready=1;
 if(text) {
  if(!shortcut_decode(text,shortcut_map))message("Invalid keymap.json: using default keyboard shortcuts.");
  free(text);
 }else if(exists(KEYMAP_FILE))message("Cannot read keymap.json: using defaults.");
}
static void keymap_open(void)
{
 int i;
 if(!keymap_window) {
  for(i=0;i<(int)SHORTCUT_KEYS;i++)keymap_key_names[i]=shortcut_keys[i].name;
  keymap_window=WindowObject,MUIA_Window_Title,"Termic: Configure shortcuts",
   MUIA_Window_ID,MAKE_ID('T','K','E','Y'),MUIA_Window_Width,700,MUIA_Window_Height,480,
   WindowContents,VGroup,Child,label("Prompt and transcript actions. Workbench/menu shortcuts stay available."),
   Child,keymap_rows=ListObject,InputListFrame,
   MUIA_Font,bodyfont?(ULONG)bodyfont:MUIV_Font_List,MUIA_List_Format,"BAR,",
   MUIA_List_Title,TRUE,MUIA_List_DisplayHook,&keymap_display_hook,End,Child,HGroup,Child,label("Key"),
   Child,keymap_key=CycleObject,MUIA_Cycle_Entries,keymap_key_names,End,
   Child,label("Modifiers"),Child,keymap_mod=CycleObject,MUIA_Cycle_Entries,keymap_mod_names,End,
   Child,button("Assign",KM_ASSIGN),End,
   Child,keymap_note=label("Select an action, choose its key, then Assign. Save persists; Use lasts this session."),
   Child,HGroup,Child,button("Defaults",KM_DEFAULTS),Child,HVSpace,
   Child,button("Save",KM_SAVE),Child,button("Use",KM_USE),Child,button("Cancel",KM_CLOSE),End,End,End;
  if(!keymap_window){message("Cannot open shortcut preferences.");return;}
  DoMethod(app,OM_ADDMEMBER,keymap_window);
  notify(keymap_window,MUIA_Window_CloseRequest,TRUE,KM_CLOSE);
  notify(keymap_rows,MUIA_List_Active,MUIV_EveryTime,KM_SELECT);
 }
 memcpy(keymap_draft,shortcut_map,sizeof(keymap_draft));keymap_refresh();
 set(keymap_note,MUIA_Text_Contents,"Select an action, choose its key, then Assign. Save persists; Use lasts this session.");
 set(keymap_window,MUIA_Window_Open,TRUE);
}
static int keymap_event(ULONG id)
{
 int n;char text[2048];struct ShortcutBinding previous;
 switch(id) {
 case KM_OPEN:keymap_open();break;
 case KM_SELECT:keymap_select();break;
 case KM_ASSIGN:
  n=value(keymap_rows,MUIA_List_Active);if(n<0 || n>=SHORTCUT_COUNT)break;
  previous=keymap_draft[n];
  keymap_draft[n].code=shortcut_keys[value(keymap_key,MUIA_Cycle_Active)].code;
  keymap_draft[n].mods=keymap_draft[n].code==0xff?0:value(keymap_mod,MUIA_Cycle_Active);
  if(!shortcut_valid(keymap_draft)) {
   keymap_draft[n]=previous;
   set(keymap_note,MUIA_Text_Contents,"Conflict or reserved typing/paste key. Disable the conflicting action first.");
  }else {keymap_refresh();set(keymap_note,MUIA_Text_Contents,"Assigned in draft. Save or Use applies the changes; Cancel discards them.");}
  break;
 case KM_DEFAULTS:shortcut_defaults(keymap_draft);keymap_refresh();
  set(keymap_note,MUIA_Text_Contents,"Defaults loaded into draft. Save or Use to apply them.");break;
 case KM_SAVE:case KM_USE:
  if(!shortcut_encode(keymap_draft,text,sizeof(text)))break;
  if(id==KM_SAVE && !save_text(KEYMAP_FILE,text,2048)) {
   set(keymap_note,MUIA_Text_Contents,"Save failed. Your previous shortcuts are still active.");break;
  }
  memcpy(shortcut_map,keymap_draft,sizeof(shortcut_map));shortcut_ready=1;shortcut_reference_refresh();
  set(keymap_window,MUIA_Window_Open,FALSE);
  message(id==KM_SAVE?"Keyboard shortcuts saved.":"Keyboard shortcuts applied for this session.");break;
 case KM_CLOSE:set(keymap_window,MUIA_Window_Open,FALSE);break;
 default:return 0;
 }
 return 1;
}
