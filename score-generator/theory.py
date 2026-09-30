"""Music-theory building blocks for the offline score generator.

Deliberately small and named, not a general theory library - just enough to
give generate_score.py's phrase walker real, scale-constrained pitch choices
instead of free chromatic wandering. Matches the project's own stated
aesthetic (post-romantic atonalism over pointillism, Scriabin/"Vers la
flamme" as reference, see docs/composer_mastermind_design.md's origin
story) - the default scale leans moody/symmetric, not diatonic-major.
"""

from __future__ import annotations

# Each scale is a list of semitone offsets from its root, one octave,
# ascending, NOT including the octave itself (so len(scale) = scale degrees
# per octave). Picked for character, not exhaustiveness.
SCALES: dict[str, list[int]] = {
    "natural_minor": [0, 2, 3, 5, 7, 8, 10],
    "harmonic_minor": [0, 2, 3, 5, 7, 8, 11],
    "dorian": [0, 2, 3, 5, 7, 9, 10],
    "phrygian": [0, 1, 3, 5, 7, 8, 10],
    "whole_tone": [0, 2, 4, 6, 8, 10],
    "octatonic": [0, 2, 3, 5, 6, 8, 9, 11],  # whole-half diminished
    "pentatonic_minor": [0, 3, 5, 7, 10],
}

DEFAULT_SCALE = "phrygian"


def scale_degree_to_midi(root: int, scale_name: str, degree: int) -> int:
    """degree is a signed scale-step index (0 = root, -1 = one degree below
    root, etc.), not a semitone offset - octaves fall out naturally from
    degree wrapping past len(scale). Clamped to the MIDI 0-127 range."""
    scale = SCALES.get(scale_name, SCALES[DEFAULT_SCALE])
    n = len(scale)
    octave, index = divmod(degree, n)
    midi = root + scale[index] + 12 * octave
    return max(0, min(127, midi))


def nearest_degree_for_midi(root: int, scale_name: str, midi_note: int, search_range: int = 24) -> int:
    """Inverse-ish lookup: the scale degree whose midi note is closest to
    midi_note, searched within +/- search_range degrees of degree 0. Used
    once per instance to seed its starting degree from a chosen register
    center."""
    scale = SCALES.get(scale_name, SCALES[DEFAULT_SCALE])
    best_degree = 0
    best_distance = None
    for degree in range(-search_range, search_range + 1):
        candidate = scale_degree_to_midi(root, scale_name, degree)
        distance = abs(candidate - midi_note)
        if best_distance is None or distance < best_distance:
            best_distance = distance
            best_degree = degree
    return best_degree
