#!/usr/bin/env python3
"""Fine-tune upstream Tiny on documented Termic QA. No cloud or target writes.

Imports the pinned upstream training kit rather than changing its network,
quantizer or inference mathematics. Checkpoints and evaluation outputs are local.
"""
import argparse
import hashlib
import importlib
import json
import math
from pathlib import Path
import random
import re
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DATA = ROOT / 'amiga/tiny-training/termic.json'
UPSTREAM_LOCK = ROOT / 'amiga/tiny-training/upstream-lock.json'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def normalize(text):
    return ' '.join(text.lower().split())


def load_data(path):
    data = json.loads(Path(path).read_text())
    if data['schema'] != 1:
        raise ValueError('Unknown training data schema')
    seen, ids = set(), set()
    for fact in data['facts']:
        if fact['id'] in ids or not (ROOT / fact['source']).is_file():
            raise ValueError('Duplicate ID or missing documentation source')
        ids.add(fact['id'])
        for split in ('train', 'validation', 'test'):
            if not fact[split]:
                raise ValueError('Every fact needs all three question splits')
            for raw in fact[split]:
                q = normalize(raw)
                if q in seen:
                    raise ValueError('Question leaks across facts/splits: ' + q)
                seen.add(q)
                text = q + fact['answer']
                if not q or len(q) > 72 or len(text) + 2 > 128:
                    raise ValueError('Question/answer exceeds Tiny context: ' + fact['id'])
                if any(ord(c) < 32 or ord(c) > 126 for c in text):
                    raise ValueError('Tiny requires printable ASCII')
    return data


def pairs(data, split):
    return [(normalize(q), f['answer'], f['id'])
            for f in data['facts'] for q in f[split]]


def training_pairs(data):
    # Only transform training questions. Reject any overlap with either holdout.
    withheld = {q for s in ('validation', 'test') for q, _, _ in pairs(data, s)}
    result = {}
    starts = (
        ('how do i ', 'how can i ', 'how could i ', 'how should i ', 'how to '),
        ('where is ', 'where can i find ', 'where do i find ', 'where to find '),
        ('what is ', 'what exactly is ', 'what does termic mean by '),
        ('how do ', 'how can ', 'how could '),
    )
    # General wording transformations, applied uniformly rather than copying
    # evaluation questions. Some combinations are awkward; keep the originals.
    synonyms = (
        ('open', 'show', 'view', 'access', 'see'),
        ('save', 'store', 'keep'),
        ('create', 'make', 'start'),
        ('run', 'execute', 'launch'),
        ('settings', 'preferences', 'configuration'),
        ('manual', 'guide', 'documentation'),
        ('code', 'source'),
        ('program', 'app', 'application'),
        ('prompt', 'message', 'instruction'),
        ('change', 'modify', 'customize'),
        ('need', 'require'),
        ('needs', 'requires'),
        ('shortcut', 'key binding'),
    )
    originals = pairs(data, 'train')
    for f in data['facts']:
        topic = f['id'].replace('_', ' ')
        originals.extend((q, f['answer'], f['id']) for q in
                         (topic, 'help with '+topic, 'tell me about '+topic))
    for q, answer, id in originals:
        variants = {q}
        for group in starts:
            for start in group:
                if q.startswith(start):
                    variants.update(other+q[len(start):] for other in group)
        for v in sorted(variants.copy()):
            for group in synonyms:
                for word in group:
                    if re.search(r'\b'+word+r'\b', v):
                        variants.update(re.sub(r'\b'+word+r'\b', other, v) for other in group)
        for prompt in sorted({p for v in variants for p in
                (v, v+'?', 'termic: '+v, v+' in termic', 'please '+v, 'tiny, '+v)}):
            if len(prompt) <= 72 and len(prompt) + len(answer) + 2 <= 128:
                if prompt in withheld:
                    continue
                if prompt in result and result[prompt][1] != answer:
                    raise ValueError('Conflicting augmented answers: '+prompt)
                result[prompt] = (prompt, answer, id)
    return list(result.values())


def exact_answer(answer, reference):
    # Preserve command/path punctuation and flags. Only harmless presentation
    # differences in sentence endings and the Amiga modifier name are ignored.
    def canonical(text):
        return normalize(text).replace('right-amiga', 'right amiga').rstrip('.')
    return canonical(answer) == canonical(reference)


def evaluate(upstream, model, data, split):
    examples = pairs(data, split)
    answers = upstream.answer_many(model, [q for q, _, _ in examples])
    rows = [dict(id=id, question=q, expected=a, answer=answers[q],
                 correct=exact_answer(answers[q], a)) for q, a, id in examples]
    passed = sum(r['correct'] for r in rows)
    return dict(split=split, correct=passed, total=len(rows), accuracy=passed / len(rows), rows=rows)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('train', 'evaluate'))
    p.add_argument('--upstream', type=Path, required=True, help='Pinned Tiny-2.1 training directory')
    p.add_argument('--checkpoint', type=Path, required=True)
    p.add_argument('--data', type=Path, default=DEFAULT_DATA)
    p.add_argument('--output', type=Path, required=True, help='New local output directory')
    p.add_argument('--steps', type=int, default=2400)
    p.add_argument('--batch', type=int, default=32)
    p.add_argument('--threads', type=int, default=2)
    p.add_argument('--lr', type=float, default=5e-4)
    p.add_argument('--replay', type=float, default=0.15)
    p.add_argument('--seed', type=int, default=20261118)
    p.add_argument('--eval-every', type=int, default=400)
    p.add_argument('--split', choices=('train', 'validation', 'test'), default='validation')
    args = p.parse_args()
    if (args.steps < 1 or args.batch < 1 or args.threads < 1 or args.eval_every < 1
            or not 0 <= args.replay < 1 or not math.isfinite(args.lr) or args.lr <= 0):
        p.error('Invalid training limits')
    data = load_data(args.data)
    lock = json.loads(UPSTREAM_LOCK.read_text())
    for name in ('train2.py', 'gen2.py'):
        if digest(args.upstream/name) != lock['files']['training/'+name]:
            raise ValueError('Upstream trainer pin mismatch: '+name)
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output/'dataset.json').write_bytes(args.data.read_bytes())
    (args.output/'trainer.py').write_bytes(Path(__file__).read_bytes())
    sys.path.insert(0, str(args.upstream.resolve()))
    upstream = importlib.import_module('train2')
    torch = upstream.torch
    torch.set_num_threads(args.threads)
    torch.manual_seed(args.seed)
    rng = random.Random(args.seed)
    model = upstream.load_model(str(args.checkpoint))
    if vars(model.c) != dict(D=96, L=4, H=4, HID=256):
        raise ValueError('Keep the validated Tiny 2.1 architecture')
    receipt = dict(data_sha256=digest(args.output/'dataset.json'), checkpoint_sha256=digest(args.checkpoint),
                   trainer_sha256=digest(__file__), upstream_commit=lock['commit'],
                   upstream_files={n: digest(args.upstream/n) for n in ('train2.py', 'gen2.py')},
                   replay_facts={p.name: digest(p) for p in sorted((args.upstream/'facts').glob('*')) if p.is_file()},
                   sources={f['source']: digest(ROOT/f['source']) for f in data['facts']},
                   config={k: v for k, v in vars(args).items() if not isinstance(v, Path)},
                   torch=torch.__version__, parameters=sum(p.numel() for p in model.parameters()))
    (args.output/'provenance.json').write_text(json.dumps(receipt, indent=2)+'\n')
    if args.action == 'evaluate':
        report = evaluate(upstream, upstream.quantized_copy(model), data, args.split)
        (args.output/'evaluation.json').write_text(json.dumps(report, indent=2)+'\n')
        print(args.split, report['correct'], '/', report['total'], flush=True)
        return
    training = training_pairs(data)
    withheld = {q for split in ('validation', 'test') for q, _, _ in pairs(data, split)}
    seen_questions = set()
    opt = torch.optim.AdamW([
        dict(params=[p for p in model.parameters() if p.dim() == 2], weight_decay=0.01),
        dict(params=[p for p in model.parameters() if p.dim() < 2], weight_decay=0.0)
    ], lr=args.lr, betas=(0.9, 0.98))
    pool = []
    def batch():
        if not pool:
            examples = []
            for _ in range(args.batch * 32):
                if rng.random() < args.replay:
                    while True:
                        q, a, _ = upstream.gen2.sample(rng)
                        q = normalize(q)
                        if q not in withheld and len(q) <= 72 and len(q) + len(a) + 2 <= 128:
                            break
                else:
                    q, a, _ = rng.choice(training)
                assert q not in withheld
                seen_questions.add(q)
                ids = upstream.encode(q + '\n' + a + '\n')
                examples.append((ids, len(q) + 1))
            examples.sort(key=lambda e: len(e[0]))
            pool.extend(examples[i:i+args.batch] for i in range(0, len(examples), args.batch))
            rng.shuffle(pool)
        examples = pool.pop()
        size = max(len(e[0]) for e in examples)
        x = torch.zeros(len(examples), size, dtype=torch.long)
        mask = torch.zeros(len(examples), size)
        for i, (ids, start) in enumerate(examples):
            x[i, :len(ids)] = torch.tensor(ids)
            mask[i, start:len(ids)] = 1
        return x, mask
    start = time.monotonic()
    best = -1
    print('training', len(training), 'questions;', receipt['parameters'], 'parameters', flush=True)
    for step in range(1, args.steps+1):
        lr = args.lr * min(1, step/100) * (0.1 + 0.9 * (1 + math.cos(math.pi*step/args.steps))/2)
        for group in opt.param_groups:
            group['lr'] = lr
        x, mask = batch()
        loss = upstream.loss_fn(model, x, mask)
        if not torch.isfinite(loss):
            raise RuntimeError('Non-finite training loss')
        opt.zero_grad(set_to_none=True)
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        opt.step()
        if step % 100 == 0:
            print('step', step, 'loss', round(loss.item(), 4), 'seconds', round(time.monotonic()-start, 1), flush=True)
        if step % args.eval_every == 0 or step == args.steps:
            upstream.save_model(model, str(args.output/'latest.pt'))
            report = evaluate(upstream, upstream.quantized_copy(model), data, 'validation')
            report['step'] = step
            (args.output/f'validation-{step}.json').write_text(json.dumps(report, indent=2)+'\n')
            print('validation', step, report['correct'], '/', report['total'], flush=True)
            if report['correct'] > best:
                best = report['correct']
                upstream.save_model(model, str(args.output/'best.pt'))
    best_model = upstream.load_model(str(args.output/'best.pt'))
    upstream.export(best_model, str(args.output/'TINY.BIN'))
    receipt.update(steps=args.steps, elapsed_seconds=time.monotonic()-start,
                   best_validation_correct=best, exported_sha256=digest(args.output/'TINY.BIN'))
    receipt['unique_training_questions'] = len(seen_questions)
    receipt['holdout_overlap'] = len(seen_questions & withheld)
    (args.output/'seen-questions.json').write_text(json.dumps(sorted(seen_questions))+'\n')
    (args.output/'provenance.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print('Exported best validation checkpoint; test split has not been evaluated.', flush=True)


if __name__ == '__main__':
    main()
