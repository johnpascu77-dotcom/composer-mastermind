#!/usr/bin/env python3
"""Cross-check of midisampler_model.py (the Python model of MidiSampler) against the REAL C++ engine, on whole setups with real
note events: random note-on/off streams are run through both and the sounding keys are compared at a checkpoint in the middle of every
sixteenth. Covers voice stealing, Gate / Latch / Start only, stop key, loops and directions, slice zones / per-key slices / sample zones,
overlapping zones, entry delay and phase.

    python midi_score_crosscheck.py [path\\to\\MidiSampler setup.json ...] [--exe path\\to\\MidiSamplerReplay.exe]

Any setup files given (Save Setup in the plugin) are checked too; a handful of synthetic setups always run. A mismatch means the model
differs from the plugin: fix the model, not the test.
"""

import json
import os
import random
import subprocess
import sys
import tempfile

import midisampler_model as mm

DEFAULT_EXE = r"C:\AudioDev\Repos\MidiSampler\build\Release\MidiSamplerReplay.exe"
SAMPLE_RATE = 44100
BPM = 120.0


# ---------------------------------------------------------------------------------------------- fixtures
def _idx(options, value):
    return options.index(value)


def make_setup_json(zones, voices=4, stop_key=-1, speed=100.0, sources=None):
    """zones: list of dicts using the human-level names of ZoneCfg (only what differs from the defaults)."""
    params = [("speed", speed), ("sync", 1), ("tempo", 120.0), ("voices", voices), ("stopKey", stop_key)]
    for i in range(8):
        z = i + 1
        d = dict(enabled=False, key_lo=0, key_hi=127, source=0, root=60, start01=0.0, end01=1.0, loop_start01=0.0, loop_end01=1.0,
                 mode="forward", playback="gate", play_mode=0, slice_by="beat", grid=1.0, count=8, min_beats=0.25, slice_thru=False,
                 slice_base_key=24, out_channel=0, invert=False, axis=60, ratio=1.0, key_map_set=False, key_step=1, pc_mask=4095,
                 delay=0.0, phase=0.0, warp_curve=0, warp_depth=0.0, warp_cycle=4.0, warp_phase=0.0, slice_index=0, base_shift=0)
        if i < len(zones):
            d.update(zones[i])
            d["enabled"] = zones[i].get("enabled", True)
        params += [
            (f"z{z}_on", 1 if d["enabled"] else 0), (f"z{z}_lo", d["key_lo"]), (f"z{z}_hi", d["key_hi"]), (f"z{z}_src", d["source"] + 1),
            (f"z{z}_root", d["root"]), (f"z{z}_start", d["start01"]), (f"z{z}_end", d["end01"]), (f"z{z}_loopStart", d["loop_start01"]),
            (f"z{z}_loopEnd", d["loop_end01"]), (f"z{z}_mode", _idx(mm.MODES, d["mode"])), (f"z{z}_gate", _idx(mm.PLAYBACKS, d["playback"])),
            (f"z{z}_playMode", d["play_mode"]), (f"z{z}_sliceBy", _idx(mm.SLICE_BYS, d["slice_by"])),
            (f"z{z}_sliceGrid", _idx(mm.GRIDS, d["grid"])), (f"z{z}_sliceCount", d["count"]), (f"z{z}_sliceMin", d["min_beats"]),
            (f"z{z}_slicePlay", 1 if d["slice_thru"] else 0), (f"z{z}_sliceKey", d["slice_base_key"]), (f"z{z}_chan", d["out_channel"]),
            (f"z{z}_invert", 1 if d["invert"] else 0), (f"z{z}_axis", d["axis"]),
            (f"z{z}_ratio", min(range(11), key=lambda k: abs(mm.RATIOS[k] - d["ratio"]))), (f"z{z}_keyMap", 1 if d["key_map_set"] else 0),
            (f"z{z}_step", d["key_step"]), (f"z{z}_pcset", d["pc_mask"]), (f"z{z}_delay", d["delay"]), (f"z{z}_phase", d["phase"]),
            (f"z{z}_wcurve", d["warp_curve"]), (f"z{z}_wdepth", d["warp_depth"]),
            (f"z{z}_wcycle", min(range(8), key=lambda k: abs(mm.CYCLES[k] - d["warp_cycle"]))), (f"z{z}_wphase", d["warp_phase"]),
            (f"z{z}_sliceIdx", d["slice_index"] + 1), (f"z{z}_shift", d["base_shift"]),
        ]
    return dict(format="MidiSamplerSetup", version=1, parameters=[dict(id=k, value=v) for k, v in params],
                manualSlices=[[] for _ in range(8)], sources=sources or [])


def make_source(rng, slot, beats=32.0, n=60):
    notes, t = [], 0.0
    while t < beats - 0.5 and len(notes) < n:
        length = rng.choice([0.25, 0.5, 0.5, 1.0, 1.5, 2.0])
        notes.append((t, length, rng.randint(48, 84), rng.randint(60, 110)))
        t += rng.choice([0.25, 0.5, 0.5, 1.0, 0.75])
    return dict(slot=slot + 1, name=f"synthetic{slot + 1}.mid", lengthBeats=beats, noteCount=len(notes),
                notes=";".join(f"{a:.6f},{b:.6f},{c},{d}" for a, b, c, d in notes))


def synthetic_setups(rng):
    src = [make_source(rng, 0), make_source(rng, 1, beats=24.0, n=40)]
    octave = lambda k: dict(key_lo=36 + 12 * k, key_hi=47 + 12 * k, root=36 + 12 * k)
    slice_zone = lambda k, idx, **kw: dict(octave(k), play_mode=2, slice_by="equal", count=12, slice_index=idx, **kw)
    return [
        ("gate slice zones, voices 4 (stealing)", make_setup_json([slice_zone(i, i * 2, mode="loop_bidir", playback="gate", ratio=0.5) for i in range(5)], voices=4, sources=src)),
        ("start only loops + stop key", make_setup_json([slice_zone(i, i, mode="loop_forward", playback="start_only", key_map_set=True, pc_mask=0b101011011011) for i in range(4)], voices=5, stop_key=23, sources=src)),
        ("latch one-shots", make_setup_json([slice_zone(i, i + 1, mode="forward", playback="latch", ratio=1.5) for i in range(3)], voices=6, sources=src)),
        ("per-key slices + sample zones", make_setup_json([dict(key_lo=24, key_hi=35, play_mode=1, slice_by="beat", grid=2.0, slice_base_key=24, mode="loop_reverse", playback="gate"),
                                                          dict(key_lo=48, key_hi=72, play_mode=0, root=60, mode="forward", playback="start_only", ratio=2.0, source=1, key_step=2)], voices=6, sources=src)),
        ("overlapping zones layer", make_setup_json([dict(key_lo=40, key_hi=70, mode="loop_forward", playback="gate"),
                                                    dict(key_lo=55, key_hi=90, mode="loop_bidir_reverse", playback="gate", source=1, ratio=0.5)], voices=8, sources=src)),
        ("phase, thru, reverse, speed 120", make_setup_json([slice_zone(0, 3, mode="reverse", playback="start_only", phase=0.4, slice_thru=True),
                                                            slice_zone(1, 5, mode="forward", playback="gate", phase=0.25)], voices=6, speed=120.0, sources=src)),
        ("entry delay", make_setup_json([slice_zone(0, 2, mode="loop_forward", playback="gate", delay=0.75),
                                         slice_zone(1, 4, mode="forward", playback="start_only", delay=1.5)], voices=6, sources=src)),
    ]


# ---------------------------------------------------------------------------------------------- random event streams
def random_events(rng, cfg, beats=64.0, density=0.9):
    keys = []
    for z in cfg.zones:
        if z.enabled:
            keys += list(range(z.key_lo, min(z.key_hi, z.key_lo + 24) + 1))
    keys += [5, 100, 126]                                           # some keys no zone answers to
    if cfg.stop_key >= 0:
        keys += [cfg.stop_key] * 2
    events, t = [], 0.0
    while t < beats - 4.0:
        if rng.random() < density:
            key = rng.choice(keys)
            length = rng.choice([1, 2, 3, 4, 6, 8, 12, 16, 24, 32]) * 0.25
            events.append((t, True, key, 100))
            events.append((t + length, False, key, 0))
        t += rng.choice([1, 1, 2, 3, 4, 8]) * 0.25
    events.sort(key=lambda e: (e[0], e[1]))                         # releases first at the same beat
    return [e for e in events if e[0] <= beats]


def write_replay2(path, cfg, events, checks):
    lines = ["engine2", f"bpm {BPM}", f"sr {SAMPLE_RATE}", f"voices {cfg.voices}", f"speed {cfg.speed}", f"stopkey {cfg.stop_key}"]
    for slot, seq in cfg.sources.items():
        lines.append(f"source {slot} {seq.length_beats!r} {len(seq.notes)}")
        lines += [f"{n.start!r} {n.length!r} {n.pitch}" for n in seq.notes]
    for z in cfg.zones:
        lines.append(" ".join(str(x) for x in [
            "zonefull", z.index, int(z.enabled), z.key_lo, z.key_hi, z.source, z.start01, z.end01, z.loop_start01, z.loop_end01,
            mm.MODES.index(z.mode), mm.PLAYBACKS.index(z.playback), z.root, z.ratio, int(z.play_mode >= 1), int(z.play_mode == 2),
            mm.SLICE_BYS.index(z.slice_by), z.grid, z.count, z.min_beats, z.slice_base_key, int(z.slice_thru), z.slice_index, z.base_shift,
            int(z.key_map_set), z.key_step, z.pc_mask, int(z.invert), z.axis, z.out_channel, z.delay, z.phase, z.warp_curve,
            z.warp_depth, z.warp_cycle, z.warp_phase, len(z.manual), *z.manual]))
    lines.append(f"events {len(events)}")
    lines += [f"{t!r} {int(on)} {key} {vel}" for t, on, key, vel in events]
    lines.append(f"checksb {len(checks)}")
    lines += [repr(c) for c in checks]
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def run_one(exe, name, setup_path_or_dict, seeds, tmp):
    if isinstance(setup_path_or_dict, dict):
        path = os.path.join(tmp, "setup.json")
        with open(path, "w") as f:
            json.dump(setup_path_or_dict, f)
    else:
        path = setup_path_or_dict
    cfg = mm.load_setup(path)
    total_checks = mismatches = ambiguous = with_sound = max_voices_seen = steals = 0
    for seed in seeds:
        rng = random.Random(seed)
        events = random_events(rng, cfg)
        checks = [(k + 0.5) * 0.25 for k in range(int(64.0 / 0.25))]
        replay = os.path.join(tmp, "replay.txt")
        write_replay2(replay, cfg, events, checks)
        result = subprocess.run([exe, replay], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"ERROR {name}: replay tool failed: {result.stderr.strip()}")
            return 1
        lines = result.stdout.strip().splitlines()
        real = [[int(x) for x in ln.split()[1:]] for ln in lines if not ln.startswith("END")]

        model = mm.EngineModel(cfg)
        i = 0
        for k, check in enumerate(checks):
            while i < len(events) and events[i][0] <= check:
                t, on, key, _ = events[i]
                (model.press if on else model.release)(key, t)
                i += 1
            keys = model.active_keys(check)
            max_voices_seen = max(max_voices_seen, model.active_count(check))
            total_checks += 1
            with_sound += 1 if real[k] else 0
            if keys != real[k]:
                diff = set(keys) ^ set(real[k])
                if diff <= model.ambiguous_keys(check, 0.02):
                    ambiguous += 1
                    continue
                mismatches += 1
                if mismatches <= 3:
                    print(f"   {name} seed {seed} beat {check:.3f}: model {keys} vs engine {real[k]}")
    status = "OK  " if mismatches == 0 else "FAIL"
    note = f"  [{'; '.join(cfg.approximations)}]" if cfg.approximations else ""
    print(f"{status} {name}: {total_checks} checkpoints ({with_sound} with sound, up to {max_voices_seen} voices), "
          f"{mismatches} mismatch(es), {ambiguous} ambiguous-skipped{note}")
    return 1 if mismatches else 0


def main() -> int:
    args = sys.argv[1:]
    exe = DEFAULT_EXE
    if "--exe" in args:
        i = args.index("--exe")
        exe = args[i + 1]
        del args[i:i + 2]
    if not os.path.exists(exe):
        print(f"MidiSamplerReplay.exe not found at {exe}\nBuild it: cmake --build build --config Release --target MidiSamplerReplay (in C:\\AudioDev\\Repos\\MidiSampler)")
        return 2
    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, setup in synthetic_setups(random.Random(1234)):
            failures += run_one(exe, name, setup, range(1, 6), tmp)
        for path in args:
            failures += run_one(exe, "YOUR SETUP " + os.path.basename(path), path, range(1, 9), tmp)
    print("THE MODEL AGREES WITH THE REAL ENGINE" if failures == 0 else f"{failures} setup(s) disagree")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
