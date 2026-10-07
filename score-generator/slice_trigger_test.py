#!/usr/bin/env python3
"""Writes the "composing by slices" timing-test kit (docs/composing_by_slices_concept.md, build-order step 2):

  docs/slice_test/slice_test_source.mid       8-bar monophonic source, one clearly different bar per slice
  docs/example_score_slice_trigger_test.json  11-bar bundle: one MPL "trigger" instance firing slice keys

Purpose: verify LIVE that 1-bar sections land on the right bar (MPL only applies a queued pattern at a bar start, so any
off-by-one-bar latency in Mastermind's section switching shows up as every trigger arriving a bar late), and that latch
ON/OFF, silent multi-bar holds and the stop-all key behave. Not a musical piece.

MidiSampler setup this bundle assumes (see docs/slice_test/README.md):
  Mode Slice, Slice by Beat grid = 4 beats, first slice key 24 (shown as C0), Sustain Loop Forward, Playback Trigger (latch),
  Stop key 23 (shown as B-1), source = slice_test_source.mid.

Deterministic, no randomness. Usage:  python slice_trigger_test.py
"""

from __future__ import annotations

import json
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.join(HERE, "..", "docs")
OUT_DIR = os.path.join(DOCS, "slice_test")

PPQ = 480
BPM = 100
BEATS_PER_BAR = 4
PATTERN_STEPS = 16

SLICE_BASE_KEY = 24          # shown as C0 in MidiSampler (middle C = C3): slice 1 = key 24, slice 2 = 25, ...
STOP_KEY = 23                # shown as B-1
SHOOTER = "SHOOT1"
SHOOTER_CHANNEL = 1


# ---------------------------------------------------------------- source .mid
def vlq(value: int) -> bytes:
    out = [value & 0x7F]
    value >>= 7
    while value:
        out.append((value & 0x7F) | 0x80)
        value >>= 7
    return bytes(reversed(out))


def write_source_midi(path: str) -> None:
    """8 bars of 4/4, one arpeggio per bar, each starting on a higher root so you can tell by ear which source bar
    (= which slice) is sounding. Strictly monophonic."""
    roots = [48, 50, 52, 55, 57, 60, 62, 64]                  # bar 1..8
    shape = [0, 4, 7, 12]                                      # root, third, fifth, octave: one note per beat
    events = []                                                # (tick, order, bytes)
    for bar, root in enumerate(roots):
        for beat, interval in enumerate(shape):
            start = (bar * BEATS_PER_BAR + beat) * PPQ
            note = root + interval
            events.append((start, 1, bytes([0x90, note, 96])))
            events.append((start + PPQ - 40, 0, bytes([0x80, note, 0])))   # tiny gap so the line stays monophonic
    events.sort(key=lambda e: (e[0], e[1]))

    track = bytearray()
    track += vlq(0) + bytes([0xFF, 0x51, 0x03]) + int(60_000_000 / BPM).to_bytes(3, "big")   # tempo
    track += vlq(0) + bytes([0xFF, 0x58, 0x04, 4, 2, 24, 8])                                  # 4/4
    last = 0
    for tick, _, data in events:
        track += vlq(tick - last) + data
        last = tick
    end_tick = len(roots) * BEATS_PER_BAR * PPQ
    track += vlq(end_tick - last) + bytes([0xFF, 0x2F, 0x00])

    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ))
        f.write(b"MTrk" + struct.pack(">I", len(track)) + bytes(track))


# ---------------------------------------------------------------- bundle
def empty_steps() -> list[dict]:
    return [dict(enabled=False, note=0, velocity=0, duration=0) for _ in range(PATTERN_STEPS)]


def fire_steps(fires: dict[int, int]) -> list[dict]:
    """fires: {step_index: key}. One trigger per step; duration 1 because a latch zone ignores note-offs anyway."""
    steps = empty_steps()
    for step, key in fires.items():
        steps[step] = dict(enabled=True, note=key, velocity=100, duration=1)
    return steps


def build_timeline() -> list[tuple[int, int, dict[int, int], str]]:
    """(start_bar, duration_bars, {step: key}, what-you-should-hear). Bars are 1-based like every bundle."""
    k = lambda slice_no: SLICE_BASE_KEY + slice_no - 1
    return [
        (1, 1, {0: k(1)},            "slice 1 (key 24) ON at beat 1 (loops: source bar 1 arpeggio, low C)"),
        (2, 2, {},                   "silent section: slice 1 keeps looping (bars 2-3)"),
        (4, 1, {0: k(1), 4: k(3)},   "beat 1: slice 1 OFF. beat 2: slice 3 ON (source bar 3 arpeggio, loops off the bar grid by one beat)"),
        (5, 2, {},                   "silent section: slice 3 keeps looping (bars 5-6)"),
        (7, 1, {0: k(3), 8: k(4)},   "beat 1: slice 3 OFF. beat 3: slice 4 ON (source bar 4 arpeggio)"),
        (8, 1, {},                   "silent: slice 4 loops"),
        (9, 1, {0: STOP_KEY},        "beat 1: STOP-ALL key. Everything goes silent"),
        (10, 2, {},                  "silent: confirm nothing is left sounding (bars 10-11)"),
    ]


def build_bundle() -> dict:
    sections, scenes = [], []
    for index, (start_bar, bars, fires, _) in enumerate(build_timeline()):
        pattern_index = index % 3                      # always write to a pattern that is not the one playing
        section_id = f"slicetest_{index + 1:02d}"
        scene_id = f"{section_id}_scene"

        scenes.append({
            "id": scene_id, "name": scene_id, "durationBars": bars, "quantize": "bar",
            "targets": [SHOOTER],
            "global": dict(activePattern=pattern_index + 1, gridMode=0, swing=0, rate=1),
            "instanceOverrides": [], "patterns": [], "mutations": [],
            "nextSceneId": "", "transitionStyle": "hard", "rampBars": 0,
        })
        sections.append(dict(
            id=section_id, name=section_id, sceneId=scene_id, startBar=start_bar, durationBars=bars,
            archetype="", layerRoles=[], budgetOverrides=[], reservedValues=[], modulatorValues=[],
            capturedContent=[dict(targetInstance=SHOOTER, patternIndex=pattern_index, steps=fire_steps(fires))],
        ))

    return dict(
        schema="ComposerMastermindComposition.v3",
        blueprint=dict(id="slice_trigger_test", name="Slice Trigger Test", sections=sections, arcCurves=[]),
        scenes=scenes,
        instances=[dict(id=SHOOTER, name=SHOOTER, midiChannel=SHOOTER_CHANNEL, role="trigger", enabled=True, lastKnownScene="")],
        motifPresets=[], modulatorTargets=[], modulationRoutes=[],
    )


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    midi_path = os.path.join(OUT_DIR, "slice_test_source.mid")
    write_source_midi(midi_path)

    bundle_path = os.path.join(DOCS, "example_score_slice_trigger_test.json")
    with open(bundle_path, "w", encoding="utf-8") as f:
        json.dump(build_bundle(), f, indent=2)

    print(f"Wrote {os.path.normpath(midi_path)}")
    print(f"Wrote {os.path.normpath(bundle_path)}")
    print("\nExpected timeline (bundle bar numbers; MEASURED LIVE 2026-10-08: it sounds exactly 2 bars later in Bitwig):")
    for start_bar, bars, fires, expect in build_timeline():
        end = start_bar + bars - 1
        span = f"bar {start_bar}" if bars == 1 else f"bars {start_bar}-{end}"
        print(f"  {span:<10} {expect}")


if __name__ == "__main__":
    main()
