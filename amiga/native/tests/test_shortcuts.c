#include "keymap.h"
#include <assert.h>
int main(void) {
    assert(shortcut_action(0x42,0,1,0,0)==KEY_SEND);
    assert(shortcut_action(0x42,KEY_SHIFT,1,0,0)==KEY_MODE);
    assert(shortcut_action(0x42,0,0,0,0)==KEY_NONE);
    assert(shortcut_action(0x42,0,0,1,0)==KEY_NONE);
    assert(shortcut_action(0x42,KEY_AMIGA,1,0,0)==KEY_NONE);
    assert(shortcut_action(0xc2,0,1,0,0)==KEY_NONE);
    assert(shortcut_action(0x26,KEY_CTRL,1,0,0)==KEY_NEWLINE);
    assert(shortcut_action(0x26,KEY_CTRL,0,0,0)==KEY_NONE);
    assert(shortcut_action(0x3a,0,1,0,1)==KEY_COMMANDS);
    assert(shortcut_action(0x3a,0,1,0,0)==KEY_NONE);
    assert(shortcut_action(0x02,KEY_SHIFT,1,0,1)==KEY_MENTION);
    assert(shortcut_action(0x02,KEY_SHIFT,1,0,0)==KEY_NONE);
    assert(shortcut_action(0x14,KEY_CTRL,1,0,0)==KEY_TRANSCRIPT);
    assert(shortcut_action(0x52,0,0,1,0)==KEY_FIND);
    assert(shortcut_action(0x52,0,1,0,0)==KEY_NONE);
    assert(shortcut_action(0x38,KEY_SHIFT|KEY_ALT,0,1,0)==KEY_TOP);
    assert(shortcut_action(0x39,KEY_SHIFT|KEY_ALT,0,1,0)==KEY_BOTTOM);
    assert(shortcut_action(0x40,KEY_CTRL,0,1,0)==KEY_MARK);
    assert(shortcut_action(0x34,KEY_CTRL,1,0,0)==KEY_UNAVAILABLE);
    assert(shortcut_action(0x33,KEY_CTRL,0,0,0)==KEY_NONE);
    {struct ShortcutBinding draft[SHORTCUT_COUNT],saved[SHORTCUT_COUNT];char json[2048];
     shortcut_defaults(draft);assert(shortcut_valid(draft));
     assert(shortcut_encode(draft,json,sizeof(json)));assert(shortcut_decode(json,saved));
     assert(!memcmp(draft,saved,sizeof(draft)));
     draft[0].code=0x51;draft[0].mods=KEY_CTRL;
     assert(shortcut_valid(draft));memcpy(shortcut_map,draft,sizeof(draft));
     assert(shortcut_action(0x42,0,1,0,0)==KEY_NONE);
     assert(shortcut_action(0x51,KEY_CTRL,1,0,0)==KEY_SEND);
     assert(shortcut_action(0x51,KEY_CTRL,0,1,0)==KEY_NONE);
     draft[1]=draft[0];assert(!shortcut_valid(draft));
     assert(!shortcut_encode(draft,json,sizeof(json)));
     shortcut_defaults(draft);draft[0].code=0x20;assert(!shortcut_valid(draft));
     draft[0].mods=KEY_CTRL;assert(shortcut_valid(draft));
     draft[0].code=0x34;assert(!shortcut_valid(draft));
     draft[0].code=0xff;draft[0].mods=0;assert(shortcut_valid(draft));
     memcpy(shortcut_map,draft,sizeof(draft));assert(shortcut_action(0x42,0,1,0,0)==KEY_NONE);
     assert(shortcut_encode(draft,json,sizeof(json)) && shortcut_decode(json,saved));
     assert(!shortcut_encode(draft,json,20));
     assert(!shortcut_decode("{}",saved));assert(!shortcut_decode(NULL,saved));
     assert(!shortcut_decode("{\"version\":1,\"bindings\":{}}",saved));
     shortcut_defaults(draft);assert(shortcut_encode(draft,json,sizeof(json)));
     strcat(json," trailing");assert(!shortcut_decode(json,saved));
     /* Reject invalid input atomically: keep the last valid map. */
     memcpy(saved,draft,sizeof(draft));assert(!shortcut_decode("{",saved));
     assert(!memcmp(saved,draft,sizeof(draft)));
    }
    return 0;
}
