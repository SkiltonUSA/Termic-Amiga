#!/usr/bin/env python3
"""Score the exported integer Tiny model using the unchanged host C engine."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
from train_tiny_termic import DEFAULT_DATA, exact_answer, load_data, pairs


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--engine-source', type=Path, required=True)
    p.add_argument('--model', type=Path, required=True, help='Converted .t68 weights')
    p.add_argument('--data', type=Path, default=DEFAULT_DATA)
    p.add_argument('--split', choices=('train', 'validation', 'test'), required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    if args.output.exists():
        p.error('Choose a new report path')
    data = load_data(args.data)
    questions = pairs(data, args.split)
    with tempfile.TemporaryDirectory() as tmp:
        binary = Path(tmp)/'tiny68k'
        subprocess.run(['cc', '-O2', '-Wall', '-Werror', '-DA5_HOST',
                        str(args.engine_source/'src/tiny68k.c'), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary), str(args.model.resolve()), '-ask'],
                                input='\n'.join(q for q, _, _ in questions)+'\n',
                                capture_output=True, text=True, check=True)
    answers = result.stdout.splitlines()
    if len(answers) != len(questions):
        raise RuntimeError('Runtime did not return one answer per question')
    rows = [dict(id=id, question=q, expected=a, answer=got, correct=exact_answer(got, a))
            for (q, a, id), got in zip(questions, answers)]
    correct = sum(r['correct'] for r in rows)
    report = dict(runtime='tiny68k integer C', model_sha256=hashlib.sha256(args.model.read_bytes()).hexdigest(),
                  dataset_sha256=hashlib.sha256(args.data.read_bytes()).hexdigest(),
                  split=args.split, correct=correct, total=len(rows), accuracy=correct/len(rows), rows=rows)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print(args.split, correct, '/', len(rows))


if __name__ == '__main__':
    main()
