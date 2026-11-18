# Amiga interface conventions

Termic uses MUI's native window menu strip, with Project and Edit first,
Settings next to Help, separators, command keys and disabled actions.
The Project Open requester imports a source drawer into a managed project.
The full workspace remains available beneath the menu strip.

Edit actions follow the focused TextEditor gadget, including task instructions
and selectable log output. Single-line fields keep their MUI editing behavior;
Tab to a button before using application keys that a field consumes.
Save is available inside the Editor frame beside Find/Next, in the compact
icon row and in the Project menu. Click the source text to type directly;
Find uses a magnifier; Find Next uses a down arrow and its own hover label.
F8 and Right-Amiga-S also save. An asterisk before the filename marks unsaved
changes. A read-only caption and disabled Editor Save button identify a busy
workspace or a file that could not be loaded. Undo, Redo,
Cut, Copy and Paste remain in Edit. The reference layout uses Helvetica 13
for workspace controls and Courier 13 for source text. If either disk font is
unavailable, the corresponding MUI font role is used. Settings > MUI Settings
controls gadget appearance; the native title bar follows system preferences.
Termic does not draw a separate blue title bar.

The FinalWriter reference supplied by the user guides presentation: crisp text,
framed groups, aligned form labels and consistent spacing. Projects, Tasks and
Files have named frames; preferences stack labelled fields in a scrollable form. Interface colours and gadget bevels use MUI; the requested
black source background and syntax palette remain local to the editor.
AmigaGuide 101 governs the help document structure, not the application typeface.
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

Native MUI Title tabs select Agent, Editor, Build / CLI, Changes and Settings.
At compact sizes, a top cycle selector also provides Projects, Tasks, Files
and CLI as full-width panes. F1-F5 and existing menu shortcuts continue to
select the corresponding work pages.

MUI owns menu creation, layout and disposal on 68k AmigaOS. The OS4 interface
pointers in the current GadTools example are not used by this AmigaOS 3.x build.
Menu strings have static lifetimes. MUI notifications dispatch existing actions.

ReBuild provides a concrete menu reference: Project/Edit ordering, grouped
actions, command keys, submenus and disabled/checkable items. Its menu editor
builds native GadTools menus using CreateMenusA and LayoutMenusA. Termic keeps
MUI ownership of its equivalent native menu strip; ReBuild is a design reference,
not a new runtime dependency or a replacement GUI toolkit.

MagicMenu's `src/misc.c` hooks Intuition menu-strip functions; using ordinary
MUI menus keeps Termic on that system path. No MagicMenu code is embedded.
ToolsDaemon concerns Workbench launchers; the manual explains optional
ToolsPrefs entries using absolute installation paths. Neither tool is installed
or reconfigured by Termic. No system startup files are changed.

This is an adaptation of the applicable conventions, not a claim of complete
style-guide compliance. Preset import/export, SETTINGS ToolTypes/CLI overrides,
an IFF project format, live file notifications, clipboard unit selection and an
application ARexx port remain unimplemented. Text source remains compiler-readable.
The main window uses native MUI/Intuition title, drag, depth, close, size and
zoom controls. Its ScreenTitle remains `Termic Amiga Workspace`. The window
opens within the current public screen's usable area; no system font or screen
mode is changed. Source and Files retain their black backgrounds.

Below 800 pixels wide or 500 pixels high, only the selected workspace pane is
shown. At larger sizes, Projects/Tasks sit on the left, the tabbed work page
in the centre, and Files above CLI on the right. The Agent page places task
instructions above a single row containing the profile selector, YOLO
control and four action icons, below the activity log. The profile selector
keeps its natural text width on the left; YOLO and the action buttons align
to the right. The YOLO selector labels its states "YOLO Off" and "YOLO On",
without a separate caption. The redundant profile caption is omitted. Both selectors match
the 22-pixel action-button height, and the four buttons have equal gaps.
The header shows the version and Build, Run, Settings, About and Quit
icons. Files and CLI have a native balance divider; column widths can also
be adjusted. Resizing changes visibility of existing widgets rather
than replacing their buffers, so unsaved text and editor position survive.

Settings and large auxiliary dialogs use MUI virtual groups and automatic
scrollbars. Tables and long forms can be scrolled at small sizes; their full
contents are not reduced to unreadable text. Status messages have hover help
for text that does not fit the available width. The application uses MUI font
metrics, not bitmap scaling or automatic physical-DPI detection.

References consulted:

- [MUI Title tabs](https://github.com/amiga-mui/muidev/wiki/MUI_Title)
- [MUI Group layout](https://github.com/amiga-mui/muidev/wiki/MUI_Group)
- [MUI scrolling and automatic scrollbars](https://github.com/amiga-mui/muidev/wiki/MUI_Scrollgroup)
- [Native MUI windows](https://github.com/amiga-mui/muidev/wiki/MUI_Window)
- [Hello MUI visual reference](https://keasigmadelta.com/blog/hello-mui-how-to-write-your-first-amigaos-app-using-mui/)
- [Workbench manual](https://wiki.amigaos.net/wiki/AmigaOS_Manual:_Workbench)
- [Workbench Fonts](https://wiki.amigaos.net/wiki/AmigaOS_Manual:_Workbench_Fonts)
- [Menus](https://wiki.amigaos.net/wiki/UI_Style_Guide_Menus)
- [Preferences](https://wiki.amigaos.net/wiki/UI_Style_Guide_Preferences)
- [Data Sharing](https://wiki.amigaos.net/wiki/UI_Style_Guide_Data_Sharing)
- [AmigaGuide 101](https://wiki.amigaos.net/wiki/AmigaGuide_101)
- [ReBuild menu editor](https://github.com/dmcoles/ReBuild/blob/main/menuObject.e)
  and [application menus](https://github.com/dmcoles/ReBuild/blob/main/rebuild.e),
  reviewed at `9884ee4e282e7c551a90ff6bd493929828c490c2`.
- [GadTools Menus](https://wiki.amigaos.net/wiki/GadTools_Menus)
- [MagicMenu source](https://github.com/jens-maus/magicmenu), reviewed at
  `01a29e65596ff870e7bb171e36c3ff1e89d4599a`.
- [ToolsDaemon 2.1a](https://aminet.net/package/util/boot/ToolsDaemon21a), including
  the original archive's ToolsDaemon.doc and its ToolsPrefs instructions.

The linked English Amiga Board discussion was blocked by an anti-bot check and
was not used as evidence. Runtime evidence and remaining checks belong in
[verification.md](verification.md).

## Action icons

Common actions use compact, pixel-aligned MUI icon buttons: floppy disk for
Save, triangle for Run/Execute, square for Stop, hammer for Build, gear for
Settings, magnifier for Find, speech bubble for Prompts and a keyboard for
Shortcuts. Dialog Save/Use/Cancel use floppy/check/cross symbols. Library
management uses new, duplicate, trash, refresh and arrow symbols.

The Tasks panel has one row of five 24x24 buttons with 16px glyphs: New,
Rename, Kanban (columns), Queue (list) and Mark Done (tick). Each has a full
action name in its hover help. Projects uses the same compact size for its
single row of New, Rename and Import Folder buttons. The agent control row
uses 22x22 action buttons to match its selectors. Other toolbars keep their existing size.

Hover for the action name and applicable shortcut. Native MUI handles focus,
pressed and disabled states; the menu strip retains descriptive names.
The icons are drawn locally with the current MUI text pen, without an icon
font, image files or another class dependency. Agent-type selectors and
command-palette entries keep their descriptive labels.

## Processors tab

The Processors tab contains the integrated native process monitor.
Tools > Process Monitor... (Right Amiga I) selects that tab. View > Activity / processes remains an alternative entry.
F4 selects it from the agent prompt. The page lives inside the main workspace
window; no separate application or child window is launched. Its asynchronous
timer lets the workspace and job queue continue servicing events.

The inventory lists all Exec tasks, executable names, task addresses, states,
priorities, stack sizes, partial tracked memory and sampled CPU. Native MUI
column separators and aligned headings distinguish each field; numeric columns
align right and the name column expands with the window. Address order
is stable; CPU and memory sort descending, name ascending. Selection follows
task identity. Pause stops sampling and freezes the display while confirmed
service restart supervision continues. Switching tabs stops CPU sampling,
while pending service restarts still complete. Returning resumes sampling
unless paused. Closing Termic releases the sampler and timer.

The existing cooperative stop and opt-in service restart protections apply.
Service definitions are read from `services.txt` beside `TermicNative`.
OS3 uptime, output counters and full private memory usage are unavailable.
The optional ZZ9000 button shows/hides the board information panel; it starts
hidden and makes no board queries until opened. It does not enable compute
offload. There is no inferred project hierarchy or ARM process inventory. ARM telemetry
is explicitly unavailable; firmware and board ownership are never changed.

`TermicNative --activity-fixture` opens Activity with synthetic data and disabled
process controls for screenshots. It does not replace normal workspace data.
The monitor sources ship under `apps/process-monitor` in the native kit and
are linked directly by `BuildNative`; no extra executable is needed.
