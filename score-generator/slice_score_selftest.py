#!/usr/bin/env python3
"""Stress test for slice_score.py: runs the generator over every strategy x playback mode x slicing x shooter count x several
seeds and requires the built-in replay verification to pass every time (exit code 0). Takes about a minute.

    python slice_score_selftest.py
"""

import itertools
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "..", "docs", "slice_test", "slice_test_source.mid")

MODES = [
    ("loop+latch", ["--sustain", "loop", "--playback", "latch"]),
    ("forward+latch", ["--sustain", "forward", "--playback", "latch"]),
    ("forward+start_only", ["--sustain", "forward", "--playback", "start_only"]),
    ("loop+start_only+stopkey", ["--sustain", "loop", "--playback", "start_only"]),
    ("loop+latch+nostopkey", ["--sustain", "loop", "--playback", "latch", "--stop-key", "-1"]),
]
SLICINGS = [
    ("beat4", ["--slice-by", "beat", "--grid", "4"]),
    ("beat1", ["--slice-by", "beat", "--grid", "1"]),
    ("equal12", ["--slice-by", "equal", "--count", "12"]),
    ("transient", ["--slice-by", "transient", "--min-slice", "1"]),
]
STRATEGIES = ["order", "retro", "arch", "random", "row"]


def main() -> int:
    failures = 0
    runs = 0
    with tempfile.TemporaryDirectory() as tmp:
        for (mode_name, mode_args), (slice_name, slice_args), strategy, shooters, seed in itertools.product(
                MODES, SLICINGS, STRATEGIES, [1, 2], [1, 2, 3]):
            if strategy == "row" and slice_name not in ("equal12", "beat1"):
                continue                                   # a row needs >= 12 slices (beat1 gives 32)
            out = os.path.join(tmp, "t.json")
            cmd = [sys.executable, os.path.join(HERE, "slice_score.py"), "--title", "t", "--source", SOURCE, "--output", out,
                   "--minutes", "1.2", "--bpm", "110", "--strategy", strategy, "--shooters", str(shooters), "--seed", str(seed),
                   "--max-layers", "4", *mode_args, *slice_args]
            result = subprocess.run(cmd, capture_output=True, text=True)
            runs += 1
            if result.returncode != 0:
                failures += 1
                print(f"FAIL {mode_name} / {slice_name} / {strategy} / shooters={shooters} / seed={seed}")
                print("   " + (result.stderr.strip().splitlines() or ["(no output)"])[-1])
    print(f"{runs} runs, {failures} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
