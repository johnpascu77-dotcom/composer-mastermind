"""Python model of MidiSampler, built from the plugin's own "Save Setup" JSON.

Reads a setup file (zones, slice settings, key maps, voice limit, stop key and the embedded source notes) and simulates what the real
engine does with a stream of note-ons / note-offs: which voices start, when they end by themselves, Gate / Latch / Start only,
stop-all key, same-key restarts and oldest-voice stealing. Mirrors Source/SamplerEngine.h faithfully (the same convention the rest of
this folder uses for the C++ it mirrors); score-generator/midi_score_crosscheck.py proves it against the real engine.

Not modelled (reported through SetupConfig.approximations): the rubato time curve changes WHEN a voice reaches its natural end, so
with a time curve on a zone, natural endings are approximate (gate / loop behaviour is exact).
"""

from __future__ import annotations

import json
import math
from dataclasses import dataclass, field

import slice_score as ss

MODES = ["forward", "reverse", "loop_forward", "loop_bidir", "loop_reverse", "loop_bidir_reverse"]
PLAYBACKS = ["gate", "latch", "start_only"]
SLICE_BYS = ["beat", "equal", "transient", "manual"]
GRIDS = [0.25, 0.5, 1.0, 2.0, 4.0]
RATIOS = [0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 0.75, 1.0, 4.0 / 3.0, 1.5, 2.0, 3.0, 4.0]
CYCLES = [0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0]
EPS = 1e-9


def is_looping(mode: str) -> bool:
    return mode.startswith("loop")


def starts_reversed(mode: str) -> bool:
    return mode in ("reverse", "loop_reverse", "loop_bidir_reverse")


@dataclass
class ZoneCfg:
    index: int = 0
    enabled: bool = False
    key_lo: int = 0
    key_hi: int = 127
    source: int = 0
    root: int = 60
    start01: float = 0.0
    end01: float = 1.0
    loop_start01: float = 0.0
    loop_end01: float = 1.0
    mode: str = "forward"
    playback: str = "gate"
    play_mode: int = 0                 # 0 sample, 1 slice (a key per slice), 2 slice zone (one slice, keys transpose)
    slice_by: str = "beat"
    grid: float = 1.0
    count: int = 8
    min_beats: float = 0.25
    slice_thru: bool = False
    slice_base_key: int = 24
    manual: list = field(default_factory=list)
    out_channel: int = 0
    invert: bool = False
    axis: int = 60
    ratio: float = 1.0
    key_map_set: bool = False
    key_step: int = 1
    pc_mask: int = 0xFFF
    delay: float = 0.0
    phase: float = 0.0
    warp_curve: int = 0
    warp_depth: float = 0.0
    warp_cycle: float = 4.0
    warp_phase: float = 0.0
    slice_index: int = 0
    base_shift: int = 0
    stage: int = 0                     # cascade stage (0-based); only used when the setup's cascade is on
    note_chance: float = 100.0         # per-zone probability that an emitted note sounds (seeded). Voices keep their keys either way, so the model ignores it
    chance_seed: int = 1

    def as_slice_zone(self) -> ss.Zone:
        """The subset of fields slice_score's mirror of computeRanges / computeSliceBoundaries needs."""
        return ss.Zone(start01=self.start01, end01=self.end01, slice_by=self.slice_by, grid_beats=self.grid, count=self.count,
                       min_beats=self.min_beats, manual=list(self.manual), base_key=self.slice_base_key, thru=self.slice_thru)


GRID_BEATS = [0.0, 0.25, 0.5, 1.0]        # the plugin's stage Grid choice: Off, 1/16, 1/8, 1/4


@dataclass
class StageCfg:
    """One cascade stage's input conditioner (mirror of msmp::StageParams in Source/Cascade.h)."""
    tap: bool = False
    triggers: bool = True              # hand-over from the previous stage: one trigger per voice, or every note
    pass_pct: float = 100.0
    seed: int = 1
    grid_beats: float = 0.0
    range_lo: int = 0
    range_hi: int = 127
    snap: bool = False
    snap_mask: int = 0xFFF

    def is_neutral(self) -> bool:
        return self.pass_pct >= 100.0 and self.grid_beats <= 0.0 and self.range_lo <= 0 and self.range_hi >= 127 and not self.snap


@dataclass
class SetupConfig:
    zones: list
    sources: dict                      # slot index (0-based) -> slice_score.Sequence
    source_names: dict
    speed: float = 100.0
    sync: bool = True
    tempo: float = 120.0
    voices: int = 4
    stop_key: int = -1
    approximations: list = field(default_factory=list)
    cascade: bool = False
    stages: list = field(default_factory=lambda: [StageCfg(), StageCfg(), StageCfg()])

    def zone_stage(self, z: ZoneCfg) -> int:
        return max(0, min(2, z.stage)) if self.cascade else 0

    def active_stages(self) -> list:
        """Stages that have an enabled zone with a loaded source, in order (stage 1 first); [0] when there are none."""
        used = sorted({self.zone_stage(z) for z in self.zones if z.enabled})
        return used or [0]

    def host_stage(self) -> int:
        """The stage the host's notes go to: the first active one."""
        return self.active_stages()[0]


def key_offset_from(z: ZoneCfg, key: int, base: int) -> int:
    d = key - base
    if not z.key_map_set:
        return d * z.key_step
    steps = [i for i in range(12) if z.pc_mask >> i & 1] or [0]
    octave, idx = divmod(d, len(steps))                  # floor division, like the C++ negative-distance fix
    return octave * 12 + steps[idx]


def fold_pitch(p: int) -> int:
    while p < 0:
        p += 12
    while p > 127:
        p -= 12
    return p


# ---------------------------------------------------------------------------------------------- setup file
def load_setup(path: str) -> SetupConfig:
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    if data.get("format") != "MidiSamplerSetup":
        raise ValueError("This is not a MidiSampler setup file (use Save Setup in the plugin).")

    values = {p["id"]: p["value"] for p in data["parameters"]}

    def v(pid, default=0.0):
        return values.get(pid, default)

    zones = []
    for i in range(8):
        z = i + 1
        cfg = ZoneCfg(
            index=i, enabled=v(f"z{z}_on") >= 0.5, key_lo=int(round(v(f"z{z}_lo", 0))), key_hi=int(round(v(f"z{z}_hi", 127))),
            source=int(round(v(f"z{z}_src", z))) - 1, root=int(round(v(f"z{z}_root", 60))),
            start01=v(f"z{z}_start", 0.0), end01=v(f"z{z}_end", 1.0), loop_start01=v(f"z{z}_loopStart", 0.0), loop_end01=v(f"z{z}_loopEnd", 1.0),
            mode=MODES[max(0, min(5, int(round(v(f"z{z}_mode")))))], playback=PLAYBACKS[max(0, min(2, int(round(v(f"z{z}_gate")))))],
            play_mode=max(0, min(2, int(round(v(f"z{z}_playMode"))))), slice_by=SLICE_BYS[max(0, min(3, int(round(v(f"z{z}_sliceBy")))))],
            grid=GRIDS[max(0, min(4, int(round(v(f"z{z}_sliceGrid", 2)))))], count=int(round(v(f"z{z}_sliceCount", 8))),
            min_beats=v(f"z{z}_sliceMin", 0.25), slice_thru=v(f"z{z}_slicePlay") >= 0.5, slice_base_key=int(round(v(f"z{z}_sliceKey", 24))),
            out_channel=int(round(v(f"z{z}_chan"))), invert=v(f"z{z}_invert") >= 0.5, axis=int(round(v(f"z{z}_axis", 60))),
            ratio=RATIOS[max(0, min(10, int(round(v(f"z{z}_ratio", 5)))))], key_map_set=v(f"z{z}_keyMap") >= 0.5,
            key_step=int(round(v(f"z{z}_step", 1))), pc_mask=int(round(v(f"z{z}_pcset", 4095))),
            delay=v(f"z{z}_delay"), phase=v(f"z{z}_phase"), warp_curve=int(round(v(f"z{z}_wcurve"))), warp_depth=v(f"z{z}_wdepth"),
            warp_cycle=CYCLES[max(0, min(7, int(round(v(f"z{z}_wcycle", 3)))))], warp_phase=v(f"z{z}_wphase"),
            slice_index=int(round(v(f"z{z}_sliceIdx", 1))) - 1, base_shift=int(round(v(f"z{z}_shift"))),
            stage=max(0, min(2, int(round(v(f"z{z}_stage"))))),
            note_chance=max(0.0, min(100.0, v(f"z{z}_chance", 100.0))), chance_seed=max(1, int(round(v(f"z{z}_cseed", z)))),
        )
        if cfg.key_hi < cfg.key_lo:
            cfg.key_lo, cfg.key_hi = cfg.key_hi, cfg.key_lo
        manual = data.get("manualSlices", [])
        if i < len(manual):
            cfg.manual = [float(x) for x in manual[i]]
        zones.append(cfg)

    sources, names = {}, {}
    for s in data.get("sources", []):
        notes = []
        for rec in s.get("notes", "").split(";"):
            parts = rec.split(",")
            if len(parts) >= 4:
                notes.append(ss.SeqNote(float(parts[0]), float(parts[1]), int(parts[2])))
        notes.sort(key=lambda n: n.start)
        last_end = max((n.end for n in notes), default=0.0)
        sources[int(s["slot"]) - 1] = ss.Sequence(notes, max(float(s.get("lengthBeats", 0.0)), last_end))
        names[int(s["slot"]) - 1] = s.get("name", "")

    cfg = SetupConfig(zones=zones, sources=sources, source_names=names, speed=v("speed", 100.0), sync=v("sync", 1.0) >= 0.5,
                      tempo=v("tempo", 120.0), voices=max(1, min(16, int(round(v("voices", 8))))), stop_key=int(round(v("stopKey", -1))))
    cfg.cascade = v("cascade") >= 0.5
    for k in range(3):
        sg = cfg.stages[k]
        sg.tap = v(f"s{k + 1}_tap") >= 0.5
        sg.triggers = v(f"s{k + 1}_mode") < 0.5
        sg.pass_pct = max(0.0, min(100.0, v(f"s{k + 1}_pass", 100.0)))
        sg.seed = max(1, min(9999, int(round(v(f"s{k + 1}_seed", 1)))))
        sg.grid_beats = GRID_BEATS[max(0, min(3, int(round(v(f"s{k + 1}_grid")))))]
        sg.range_lo = max(0, min(127, int(round(v(f"s{k + 1}_lo", 0)))))
        sg.range_hi = max(0, min(127, int(round(v(f"s{k + 1}_hi", 127)))))
        if sg.range_hi < sg.range_lo:
            sg.range_lo, sg.range_hi = sg.range_hi, sg.range_lo
        sg.snap = v(f"s{k + 1}_snap") >= 0.5
        sg.snap_mask = max(1, min(4095, int(round(v(f"s{k + 1}_snapSet", 4095)))))
    if cfg.cascade and len(cfg.active_stages()) > 1:
        cfg.approximations.append("The cascade has several stages: the model predicts stage 1 exactly; what the later stages play is only known from the real-engine replay")
    for z in zones:
        if z.enabled and z.note_chance < 100.0:
            cfg.approximations.append(f"Zone {z.index + 1} thins its notes to {z.note_chance:g} % (the output is predicted by the real-engine replay, not the model)")
        if z.enabled and z.warp_curve > 0 and z.warp_depth > 0:
            cfg.approximations.append(f"Zone {z.index + 1} uses a time curve: when its voices end by themselves is approximate")
        if z.enabled and z.delay > 0:
            cfg.approximations.append(f"Zone {z.index + 1} has an entry delay of {z.delay:g} beats")
    return cfg


# ---------------------------------------------------------------------------------------------- engine model
@dataclass
class Voice:
    zone: int
    key: int
    channel: int
    start: float
    end: float                 # math.inf while it loops / is held
    age: int
    one_shot: bool             # ignores key release (Latch and Start only)
    latch: bool
    transpose: int = 0
    window: tuple = (0.0, 0.0)
    active: bool = True


class EngineModel:
    """Feed it events in time order with press()/release(); read active keys at any time afterwards with active_keys(t)."""

    def __init__(self, cfg: SetupConfig):
        self.cfg = cfg
        self.voices: list[Voice] = []
        self.age = 0
        self.rate_base = cfg.speed / 100.0              # source beats per host beat, before each zone's own ratio
        self.stage = cfg.host_stage()                   # the model is the stage the host's notes go to
        self.zones = [z for z in cfg.zones if cfg.zone_stage(z) == self.stage]
        self.cond = cfg.stages[self.stage]
        self.rng = 0                                    # xorshift state of the stage's seeded pass %, like Cascade::rand01
        self.slots: dict = {}                           # (channel, host key) -> key actually pressed, or None when it was dropped

    # ---- zone queries
    def key_is_mapped(self, key: int) -> bool:
        if key == self.cfg.stop_key:
            return True
        return any(self._zone_matches(z, key) for z in self.zones)

    def _zone_matches(self, z: ZoneCfg, key: int) -> bool:
        seq = self.cfg.sources.get(z.source)
        return z.enabled and z.key_lo <= key <= z.key_hi and seq is not None and bool(seq.notes)

    def start_info(self, z: ZoneCfg, key: int):
        """Mirror of startVoice's window logic. Returns (window, transpose, start_pos, direction) or None when the key plays nothing."""
        seq = self.cfg.sources[z.source]
        sz = z.as_slice_zone()
        start, end = ss.compute_range(sz, seq)
        win_start, win_end = start, end
        transpose = key_offset_from(z, key, z.root)
        if z.play_mode >= 1:
            b = ss.compute_slice_boundaries(seq, sz)
            ns = len(b) - 1
            idx = z.slice_index if z.play_mode == 2 else key - z.slice_base_key
            if idx < 0 or idx >= ns:
                return None
            win_start = b[idx]
            win_end = end if z.slice_thru else b[idx + 1]
            transpose = key_offset_from(z, key, z.key_lo) + z.base_shift if z.play_mode == 2 else 0
        span = win_end - win_start
        enter = min(max(z.phase, 0.0), 1.0) * span
        if starts_reversed(z.mode):
            return (win_start, win_end), transpose, max(win_start, win_end - enter), -1
        return (win_start, win_end), transpose, min(win_end, win_start + enter), +1

    def natural_length(self, z: ZoneCfg, window, pos: float, direction: int) -> float:
        """Host beats until a voice ends by itself (inf for loops); includes the entry delay."""
        if is_looping(z.mode):
            return math.inf
        rate = max(self.rate_base * max(z.ratio, 0.01), 1e-9)
        travel = (window[1] - pos) if direction > 0 else (pos - window[0])
        return z.delay + max(0.0, travel) / rate

    # ---- input conditioning of the host's notes (range, seeded pass %, pitch-set snap); the grid is assumed to be met by the caller
    def reset_random(self) -> None:
        self.rng = 0

    def _rand01(self) -> float:
        if self.rng == 0:
            self.rng = ((self.cond.seed * 2654435761) & 0xFFFFFFFF) | 1
        s = self.rng
        s ^= (s << 13) & 0xFFFFFFFF
        s ^= s >> 17
        s ^= (s << 5) & 0xFFFFFFFF
        self.rng = s
        return (s >> 8) / 16777216.0

    @staticmethod
    def snap_pitch(pitch: int, mask: int) -> int:
        mask &= 0xFFF
        if mask == 0:
            return pitch
        for d in range(12):
            if mask >> ((pitch - d) % 12) & 1 and pitch - d >= 0:
                return pitch - d
            if mask >> ((pitch + d) % 12) & 1 and pitch + d <= 127:
                return pitch + d
        return pitch

    def conditioned_key(self, key: int):
        """What a host press of `key` becomes (None = dropped). Consumes one random number exactly when the C++ does."""
        c = self.cond
        if key < c.range_lo or key > c.range_hi:
            return None
        if c.pass_pct < 100.0 and self._rand01() * 100.0 >= c.pass_pct:
            return None
        return self.snap_pitch(key, c.snap_mask) if c.snap else key

    # ---- events
    def press(self, key: int, t: float, channel: int = 0) -> None:
        if not self.key_is_mapped(key):
            return
        if key == self.cfg.stop_key:
            self.stop_all(t)
            self.reset_random()
            self.slots.clear()
            return
        used = self.conditioned_key(key)
        self.slots[(channel, key)] = used
        if used is None:
            return
        self._press_raw(used, t, channel)

    def _press_raw(self, key: int, t: float, channel: int = 0) -> None:
        # a latch voice already running on this key: the press switches it off
        if any(v.active and v.latch and v.key == key and v.channel == channel and self._alive(v, t) for v in self.voices):
            for v in self.voices:
                if v.active and v.key == key and v.channel == channel and self._alive(v, t):
                    self._stop(v, t)
            return
        for v in self.voices:                                        # a re-pressed key restarts cleanly
            if v.active and v.key == key and v.channel == channel and self._alive(v, t):
                self._stop(v, t)
        for z in self.zones:
            if not self._zone_matches(z, key):
                continue
            info = self.start_info(z, key)
            if info is None:
                continue
            window, transpose, pos, direction = info
            alive = [v for v in self.voices if v.active and self._alive(v, t)]
            if len(alive) >= self.cfg.voices:
                self._stop(min(alive, key=lambda v: v.age), t)       # oldest voice is stolen
            self.age += 1
            end = t + self.natural_length(z, window, pos, direction)
            self.voices.append(Voice(z.index, key, channel, t, end, self.age, z.playback != "gate", z.playback == "latch",
                                     transpose, window))

    def release(self, key: int, t: float, channel: int = 0) -> None:
        used = self.slots.pop((channel, key), None)         # a release without a press (or after a stop-all) does nothing, as in Cascade
        if used is None:
            return
        key = used
        for v in self.voices:
            if v.active and not v.one_shot and v.key == key and v.channel == channel and self._alive(v, t):
                self._stop(v, t)

    def stop_all(self, t: float) -> None:
        for v in self.voices:
            if v.active and self._alive(v, t):
                self._stop(v, t)
        self.slots.clear()

    @staticmethod
    def _alive(v: Voice, t: float) -> bool:
        return v.active and v.end > t + EPS

    @staticmethod
    def _stop(v: Voice, t: float) -> None:
        v.end = min(v.end, t)
        v.active = False

    # ---- queries
    def active_keys(self, t: float) -> list[int]:
        return sorted({v.key for v in self.voices if v.start <= t + EPS and v.end > t + EPS})

    def active_count(self, t: float) -> int:
        return sum(1 for v in self.voices if v.start <= t + EPS and v.end > t + EPS)

    def ambiguous_keys(self, t: float, tol: float) -> set:
        """Keys with a voice that ends within tol of t (natural ends are not exact to the sample)."""
        return {v.key for v in self.voices if v.end != math.inf and abs(v.end - t) < tol}
