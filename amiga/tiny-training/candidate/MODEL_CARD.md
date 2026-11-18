# Tiny Termic experimental candidate

**Trained, exported, and not promoted to the default model.** This candidate
memorizes documented examples well but is unreliable on unseen wording.
It must not be treated as an authoritative Termic help system.
The original bundled Tiny model and its pins remain unchanged.

## Training and evaluation

The 18 November 2026 run fine-tuned WollyMonjac's Tiny 2.1 checkpoint on
71 Termic Amiga documentation facts, with seven authored training questions
per fact and generated wording variants. Validation and test each contain
one separate question per fact. Test questions were evaluated only after the
final training run. The sampled-question audit found no exact overlap with
either held-out split. This tests rephrasing of known facts, not new facts.

The final run used 16,000 optimizer steps, batch size 32, two CPU threads,
peak learning rate 0.0003, a cosine schedule, seed 20261118, and 25% upstream
fact replay. The checkpoint at step 8,000 was selected using validation.
Architecture and integer inference code were unchanged: 366,528 parameters,
D=96, L=4, H=4, HID=256; question limit 72 characters; context 128 tokens.

The **exported integer C runtime**, using `termic.t68`, achieved:

| Question set | Correct complete answers | Accuracy |
| --- | ---: | ---: |
| Authored training questions | 494 / 497 | 99.4% |
| Validation phrasings | 37 / 71 | 52.1% |
| Untouched test phrasings | 31 / 71 | 43.7% |

The original checkpoint scored 0/71 on validation. The initial narrow-corpus
trials did poorly; broader training phrasings improved validation, but the
final held-out result remains inadequate. No subsequent tuning used the test
answers. See `test-results.json` for every test question, expected answer,
actual integer-model answer and strict grading result. Grading preserves
command flags, paths, numbers and negation. The training snapshot used a
word-based validation scorer; the final reports use the stricter scorer.

## Numerical checks

- Original `TINY.C` versus Python float reference: logits within 1.93e-6
  relative error on the probe, but greedy answers matched only 70/71.
  **The full reference gate failed.**
- Integer oracle versus original float C: minimum logit correlation 0.99975,
  mean top-10 overlap 9.94/10, but only 69/71 identical greedy answers.
  **The full float/integer accuracy gate failed.**
- Host integer C versus integer oracle: 19,493 score/activity values matched;
  all 71 domain and 40 general-domain greedy answers matched the oracle,
  with sampled and prefill checks passing. This is numerical parity, not
  factual answer accuracy.
- VBCC-compiled 68000 C and assembly kernels in Musashi each matched 28,280
  score/activity values, including an additional long Termic ARexx response,
  plus 9 greedy and 2 sampled answers. The unmodified gate's stock-model
  prefix-cache speed assertion failed for both kernels: changed weights
  disable that model-specific cache, so forcing its option gives no speedup.
  A separate host check confirmed `prefix_ok == 0` and identical answers
  with the option toggled. The original failing gate results were retained;
  no inference code or oracle was changed to make them pass.
- Observed float-reference ranges included residual magnitude 86.747 and
  value magnitude 5.960, below the engine's value-magnitude assumption of 64.
  This does not bound arbitrary future questions.

Physical Amiga execution has **not** been verified for these weights.
Training and tests did not acquire the hardware bridge or alter an Amiga.

## Files and use

`termic.t68` is the actual 386,026-byte converted model, not a prompt or a
retrieval index. `provenance.json` records the dataset, source, trainer,
initial-checkpoint and exported-weight hashes. The full-precision checkpoint
and exact trainer snapshot are also retained in the local training archive.

To experiment separately, use the existing `tiny68k` runtime from an Amiga
CLI with a 256 KiB stack:

```text
Stack 262144
tiny68k termic.t68
```

Ask one short question and type `QUIT` to exit. Known examples include
`how do i save code`, `how do i run arexx`, and `how do i open kanban`.
These are training examples, not evidence of generalization. Do not overwrite
`Termic:Tools/Tiny/tiny.t68` with this candidate. It has no API access, file
editing, build tools, live hardware access or conversation memory.

TINY 2.1 is by WollyMonjac; the integer Amiga port is SkiltonUSA/tiny68k.
Keep `LICENSE` and `DATA_LICENSES.md` with the model. The initial checkpoint
and replay include mledoze/countries (ODbL) and Periodic-Table-JSON
(CC BY-SA 3.0) information. Termic-specific QA uses this repository's MIT
licensed documentation. Full upstream pins are in `../upstream-lock.json`.
