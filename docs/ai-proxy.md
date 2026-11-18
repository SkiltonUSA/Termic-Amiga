# Termic AI Proxy for macOS and Windows 11

Termic AI Proxy is a standalone desktop application. Open it to connect the
Amiga task composer to Codex, Claude Code or Cursor on your host computer.
It bundles Python, Qt and the MCP client. The three vendor CLIs and the
DevBench bridge are separate prerequisites.

![Termic AI Proxy with synthetic account status](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.4-beta/ai-proxy.png)

Proxy build `11fb72f` includes host Git worktrees, native MicroPython tools and
a 15-minute native build reply budget. Other tool replies retain a 3-minute
limit; cancellation stays available and timed-out builds are never replayed.
Verified bundles are available as
[macOS Apple Silicon download](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.3-beta/Termic-AI-Proxy-macOS.zip)
and [Windows 11 x64 download](https://github.com/SkiltonUSA/Termic-Amiga/releases/download/v0.1.3-beta/Termic-AI-Proxy-Windows11.zip).
Both passed 47 portable tests, packaging, MCP smoke, both mailbox modes and
desktop lifecycle checks. Update the native worker and proxy together to use
the new commands. Older peers retain their compatible transport but do not gain
new native tools from a host-only update. Build the native worker with
`BuildNative`. Release downloads are public; the separate CI artifacts described
below require GitHub sign-in and expire after 30 days.

## Start the app

On macOS, open **Termic AI Proxy.app** from Applications. On Windows 11,
extract the Windows download completely and open **Termic AI Proxy.exe**.
Keep the `_internal` folder beside the executable. No Python installation or
Terminal window is needed to run the packaged app.

Additional development builds are attached to successful
[AI Proxy desktop builds](https://github.com/SkiltonUSA/Termic-Amiga/actions/workflows/ai-proxy.yml)
runs. The macOS artifact contains an app ZIP; the Windows artifact contains
an executable and its dependencies. GitHub requires sign-in for these artifacts,
and retains them for 30 days. Builds are unsigned development builds, not
notarized releases. The release Mac app is ARM64 (Apple Silicon); build locally
on Intel macOS for an Intel bundle. The Windows download is x64.

The window provides:

- Start and Stop, with task progress and connection errors.
- Automatic reconnection after temporary bridge failures, with a visible
  Reconnecting status and retries capped at one every 30 seconds.
- Automatic connection when the app opens (enabled initially).
- A configurable DevBench MCP URL.
- Executable paths, Browse, Check and Sign in for each provider.

Closing the app stops its relay and requests cancellation of the active host
agent. It waits for cleanup; a synchronous native build can delay stopping.
Reconnection preserves the last consumed task ID for the running proxy; it
does not replay a task after a lost acknowledgement. Send a new task from the
Amiga after a failed or timed-out request. Stop remains available during retry.
It does not install a login item, service or background startup task.
Only one proxy can own the relay at a time. Stop the old Terminal-based relay
before opening this application.

## Host sign-in

Install the vendor CLI on the host. Use **Browse** if it is not on PATH.
If the proxy is running, click **Stop** first. Use **Sign in** to complete
the provider's browser authentication, then click **Start proxy**. **Check**
checks stored login state without running a model; a saved login can still be
expired, or have no remaining usage. A real request is the final authentication
check.

| Provider | Host CLI | Amiga model field |
| --- | --- | --- |
| Codex | `codex` | An account-supported Codex model ID; blank uses its default |
| Claude | `claude` | A Claude Code model ID or supported alias such as `sonnet` |
| Cursor | `cursor-agent` / `agent` | A Cursor model ID; blank uses its default |

Model names are passed as process arguments, never shell commands. There is
no automatic fallback to another model. Invalid or inaccessible models fail
with an error in the Amiga transcript. The app does not contain a frozen list
of model names or imply that every subscription includes every model.

Claude uses a Claude.ai subscription login. Cursor uses the login in the
proxy's dedicated Cursor configuration directory. Codex uses the existing
host file-based ChatGPT login through a local auth link, or a local copy on
Windows where creating a symlink is unavailable. Credentials never travel to
the Amiga. Existing normal Cursor settings and MCP servers are not modified.

On Windows, select native `.exe` CLIs. The npm `codex.cmd` installation is
also supported by launching its Node entry directly. Arbitrary batch wrappers
are rejected. WSL-only installations are not discovered by the native app.

Official installation and usage references:
[Codex](https://learn.chatgpt.com/docs/cli/reference),
[Claude Code](https://code.claude.com/docs/en/cli-reference),
[Cursor on macOS and native Windows](https://cursor.com/docs/cli/installation).

## Bridge connection

All Amiga-facing clients use `http://127.0.0.1:55110/mcp`, the Amiga-MCP
reservation gateway. Amiga-MCP and its connected DevBench backend must already
be running. DevBench remains on port 55010 behind the gateway; clients must
not bypass it. The physical Amiga TCP port is unchanged. The proxy does not
start either service. Saved proxy settings using the old localhost port are
migrated on launch; custom endpoints are preserved.

The proxy, stdio connector and hardware acceptance scripts reserve protected
operations automatically. Idle polling does not hold a reservation. After a
task, the proxy releases only when the gateway is enabled with an empty queue
and no Termic job ports remain. Another client's reservation is respected;
the proxy waits and reconnects without replaying an interrupted task.

The proxy keeps its recovery token privately in `bridge-reservation.json`
beside `proxy.json` and resumes its own reservation on reconnect. Other
clients save private recovery files in the host's Termic-Amiga support
folder under `bridge-reservations`; a retained reservation's path is printed
to stderr. Tokens must not be posted in logs or committed. An uncertain
failure retains the reservation. Inspect gateway status and finish hardware
cleanup before resuming with that token and explicitly releasing it. Clients
never enable a paused gateway or remove another client's reservation.
Hardware tests remain responsible for stopping any applications they launch;
closing an MCP session alone does not prove those applications have stopped.
Screenshot clients use `amiga_screenshot` through MCP; port 55110 does not
provide DevBench's REST screenshot route.

The proxy can connect to a bridge on another host. Transfers use MCP file
read/write operations and verify uploads by CRC32, so no shared filesystem
is needed. Use the bridge's reachable URL or an SSH tunnel. For example, a
Windows proxy can use a tunnel to a Mac bridge bound to loopback; configuring
that tunnel and its SSH login is separate from provider sign-in. Do not expose
an unauthenticated DevBench bridge to the public internet.

## Amiga setup

1. Open **Settings > Agents / CLI / Scripts**.
2. Choose **Add Codex**, **Add Claude** or **Add Cursor**.
3. Set **Model**, file access, and the relay call limit. Save or Use.
4. Select that profile beneath the task composer and send with Tab.

The existing profile labels say **on Mac**; they also route to a Windows proxy.
History is separate by task, profile and provider for Claude/Cursor. Changing
a model applies to the next turn. **New conversation** clears that provider's
thread reference and retains the transcript. After changing file access,
start a new conversation to refresh the advertised tool list.

All three providers share nine native tools: list_sources, read_source,
read_file, write_source, write_file, build, system_info, run_arexx and
run_program. The native worker enforces task paths, backups and read-only
access. Read-only mode blocks writes, builds and execution. See
[Agent execution](agent-execution.md) for output, exit codes, hardware queries,
Stop behavior and run limits. General DevBench tools remain unavailable.

Claude and Cursor consume these tools through a private stdio MCP adapter.
Cursor additionally uses `await_operation` to collect each native result in
polls of up to 20 seconds. Polling retains the original operation, does not
consume the native call limit, and does not repeat a build or program run.
Start a new conversation after upgrading so Cursor receives the revised tools.
The adapter talks to an ephemeral loopback-only listener with a per-task
secret. It has no independent bridge credentials or arbitrary command tool.
Claude's built-in tools, skills, hooks and other MCP configurations are disabled
for these tasks. Cursor uses a dedicated empty workspace and configuration,
with local shell, file reads/writes and web fetch denied. This is provider
permission configuration, not an OS sandbox or a guarantee about every future
Cursor built-in tool. Native Amiga bounds remain enforced independently.
The proxy registers its MCP adapter in the dedicated workspace's
`.cursor/mcp.json` for each turn and removes that registration afterward.
`CURSOR_CONFIG_DIR` keeps CLI settings and authentication separate; it does
not replace the documented [workspace MCP location](https://cursor.com/docs/mcp).

Limits, cancellation behavior and transcript handling are shared with the
[Codex relay](codex-relay.md). One task runs at a time. No ARM offload or
standalone cloud authentication is added to the Amiga.

## Build and test

Use Python 3.14 or later on the target OS:

```sh
python -m pip install -r scripts/proxy/requirements.txt
python scripts/proxy/check.py
python scripts/proxy/build.py
```

Run `scripts/proxy/smoke_bundle.py` with the built executable path. This checks
Qt startup, a synthetic screenshot and real bundled stdio MCP communication.
The GitHub workflow runs these checks on macOS and Windows. It uses synthetic
data, needs no cloud secrets and does not contact an Amiga.

Native profile and tool integration can be checked with
`accept_agents.py --providers`, in addition to its required binary arguments.
Use `--host-agent claude` or `--host-agent cursor` for authenticated
read/write/build and conversation-resume acceptance using the host login.
This closes production normally, runs synthetic Claude/Cursor tasks in an
isolated installation, and restores settings, clipboard and window geometry.
The `--providers` mode uses synthetic MCP clients; only the opt-in
`--host-agent` mode tests cloud authentication. See [verification](verification.md)
for the actual tested scope.

## Live response and tool feedback

Codex, Claude and Cursor host profiles stream visible assistant text into the
Amiga transcript. Updates are batched for about one second, then delivered
through the existing mailbox; bridge latency determines the visible cadence.
Pending text is flushed before tool start markers and completion. The
transcript includes a marker when each Amiga tool returns, including failed
compiler results; a returned tool does not by itself mean a successful build.
The CLI panel shows native tool activity while the worker is running.

Replies wrap to the activity pane width. Activity opens at the latest line
and follows new output automatically. Selecting text temporarily holds
updates for copying; releasing selection catches up to the bottom. The existing 256 KiB transcript
limit still applies. A disconnect never replays an uncertain text delivery.

Mailbox reads verify the complete file against its named CRC32 and byte count.
Transient read failures, incomplete chunks and replaced files are retried up to
three times. A missing write acknowledgement is recovered only when a checksum
proves that the exact uploaded prefix arrived. Deletes require a fresh directory
listing proving absence. If publication loses its acknowledgement, the proxy
waits for the worker's matching task ID and sequence reply without publishing
again. These checks reduce false failures without risking duplicate builds or
program executions; persistent failures still interrupt the task.

Claude uses its documented
[partial message events](https://code.claude.com/docs/en/agent-sdk/streaming-output).
Cursor uses its documented
[partial-output filtering](https://cursor.com/docs/cli/reference/output-format)
to skip duplicate pre-tool and final flushes. The direct OpenAI API profile
does not gain token streaming from this update.

## Task worktrees and GitHub controls

The Amiga's **Right Amiga G** dialog uses this proxy for isolated host Git
worktrees, sync/commit and confirmed PR creation. Install Git and GitHub CLI
(`gh`) on the host and authenticate there. The credentials stay on the host;
no AI model is involved. See [Task workflow](task-workflow.md#git-branches-host-worktrees-and-github)
for repository/branch setup and dirty-worktree protection.
