# Task board, queue and review

Open **View > Kanban board** with **Right Amiga Y**. The board uses the same
lifecycle as desktop Termic: **Backlog**, **Needs attention**, **Working**,
**Review**, **Done** and **Archived**. It shows managed project workspaces and
their tasks, with independent column scrolling, counts, search and an
all/current-project filter. Search matches task or project names; press Return
to apply it. Up to 128 matching workspaces are displayed.

Select a row to open its workspace, queue an agent/build, review agent edits,
mark it done, archive it or restore it to Backlog. Working reflects actual
registered jobs. A completed agent run reaches Review; it does not mark the
task Done on your behalf. Queued/running tasks cannot be marked done or archived.
Drag a task with the left mouse button between Backlog, Needs attention, Review,
Done and Archived (Esc cancels), or
use the buttons. A drop saves the task state; it never starts a job. Working
accepts only real running jobs. Queued/running tasks reject state changes,
and a failed save leaves the task in its previous column.

## Persistent queue

**Right Amiga J** opens Queue. Add the current agent instruction or build,
move pending entries up/down, remove pending/finished entries and clear
finished history. The queue holds 64 entries. Queued workspaces are reserved
against simultaneous GUI/MCP writes. Other tasks can still be edited.

The **Parallel work** selector allows one to four independent workers; one is
the default. Resume dispatches queued jobs up to that limit, counting workers
already attached to the UI. Lowering the limit lets existing jobs finish.
Native builds and direct API/native-agent tasks can overlap in separate tasks.
Codex, Claude, Cursor and host Git requests share a relay mailbox, so only one
host request runs at a time. The queue waits at a blocked head entry; it does
not reorder pending work to bypass that entry. Externally launched workers in other
projects can run independently; task locks prevent simultaneous access to the
same task. A failed job pauses the queue and leaves later entries pending. Inspect the failed
task's activity before resuming. Resume continues the remaining work; it does
not retry the failed entry. Add that task again explicitly to retry it.

A failure leaves already-running jobs active and pauses further launches.
Pause prevents the next launch; **Stop** remains the control for an active job.
Queue state is stored in `Termic:Config/queue.json`, with `.bak` backup. Termic
always starts paused. It reattaches to a registered running worker; an
interrupted job without a worker becomes Needs attention and is never replayed
automatically. An invalid queue file disables dispatch until repaired/restored.

## Broadcast to agents

Open **Queue > Broadcast...** from a saved, idle task. Select agent profiles
with Shift-click and enter one shared instruction. **Queue copies** creates a
fresh source-copy task per selected profile, with separate instructions, files,
review journals and conversations. Existing tasks are preserved. YOLO is off in
these copies. Native CLI agent profiles must be configured to read `task.txt`.

Broadcast pauses dispatch and opens the queue. Inspect the new tasks and worker
limit before choosing Resume. Host profiles still run serially; independent
native/API tasks can use the parallel limit. This is not a race mode: every
queued task runs unless explicitly removed or stopped. A partial storage failure
leaves completed copies queued and reports the count; inspect unfinished copies
before retrying. No job starts automatically from the Broadcast dialog.

## Review agent edits

**Right Amiga U**, the Changes page or the board's Review button opens file
review. Select a file for a full-context unified diff on a black background:
green `+` lines are additions, red `-` lines are removals and blue lines are
headers. The View selector also offers the original Before / after comparison.
**Accept file** keeps the edit; **Revert file** restores the original after
confirmation. A generated version is retained as `.bak`. Reverting a new file
moves that file to `.bak`. Changes made since the recorded agent edit cause a
conflict, so review cannot silently overwrite a subsequent user edit.

Snapshots survive restarts in each task's hidden `.review.json` and
`.review-XX.before/after` files. They cover writes made through the agent's
`write_file` and `write_source` tools. Program/ARexx side effects, external
editors and the standalone MCP file interface are not captured by this journal;
use Git diff for those changes. Accept/Revert applies to the whole file,
not individual hunks. Large changed regions use replacement alignment to bound
68k processing and memory; no lines are discarded. Missing final newlines are
marked explicitly. Resolving edits does not automatically mark a task Done.

## Editor, compiler locations and scripts

C/H, Python and ARexx receive syntax colours, including a delayed refresh after
editing. Recolouring preserves text, cursor, dirty status and Undo. It waits
while text is selected. Saving always exports plain source.

Build / CLI lists recognized compiler locations below the output. Double-click
a VBCC or GCC-style diagnostic to open its source file at that line. Paths must
remain inside the task; unsupported diagnostic formats remain in the log.

Agent and MCP source tools support C/H, Python, ARexx and text/Markdown files.
Reserved task metadata and guidance remain protected from agent writes.
`run_python` executes a task's `.py` file with the pinned native MicroPython
runtime and returns its real exit status and captured output. It follows the
same access limits and cancellation rules as ARexx/program execution.

## Scratchpads

**Right Amiga L** opens task notes. Enter a name containing letters, digits,
`-` or `_`, choose New note, then edit and Save. Notes use the same
`.scratchpads/<name>` storage as the scratchpad MCP tools. Each note is limited
to 32 KiB and saves retain `.bak`. Switching notes and closing the window save
changes; a failed save keeps the note open. Notes cannot be saved while their
task is reserved by a queued or running job.

## Git branches, host worktrees and GitHub

**Right Amiga G** opens Git branches and worktrees. Native actions list, create
and switch branches in the current task's initialized Amiga Git repository.
They use normal Git protections and never force checkout. Task source copies
remain independent. The installed native amiga-git does not support worktrees.

Host actions require the updated **Termic AI Proxy**, Git and GitHub CLI (`gh`)
on the Mac or Windows host. Sign in to GitHub there and configure your Git
commit name/email on that host. Enter `owner/repository`,
a task branch distinct from the base branch, and a commit/PR title.

- **Worktree status** prepares/opens the task branch in a managed host worktree
  and returns status. Creating it may fetch the named repository.
- **Sync and commit** reads supported task source files from the Amiga and
  commits changes on that host branch. It refuses a dirty worktree, including
  untracked files. Files missing from the Amiga are not deleted from the host.
- **Create PR** asks for confirmation, synchronizes, commits and pushes the
  task branch, then creates a GitHub PR or reuses an existing one.

Credentials stay on the host. These operations run through the existing relay,
without an AI request. Stop cancels host subprocesses. A failed sync/commit can
leave a dirty worktree for inspection; it is not automatically reset or retried.
The proxy stores repositories/worktrees below its state drawer's `git/` folder.
These controls target GitHub HTTPS repositories; alternate hosts/remotes and
pull-request merging are outside this implementation.

## Process-monitor acceptance task

The [Process Monitor](../amiga/apps/process-monitor/README.md) exercises a
complete development task: queue the agent, generate source through native
file tools, inspect kernel-facing code, queue compilation and a disposable
service self-test, review edits and mark the task Done. Its real hardware
report remains local; screenshots use synthetic data with controls disabled.

Long builds keep their worker registered until final state, log and result
are published. The queue waits for that registration to disappear before
starting another entry. A successful build also requires BuildProject's
fresh completion marker after Program.new promotion. Successful nested build
scripts must reach EOF; `Quit 0` can unwind the outer AmigaDOS script early.
Update TermicAgent and BuildProject together.
