# Agents, CLI, scripts and MCP on Amiga

Settings now opens two native MUI windows: **Agents / CLI / Scripts** and
**Agent access / MCP**. Their controls adapt the desktop registry to the
user-selected direct API and Amiga-native execution model.

## Agent profiles

The launcher on the Agent page selects a saved profile. Profiles are independent
of projects; the selected profile ID and YOLO choice are saved in the task when
it starts. The supplied profiles are OpenAI API and Amiga CLI. Desktop Claude,
Codex and other CLI executables are not bundled or marked as installed.

- Add API, Add Codex, Add CLI, Add ARexx and Add MicroPython create editable profiles, up to 32.
- Duplicate makes a separate profile. API duplicates need their own credential;
  they do not silently inherit another account's key.
- Built-ins can be edited and reset but not removed. Reset preserves credentials
  and custom profiles. Removing a custom profile does not delete its credential
  file; profile IDs are not reused. Tasks naming a removed profile must choose
  another before running.
- All / API and CLI agents / CLI and Scripts filters the list. Show in launcher controls
  new selections; a task's existing hidden selection is retained.
- Save persists settings to Config and ENV. Use updates ENV until reboot. Cancel
  closes without applying draft changes. Launches use applied settings.

An API profile chooses a model, key, source access, step limit (1–32), output
limit (256–16384 tokens), session resumption and token reporting. The built-in
OpenAI profile retains the existing main Settings key. An empty model uses the
main Settings model. An empty key field retains that profile's existing key.
Only OpenAI's Responses API is implemented for direct API profiles; other providers need their own
transport and request/response adapters.

Read-only profiles advertise only reading/listing tools and reject writing or
building even if a response requests them. Edit-and-build profiles retain the
existing bounded C/H file tools; neither mode gives the model a shell tool.
API sessions are separated by profile and task. New conversation resets the
selected profile's session. Usage displays actual input/output tokens supplied
by the API for each response, not a subscription quota or context-capacity estimate.

The **Codex on Mac** profile uses the [Mac relay](codex-relay.md) and ChatGPT
sign-in. Its key, CLI arguments, token limit and usage controls are disabled;
model, file access, tool-call limit and session resumption apply.

Native CLI and script profiles run a configured Amiga executable in the task drawer.
Default arguments are always passed. YOLO arguments are appended only when
selected in the launcher. Resume arguments apply after a successful launch of
that profile when resumption is enabled. Arguments are AmigaDOS shell syntax,
not POSIX syntax. Environment accepts one NAME=value per line (1023 bytes total)
and sets worker-local variables inherited by its child command, without writing
them to ENV:. Native commands have ordinary AmigaDOS access; they are not an OS
sandbox. Amiga CLI opens its own console; Termic tracks the launcher exit,
not the separate console's entire lifetime.

Desktop Keychain roles, Docker, host CLI login stores, live terminal YOLO
commands and external CLI hook installation are not implemented. Native workers
report their own running/completion/failure state. This is not full desktop
feature parity.

Profiles live in Config/agents.json, with ENV:Termic/agents.json taking precedence.
Keys are separate plaintext files: api-key for the built-in API profile and
key-PROFILE for other API profiles. The registry contains no API keys; environment
values can still be sensitive, so it is local configuration and is not packaged.

## Amiga MCP and CLI

TermicMCP is a native 68k executable. It presents adapted Termic routines through
newline-delimited JSON-RPC on standard input/output. It supports legacy MCP
initialization (2025-11-25) and modern server/discover metadata (2026-07-28).
The original desktop transport and tool names were consulted in
src-tauri/src/mcp_server.rs; the Amiga implementation uses native source-copy
tasks rather than desktop Git worktrees or PTYs.

Enable tools in **Settings → Agent access / MCP**, then Save or Use. The default
is disabled. Turning it off rejects further tool calls; it does not cancel
workers already started. No network listener or automatic startup service is
installed by this setting.

Native Shell connection:

```text
Termic:TermicMCP --stdio
```

A file-based CLI uses the same dispatcher:

```text
Termic:TermicMCP --request request.json --reply reply.json
```

Example request:

```json
{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"project_list","arguments":{}}}
```

For a client on another computer, run scripts/amiga/mcp_bridge.py with a Python
environment containing the `mcp` package. It uses an already-running DevBench
bridge; it neither starts that service nor changes its access policy. The bridge
and connector must share the host filesystem used for temporary transfers.
Requests and replies use temporary files that the connector removes afterwards.

Example MCP client configuration, replacing the paths with your local checkout
and Python environment:

```json
{
  "mcpServers": {
    "termic-amiga": {
      "command": "/path/to/venv/bin/python",
      "args": ["/path/to/Termic-Amiga/scripts/amiga/mcp_bridge.py",
               "--bridge", "http://127.0.0.1:55010/mcp"]
    }
  }
}
```

This is a stdio connection, not an HTTP endpoint at that bridge URL for Termic
clients: the URL belongs to DevBench, which transports requests to TermicMCP.
Configure the command in the client's MCP settings. Desktop Termic peer discovery,
pairing codes, peer_call and automatic Claude/Codex registration are not ported.

| Routine | Amiga behavior |
| --- | --- |
| project_list / task_list | List managed projects and independent task drawers. |
| agent_list | List profile names, IDs and types, without keys or environment. |
| task_new | Copy project C/H sources into a new named task; does not start an agent. |
| task_send | Save a prompt and start the task's direct API profile. |
| task_status / task_log / task_result | Read running state, worker output and saved transcript. |
| task_stop | Request cancellation of an MCP-created task's native worker. |
| task_build | Start a native build in an MCP-created task. |
| file_read / file_write | Read or save task-relative C/H files, retaining backups. |
| scratchpad_write / scratchpad_read / scratchpad_list | Keep notes in a task's hidden .scratchpads drawer. |

Project and task arguments use drawer IDs, containing letters, digits, `-` or `_`.
Every task routine takes both project and task. Tool schemas are authoritative;
unsupported desktop arguments are rejected rather than silently ignored.
Read and write payloads are bounded to 32 KiB, prompts to 8 KiB and RPC requests
to 64 KiB. New tasks receive Git ignore rules for metadata and scratchpads.

Writes and launches require tasks created by MCP, refuse active jobs and refuse
the workspace selected in the editor. A per-task lock serializes MCP mutations.
This preserves independent user workspaces. MCP can read managed task C/H files,
but cannot read Config credentials, select arbitrary device paths or run arbitrary
shell commands. The existing bridge grants access to the machine; this local
stdio adapter does not add its own network authentication layer.

The native UI is notified through an Exec signal when MCP changes tasks or a
worker finishes. It refreshes the task list without reloading the current editor.
Scratchpads are exposed through MCP; a native scratchpad editing tab is not yet
implemented. Direct API inference requires an API key. The separate Codex relay uses the
Mac's ChatGPT sign-in; see its documentation and verification record.

Protocol references: [stdio transport](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/stdio)
and [Streamable HTTP](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/streamable-http).

## CLI, ARexx and MicroPython

Build / CLI offers three command modes. CLI runs the entered AmigaDOS command.
ARexx prefixes the entered script path/arguments with `SYS:Rexxc/RX`.
MicroPython prefixes them with `Termic:Tools/MicroPython/micropython`.
All run in the selected workspace and capture output. For interactive input,
open the Amiga CLI profile and run the interpreter in that separate console.

Add CLI creates a `C:NewShell` profile. Add ARexx and Add MicroPython create
profiles with `Termic:Examples/hello.rexx` and `Termic:Examples/hello.py`
respectively. Set Default arguments to your own script path and arguments.
Existing profile IDs and credentials are retained; the original built-in
Amiga Shell display name becomes Amiga CLI, unless it was customized.

The editor and project/task source copies accept `.py`, `.rexx` and `.rx`.
These files use plain text; syntax colouring remains specific to C/H.
The cloud agent and MCP can also read the six named project guidance files.
Writes remain restricted to C/H sources.
This adds script execution, not custom Termic ARexx automation commands.
ARexx uses the system RX command and requires a running RexxMast.

The MicroPython runtime is pinned in `amiga/native/micropython-lock.json`.
Fetch it on the development host with:

```sh
python3 scripts/amiga/fetch_micropython.py --output /path/to/Termic/Tools/MicroPython
```

The fetcher verifies SHA-256 and includes upstream LICENSE, README and source
revision metadata. Local installation kits also include this runtime and the
sample scripts; `package_native.py --download` fetches missing dependencies.
The native runtime runs on the 68k CPU, not the ZZ9000 ARM processor. It is
MicroPython, not full desktop CPython; libraries must be checked for compatibility.
The existing host MCP connector continues to require host CPython and its `mcp`
package. Installing the native runtime does not replace that connector.

Upstream: [MicroPython Amiga port](https://github.com/OoZe1911/micropython-amiga-port).
ARexx command reference: [AmigaOS ARexx utilities](https://wiki.amigaos.net/wiki/AmigaOS_Manual:_ARexx_Command_Utilities).
