# Verification and limitations

The Amiga application was exercised on an A4000TX with a TF4060/68060,
Workbench 3.2.3, MUI 3.8 and subsequently MUI 5.0 (muimaster 21.227),
TextEditor.mcc 15.56, AmiSSL 5.27 and ZZ9000 RTG.
The screenshots show synthetic fixtures on that machine.

Verified through the written hardware suites:

- Project/task creation, panel navigation, local compilation and program execution.
- Embedded editing, backup saves and exact plain-source export from coloured text.
- Local Git initialization, commits, status and diffs on an FFS long-filename image.
- Shell/build failure reporting and preservation of the previous executable.
- Quit/reopen with last-workspace restoration.
- About opening from the editor, closing/reopening, and all three Amiga version tags.
- Native menu order and command keys, ghosted Copy/Paste states, and menu shortcuts.
  Rechecked with menu-only editing controls, restored Helvetica/Courier fonts
  and framed sections; workspace, Settings and About screenshots were inspected.
- Clipboard output is an IFF FORM FTXT with the expected CHRS text; pasting into
  task instructions and saving produces exact plain text without colour codes.
- Preferences Use leaves saved defaults unchanged; Cancel discards edits; Save
  persists the restored effective values. Tests use synthetic models and no key.
- Help launches the 18-node AmigaGuide manual in MultiView; Shift-Help opens About.
  Keyboard link navigation reached the Installation and dependencies chapter.

After the MUI 5 upgrade, the same file/clipboard, preferences, About and Help
checks passed with `accept_style.py --mui-custom-menus`. MUI 5 did not expose
these menus through the Intuition MenuStrip inspected by the helper, so that
mode checks actual shortcut effects and explicitly records the menu-structure
inspection as untested. All GUI test launches use a 256 KiB stack.

The final production editor, preferences and About screenshots were inspected,
and the installed GUI, worker and guide matched host CRC32 checks. A subsequent
production Help launch stalled at MultiView’s “Loading…” window; its screenshot
request and a follow-up bridge request timed out. Production Help navigation
therefore still needs a repeat check, despite the fixture Help pass. The reboot
also exposed a separate MUI warning for Poseidon’s PoPo helper (8192-byte stack;
MUI recommends 32000); that system compatibility issue remains unresolved.

The `accept_style.py` suite uses a fresh temporary installation and restores the
previous clipboard and production app afterwards. Native menus were inspected
while MagicMenu and ToolsDaemon were already running. Mouse popup appearance and
new ToolsDaemon launcher entries have not been separately exercised.
Single-line MUI fields consume some application keys: Tab to a button or use the
mouse menus. Buttons in this setup activate with Return, not Space.

Portable checks validate the manual's node/link graph, packaging inclusion and
preference precedence alongside the existing protocol, agent, syntax and file tests.
See [Amiga interface conventions](amiga-interface.md) for the implemented scope.

The worker's stale-output regression failed before its fix and passed afterward.
Verified HTTPS returned the expected unauthenticated HTTP 401.
Authenticated cloud model/tool exchange has not been verified with a user key.

The existing long-filename image was prepared separately. Startup mounting and
refusal to recreate an active volume were exercised; automatic fresh-image
creation by `TermicStorage --create` has not been hardware-tested.
No ARM compute payload or firmware change is part of this application.
Automated checks and screenshots do not replace human interaction testing.

The API key is stored in plaintext in the local installation's Config drawer
and, when applied through preferences, in ENV:Termic until reboot.
It is excluded from this repository and installation kits. Source and requested
compiler output are sent to the selected cloud API when an agent runs.

The installed GUI, worker and guide were checked against the host-built files
using CRC32 after transfer. Previous executables and changed source files were
retained in the installation's Backup drawer before promotion.
