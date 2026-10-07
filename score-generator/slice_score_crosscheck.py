#!/usr/bin/env python3
"""Cross-checks slice_score.py's Python model of MidiSampler against the REAL C++ engine.

For each scenario it generates a score, takes the trigger presses straight out of the finished bundle, replays them through
MidiSamplerReplay.exe (built from C:\\AudioDev\\Repos\\MidiSampler, which runs the actual SamplerEngine.h) and compares which
slice keys are sounding at checkpoints in the middle of every bar with what the Python VoiceModel predicts.

    python slice_score_crosscheck.py [path\\to\\MidiSamplerReplay.exe]

A mismatch means the generator's idea of slice lengths / latch / start-only / stop-all differs from the plugin: fix the model, not the
test. Checkpoints sit in the middle of every step, away from press times; a slice that ends within 0.5 % of a bar of the
checkpoint is skipped as "ambiguous" (reported).
"""

import os
import subprocess
import sys
import tempfile

import slice_score as ss

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "..", "docs", "slice_test", "slice_test_source.mid")
DEFAULT_EXE = r"C:\AudioDev\Repos\MidiSampler\build\Release\MidiSamplerReplay.exe"
SAMPLE_RATE = 44100

SCENARIOS = [
    ("loop+latch beat4", ["--sustain", "loop", "--playback", "latch", "--slice-by", "beat", "--grid", "4"]),
    ("loop+latch beat1.5 (odd lengths)", ["--sustain", "loop", "--playback", "latch", "--slice-by", "beat", "--grid", "1.5"]),
    ("forward+latch beat2", ["--sustain", "forward", "--playback", "latch", "--slice-by", "beat", "--grid", "2"]),
    ("forward+start_only beat2", ["--sustain", "forward", "--playback", "start_only", "--slice-by", "beat", "--grid", "2"]),
    ("loop+start_only equal12", ["--sustain", "loop", "--playback", "start_only", "--slice-by", "equal", "--count", "12"]),
    ("forward+latch transient", ["--sustain", "forward", "--playback", "latch", "--slice-by", "transient", "--min-slice", "1"]),
    ("forward+latch thru", ["--sustain", "forward", "--playback", "latch", "--slice-by", "beat", "--grid", "4", "--thru"]),
    ("forward ratio 2/3 speed 90", ["--sustain", "forward", "--playback", "start_only", "--slice-by", "beat", "--grid", "4",
                                      "--ratio", "0.6666667", "--speed", "90"]),
    ("loop+latch no stop key", ["--sustain", "loop", "--playback", "latch", "--stop-key", "-1", "--slice-by", "beat", "--grid", "2"]),
]


def write_replay_input(path, args, seq, zone, cfg, bundle, checks):
    by = {"beat": 0, "equal": 1, "transient": 2, "manual": 3}[zone.slice_by]
    playback = {"latch": 1, "start_only": 2}[zone.playback]
    lines = [f"bpm {cfg.bpm}", f"sr {SAMPLE_RATE}", f"bpb {cfg.beats_per_bar}", f"voices {cfg.max_voices}", f"speed {cfg.speed_percent}",
             f"stopkey {-1 if cfg.stop_key is None else cfg.stop_key}", f"ratio {zone.ratio}", f"length {seq.length_beats!r}",
             f"zone {zone.start01} {zone.end01} {by} {zone.grid_beats} {zone.count} {zone.min_beats} {zone.base_key} "
             f"{1 if zone.loop else 0} {playback} {1 if zone.thru else 0}",
             f"notes {len(seq.notes)}"]
    lines += [f"{n.start!r} {n.length!r} {n.pitch}" for n in seq.notes]
    lines.append(f"manual {len(zone.manual)}")
    lines += [repr(f) for f in zone.manual]
    presses = []
    for sec in bundle["blueprint"]["sections"]:
        for entry in sec["capturedContent"]:
            for step_index, step in enumerate(entry["steps"]):
                if step["enabled"]:
                    presses.append((sec["startBar"], step_index, step["note"]))
    presses.sort()
    lines.append(f"presses {len(presses)}")
    lines += [f"{b} {s} {k}" for b, s, k in presses]
    lines.append(f"checks {len(checks)}")
    lines += [repr(c) for c in checks]
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return presses


def python_prediction(bundle, seq, zone, cfg, checks):
    bar_lengths = ss.slice_bar_lengths(seq, zone, cfg.speed_percent, cfg.beats_per_bar)
    model = ss.VoiceModel(bar_lengths, zone)
    events = []
    for sec in bundle["blueprint"]["sections"]:
        for entry in sec["capturedContent"]:
            for step_index, step in enumerate(entry["steps"]):
                if step["enabled"]:
                    events.append((float(sec["startBar"] - 1) + step_index / 16.0, step["note"]))
    events.sort()
    out, i = [], 0
    for check in checks:
        while i < len(events) and events[i][0] <= check:
            t, key = events[i]
            if cfg.stop_key is not None and key == cfg.stop_key:
                model.stop_all(t)
            else:
                model.press(key - zone.base_key, t)
            i += 1
        keys = sorted(zone.base_key + idx for idx in model.active_set(check))
        ambiguous = {zone.base_key + idx for idx, until in model.until.items() if abs(until - check) < 0.005}
        out.append((keys, ambiguous))
    return out


def run_scenario(exe, name, extra, tmp):
    argv = ["--title", "x", "--source", SOURCE, "--output", os.path.join(tmp, "x.json"), "--minutes", "1.3", "--bpm", "110",
            "--strategy", "random", "--seed", "5", "--max-layers", "4", *extra]
    args = ss.build_parser().parse_args(argv)
    seq, zone, cfg, n_slices = ss.make_setup(args)
    events, energy, active_after = ss.plan_events(cfg, seq, zone)
    bundle = ss.compile_bundle(args.title, cfg, zone, events, energy)

    # a checkpoint in the middle of EVERY step of every bar (press times are on step boundaries, so each checkpoint is half a step
    # away from a press): catches wrong slice lengths, wrong latch/restart behaviour and wrong stop-all handling alike
    checks = [(m - 1) + (s + 0.5) / 16.0 for m in range(1, cfg.bars + 1) for s in range(16)]
    replay_in = os.path.join(tmp, "replay.txt")
    presses = write_replay_input(replay_in, args, seq, zone, cfg, bundle, checks)
    result = subprocess.run([exe, replay_in], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"ERROR {name}: replay tool failed: {result.stderr.strip()}")
        return 1

    real = []
    for line in result.stdout.strip().splitlines():
        parts = line.split()
        real.append([int(x) for x in parts[1:]])
    predicted = python_prediction(bundle, seq, zone, cfg, checks)

    mismatches = ambiguous = 0
    sounding = sum(1 for keys in real if keys)
    for check, (keys, amb), real_keys in zip(checks, predicted, real):
        if keys == real_keys:
            continue
        diff = set(keys) ^ set(real_keys)
        if diff and diff <= amb:
            ambiguous += 1
            continue
        mismatches += 1
        if mismatches <= 3:
            print(f"   bar {int(check) + 1} step {int((check % 1) * 16)} (t={check:.4f}): Python {keys} vs engine {real_keys}")
    status = "OK  " if mismatches == 0 else "FAIL"
    print(f"{status} {name}: {len(checks)} checkpoints, {len(presses)} presses, {n_slices} slices, "
          f"{sounding} checkpoints with sound, {mismatches} mismatch(es), {ambiguous} ambiguous-skipped")
    return 1 if mismatches else 0


def main() -> int:
    exe = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_EXE
    if not os.path.exists(exe):
        print(f"MidiSamplerReplay.exe not found at {exe}\nBuild it: cmake --build build --config Release --target MidiSamplerReplay "
              f"(in C:\\AudioDev\\Repos\\MidiSampler)")
        return 2
    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, extra in SCENARIOS:
            failures += run_scenario(exe, name, extra, tmp)
    print("ALL SCENARIOS AGREE WITH THE REAL ENGINE" if failures == 0 else f"{failures} scenario(s) disagree")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
