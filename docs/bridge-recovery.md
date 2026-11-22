# Acceptance gateway and recovery

Hardware acceptance clients use `http://127.0.0.1:55110/mcp`.
The backend on port 55010 is not a supported direct endpoint.
All three `accept_about.py`, `accept_colours.py` and `accept_mui.py` accept
`--server`, `--output` and `--reservation-file`. Tool requests time out after
60 seconds, control requests after at most 45 seconds, and cleanup after
15 seconds. Screenshot conversion has a separate 30-second deadline.

## Reservation recovery

Each run has its own private token file. Output contains
`bridge-recovery.json`, which records the endpoint and **path**, never the
token. Tokens live in `~/Library/Application Support/Termic-Amiga/bridge-reservations`
on macOS, or `%LOCALAPPDATA%/Termic-Amiga/bridge-reservations` on Windows.
Do not copy tokens into evidence, chat, or version control. Process locks
prevent concurrent use of the same recovery file or output directory.
Different runs do not automatically adopt another run's reservation.

An ordinary assertion failure releases ownership only after the gateway is
enabled and idle, no `TermicJob.` ports remain, and any registered UI cleanup
has succeeded. Transport failures, uncertain tool replies, paused gateways,
and unverified UI state retain the token and print its recovery-file path.
Cleanup failures preserve the original exception. `passed.json` is written
only after cleanup and session shutdown succeed.

After inspecting the target and resolving any unfinished operation, rerun
with the same output directory and the printed private path:

```sh
python3 scripts/amiga/accept_about.py \
  --output .context/amiga-about \
  --reservation-file "/private/path/from-the-failed-run.json"
```

About and colour recovery reruns their complete checks. A MUI interruption
after the fixture commit can resume with:

```sh
python3 scripts/amiga/accept_mui.py \
  --output .context/amiga-mui \
  --resume-task "Termic:Projects/acceptance-example/Tasks/task-0001"
```

`--resume-task` reads the original output's recovery pointer, then verifies
the selected task and fixture commit before continuing. For an older run
without that pointer, supply its printed `--reservation-file` explicitly.
There is no automatic discovery of random token files. A fresh run refuses
to overwrite unresolved recovery evidence. Use separate output directories
for independent runs.

If the gateway is paused, inspect its status and the Amiga first. Resolve
pending requests, requesters or jobs, then have the gateway operator clear
the pause through the gateway controls before resuming. These scripts never
enable it automatically or bypass it. A rejected token is preserved with
an explanation: another run may now own the gateway. Do not release that
other reservation or delete its files. When the gateway confirms there is
no reservation, the obsolete local token is discarded safely. If acquisition
failed before a token could be saved, gateway-operator recovery is required;
there is no valid local token to invent or replay.

The UI end-state contract is one frontmost `Termic Amiga Workspace` window,
with no About requester or test `Termic Project` window. About cleanup may
close its single frontmost requester with Escape. It never injects cleanup
keys into an unrelated or ambiguous window. The workspace stays open for
the next test, as the existing tests expect; this does not restore the
user's original project or executable. Uncertain failures require inspection
instead of blind Quit/relaunch. No claim of release or success is made until
this state and the gateway's idle state have been checked.

Run the reservation tests without MCP or physical hardware:

```sh
python3 -m unittest discover -s scripts/amiga -p 'test_bridge_recovery.py'
```

The fake gateway covers idle release, assertions, timeouts, cancellation,
stale tokens, paused recovery, concurrent clients, cleanup failures and
window ownership. A separate-process check verifies output-directory locking.
