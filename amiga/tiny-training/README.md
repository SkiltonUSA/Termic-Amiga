# Tiny trained on Termic Amiga documentation

The [experimental trained candidate](candidate/MODEL_CARD.md) is available,
but scored only 31/71 on unseen test questions and is not the default.

This is supervised fine-tuning of WollyMonjac's Tiny 2.1 checkpoint, using
Termic Amiga documentation as the source of short question/answer facts.
It trains model weights. It does not add document retrieval, executable tools,
access to user projects, or knowledge of a machine's current state.

`termic.json` contains 71 facts, each with a documentation source, seven
training questions, one validation question and one test question. Topics cover
editing, builds, CLI/ARexx/MicroPython, preferences, agents, MCP, shortcuts,
prompts, task queues, Kanban, review, Git, Processors and offline Tiny limits.
Only repository documentation is used. API keys, user projects, conversations,
logs and hardware captures are not training inputs.

## Reproduce training

Use the upstream revision and input hashes in `upstream-lock.json`. Clone
`https://github.com/WollyMonjac/Tiny-2.1` and check out that revision. On the
host, install `numpy` and `torch` into a dedicated virtual environment.
The recorded run uses Python 3.14, NumPy 2.5.5 and PyTorch 2.15.0 on CPU.

From the Termic-Amiga repository:

```sh
python scripts/amiga/train_tiny_termic.py train \
  --upstream /path/to/Tiny-2.1/training \
  --checkpoint /path/to/Tiny-2.1/training/tiny2.pt \
  --output /path/to/new-training-run \
  --steps 16000 --lr 0.0003 --replay 0.25 --eval-every 4000
```

The trainer keeps the upstream 366,528-parameter architecture and export
format unchanged. It uses seeded question selection, CPU training and a
cosine learning-rate schedule. Twenty-five percent of samples come from the
upstream facts to reduce forgetting. The remaining samples are documented
Termic answers with question wording variants. Variants must fit Tiny's
72-character question and 128-token combined context. Validation and test
questions are excluded from both augmentation and replay.

The output directory must be new. It contains a dataset snapshot, trainer
snapshot, input/source checksums, sampled-question audit, validation results,
latest and best checkpoints, and the exported `TINY.BIN`. Best checkpoint
selection uses validation accuracy only. The test set is not evaluated during
training. Hardware and library versions can affect training reproducibility;
artifact hashes identify the exact resulting weights.

## Evaluate the deployed representation

Convert the best exported weights using the pinned tiny68k source from
`amiga/native/tiny-lock.json`:

```sh
python /path/to/tiny68k/convert_tiny.py \
  /path/to/new-training-run/TINY.BIN /path/to/termic.t68
python scripts/amiga/evaluate_tiny_termic.py \
  --engine-source /path/to/tiny68k --model /path/to/termic.t68 \
  --split test --output /path/to/new-test-report.json
```

This compiles the unchanged integer C engine and evaluates the actual `.t68`
weights. Scoring requires the complete documented answer, ignoring only case,
spacing, the final period and hyphenation of Right Amiga. Command flags and
path punctuation are preserved. Wrong keys, numbers, negation, omitted
steps and extra claims fail. Training-question scores measure memorization;
held-out wording scores measure a limited form of generalization. Neither
establishes correctness for arbitrary questions.

Before promoting weights, also run tiny68k's original-C/float comparison,
integer/float accuracy comparison, integer-C/oracle verification and the
VBCC/Musashi gate in a separate checkout using the candidate model. Include
Termic questions and long responses in those checks, and confirm activation
ranges fit the fixed-point engine. Its model-specific prefix cache must not
be reused for different weights; the existing runtime checks the model hash.
Physical Amiga execution remains a separate acceptance step.

Do not overwrite the bundled `tiny-2.1` model merely because training loss
is low. A model can memorize its training examples and still give wrong
instructions when a user changes the wording. Keep candidates separate until
answer quality and numerical/runtime checks justify promotion.

## Attribution

Tiny 2.1 and the trainer are by WollyMonjac; the integer Amiga port is by
SkiltonUSA/tiny68k. Retain the upstream MIT `LICENSE` and `DATA_LICENSES.md`
with derived models. The initial checkpoint and replay include information
from mledoze/countries (ODbL) and Periodic-Table-JSON (CC BY-SA 3.0).
The additional Termic documentation QA is covered by this repository's AGPL-3.0
license. See each fact's `source` for its documented behavior.
