---
name: termic
description: Develop native Amiga programs in Termic Amiga using its C/H, Python, ARexx and text agent tools, native builds, CLI scripts and adapted MCP task routines. Use for work in an Amiga project, not for the desktop React/Tauri application.
---

# Termic Amiga

Use AGENTS.md for project conventions and the user's current request for scope.
Projects and tasks are independent source copies, not desktop Git worktrees.

For native development, read [build and scripts](references/development.md).
For external task orchestration, read [Amiga MCP](references/mcp.md).

The direct API worker loads this file and AGENTS.md at the start of a request.
Its read_file tool can read these instruction files and the linked references.
Its write tools accept C/H, Python, ARexx (.rexx/.rx) and text/Markdown sources. Read-only profiles
reject writing, builds and execution. Use list_sources and read before editing.
Use build then run_program for C, run_arexx for ARexx, or run_python for a
saved .py script when requested.
Execution tools return output and exit codes; check them before claiming success.
Use system_info for detected hardware rather than project defaults.
Report what was verified and what still needs a user or hardware check.

The native app provides CLI, ARexx and MicroPython execution for user commands.
Agent run tools execute saved ARexx/Python and the freshly built Program; they do not
provide an arbitrary shell-command tool or interactive input.
The host MCP connector runs in host CPython. Native MicroPython is a separate
runtime on the 68k processor. No automatic peer pairing or subagent routing
is implemented by this Amiga port.

These are real project-local files. There are no links to external desktop
profile folders. Editing them changes guidance for the next agent request;
it does not install another agent executable or change its credentials.

Agent file writes are journaled for user accept/revert review. Do not overwrite
a conflict after a later user edit. The task board and persistent queue track
progress; completing a response does not mark the task Done for the user.
