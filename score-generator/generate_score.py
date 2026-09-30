#!/usr/bin/env python3
"""Offline generator for Composer Mastermind composition-bundle JSON
("scores" - docs/composition_bundle_format.md, schema
ComposerMastermindComposition.v3), with real generative judgment: actual
literal note content per section/instance (BlueprintSection.capturedContent),
not just structure - so the output plays back deterministically in Absolute
mode, ready to go, the "piano roll" promise Absolute mode exists for. Also
derives real MotifPreset coverage for every archetype used, so a listener
who switches to Generative mode isn't met with the doc's own documented
"silent, no error" gotcha (a section with no matching motif preset).

No dependency on a live Composer Mastermind session, MPL, or Bitwig - this
is pure Python, reading nothing and writing one JSON file. Deterministic for
a given --seed (same "no raw/unseeded randomness" discipline the C++
generative code holds itself to - see arc.py's own docstring for exactly
which C++ files this mirrors).

Usage:
    python generate_score.py --title "Winter Light" --minutes 5 \
        --shape arch_rise_fall --scale dorian --seed 42 \
        --output winter_light.json
"""

from __future__ import annotations

import argparse
import json
import random
import sys
from dataclasses import dataclass, field

import arc
import theory

PATTERN_STEPS = 16  # MPL's fixed step-array size, binary or ternary alike
                     # (ternary just uses the leading 12 and leaves 12-15
                     # disabled - CCMapping::effectiveStepCount's own
                     # convention).

# Per-archetype phrase character - deliberately simple and explainable, the
# same spirit as BlueprintGenerator.cpp's own baseline tables (arc.py's
# ARCHETYPE_BASELINES), just one layer more detailed since this also has to
# decide real notes, not only curve values.
#   density: fraction of available grid steps that get a note decision
#   step_bias: -1..1, net upward (positive) or downward (negative) melodic
#       drift; 0 = balanced random walk
#   step_max: largest single scale-degree leap (0 = only repeats/steps of 1)
#   durations: (grid-step length, relative weight) choices
#   velocity: (min, max) MIDI velocity range
#   grid_mode: 0 = Binary (16 steps), 1 = Ternary (12 steps)
#   swing: 0.0 / 66.67 / 100.0 (Off / Triplet / Shuffle)
ARCHETYPE_CHARACTER: dict[str, dict] = {
    "presentation": dict(density=0.45, step_bias=0.0, step_max=2,
                          durations=[(4, 0.35), (8, 0.25), (2, 0.30), (1, 0.10)],
                          velocity=(55, 82), grid_mode=0, swing=0.0),
    "build": dict(density=0.60, step_bias=0.35, step_max=3,
                   durations=[(2, 0.35), (1, 0.25), (4, 0.30), (8, 0.10)],
                   velocity=(65, 100), grid_mode=0, swing=0.0),
    "peak": dict(density=0.80, step_bias=0.10, step_max=5,
                  durations=[(1, 0.35), (2, 0.35), (4, 0.20), (8, 0.10)],
                  velocity=(90, 122), grid_mode=1, swing=66.67),
    "release": dict(density=0.35, step_bias=-0.35, step_max=2,
                      durations=[(8, 0.35), (4, 0.35), (2, 0.20), (1, 0.10)],
                      velocity=(45, 76), grid_mode=0, swing=0.0),
}

# Per-role register center (semitones from the piece root) and how far a
# role's own melodic walk is allowed to range either side of that center -
# anchor stays low and steady, motif carries the lead above it, counterpoint
# interleaves in between. Density/step_max multipliers give each role a
# distinct character within the same archetype rather than three identical
# copies of one line.
ROLE_PROFILE: dict[str, dict] = {
    "anchor": dict(register_offset=-22, register_span=10, density_mult=0.6, step_max_mult=0.6, phase=0),
    "motif": dict(register_offset=17, register_span=14, density_mult=1.0, step_max_mult=1.0, phase=0),
    "counterpoint": dict(register_offset=-2, register_span=16, density_mult=0.85, step_max_mult=0.85, phase=2),
}
DEFAULT_ROLE_PROFILE = dict(register_offset=0, register_span=14, density_mult=0.85, step_max_mult=0.85, phase=0)


@dataclass
class Instance:
    id: str
    role: str
    channel: int


def weighted_choice(rng: random.Random, weighted: list[tuple[int, float]]) -> int:
    total = sum(w for _, w in weighted)
    r = rng.uniform(0, total)
    upto = 0.0
    for value, weight in weighted:
        upto += weight
        if r <= upto:
            return value
    return weighted[-1][0]


def generate_pattern_steps(rng: random.Random, scale_name: str, root: int, archetype: str, role: str,
                            start_degree: int, grid_steps: int) -> tuple[list[dict], int]:
    """Walks forward through one instance's step grid for one section,
    returning (steps, ending_degree) - the degree is threaded back in by the
    caller so an instance's melodic line continues coherently across
    sections instead of resetting each time. Monophonic by construction
    (never places a second note inside an already-claimed duration), so no
    separate overlap pass is needed, matching MPL's own one-voice-per-
    pattern invariant."""
    character = ARCHETYPE_CHARACTER[archetype]
    role_profile = ROLE_PROFILE.get(role, DEFAULT_ROLE_PROFILE)

    density = character["density"] * role_profile["density_mult"]
    step_max = max(0, round(character["step_max"] * role_profile["step_max_mult"]))
    p_up = max(0.05, min(0.95, 0.5 + character["step_bias"] / 2))

    register_center_degree = theory.nearest_degree_for_midi(root, scale_name, root + role_profile["register_offset"])
    span = role_profile["register_span"]
    degree_min = register_center_degree - span
    degree_max = register_center_degree + span

    degree = start_degree
    steps: list[dict] = []
    busy_until = role_profile["phase"] % max(1, grid_steps)

    for _ in range(busy_until):
        steps.append(dict(enabled=False, note=0, velocity=0, duration=0))

    step_index = busy_until
    while step_index < grid_steps:
        if rng.random() < density:
            if step_max > 0:
                magnitude = rng.randint(0, step_max)
            else:
                magnitude = 0
            direction = 1 if rng.random() < p_up else -1
            degree = max(degree_min, min(degree_max, degree + direction * magnitude))

            note = theory.scale_degree_to_midi(root, scale_name, degree)
            duration = weighted_choice(rng, character["durations"])
            duration = max(1, min(duration, grid_steps - step_index))
            velocity_min, velocity_max = character["velocity"]
            velocity = max(1, min(127, rng.randint(velocity_min, velocity_max)))

            steps.append(dict(enabled=True, note=note, velocity=velocity, duration=duration))
            busy_until = step_index + duration
            step_index += 1
            for _ in range(duration - 1):
                if step_index >= grid_steps:
                    break
                steps.append(dict(enabled=False, note=0, velocity=0, duration=0))
                step_index += 1
        else:
            steps.append(dict(enabled=False, note=0, velocity=0, duration=0))
            step_index += 1

    while len(steps) < PATTERN_STEPS:
        steps.append(dict(enabled=False, note=0, velocity=0, duration=0))

    return steps[:PATTERN_STEPS], degree


def rotate_steps(steps: list[dict], amount: int) -> list[dict]:
    """Circular rotation of the raw step array - the P2 variant in the
    base/rotated/inverted cycle below. Distinct from MPL's own Rotation CC
    parameter: capturedContent writes literal steps directly
    (ComposerCore::enterSection's Absolute branch), bypassing every scene-
    level pattern modifier entirely, so getting a rotated *sound* out of a
    captured pattern means actually rotating the step data here, not
    leaning on MPL's playback-time Rotation."""
    n = len(steps)
    if n == 0:
        return []
    amount = amount % n
    return [dict(s) for s in (steps[amount:] + steps[:amount])]


def invert_steps(steps: list[dict]) -> list[dict]:
    """Mirrors every enabled note's pitch around the pattern's own mean
    pitch (2*center - note, clamped) - the P3 variant. Rests/disabled steps
    pass through unchanged; MotifEngine.cpp's own invert() does the
    equivalent thing to a relative MotifNote cell (negate semitoneOffset),
    just applied here to absolute captured pitches instead."""
    enabled_notes = [s["note"] for s in steps if s["enabled"]]
    if not enabled_notes:
        return [dict(s) for s in steps]
    center = sum(enabled_notes) / len(enabled_notes)

    result = []
    for s in steps:
        if s["enabled"]:
            new_note = max(0, min(127, round(2 * center - s["note"])))
            result.append(dict(enabled=True, note=new_note, velocity=s["velocity"], duration=s["duration"]))
        else:
            result.append(dict(s))
    return result


# Minimum bars a macro-section needs before it's worth splitting into a
# base/rotated/inverted mini-section cycle at all - below this, a 3-way
# split would produce mini-sections too short to read as real phrases, so
# the section just stays whole (P1/base only, the old behavior).
MIN_BARS_FOR_VARIATION_CYCLE = 6


def split_into_mini_sections(start_bar: int, end_bar: int) -> list[tuple[int, int]]:
    """Up to 3 roughly-equal, bar-contiguous chunks (fewer if the macro-
    section is too short - see MIN_BARS_FOR_VARIATION_CYCLE). Any remainder
    bars go to the last chunk rather than leaving a gap."""
    total = end_bar - start_bar
    if total < MIN_BARS_FOR_VARIATION_CYCLE:
        return [(start_bar, end_bar)]

    chunk = total // 3
    bounds = [start_bar, start_bar + chunk, start_bar + 2 * chunk, end_bar]
    return [(bounds[i], bounds[i + 1]) for i in range(3)]


def derive_motif_preset(preset_id: str, tags: list[str], steps: list[dict]) -> dict:
    """One MotifNote per raw grid step, not per enabled note - exact same
    convention as MotifEngine::deriveMotifPresetFromPattern (C++, added
    2026-09-21 for the "capture from MPL" workflow): a disabled step becomes
    a rest, an enabled step's semitoneOffset is relative to the pattern's
    own first enabled note, relativeDuration/relativeVelocity read straight
    off the step (velocity/100). Kept identical on purpose so a listener
    comparing Absolute playback against what Generative mode would derive
    from the same content sees consistent behavior."""
    first_note = None
    for step in steps:
        if step["enabled"]:
            first_note = step["note"]
            break

    notes = []
    if first_note is not None:
        for step in steps:
            if step["enabled"]:
                notes.append(dict(
                    semitoneOffset=step["note"] - first_note,
                    relativeDuration=float(max(1, step["duration"])),
                    relativeVelocity=step["velocity"] / 100.0,
                    isRest=False,
                ))
            else:
                notes.append(dict(semitoneOffset=0, relativeDuration=1.0, relativeVelocity=1.0, isRest=True))

    return dict(id=preset_id, name=preset_id, tags=tags, notes=notes)


@dataclass
class GeneratedPiece:
    bundle: dict = field(default_factory=dict)


def generate_piece(title: str, minutes: float, shape: str, scale_name: str, root: int, bpm: float,
                    beats_per_bar: int, section_count: int, restlessness: float, seed: int,
                    instances: list[Instance]) -> dict:
    rng = random.Random(seed)

    plan = arc.duration_plan(minutes, beats_per_bar, bpm, section_count)
    breakpoints = arc.generate_breakpoints(shape, plan.section_boundaries, restlessness, rng)
    sections_plan = arc.classify_archetypes(breakpoints)

    if not sections_plan:
        raise ValueError("Could not derive sections - need at least 2 section boundaries (raise --minutes or --sections)")

    instance_degrees: dict[str, int] = {}
    for instance in instances:
        role_profile = ROLE_PROFILE.get(instance.role, DEFAULT_ROLE_PROFILE)
        instance_degrees[instance.id] = theory.nearest_degree_for_midi(
            root, scale_name, root + role_profile["register_offset"])

    motif_presets_by_archetype: dict[str, dict] = {}
    scenes = []
    blueprint_sections = []

    # Base/rotated/inverted cycle (2026-09-21, closing the "does it use P1/
    # P2/P3" gap): each macro-section's underlying idea is generated ONCE
    # (base_steps_by_instance), then a long-enough macro-section is split
    # into up to 3 mini-BlueprintSections that each target a different
    # patternIndex (0/1/2) with a related variant of that same idea -
    # P1=base, P2=rotated, P3=inverted, the exact convention
    # MotifEngine::seedPhraseChainPatterns already uses for the live
    # generative engine, just applied here to literal captured content
    # instead of a relative MotifPreset cell. Real repetition-with-variation
    # at a musical (few-bar) timescale, not just a hard cut every macro-
    # section boundary.
    variant_fns = [lambda s: [dict(x) for x in s], lambda s: rotate_steps(s, len(s) // 4), invert_steps]

    for i, section_plan in enumerate(sections_plan):
        archetype = section_plan.archetype
        character = ARCHETYPE_CHARACTER[archetype]
        grid_steps = 12 if character["grid_mode"] == 1 else 16

        base_steps_by_instance: dict[str, list[dict]] = {}
        for instance in instances:
            steps, new_degree = generate_pattern_steps(
                rng, scale_name, root, archetype, instance.role, instance_degrees[instance.id], grid_steps)
            instance_degrees[instance.id] = new_degree
            base_steps_by_instance[instance.id] = steps

        if archetype not in motif_presets_by_archetype:
            # Prefer the motif-role instance, but a section this sparse can
            # (rarely) land on zero enabled steps for any one instance -
            # Validation::isValidMotifPreset rejects an empty-notes preset,
            # so fall back to whichever instance actually has content rather
            # than risk minting an unimportable one.
            motif_instance = next((i for i in instances if i.role == "motif"), instances[0])
            candidates = [base_steps_by_instance[motif_instance.id]] + [
                base_steps_by_instance[i.id] for i in instances if i.id != motif_instance.id]
            representative_steps = next(
                (steps for steps in candidates if any(step["enabled"] for step in steps)), candidates[0])
            motif_presets_by_archetype[archetype] = derive_motif_preset(
                f"{title}_{archetype}_motif", [archetype], representative_steps)

        layer_roles = []
        reserved_values = []
        if archetype == "peak":
            for instance in instances:
                layer_roles.append(dict(targetInstance=instance.id, layerRole="foreground"))
                reserved_values.append(dict(targetInstance=instance.id, patternIndex=0, type="inversion", value=1))
        elif archetype == "release":
            for instance in instances:
                if instance.role == "counterpoint":
                    layer_roles.append(dict(targetInstance=instance.id, layerRole="background"))

        mini_chunks = split_into_mini_sections(section_plan.start_bar, section_plan.end_bar)
        for variant_index, (mini_start, mini_end) in enumerate(mini_chunks):
            pattern_index = variant_index % 3
            variant_fn = variant_fns[pattern_index]

            section_id = f"{title}_section{i + 1}" if len(mini_chunks) == 1 else f"{title}_section{i + 1}_{variant_index + 1}"
            scene_id = f"{section_id}_scene"
            duration_bars = mini_end - mini_start

            captured_content = [
                dict(targetInstance=instance.id, patternIndex=pattern_index,
                     steps=variant_fn(base_steps_by_instance[instance.id]))
                for instance in instances
            ]

            scenes.append(dict(
                id=scene_id, name=scene_id, durationBars=duration_bars, quantize="bar",
                targets=[instance.id for instance in instances],
                global_=dict(activePattern=pattern_index + 1, gridMode=character["grid_mode"],
                              swing=character["swing"], rate=1),
                instanceOverrides=[], patterns=[], mutations=[], nextSceneId="", transitionStyle="hard", rampBars=0,
            ))

            blueprint_sections.append(dict(
                id=section_id, name=f"{section_id}_{archetype}", sceneId=scene_id,
                startBar=mini_start, durationBars=duration_bars, archetype=archetype,
                layerRoles=layer_roles, budgetOverrides=[], reservedValues=reserved_values, modulatorValues=[],
                capturedContent=captured_content,
            ))

    arc_curves = [dict(dimension="energy", points=[dict(bar=b, value=v) for b, v in breakpoints])]
    for dimension in ("tension", "density", "complexity", "coherence"):
        points = []
        for section_plan in sections_plan:
            points.append(dict(bar=section_plan.start_bar, value=arc.ARCHETYPE_BASELINES[section_plan.archetype][dimension]))
        points.append(dict(bar=sections_plan[-1].end_bar,
                            value=arc.ARCHETYPE_BASELINES[sections_plan[-1].archetype][dimension]))
        arc_curves.append(dict(dimension=dimension, points=points))

    bundle = dict(
        schema="ComposerMastermindComposition.v3",
        blueprint=dict(id=title, name=title, sections=blueprint_sections, arcCurves=arc_curves),
        scenes=[_fix_scene_keys(scene) for scene in scenes],
        instances=[dict(id=i.id, name=i.id, midiChannel=i.channel, role=i.role, enabled=True, lastKnownScene="")
                   for i in instances],
        motifPresets=list(motif_presets_by_archetype.values()),
        modulatorTargets=[],
        modulationRoutes=[],
    )
    return bundle


def _fix_scene_keys(scene: dict) -> dict:
    # dict(global_=...) is a Python-keyword workaround - rename to the
    # bundle format's real key before serializing.
    scene = dict(scene)
    scene["global"] = scene.pop("global_")
    return scene


def parse_instances(spec: str) -> list[Instance]:
    """id:role:channel[,id:role:channel...] - defaults to the standard
    anchor/motif/counterpoint trio on channels 1-3 if omitted."""
    if not spec:
        return [Instance("MPL1", "anchor", 1), Instance("MPL2", "motif", 2), Instance("MPL3", "counterpoint", 3)]

    instances = []
    for entry in spec.split(","):
        parts = entry.strip().split(":")
        if len(parts) != 3:
            raise ValueError(f"Bad --instances entry '{entry}' - expected id:role:channel")
        instance_id, role, channel = parts
        instances.append(Instance(instance_id.strip(), role.strip(), int(channel)))
    return instances


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--title", required=True, help="Piece/blueprint id and name")
    parser.add_argument("--minutes", type=float, default=4.0, help="Target length in minutes (default 4)")
    parser.add_argument("--bpm", type=float, default=100.0, help="Flat tempo, BPM (default 100)")
    parser.add_argument("--beats-per-bar", type=int, default=4, help="Default 4")
    parser.add_argument("--sections", type=int, default=6, help="Desired section count (default 6)")
    parser.add_argument("--shape", choices=arc.SHAPES, default="arch_rise_fall",
                         help="Named whole-piece energy shape (default arch_rise_fall)")
    parser.add_argument("--scale", choices=sorted(theory.SCALES.keys()), default=theory.DEFAULT_SCALE,
                         help=f"Default {theory.DEFAULT_SCALE}")
    parser.add_argument("--root", type=int, default=57, help="Root note, MIDI (default 57 = A3)")
    parser.add_argument("--restlessness", type=float, default=0.35,
                         help="0..1, seeded jitter on the energy curve (default 0.35)")
    parser.add_argument("--seed", type=int, default=None, help="Omit for a random (but reported) seed")
    parser.add_argument("--instances", default="", help="id:role:channel,... (default MPL1:anchor:1,MPL2:motif:2,MPL3:counterpoint:3)")
    parser.add_argument("--output", required=True, help="Output JSON file path")
    args = parser.parse_args()

    seed = args.seed if args.seed is not None else random.randint(0, 2**31 - 1)
    instances = parse_instances(args.instances)

    bundle = generate_piece(
        title=args.title, minutes=args.minutes, shape=args.shape, scale_name=args.scale, root=args.root,
        bpm=args.bpm, beats_per_bar=args.beats_per_bar, section_count=args.sections,
        restlessness=args.restlessness, seed=seed, instances=instances,
    )

    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(bundle, f, indent=2)

    total_bars = bundle["blueprint"]["sections"][-1]["startBar"] + bundle["blueprint"]["sections"][-1]["durationBars"]
    print(f"Wrote '{args.output}': {len(bundle['blueprint']['sections'])} sections, {total_bars} bars, "
          f"{len(bundle['motifPresets'])} motif preset(s), seed={seed}", file=sys.stderr)


if __name__ == "__main__":
    main()
