# Offline Tiny agent

Termic includes a **Tiny 2.1** profile with model `tiny-2.1`.
It is the default when a task has no explicit agent selection. Existing task
selections, API credentials and host profiles are preserved. Older profile
registries gain Tiny when loaded if a free profile slot is available. A full
32-profile registry must have a custom profile removed before Tiny can be
added. Choose **Add Tiny** in Agents to create another Tiny profile.

TINY 2.1 is by [WollyMonjac](https://github.com/WollyMonjac/Tiny-2.1).
The Amiga inference engine is [tiny68k](https://github.com/SkiltonUSA/tiny68k).
This is a small, 367K-parameter question-answering model, not a coding agent.
Ask one question of up to 72 printable ASCII characters, for example
`What is the capital of Peru?`. Longer questions are rejected rather than
silently truncated. Whitespace is normalized and ASCII letters lowercased.
The model can give incorrect answers outside its training facts.

Inference runs entirely on the Amiga 68k CPU. No internet, API key, Mac proxy,
subscription or ZZ9000 is needed. The model stays at
`Termic:Tools/Tiny/tiny.t68`; the runtime is `Termic:Tools/Tiny/tiny68k`.
Task text is supplied through a temporary input file, never shell source.
The worker records the answer in the task transcript displayed in Agent
activity, and reports failures through the normal task result. Stop uses the
worker's cooperative cancellation path. There is no automatic cloud fallback.
Termic adds the local commands below around Tiny's model. Project access is
**read-only**. Tiny cannot edit, build or execute project code. Its neural model
still receives only the current short question; saved context is used by recall
and explicit handoff, not squeezed into the model's 72-character input.

## Local commands

Select Tiny, enter a command in **Task instructions**, then start the agent.
Commands and their results appear in Agent activity. These commands work without
network access, except for the explicit host-agent handoff.

| Command | Result |
| --- | --- |
| `/help` | Show the command list. |
| `/docs yolo` | Search the installed AmigaGuide and display the best matching chapter, with its title and node ID. |
| `/files` | List supported source files under the current task/project. |
| `/read main.c` | Display a project source file, without changing it. |
| `/history` | Show saved recent conversation and notes. |
| `/recall MUI` | Search saved questions, answers and notes for that phrase. |
| `/remember Use MUI 5 for this project` | Save an explicit note for recall and handoff. |
| `/forget` | Clear Tiny's saved context; keep the activity transcript. |
| `/agents` | List configured Codex, Claude and Cursor profile IDs. |
| `/handoff ID Review main.c without changing it` | Send the request and saved context to the named host agent. Replace `ID` with an ID from `/agents`. |

Guide lookup is a local keyword search over `Termic:Termic.guide`. It displays
source text, not a generated answer. Search by feature name, such as `kanban`,
`shortcuts`, `ARexx` or `yolo`. Excerpts are limited to 12,000 characters; open
Help for the complete chapter. Ordinary questions still use Tiny inference;
use `/docs` explicitly when you want documented Termic behavior.

File access uses Termic's existing project containment checks. It accepts C/H,
Python, ARexx, text/Markdown and the named project guidance files, up to 32 KiB.
Device paths, parent traversal, reserved task/session files and links outside
the project are rejected. There is no Tiny write or execution command.

## Conversation memory

Memory persists in `.tiny-memory.json` in each task's directory. It survives
closing Termic and restarting the Amiga, and is separate for each task. The
New projects ignore `.tiny-memory*` in Git. For an existing repository, add
that pattern to its `.gitignore` before committing task state.

The last **eight** recorded turns retain up to **256 characters of the question**
and **1,024 characters of the answer**. Explicit notes retain up to **4,096
characters**. Full results remain in `transcript.txt`, subject to its existing
256 KiB limit. History/recall queries do not displace saved turns.

Reading a project file records an excerpt in memory. Use `/history` to inspect
what is retained before handoff. `/forget` resets memory and removes its ordinary
backup, but does not erase the transcript, external backups or a host agent's
conversation. Invalid memory is preserved and reported rather than silently
replaced; `/forget` explicitly resets it.

This is application-managed context, not a larger neural context window.
Tiny does not automatically resolve pronouns or reason over earlier messages.
Use `/recall` to retrieve earlier information, or hand off to a capable agent.

## Agent handoff

Create or configure a Codex, Claude or Cursor host profile in **Settings >
Agents / CLI / Scripts**, and start Termic AI Proxy on the Mac or Windows PC.
Use `/agents` to obtain its ID, then `/handoff ID your request`.

Only that explicit command starts a handoff. The selected target receives:

- Your complete new request, up to 3,000 characters.
- Up to 512 characters of saved notes.
- The newest complete saved turns that fit within 4,096 characters, newest first.

Context is labelled reference data; Tiny's answers may be wrong and should be
verified. File excerpts previously read can be included. The target's configured
model, permissions, step limit and session-resume settings apply. Credentials
remain on the host. Read-only Tiny access does not change the target profile's
permissions; choose a read-only target for a read-only handoff.

Replies and progress appear in the same activity transcript. The task's Tiny
selection and original instructions remain unchanged. Stop uses the existing
relay cancellation path. If the host is unavailable or the ID is invalid,
Termic reports failure without selecting another provider. Direct API and local
CLI profiles are not handoff targets in this implementation.

The original 0.1.4 Beta archives and MUI guide update predate these local-assistant
features. They require the updated worker, guide and source from this revision.

## Verification status

The local-assistant commands, memory bounds, corrupt-state preservation,
read-only path rejection and handoff context construction pass host tests.
The worker and MUI application cross-compile. The written physical acceptance
runner is `scripts/amiga/accept_tiny_assistant.py`; its synthetic provider
fixtures check relay handoffs without a live provider login. Physical acceptance
of these new features and target installation are pending: the bridge paused
on the initial port-list request, before the fixture was launched.

The following results describe the earlier Tiny inference/transcript fix:

The host suite passes 101 tests, including profile migration, Tiny question
bounds, transcript recording, cancellation and failure handling. Native
application and MUI lifecycle/scaling test binaries cross-compile successfully.
The unchanged Tiny engine passes the upstream integer oracle: all 19,493
score values match. Its VBCC-compiled 68000 engine and assembly kernel also
match those values in Musashi, including prefill and greedy/sampled answers.
These checks establish numerical parity, not speed on a physical Amiga.

The offline LHA passes archive integrity checks and AmiPkg's parser, dependency
resolver and fresh local install plan. The physical Amiga now passes offline question execution and repeated transcript
writes: `What is the capital of Peru?` returns `Lima.` with exit code 0,
and a second run retains both answers. Cancellation and the revised Processors
tab still need physical Amiga acceptance. Version 0.1.4 Beta packages these changes; the older 0.1.3 release remains available.

## Exit code 20 after an answer

The original 0.1.4 Beta worker could print Tiny's answer and then fail while
opening `transcript.txt` in C append mode. On the tested project volume this
returns AmigaDOS error 211, even for a new file. The corrected worker uses the
same bounded backup-and-replace transcript writer as the relay, retains prior
answers, and reports a transcript-saving failure explicitly. It does not alter
Tiny's model or improve the accuracy of its answers. The original 0.1.4 release
archives predate this correction; use a worker built from the corrected source.

## Preparing an offline kit

The installation drawer includes the runtime, model, README, upstream MIT
notice, data attribution and a SHA-256 provenance manifest. Once copied to
`Termic:Tools/Tiny`, it needs no download at launch. `package_native.py --tiny`
bundles that whole drawer and validates the model and runtime checksums.
The 0.1.4 Beta release includes this offline drawer; 0.1.3 predates it.

For a host build, obtain the tiny68k source revision and upstream model revision
pinned in `amiga/native/tiny-lock.json`. The original weights are
`dos/TINY.BIN` in the upstream repository; download `LICENSE` and
`DATA_LICENSES.md` from the same revision. With NumPy installed, convert the
weights using that pinned tiny68k checkout:

```sh
python3 /path/to/tiny68k/convert_tiny.py /path/to/TINY.BIN /path/to/tiny.t68
python3 scripts/amiga/build_tiny.py \
  --source /path/to/tiny68k --model /path/to/tiny.t68 \
  --notices /path/to/upstream-notices \
  --tools /path/to/vbcc/bin --vc-config /path/to/amiga-vc.config \
  --output /path/to/Tiny
```

The builder verifies source and model pins, compiles the unchanged 68000
integer engine plus assembly kernel, and emits the offline drawer. It does
not install anything on a target or publish model files. The package builder
requires this drawer explicitly; a fresh kit cannot silently omit the default
agent's model. Existing installations can copy the drawer independently.

## Training sources

The [Termic documentation training pipeline](../amiga/tiny-training/README.md)
fine-tunes Tiny's weights using documented Amiga workflows and a separate
held-out question set. Candidate training does not automatically replace the
bundled model or give Tiny execution tools.

The upstream [training kit](https://github.com/WollyMonjac/Tiny-2.1/tree/main/training)
contains the facts, question generator, PyTorch trainer and checkpoint. Training
is a separate host task, not an Amiga UI action. See its `TRAINING.TXT` before
editing facts. The model includes information from mledoze/countries (ODbL)
and Periodic-Table-JSON (CC BY-SA 3.0); retain `DATA_LICENSES.md` with it.

A retrained model is not interchangeable with the pinned `tiny-2.1` runtime
without validation. Run the upstream reference, integer-oracle and compiled
68000 accuracy gates before updating the model pin. Termic does not download
or substitute a different model automatically.
