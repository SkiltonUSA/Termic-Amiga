# Codex on Mac, controlled from Termic Amiga

The **Codex on Mac** profile sends the Amiga task composer to a local Mac relay.
Codex uses its ChatGPT sign-in on the Mac. Source reads, edits and compilation
execute on the Amiga through the existing DevBench MCP bridge. No API key or
ChatGPT access/refresh token is sent to the Amiga.

![Codex on Mac profile in MUI](screenshots/codex-profile.png)

For the standalone desktop application and Claude/Cursor support, see
[Termic AI Proxy](ai-proxy.md). The command-line launcher below is optional.

## Install and start

On the Mac, install Codex CLI and sign in with `codex login`. Then, from this
repository:

```sh
python3 scripts/amiga/install_codex_relay.py
```

The installer creates a private Python environment with MCP 1.30.0 and copies
the relay scripts into `~/Library/Application Support/Termic-Amiga/relay`.
It creates `~/Applications/Termic Amiga Codex.command`. Double-click that launcher
and leave its Terminal window open. Ctrl C stops the relay. It does not modify
login items, system startup or your normal Codex configuration.

The existing DevBench bridge must already be running at
`http://127.0.0.1:55010/mcp`. For a different address, run the script with
`--bridge URL`. File transfers use MCP operations directly, without a shared
filesystem. No second DevBench bridge is started. Claude/Cursor tasks use a
private authenticated loopback adapter described in the proxy documentation.

On the Amiga:

1. Open **Settings > Agents / CLI / Scripts**, or press Right Amiga E.
2. Select **Add Codex**. Keep the model blank to use Codex's default, or enter a
   model available to your account. Choose read-only or edit/build/run access.
3. Save the profile, then select **Codex on Mac** beneath the task composer.
4. Enter a task and press Tab or the Start Agent icon.

Responses and tool progress appear in the transcript. Build / CLI shows worker
status. Stop (Shift F10) requests cancellation. New conversation discards the
selected profile's local thread reference; it retains the transcript. After
changing a profile's file-access mode, start a new conversation to refresh
Codex's advertised tool list. The native worker enforces the current mode
even for an older conversation.

The separate **Agent access / MCP** setting controls external TermicMCP callers.
A user-initiated relay worker does not require that setting to be enabled.

## Connection and scope

```text
Amiga composer -> native TermicAgent mailbox
                         |
                existing DevBench MCP
                         |
                Mac relay -> Codex app-server -> OpenAI
                         |
              bounded tool requests back to TermicAgent
```

Codex app-server speaks its own JSON-RPC protocol, not MCP. The relay uses its
experimental dynamic-tool interface and the existing MCP transport to the
Amiga. It does not use the removed `codex mcp-server` command. This integration
was developed against Codex CLI 0.166.0; recheck it when upgrading Codex.

Nine tools are exposed: list_sources, read_source, read_file, write_source,
write_file, build, system_info, run_arexx and run_program. Reads/writes cover
C/header and ARexx sources; reads also cover named project guidance. The
worker enforces task paths, backup saves and read-only access independently
of the host. Read-only mode blocks builds and execution. The worker owns the
task busy lock during execution. See [Agent execution](agent-execution.md).
The model never receives general-purpose DevBench bridge tools.

Local Mac execution environments, shell tools, apps, browser tools and subagents
are disabled for relay conversations. Additional permission requests are
rejected. Native builds still execute the task's configured AmigaDOS build
command with ordinary AmigaOS permissions, just as the existing native API
agent does. This is not an operating-system sandbox on the Amiga.

Project AGENTS.md and SKILL.md are included on each turn, bounded to 8 KiB each.
Conversation IDs are saved per task/profile and resumed on the Mac. The relay
uses its own Codex home/history directory and a local link to the existing
file-based Codex sign-in. It does not read or print the token contents. If the
Mac uses a keychain-only sign-in, sign into the isolated Codex home separately;
the installer reports that a file-based sign-in was not found.

Only one relay task runs at a time. Requests use a unique job ID and increasing
sequence numbers; stale or repeated frames cannot repeat a tool operation.
The Amiga waits up to 60 seconds for the initial connection and 120 seconds
between operations. The Mac sends keepalives while Codex works. A turn has a
25-minute Mac limit and a 30-minute native wait-loop limit, excluding time spent
inside synchronous native tool calls. An interrupted build may take time to
return from the native compiler before cancellation is observed.

If the relay stops or the bridge disconnects, the worker fails or times out
and preserves source backups. The desktop proxy reconnects after transient
bridge errors, using delays from 2 to 30 seconds. It retains the consumed job
ID across reconnects, so a lost acknowledgement cannot repeat an edit or build.
Send a new task after connection returns. An explicitly stopped proxy needs
Start or a relaunch. It does not automatically replay a failed task. A stale conversation that cannot resume
needs **New conversation**. There is no automatic fallback to a different model
or a new conversation.

Mailbox JSON frames are limited to 64 KiB, including escaped tool arguments.
Transcript updates use backup-and-replace writes and close the file between
messages so the GUI can read progress. The transcript limit is 256 KiB; archive
a full transcript before continuing. It is never silently truncated.

Assistant text deltas are combined for roughly one second on the host and
forwarded as plain ASCII text. Transfer/acknowledgement time can make visible
updates slower. Pending text is delivered before tool markers and turn
completion, and completed messages are reconciled against their deltas to
avoid duplication. Unsupported characters are replaced for the classic
TextEditor widget. The GUI refreshes only while jobs run, appends changed
transcripts, follows the latest line and holds selected text until it is released.
This live text path applies to host profiles; the direct API profile is unchanged.

The adapter uses the documented
[Codex app-server events](https://developers.openai.com/codex/app-server)
and checks thread/turn identity before accepting message deltas. The API output-token and usage-reporting controls are disabled
for this profile. **API steps / relay calls** bounds its tool calls (1–32); it is
not a subscription quota or a model-token budget. YOLO and CLI arguments do not
apply. ChatGPT account entitlements and usage limits still apply.

## Mailbox transport

The host checks the relay drawer through the bridge's directory and file
operations. A malformed reply or unreadable file is a connection failure,
never evidence that no task exists. Read-only listings, file reads and
checksum checks have bounded retries for transient bridge errors. Writes
are checksum-verified before one native rename publishes each message;
unconsumed requests are never overwritten. Deletion is checked against a
fresh directory listing. Commands and tool executions are not replayed
when a reply is lost.

Updated workers advertise optional `reply_slots`. An updated host then
requests alternating `.r0` and `.r1` replies. The next input acknowledges
consumption of the previous reply, which the native worker deletes locally.
The host polls the other filename, avoiding both shell cleanup per message
and file-lock races during replacement. Older peers continue using `.out`.
Final cleanup removes both slots. ID, sequence and checksum checks apply in
both modes; invalid slot/sequence combinations never execute a tool.

Reply and Stop checks share one directory listing per poll. Sending combines
its initial Stop, pending-input and temporary-file checks, then reads a fresh
listing after upload before publication. Stop arriving during upload prevents
publication. These snapshots are discarded after the operation and refreshed
after deletions; no directory cache is retained between messages.

Pending tasks are checked every 0.5 seconds (plus bridge round-trip time),
and streamed deltas are coalesced for 0.2 seconds before transfer. These
reduce transport delays; cloud model reasoning and native builds still take
their own time.

## Checks

`codex_relay.py --check` verifies ChatGPT authentication without model inference.
Portable tests cover mailbox ordering, stopped jobs, request validation,
read-only tool exposure, native replay rejection, tool-call limits, conversation
IDs, completion and local credential linking. The opt-in hardware test is:

```sh
python scripts/amiga/accept_agents.py --gui /path/to/TermicNative \
  --worker /path/to/TermicAgent --mcp /path/to/TermicMCP \
  --probe /path/to/ChromeProbe --micropython /path/to/micropython --codex-relay
```

It uses the signed-in ChatGPT account for synthetic test prompts. It closes
production normally, uses an isolated Amiga installation, preserves the clipboard
on the Amiga, and restores ENV settings, assigns and window geometry afterward.
Do not use the Amiga mouse/keyboard while the written UI suite runs.

Official references: [Codex authentication](https://learn.chatgpt.com/docs/auth),
[app-server protocol](https://learn.chatgpt.com/docs/app-server), and
[Codex MCP server removal](https://learn.chatgpt.com/docs/mcp-server).
