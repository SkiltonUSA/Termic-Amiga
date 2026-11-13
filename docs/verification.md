# Verification and limitations

The Amiga application was exercised on an A4000TX with a TF4060/68060,
Workbench 3.2.3, MUI 3.8 and subsequently MUI 5.0 (muimaster 21.227),
TextEditor.mcc 15.56, AmiSSL 5.27 and ZZ9000 RTG.
The screenshots show synthetic fixtures on that machine.

The earlier CLI/script update cross-built
for 68k and passed 11 portable test groups, including profile persistence,
credential separation, read-only API enforcement, MCP protocol handling and
filesystem boundaries. After a physical reboot, the written `accept_agents.py`
hardware suite passed profile Save/Use/Cancel, the MCP enable setting, task
creation, source writes, scratchpad round trips and traversal rejection.
The native command profile passed a worker-local environment value to AmigaDOS
Get. The host stdio connector completed initialization, listed all 15 tools and
called project_list through the real DevBench bridge and native TermicMCP.

Asynchronous MCP builds produced a native Hunk executable with VBCC on the Amiga.
An intentional compiler failure returned exit code 20 and left the previous
executable byte-for-byte intact. Tests use absolute SDK paths without changing
system compiler assigns. The final fixture closed normally, its public control
port disappeared, and original settings, assign and production window geometry
were restored. Screenshots show synthetic fixtures without API credentials.

An earlier test cleanup left an orphan window and stale control port; its cause
was not established. The test now closes through normal MUI events and checks
that the public port is removed. Task-list refresh notifications were also
corrected before the successful post-reboot runs. Authenticated API calls remain
unverified without a user key. Peer discovery, desktop pairing and automatic
client registration are not part of this native implementation.
The tested GUI, worker and TermicMCP were installed with a rollback backup.
The CLI/script update's CRC32 values are DEACE341, 6421F5AB and 920AB511 respectively;
the installed guide and changed source files also match the host copies.
The production window was restored to 1920 by 789 at (0, 11).
See [Agents and MCP](agents-and-mcp.md) for implemented features and limitations.

The CLI/script acceptance run also passed ARexx and MicroPython example scripts
through both saved launch profiles and the worker actions used by Build / CLI.
Each printed the expected output and returned exit code zero. The tested native
runtime identifies itself as MicroPython v1.28.0, build 239; its binary, license,
upstream README and pinned provenance accompany the installation.
ARexx used the system RX command and the already-running RexxMast.
MicroPython networking, TLS, third-party modules and interactive REPL behavior
have not been exercised by this suite. Script execution adds no new custom
Termic ARexx host commands; API/MCP writes remain limited to C/H; the instruction update also permits
reading the six named project guidance files.

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

## Custom title bar and Files list

The custom bar was built for 68020+ and exercised on MUI 5 on the A4000TX.
The written `accept_chrome.py` suite uses the same native window identifier as
`inspect_chrome.c` and tests Zoom/restore, title-bar dragging, grip resizing,
and Back/foreground restoration with actual injected input. Its optional final
step clicks X and checks that the window is gone. Host tests cover movement at
screen edges and resize limits, including a screen smaller than the minimum.
The Files list was visually inspected with light text on black.

Older native-title suites remain historical evidence for editor, clipboard and
preferences behaviour; their window-name guards are not compatible with the
new borderless main window. The custom bar has not been retested on MUI 3.8.
The existing Poseidon/MultiView issues above are not resolved by this change.

The title-bar icon update was rebuilt and passed the same hardware controls
suite, including X close, depth, zoom/restore, dragging and resizing. The compact
buttons use MUI custom Area drawings with explicit minimum/default/maximum sizes;
MUI supplies the frames and input handling. The final editor/preferences captures
were inspected after installation, and transferred executable/guide CRCs matched
the host files.

The resolution-scaled bar was rebuilt and passed the same hardware suite at
1920x800, including close. Its content height is now 26 rather than 32 pixels
at that resolution. Host tests cover the low-resolution minimum, 800/1080/1600
line sizes, the upper cap and invalid inputs. Other display resolutions have
not been exercised on the physical Amiga. Scaling uses the default public
screen height at launch; a resolution change requires restarting Termic.
The installed executable and guide matched host CRC32 checks. Editor and
preferences screenshots were inspected, and the user's maximized window
rectangle was restored after the windowed acceptance run.


## Prompt library, instructions, shortcuts and action icons

The combined update cross-builds for 68k and passes 14 portable groups. These
include shipped/default prompt resolution, edited/default separation across
updates, custom persistence, ordering, disable/delete state, invalid contents,
keyboard context boundaries, guidance serialization and MCP guidance file
copy/read/write restrictions. The skill template passes quick_validate.py.

The written accept_agents.py suite calls accept_workflow.py in its isolated
installation. The final A4000TX run passed:

- Installing guidance twice while preserving a project-owned AGENTS.md.
- Tab-to-send and Ctrl J in the prompt, with missing credentials stopping the
  request before network access; editor Tab does not launch another agent.
- Commands and keyboard-reference windows opening and closing.
- All 17 shipped prompts displayed; Review inserted without automatic send.
- Prompt Save, Use, duplicate, repeated reopening and Cancel after draft removal.
  Saving untouched defaults produces no overrides.
- Agent profile Save/Use/Cancel and MCP enable, through native icon controls.
- Native MCP task/file/scratchpad routines, traversal rejection, successful
  compilation, failed-build preservation, and host stdio transport.
- CLI environment inheritance, ARexx and MicroPython examples through profiles
  and worker command actions.
- Normal application close, public-port removal, and restoration of original
  ENV settings, Termic assign and 1920x789 production window geometry.

The final workspace, prompt library and shortcut screenshots were inspected.
The sidebar groups retain readable widths with compact icon actions. Tooltips
and descriptive menu commands accompany the icons. The prompt body is light
text on black. The screenshots contain synthetic task data and default prompt
examples, not findings from a review of the application.

A test exposed a queued List_Active refresh loop when reopening Prompts.
Notifications are now detached while rebuilding the list and reattached after
selection is restored; repeated reopen/save/cancel passed after the fix.
A title-bar key handler also ran after the active TextEditor handler; shortcuts
now register on the editor itself, with a main-window/focus guard.

Image paste, adjustable reasoning, subagent switching and custom keymaps remain
unavailable. Ctrl G opens the built-in editor. Shift Tab changes profiles.
Transcript search and selection use the native TextEditor widget; the suite
exercises transcript/search focus, not every scroll/selection combination or
physical-keyboard timing of double Escape. Cloud authentication is still not
verified with a real user key. The action icons have been tested on MUI 5 at
1920x800; other MUI versions and display resolutions need a separate check.

The verified build was installed with checksum verification for every promoted
file. Installed executable CRC32 values are EFC5E768 (GUI, 121848 bytes),
F62B6823 (worker, 63564 bytes), and 8726B98B (MCP, 58716 bytes). Previous files
are retained in SD032G:Dev/Termic/Backup/agents-1794598993. Missing project
instructions were installed while existing files were preserved. The production
application reopened successfully at 1920x789 with its control port present.

## Codex on Mac relay

Twenty portable test groups pass, including native relay frame ordering, stale
IDs, session validation, call limits, host mailbox ordering, stopped jobs,
read-only tool exposure, completion and resumption dispatch. All 68k binaries
cross-build. Codex CLI 0.166.0 reports ChatGPT authentication in the isolated
relay configuration; no API key or sign-in token was transferred to the Amiga.

The existing core hardware checks passed during the integrated runs: profiles,
prompts, shortcuts, native MCP, build/failure preservation, CLI, ARexx and
MicroPython. Relay failures were then narrowed with a focused rerun of the
written accept_codex.py checks in the same isolated installation. The final
relay checks passed on the A4000TX:

- The MUI composer launched the selected Codex on Mac profile with Tab.
- A real signed-in Codex turn listed/read source and guidance, wrote a small
  C89 console program, and compiled it with the native Amiga toolchain.
  The resulting Program has an Amiga Hunk header. The program was not run.
- A second turn resumed the saved thread and recalled the generated marker.
- Shift F10 stopped a worker waiting for the Mac without starting inference.
- Native read-only mode rejected write_source and build even when the test
  client explicitly requested them. Source bytes were preserved.
- A read_file request outside the task was rejected.
- The clipboard, ENV preferences, Termic assign and production window geometry
  were restored after the test.

Native probes exposed two compatibility issues. Append/update fopen modes
failed on this setup; the relay now uses bounded backup-and-replace transcript
writes, closing the file between messages. Long temporary mailbox names could
be created through stdio but failed native Rename with DOS error 205. A direct
long/short comparison confirmed shorter names work; all generated mailbox
filenames, including temporary suffixes, are now at most 30 characters.

The direct API branch still uses append-mode transcript output and needs a
separate repair/verification on this filesystem. The verified Codex relay does
not use that branch. Its completed-message display is not token streaming.
One relay task runs at a time. Live cancellation during inference/compiler
execution, disconnect/reboot recovery and different MUI/display configurations
remain additional hardware checks; stop-before-connection and portable
disconnection handling were exercised. No AmiPkg release was published.

Installed files were checksum-verified. GUI: 122308 bytes, CRC32 A4E31D38;
worker: 68832 bytes, B200C168; MCP: 58760 bytes, 3793A0B3. The rollback
backup is SD032G:Dev/Termic/Backup/agents-1794608665. The production app
reopened at 1920x789 with its control port present. The installed MCP handshake,
15-tool catalogue and disabled-by-default gate passed. The Mac launcher
uses an installed copy of the tested relay and passes the sign-in check.

## Desktop proxy, Claude and Cursor

The desktop proxy adds native profile kinds 6 and 7, provider-specific session
references, model arguments, a bounded stdio MCP adapter and cross-host file
transfers. The Mac GUI is bundled with Qt/Python; Windows uses the same source.
Portable tests include real stdio MCP communication, invalid client tokens,
read-only rejection, call limits, provider/model/resume routing, completed and
failed CLI responses, missing login cleanup and duplicate proxy locks.

A live Claude request was attempted. Its status command reported a saved
Claude.ai Pro login, but inference failed because that OAuth session could not
be refreshed. The user chose to finish without live Claude testing. Cursor's
CLI was installed but no authenticated Cursor request has passed. Neither
provider is claimed to have passed live cloud inference. The host status label
says Login saved because stored login state alone cannot establish live access.

The written `accept_providers` suite uses synthetic MCP clients against native
Amiga workers. It checks each provider/model request, C/H read/write, native
VBCC compilation, history resumption, read-only rejection and waiting-worker
Stop. This establishes the local tool path independently of cloud access.
The bundled smoke test checks Qt startup and actual executable-to-executable
MCP pipes; it does not authenticate to any model or contact the Amiga.

The Claude/Cursor hardware suite passed on the A4000TX with MUI 5, in the
isolated fixture agents-1794607649. It restored production at 1920 x 789.
Twenty-six host test groups passed. The macOS bundled GUI and stdio MCP smoke
test passed. Live inference for the two new providers remains unverified.

The packaged macOS application also passed written Start, Stop, restart and
window-close tests against a synthetic HTTP MCP bridge. These use actual
GUI button handlers and worker processes, with no cloud or Amiga dependency.
