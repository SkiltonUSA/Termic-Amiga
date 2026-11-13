# Amiga interface conventions

Termic uses MUI's native window menu strip, with Project and Edit first,
Settings next to Help, separators, command keys and disabled actions.
The Project Open requester imports a source drawer into a managed project.
The full workspace remains available beneath the menu strip.

Edit actions follow the focused TextEditor gadget, including task instructions
and selectable log output. Single-line fields keep their MUI editing behavior;
Tab to a button before using application keys that a field consumes.
The editor has no duplicate command button row. Save is in Project; Undo, Redo,
Cut, Copy and Paste are in Edit. Standard MUI font roles (normal, button, list
and fixed-width) follow configured preferences instead of forcing Helvetica or
Courier. Interface colours use MUI pens; the requested black source background
and syntax palette remain local to the source editor.
TextEditor handles IFF FTXT clipboard exchange. Source and task saves export
without formatting, and pasted colours are disabled so clipboard styles do not
become escape sequences in source files or task instructions.

Save writes application defaults in `Termic:Config` and current overrides in
`ENV:Termic`; Use writes only the latter, lasting until reboot. Cancel restores
the effective settings and discards unapplied edits. Project-drawer changes need
an application restart. Worker processes read the same overrides, so subsequent
jobs use the applied model/key. Already running jobs retain their loaded values.
API keys remain plaintext local files and are never displayed in the editor.
Build-command saving is separate workspace data. Preference writes are atomic
per file, not a transaction spanning the whole set; a partial failure is reported.

Help opens the locally installed `Termic.guide` in MultiView. The manual includes
a contents node, linked chapters and alphabetical index, and does not execute
commands from links. Shift-Help and Project > About show version and credits.
The local installation-kit builder includes the guide automatically.

MUI owns menu creation, layout and disposal on 68k AmigaOS. The OS4 interface
pointers in the current GadTools example are not used by this AmigaOS 3.x build.
Menu strings have static lifetimes. MUI notifications dispatch existing actions.

MagicMenu's `src/misc.c` hooks Intuition menu-strip functions; using ordinary
MUI menus keeps Termic on that system path. No MagicMenu code is embedded.
ToolsDaemon concerns Workbench launchers; the manual explains optional
ToolsPrefs entries using absolute installation paths. Neither tool is installed
or reconfigured by Termic. No system startup files are changed.

This is an adaptation of the applicable conventions, not a claim of complete
style-guide compliance. Preset import/export, SETTINGS ToolTypes/CLI overrides,
an IFF project format, live file notifications, clipboard unit selection and an
application ARexx port remain unimplemented. Text source remains compiler-readable.
The workspace is intended for the A4000TX's RTG desktop, not a 640x200 layout.

References consulted:

- [Workbench manual](https://wiki.amigaos.net/wiki/AmigaOS_Manual:_Workbench)
- [Workbench Fonts](https://wiki.amigaos.net/wiki/AmigaOS_Manual:_Workbench_Fonts)
- [Menus](https://wiki.amigaos.net/wiki/UI_Style_Guide_Menus)
- [Preferences](https://wiki.amigaos.net/wiki/UI_Style_Guide_Preferences)
- [Data Sharing](https://wiki.amigaos.net/wiki/UI_Style_Guide_Data_Sharing)
- [AmigaGuide 101](https://wiki.amigaos.net/wiki/AmigaGuide_101)
- [GadTools Menus](https://wiki.amigaos.net/wiki/GadTools_Menus)
- [MagicMenu source](https://github.com/jens-maus/magicmenu), reviewed at
  `01a29e65596ff870e7bb171e36c3ff1e89d4599a`.
- [ToolsDaemon 2.1a](https://aminet.net/package/util/boot/ToolsDaemon21a), including
  the original archive's ToolsDaemon.doc and its ToolsPrefs instructions.

The linked English Amiga Board discussion was blocked by an anti-bot check and
was not used as evidence. Runtime evidence and remaining checks belong in
[verification.md](verification.md).
