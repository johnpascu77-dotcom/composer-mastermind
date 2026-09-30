"""Ported from this repo's own C++ generative core, faithfully, not
reinvented - so a score built offline by this script lines up with what
Composer Mastermind's own Propose flow would have produced given the same
shape/seed, and so its vocabulary (shape names, archetype baselines) stays
one system, not a second invented one:

- src/policy/ArcShapeLibrary.cpp  -> SHAPES / sample_shape / generate_breakpoints
- src/policy/DurationCalculator.cpp (flat-tempo case only) -> duration_plan
- src/policy/BlueprintGenerator.cpp -> classify_archetypes / ARCHETYPE_BASELINES

Pure, deterministic for a given seed - same "no raw/unseeded randomness"
discipline the C++ side holds itself to.
"""

from __future__ import annotations

import math
import random
from dataclasses import dataclass, field

PI = math.pi

SHAPES = [
    "organic_build",
    "arch_rise_fall",
    "long_fade",
    "terraced_blocks",
    "surging_waves",
    "heroic_journey",
    "suspense_release",
    "mosaic_episodic",
    "catastrophe_collapse",
    "pastoral_plateau",
]


def _clamp01(x: float) -> float:
    return max(0.0, min(1.0, x))


def sample_shape(shape: str, t: float) -> float:
    """One value per shape at normalized position t (0..1 across the whole
    piece) - exact port of ArcShapeLibrary.cpp's sampleShape."""
    if shape == "organic_build":
        return _clamp01(t ** 1.6)

    if shape == "arch_rise_fall":
        return _clamp01(math.sin(t * PI))

    if shape == "long_fade":
        return _clamp01((1.0 - t) ** 1.3)

    if shape == "terraced_blocks":
        step = max(0, min(3, int(t * 3.999)))
        levels = [0.2, 0.5, 0.35, 0.9]
        return levels[step]

    if shape == "surging_waves":
        base = 0.12 + 0.5 * t
        swell = 0.32 * (0.5 - 0.5 * math.cos(t * 6.0 * PI))
        return _clamp01(base + swell)

    if shape == "heroic_journey":
        if t < 0.28:
            return _clamp01(0.42 + 0.06 * math.sin(t * 12.0))
        if t < 0.52:
            u = (t - 0.28) / 0.24
            return _clamp01(0.4 - 0.24 * u)
        u = (t - 0.52) / 0.48
        return _clamp01(0.18 + 0.82 * (u ** 1.2))

    if shape == "suspense_release":
        if t < 0.72:
            return _clamp01(0.16 + 0.14 * t)
        if t < 0.84:
            u = (t - 0.72) / 0.12
            return _clamp01(0.24 + 0.7 * u)
        u = (t - 0.84) / 0.16
        return _clamp01(0.94 - 0.5 * u)

    if shape == "mosaic_episodic":
        return _clamp01(0.5 + 0.42 * math.sin(t * 7.3 + 1.1))

    if shape == "catastrophe_collapse":
        if t < 0.56:
            return _clamp01(0.82 * ((t / 0.56) ** 1.3))
        if t < 0.63:
            u = (t - 0.56) / 0.07
            return _clamp01(0.82 - 0.74 * u)
        u = (t - 0.63) / 0.37
        return _clamp01(0.08 + 0.4 * u)

    if shape == "pastoral_plateau":
        if t < 0.22:
            return _clamp01((t / 0.22) * 0.5)
        if t < 0.85:
            return _clamp01(0.5 + 0.06 * math.sin(t * 8.0 * PI))
        u = (t - 0.85) / 0.15
        return _clamp01(0.5 + u * 0.22)

    return _clamp01(t)


def generate_breakpoints(shape: str, section_boundaries: list[int], restlessness: float,
                          rng: random.Random) -> list[tuple[int, float]]:
    """One (bar, value) breakpoint per boundary. restlessness (0..1) adds a
    bounded seeded jitter on top of the shape's own smooth curve - "riding
    on top of the shape's coherent curve, not instead of it," same as the
    C++ original."""
    if len(section_boundaries) < 2:
        return []

    restlessness = _clamp01(restlessness)
    total_bars = section_boundaries[-1]

    breakpoints: list[tuple[int, float]] = []
    for bar in section_boundaries:
        t = (bar / total_bars) if total_bars > 0 else 0.0
        value = sample_shape(shape, t)
        if restlessness > 0.0:
            value = _clamp01(value + rng.uniform(-0.08, 0.08) * restlessness)
        breakpoints.append((bar, value))

    return breakpoints


@dataclass
class DurationPlan:
    total_bars: int
    section_boundaries: list[int] = field(default_factory=list)


def duration_plan(target_minutes: float, beats_per_bar: int, bpm: float, desired_section_count: int) -> DurationPlan:
    """Flat-tempo case only (this generator doesn't offer a tempo ramp) -
    DurationCalculator.cpp's totalBeats collapses to bpm * minutes for a
    single tempo point, matching its own doc comment exactly."""
    beats_per_bar = max(1, beats_per_bar)
    desired_section_count = max(1, desired_section_count)
    target_minutes = max(0.0, target_minutes)

    beats = bpm * target_minutes
    total_bars = max(1, round(beats / beats_per_bar))

    boundaries = [0]
    for i in range(1, desired_section_count):
        boundary = round((total_bars * i) / desired_section_count)
        if boundary > boundaries[-1]:
            boundaries.append(boundary)
    if boundaries[-1] != total_bars:
        boundaries.append(total_bars)

    return DurationPlan(total_bars=total_bars, section_boundaries=boundaries)


@dataclass
class SectionPlan:
    start_bar: int
    end_bar: int
    archetype: str  # "presentation" | "build" | "peak" | "release"


def classify_archetypes(breakpoints: list[tuple[int, float]]) -> list[SectionPlan]:
    """Exact port of BlueprintGenerator.cpp's buildSectionPlans +
    classifyArchetypes: each consecutive breakpoint pair is one section;
    exactly one section is "peak" (the one ending at, or opening at, the
    curve's global maximum); everything before it is build (rising more
    than kRiseThreshold) or presentation; everything after is release."""
    if len(breakpoints) < 2:
        return []

    rise_threshold = 0.05

    plans = []
    for i in range(len(breakpoints) - 1):
        plans.append((breakpoints[i][0], breakpoints[i + 1][0], breakpoints[i][1], breakpoints[i + 1][1]))

    peak_index = 0
    peak_value = plans[0][2]
    for i, (_, _, _, end_value) in enumerate(plans):
        if end_value > peak_value:
            peak_value = end_value
            peak_index = i

    sections = []
    for i, (start_bar, end_bar, start_value, end_value) in enumerate(plans):
        if i == peak_index:
            archetype = "peak"
        elif i < peak_index:
            archetype = "build" if (end_value - start_value) > rise_threshold else "presentation"
        else:
            archetype = "release"
        sections.append(SectionPlan(start_bar=start_bar, end_bar=end_bar, archetype=archetype))

    return sections


# Exact port of BlueprintGenerator.cpp's tensionBaseline/complexityBaseline/
# coherenceBaseline/densityBaseline/energyBaseline tables - "simple and
# explainable on purpose, not a precise musical model."
ARCHETYPE_BASELINES: dict[str, dict[str, float]] = {
    "presentation": {"tension": 0.2, "complexity": 0.2, "coherence": 0.6, "density": 0.3, "energy": 0.3},
    "build":        {"tension": 0.5, "complexity": 0.5, "coherence": 0.5, "density": 0.5, "energy": 0.6},
    "peak":         {"tension": 0.9, "complexity": 0.8, "coherence": 0.8, "density": 0.9, "energy": 1.0},
    "release":      {"tension": 0.35, "complexity": 0.3, "coherence": 0.45, "density": 0.4, "energy": 0.3},
}
