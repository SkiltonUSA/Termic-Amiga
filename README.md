# Termic Amiga

**Native MUI development workspace for AmigaOS. Version 0.1.0 Beta.**

This repository contains the Amiga implementation, separated from the desktop
[Termic project](https://github.com/simion/termic). It runs on the Amiga using
MUI and native 68k tools. It does not require the desktop React/Tauri application.

![Termic Amiga with CLI beneath Files](docs/screenshots/workspace-cli.png)

## Features

- Projects and independent source-copy task workspaces.
- Embedded editor with a black background and C/header syntax colours on load.
- Resolution-scaled blue title bar with close, depth and zoom icons, drag support and a black Files list.
- Source backups, undo/redo, clipboard and search.
- Native VBCC build/run and explicit AmigaDOS commands with captured output.
- Persistent CLI panel below Files, with a resizable half-height split, command entry and job output.
- Native Git initialization, status, diffs and source commits.
- Agent tools for C/header and ARexx files, native builds, ARexx/program execution and live hardware queries.
- Codex, Claude and Cursor host profiles, with model selection and live response/tool feedback on the Amiga.
- One black Agent activity pane with cyan responses, amber worker logs and a narrower instruction field beside the controls.
- Standalone Termic AI Proxy desktop app for macOS and Windows 11.
- Agents / CLI / Scripts profiles with separate credentials, launch arguments, sessions and access limits.
- CLI, ARexx and MicroPython script execution, with a pinned native MicroPython runtime.
- Native MCP/CLI routines for task orchestration, builds, source files and scratchpads.
- Editable prompt library with all 17 desktop Termic defaults and Save/Use/Cancel.
- Project-local AGENTS.md and Termic SKILL.md guidance loaded by the API worker.
- Compose, Session and Transcript keyboard reference, with Tab-to-send in the prompt.
- Persistent workspace selection, task status, model settings and an About window.
- Compact action icons with tooltips, native menus and Right-Amiga shortcuts.
- Editor commands in the menu strip, crisp Helvetica/Courier fonts and framed MUI panels.
- Save/Use/Cancel preferences and shared Amiga IFF text clipboard.
- Linked AmigaGuide manual available with Help or Help > Contents... .

![Live response and tool feedback during a synthetic running task](docs/screenshots/live-feedback.png)

![About Termic Amiga](docs/screenshots/about.png)

![Amiga preferences with Save, Use and Cancel](docs/screenshots/preferences.png)

See [Prompts, instructions and keyboard shortcuts](docs/prompts-and-instructions.md)
for the prompt library, project guidance and screenshot-style key bindings.

![Termic prompt library with black editor and action icons](docs/screenshots/prompts.png)

![Compose, Session and Transcript shortcuts](docs/screenshots/keyboard-shortcuts.png)

See [Agent execution on the Amiga](docs/agent-execution.md) for the shared
tools, execution results and hardware detection. Cursor collects slow operations
through bounded polling; start a **New conversation** after updating the proxy
and worker to load the revised tools. The proxy now checks task mailboxes
using direct file operations, with bounded retries for read-only bridge
errors. Updated proxy/worker pairs use alternating reply files to reduce
per-message bridge traffic and follow streamed output sooner. See [transport details](docs/codex-relay.md#mailbox-transport).

Start with [Termic AI Proxy for macOS and Windows 11](docs/ai-proxy.md) for
Codex, Claude and Cursor setup, downloads and model selection.
[Codex relay details](docs/codex-relay.md) describe the ChatGPT integration.

![Termic AI Proxy desktop application](docs/screenshots/ai-proxy.png)

![Claude profile with Amiga model selection](docs/screenshots/claude-profile.png)

![Cursor profile with Amiga model selection](docs/screenshots/cursor-profile.png)

See [Agents, CLI, scripts and MCP](docs/agents-and-mcp.md) for setup, client connections
and differences from desktop Termic.

![Native agent profiles with separate credentials and access settings](docs/screenshots/agents.png)

![Amiga MCP access with Save, Use and Cancel](docs/screenshots/agent-access.png)

[Amiga interface conventions](docs/amiga-interface.md) describes the menu,
preferences and clipboard behavior and its remaining limitations.

## Requirements and installation

AmigaOS 3.2, 68020 or newer, Fast RAM, MUI 3.8+ and TextEditor.mcc 15.56.
The application is also tested with MUI 5.0 and uses its native Title tabs.
Development uses VBCC and NDK3.2. Native Git requires long filenames and was
verified with AmiSSL 5.27. Cloud access additionally needs a compatible TCP/IP
stack, CA certificates, an accurate clock and your own OpenAI API key.

Press **Help** for the [AmigaGuide manual](amiga/native/Termic.guide), with linked
chapters, an index and optional ToolsDaemon Workbench launcher instructions.
Install `Termic.guide` beside `TermicNative`; it opens in MultiView.
**Shift-Help** opens About. MagicMenu and ToolsDaemon are optional and unbundled.

![AmigaGuide contents on Workbench](docs/screenshots/guide.png)

See the [native installation guide](amiga/native/ReadMe) for installation layout,
SDK setup, builds, project storage, shortcuts and limits. Dependency archives
are pinned by SHA-256 in [sdk-lock.json](amiga/native/sdk-lock.json).
The source is backed up here; an AmiPkg release has not yet been published.

## Build and verification

`amiga/native/BuildNative` builds TermicNative, TermicAgent, TermicStorage and TermicMCP
on the Amiga in the installation layout described in the guide. Their shared
version is defined in [version.h](amiga/native/version.h), independently of the
desktop version. All four support `--version` and AmigaDOS `Version FILE FULL`.

Run portable tests on a host with a C compiler and AddressSanitizer:

```sh
python3 -m unittest discover -s scripts/amiga -p 'test_*.py'
```

The opt-in `accept_chrome.py` checks the current custom window controls through
an already-running DevBench MCP server. Compile `amiga/native/tests/inspect_chrome.c`
with the Amiga compiler and `-lamiga`, then supply that helper with `--probe`.
It expects the 1920x800 acceptance desktop and a 1400x650 Termic candidate.
Keep the bridge window beneath Termic's upper-left corner for the depth check.
Use `--close-at-end` to include the close-button check. It uses injected mouse
input; do not run it while someone is using the Amiga.

The earlier `accept_mui.py`, `accept_colours.py`, `accept_about.py` and
`accept_style.py` preserve the native-title acceptance workflows. Their window
name guards target the earlier native title bar and require adaptation before
use with the current borderless window; do not bypass those guards.

`accept_agents.py --cli-panel-only`, with its required binary arguments, checks
CLI command entry, success/failure output, editor focus, copy, save/undo and
window resizing in an isolated Amiga installation. It restores the production
settings and clipboard afterward and requires no cloud credentials.
`--live-feedback-only` checks incremental transcript display before completion,
clipboard selection during updates, combined worker logs, tool start/return markers,
completion, cancellation and clearing the previous run's log. Compile
`amiga/native/tests/inspect_ui.c` with the Amiga compiler and `-lamiga` to
`.context/inspect-ui` for its clipboard checks. Stop the AI Proxy before
running hardware acceptance, then restart it after production is restored.
`amiga/native/tests/test_texteditor_stream.c` is an additional native MUI
regression for preserving text across streamed chunk boundaries.
`amiga/native/tests/test_activity_scroll.c` checks following the final line
on the installed MUI/TextEditor classes. Compile it with the same SDK
headers and `-lamiga`, then run `accept_activity_scroll.py --binary <probe>`
with AI Proxy stopped. It uses synthetic text and leaves production tasks
and settings untouched.

[Verification and limitations](docs/verification.md) distinguishes tested
behavior from functionality still awaiting hardware or authenticated checks.

## Scope

Tasks are independent source snapshots, not Git worktrees. The shell captures
commands; it is not an interactive PTY. Desktop agent CLIs run on the host through the proxy.
Queue/broadcast/race modes and integrated remote PR controls are not implemented.
ZZ9000 supplies the existing RTG display; ARM compute offload is not implemented.

## Credits and licence

Amiga adaptation: Dominic Skilton, with Codex assistance.
Original Termic: Simion Agavriloaei and contributors.
Development uses MUI, TextEditor.mcc, VBCC, vasm, vlink, NDK, AmiSSL, cJSON,
native Git and the [Amiga MCP bridge](https://github.com/geekychris/amiga_mcp).

Termic is licensed under the [GNU AGPL](LICENSE), version 3 or later.
Third-party components retain their own licences and notices.
This snapshot derives from the native Amiga work at
[commit 30562586](https://github.com/SkiltonUSA/termic/commit/30562586).
