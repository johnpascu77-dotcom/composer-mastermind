#!/usr/bin/env python3
"""Offline generator for "composing by slices" scores (docs/composing_by_slices_concept.md).

MPL instances are only trigger "shooters": each fires a key (note number) that MidiSampler (downstream) maps to a slice of a
loaded MIDI source. This tool plans WHICH slices sound WHEN over a whole piece, driven by the same energy-arc shapes the rest
of this generator uses, and writes an ordinary composition bundle (ComposerMastermindComposition.v3) whose sections hold literal
captured patterns, plus a companion MidiSampler setup sheet and a bar-by-bar timeline.

Why it is built the way it is (all measured live 2026-10-08, see docs/slice_test/README.md):
  * MPL repeats its pattern every cycle, so a trigger fired once per cycle toggles a latch EVERY bar. A held state across bars
    therefore needs bar-resolution sections: one 1-bar section for every bar that has trigger events, one multi-bar silent
    section for every run of bars without events (the latch keeps the slices sounding through it).
  * Pattern index rotates P1 -> P2 -> P3 -> P1 ... per section, so the pattern being written is never the one MPL is playing.
  * Mastermind and MPL together delay every section by exactly 2 bars: a section with startBar N is heard in Bitwig bar N+2.
    This tool plans in BUNDLE bars and states the audible bar (+2) everywhere; the first sound is in Bitwig bar 3.
  * A latch press on a slice that already ended by itself would START it again, so the planner simulates every voice and only
    presses when its model says the press does what is intended. An independent replay of the finished bundle checks that.

MidiSampler is mirrored faithfully (SamplerEngine.h computeRanges/computeSliceBoundaries; Playback Latch/Start only; stop-all
key), the same convention this folder uses for ArcShapeLibrary and BlueprintGenerator.

Usage:
    python slice_score.py --title "Bach Clouds" --source ../docs/slice_test/slice_test_source.mid --minutes 2 --bpm 110 \\
        --shape arch_rise_fall --strategy arch --seed 7 --output ../docs/slice_score_demo.json
"""

from __future__ import annotations

import argparse
import json
import math
import os
import random
import struct
import sys
from dataclasses import dataclass, field

import arc

PATTERN_STEPS = 16

# ---------------------------------------------------------------------------------------------------------------------------
# RUNNING FROM IDLE: open this file (File > Open) and press F5. With no command-line options the settings below are used.
# Edit them, save, press F5 again. Paths are relative to this file's folder. (From a terminal you can instead pass the same
# options after the file name, e.g.  python slice_score.py --title X --source Y.mid --output Z.json --strategy row ...)
# ---------------------------------------------------------------------------------------------------------------------------
IDLE_SETTINGS = [
    "--title", "slice_demo",
    "--source", "../docs/slice_test/slice_test_source.mid",   # the .mid file you load into MidiSampler
    "--output", "../docs/slice_score_demo.json",              # .setup.md and .timeline.txt are written next to it
    "--minutes", "1.5",
    "--bpm", "110",
    "--shape", "arch_rise_fall",   # energy shape over the whole piece (see arc.py SHAPES)
    "--strategy", "arch",          # order | retro | arch | random | row
    "--sustain", "loop",           # loop | forward
    "--playback", "latch",         # latch | start_only
    "--slice-by", "beat",          # beat | equal | transient
    "--grid", "4",                 # slice length in beats when --slice-by beat
    "--max-layers", "3",
    "--seed", "7",
]
LAG_BARS = 2                 # measured: bundle bar N is heard in Bitwig bar N + LAG_BARS
MAX_SLICES = 64              # SamplerEngine.h kMaxSlices
EPS = 1e-6


# ======================================================================== MIDI source (mirror of MidiSamplerProcessor::loadMidiFile)
@dataclass
class SeqNote:
    start: float
    length: float
    pitch: int

    @property
    def end(self) -> float:
        return self.start + self.length


@dataclass
class Sequence:
    notes: list[SeqNote]
    length_beats: float


def _read_vlq(data: bytes, pos: int) -> tuple[int, int]:
    value = 0
    while True:
        byte = data[pos]
        pos += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, pos


def read_midi_sequence(path: str) -> Sequence:
    """All tracks merged, note-on/off paired per (channel, pitch), ticks -> beats. Same rules as the plugin: an unterminated note
    ends at the last event time, a note is at least one tick long, total length = max(last note end, last event time)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"MThd":
        raise ValueError("Not a MIDI file")
    _, fmt, ntracks, division = struct.unpack(">IHHH", data[4:14])
    del fmt
    if division & 0x8000:
        raise ValueError("SMPTE-timed MIDI files are not supported")
    ppq = float(division)

    notes: list[SeqNote] = []
    max_tick = 0
    pos = 14
    for _ in range(ntracks):
        if data[pos:pos + 4] != b"MTrk":
            raise ValueError("Bad track chunk")
        size = struct.unpack(">I", data[pos + 4:pos + 8])[0]
        track = data[pos + 8:pos + 8 + size]
        pos += 8 + size

        tick = 0
        p = 0
        status = 0
        open_notes: dict[tuple[int, int], int] = {}
        while p < len(track):
            delta, p = _read_vlq(track, p)
            tick += delta
            max_tick = max(max_tick, tick)
            b = track[p]
            if b == 0xFF:                                # meta
                length, p2 = _read_vlq(track, p + 2)
                p = p2 + length
                continue
            if b in (0xF0, 0xF7):                        # sysex
                length, p2 = _read_vlq(track, p + 1)
                p = p2 + length
                continue
            if b & 0x80:
                status = b
                p += 1
            kind, channel = status & 0xF0, status & 0x0F
            if kind in (0xC0, 0xD0):
                p += 1
                continue
            d1, d2 = track[p], track[p + 1]
            p += 2
            if kind == 0x90 and d2 > 0:
                open_notes[(channel, d1)] = tick
            elif kind == 0x80 or (kind == 0x90 and d2 == 0):
                start = open_notes.pop((channel, d1), None)
                if start is not None:
                    notes.append(SeqNote(start / ppq, max(1, tick - start) / ppq, d1))
        for (channel, pitch), start in open_notes.items():
            notes.append(SeqNote(start / ppq, max(1, max_tick - start) / ppq, pitch))

    if not notes:
        raise ValueError("The file contains no notes")
    notes.sort(key=lambda n: n.start)
    last_end = max(n.end for n in notes)
    return Sequence(notes, max(last_end, max_tick / ppq))


# ======================================================================== MidiSampler slicing (mirror of SamplerEngine.h)
@dataclass
class Zone:
    start01: float = 0.0
    end01: float = 1.0
    slice_by: str = "beat"            # beat | equal | transient | manual
    grid_beats: float = 4.0
    count: int = 8
    min_beats: float = 0.25
    manual: list[float] = field(default_factory=list)   # fractions of the source length
    base_key: int = 24
    thru: bool = False
    loop: bool = True                  # Sustain mode Loop Forward (True) or Forward one-shot (False)
    playback: str = "latch"            # latch | start_only
    ratio: float = 1.0
    min_loop_beats: float = 0.0625


def compute_range(zone: Zone, seq: Sequence) -> tuple[float, float]:
    length = seq.length_beats
    start = min(max(zone.start01, 0.0), 1.0) * length
    end = min(max(zone.end01, 0.0), 1.0) * length
    if end < start + zone.min_loop_beats:
        end = min(length, start + zone.min_loop_beats)
    if end < start + 1e-9:
        start = max(0.0, end - zone.min_loop_beats)
    return start, end


def compute_slice_boundaries(seq: Sequence, zone: Zone) -> list[float]:
    """Ascending boundaries b[0..n]; slice i = [b[i], b[i+1]). Port of computeSliceBoundaries."""
    start, end = compute_range(zone, seq)
    b = [start]

    def push(x: float, min_gap: float) -> None:
        if len(b) < MAX_SLICES and x >= b[-1] + min_gap and x < end - 1e-6:
            b.append(x)

    if zone.slice_by == "beat":
        g = max(zone.grid_beats, 1.0 / 64.0)
        i = 1
        while len(b) < MAX_SLICES:
            x = start + g * i
            if x >= end - 1e-6:
                break
            push(x, 1e-6)
            i += 1
    elif zone.slice_by == "equal":
        cnt = min(max(zone.count, 1), MAX_SLICES)
        for i in range(1, cnt):
            push(start + (end - start) * i / cnt, 1e-6)
    elif zone.slice_by == "transient":
        gap = max(zone.min_beats, 1e-6)
        for note in seq.notes:
            if note.start <= start:
                continue
            if note.start >= end:
                break
            push(note.start, gap)
    elif zone.slice_by == "manual":
        for x in sorted(f * seq.length_beats for f in zone.manual[:MAX_SLICES]):
            if x > start:
                push(x, 1e-4)
    else:
        raise ValueError(f"Unknown slice-by '{zone.slice_by}'")

    b.append(end)
    return b


def slice_bar_lengths(seq: Sequence, zone: Zone, speed_percent: float, beats_per_bar: int) -> list[float]:
    """How many host bars each slice lasts when played once (MidiSampler advances source beats at host tempo * speed * ratio;
    in Thru mode a slice plays from its start to the end of the range)."""
    b = compute_slice_boundaries(seq, zone)
    _, range_end = compute_range(zone, seq)
    rate = max(1e-6, (speed_percent / 100.0) * zone.ratio)
    out = []
    for i in range(len(b) - 1):
        end = range_end if zone.thru else b[i + 1]
        out.append((end - b[i]) / rate / beats_per_bar)
    return out


# ======================================================================== voice model (mirror of MidiSampler Latch / Start only / stop-all)
class VoiceModel:
    """Time is in bundle bars as a float (bar 1 starts at 0.0). A voice is `active` while until > t."""

    def __init__(self, bar_lengths: list[float], zone: Zone):
        self.bar_lengths = bar_lengths
        self.zone = zone
        self.until: dict[int, float] = {}           # slice index -> time it stops (inf = loops until pressed again / stop-all)

    def active(self, idx: int, t: float) -> bool:
        return self.until.get(idx, -1.0) > t + EPS

    def active_set(self, t: float) -> list[int]:
        return sorted(i for i in self.until if self.until[i] > t + EPS)

    def press(self, idx: int, t: float) -> str:
        """Returns what the press did: 'start', 'restart' or 'stop'."""
        was_active = self.active(idx, t)
        if self.zone.playback == "latch" and was_active:
            self.until[idx] = t
            return "stop"
        self.until[idx] = math.inf if self.zone.loop else t + self.bar_lengths[idx]
        return "restart" if was_active else "start"

    def stop_all(self, t: float) -> None:
        for i in list(self.until):
            if self.until[i] > t:
                self.until[i] = t


# ======================================================================== strategies: which slice comes next
class Strategy:
    def __init__(self, name: str, n_slices: int, rng: random.Random):
        self.name, self.n, self.rng = name, n_slices, rng
        self.pointer = 0
        self.row: list[int] = []
        self.form = 0
        if name == "row":
            if n_slices < 12:
                raise ValueError("--strategy row needs at least 12 slices (use --slice-by equal --count 12)")
            self.row = list(range(12))
            rng.shuffle(self.row)

    def _row_form(self) -> list[int]:
        row = self.row
        inv = [(12 - x) % 12 for x in row]
        return [row, inv, row[::-1], inv[::-1]][self.form % 4]

    def next_slice(self, energy: float, excluded: set[int]) -> int | None:
        candidates = [i for i in range(self.n) if i not in excluded]
        if not candidates:
            return None
        if self.name == "order":
            for _ in range(self.n):
                idx = self.pointer % self.n
                self.pointer += 1
                if idx not in excluded:
                    return idx
        elif self.name == "retro":
            for _ in range(self.n):
                idx = (self.n - 1 - self.pointer) % self.n
                self.pointer += 1
                if idx not in excluded:
                    return idx
        elif self.name == "arch":
            target = energy * (self.n - 1) + self.rng.uniform(-1.0, 1.0)
            return min(candidates, key=lambda i: abs(i - target))
        elif self.name == "row":
            form = self._row_form()
            for _ in range(12):
                idx = form[self.pointer % 12]
                self.pointer += 1
                if idx < self.n and idx not in excluded:
                    return idx
            return self.rng.choice(candidates)
        elif self.name == "random":
            return self.rng.choice(candidates)
        else:
            raise ValueError(f"Unknown strategy '{self.name}'")
        return self.rng.choice(candidates)

    def advance_form(self) -> None:
        self.form += 1


# ======================================================================== planner
@dataclass
class Event:
    bar: int            # bundle bar, 1-based
    step: int           # 0..15
    key: int
    intent: str         # 'start' | 'stop' | 'stopall'


@dataclass
class PlanConfig:
    bars: int
    bpm: float
    beats_per_bar: int
    shape: str
    strategy: str
    seed: int
    max_layers: int
    max_voices: int
    min_hold: int
    max_hold: int
    swap_chance: float
    breaths: bool
    stop_key: int | None
    shooters: int
    tail_bars: int
    restlessness: float
    speed_percent: float = 100.0


STEP_ORDER = [0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15]   # preferred step positions for events within a bar


def energy_curve(cfg: PlanConfig, rng: random.Random) -> list[float]:
    play_bars = max(1, cfg.bars - cfg.tail_bars)
    values = []
    for m in range(1, cfg.bars + 1):
        if m > play_bars:
            values.append(0.0)
            continue
        t = (m - 0.5) / play_bars
        e = arc.sample_shape(cfg.shape, t) + rng.uniform(-1, 1) * cfg.restlessness * 0.15
        values.append(max(0.0, min(1.0, e)))
    return values


def plan_events(cfg: PlanConfig, seq: Sequence, zone: Zone) -> tuple[list[Event], list[float], list[list[int]]]:
    """Returns (events sorted by time, energy per bar, active slice set at the END of each bar)."""
    rng = random.Random(cfg.seed)
    bar_lengths = slice_bar_lengths(seq, zone, cfg.speed_percent, cfg.beats_per_bar)
    n = len(bar_lengths)
    model = VoiceModel(bar_lengths, zone)
    strategy = Strategy(cfg.strategy, n, rng)
    energy = energy_curve(cfg, rng)
    play_bars = max(1, cfg.bars - cfg.tail_bars)

    events: list[Event] = []
    active_after: list[list[int]] = []
    next_decision = 1
    started_order: list[int] = []                # latch + loop: oldest first, for choosing what to stop
    margin = 1.0 / PATTERN_STEPS

    for m in range(1, cfg.bars + 1):
        e = energy[m - 1]
        t0 = float(m - 1)
        bar_events: list[tuple[str, int]] = []    # (intent, slice index) in intended order

        in_tail = m > play_bars
        last_play_bar = (m == play_bars)

        if zone.loop:
            # ---- sustaining layers: change the set of latched slices every `hold` bars
            if m >= next_decision or last_play_bar:
                hold = int(round(cfg.max_hold + (cfg.min_hold - cfg.max_hold) * e))
                next_decision = m + max(1, hold)
                if in_tail or last_play_bar:
                    target = 0
                elif cfg.breaths and e < 0.08 and m > 2:                   # never open the piece with a breath
                    target = 0
                else:
                    target = max(1, min(cfg.max_layers, cfg.max_voices, int(round(1 + e * (cfg.max_layers - 1)))))
                current = [i for i in started_order if model.active(i, t0)]
                already_sounding = set(current)                                 # only these can be stopped this bar
                changes = 0
                limit = 4                                                      # max trigger events per bar
                if zone.playback == "start_only":
                    # A Start-only voice cannot be stopped individually. Shrinking or reshuffling the layers is therefore a
                    # stop-all followed by restarting the new set in the same bar (stop-all always precedes the starts).
                    reshuffle = bool(current) and (len(current) > target
                                                   or (target > 0 and rng.random() < cfg.swap_chance * (0.4 + e)))
                    if reshuffle:
                        bar_events.append(("stopall", -1))
                        current = []
                        already_sounding = set()
                else:
                    # stop extras (oldest first), then start missing ones
                    while len(current) > target and changes < limit:
                        victim = current.pop(0)
                        bar_events.append(("stop", victim))
                        changes += 1
                stopped_now = {i for kind, i in bar_events if kind == "stop"}
                while len(current) < target and changes < limit:
                    pick = strategy.next_slice(e, set(current) | stopped_now)
                    if pick is None:
                        break
                    current.append(pick)
                    bar_events.append(("start", pick))
                    changes += 1
                if (zone.playback == "latch" and len(current) == target and target > 0 and changes < limit - 1
                        and rng.random() < cfg.swap_chance * (0.4 + e)):
                    candidates = [i for i in current if i in already_sounding]    # never a slice that starts in this same bar
                    if candidates:
                        victim = candidates[0]                                 # swap: one out, one in, same layer count
                        pick = strategy.next_slice(e, set(current) | {victim})
                        if pick is not None:
                            current.remove(victim)
                            bar_events.append(("stop", victim))
                            bar_events.append(("start", pick))
                            current.append(pick)
                if m % 8 == 0:
                    strategy.advance_form()
        else:
            # ---- one-shot slices: fire new ones with a probability that follows the energy curve
            if not in_tail:
                active_now = [i for i in model.active_set(t0)]
                free_voices = min(cfg.max_layers, cfg.max_voices) - len(active_now)
                fire_p = 0.15 + 0.85 * e
                if cfg.breaths and e < 0.08 and m > 2:
                    fire_p = 0.0
                if m == 1:
                    fire_p = 1.0                                               # the first bar always starts something
                tries = 0
                while free_voices > 0 and tries < 2 and rng.random() < fire_p:
                    tries += 1
                    excluded = set(active_now) if zone.playback == "latch" else set()
                    # a latch one-shot press must land on a voice that is certainly finished, not one ending within a step
                    if zone.playback == "latch":
                        excluded |= {i for i in range(n) if model.until.get(i, -1.0) > t0 - margin}
                    pick = strategy.next_slice(e, excluded)
                    if pick is None:
                        break
                    bar_events.append(("start", pick))
                    active_now.append(pick)
                    free_voices -= 1

        # ---- end of piece: make sure nothing is left sounding (stop-all key if available, otherwise explicit stops)
        if last_play_bar and zone.loop and zone.playback == "latch" and cfg.stop_key is None:
            for i in list(started_order):
                if model.active(i, t0) and ("stop", i) not in bar_events:
                    bar_events.append(("stop", i))

        # ---- assign steps and apply to the model in time order
        steps = STEP_ORDER[:]
        if bar_events:
            first = rng.choice([0, 0, 0, 4, 8])
            steps = [first] + [s for s in STEP_ORDER if s != first]
        used = 0
        stopall_needed = False
        if (last_play_bar and zone.loop and cfg.stop_key is not None
                and (model.active_set(t0) or any(k == "start" for k, _ in bar_events))):
            stopall_needed = True

        planned: list[Event] = []
        # stops precede starts inside a bar so a swap never exceeds the voice budget
        ordered = ([x for x in bar_events if x[0] == "stopall"] + [x for x in bar_events if x[0] == "stop"]
                   + [x for x in bar_events if x[0] == "start"])
        step_iter = iter(steps)
        for intent, idx in ordered:
            step = next(step_iter)
            if intent == "stopall":
                planned.append(Event(m, step, cfg.stop_key, "stopall"))
            else:
                planned.append(Event(m, step, zone.base_key + idx, intent))
            used += 1
        if stopall_needed:
            planned = [Event(m, 0, cfg.stop_key, "stopall")]       # supersedes the individual events of that bar

        planned.sort(key=lambda ev: ev.step)
        # make sure stops still precede starts after sorting by step: re-assign steps in ascending order of intent
        if len(planned) > 1 and planned[0].intent != "stopall":
            stops = [ev for ev in planned if ev.intent in ("stopall", "stop")]
            starts = [ev for ev in planned if ev.intent == "start"]
            slots = sorted(ev.step for ev in planned)
            for ev, step in zip(stops + starts, slots):
                ev.step = step
            planned = stops + starts

        for ev in planned:
            t = t0 + ev.step / PATTERN_STEPS
            if ev.intent == "stopall":
                model.stop_all(t)
                started_order.clear()
            else:
                idx = ev.key - zone.base_key
                result = model.press(idx, t)
                if ev.intent == "start":
                    assert result in ("start", "restart"), f"bar {m}: planned start became {result}"
                    if idx in started_order:
                        started_order.remove(idx)
                    started_order.append(idx)
                else:
                    assert result == "stop", f"bar {m}: planned stop became {result}"
                    if idx in started_order:
                        started_order.remove(idx)
            events.append(ev)

        active_after.append(model.active_set(float(m) - EPS))

    return events, energy, active_after


# ======================================================================== compiler: events -> sections with P1/P2/P3 rotation
def empty_steps() -> list[dict]:
    return [dict(enabled=False, note=0, velocity=0, duration=0) for _ in range(PATTERN_STEPS)]


def steps_for(events: list[Event], velocities: dict[tuple[int, int], int]) -> list[dict]:
    steps = empty_steps()
    for ev in events:
        steps[ev.step] = dict(enabled=True, note=ev.key, velocity=velocities.get((ev.bar, ev.step), 100), duration=1)
    return steps


def compile_bundle(title: str, cfg: PlanConfig, zone: Zone, events: list[Event], energy: list[float]) -> dict:
    by_bar: dict[int, list[Event]] = {}
    for ev in events:
        by_bar.setdefault(ev.bar, []).append(ev)

    # trigger velocity follows the energy curve (MidiSampler scales source velocity by it): soft low energy, hard peaks
    velocities = {}
    for ev in events:
        velocities[(ev.bar, ev.step)] = int(round(55 + 70 * energy[ev.bar - 1]))

    runs: list[tuple[int, int, bool]] = []          # (start_bar, bars, has_events)
    m = 1
    while m <= cfg.bars:
        if m in by_bar:
            runs.append((m, 1, True))
            m += 1
        else:
            end = m
            while end + 1 <= cfg.bars and (end + 1) not in by_bar:
                end += 1
            runs.append((m, end - m + 1, False))
            m = end + 1

    instance_ids = [f"SHOOT{i + 1}" for i in range(cfg.shooters)]

    def shooter_of(key: int) -> int:
        return (key - zone.base_key) % cfg.shooters if key != cfg.stop_key else 0

    sections, scenes = [], []
    for index, (start_bar, bars, has_events) in enumerate(runs):
        pattern_index = index % 3
        section_id = f"{title}_s{index + 1:03d}"
        scene_id = f"{section_id}_scene"
        captured = []
        for s, instance_id in enumerate(instance_ids):
            mine = [ev for ev in by_bar.get(start_bar, []) if shooter_of(ev.key) == s] if has_events else []
            captured.append(dict(targetInstance=instance_id, patternIndex=pattern_index, steps=steps_for(mine, velocities)))
        scenes.append({
            "id": scene_id, "name": scene_id, "durationBars": bars, "quantize": "bar", "targets": instance_ids,
            "global": dict(activePattern=pattern_index + 1, gridMode=0, swing=0, rate=1),
            "instanceOverrides": [], "patterns": [], "mutations": [], "nextSceneId": "", "transitionStyle": "hard", "rampBars": 0,
        })
        sections.append(dict(
            id=section_id, name=section_id, sceneId=scene_id, startBar=start_bar, durationBars=bars, archetype="",
            layerRoles=[], budgetOverrides=[], reservedValues=[], modulatorValues=[], capturedContent=captured,
        ))

    arc_points = [dict(bar=m, value=round(energy[m - 1], 3)) for m in range(1, cfg.bars + 1, max(1, cfg.bars // 24))]
    return dict(
        schema="ComposerMastermindComposition.v3",
        blueprint=dict(id=title, name=title, sections=sections, arcCurves=[dict(dimension="energy", points=arc_points)]),
        scenes=scenes,
        instances=[dict(id=i, name=i, midiChannel=n + 1, role="trigger", enabled=True, lastKnownScene="")
                   for n, i in enumerate(instance_ids)],
        motifPresets=[], modulatorTargets=[], modulationRoutes=[],
    )


# ======================================================================== independent verification (replays the FINISHED bundle)
def verify_bundle(bundle: dict, seq: Sequence, zone: Zone, cfg: PlanConfig, planned_active: list[list[int]]) -> list[str]:
    """Reads only the bundle: every non-empty section must be exactly 1 bar (a looping pattern would re-press every bar), pattern
    indices must rotate, then the presses are replayed through a fresh VoiceModel and compared with the planner's intent."""
    problems: list[str] = []
    bar_lengths = slice_bar_lengths(seq, zone, cfg.speed_percent, cfg.beats_per_bar)
    model = VoiceModel(bar_lengths, zone)
    n = len(bar_lengths)
    prev_pattern = None
    max_active = 0

    sections = bundle["blueprint"]["sections"]
    for i, sec in enumerate(sections):
        patterns = {e["patternIndex"] for e in sec["capturedContent"]}
        if len(patterns) != 1:
            problems.append(f"{sec['id']}: more than one pattern index in one section")
        pattern = next(iter(patterns))
        if i > 0 and pattern != (prev_pattern + 1) % 3:
            problems.append(f"{sec['id']}: pattern {pattern + 1} does not follow {prev_pattern + 1} in the P1->P2->P3 rotation")
        prev_pattern = pattern
        if i > 0 and sec["startBar"] != sections[i - 1]["startBar"] + sections[i - 1]["durationBars"]:
            problems.append(f"{sec['id']}: gap or overlap with the previous section")

        presses = []
        for entry in sec["capturedContent"]:
            for step_index, step in enumerate(entry["steps"]):
                if step["enabled"]:
                    presses.append((step_index, step["note"]))
        if presses and sec["durationBars"] != 1:
            problems.append(f"{sec['id']}: a section with trigger notes lasts {sec['durationBars']} bars, so they would fire every bar")

        t0 = float(sec["startBar"] - 1)
        for step_index, key in sorted(presses):
            t = t0 + step_index / PATTERN_STEPS
            if cfg.stop_key is not None and key == cfg.stop_key:
                model.stop_all(t)
            else:
                idx = key - zone.base_key
                if idx < 0 or idx >= n:
                    problems.append(f"{sec['id']}: key {key} is outside the slice map")
                    continue
                model.press(idx, t)
            max_active = max(max_active, len(model.active_set(t)))

        end = float(sec["startBar"] - 1 + sec["durationBars"])
        bar_after = sec["startBar"] + sec["durationBars"] - 1
        # compare with the planner's intent at the end of the last bar of this section
        if 0 <= bar_after - 1 < len(planned_active):
            now = model.active_set(end - EPS)
            if now != planned_active[bar_after - 1]:
                problems.append(f"{sec['id']}: replay has slices {now} sounding at the end of bar {bar_after}, planner intended "
                                f"{planned_active[bar_after - 1]}")

    final = model.active_set(float(cfg.bars))
    if final:
        problems.append(f"slices {final} are still sounding after the last bar")
    if max_active > cfg.max_voices:
        problems.append(f"{max_active} slices sound at once but MidiSampler is limited to {cfg.max_voices} voices")
    if sections and any(s["steps"] and any(x["enabled"] for x in s["steps"]) for s in sections[-1]["capturedContent"]):
        problems.append("the last section still fires triggers, so MPL would keep toggling after the blueprint ends")
    return problems


# ======================================================================== human-readable outputs
def key_name(key: int) -> str:
    names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    return f"{names[key % 12]}{key // 12 - 2}"           # the plugin's naming: middle C = C3, so 24 = C0, 23 = B-1


def write_timeline(path: str, cfg: PlanConfig, zone: Zone, events: list[Event], energy: list[float], active_after: list[list[int]],
                   bar_lengths: list[float]) -> None:
    by_bar: dict[int, list[Event]] = {}
    for ev in events:
        by_bar.setdefault(ev.bar, []).append(ev)
    lines = [f"Timeline: bundle bar N is HEARD in Bitwig bar N + {LAG_BARS} (play from bar 1; Bitwig bars 1-{LAG_BARS} are pre-roll).",
             f"Slices: {len(bar_lengths)} (key {zone.base_key} = slice 1, shown as {key_name(zone.base_key)}); "
             f"slice lengths in bars: {', '.join(f'{x:.2f}' for x in bar_lengths[:16])}{' ...' if len(bar_lengths) > 16 else ''}",
             "", f"{'bundle':>6} {'Bitwig':>6}  {'energy':>6}  events -> sounding slices", "-" * 78]
    for m in range(1, cfg.bars + 1):
        evs = by_bar.get(m, [])
        text = ", ".join(
            ("STOP-ALL" if ev.intent == "stopall" else f"{ev.intent.upper()} slice {ev.key - zone.base_key + 1}") + f" @step {ev.step}"
            for ev in evs) or "-"
        sounding = " ".join(str(i + 1) for i in active_after[m - 1]) or "(silence)"
        lines.append(f"{m:>6} {m + LAG_BARS:>6}  {energy[m - 1]:>6.2f}  {text:<46} -> {sounding}")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def write_setup(path: str, args, cfg: PlanConfig, zone: Zone, n_slices: int, source_name: str) -> None:
    keys_hi = zone.base_key + n_slices - 1
    text = f"""# MidiSampler / Bitwig setup for "{args.title}"

Generated by slice_score.py. The score assumes exactly this setup; a different setting changes slice lengths and breaks the plan.

## MidiSampler (zone 1 enabled, every other zone OFF)
- Load `{source_name}` into zone 1's source slot.
- Mode **Slice**, Slice by **{zone.slice_by}**{f', grid **{zone.grid_beats:g} beats**' if zone.slice_by == 'beat' else ''}{f', slices **{zone.count}**' if zone.slice_by == 'equal' else ''}{f', min slice **{zone.min_beats:g} beats**' if zone.slice_by == 'transient' else ''}, Slice playback **{'Thru' if zone.thru else 'Slice'}**.
- Range Start/End **{zone.start01 * 100:g} % - {zone.end01 * 100:g} %**.
- First slice key **{zone.base_key}** (shown as {key_name(zone.base_key)}); the {n_slices} slices use keys {zone.base_key}-{keys_hi} ({key_name(zone.base_key)}-{key_name(keys_hi)}).
- Sustain mode **{'Loop Forward' if zone.loop else 'Forward'}**, Playback **{'Trigger (latch)' if zone.playback == 'latch' else 'Start only'}**.
- Speed **{args.speed:g} %**, Sync to host on, zone speed ratio **{zone.ratio:g}**, Voices **{cfg.max_voices} or more**.
- Stop-All Key **{cfg.stop_key if cfg.stop_key is not None else 'Off'}**{f' ({key_name(cfg.stop_key)})' if cfg.stop_key is not None else ''}.

## MPL ({cfg.shooters} shooter instance{'s' if cfg.shooters > 1 else ''}: {', '.join(f'SHOOT{i + 1} = channel {i + 1}' for i in range(cfg.shooters))})
- Binary 16 grid, pattern length 16, Rate Normal, External Control ON, each on its own channel, output into MidiSampler.
- Do NOT leave a pattern looping from an earlier run: set MPL's pattern to Stopped (or start from a fresh project) before playing.

## Mastermind / Bitwig
- Load the score, Content mode **Absolute**. Host tempo about **{cfg.bpm:g} BPM** ({cfg.beats_per_bar}/4), start playback at **bar 1**.
- Everything is heard **{LAG_BARS} bars after the bundle's bar numbers** (measured). First sound: Bitwig bar {1 + LAG_BARS}.
- Piece length: {cfg.bars} bundle bars = Bitwig bars {1 + LAG_BARS}-{cfg.bars + LAG_BARS}. The final section is silent; stop the transport after it.
"""
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


# ======================================================================== CLI
def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--title", required=True)
    parser.add_argument("--source", required=True, help="the .mid file loaded into MidiSampler")
    parser.add_argument("--output", required=True, help="bundle JSON path (companion .setup.md and .timeline.txt are written next to it)")
    parser.add_argument("--minutes", type=float, default=2.0)
    parser.add_argument("--bars", type=int, default=0, help="override the length; default derived from --minutes/--bpm")
    parser.add_argument("--bpm", type=float, default=110.0)
    parser.add_argument("--beats-per-bar", type=int, default=4)
    parser.add_argument("--shape", choices=arc.SHAPES, default="arch_rise_fall")
    parser.add_argument("--restlessness", type=float, default=0.35)
    parser.add_argument("--seed", type=int, default=None)
    parser.add_argument("--strategy", choices=["order", "retro", "arch", "row", "random"], default="arch")
    parser.add_argument("--max-layers", type=int, default=3, help="most slices sounding at once")
    parser.add_argument("--voices", type=int, default=8, help="MidiSampler's Voices setting")
    parser.add_argument("--min-hold", type=int, default=2, help="fewest bars between changes (at peak energy)")
    parser.add_argument("--max-hold", type=int, default=6, help="most bars between changes (at low energy)")
    parser.add_argument("--swap-chance", type=float, default=0.5)
    parser.add_argument("--no-breaths", action="store_true", help="do not drop to silence when the energy curve is near zero")
    parser.add_argument("--shooters", type=int, default=1, help="number of MPL trigger instances")
    parser.add_argument("--tail-bars", type=int, default=2, help="silent bars at the end (lets the last slices ring out)")
    # MidiSampler zone mirror
    parser.add_argument("--slice-by", choices=["beat", "equal", "transient"], default="beat")
    parser.add_argument("--grid", type=float, default=4.0, help="beat grid slice length in beats")
    parser.add_argument("--count", type=int, default=8, help="slice count for --slice-by equal")
    parser.add_argument("--min-slice", type=float, default=0.25)
    parser.add_argument("--base-key", type=int, default=24)
    parser.add_argument("--thru", action="store_true")
    parser.add_argument("--sustain", choices=["loop", "forward"], default="loop")
    parser.add_argument("--playback", choices=["latch", "start_only"], default="latch")
    parser.add_argument("--ratio", type=float, default=1.0)
    parser.add_argument("--speed", type=float, default=100.0, help="MidiSampler global Speed, percent")
    parser.add_argument("--range-start", type=float, default=0.0)
    parser.add_argument("--range-end", type=float, default=1.0)
    parser.add_argument("--stop-key", type=int, default=23, help="-1 = none")
    return parser


def make_setup(args):
    """Turns parsed arguments into (source sequence, MidiSampler zone mirror, planner config, slice count)."""
    seq = read_midi_sequence(args.source)
    zone = Zone(start01=args.range_start, end01=args.range_end, slice_by=args.slice_by, grid_beats=args.grid, count=args.count,
                min_beats=args.min_slice, base_key=args.base_key, thru=args.thru, loop=args.sustain == "loop",
                playback=args.playback, ratio=args.ratio)
    boundaries = compute_slice_boundaries(seq, zone)
    n_slices = len(boundaries) - 1
    if zone.base_key + n_slices - 1 > 127:
        sys.exit(f"{n_slices} slices from key {zone.base_key} run past MIDI key 127; lower --base-key")
    stop_key = None if args.stop_key < 0 else args.stop_key
    if stop_key is not None and zone.base_key <= stop_key < zone.base_key + n_slices:
        sys.exit(f"--stop-key {stop_key} collides with the slice keys {zone.base_key}-{zone.base_key + n_slices - 1}")
    if zone.playback == "start_only" and zone.loop and stop_key is None:
        sys.exit("Start only + loop slices can never be stopped individually: give a --stop-key or use --sustain forward")

    seed = args.seed if args.seed is not None else random.randint(0, 2 ** 31 - 1)
    bars = args.bars if args.bars > 0 else max(8, int(round(args.minutes * args.bpm / args.beats_per_bar)))
    cfg = PlanConfig(bars=bars, bpm=args.bpm, beats_per_bar=args.beats_per_bar, shape=args.shape, strategy=args.strategy, seed=seed,
                     max_layers=max(1, min(args.max_layers, args.voices)), max_voices=args.voices, min_hold=max(1, args.min_hold),
                     max_hold=max(args.min_hold, args.max_hold), swap_chance=args.swap_chance, breaths=not args.no_breaths,
                     stop_key=stop_key, shooters=max(1, args.shooters), tail_bars=max(1, args.tail_bars),
                     restlessness=args.restlessness, speed_percent=args.speed)
    return seq, zone, cfg, n_slices


def generate(args) -> dict:
    """Plans, compiles, verifies and writes everything for parsed arguments. Returns a result dict (never prints, never exits), so the
    command line, IDLE and the desktop window (slice_score_gui.py) all share exactly the same code path.
    Raises SystemExit(message) for invalid settings, like make_setup does."""
    seq, zone, cfg, n_slices = make_setup(args)

    events, energy, active_after = plan_events(cfg, seq, zone)
    bundle = compile_bundle(args.title, cfg, zone, events, energy)
    problems = verify_bundle(bundle, seq, zone, cfg, active_after)

    out_dir = os.path.dirname(os.path.abspath(args.output))
    os.makedirs(out_dir, exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(bundle, f, indent=2)
    base = os.path.splitext(args.output)[0]
    bar_lengths = slice_bar_lengths(seq, zone, args.speed, args.beats_per_bar)
    write_timeline(base + ".timeline.txt", cfg, zone, events, energy, active_after, bar_lengths)
    write_setup(base + ".setup.md", args, cfg, zone, n_slices, os.path.basename(args.source))

    return dict(
        bundle_path=args.output, setup_path=base + ".setup.md", timeline_path=base + ".timeline.txt",
        sections=len(bundle["blueprint"]["sections"]), bars=cfg.bars, slices=n_slices, seed=cfg.seed,
        presses=sum(1 for ev in events if ev.intent != "stopall"), problems=problems,
        summary=(f"{len(bundle['blueprint']['sections'])} sections over {cfg.bars} bundle bars (heard in Bitwig bars "
                 f"{1 + LAG_BARS}-{cfg.bars + LAG_BARS}), {n_slices} slices, "
                 f"{sum(1 for ev in events if ev.intent != 'stopall')} trigger presses, seed={cfg.seed}"),
    )


def main(argv=None) -> None:
    if argv is None and len(sys.argv) == 1:                  # no options given (e.g. F5 in IDLE): use IDLE_SETTINGS
        here = os.path.dirname(os.path.abspath(__file__))
        argv = list(IDLE_SETTINGS)
        for flag in ("--source", "--output"):
            i = argv.index(flag) + 1
            argv[i] = os.path.normpath(os.path.join(here, argv[i]))
        print("No options given: using IDLE_SETTINGS from the top of slice_score.py", file=sys.stderr)
    args = build_parser().parse_args(argv)
    result = generate(args)

    print(f"Wrote '{result['bundle_path']}': {result['summary']}", file=sys.stderr)
    print(f"      plus {os.path.basename(result['setup_path'])} and {os.path.basename(result['timeline_path'])}", file=sys.stderr)
    if result["problems"]:
        print("\nVERIFICATION FAILED:", file=sys.stderr)
        for problem in result["problems"]:
            print("  - " + problem, file=sys.stderr)
        sys.exit(1)
    print("Verification passed: bundle replays to exactly the planned slice states, no hanging voices.", file=sys.stderr)


if __name__ == "__main__":
    main()
