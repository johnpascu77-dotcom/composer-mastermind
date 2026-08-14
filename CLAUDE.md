# Composer Mastermind — Orientation

Read this first, then go to the linked docs for actual content — this file is a map, not a duplicate. Keep it
short; put real explanation in `docs/`, not here.

## What this is

An autonomous MIDI "conductor" plugin (JUCE, VST3 + Standalone) that drives several instances of a companion
instrument plugin, MIDI Pattern Launcher (MPL), via MIDI CC, following a narrative arc over a piece. **"Composer"
means the plugin itself is the autonomous composer — not a live-performance tool for a human to play.**

Start here, in order:
1. [docs/composer_mastermind_design.md](docs/composer_mastermind_design.md) — vision, architecture, phased
   roadmap (v0.1 through v1.1). Updated every session; read this before assuming anything about scope or status.
2. [docs/technical_spec_checklist.md](docs/technical_spec_checklist.md) — granular done/not-done by file, faster
   than diffing `src/`.
3. [docs/routing_policy_v0_1.md](docs/routing_policy_v0_1.md), [docs/scene_format_v0_1.md](docs/scene_format_v0_1.md),
   [docs/mutation_policy_v0_1.md](docs/mutation_policy_v0_1.md), [docs/scheduling_policy_v0_1.md](docs/scheduling_policy_v0_1.md)
   — subsystem detail, as needed.
4. [docs/atonal_phrase_engine_concepts.md](docs/atonal_phrase_engine_concepts.md) — mined concepts for future
   arc/governor work, not yet implemented.

## Related projects outside this repo (easy to lose track of — not discoverable by exploring this folder alone)

- **MIDI Pattern Launcher (MPL)** — `C:\Users\Asus\Documents\JUCE\Projects\NewProject\Source\` (note: sibling
  project's own root, one level up from this repo, *not* a subfolder of it). The instrument this plugin drives.
  Its actual External Control CC protocol is in `Source/PluginProcessor.cpp`, `handleExternalControlCC` — must
  stay byte-compatible with `src/midi/CCMapping.h` here.
- **Source roadmap doc** — `C:\Users\Asus\Documents\MIDI PATTERN LAUNCHER ION\MIDI Pattern Composer Architectural
  Roadmap.md` — the original conceptual vision doc this whole project operationalizes.
- **atonal_phrase_engine** — `C:\Users\Asus\Documents\atonal_phrase_engine\` — unrelated Python note-generator,
  mined once for transferable arc/governance concepts (see doc link above). Not otherwise connected to this repo.

## Build

```
cmake --build build --config Release --target ComposerMastermind_VST3
```

**Known quirk:** the post-build step copies into `C:\Program Files\Common Files\VST3\`. If Bitwig has the plugin
loaded, this fails with a file-lock error (not a permissions error) — close Bitwig first, then rebuild.
