# Composer Mastermind — Orientation

Read this first, then go to the linked docs for actual content — this file is a map, not a duplicate. Keep it
short; put real explanation in `docs/`, not here.

## What this is

An autonomous MIDI "conductor" plugin (JUCE, VST3 + Standalone) that drives several instances of a companion
instrument plugin, MIDI Pattern Launcher (MPL), via MIDI CC, following a narrative arc over a piece. **"Composer"
means the plugin itself is the autonomous composer — not a live-performance tool for a human to play.**

**Using the Expert UI to actually build a piece?** Start with [docs/user_guide.md](docs/user_guide.md) instead —
a real walkthrough, real values, zero to a playing piece. Already comfortable with that core loop and want every
tab and pending-list field to earn its keep in one real piece, plus the same piece built a second way starting
from the Score View?
[docs/advanced_workflow_example.md](docs/advanced_workflow_example.md) is the deeper companion, same voice, same
method. The list below is for continuing development on the plugin itself, not for operating it.

Start here, in order:
1. [docs/composer_mastermind_design.md](docs/composer_mastermind_design.md) — vision, architecture, phased
   roadmap (v0.1 through v1.1). Updated every session; read this before assuming anything about scope or status.
2. [docs/technical_spec_checklist.md](docs/technical_spec_checklist.md) — granular done/not-done by file, faster
   than diffing `src/`.
3. [docs/routing_policy_v0_1.md](docs/routing_policy_v0_1.md),
   [docs/mutation_policy_v0_1.md](docs/mutation_policy_v0_1.md), [docs/scheduling_policy_v0_1.md](docs/scheduling_policy_v0_1.md)
   — subsystem detail, as needed. [docs/scene_format_v0_1.md](docs/scene_format_v0_1.md) is superseded/stale — see
   the format doc below instead.
4. [docs/composition_bundle_format.md](docs/composition_bundle_format.md) — the "composing by numbers" JSON file
   format: one file (Blueprint + Scenes + Instances + Motif Presets + Modulator Targets) that fully reconfigures
   the plugin on import. Built and live; ships with a ready-to-import template.
   [score-generator/](score-generator/generate_score.py) is a standalone Python tool (no live session/Bitwig
   needed) that generates a full bundle offline — real per-section literal note content
   (`capturedContent`, for deterministic Absolute-mode playback) plus matching `MotifPreset` coverage, not just
   structure. Mirrors `ArcShapeLibrary`/`DurationCalculator`/`BlueprintGenerator`'s own C++ logic faithfully
   (see its own module docstrings for exactly which files) rather than inventing a second vocabulary.
   [slice_score.py](score-generator/slice_score.py) is the sibling generator for "composing by slices" (MPL as trigger shooter driving the
   MidiSampler plugin); concept, measured 2-bar lag and checks in [docs/composing_by_slices_concept.md](docs/composing_by_slices_concept.md).
5. [docs/atonal_phrase_engine_concepts.md](docs/atonal_phrase_engine_concepts.md) — mined concepts for future
   arc/governor work, not yet implemented.
6. [docs/score_timeline_ui_concept.md](docs/score_timeline_ui_concept.md) — condensed-score piano roll, live sync,
   MIDI drag-out; built and live (Track A) — see the doc for exactly what's confirmed vs. still open.
7. [docs/arc_dimension_mapping_concept.md](docs/arc_dimension_mapping_concept.md) — giving the 5 arc dimensions
   real playback-time consumers (phrase-chaining, melodic curve, swing, mutation budget, Coherence-driven
   Retrograde/M7 divergence). Built and live for all 5 dimensions.

## Related projects outside this repo (easy to lose track of — not discoverable by exploring this folder alone)

- **MIDI Pattern Launcher (MPL)** — `C:\Users\Asus\Documents\JUCE\Projects\NewProject\Source\` (note: sibling
  project's own root, one level up from this repo, *not* a subfolder of it). The instrument this plugin drives.
  Its actual External Control CC protocol is in `Source/PluginProcessor.cpp`, `handleExternalControlCC` — must
  stay byte-compatible with `src/midi/CCMapping.h` here.
- **Source roadmap doc** — `C:\Users\Asus\Documents\MIDI PATTERN LAUNCHER ION\MIDI Pattern Composer Architectural
  Roadmap.md` — the original conceptual vision doc this whole project operationalizes.
- **atonal_phrase_engine** — `C:\Users\Asus\Documents\atonal_phrase_engine\` — unrelated Python note-generator,
  mined once for transferable arc/governance concepts (see doc link above). Not otherwise connected to this repo.
- **Atonal Motive Engine (AME)** — `C:\atonal-motive-engine\` (own repo, github.com/johnpascu77-dotcom/AtonalMotiveEngine).
  CLAP motive transformer with a 4-stage operation chain and presets. MC steers it with ordinary Modulator Targets
  on CC 85-90 (preset / scan / active stages / stage mask / output tap / bar-line retrigger); recipes in
  [docs/ame_bridge.md](docs/ame_bridge.md), contract in AME's `docs/MASTERMIND_BRIDGE.md`. Modulator targets can
  `sendAhead` (one bar early) so AME has a bar to prepare. **Style profiles** (a shared vocabulary of AME presets +
  an MC score + pitch field): first one is `romanian_shadow` — [docs/style_profile_romanian_shadow.md](docs/style_profile_romanian_shadow.md),
  generator `score-generator/romanian_shadow.py` (one AME per voice, editable plan; `--help`), also as a browser page
  `score-generator/ame_score_generator.html` behind Score View's **AME Score...** button.
- **MidiTools** — `C:\AudioDev\Repos\MidiTools` (own local git repo, no remote yet). Offline piano-roll edit suite inspired by
  Ableton Live 12 MIDI Tools: Connect driven by Slonimsky/modal logic, Chop driven by a rhythm-preset library. Standalone
  app + std-only core library; Phase 0 + Connect (Slonimsky logic) + Chop (rhythm presets, load measures) + 6 Connect logics + Key/Scale + Phase 2 + 3 tools (Recombine, Serial, Velocity, Time Warp, Quantize, Strum, Arpeggiate, Ornament) + a real-time Arpeggiator plugin (VST3+CLAP, Plugin/) built 2026-10-24; live-tested in Bitwig in a ChordHarmonizer -> Arpeggiator -> OrchCapture -> Dorico chain (`Docs/MidiTools_Scoping.md`). Not connected to this repo.
- **Orch System** — `C:\AudioDev\Repos\` (own separate git repos per plugin: `OrchNoteMapper`, `OrchGate`,
  `OrchConductor`; roadmap at `ORCH_SYSTEM_ROADMAP.md` and `OrchConductor\Docs\OrchGate_OrchConductor_Roadmap.md`
  in that folder). A second, independent modular orchestral MIDI plugin family (per-instrument range/keyswitch
  mapping + participation gating + orchestration-preset control in Bitwig, real sample rendering eventually
  external via Vienna Ensemble Pro), predating this project. Research-phase-only thread (as of 2026-08-27) on
  linking it downstream of Composer Mastermind + MPL, with MC's Blueprint/ArcSet driving `OrchConductor`'s CC
  control path for form/narrative shape — the thing pure per-track randomization there can't supply on its own.
  Notation (Dorico) accuracy is the real target of that whole ecosystem, not audio realism. Not otherwise
  connected to this repo yet.

## Build

```
cmake --build build --config Release --target ComposerMastermind_VST3
```

**Known quirk:** the post-build step copies into `C:\Program Files\Common Files\VST3\`. If Bitwig has the plugin
loaded, this fails with a file-lock error (not a permissions error) — close Bitwig first, then rebuild.

## Version control

Public GitHub remote: https://github.com/johnpascu77-dotcom/composer-mastermind (`origin/main`). `external/JUCE/`
is gitignored (vendored, has its own nested `.git` — see the comment in `.gitignore`). **Standing instruction from the user
(2026-10-08): commit and push every change by default, in this repo and in every plugin repo — no need to ask each time.**
Force-pushes and rewriting pushed history still need an explicit yes.
