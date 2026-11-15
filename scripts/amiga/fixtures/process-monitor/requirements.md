# Process monitor requirements

Native C89 app, default opens full MUI window, title Termic Process Monitor.
Use supplied mui_compat.h. No TextEditor dependency needed. Crisp system fonts,
scalable window, process list, details, memory summary, ARM status, service controls,
Refresh / Stop selected / Restart service / About / Quit. Tooltips explain metrics.
Auto-refresh about one second without losing selection. Default at least 950x520.
Lists may use tab-separated strings and MUI_List_ConstructHook_String.
Main.c includes your headers; build command already links -lamiga -lmieee.
Own/release libraries, timers, interrupt, ports, allocations and MUI objects.

68k task/process inventory:
- Snapshot ExecBase ThisTask, TaskReady, TaskWait under Forbid, copy bounded
  names, pointer identity, state, signed priority, stack bounds, tc_MemEntry
  allocation totals. No malloc, printing, DOS I/O or waiting while forbidden.
- Max 256 rows and bounded list traversal; deduplicate ThisTask.
- tc_MemEntry is ONLY tracked allocation, NOT total process heap; label it partial.
  Stack capacity separately. OS3 has no complete per-process allocation ownership.
- CPU load must be an explicitly labelled statistical estimate, never invented.
  A VBlank server may count current ThisTask only when tc_State==TS_RUN; otherwise
  idle. Bounded fixed hash table counts, no allocation/DOS/Wait in interrupt.
  Use is_Data and register a1. Preserve all required registers and return Z set.
  Use a small explicit inline assembly wrapper if C return doesn't guarantee CCR.
  Interrupt priority BELOW 10; remove before freeing state. Copy counters with a
  short Disable/Enable, format outside. Show sample count and window duration.
  No task hooks, no kernel patches, no SetFunction, no MMU/cache changes.
- Name/address/state/priority/stack/tracked bytes/sample CPU%, global Chip/Fast
  free/total, overall busy/idle sample percentage. No false CPU clock claim.
- Task pointers must be revalidated against live lists before any Signal; compare
  captured name/stack identity too. Never dereference stale saved pointers.

ZZ9000:
- Read-only AutoConfig enumeration via expansion.library, manufacturer 0x6d6e,
  product4 graphics and product5 RAM. Report actual base/size.
- The installed firmware is legacy XX19c, XACP1.7 (configuration, not detected
  from OS). It predates SDK v2 service discovery; do not install new firmware or
  pretend to call current zz9k.library APIs on it.
- Core0 firmware handles display/USB etc; Core1 can run a custom bare-metal payload.
  No Linux-style process table exposed by this firmware. No register writes or
  reset/offload. Display ARM load, private heap, priorities, firmware dependencies
  as unavailable, with reason, rather than zero or made-up processes.
- FindPort("Sixies.ARM.Debug.Owner") under Forbid can report a cooperative 68k
  owner's name. Absence means no registered owner, NOT proof ARM is idle.
- Report ZZ shared AutoConfig RAM total; OS free memory within its MemHeaders may
  be shown labelled shared OS RAM, NOT ARM private heap. Optional if unsafe.

Dependencies and services:
- OS3 has no general per-process dependency graph. Show unknown unless declared.
  Never attribute every loaded system library to each process.
- Implement services.txt as opt-in line-based service definitions with display name,
  named port, start command, comma-separated required ports. Bounded parsing.
  Loading definitions does not execute anything. Document exact format and example.
- Stop selected sends cooperative SIGBREAKF_CTRL_C only after confirmation and
  revalidation; no RemTask or forced kill. Reject Exec tasks, self, system handlers,
  bridge, Termic/relay and other critical processes. Explain ignored break requests.
- Restart only explicitly configured services; validate dependencies first, confirm,
  request stop, WAIT until old port AND task identity are gone, then start once.
  If timeout, do not launch duplicate. Never auto-restart after crash/failed check.
  Avoid blocking the MUI event loop for the wait; use a pending state/timer.
- Arbitrary commands from user service configuration are explicit user actions.
  Never start/restart services merely on app launch. No default system-service entries.

CLI acceptance modes (must be bounded):
--report : sample ~3 seconds, print read-only report with metrics/limitations, exit0.
--fixture-service : own a unique Termic.Monitor.Fixture named port, allocate test
  memory, small workload and Delay, handle CTRL_C by removing port/freeing memory
  and exit. Refuse if port already exists; never take over an existing instance.
--self-test : refuse if fixture port already exists; launch ONLY this executable's
  --fixture-service, verify port and task, stop cooperatively, wait actual exit,
  restart once, stop again and verify cleanup. Check captured task data/percentages
  and stale selection rejection. Print PROCESS_MONITOR_SELFTEST_PASS only if all
  assertions pass, otherwise nonzero. Never signal any unrelated process.
--fixture-ui : show synthetic process names/values for screenshot acceptance,
  clearly label synthetic mode, disable real process/service control in this mode.

Keep service start/stop and identity code shared between UI and self-test.
Supply README.md with build/use/metrics limitations and services.txt syntax.
Build only at this stage. The coordinator will audit and execute acceptance next.

The monitor's own disposable Termic.Monitor.Fixture service is the explicit
exception to the critical-process name guard; its ownership must be verified.
For self-test launch, resolve this executable's actual drawer/name (not a guessed
installation path). Check both port disappearance and task exit before restarting.
Integer percentage arithmetic must avoid overflow. A zero sample count is N/A.
Keep selection by task identity across refresh; never silently select a replacement
row and signal a different process. Shared model/parser code should be portable
where possible so the coordinator can test malformed definitions and accounting.

AmigaDOS often names processes "Background CLI". Also copy the CLI command name
from a DOS process's pr_CLI / cli_CommandName BSTR, safely and bounded under the
snapshot lock (include dos/dosextens.h). Show executable/CLI identity alongside
that generic task name and address; do not collect command arguments. Critical
process guards must consider this command identity and owned public ports, not
only tc_Node.ln_Name. Never signal a bridge or Termic worker merely because its
Exec task name happens to be "Background CLI".

The bundled VBCC manual section 4.8 provides __amigainterrupt specifically for
AmigaOS interrupt-server functions: it suppresses stack checking and sets the
condition flags from the d0 return value. Use that attribute with a zero ULONG
return and __reg("a1") data argument for the VBlank sampler. __saveds may be
used for its data base. Do NOT use __interrupt, which returns RTE instead of RTS.
The coordinator will inspect the generated assembly before execution.

Keep the snapshot's forbidden section short. Use a shared budget (for example
4096 memory entries across the whole snapshot), not an unbounded or multiplicative
walk over all task allocation lists. Mark clipped counts as partial. Do not sum
stack capacity into tracked allocations, since the stack may already be registered.
