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
Tiny has no file/build tools, YOLO behavior or conversation memory. Choose
Codex, Claude, Cursor or OpenAI when those capabilities are needed.

## Verification status

The host suite passes 100 tests, including profile migration, Tiny question
bounds, transcript recording, cancellation and failure handling. Native
application and MUI lifecycle/scaling test binaries cross-compile successfully.
The unchanged Tiny engine passes the upstream integer oracle: all 19,493
score values match. Its VBCC-compiled 68000 engine and assembly kernel also
match those values in Musashi, including prefill and greedy/sampled answers.
These checks establish numerical parity, not speed on a physical Amiga.

The offline LHA passes archive integrity checks and AmiPkg's parser, dependency
resolver and fresh local install plan. Target installation, offline task
execution, cancellation and the revised Processors tab still need physical
Amiga acceptance. Version 0.1.4 Beta packages these changes; the older 0.1.3 release remains available.

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
