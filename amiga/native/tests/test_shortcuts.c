#include "shortcuts.h"
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
    return 0;
}
