# Agent execution on the Amiga

Direct OpenAI profiles and the Codex, Claude and Cursor host profiles share
one native tool dispatcher. The host forwards requests; file access,
compilation, execution and hardware queries happen on the Amiga.

| Tool | Result |
| --- | --- |
| `list_sources` | C/header and `.rexx`/`.rx` files in the task. |
| `read_source`, `write_source` | Read or replace the existing `main.c`. |
| `read_file`, `write_file` | Task C/header or ARexx files; reads also accept the named guidance files. Writes keep backups. |
| `system_info` | Live CPU flags/model, OS-visible memory, Exec/library versions, RexxMast status and raw AutoConfig board IDs. |
| `build` | Run the task's configured build, returning output and the build-script exit code. |
| `run_arexx` | Execute a saved task-relative `.rexx`/`.rx` file with `SYS:Rexxc/RX`. |
| `run_program` | Run the task's `Program` Hunk executable after a successful build in the current turn. |

Dispatched execution results contain `success`, `exit_code`, `output`,
`output_truncated` and `cancelled`. Preflight failures also return an
`error`. Validation refusals (such as an invalid path, read-only access or
a missing successful build) return an explanation without launching a command.
For ARexx, exit_code is the RX command status; an ARexx EXIT expression is a
returned value, not necessarily a failing DOS exit code. A completed
conversation means the model finished its response;
it does not establish that the requested program succeeded. Check the
execution result. Compiler failure never runs the previous build.

Source edits and script execution invalidate the current turn's successful
build. `run_program` consumes that build permission; build again before
another run. This avoids silently executing an older program after a failed
build or an edit. The normal user-controlled Run button remains independent.

Read-only profiles allow file reads, listing and hardware inspection. They
reject writing, builds and both execution tools at the native dispatcher as
well as the proxy. Paths reject device names, traversal and resolved paths
outside the task. Source files are limited to 32 KiB and returned execution
output to 16 KiB. The full captured output is task-local `.command.log`;
ordinary-sized output is also copied to the worker log and Agent activity.

ARexx and programs execute with ordinary AmigaOS access. The file checks are
not an OS sandbox for generated code. There is no general shell tool.
Arguments and interactive stdin are not provided by these run tools. Use the
CLI panel for those workflows. Builds retain the existing user-configured
build command.

Run tools wait until the program exits. Use bounded console tests for agent
work; close a GUI program to finish its execution. Stop sends a cooperative
break and retains the stop request if a program consumes Ctrl-C. A program
that ignores breaks can continue running. The proxy's tool-response wait is
180 seconds, so long-running applications should be launched from the CLI
or the application's normal Run control. A timeout is not an exit code or
proof that an executable stopped; never automatically replay that run.
The worker handles the VBCC runtime's Ctrl-C check itself so a stop during
file I/O still releases the task lock and public message ports. A native
build may finish its synchronous command before cancellation is reported.

Cursor receives an operation ID immediately and collects the actual result
with `await_operation`. Each poll waits at most 20 seconds; the original
operation continues between polls. Until its result is collected, another
tool request returns that same operation ID without launching another action.
Completed results remain available for the current task, and polls do not
consume native tool calls. This prevents slow Amiga operations from requiring
one long Cursor MCP request. The underlying native timeout and cooperative
Stop limits still apply. Claude and Codex keep their synchronous tool replies.

`system_info` reports what AmigaOS exposes at query time. CPU MHz, motherboard
marketing model and ARM availability are left unknown. Raw expansion IDs are
reported without guessing board names. Free memory is a changing snapshot;
OS-visible Chip RAM can exclude reserved addresses. The agent must use this
tool when asked for detected specifications instead of repeating project
context as hardware measurements.

After installing the updated native worker and host proxy, choose **New
conversation** to discard a session's old tool definitions. Credentials and
existing transcripts remain in place. Existing project guidance is preserved;
the updated tool definitions/instructions take precedence over older guidance
that describes a C/header-only agent.

## Example requests

With an editable agent profile selected, start a new conversation and ask:

> Detect this Amiga's CPU and memory. Save report.rexx to print those detected
> values, run it, and show the actual output and exit code.

For a C project:

> Read the sources, build this project, fix any compiler errors, then run the
> resulting Program once and report its output and exit code.

## Verification

Portable tests check schemas, read-only restrictions, ARexx file access,
invalid arguments/paths, build-before-run, and invalidation after edits and
failed builds. The native worker is cross-built with the installed NDK.

`scripts/amiga/accept_execution.py` is an opt-in hardware test using a separate
fixture task and the actual relay mailbox. Stop AI Proxy before running it.
Supply `--worker`, `--stop-probe` (compiled from
`amiga/native/tests/stop_job.c`) and an editable host-agent `--profile` with at
least eight tool calls. It performs no model request or user-task edit. It
checks ARexx success/failure, C build-and-run, nonzero program exits, failed
build preservation, stale-run blocking, hardware queries and cooperative Stop.
Restart AI Proxy afterwards.

`scripts/amiga/accept_stop_build.py --worker /path/to/TermicAgent --profile ID`
uses the earlier fixture's Stop helper to interrupt a deliberately delayed
build. It checks the cancelled result, final exit status, and removal of the
relay request, task lock and both public ports. It performs no cloud request.

`scripts/amiga/accept_execution_agent.py` follows that check with an
authenticated host-agent request: inspect hardware, write an ARexx script
using the detected values, and execute it. It uses the installed worker and
an existing host login. It defaults to the prior fixture's Codex profile.

`scripts/amiga/accept_build_agent.py` creates a fresh task containing an
intentional compiler error, asks the agent to build before editing, repair
the error, rebuild and run once. It verifies native log order, captured
output, final source and the Hunk executable independently of the model's
summary. Both checks request cooperative cleanup if a fixture fails while
its worker is active. No uncertain execution is automatically replayed.

Both scripts accept `--provider codex|claude|cursor` and `--profile ID`.
By default they copy the installed worker. To check a candidate before
installation, pass `--worker /path/to/TermicAgent`; it must be an Amiga Hunk,
and its transfer is checked by CRC32 and byte count.
For Claude or Cursor, supply an existing editable Amiga profile of that
provider, for example:

```sh
python scripts/amiga/accept_execution_agent.py --provider claude --profile custom-2
python scripts/amiga/accept_build_agent.py --provider cursor --profile custom-3
```

These profile IDs are examples; use the IDs from your own installation.
The scripts select the profile only in the new fixture task and do not
create or edit profile settings. Keep AI Proxy idle and stopped during the
checks and restart it afterwards. Evidence remains private under
`.context/execution-acceptance/`, in separate directories for each provider.
