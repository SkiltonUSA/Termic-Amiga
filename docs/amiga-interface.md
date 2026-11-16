# Amiga interface conventions

Termic uses MUI's native window menu strip, with Project and Edit first,
Settings next to Help, separators, command keys and disabled actions.
The Project Open requester imports a source drawer into a managed project.
The full workspace remains available beneath the menu strip.

Edit actions follow the focused TextEditor gadget, including task instructions
and selectable log output. Single-line fields keep their MUI editing behavior;
Tab to a button before using application keys that a field consumes.
The editor has no duplicate command button row. Save is in Project; Undo, Redo,
Cut, Copy and Paste are in Edit. The readable application fonts are restored:
Helvetica 13 for controls and Courier 13 for code, with MUI font roles used
only as fallback if those disk fonts are unavailable. The generic font-role
change was rejected because the machine defaults were less legible.

The FinalWriter reference supplied by the user guides presentation: crisp text,
framed groups, aligned form labels and consistent spacing. Projects, Tasks and
Files have named frames; preferences group Cloud agent and Development fields
into aligned columns. Interface colours and gadget bevels use MUI; the requested
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

MUI 5 uses its Title class for workspace tabs, with an individual Helvetica
text object for each label. The older Register implementation clipped labels
on the tested MUI 5 installation; changing font inheritance did not fix it.
MUI 3.8 keeps its Register tabs.

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
The main window now draws its own taller blue title bar on Workbench. The cross icon uses
the existing save-and-quit action; overlapping squares send the window behind
other windows; the outlined square fits the screen and restores its previous
rectangle. These are pixel-aligned MUI drawings with hover tooltips and native
button focus/pressed feedback, with no external image dependency. Drag the title text
to move it and the lower-right blue grip to resize it. Movement stays below the
screen bar and resize honours MUI's minimum dimensions. Workbench fonts and
other applications' borders are untouched. The Files list uses a black background
and light filenames; column formatting leaves the stored filenames unchanged.

The custom bar uses a borderless native window with no native window title.
Its ScreenTitle remains `Termic Amiga Workspace`; the native test helper uses
that identifier. A window-list order is not a reliable indication of layer
stacking, so the depth check uses the Layers library's `WhichLayer` query.

The workspace is intended for the A4000TX's RTG desktop, not a 640x200 layout.

References consulted:

- [MUI Title class](https://github.com/amiga-mui/muidev/wiki/MUI_Title)
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

The title bar scales from the default public screen's pixel height when Termic
starts. At 800 screen lines, the button/title content is 26 pixels high (formerly
32), with 16-pixel icons and a 13-pixel title font. MUI adds its frame/insets.
At 1080 lines, the content grows to 35 pixels; at 1600 lines it is 52 pixels.
Content height is bounded to 20–52 pixels to keep controls usable at extremes.
The title font and resize grip scale too; editor and other UI fonts stay unchanged.
This is resolution-based sizing, not physical-DPI detection. Restart Termic after
changing screen resolution; resizing or zooming the window does not change its
control scale. A missing title font falls back to the existing UI font.


## Action icons

Common actions use compact, pixel-aligned MUI icon buttons: floppy disk for
Save, triangle for Run/Execute, square for Stop, hammer for Build, gear for
Settings, magnifier for Find, speech bubble for Prompts and a keyboard for
Shortcuts. Dialog Save/Use/Cancel use floppy/check/cross symbols. Library
management uses new, duplicate, trash, refresh and arrow symbols.

Hover for the action name and applicable shortcut. Native MUI handles focus,
pressed and disabled states; the menu strip retains descriptive names.
The icons are drawn locally with the current MUI text pen, without an icon
font, image files or another class dependency. Agent-type selectors and
command-palette entries keep their descriptive labels.

## Activity window

Tools > Process Monitor... (Right Amiga I) opens the integrated native
process monitor. View > Activity / processes remains an alternative entry.
F4 opens it from the agent prompt. It is a child window of
Termic's MUI application, with its own asynchronous timer, so the workspace
and job queue continue servicing events. Reopening activates the same window.

The inventory lists all Exec tasks, executable names, task addresses, states,
priorities, stack sizes, partial tracked memory and sampled CPU. Native MUI
column separators and aligned headings distinguish each field; numeric columns
align right and the name column expands with the window. Address order
is stable; CPU and memory sort descending, name ascending. Selection follows
task identity. Pause stops sampling and freezes the display while confirmed
service restart supervision continues. Closing removes the interrupt sampler,
timer, fonts and window; closing Termic performs the same cleanup.

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
