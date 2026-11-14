# Prompts, project instructions and keyboard shortcuts

Open **Prompts** beside the task composer, **Settings > Prompts**, or press
**Right Amiga P**. The library imports all 17 defaults from desktop Termic:
Review, Deep review, Write tests, Security review, Explain the changes,
Commit, Commit and push, Fix merge conflicts, Verify end to end, Fix the bug,
Work on the issue, Simplify, What is the state?, Continue from last,
Hand off to another agent, Update the docs and Research first.

Titles and bodies are editable. New, Duplicate, Remove, Reset selected,
Restore missing defaults, Up/Down and Enabled manage the library.
Save writes `Termic:Config/prompts.json` and the current ENV preference;
Use writes only `ENV:Termic/prompts.json`. Cancel discards library edits.
Insert + Use appends the selected prompt to the current task composer and
uses library edits until reboot. It preserves existing task text and refuses
insertions that exceed the composer's 8192-byte limit. Choose the agent profile
and press Tab to send. It never sends automatically.

The C implementation follows the desktop store's delta model: customs,
overrides, deletedBuiltins, disabled and order. Unedited defaults follow the
shipped version; edited bodies remain yours. Limits are 48 prompts, 95-byte
titles and 8192-byte bodies. Desktop labels, destination routing to parallel
agent tabs and queued sends are not implemented.

The shipped bodies come from `src/store/prompts.ts`, `src/lib/builtinPrompts.ts`
and `src/lib/review.ts`. `amiga/native/Prompts/provenance.json` records the
source revision. To refresh from a local desktop checkout:

```sh
node scripts/amiga/import_prompts.mjs /path/to/desktop-termic
```

These are reusable agent instructions, not executable AmigaDOS scripts.
The import retains the upstream bodies, converts typography to ASCII and adds
an Amiga tool-limit preamble. The native API agent has no arbitrary shell,
Git/push, browser, PR or subagent tools. Templates requesting those operations
must report the limitation or use a capable external agent. POSIX examples
in upstream templates require translation before use with AmigaDOS.

Agent profiles and MCP settings are also available directly through
**Right Amiga E** and **Right Amiga M**. Common action buttons use compact
icons; hover for their names. The native menus retain text labels.

## Project guidance

New projects include these real files; new tasks copy them locally:

```text
AGENTS.md
CLAUDE.md
.agents/skills/termic/SKILL.md
.agents/skills/termic/.termic-version
.agents/skills/termic/references/development.md
.agents/skills/termic/references/mcp.md
```

For an existing project or task, select it and choose **Workspace > Install
agent instructions**, or use the `/instructions` command. The worker copies
missing files from `Termic:Template` and preserves existing files. The CLI
form is `Termic:TermicAgent --instructions --project Work:MyProject`.
The Files list exposes these documents for editing with normal backup saves.
`CLAUDE.md` is a compatibility pointer; it does not install a Claude executable.
No links into external desktop profile folders are created.

For each native API request, the worker includes the current `AGENTS.md` and
`.agents/skills/termic/SKILL.md` as project guidance, up to 8192 bytes each.
Missing, unreadable or oversized files are omitted. The six named files are
also available through the API and MCP `read_file`/`file_read` tools. Agent
writes accept C/H and ARexx; task-orchestration MCP writes remain C/H-only.
MCP-created tasks copy the six files.
Path traversal and files resolving outside the workspace remain blocked.
The current user task takes precedence over project defaults.

## Keyboard

**Right Amiga K**, **Help > Keyboard shortcuts**, or the **Shortcuts** button
opens the black reference window, grouped like the supplied screenshot.
`?` in an empty prompt opens it too. Escape closes it.

| Context | Keys | Action |
| --- | --- | --- |
| Prompt | Tab | Send current message through the selected profile |
| Prompt | Shift Tab | Cycle agent profiles |
| Prompt | Ctrl J | Insert newline |
| Prompt | Ctrl G | Focus built-in code editor |
| Prompt | Ctrl R | Search transcript history |
| Prompt | Esc twice, within the system double-click interval | Recall last sent message for editing |
| Empty prompt | `/` | Commands palette |
| Empty prompt | `@` | Mention the file currently open in the editor |
| Empty prompt | `!` | Focus CLI command field; does not execute |
| Prompt | F4 | Inspect activity in Build / CLI |
| Prompt or transcript | Ctrl T | Focus transcript |
| Prompt or transcript | Ctrl C | Quit through normal save handling |
| Transcript | F3 | Focus history search; Return finds next with wraparound |
| Transcript | PgUp / PgDn | Scroll |
| Transcript | Ctrl Home / End, or Shift Alt comma / period | Top / latest |
| Transcript | Ctrl Space, then Up / Down | Select lines starting at the top visible line |
| Transcript | Ctrl Space | Finish selection; Right Amiga C copies |
| Transcript | Esc | Return focus to prompt |

Other controls retain their existing Tab behavior. Right Amiga V pastes text.
US key positions are used for symbols. Extended Home/End/PgUp/PgDn keys depend
on the keyboard adapter; Shift Alt comma/period work on classic keyboards.
The previous function-key shortcuts remain in other contexts.

The palette offers Prompts, keyboard reference, instruction installation,
CLI, editor and transcript. Pasted slash commands followed by Tab accept
`/prompts`, `/keymap`, `/help`, `/commands`, `/instructions`, `/cli`, `/editor`
and `/history`. Unknown commands stay in the composer and are not sent.
Image paste, reasoning adjustment, subagent switching and custom keymap
editing are unavailable. The reference window labels those limits.
