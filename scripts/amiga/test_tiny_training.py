"""Check context limits and evaluation separation for offline training."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import struct
import unittest
from train_tiny_termic import DEFAULT_DATA, exact_answer, load_data, pairs, training_pairs


class TinyTrainingTests(unittest.TestCase):
    def test_candidate_weights_match_recorded_model_and_architecture(self):
        candidate = DEFAULT_DATA.parent/'candidate'
        receipt = json.loads((candidate/'provenance.json').read_text())
        raw = (candidate/'termic.t68').read_bytes()
        self.assertEqual(hashlib.sha256(raw).hexdigest(), receipt['converted_sha256'])
        self.assertEqual(raw[:4], b'T68A')
        self.assertEqual(struct.unpack('>7H', raw[4:18]), (1,96,4,4,256,97,128))
        self.assertFalse(receipt['promoted'])
        self.assertEqual(receipt['holdout_overlap'], 0)

    def test_documented_corpus_and_disjoint_augmented_training(self):
        data = load_data(DEFAULT_DATA)
        self.assertGreaterEqual(len(data['facts']), 70)
        trained = training_pairs(data)
        heldout = {q for split in ('validation', 'test') for q, _, _ in pairs(data, split)}
        self.assertFalse(heldout & {q for q, _, _ in trained})
        for q, a, _ in trained:
            self.assertLessEqual(len(q), 72)
            self.assertLessEqual(len(q)+len(a)+2, 128)
            self.assertTrue((q+a).isascii())
        self.assertEqual(trained, training_pairs(data))

    def test_invalid_corpus_is_rejected(self):
        valid = load_data(DEFAULT_DATA)
        for defect in ('overlong', 'unicode', 'leak', 'missing_source'):
            data = copy.deepcopy(valid)
            fact = data['facts'][0]
            if defect == 'overlong': fact['answer'] = 'x'*128
            if defect == 'unicode': fact['answer'] = 'Caf\u00e9'
            if defect == 'leak': fact['test'] = fact['train'][:1]
            if defect == 'missing_source': fact['source'] = 'missing-doc.txt'
            with tempfile.TemporaryDirectory() as tmp:
                path = Path(tmp)/'data.json'
                path.write_text(json.dumps(data))
                with self.assertRaises(ValueError): load_data(path)

    def test_answer_scoring_preserves_numbers_and_negation(self):
        self.assertTrue(exact_answer('Right-Amiga S.', 'right amiga s'))
        self.assertFalse(exact_answer('Tiny can build.', 'Tiny cannot build.'))
        self.assertFalse(exact_answer('Use port 55010.', 'Use port 55110.'))
        self.assertFalse(exact_answer('Use F7.', 'Use F8.'))
        self.assertFalse(exact_answer('MUI 3.8', 'MUI 5.0'))
        self.assertFalse(exact_answer('Termic:TermicMCP stdio', 'Termic:TermicMCP --stdio'))
        self.assertFalse(exact_answer('Termic/Tools/Tiny', 'Termic:Tools/Tiny'))


if __name__ == '__main__':
    unittest.main()
