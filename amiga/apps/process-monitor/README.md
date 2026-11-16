# Termic Process Monitor 0.1.2 Beta

Native MUI 5 application for AmigaOS 3.2, built through Termic's Kanban queue.
The task inventory covers the Amiga Exec CPU (68060 on the configured TF4060).
The optional ZZ9000 panel identifies its AutoConfig graphics/shared-memory boards and
checks the optional Sixies ARM debug-owner port without touching registers.

Build from this drawer with the Termic SDK installed:

```
Stack 262144
vc +aos68k -cpu=68020 -O1 -ITermic:SDK/MUI/Developer/C/Include main.c -o Program -lamiga -lmieee
Program
```

CLI modes: `Program --report` takes a short read-only sample; `--self-test`
creates, measures, restarts and stops its own disposable service. It refuses
an already-running fixture and never signals other processes. `--fixture-ui`
uses entirely synthetic data and disables controls, for public screenshots.
Use a 262144-byte stack, including the Workbench icon's Stack setting.

## Integrated Termic window

The same monitor is linked into TermicNative. Open Tools > Process Monitor...
(Right Amiga I), or F4 from the agent prompt. The child MUI window shares the
workspace event loop; jobs and editing continue. CPU/Memory sorts descend,
Name ascends, and Address restores stable ordering. Pause stops sampling;
confirmed service restarts continue. Closing releases the sampler and timer.
`services.txt` is beside TermicNative for this mode. No separate Program is
required. OS3 uptime and output counters are explicitly unavailable.

## Optional ZZ9000 information

The **ZZ9000...** button shows board and ARM information. **Hide ZZ9000**
returns that space to the process list. The panel starts hidden each time the
window opens, and hidden panels do not query ZZ9000 state. This is a display
option, not an ARM compute-offload switch. Unsupported telemetry remains
explicitly unavailable. The CLI `--report` still includes board information.

## Reading the display

The process table uses native MUI column dividers with aligned headings and
right-aligned numbers. The name column expands when the window is widened.

The resizable window lists executable/task name, address, Exec state, signed priority,
stack capacity, partial tracked allocation and sampled CPU percentage. Select
a row for its full task name, executable and declared service dependencies.
Rows remain ordered by address as tasks change state. Selection follows the
same task across refreshes; newly created or exited tasks can change row indices.

CPU is a statistical VBlank sample of the running Exec task, reset each refresh
interval. Short jobs can be missed; this is not cycle-accurate accounting.
The interval duration uses the system VBlank rate. Unassigned samples include
departed/unlisted tasks and hash collisions. Zero
samples mean unavailable. Global Chip/Fast figures are OS free/total values.
`tc_MemEntry` tracks only some allocations, not total private heap or RSS.
A plus sign marks clipped accounting. Inventory is bounded to 256 tasks,
512 nodes per task list and 4096 memory entries; clipping is reported.

The configured XX19c/XACP1.7 ZZ9000 firmware has no supported ARM process table,
per-process load, priority, private-heap or dependency API. Those measurements
are unavailable. AutoConfig shared RAM is not ARM private heap. An absent ARM
owner port does not prove the ARM is idle. No ARM firmware/reset command runs.
TF4060 is configured hardware; the 68060 flag is detected from Exec.

## Process control and opt-in services

**Stop selected** confirms and sends CTRL-C cooperatively. Processes may ignore
it. There is no forced RemTask: forcibly removing an OS3 task can strand locks
and corrupt shared state. Self, non-CLI tasks, system handlers, networking,
Termic/bridge/debug infrastructure and stale identities are refused. Identity
is revalidated under Forbid immediately before signalling; OS3 provides no
process generation identifier, so complete immunity to address reuse is not
possible. Inspect the selection before confirming.

`services.txt` beside the executable enables restart definitions, one per line:

```
# name | owning public message port | launch command | required ports
Example service|EXAMPLE.SERVICE|Example:Service|EXAMPLE.REQUIRED
```

These are trusted commands explicitly supplied by the user, executed with
normal AmigaDOS permissions. Loading definitions never executes them. Maximum
24 entries; duplicate names/ports, malformed fields and overlong lines are
ignored. Names/ports/commands/dependencies have 63/79/255/255-character limits.
Required ports are comma-separated; empty means no declared dependency.
This declares dependencies, it does not discover library or device ownership.

**Restart service** requires a live owning task and all declared required ports.
It confirms, requests CTRL-C, waits at most 10 seconds for BOTH old task and
port to disappear, then launches once with independent NIL: streams and a
262144-byte stack. It waits another 10 seconds for the service port to return.
A timeout never launches a duplicate or retries automatically. Inspect service
state before another attempt. Closing the monitor cancels pending supervision;
a newly launched service is independent of the monitor.

The self-test uses two owned ports, including a per-run proof port, to permit
only its own otherwise-protected fixture. Paths containing AmigaDOS escape or
substitution characters are refused by the fixture launcher. This restriction
does not rewrite user-supplied service commands.

AGPL-3.0-or-later, Termic-Amiga contributors.
https://github.com/SkiltonUSA/Termic-Amiga

Implementation references: Exec's
[AddIntServer](https://developer.amigaos3.net/autodocs/exec.library/AddIntServer.html),
[Signal](https://developer.amigaos3.net/autodocs/exec.library/Signal.html) and
[DOS SystemTagList](https://developer.amigaos3.net/autodocs/dos.library/SystemTagList.html).
The interrupt server returns zero with Z set and performs no allocation or OS
calls. Task identity checks use bounded traversals while scheduling is forbidden;
only counter copying/reset briefly disables interrupts.
