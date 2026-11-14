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

`scripts/amiga/accept_execution_agent.py` follows that check with an
authenticated Codex request in the same isolated fixture: inspect hardware,
write an ARexx script using the detected values, and execute it. It uses the
existing host login and saves evidence privately under `.context/`.

`scripts/amiga/accept_build_agent.py` checks the installed worker with an
existing Codex login. With AI Proxy idle and stopped, run it after the native
acceptance above using an editable Codex profile. It creates a fresh task
containing an intentional compiler
error, asks the agent to build before editing, repair the error, rebuild and
run once. It verifies the native log order, captured output, final source and
Hunk executable, independently of the model's summary. Evidence stays under
`.context/execution-acceptance/build-agent/`. Restart AI Proxy afterwards.
