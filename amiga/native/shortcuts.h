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
static int shortcut_action(unsigned code, unsigned mods, int compose, int history, int empty)
{
    if (code >= 0x80 || (mods & KEY_AMIGA)) return KEY_NONE;
    if (!compose && !history) return KEY_NONE;
    if (mods == KEY_CTRL) {
        switch (code) {
        case 0x14: return KEY_TRANSCRIPT;
        case 0x13: return KEY_HISTORY;
        case 0x33: return KEY_QUIT;
        case 0x24: return KEY_EDITOR;
        case 0x26: return compose ? KEY_NEWLINE : KEY_NONE;
        case 0x34: return compose ? KEY_UNAVAILABLE : KEY_NONE;
        case 0x40: return history ? KEY_MARK : KEY_NONE;
        case 0x70: return history ? KEY_TOP : KEY_NONE;
        case 0x71: return history ? KEY_BOTTOM : KEY_NONE;
        }
    }
    if (history) {
        if (mods == (KEY_SHIFT|KEY_ALT) && code == 0x38) return KEY_TOP;
        if (mods == (KEY_SHIFT|KEY_ALT) && code == 0x39) return KEY_BOTTOM;
        if (!mods) {
            switch (code) {
            case 0x52: return KEY_FIND;
            case 0x48: return KEY_PAGEUP;
            case 0x49: return KEY_PAGEDOWN;
            case 0x4c: return KEY_UP;
            case 0x4d: return KEY_DOWN;
            }
        }
    }
    if (compose) {
        if (code == 0x42 && mods == KEY_SHIFT) return KEY_MODE;
        if (!mods) {
            if (code == 0x42) return KEY_SEND;
            if (code == 0x45) return KEY_ESCAPE;
            if (code == 0x53) return KEY_ACTIVITY;
            if (empty && code == 0x3a) return KEY_COMMANDS;
        }
        if (mods == KEY_SHIFT && empty) {
            if (code == 0x02) return KEY_MENTION;
            if (code == 0x01) return KEY_CLI;
            if (code == 0x3a) return KEY_COMMANDS;
        }
        if (mods == KEY_ALT && (code == 0x38 || code == 0x39 ||
            (empty && (code == 0x4e || code == 0x4f)))) return KEY_UNAVAILABLE;
    }
    return KEY_NONE;
}
#endif
