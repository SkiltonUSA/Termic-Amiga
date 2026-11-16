#ifndef TERMIC_SHORTCUTS_H
#define TERMIC_SHORTCUTS_H
/* Amiga raw keys. Symbols use the US key positions; navigation keys match
 * TextEditor.mcc's extended keyboard definitions. No toolkit dependency. */
enum { KEY_NONE, KEY_SEND, KEY_MODE, KEY_NEWLINE, KEY_EDITOR, KEY_HISTORY,
       KEY_TRANSCRIPT, KEY_QUIT, KEY_ACTIVITY, KEY_COMMANDS, KEY_MENTION,
       KEY_CLI, KEY_ESCAPE, KEY_FIND, KEY_TOP, KEY_BOTTOM, KEY_PAGEUP,
       KEY_PAGEDOWN, KEY_MARK, KEY_UP, KEY_DOWN, KEY_UNAVAILABLE };
#define KEY_SHIFT 1
#define KEY_CTRL 2
#define KEY_ALT 4
#define KEY_AMIGA 8
#include <string.h>
#define SHORTCUT_COUNT 16
#define SHORTCUT_COMPOSE 1
#define SHORTCUT_HISTORY 2
struct ShortcutBinding {unsigned code,mods;};
struct ShortcutDefinition {const char *id,*title;int action,contexts;struct ShortcutBinding binding;};
static const struct ShortcutDefinition shortcut_defs[SHORTCUT_COUNT]={
 {"send","Send message",KEY_SEND,1,{0x42,0}},
 {"profile","Change agent profile",KEY_MODE,1,{0x42,KEY_SHIFT}},
 {"newline","Insert new line",KEY_NEWLINE,1,{0x26,KEY_CTRL}},
 {"editor","Open editor",KEY_EDITOR,3,{0x24,KEY_CTRL}},
 {"history","Search history",KEY_HISTORY,3,{0x13,KEY_CTRL}},
 {"transcript","Open transcript",KEY_TRANSCRIPT,3,{0x14,KEY_CTRL}},
 {"quit","Quit (save edits)",KEY_QUIT,3,{0x33,KEY_CTRL}},
 {"activity","Process Monitor",KEY_ACTIVITY,1,{0x53,0}},
 {"find","Find in transcript",KEY_FIND,2,{0x52,0}},
 {"mark","Select transcript lines",KEY_MARK,2,{0x40,KEY_CTRL}},
 {"top","Top of transcript",KEY_TOP,2,{0x70,KEY_CTRL}},
 {"bottom","Latest transcript line",KEY_BOTTOM,2,{0x71,KEY_CTRL}},
 {"pageup","Page up",KEY_PAGEUP,2,{0x48,0}},
 {"pagedown","Page down",KEY_PAGEDOWN,2,{0x49,0}},
 {"up","Up / extend selection",KEY_UP,2,{0x4c,0}},
 {"down","Down / extend selection",KEY_DOWN,2,{0x4d,0}}
};
struct ShortcutKey {const char *name;unsigned code;};
static const struct ShortcutKey shortcut_keys[]={
 {"Disabled",0xff},{"Tab",0x42},{"Space",0x40},{"F1",0x50},{"F2",0x51},{"F3",0x52},{"F4",0x53},
 {"Home",0x70},{"End",0x71},{"Page Up",0x48},{"Page Down",0x49},{"Up",0x4c},{"Down",0x4d},
 {"A",0x20},{"B",0x35},{"C",0x33},{"D",0x22},{"E",0x12},{"F",0x23},{"G",0x24},
 {"H",0x25},{"I",0x17},{"J",0x26},{"K",0x27},{"L",0x28},{"M",0x37},{"N",0x36},
 {"O",0x18},{"P",0x19},{"Q",0x10},{"R",0x13},{"S",0x21},{"T",0x14},{"U",0x16},
 {"V",0x34},{"W",0x11},{"X",0x32},{"Y",0x15},{"Z",0x31}
};
#define SHORTCUT_KEYS (sizeof(shortcut_keys)/sizeof(shortcut_keys[0]))
static struct ShortcutBinding shortcut_map[SHORTCUT_COUNT];
static int shortcut_ready;
static void shortcut_defaults(struct ShortcutBinding *map)
{
 int i;for(i=0;i<SHORTCUT_COUNT;i++)map[i]=shortcut_defs[i].binding;
}
static int shortcut_valid(const struct ShortcutBinding *map)
{
 int i,j;unsigned code,mods;
 for(i=0;i<SHORTCUT_COUNT;i++) {
  code=map[i].code;mods=map[i].mods;
  if(mods>7)return 0;
  if(code==0xff){if(mods)return 0;continue;}
  for(j=0;j<(int)SHORTCUT_KEYS && shortcut_keys[j].code!=code;j++);
  if(j==(int)SHORTCUT_KEYS)return 0;
  /* Preserve normal typing, paste and unavailable image-paste handling. */
  if((code<0x40 || code==0x40) && !(mods&(KEY_CTRL|KEY_ALT)))return 0;
  if(code==0x34 && mods==KEY_CTRL)return 0;
  for(j=0;j<i;j++)if(code==map[j].code && mods==map[j].mods &&
     (shortcut_defs[i].contexts&shortcut_defs[j].contexts))return 0;
 }
 return 1;
}
static int shortcut_action(unsigned code, unsigned mods, int compose, int history, int empty)
{
 int i,context=(compose?1:0)|(history?2:0);
 if(code>=0x80 || (mods&KEY_AMIGA) || !context)return KEY_NONE;
 if(!shortcut_ready){shortcut_defaults(shortcut_map);shortcut_ready=1;}
 for(i=0;i<SHORTCUT_COUNT;i++)if((context&shortcut_defs[i].contexts) &&
    code==shortcut_map[i].code && mods==shortcut_map[i].mods)return shortcut_defs[i].action;
 if(history) {
  if(mods==(KEY_SHIFT|KEY_ALT) && code==0x38)return KEY_TOP;
  if(mods==(KEY_SHIFT|KEY_ALT) && code==0x39)return KEY_BOTTOM;
 }
 if(compose) {
  if(!mods && code==0x45)return KEY_ESCAPE;
  if(!mods && empty && code==0x3a)return KEY_COMMANDS;
  if(mods==KEY_SHIFT && empty) {
   if(code==0x02)return KEY_MENTION;
   if(code==0x01)return KEY_CLI;
   if(code==0x3a)return KEY_COMMANDS;
  }
  if((mods==KEY_CTRL && code==0x34) ||
     (mods==KEY_ALT && (code==0x38 || code==0x39 || (empty && (code==0x4e || code==0x4f)))))return KEY_UNAVAILABLE;
 }
 return KEY_NONE;
}
#endif
