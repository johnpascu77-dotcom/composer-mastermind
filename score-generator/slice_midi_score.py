#!/usr/bin/env python3
"""Writes a standard MIDI clip that "scores" a MidiSampler setup (docs/composing_by_slices_concept.md, plain-clip route).

The clip is played INTO MidiSampler (Bitwig: put it on the track before the plugin). Each note is a trigger and a transposer:
  - the octave of the note picks the ZONE (so the zone's slice), the key inside that octave picks the transposition;
  - a long note is a layer that sounds as long as it is held (Gate zones), loops included;
  - velocity is dynamics.
No MPL, no lag, no latch toggles: it is an ordinary piano-roll clip, editable in the DAW.

The setup comes straight from the plugin: use "Save Setup..." in MidiSampler and give the .json to --setup. It carries the zones, slice
numbers, key maps, voice limit, stop key AND the source notes, so nothing has to be copied by hand. The tool plans against a Python model of
the engine (midisampler_model.py, cross-checked against the real C++ engine) and then verifies the finished clip twice: through the model,
and through the real engine (MidiSamplerReplay.exe) when it is available.

Musical plan, driven by the same energy shapes as the other generators:
  * energy sets how many layers sound at once (--max-layers), how long they are held before the set changes (--min-hold/--max-hold bars),
    and the velocity; quiet stretches can drop to silence;
  * which zone/slice comes next: --strategy order | retro | arch (follows the energy) | random;
  * which key inside the zone (= which transposition): a small melodic walk that prefers neighbouring keys and leaps more as energy rises;
    --allowed-transpositions restricts the sounding transpositions (semitones mod 12) and --avoid-clashes keeps new layers away from minor
    seconds against what already sounds;
  * short ACCENT notes on the 16th grid ornament the layers (--accent-rate), only on zones whose voices end by themselves or when released.
Start only + loop zones cannot be released one by one, so for those the layer set changes by a stop-all (the setup's Stop key) followed by
a fresh set ("phrases"); that needs a Stop key in the setup.

Usage (or run slice_midi_gui.py / run_slice_midi.bat for a window):
    python slice_midi_score.py --setup "MidiSampler setup.json" --output clip.mid --minutes 2 --shape arch_rise_fall --seed 7
"""

from __future__ import annotations

import argparse
import math
import os
import random
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field

import arc
import midisampler_model as mm
import slice_score as ss

PPQ = 480
DEFAULT_EXE = r"C:\AudioDev\Repos\MidiSampler\build\Release\MidiSamplerReplay.exe"
EPS = 1e-9


# ======================================================================================== targets and notes
@dataclass
class Target:
    key: int
    zones: list                 # zone indices whose voices this key starts
    transpose: int              # semitones the first zone's voice sounds above/below the source
    slice_label: str            # for the report ("#16", "key 24", "whole")
    voices: int                 # how many voices one press starts


@dataclass
class Note:
    start: float
    key: int
    velocity: int
    end: float = math.inf
    kind: str = "layer"         # layer | accent | stop
    zone: int = -1


@dataclass
class Plan:
    notes: list
    energy: list
    bars: int
    log: list                   # (bar, text) lines for the report
    steals: int
    skipped: int


def collect_targets(cfg: mm.SetupConfig, model: mm.EngineModel, zone_filter, max_degrees: int) -> dict:
    """zone index -> list of Targets (the keys a clip may use for that zone)."""
    out = {}
    for z in model.zones:                                                  # the zones of the stage the host's notes go to
        if not z.enabled or (zone_filter and (z.index + 1) not in zone_filter):
            continue
        keys = []
        if z.play_mode == 1:                                               # a key per slice: keys are the slices
            keys = list(range(z.slice_base_key, min(127, z.slice_base_key + 64)))
        else:
            keys = list(range(z.key_lo, min(z.key_hi, z.key_lo + max_degrees - 1) + 1))
        targets = []
        for key in keys:
            if not (z.key_lo <= key <= z.key_hi) or not model._zone_matches(z, key):
                continue
            info = model.start_info(z, key)
            if info is None:
                continue
            label = f"#{z.slice_index + 1}" if z.play_mode == 2 else (f"slice {key - z.slice_base_key + 1}" if z.play_mode == 1 else "whole")
            n_voices = sum(1 for other in model.zones if model._zone_matches(other, key) and model.start_info(other, key) is not None)
            targets.append(Target(key, [o.index for o in model.zones if model._zone_matches(o, key)], info[1], label, n_voices))
        if targets:
            out[z.index] = targets
    return out


# ======================================================================================== planner
class Planner:
    def __init__(self, cfg: mm.SetupConfig, args, rng: random.Random):
        self.cfg, self.args, self.rng = cfg, args, rng
        self.model = mm.EngineModel(cfg)
        self.cond = cfg.stages[cfg.host_stage()]
        self.tick = max(args.grid_beats, self.cond.grid_beats)      # a stage-1 Grid delays notes that are off its lines: stay on them
        self.bpb = args.beats_per_bar
        self.cap = cfg.voices
        zone_filter = {int(x) for x in args.zones.split(",")} if args.zones else None
        self.targets = collect_targets(cfg, self.model, zone_filter, args.max_degrees)
        if not self.targets:
            raise SystemExit("No usable zones: enable at least one zone with a loaded source in the setup (and check --zones).")

        for z in self.model.zones:
            if z.enabled and z.index in self.targets and z.playback == "latch":
                raise SystemExit(f"Zone {z.index + 1} is set to Trigger (latch): a latch zone toggles on every press, which a clip cannot plan "
                                 f"safely. Set it to Gate or Start only in MidiSampler and save the setup again.")
        # zone classes
        self.gate_zones = [i for i in self.targets if cfg.zones[i].playback == "gate"]
        self.loop_start_only = [i for i in self.targets if cfg.zones[i].playback == "start_only" and mm.is_looping(cfg.zones[i].mode)]
        self.oneshot_zones = [i for i in self.targets if cfg.zones[i].playback == "start_only" and not mm.is_looping(cfg.zones[i].mode)]
        self.reset_mode = bool(self.loop_start_only)
        if self.reset_mode and cfg.stop_key < 0:
            raise SystemExit("A Start only + loop zone can never be released one by one, so the clip would pile up voices. Set a Stop key in "
                             "MidiSampler (Stop key B-1 is a good one) and save the setup again, or switch those zones to Gate.")
        if cfg.stop_key >= 0 and any(cfg.zones[i].key_lo <= cfg.stop_key <= cfg.zones[i].key_hi for i in self.targets):
            self.stop_collides = True
        else:
            self.stop_collides = False
        self.layer_zones = sorted(set(self.gate_zones) | set(self.loop_start_only))
        if not self.layer_zones:
            raise SystemExit("Every enabled zone ends by itself (Start only, no loop): there is nothing to hold as a layer. Use Gate or looping zones.")
        self.accent_zones = sorted(set(self.gate_zones) | set(self.oneshot_zones))

        # state
        self.layers: list[Note] = []                       # notes of layers we may still release
        self.notes: list[Note] = []
        self.pending: dict[int, list] = {}
        self.log: list = []
        self.steals = 0
        self.skipped = 0
        self.zone_strategy = ss.Strategy(args.strategy, len(self.layer_zones), rng)
        self.last_degree: dict[int, int] = {}              # zone -> index into its target list

    # ---- helpers
    def _alive_layers(self, t):
        self.layers = [n for n in self.layers if self._note_alive(n, t)]
        return self.layers

    def _note_alive(self, n: Note, t: float) -> bool:
        if n.end != math.inf:
            return False
        return any(v.key == n.key and v.start <= n.start + EPS and v.end > t + EPS for v in self.model.voices)

    def _sounding_transpositions(self, t, exclude_zone=None):
        out = []
        for v in self.model.voices:
            if v.start <= t + EPS and v.end > t + EPS and v.zone != exclude_zone:
                out.append(v.transpose % 12)
        return out

    def _allowed(self, transpose: int) -> bool:
        allowed = self.args.allowed
        return allowed is None or (transpose % 12) in allowed

    def _press(self, t: float, target: Target, velocity: int, kind: str, zone: int) -> Note | None:
        """Press a key now if the voice limit allows it; returns the Note (not yet released) or None when skipped."""
        if self.model.active_count(t) + target.voices > self.cap:
            self.skipped += 1
            return None
        before = len(self.model.voices)
        self.model.press(target.key, t)
        note = Note(t, target.key, int(max(1, min(127, velocity))), kind=kind, zone=zone)
        self.notes.append(note)
        return note

    def _release(self, note: Note, t: float) -> None:
        note.end = max(t, note.start + self.tick)
        self.model.release(note.key, note.end)

    def _choose_zone(self, e: float, pool: list[int], avoid: set) -> int:
        order = [i for i in pool if i not in avoid] or list(pool)
        if len(order) == 1:
            return order[0]
        # the strategy walks over the layer zones; for pools of other kinds fall back to a seeded pick in the same spirit
        if pool is self.layer_zones or pool == self.layer_zones:
            for _ in range(len(self.layer_zones) * 2):
                idx = self.zone_strategy.next_slice(e, set(range(len(self.layer_zones))) - {self.layer_zones.index(i) for i in order})
                if idx is not None:
                    return self.layer_zones[idx]
        return self.rng.choice(order)

    def _choose_target(self, zone: int, e: float, t: float, held_keys: set) -> Target | None:
        cands = [(i, tg) for i, tg in enumerate(self.targets[zone]) if tg.key not in held_keys and self._allowed(tg.transpose)]
        if self.args.avoid_clashes:
            others = self._sounding_transpositions(t, exclude_zone=zone)
            ok = [(i, tg) for i, tg in cands if all(min((tg.transpose - o) % 12, (o - tg.transpose) % 12) > 1 for o in others)]
            cands = ok or cands
        if not cands:
            return None
        prev = self.last_degree.get(zone)
        spread = 1.0 + 4.0 * e
        weights = [1.0 if prev is None else math.exp(-abs(i - prev) / spread) for i, _ in cands]
        pick = self.rng.choices(cands, weights=weights, k=1)[0]
        self.last_degree[zone] = pick[0]
        return pick[1]

    def _velocity(self, e: float, accent: bool = False) -> int:
        lo, hi = self.args.velocity_range
        base = lo + (hi - lo) * (0.15 + 0.85 * e) + (10 if accent else 0)
        return int(round(base + self.rng.uniform(-6, 6)))

    # ---- planning
    def run(self) -> Plan:
        a = self.args
        bars = a.bars
        play_bars = max(1, bars - a.tail_bars)
        energy = []
        for m in range(bars):
            if m >= play_bars:
                energy.append(0.0)
            else:
                t01 = (m + 0.5) / play_bars
                energy.append(max(0.0, min(1.0, arc.sample_shape(a.shape, t01) + self.rng.uniform(-1, 1) * a.restlessness * 0.15)))
        total_ticks = int(round(bars * self.bpb / self.tick))
        play_end_tick = int(round(play_bars * self.bpb / self.tick))
        ticks_per_bar = int(round(self.bpb / self.tick))
        next_decision = 0
        stop_key = self.cfg.stop_key

        for i in range(total_ticks):
            t = i * self.tick
            bar = i // ticks_per_bar
            e = energy[bar] if bar < bars else 0.0

            if i % ticks_per_bar == 0 and bar >= next_decision and i <= play_end_tick:
                hold = int(round(a.max_hold + (a.min_hold - a.max_hold) * e))
                next_decision = bar + max(1, hold)
                self._decide(i, bar, e, bar >= play_bars - 1 and i >= play_end_tick - ticks_per_bar, ticks_per_bar)

            # actions scheduled for this tick: releases first, then stop-all, then presses
            for kind, payload in sorted(self.pending.pop(i, []), key=lambda x: {"release": 0, "stop": 1, "layer": 2, "accent": 3}[x[0]]):
                if kind == "release":
                    if payload.end == math.inf and self._note_alive(payload, t):
                        self._release(payload, t)
                elif kind == "stop":
                    self._stop_all(t)
                elif kind == "layer":
                    tg = self._pick_layer_target(e, t)
                    if tg is not None:
                        note = self._press(t, tg, self._velocity(e), "layer", tg.zones[0])
                        if note is not None:
                            self.layers.append(note)
                            self.log.append((bar, f"+ layer Z{tg.zones[0] + 1} {tg.slice_label} key {tg.key} ({tg.transpose:+d} st)"))
                elif kind == "accent":
                    self._accent(i, t, e, bar, play_end_tick)

            # spontaneous accents on the grid
            if self.accent_zones and i < play_end_tick - 2 and a.accent_rate > 0 and e > 0.05:
                if self.rng.random() < a.accent_rate * e * self.tick:
                    self.pending.setdefault(i, []).append(("accent", None))
                    # processed on the next pass of this tick would be too late: handle immediately
                    self.pending[i].pop()
                    self._accent(i, t, e, bar, play_end_tick)

        # end of piece: everything that is still held is released, loops are cut
        end_t = play_end_tick * self.tick
        for n in list(self.layers):
            if n.end == math.inf and self._note_alive(n, end_t):
                self._release(n, end_t)
        if self.reset_mode or self.model.active_count(end_t + EPS) > 0 or len(self.cfg.active_stages()) > 1:
            if stop_key >= 0 and (self.model.active_count(end_t + EPS) > 0 or len(self.cfg.active_stages()) > 1):
                self._stop_all(end_t, final=True)
        return Plan(self.notes, energy, bars, self.log, self.steals, self.skipped)

    def _stop_all(self, t: float, final: bool = False) -> None:
        key = self.cfg.stop_key
        note = Note(t, key, 100, kind="stop")
        note.end = t + self.tick
        self.notes.append(note)
        self.model.press(key, t)
        for n in self.layers:
            if n.end == math.inf:
                n.end = max(t, n.start + self.tick)
        self.layers = []
        self.log.append((int(t // self.bpb), "STOP-ALL" + (" (end)" if final else "")))

    def _pick_layer_target(self, e: float, t: float):
        held = {n.key for n in self.layers if n.end == math.inf}
        alive_zones = {v.zone for v in self.model.voices if v.start <= t + EPS and v.end > t + EPS}
        zone = self._choose_zone(e, self.layer_zones, alive_zones)
        tg = self._choose_target(zone, e, t, held)
        if tg is None:
            for other in self.layer_zones:
                tg = self._choose_target(other, e, t, held)
                if tg is not None:
                    break
        return tg

    def _accent(self, i: int, t: float, e: float, bar: int, play_end_tick: int) -> None:
        pool = self.accent_zones
        held = {n.key for n in self.notes if n.end == math.inf}
        zone = self.rng.choice(pool)
        tg = self._choose_target(zone, e, t, held)
        if tg is None:
            return
        length_ticks = self.rng.choice([1, 1, 2, 3, 4])
        end_tick = min(i + length_ticks, play_end_tick - 1)
        note = self._press(t, tg, self._velocity(e, accent=True), "accent", zone)
        if note is None:
            return
        self.pending.setdefault(end_tick, []).append(("release", note))
        self.log.append((bar, f"  accent Z{zone + 1} {tg.slice_label} key {tg.key} ({tg.transpose:+d} st) {length_ticks}/16"))

    def _decide(self, i: int, bar: int, e: float, is_last: bool, ticks_per_bar: int) -> None:
        a, t = self.args, i * self.tick
        in_tail = bar >= max(1, a.bars - a.tail_bars)
        if is_last or in_tail:
            target = 0
        elif a.breaths and e < 0.08 and bar > 1:
            target = 0
        else:
            target = max(1, min(a.max_layers, self.cap - a.reserve_voices, int(round(1 + e * (a.max_layers - 1)))))
        alive = self._alive_layers(t)
        offs = lambda: self.rng.choice([0, 0, 0, 2, 4, 6, 8]) if a.stagger else 0           # in grid ticks, a little stagger within the bar

        if self.reset_mode:
            reshuffle = bool(alive) and (len(alive) > target or (target > 0 and self.rng.random() < a.swap_chance * (0.4 + e)))
            start_from = i
            if reshuffle:
                self.pending.setdefault(i, []).append(("stop", None))
                start_from = i + 1
                alive = []
            for k in range(max(0, target - len(alive))):
                self.pending.setdefault(min(start_from + offs(), i + ticks_per_bar - 2), []).append(("layer", None))
            return

        released = 0
        while len(alive) - released > target:
            victim = alive[released]
            released += 1
            self.pending.setdefault(i + offs(), []).append(("release", victim))
        n_new = max(0, target - (len(alive) - released))
        if (n_new == 0 and target > 0 and alive and self.rng.random() < a.swap_chance * (0.4 + e)
                and len(alive) - released > 0):
            victim = alive[released]                                                          # swap: one out, one in
            self.pending.setdefault(i + offs(), []).append(("release", victim))
            n_new = 1
        for _ in range(n_new):
            self.pending.setdefault(i + offs(), []).append(("layer", None))


# ======================================================================================== MIDI file
def vlq(value: int) -> bytes:
    out = [value & 0x7F]
    value >>= 7
    while value:
        out.append((value & 0x7F) | 0x80)
        value >>= 7
    return bytes(reversed(out))


def finalize_ends(notes: list, model: mm.EngineModel, piece_end: float) -> None:
    """Start only notes have no release of their own: give them the time their voice really ends (cosmetic length for the piano roll)."""
    for n in notes:
        if n.end == math.inf:
            ends = [v.end for v in model.voices if v.key == n.key and abs(v.start - n.start) < 1e-6 and v.end != math.inf]
            n.end = max(ends) if ends else piece_end
            n.end = max(n.end, n.start + 0.25)


def write_midi(path: str, notes: list, bpm: float, bars: int, bpb: int, title: str) -> None:
    events = []                                                      # (tick, order, bytes)
    for n in notes:
        on = int(round(n.start * PPQ))
        off = max(on + 1, int(round(n.end * PPQ)))
        order_on = 0 if n.kind == "stop" else 2                      # a stop-all press sorts before layer presses at the same tick
        events.append((on, order_on, bytes([0x90, n.key, n.velocity])))
        events.append((off, 1, bytes([0x80, n.key, 0])))
    events.sort(key=lambda e: (e[0], e[1]))
    track = bytearray()
    name = title.encode("utf-8")[:60]
    track += vlq(0) + bytes([0xFF, 0x03, len(name)]) + name
    track += vlq(0) + bytes([0xFF, 0x51, 0x03]) + int(round(60_000_000 / bpm)).to_bytes(3, "big")
    track += vlq(0) + bytes([0xFF, 0x58, 0x04, bpb, 2, 24, 8])
    last = 0
    for tick, _, data in events:
        track += vlq(tick - last) + data
        last = tick
    end = max(last, int(round(bars * bpb * PPQ)))
    track += vlq(end - last) + bytes([0xFF, 0x2F, 0x00])
    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, PPQ))
        f.write(b"MTrk" + struct.pack(">I", len(track)) + bytes(track))


def read_clip(path: str) -> list:
    """(start_beat, key, end_beat) for every note of a clip, via slice_score's SMF reader."""
    seq = ss.read_midi_sequence(path)
    return [(n.start, n.pitch, n.end) for n in seq.notes]


# ======================================================================================== verification
def verify(cfg: mm.SetupConfig, clip_path: str, total_beats: float, exe: str | None, tmp: str) -> tuple[list, list]:
    """Replays the finished FILE through a fresh model and (when available) through the real engine; returns (problems, info lines)."""
    problems, info = [], []
    notes = read_clip(clip_path)
    events = []
    for start, key, end in notes:
        events.append((start, True, key, 100))
        events.append((end, False, key, 0))
    events.sort(key=lambda e: (e[0], e[1], 0 if e[2] == cfg.stop_key else 1))

    model = mm.EngineModel(cfg)
    tail = 8.0
    if len(cfg.active_stages()) > 1:                         # later stages may ring on after the clip: wait for their longest natural ending
        longest = max((cfg.sources[z.source].length_beats / max(0.01, cfg.speed / 100.0 * z.ratio) + z.delay
                       for z in cfg.zones if z.enabled and z.source in cfg.sources), default=0.0)
        tail += longest + 4.0
    checks = [(k + 0.5) * 0.25 for k in range(int((total_beats + tail) / 0.25))]
    expected = []
    i = 0
    max_voices = 0
    for c in checks:
        while i < len(events) and events[i][0] <= c:
            t, on, key, _ = events[i]
            (model.press if on else model.release)(key, t)
            i += 1
        expected.append(model.active_keys(c))
        max_voices = max(max_voices, model.active_count(c))
    if expected[-1]:
        problems.append(f"voices are still sounding after the end of the clip: keys {expected[-1]}")
    unmapped = sorted({k for _, k, _ in notes if not model.key_is_mapped(k)})
    if unmapped:
        problems.append(f"the clip uses keys no zone answers to: {unmapped}")
    info.append(f"model replay of the file: up to {max_voices} voices at once (limit {cfg.voices}), nothing left sounding at the end")

    if exe and os.path.exists(exe):
        import midi_score_crosscheck as cc
        replay = os.path.join(tmp, "verify_replay.txt")
        cc.write_replay2(replay, cfg, events, checks)
        result = subprocess.run([exe, replay], capture_output=True, text=True)
        if result.returncode != 0:
            problems.append("the real-engine replay failed: " + result.stderr.strip())
        else:
            lines = result.stdout.strip().splitlines()
            real = [[int(x) for x in ln.split()[1:]] for ln in lines if not ln.startswith(("END", "FIRSTPITCH", "MAXV", "OUT"))]
            end_line = next((ln for ln in lines if ln.startswith("END")), "END ? ?")
            mism = 0
            model2 = mm.EngineModel(cfg)
            j = 0
            for k, c in enumerate(checks):
                while j < len(events) and events[j][0] <= c:
                    t, on, key, _ = events[j]
                    (model2.press if on else model2.release)(key, t)
                    j += 1
                keys = model2.active_keys(c)
                if keys != real[k] and not ((set(keys) ^ set(real[k])) <= model2.ambiguous_keys(c, 0.02)):
                    mism += 1
            outstanding = end_line.split()
            if mism:
                problems.append(f"the real engine disagrees with the model at {mism} of {len(checks)} checkpoints")
            if len(outstanding) >= 3 and (outstanding[1] != "0" or outstanding[2] != "0"):
                problems.append(f"the real engine still has {outstanding[1]} notes / {outstanding[2]} voices sounding at the end")
            if not mism:
                info.append(f"REAL ENGINE replay of the file: sounding keys agree at all {len(checks)} checkpoints, nothing left sounding"
                            + (" (stage 1 compared; later stages checked below)" if len(cfg.active_stages()) > 1 else ""))
            cloud = cloud_stats(cfg, events, checks, exe, tmp)
            if cloud is not None:
                info.extend(cloud[0])
                problems.extend(cloud[1])
    else:
        info.append("real-engine replay skipped (MidiSamplerReplay.exe not found)")
    return problems, info


def cloud_stats(cfg: mm.SetupConfig, events: list, checks: list, exe: str, tmp: str):
    """Replays the clip through the real cascade and describes what actually leaves the plugin. Returns (info lines, problems)."""
    import midi_score_crosscheck as cc
    replay = os.path.join(tmp, "cloud_replay.txt")
    cc.write_replay2(replay, cfg, events, checks, dump=True, all_stage_keys=True)
    result = subprocess.run([exe, replay], capture_output=True, text=True)
    if result.returncode != 0:
        return [], ["the cascade replay failed: " + result.stderr.strip()]
    ons, offs = [], []
    maxv = [0, 0, 0]
    end = None
    for ln in result.stdout.splitlines():
        parts = ln.split()
        if not parts:
            continue
        if parts[0] == "OUT":
            beat, on, _ch, pitch = float(parts[1]), int(parts[2]), int(parts[3]), int(parts[4])
            (ons if on else offs).append((beat, pitch))
        elif parts[0] == "MAXV":
            maxv = [int(x) for x in parts[1:4]]
        elif parts[0] == "END":
            end = (int(parts[1]), int(parts[2]))
    info, problems = [], []
    if end is not None and (end[0] != 0 or end[1] != 0):
        problems.append(f"after the clip the plugin still holds {end[0]} output notes / {end[1]} voices (a looping zone on a later stage in Start only mode can only be "
                        f"ended by the Stop key; use Gate there, or set a Stop key)")
    stages = cfg.active_stages()
    if len(stages) > 1 or not cfg.stages[stages[0]].is_neutral() or any(z.enabled and z.note_chance < 100.0 for z in cfg.zones):
        if ons:
            pitches = [p for _, p in ons]
            span = max(b for b, _ in ons) - min(b for b, _ in ons)
            bars = max(1.0, span / 4.0)
            events_sorted = sorted([(b, 1) for b, _ in ons] + [(b, -1) for b, _ in offs], key=lambda e: (e[0], e[1]))
            sounding = peak = 0
            for _, d in events_sorted:
                sounding += d
                peak = max(peak, sounding)
            info.append(f"what leaves the plugin (real engine): {len(ons)} notes, pitches {min(pitches)}-{max(pitches)}, about {len(ons) / bars:.0f} notes per bar, "
                        f"at most {peak} sounding at once; most voices busy per stage: " + ", ".join(f"stage {k + 1}: {maxv[k]}" for k in stages))
        else:
            info.append("what leaves the plugin (real engine): nothing (every trigger was dropped by the conditioning, or no zone answers)")
    return info, problems


# ======================================================================================== reporting
def write_report(path: str, args, cfg: mm.SetupConfig, plan: Plan, planner: Planner, info: list, problems: list, setup_path: str) -> None:
    lines = [f"Clip for MidiSampler setup: {os.path.basename(setup_path)}", "",
             f"{plan.bars} bars of {args.beats_per_bar}/4 at {args.bpm:g} BPM (tempo only labels the file), seed {args.seed}, shape {args.shape}, strategy {args.strategy}",
             f"Voice limit in the setup: {cfg.voices}   Stop key: {cfg.stop_key if cfg.stop_key >= 0 else 'off'}   "
             f"Phrase mode (start-only loops): {'yes' if planner.reset_mode else 'no'}", ""]
    cond = planner.cond
    if cfg.cascade and len(cfg.active_stages()) > 1:
        lines.append(f"Cascade ON: the clip drives stage {cfg.host_stage() + 1}; later stages are played by what it emits.")
        for k in cfg.active_stages():
            names = ", ".join(f"Z{z.index + 1}" for z in cfg.zones if z.enabled and cfg.zone_stage(z) == k)
            sg = cfg.stages[k]
            how = "host notes" if k == cfg.host_stage() else ("one trigger per voice" if sg.triggers else "every note") + " of the previous stage"
            lines.append(f"  stage {k + 1}: {names}  (input: {how}{', tapped to the output' if sg.tap else ''})")
        lines.append("")
    if not cond.is_neutral():
        bits = []
        if cond.pass_pct < 100.0: bits.append(f"{cond.pass_pct:g} % of the notes pass (seed {cond.seed}; the clip is planned with the same thinning)")
        if cond.grid_beats > 0: bits.append(f"notes wait for a {cond.grid_beats:g}-beat grid (the clip is written on it)")
        if cond.range_lo > 0 or cond.range_hi < 127: bits.append(f"range {cond.range_lo}-{cond.range_hi}")
        if cond.snap: bits.append(f"snap to pitch classes {[i for i in range(12) if cond.snap_mask >> i & 1]}")
        lines += ["Input conditioning of the host's notes (stage " + str(cfg.host_stage() + 1) + "): " + "; ".join(bits), ""]
    lines.append("Zones used:")
    for zi in sorted(planner.targets):
        z = cfg.zones[zi]
        kinds = {1: "slice per key", 2: f"slice zone, slice {z.slice_index + 1}", 0: "whole source"}[z.play_mode]
        lines.append(f"  Z{zi + 1}: keys {z.key_lo}-{z.key_hi}  {kinds}  {z.playback}  {z.mode}  ratio {z.ratio:g}  {len(planner.targets[zi])} keys usable")
    lines += ["", "Setup the clip assumes (check these in MidiSampler before playing it):",
              f"  - {cfg.source_names.get(0, 'the source')} loaded, the zones exactly as in the saved setup, Voices = {cfg.voices}, Speed {cfg.speed:g} %",
              "  - play the clip INTO MidiSampler (clip on the track before the plugin), host tempo anything: durations are in beats", ""]
    if cfg.approximations:
        lines += ["Notes on accuracy:"] + [f"  - {a}" for a in cfg.approximations] + [""]
    lines += ["Checks:"] + [f"  OK  {x}" for x in info] + [f"  PROBLEM  {p}" for p in problems] + [""]
    lines.append(f"{sum(1 for n in plan.notes if n.kind == 'layer')} layer notes, {sum(1 for n in plan.notes if n.kind == 'accent')} accent notes, "
                 f"{sum(1 for n in plan.notes if n.kind == 'stop')} stop-all presses, {plan.skipped} presses skipped (voice limit)")
    lines += ["", f"{'bar':>4} {'energy':>6}  what happens"]
    by_bar: dict = {}
    for bar, text in plan.log:
        by_bar.setdefault(bar, []).append(text)
    for bar in range(plan.bars):
        lines.append(f"{bar + 1:>4} {plan.energy[bar]:>6.2f}  " + ("; ".join(by_bar.get(bar, [])) or "-"))
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


# ======================================================================================== CLI
def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--setup", required=True, help="a MidiSampler setup (.json from Save Setup...)")
    p.add_argument("--output", required=True, help="the .mid clip to write (a .report.txt is written next to it)")
    p.add_argument("--minutes", type=float, default=2.0)
    p.add_argument("--bars", type=int, default=0, help="exact length in bars (overrides --minutes)")
    p.add_argument("--bpm", type=float, default=100.0, help="tempo written into the file (and used to turn --minutes into bars)")
    p.add_argument("--beats-per-bar", type=int, default=4)
    p.add_argument("--grid-beats", type=float, default=0.25, help="every note starts and ends on this grid, in beats (0.25 = 16ths)")
    p.add_argument("--shape", choices=arc.SHAPES, default="arch_rise_fall")
    p.add_argument("--restlessness", type=float, default=0.35)
    p.add_argument("--seed", type=int, default=None)
    p.add_argument("--strategy", choices=["order", "retro", "arch", "random"], default="arch")
    p.add_argument("--max-layers", type=int, default=3, help="most long notes sounding at once")
    p.add_argument("--reserve-voices", type=int, default=1, help="voices kept free for accents")
    p.add_argument("--min-hold", type=int, default=2)
    p.add_argument("--max-hold", type=int, default=6)
    p.add_argument("--swap-chance", type=float, default=0.5)
    p.add_argument("--accent-rate", type=float, default=1.0, help="short ornament notes per beat at full energy (0 = none)")
    p.add_argument("--no-breaths", dest="breaths", action="store_false", help="do not drop to silence when the energy is near zero")
    p.add_argument("--no-stagger", dest="stagger", action="store_false", help="start all layers exactly on the bar line")
    p.add_argument("--tail-bars", type=int, default=2)
    p.add_argument("--zones", default="", help="only use these zones, e.g. 1,2,4 (default: every enabled zone)")
    p.add_argument("--max-degrees", type=int, default=12, help="keys per zone the clip may use")
    p.add_argument("--allowed-transpositions", default="", help="semitones mod 12 the sounding transpositions may have, e.g. 0,3,7")
    p.add_argument("--avoid-clashes", action="store_true", help="keep new layers a minor second away from the transpositions already sounding")
    p.add_argument("--velocity-range", default="50,118", help="lowest,highest velocity")
    p.add_argument("--exe", default=DEFAULT_EXE, help="MidiSamplerReplay.exe for the real-engine check (skipped if missing)")
    return p


def generate(args) -> dict:
    cfg = mm.load_setup(args.setup)
    args.seed = args.seed if args.seed is not None else random.randint(0, 2 ** 31 - 1)
    args.bars = args.bars if args.bars > 0 else max(8, int(round(args.minutes * args.bpm / args.beats_per_bar)))
    args.allowed = {int(x) % 12 for x in args.allowed_transpositions.split(",") if x.strip() != ""} or None
    lo, hi = (int(x) for x in args.velocity_range.split(","))
    args.velocity_range = (max(1, lo), min(127, hi))
    rng = random.Random(args.seed)

    planner = Planner(cfg, args, rng)
    plan = planner.run()
    piece_end = args.bars * args.beats_per_bar
    finalize_ends(plan.notes, planner.model, piece_end)

    out = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    write_midi(out, plan.notes, args.bpm, args.bars, args.beats_per_bar, os.path.splitext(os.path.basename(out))[0])
    with tempfile.TemporaryDirectory() as tmp:
        problems, info = verify(cfg, out, piece_end, args.exe, tmp)
    if planner.steals:
        problems.append(f"{planner.steals} voices were stolen")
    report = os.path.splitext(out)[0] + ".report.txt"
    write_report(report, args, cfg, plan, planner, info, problems, args.setup)
    return dict(clip=out, report=report, notes=len(plan.notes), bars=args.bars, seed=args.seed, problems=problems, info=info,
                summary=f"{len(plan.notes)} notes over {args.bars} bars, seed {args.seed}")


def main(argv=None) -> None:
    args = build_parser().parse_args(argv)
    try:
        result = generate(args)
    except SystemExit as exc:
        sys.exit(f"Cannot generate: {exc}" if isinstance(exc.code, str) else exc.code)
    print(f"Wrote '{result['clip']}': {result['summary']}", file=sys.stderr)
    print(f"      plus {os.path.basename(result['report'])}", file=sys.stderr)
    for line in result["info"]:
        print("  OK  " + line, file=sys.stderr)
    if result["problems"]:
        print("\nVERIFICATION FAILED:", file=sys.stderr)
        for p in result["problems"]:
            print("  - " + p, file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
