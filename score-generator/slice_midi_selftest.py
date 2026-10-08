#!/usr/bin/env python3
"""Self-test of slice_midi_score.py on synthetic setups (no personal files): every generated clip must verify against the model and the
real engine, and a deliberately corrupted clip must be flagged (negative control).   python slice_midi_selftest.py"""
import json, os, random, sys, tempfile

import midi_score_crosscheck as cc
import slice_midi_score as sm


def run(setup, out, **kw):
    argv = ["--setup", setup, "--output", out, "--bars", str(kw.pop("bars", 24))]
    for k, v in kw.items():
        argv += [f"--{k.replace('_', '-')}"] + ([] if v is True else [str(v)])
    return sm.generate(sm.build_parser().parse_args(argv))


def main() -> int:
    bad = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, js in cc.synthetic_setups(random.Random(1234)):
            path = os.path.join(tmp, "s.json")
            json.dump(js, open(path, "w"))
            for seed in (1, 2, 3):
                try:
                    r = run(path, os.path.join(tmp, f"c{seed}.mid"), seed=seed, exe=cc.DEFAULT_EXE)
                    status = "OK  " if not r["problems"] else "FAIL"
                    detail = r["problems"] if r["problems"] else r["summary"]
                except SystemExit as e:
                    status, detail = "SKIP", str(e)[:90]        # e.g. latch zones are refused on purpose
                if status == "FAIL":
                    bad += 1
                print(f"{status} {name} seed {seed}: {detail}")
        # negative control: remove all Note-Offs-equivalents by truncating a clip's stop-all -> must be reported
        js = dict(cc.synthetic_setups(random.Random(1234)))["start only loops + stop key"]
        path = os.path.join(tmp, "n.json"); json.dump(js, open(path, "w"))
        r = run(path, os.path.join(tmp, "n.mid"), seed=5, exe=cc.DEFAULT_EXE)
        cfg = sm.mm.load_setup(path)
        notes = [n for n in sm.read_clip(r["clip"]) if n[1] != cfg.stop_key]
        ev_path = os.path.join(tmp, "broken.mid")
        class N:  # minimal stand-in for Note
            def __init__(s, a, k, b): s.start, s.key, s.end, s.velocity, s.kind = a, k, b, 90, "layer"
        sm.write_midi(ev_path, [N(a, k, b) for a, k, b in notes], 100, 24, 4, "broken")
        problems, _ = sm.verify(cfg, ev_path, 96.0, cc.DEFAULT_EXE, tmp)
        if problems:
            print("OK   negative control: clip without its stop-all presses is flagged")
        else:
            bad += 1; print("FAIL negative control not detected")
    print("ALL CLIP TESTS PASSED" if not bad else f"{bad} problem(s)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
