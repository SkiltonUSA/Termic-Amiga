---
name: termic
description: Develop native Amiga programs in Termic Amiga using its C/H agent tools, native builds, CLI scripts and adapted MCP task routines. Use for work in an Amiga project, not for the desktop React/Tauri application.
---

# Termic Amiga

Use AGENTS.md for project conventions and the user's current request for scope.
Projects and tasks are independent source copies, not desktop Git worktrees.

For native development, read [build and scripts](references/development.md).
For external task orchestration, read [Amiga MCP](references/mcp.md).

The direct API worker loads this file and AGENTS.md at the start of a request.
Its read_file tool can read these instruction files and the linked references.
Its write tools remain restricted to C/H sources. Read-only profiles also
reject building. Use list_sources, read before changing source, then build.
Report what was verified and what still needs a user or hardware check.

The native app provides CLI, ARexx and MicroPython execution for user commands.
This does not give the direct API model an arbitrary command-execution tool.
The host MCP connector runs in host CPython. Native MicroPython is a separate
runtime on the 68k processor. No automatic peer pairing or subagent routing
is implemented by this Amiga port.

These are real project-local files. There are no links to external desktop
profile folders. Editing them changes guidance for the next agent request;
it does not install another agent executable or change its credentials.
