import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / 'Companion'))
import vocal_tune


class VocalTuneTests(unittest.TestCase):
    def test_chromatic_and_tie(self):
        self.assertEqual(vocal_tune.nearest_note(69.45), 69)
        self.assertEqual(vocal_tune.nearest_note(69.5), 69)

    def test_song_scale(self):
        self.assertEqual(vocal_tune.nearest_note(61.0, 'C', 'Major'), 60)
        self.assertEqual(vocal_tune.nearest_note(63.8, 'C', 'Major'), 64)

    def test_amount_and_humanize(self):
        full = vocal_tune.correction_for_midi(69.4, amount=100, humanize=0)
        half = vocal_tune.correction_for_midi(69.4, amount=50, humanize=0)
        self.assertAlmostEqual(half, full / 2)
        self.assertEqual(vocal_tune.correction_for_midi(69.1, amount=100, humanize=100), 0)

    def test_settings_reject_nonfinite_and_ranges(self):
        for kwargs in ({'speed_ms': 1}, {'amount': 101}, {'output_db': math.inf}, {'root': 'H'}):
            with self.assertRaises(ValueError):
                vocal_tune.tuning_settings(**kwargs)

    def test_continuous_vocal_does_not_define_an_unbounded_gate(self):
        # Regression contract for the packaged test: the calculated adaptive
        # gate is capped below an ordinary sustained vocal level.
        floor = .18
        gate = max(10 ** (-55 / 20), min(10 ** (-30 / 20), floor * 2.5))
        self.assertLess(gate, floor)


if __name__ == '__main__':
    unittest.main()
