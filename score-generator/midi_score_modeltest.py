#!/usr/bin/env python3
"""Tests of midisampler_model.py against the real engine, beyond the key-stream cross-check:

 1. PITCH PROBE: for random zone setups (slice zones, per-key slices, sample zones, steps / pitch-set key maps, invert, zone transpose,
    different roots and keyboard octaves) one key is pressed on a source whose notes all share one pitch; the first pitch the REAL engine
    emits must equal the model's transposition arithmetic (key offset, invert, octave fold).
 2. NEGATIVE CONTROLS: the model is deliberately broken three plausible ways (no voice stealing, wrong natural-end times, latch ignored);
    the key-stream cross-check must catch each, otherwise it proves nothing.

    python midi_score_modeltest.py
"""

import os
import random
import subprocess
import sys
import tempfile

import midi_score_crosscheck as cc
import midisampler_model as mm
import slice_score as ss


def model_first_pitch(cfg, key, source_pitch):
    model = mm.EngineModel(cfg)
    for z in cfg.zones:
        if model._zone_matches(z, key):
            info = model.start_info(z, key)
            if info is None:
                return -1
            transpose = info[1]
            p = 2 * z.axis - source_pitch if z.invert else source_pitch
            return mm.fold_pitch(p + transpose)
    return -1


def pitch_probe(exe, tmp, runs=160):
    rng = random.Random(77)
    failures = 0
    source_pitch = 60
    # contiguous one-beat notes of a single pitch: whatever direction / window a voice starts in, the first note carries that pitch
    src = dict(slot=1, name="uniform.mid", lengthBeats=32.0, noteCount=32,
               notes=";".join(f"{i:.6f},1.000000,{source_pitch},100" for i in range(32)))
    for n in range(runs):
        lo = rng.randint(0, 100)
        hi = min(127, lo + rng.randint(3, 24))
        zone = dict(key_lo=lo, key_hi=hi, root=rng.randint(lo, hi), play_mode=rng.choice([0, 1, 2, 2]), slice_by="equal", count=rng.randint(2, 16),
                    slice_index=rng.randint(0, 3), base_shift=rng.randint(-14, 14), invert=rng.random() < 0.3, axis=rng.randint(50, 70),
                    key_map_set=rng.random() < 0.5, key_step=rng.choice([-5, -2, 1, 1, 2, 3, 5, 7]), pc_mask=rng.randint(1, 4095),
                    slice_base_key=rng.randint(max(0, lo - 3), lo + 3), mode=rng.choice(["forward", "reverse", "loop_forward", "loop_reverse"]),
                    playback="gate", phase=rng.choice([0.0, 0.0, 0.3]))
        path = os.path.join(tmp, "probe_setup.json")
        import json
        with open(path, "w") as f:
            json.dump(cc.make_setup_json([zone], voices=4, sources=[src]), f)
        cfg = mm.load_setup(path)
        key = rng.randint(lo, hi)
        expected = model_first_pitch(cfg, key, source_pitch)
        replay = os.path.join(tmp, "probe.txt")
        cc.write_replay2(replay, cfg, [(0.0, True, key, 100)], [8.0])
        out = subprocess.run([exe, replay], capture_output=True, text=True).stdout
        real = int([ln for ln in out.splitlines() if ln.startswith("FIRSTPITCH")][0].split()[1])
        if real != expected:
            failures += 1
            if failures <= 4:
                print(f"   probe {n}: key {key} zone {zone}\n      model {expected} vs engine {real}")
    print(f"{'OK  ' if failures == 0 else 'FAIL'} pitch probe: {runs} random zone setups, {failures} mismatch(es)")
    return failures


def negative_controls(exe, tmp):
    setups = dict(cc.synthetic_setups(random.Random(1234)))
    problems = 0

    def detect(label, setup_name, patch):
        nonlocal problems
        saved = patch()
        try:
            rc = cc.run_one(exe, "(control) " + label, setups[setup_name], range(1, 6), tmp)
        finally:
            saved()
        if rc == 0:
            problems += 1
            print(f"FAIL negative control '{label}' was NOT detected")
        else:
            print(f"OK   negative control '{label}' is detected")

    # 1. no voice stealing
    def no_stealing():
        original = mm.EngineModel.press
        def press(self, key, t, channel=0):
            saved_limit = self.cfg.voices
            self.cfg.voices = 99
            try:
                original(self, key, t, channel)
            finally:
                self.cfg.voices = saved_limit
        mm.EngineModel.press = press
        return lambda: setattr(mm.EngineModel, "press", original)
    detect("no voice stealing", "gate slice zones, voices 4 (stealing)", no_stealing)

    # 2. natural endings 30 % too late
    def wrong_ends():
        original = mm.EngineModel.natural_length
        mm.EngineModel.natural_length = lambda self, z, w, p, d: original(self, z, w, p, d) * 1.3
        return lambda: setattr(mm.EngineModel, "natural_length", original)
    detect("natural endings 30 % late", "latch one-shots", wrong_ends)

    # 3. latch ignored (a second press restarts instead of switching off)
    def no_latch():
        original = mm.Voice.__init__
        def init(self, *a, **k):
            original(self, *a, **k)
            self.latch = False
        mm.Voice.__init__ = init
        return lambda: setattr(mm.Voice, "__init__", original)
    detect("latch ignored", "latch one-shots", no_latch)
    return problems


def main() -> int:
    exe = cc.DEFAULT_EXE
    if len(sys.argv) > 2 and sys.argv[1] == "--exe":
        exe = sys.argv[2]
    if not os.path.exists(exe):
        print(f"MidiSamplerReplay.exe not found at {exe}")
        return 2
    with tempfile.TemporaryDirectory() as tmp:
        failures = pitch_probe(exe, tmp)
        failures += negative_controls(exe, tmp)
    print("ALL MODEL TESTS PASSED" if failures == 0 else f"{failures} problem(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
