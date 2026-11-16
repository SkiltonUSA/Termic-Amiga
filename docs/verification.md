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
Termic ARexx host commands. At that stage API/MCP writes were limited to C/H;
the instruction update also permitted reading six named project guidance files.
The agent execution update below supersedes that agent-file limitation.

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

Both platform jobs in [desktop build 43522993207](https://github.com/SkiltonUSA/Termic-Amiga/actions/runs/43522993207)
passed their portable suite, packaging, bundled GUI/MCP smoke test and written
Start/Stop/restart/close test. Windows ran on a GitHub-hosted Windows runner;
a physical Windows 11 PC connected to this Amiga has not been tested.
The Windows artifact ZIP was downloaded and its integrity and executable /
_internal layout checked. No model credentials were used in CI.

The tested native build was installed with checksum verification and backup
at `SD032G:Dev/Termic/Backup/agents-1794610294`. GUI CRC32 4FBA275C,
worker 0EE89984, MCP B3344EDF. Installed MCP handshake, 15 tools and the
default-disabled gate passed. Production geometry and settings were restored.
The Mac desktop app was installed and its running relay connection verified.


## Proxy connection recovery

The installed proxy was subsequently found stopped with an opaque MCP
TaskGroup error. The original nested exception was not recorded, so its exact
cause is unknown. Transport failures now reconnect with bounded backoff and
show a readable status. Partially initialized MCP contexts are closed before
retry. The running proxy retains its consumed job ID across reconnects.
Regression tests cover failed initialization cleanup, interrupted-job replay
prevention, retry backoff, Stop during retry, lock release, fatal input errors,
HTTP status classification and an actual refused localhost connection.
The packaged GUI test now starts with an unavailable synthetic Amiga and
simulates a second disconnection after connecting, before Stop/restart/close.

All 18 portable proxy test groups passed after this change, as did the local
macOS bundled startup/MCP and expanded GUI connection-recovery tests.

[Recovery build 43526908696](https://github.com/SkiltonUSA/Termic-Amiga/actions/runs/43526908696)
passed on macOS and Windows, including the expanded packaged GUI recovery
checks. The installed Mac app was updated with a local backup and reconnected
to the actual Amiga bridge. The new Windows download passed archive integrity
checks. Claude and Cursor live authentication remains unverified.


## Claude subscription authentication completed

The user refreshed Claude's browser login. Claude Code then reported a
Claude.ai Pro login and passed a real model/MCP smoke test. The authenticated
Amiga acceptance subsequently passed GUI task submission, source reading and
writing, native compilation, conversation resume, Stop while waiting,
read-only enforcement and path-traversal rejection. The generated program was
compiled but not run. This supersedes the earlier expired-Claude-login result.
The live transcript recorded the actual read_source, write_source and build
calls. The resume check examines only the new turn's response text.

Claude emits the same session ID on many events. The relay now saves it once
per turn, avoiding redundant Amiga file writes, and rejects a changed ID
within a turn. A subprocess protocol regression test covers repeated IDs.
Cursor browser sign-in and authenticated Cursor execution remain pending.

[Session update build 43529096729](https://github.com/SkiltonUSA/Termic-Amiga/actions/runs/43529096729)
passed portable, packaged MCP and desktop lifecycle/recovery checks on macOS
and Windows. The updated Mac app was installed and restarted after restoring
the production Amiga workspace and removing the temporary helper. The Windows
archive was downloaded and integrity-checked. Cursor sign-in is still pending.

## Cursor subscription authentication completed

Cursor browser sign-in is now saved in the proxy's dedicated configuration.
The first authenticated smoke test exposed missing MCP discovery: the CLI
did not load the adapter from the custom CLI configuration directory.
Registering it in the dedicated workspace's `.cursor/mcp.json` fixed the
failure. The regression test checks discovery configuration at subprocess
launch and cleanup after both successful and failed turns.

The real Cursor smoke test then called list_sources successfully. On the
A4000TX, authenticated acceptance passed GUI submission, read_source,
write_source, native VBCC compilation, conversation resume, Stop while waiting,
native read-only enforcement and path-traversal rejection. The resulting
Hunk executable was compiled, not run. Settings, clipboard, assign and the
production window were restored. This supersedes the earlier pending-Cursor
results. All three host providers have now passed authenticated Amiga tests.

All 18 portable proxy tests, the rebuilt macOS GUI/MCP smoke test and packaged
connection-recovery/Start/Stop/restart/close checks passed. The Mac application
was backed up and updated with the MCP discovery fix.

[Cursor MCP build 43543665677](https://github.com/SkiltonUSA/Termic-Amiga/actions/runs/43543665677)
passed portable tests, packaging, bundled MCP and desktop lifecycle/recovery
checks on both macOS and Windows. A physical Windows 11 connection to this
Amiga remains untested.

## Persistent CLI beneath Files

The right sidebar now retains Files above a CLI panel with an adjustable
roughly half-height split. CLI, ARexx and MicroPython command entry stays
visible alongside the editor or agent view. The Build / CLI tab keeps the
larger mirrored job log. This is captured command execution, not a PTY for
interactive full-screen shell applications.

The focused written hardware acceptance passed on the A4000TX with the final
GUI and worker: Return-to-run, successful output, native exit code 20, output
replacement and clipboard copy, source staying visible, one character typed
exactly once, save/undo/save restoring the original source, and resizing from
1400x650 to 1920x789. The test restored the production preferences, clipboard,
assign and window. Both captured layouts were read back and inspected.

The run exposed and verified fixes for click-to-focus after TextEditor was
unlocked, saving an undo back to its original imported text, and completion
being published before buffered command output reached the log. The worker
now flushes output before publishing its result, and the GUI reads the log
once more after observing completion. All 33 portable tests and the final
68k cross-build passed.

The verified GUI and worker, matching sources, ReadMe and AmigaGuide were
installed after staging, checksumming and backing up the previous files.
Production reopened at 1920x789. Installed GUI: 123340 bytes, CRC32
`C4EC0264`; worker: 69284 bytes, CRC32 `820A3498`. The backup is
`SD032G:Dev/Termic/Backup/cli-panel-1794624848`.

![Files above CLI in the windowed acceptance fixture](screenshots/workspace-cli.png)

![Files above CLI at full desktop width](screenshots/workspace-cli-wide.png)

## Live host-agent feedback

Codex, Claude and Cursor now forward buffered text deltas during generation.
Real authenticated smoke checks against a synthetic tool endpoint observed
396 Codex deltas, 180 Claude deltas and 460 Cursor deltas. Text reached the
mailbox before completion in all three cases, the start/end markers each
appeared exactly once, and each provider called list_sources. These smoke
checks validate real provider events; their tool endpoint uses synthetic
source data rather than the Amiga filesystem.

The portable suite passes 38 tests, including chunk batching, final-message
reconciliation, wrong-thread filtering, duplicate Cursor flushes, Claude text
versus tool/reasoning deltas, transport failure, buffer limits and tool ordering.
The standalone TextEditor native regression preserves a sentence split across
two inserts exactly.

Repeated live updates exposed a native reply-file publication failure. The
proxy now removes an acknowledged reply before publishing the next request,
so it does not poll an old reply during replacement. Native publication also
uses a shorter temporary basename: the stop-marker temporary filename had
exceeded the classic 30-byte filename limit. The mailbox order regression
checks that reply removal happens before each new request.

The written A4000TX acceptance passed with text visible while the worker
remained busy, exact joining across partial-text boundaries, stable clipboard
selection (including when the job completes), catch-up after selection
release, tool start/return markers and
final completion without duplication. Stop after partial output published its
marker successfully and rejected further text. Production settings, assign,
clipboard and window geometry were restored. The refreshed Mac bundle passed
GUI/MCP smoke and Start/Stop/reconnect/restart/close checks.

![Synthetic response and tool activity while the task is still running](screenshots/live-feedback.png)

The final GUI and worker were installed with matching source files, ReadMe
and AmigaGuide, after staging and checksum verification. Installed GUI:
124144 bytes, CRC32 `4C9A117E`; worker: 69496 bytes, CRC32 `15796181`.
The rollback backup is
`SD032G:Dev/Termic/Backup/live-feedback-1794668662`. The production window
reopened at 1920x789, and the Mac proxy bundle was backed up and updated.
This supersedes the earlier completed-message-only display limitation.

[Live feedback build 43608154271](https://github.com/SkiltonUSA/Termic-Amiga/actions/runs/43608154271)
passed portable tests, packaging, bundled MCP and desktop lifecycle/recovery
checks on macOS and Windows. The Windows archive passed integrity and
executable/dependency-layout checks. A physical Windows 11 connection to the
Amiga remains untested.


## Agent execution and live hardware queries

The shared native dispatcher now provides system_info, run_arexx and
run_program for direct API and host-agent profiles. All 38 portable tests
passed, including read-only enforcement, tool schemas, ARexx file access,
path rejection, and build permission invalidation after edits or failures.
The worker, GUI and MCP programs cross-built for 68k. The Mac proxy bundle
passed its GUI and stdio MCP smoke test.

The written hardware acceptance passed ARexx output and syntax-error status,
native VBCC build-and-run, nonzero program exits, failed-build diagnostics,
preservation of the previous executable, refusal to run a stale build,
live hardware queries and cooperative Stop. Testing caught and corrected
RX treating a quoted filename as inline ARexx and the build script exiting
or losing its failure status before displaying compiler diagnostics.

Authenticated Codex acceptance then queried the actual hardware, wrote an
ARexx script containing the detected CPU and memory values, executed it,
and returned its output and exit code. This ran in an isolated fixture
through the real relay and existing host login. The test verifies the worker
log, saved script, transcript and completion status. No credentials were
copied to the Amiga.

At this stage the new execution tools had been exercised live with Codex.
Claude and Cursor shared the dispatcher and passed portable adapter tests;
the subsequent execution checks are recorded below.
Direct API execution and a physical Windows 11 connection remain untested.
See [Agent execution](agent-execution.md) for the synchronous run limits,
cooperative Stop and New conversation requirement after upgrading.

The subsequent `accept_build_agent.py` check used the installed worker and
an isolated C task. Authenticated Codex read the deliberately broken source,
received the real compiler failure, corrected the source, rebuilt and ran
Program exactly once. Native log ordering, captured output, exit code zero,
final source and the Hunk executable all passed verification. The Mac proxy
was restored after the check; existing user tasks were not edited.

All 17 shipped prompt introductions now describe ARexx execution, build/run
and hardware queries, with read-only restrictions. The eight native test
groups passed, including prompt limits and preservation of custom overrides.
The importer reproduced the updated defaults byte-for-byte from the desktop
source. User-customised prompts remain unchanged.

[Execution proxy build 43619273350](https://github.com/SkiltonUSA/Termic-Amiga/actions/runs/43619273350)
passed portable tests, packaging, bundled MCP smoke tests and desktop
lifecycle/recovery checks on both macOS and Windows. Download links are in
[AI Proxy setup](ai-proxy.md). This does not establish a physical Windows 11
to Amiga connection test.


## Session saving, Cursor operation polling and Stop cleanup

Long profile IDs could make a session filename and its temporary save share
one truncated 30-character name on the target filesystem. Such IDs now use a
short stable filename, with room for temporary and backup suffixes. Ordinary
short IDs retain their existing filenames. Boundary, provider-separation,
known-hash and replacement-save tests passed.

A live Cursor build check received MCP request timeouts and issued repeated
tool calls. Cursor now starts one native operation and collects its result
with `await_operation`, using polls of at most 20 seconds. Pending requests
return the existing operation ID. Completed results remain available for the
current task; poll cancellation does not cancel or repeat the native action.
Read-only restrictions and the native tool budget still apply. A provider
cannot finish successfully while its operation result remains uncollected.

Stopping the failed fixture exposed a second defect: the default VBCC
Ctrl-C handler could exit during file I/O without releasing worker resources.
A native control probe reproduced the abrupt exit; the replacement handler
latched cancellation and returned normally. The written Stop-during-build
acceptance then verified cancellation, exit status 20, and removal of both
public ports, the relay request and the task lock on the final worker.

All eight native test groups and 27 portable proxy tests passed, including
real stdio MCP polling, duplicate requests, cached results, cancelled polls,
read-only enforcement, task limits and cleanup. The rebuilt Mac bundle passed
GUI/MCP smoke tests and desktop reconnect, Start, Stop and close acceptance.

Authenticated Claude and Cursor then both passed the hardware-to-ARexx and
C repair/build/run checks using the final candidate worker. Each C check
required exactly one failed build, one repaired build and one execution.
Native logs, saved sources, captured output, Hunk headers and exit status were
verified independently of the model's report. Cursor completed operations
lasting longer than a minute by polling, with no repeated native builds or
executions. Temporary profiles were removed and original settings restored.
These checks supersede the earlier Claude/Cursor execution verification gap.
Direct API execution and a physical Windows 11 connection remain untested.

The verified native GUI, worker and MCP binaries, matching changed source
files, ReadMe and AmigaGuide were installed after checksum validation and a
rollback backup. The production window reopened with its control port active.
The installed worker is 76056 bytes, CRC32 `6C8D75FC`. Original Amiga profiles
were checked against their pre-test state; no fixture worker ports remained.
The Mac proxy was backed up, replaced with the tested bundle, and confirmed
ready with an idle relay.

[Provider reliability build 43630569708](https://github.com/SkiltonUSA/Termic-Amiga/actions/runs/43630569708)
passed portable tests, packaging, bundled MCP smoke tests and desktop
lifecycle/recovery checks on both macOS and Windows. Updated downloads are
linked in [AI Proxy setup](ai-proxy.md). The Windows archive passed integrity,
executable-header and bundled Python/Qt layout checks. Windows hardware
connectivity remains outside this automated verification.


## Packaged task pickup and bridge mailbox recovery

A subsequent production task timed out waiting for the host before any
provider connection or native tool action. Earlier authenticated hardware
checks dispatched jobs from the source helper; the packaged desktop checks
covered connection lifecycle but did not exercise pending task pickup.

The shell-based existence check could receive a capture filename or an
unrelated bridge error. It has been replaced with validated directory and
file operations. A two-client read control failed with the old transport;
all 20 reads passed with the replacement. A separate native-file probe
verified checksum, publication, readback and deletion. Reading a running
worker's exclusively opened log also reproduced an unrelated READFILE error
interrupting CHECKSUM. Only read-only verification is retried, never the
write, rename or native execution.

The packaged acceptance now presents an actual synthetic request through
MCP, injects malformed directory and unrelated checksum replies, verifies
ordered acknowledgements, and requires the packaged worker to report a
missing provider executable back to the simulated Amiga. This needs no
account or model access and runs in both desktop CI jobs. All 35 portable
proxy tests passed locally. Native binaries are unchanged by this fix.

The reported production task then passed through the installed Mac app's
normal idle mailbox pickup. The previous conversation and transcript were
backed up; a fresh conversation loaded the current native tool schema.
Hardware detection, source save and exactly one ARexx execution completed
with captured output and exit status 0. The earlier transcript remained
intact. This exercises the previously missing packaged-app-to-hardware path.
Private task contents and hardware transcripts are not public fixtures.

The final installed Mac bundle matches the tested build. The app is running,
the production task is in Review with exit status 0, its lock is gone, the
relay is idle, and the native GUI remains active. The worker checksum is
unchanged. [Mailbox recovery build 43641003631](https://github.com/SkiltonUSA/Termic-Amiga/actions/runs/43641003631)
passed all 35 proxy tests, packaging, bundled MCP smoke, packaged mailbox
acceptance and desktop lifecycle checks on macOS and Windows. The current
downloads are linked in [AI Proxy setup](ai-proxy.md). A physical Windows 11
to Amiga connection remains untested.

## Activity follows the latest line

The activity viewport used a conditional follow check and mixed TextEditor
pixel positions with line counts. Replacing content could leave the cursor
at the start, allowing a subsequent show/layout to return there. Activity
now moves the cursor to EOF and asks TextEditor to reveal it after loading,
appending or replacing content, and after the initial window opens. Selected
text still holds incoming updates until the selection is released. Typing
focus is not moved away from the input field.

The synthetic native MUI regression in `test_activity_scroll.c` checks long
content loading, following after manual scroll-up with an unchanged cursor,
appending, replacement, reopening, input focus preservation and short text.
It runs through `accept_activity_scroll.py` without opening production task
files or changing settings. The eight portable native test groups passed.
The GUI and regression probe cross-compiled with VBCC.

All native scroll checks passed on the Amiga's installed MUI/TextEditor
classes. The tested GUI, matching sources, ReadMe and AmigaGuide were
installed after staging, checksum verification and a rollback backup.
The GUI is 126304 bytes, CRC32 `A694DF95`. Production reopened at its original
window geometry and AI Proxy was restarted. No worker or proxy code changed.


## Proxy transport latency

A nine-frame physical Amiga benchmark used four pings, four synthetic text
updates and one source read, without cloud inference. The previous transport
had a median round trip of 3.80 seconds. With negotiated alternating reply
files the median was 1.55 seconds, about 59% lower (2.4 times faster). Repeating
the legacy mode against the candidate worker gave 3.91 seconds. These are
short local transport measurements, not end-to-end model response timings.
The fast run used no shell calls during the measured exchanges, versus eight
for the legacy run, and reduced directory calls from 73 to 54.

The proxy checks idle tasks every 0.5 seconds instead of two seconds, and
coalesces live output for 0.2 seconds instead of one second. Messages still
use checksum verification, strict sequence/ID checks and no automatic replay.
The native protocol rejects invalid reply slots before invoking tools.
Older hosts and workers retain the `.out` exchange. Both modes passed the
packaged worker acceptance; all 36 portable proxy checks and the native
protocol checks passed locally.


A repeat on the installed worker measured a 1.73-second median and then
saved and executed a synthetic ARexx script successfully through the fast
mailbox. An earlier attempt overlapped app autostart and was discarded;
the repeat held exclusive proxy ownership. The installed app also picked
up and completed a synthetic Codex task through the new reply mode.
The updated native worker is 76380 bytes, CRC32 `6C934D91`; it and the Mac
proxy were installed with rollback backups. The activity-scroll GUI remains
unchanged. Physical Windows-to-Amiga connectivity remains untested.

[Fast reply build 43644759972](https://github.com/SkiltonUSA/Termic-Amiga/actions/runs/43644759972)
passed all 36 portable proxy tests, packaging, bundled MCP smoke, both
legacy and slot-based packaged mailbox tests, and desktop lifecycle checks
on macOS and Windows. Updated downloads are in [AI Proxy setup](ai-proxy.md).
Final checks confirmed the installed Mac app matches the build, the worker
matches its deployed checksum, the GUI remains active, and the relay is idle.


## Batched mailbox checks

A further host-only optimization shares directory listings within each
message operation. Response and Stop polling use one fresh snapshot. Initial
upload checks are combined, but publication still requires a fresh listing
and checks Stop again after checksum verification. Cleanup refreshes the
snapshot after every deletion. No snapshot survives into another message.

The same nine-frame hardware benchmark used 35 directory calls instead of
56 and measured a 1.40-second median, versus the preceding installed run's
1.73 seconds. A synthetic ARexx save and execution also passed. These short
transport samples vary with bridge load and do not measure AI thinking time.
All 39 portable proxy tests passed, including new Stop-during-upload,
snapshot-refresh and pending-request preservation checks. The rebuilt Mac
bundle passed both mailbox modes, MCP smoke and desktop lifecycle acceptance.
The Amiga GUI and worker require no update for this change.


The repeat using the installed worker measured a 1.36-second median with
37 directory reads and passed the ARexx save/run check again. The native
worker and activity-scroll GUI are unchanged. The Mac proxy was replaced
with the tested bundle after a rollback backup.

[Batch-check build 43652953017](https://github.com/SkiltonUSA/Termic-Amiga/actions/runs/43652953017)
passed all 39 portable proxy tests, packaging, bundled MCP smoke, both mailbox
modes and desktop lifecycle checks on macOS and Windows. Final local checks
confirmed the installed app matches the tested build, the proxy is running,
the native binaries retain their previous checksums and the relay is idle.
Downloads are updated in [AI Proxy setup](ai-proxy.md). A physical Windows
to Amiga connection remains untested.

## Task workflow and development services

The portable suite passes 72 tests, including ASan/UBSan checks for persistent
queue serialization/recovery, allocation failures, review conflicts and interrupted
reverts, compiler locations, syntax modes and protected file names. The proxy
suite passes 47 tests. The rebuilt macOS bundle passes its GUI/MCP smoke, both
mailbox protocols and reconnect/Start/Stop/window-close acceptance.

On the physical 68060, the written execution suite passes ARexx, MicroPython,
native build/run, actual nonzero exit codes, stale-run rejection, failed-build
preservation and cooperative cancellation. Native Git branch creation/list/switch
passes on the long-filename project volume. A native host-Git request reads its
Amiga task sources through the relay and commits them into an isolated host
worktree without changing the host's original checkout. That acceptance uses a
local synthetic repository; it does not push or open a GitHub PR.

The dependency checker detects the installed CPU, AmigaOS, MUI 5, TextEditor,
SDK, MicroPython, Git, ARexx and AmiSSL requirements. Editor recolouring preserves
text, cursor, dirty state and Undo on the installed TextEditor class. Written MUI
checks cover accepting/reverting agent changes and creating/saving a scratchpad.

[Workflow proxy build 43671383900](https://github.com/SkiltonUSA/Termic-Amiga/actions/runs/43671383900)
passed the 47-test suite, packaging, bundled MCP smoke, both mailbox modes and
desktop lifecycle checks on macOS and Windows. The tested macOS bundle was
installed with an independent rollback copy. A physical Windows-to-Amiga
connection remains outside that CI acceptance.

A fresh isolated installation on the Amiga passed original SDK archive checks,
SDK/UI extraction, compilation of all five native programs and `TermicCheck`.
The written Kanban acceptance covers startup pause, ordered execution, failure
pause, explicit resume without replaying a failed entry, Done/Archive/Restore,
queue reorder/removal and clearing completed history. Review accept/revert and
scratchpad creation/save also passed in that installation. A second GUI launch
is rejected before changing the running installation's assign or preferences;
the hardware regression checks both the assign and original window identity.

The full queue regression was repeated with the final native-built GUI and
worker after repairing completion publication. A deterministic observer test
fails with the previous port-before-result ordering and passes with final
state, flushed output and result published before unregistering the worker.
The GUI now counts consecutive absent-worker polls and waits for registration
to disappear even when a result is already present. Time spent compiling no
longer consumes the missing-result grace period.

The process-monitor task also reproduced a native build that outlasted the
old 180-second proxy budget. The updated proxy completed a 298-second native
build with exit zero. A deliberately early `Quit 0` now fails with a structured
error and leaves the old executable unchanged; BuildProject must publish its
fresh completion receipt after promoting Program.new.

The Process Monitor task completed the actual native queue workflow: reviewed
source updates through agent file tools, queued VBCC compilation and self-test,
Review, acceptance of all six audited files and Done. The self-test enumerated
and sampled the physical Exec task list, rejected self/stale/dependency failures,
restarted its authenticated disposable service through the shared state machine,
and cooperatively stopped it. It never signalled an unrelated process.
All six remote source checksums matched the audited host files.

The read-only report detected the 68060 and ZZ9000 AutoConfig graphics/shared
RAM boards. Its live task data stays private. The synthetic MUI window opened,
resized to 1200 by 650 and 1000 by 580, and closed normally; both screenshots
were inspected. The task's review journal was empty before marking it Done.
Host tests also cover the actual UI control handler, including selected service,
confirmation, dependencies and ownership changing before restart.

CPU percentages are statistical VBlank samples. Per-process memory is partial
Exec tracking plus stack capacity, not RSS or a complete heap measurement.
Dependencies require opt-in service definitions. The installed legacy ZZ9000
firmware has no supported ARM task/load/private-heap API; those fields remain
unavailable, and no firmware/reset operation was attempted.

The final live-window check exposed selection scrolling as Exec moved tasks
between Ready and Wait lists. The monitor now orders its copied inventory by
address outside the scheduling/interrupt locks. Its queued native build,
disposable-service self-test, review and Done flow passed again. The installed
live window retained the selected row during refresh; its private screenshot
was inspected. The final monitor is 34844 bytes, CRC32 `44F08E90`.

The tested platform and monitor were installed with separate rollback copies.
Existing projects/preferences were preserved, and the new Process Monitor
project/task received missing AGENTS, CLAUDE and Termic skill guidance without
replacing project-owned files. Production window geometry was restored and
the AI Proxy restarted. The installed dependency checker reports zero missing
requirements.

The final native LHA and pinned AmiPkg entry passed upstream schema validation,
client parser/resolver/CPU constraints and a fresh host copy-plan installation.
The expected validator notice concerns the explicitly reviewed AmigaDOS pre/post
scripts. Those SDK/UI scripts and dependency checks also passed the earlier
fresh installation on the actual Amiga. The standalone monitor LHA passed its
archive integrity check and includes the tested binary, Workbench icon and source.

## Activity integration candidate (15 November 2026)

The native process monitor is now linked into TermicNative as View >
Activity / processes (Right Amiga I). The integration candidate cross-builds
without diagnostics for the workspace, standalone monitor and native
`tests/test_activity.c` harness. All 73 host tests pass, including CPU/memory
ordering, overflow boundaries, existing service-control guards and guide links.

The native lifecycle harness passed on the A4000TX: singleton opening,
selected-task retention through sorting, pause/resume, close/reopen,
timer/sampler cleanup and parent application survival. The full workspace
passed View-menu opening, resizing, closing and reopening Activity with real
Exec data. Synthetic normal, paused and resized screenshots were inspected.
The candidate adds no firmware changes or new runtime dependencies.
Native VBCC compilation also passed on the A4000TX. The resulting workspace
executable (181040 bytes, CRC32 `74230737`) was installed after backing up the
previous executable, source and guide. The installed version passed menu
opening, closing and reopening Activity without closing the workspace; its
live screenshot was inspected. TermicCheck reported zero missing dependencies.
The rebuilt local candidate LHA passed AmiPkg schema, parser/resolver, CPU
constraints and fresh host copy-plan validation, with the expected notice to
review the package's AmigaDOS installation scripts. This was an upgrade of the
existing installation, not a fresh target installation of that archive.
The published 0.1.0 Beta release has not been replaced by this candidate.

### Process column separators (15 November 2026)

Both the integrated Activity window and standalone Process Monitor now use
MUI's native multi-column list, title row and vertical bars. Numeric columns
align right; the process name column takes the remaining width. Both VBCC
cross-builds and the native lifecycle harness passed, as did all 73 host tests.
Synthetic screenshots confirm the separators and heading alignment at 1000
and 900 pixels wide.
The checked cross-built workspace (177624 bytes, CRC32 `DE846AB6`) and standalone
monitor (35304 bytes, CRC32 `BE523469`) were installed with rollback copies.
The installed Activity window passed opening, closing and reopening, and its
live screenshot was inspected. The Amiga-native compiler was not rerun for
this presentation change.

### Process Monitor menu and shortcut reference (15 November 2026)

Tools > Process Monitor now owns Right Amiga I; View > Activity / processes
remains an alternative entry without a duplicate shortcut. The in-app keyboard
reference, AmigaGuide, ReadMe and keyboard table document Right Amiga I and the
prompt-only F4 alternative. All 73 host tests and the VBCC cross-build passed.
The installed workspace (CRC32 `8DB7CB8E`) passed shortcut opening/closing.
Native menu and shortcut-reference screenshots were inspected. Automated mouse
menu selection did not open the window; mouse selection remains a manual check.
The prior executable and changed source/help files were backed up on target.

## 0.1.1 Beta monitor release (15 November 2026)

The optional ZZ9000 panel starts hidden in the integrated and standalone
monitors. The button shows/hides it and hidden panels do not query board/ARM
state. The native lifecycle harness verifies default visibility, show/hide,
reopening, sorting, pause/resume and sampler/timer cleanup. Synthetic screenshots
of both panel states, pause and resizing were inspected.

The earlier mouse-menu check is now superseded: a native input sequence from
`amiga/native/tests/click_menu.c` selected Tools > Process Monitor on the actual
Amiga, and the installed window opened, closed and reopened successfully.
This required no change to the menu event handler.

All 73 portable tests passed. The five native programs and standalone monitor
were cross-built with VBCC, installed with rollback copies, and their 0.1.1 Beta
version output checked on the Amiga. TermicCheck reported zero missing
dependencies. The existing TLS source emits an NDK timeval pointer-type warning
in this cross-build; authenticated direct-API execution remains unverified.
The standalone version output also now retains the initial T in Termic.

The native LHA and AmiPkg entry passed schema, resolver, CPU-floor, dependency
minima and fresh host copy-plan validation, with the expected installation-script
review notice. This was an upgrade on the target; a fresh Amiga installation of
this exact archive was not repeated. Physical Windows-to-Amiga testing remains
outstanding. The native direct-API credential file is empty, so no authenticated
API request was attempted. ZZ9000 telemetry/offload is not implemented by the
display toggle.

## 0.1.2 Beta workflow checks (15 November 2026)

All 74 portable tests passed, including unified-diff reconstruction and bounded
large changes, shortcut map validation, and queue worker limits. The five native
programs and standalone monitor cross-built with VBCC. The existing TLS timeval
pointer warning remains; no authenticated direct-API test was possible.

The native workflow fixture exercised shortcut Use/Cancel/Save/reload, independent
broadcast copies and their instructions, relay serialization, and two overlapping
native build workers. Both builds produced Amiga executables. A real input.device
mouse gesture moved a task from Review to Done and the persisted state was checked.
The board uses a local MUI pointer handler with a captured task path, destination
feedback and Esc cancellation. Native checks also covered cancellation, refusal of
manual Working transitions, and reserved-task protection. No synthetic agent
profile made a model request.

Synthetic captures of the board, shortcut preferences/reference, broadcast and
queue were inspected. Broadcast text imports explicitly apply light colours on
black; shortcut preferences use native columns. An unrelated bridge file-data
error interrupted the last queue capture after the other captures succeeded.
The fixture's successful completion and saved drop marker were recovered and its
binary checksum matched. The complete queue screenshot from the preceding run
was retained; production queue code was unchanged. Partial screenshot transfers
were rejected and retried, never treated as valid images.

The actual review window passed plain-text export checks in both unified and
before/after modes. The unified view uses green additions, red removals and blue
headers, with whole-file Accept/Revert operations. Large changes fall back to
replacement alignment without omitting file contents.

The LHA and AmiPkg entry passed schema, dependency minima/order, missing-dependency
rejection, CPU constraints and a fresh host copy-plan validation. AmiPkg reported
zero errors and its expected notice to review AmigaDOS installation scripts.
Physical Windows-to-Amiga use and authenticated direct-API execution remain
unverified. ZZ9000 information remains optional; ARM compute offload is not part
of this release.

The 0.1.2 upgrade was installed on the A4000TX after checksum verification of
rollback copies. All 24 updated files matched their staged CRCs. The five native
programs and standalone monitor reported 0.1.2 Beta; TermicCheck reported zero
missing dependencies. The normal workspace reopened at its original size, and
Tools > Process Monitor opened and closed successfully. User projects and
configuration were preserved. This was an upgrade, not a fresh target install.

## Proxy reliability and real agent workflow (15 November 2026)

All 83 portable tests passed, including 20 mailbox transport tests. The
proxy-focused subset passed all 56 checks. Added faults cover same-sized wrong
file bytes, file replacement between chunks, timeouts, incomplete uploads and
lost write/delete/rename acknowledgements. Assertions require single mutation
attempts, exact checksum verification and ID/sequence-matched replies. Four
recovery tests fail against the preceding relay implementation and pass with
this change. This demonstrates the tested recovery paths, not a diagnosis of
every earlier live bridge failure.

The rebuilt macOS desktop bundle passed its startup/stdio smoke test, legacy
and alternating-slot mailbox acceptance, and desktop start/stop/reconnect/close
checks. It was installed with a rollback copy of the previous app. No native
production executable changed; the installed Amiga version remains 0.1.2 Beta.

`accept_agent_workflow.py` and `tests/test_agent_workflow.c` exercised the real
production queue, worker, review and board functions in an isolated Acceptance
drawer through the installed desktop proxy. Codex authenticated with the Mac's
existing ChatGPT sign-in; no credential was copied to the Amiga. A saved Codex
profile was added while retaining the existing OpenAI/CLI defaults.

The agent read the fixture requirements, replaced the deliberately incomplete
`main.c`, compiled a native System Summary program with VBCC on the Amiga and
executed it exactly once. Both build and execution returned zero; the captured
output ended in `SYSTEM_SUMMARY_OK`. The generated source uses public Exec
interfaces to report library version and available Chip/Fast memory. It makes
no process, service or ARM hardware changes. Its source and native tool log were
inspected before accepting the file through the real review handler. The task
then reached persisted Done state, and the harness exited successfully. Ready,
coloured review and Done screenshots were inspected; these contain synthetic
task data and source, not the target's memory readings.

The harness invokes the real queue and review handlers through a control file;
this is not a manual mouse/keyboard test of the entire main window. The normal
workspace was reopened after the fixture, with no active worker or pending
request. Physical Windows-to-Amiga acceptance and authenticated direct API
requests remain outstanding. Host AI tasks remain serial.

To repeat the authenticated acceptance, compile `test_agent_workflow.c` with
the same GUI dependencies and embedded monitor used by `test_workflow_window.c`.
Compile `tests/inspect_chrome.c` as ChromeProbe. Keep the desktop proxy running,
use an editable native profile with ID `codex`, and supply a VBCC driver config
whose compiler, assembler, linker, headers and libraries use absolute target
paths:

```sh
python scripts/amiga/accept_agent_workflow.py \
  --binary /path/to/AgentWorkflowCheck --probe /path/to/ChromeProbe \
  --compiler-config /path/to/native-build.config \
  --output .context/agent-workflow-new
```

The output directory must be new. At Review, inspect the generated `main.c` and
`job.log`; put the source SHA256 from `review-ready.json` into `approve-review`
in that directory to continue. Approval is bound to the exact generated source.
Each run creates a separate target drawer and retains its generated program.

## Responsive native MUI workspace (15 November 2026)

The custom borderless title bar was replaced with native MUI/Intuition window
controls. The workspace uses a compact page selector below 800x500 and shows
Projects/Tasks plus Files/CLI beside the work page at larger sizes. Fonts now
follow MUI preferences. Large forms and auxiliary windows have native virtual
groups with automatic scrollbars. This is layout adaptation, not bitmap or
automatic DPI scaling.

`test_responsive_window.c` builds the actual workspace UI in a separate MUI
application with synthetic data. Its size/page matrix passed at 320x240,
340x280, 480x320, 640x480, 800x600 and 1024x800, including repeated transitions
between compact and wide layouts. All nine workspace choices were exercised;
unsaved source and instructions, dirty flags and the source cursor survived.
The editor Find field and CLI command field were checked inside the visible
window. Native borders and sizing gadgets were present. Agents, MCP access,
Kanban, queue, review, scratchpad, Git and the synthetic integrated monitor
also resized to 340x280. Large tables/forms use scrolling at this size.

The written host acceptance captured and inspected the compact editor, agent,
CLI and settings pages plus the 1024x800 workspace. A capture obscured by another
application was discarded; the final capture routine verifies foreground state
before and after transfer and requires all pixel rows. Screenshots contain
synthetic task text, model names and project paths. The tests resize windows
on the existing RTG screen; they do not switch Workbench display modes or
constitute a human test of native drag/zoom gestures.

All 83 portable tests passed. The workspace, standalone monitor and native
acceptance program cross-built. The installed workspace and 19 accompanying
source/manual files matched their staged CRC32 values, with a verified rollback
copy made before replacement. The new native-title workspace reopened, and
TermicCheck reported zero missing dependencies. Projects, credentials and
application configuration were retained. The standalone monitor was cross-built;
its separately installed project executable was not replaced in this update.
The native version remains 0.1.2 Beta; the published archive predates this UI.

To repeat the UI acceptance, compile `tests/test_responsive_window.c` with the
GUI dependencies, `TERMIC_EMBEDDED` and the process-monitor include directory.
Compile `tests/inspect_chrome.c` as the window probe, then run:

```sh
python scripts/amiga/accept_responsive_ui.py \
  --binary /path/to/ScalingCheck --probe /path/to/ChromeProbe \
  --output .context/responsive-ui-new
```

The output directory must be new. The fixture uses its own application identity
and Acceptance drawer; the normal workspace can remain open.

## Original workspace arrangement on MUI 5 (15 November 2026)

The screenshot reference is restored as Projects/Tasks on the left, the five
native MUI Title tabs in the centre, and Files above CLI on the right. The
header includes the version and Build, Run, Settings, About and Quit icons.
Task instructions sit beside the profile/YOLO/action controls below the
coloured activity log. Helvetica 13 and Courier 13 restore the earlier text
presentation, with MUI role fallbacks if those disk fonts are unavailable.
This supersedes the font choice and column order in the preceding update.
Native title, close, depth, size and zoom controls remain in use.

At compact sizes the cycle selector exposes all nine workspace panes; the
five work pages retain labelled native tabs. Hiding Title.mui directly left
empty tab outlines in the first hardware capture, so the tabs remain visible.
The final wide arrangement gives the agent controls enough room to display
the profile and YOLO values. Forms can still scroll on small windows.

The 83 portable tests passed. The native workspace, standalone monitor and
resize fixture cross-built without warnings. Hardware acceptance exercises
all nine panes from 320x240 through 1024x800, including repeated compact/wide
transitions and preservation of unsaved source, instructions and cursor.
The fixture also checks column order, tab notification/selector synchronization
and the visible right edge of the agent controls at 1024x800. Captures use
synthetic data; the normal workspace is not the screenshot fixture.

The complete hardware run passed, including 340x280 auxiliary windows and
the integrated monitor. Final 340x280 and 1024x800 captures were inspected.
The installed executable and four source/manual files matched staged CRC32
checksums. A verified rollback copy was retained, the native-title application
reopened, and TermicCheck reported zero missing dependencies. Testing resized
windows on the existing RTG screen; it did not change Workbench display modes.
The native version remains 0.1.2 Beta, and the published release archive has
not been replaced by this development build.
