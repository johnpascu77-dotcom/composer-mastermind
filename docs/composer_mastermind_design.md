# Composer Mastermind — Design & Roadmap

## Provenance

This document operationalizes `MIDI Pattern Composer / Launcher Ecosystem — Conceptual and Architectural Roadmap`
(`C:\Users\Asus\Documents\MIDI PATTERN LAUNCHER ION\MIDI Pattern Composer Architectural Roadmap.md`), which describes
a companion plugin called **MIDI Pattern Composer**. Composer Mastermind *is* that plugin — same concept, built
independently before the two got connected. The rest of this doc maps that roadmap's vision onto this codebase's
actual files and proposes a phased path from where the code is today to where the roadmap points.

**Naming (2026-08-14):** confirmed low-stakes — "MIDI Pattern Composer" is an acceptable name, but the name itself
isn't the point. What matters is the capability described below. Not formally decided; revisit once the generative
layer exists and it's clearer which name actually fits the finished thing.

## North Star

> Musically meaningful complexity without cognitive overload.

MIDI Pattern Launcher (MPL) is the **engine** — it stores and plays motives. Composer Mastermind is the
**narrative intelligence** — it decides which motives play, when, on which instance, and how they mutate, according
to a formal arc. Composer Mastermind should never generate notes itself; it only ever sends CC to MPL instances (see
[routing_policy_v0_1.md](routing_policy_v0_1.md)).

The roadmap's guiding metaphor: *three horses, one direction* — several MPL instances, each with a distinct musical
role, driven toward one coherent form.

**Identity clarification (2026-08-15):** the "Composer" in the name is the plugin itself, not a human performer
using it. Composer Mastermind is not a live-performance instrument — it is an autonomous system that decides and
plays a formal arc on its own, the way a human composer decides a piece's form in advance rather than a performer
improvising it live. Manual/live-trigger input (a controller sending notes/CC to nudge scenes in real time), if it
ever gets built, is at most a minor debug/override convenience — it must never shape core architecture decisions
the way the autonomous arc-following behavior does.

### Player vs. Composer — a scope commitment (2026-08-14)

The source roadmap's v1.0 ("Narrative Blueprint Composer," §21.2) describes a system that **plays back** a
hand-authored JSON blueprint — sections, arcs, and layer roles the user writes out in advance. That's necessary
infrastructure but it stops short of what the name "Composer Mastermind" implies: the plugin should eventually
**propose** the blueprint itself, not just execute one handed to it.

Concretely, that means a generative layer sitting *above* the v0.5 blueprint player, which:

- assembles a candidate narrative arc (section list, foreground/support/background assignment, energy/tension/
  density curves) from the role and arc **presets** already planned in v0.4/v0.5 — building blocks, not
  free-form generation from nothing;
- respects the same cognitive-load governor and mutation budgets ([mutation_policy_v0_1.md](mutation_policy_v0_1.md))
  that already constrain manual/scripted blueprints, so a generated journey is never less disciplined than a
  hand-written one;
- is a *proposal*, inspectable and editable before it plays — the user should be able to see the arc it picked,
  swap a section, and re-render, not just accept an opaque black box.

This is explicitly **not** scoped yet — it depends on v0.4/v0.5 (roles, budgets, arc evaluator) existing first,
since generation needs the same building blocks a human author would use. Recorded here so the blueprint-player
milestone doesn't get mistaken for the finished vision.

**Precedent found (2026-08-15):** `atonal_phrase_engine`'s named groove vocabulary (rhythmic identities bound to
formal function, not generic random pools) and its legacy-grammar-map mechanism (reapplying small-form roles onto
a larger cyclic form) are concrete precedent for this layer's preset library — a vocabulary of named archetypes
(`presentation`, `local_climax_approach`, `global_climax`, `coda_echo_aftermath`) bound to arc position, reusable
across differently-sized pieces rather than composed from scratch per piece. See
[atonal_phrase_engine_concepts.md](atonal_phrase_engine_concepts.md).

### "Send To All" Is One Preset Among Many (2026-08-14)

Confirmed and fixed a real gap in the model, not just a future ambition: driving several instances doesn't mean
sending them identical behavior. Different instances should be able to run **contrasting, simultaneous** states —
one binary while another is ternary, one static while another develops — as the normal case, not an edge case.
"Send the same value to everyone" (today's editor test button) is one specific, simple preset among many possible
combinations, not the default interaction model.

**Fixed now:** `Scene::global` (Active Pattern / Grid Mode / Swing) was being sent identically to every instance in
`Scene::targets`, with no way to differentiate — `Router::sendSceneToInstance` in
[`routing/Router.cpp`](../src/routing/Router.cpp) didn't support per-instance divergence for these three
parameters, even though `ScenePattern` already supported it for transpose/rotation/length/inversion. Added
`SceneInstanceOverride` to [`model/Scene.h`](../src/model/Scene.h): a per-instance override of Active
Pattern/Grid Mode/Swing (sentinel `-1` = inherit `SceneGlobal`), resolved in `Router::sendSceneToInstance` before
CC encoding. `Scene::global` is now correctly understood as a *default*, with `instanceOverrides` for whichever
instances should diverge from it — including deliberately running different grid modes at once, which is what
makes a genuine poly-rhythmic relationship between two instances possible at the protocol level (previously
impossible: every targeted instance was forced to the same grid mode).

**Not yet built (this is the JSON-library vision, v0.2+):** the mechanism now exists, but nothing yet *authors*
interesting combinations — no library of named, reusable, chainable presets. The plan (extending v0.2 scene
persistence and v0.4/v0.5 roles/arcs):

- **Presets are the library's unit**, not raw scenes. A preset is a named, taggable, reusable bundle of
  differentiated parameter values expressing one musical intention — e.g. `polyrhythm_AB_16v12` (instance A
  binary-16, instance B ternary-12, deliberately, for 3-against-2 tension), `unify_binary` (everyone converges
  to the same grid, a release gesture), `climax_all_active` (every role in foreground). Sections in a blueprint
  reference presets by id rather than inlining parameter values, so the library is genuinely reusable across
  pieces, not copy-pasted per song.
- **Presets should be role-addressed, not instance-id-addressed.** A preset written against `anchor`/`motif`/
  `counterpoint` roles (see the role-presets idea below) applies regardless of which concrete instance ids are
  registered in a given project — resolved to actual instance ids at apply-time via each `Instance::role`. Writing
  presets against literal ids like `mpl_1` would make the library non-portable between projects/sessions.
- **Preset categories, not one flat list:** role presets (default behavior for a musical function), arc presets
  (how a parameter moves over a section), and *rhythmic-relationship* presets (joint choices across two or more
  instances — grid-mode contrast, rotation offsets chosen together for a specific groove, not independently per
  instance) are different things and should be different JSON object types in the library, even though they all
  ultimately resolve to the same `SceneInstanceOverride`/`ScenePattern`/`Mutation` primitives underneath.

This section supersedes the flat assumption in the current [scene_format_v0_1.md](scene_format_v0_1.md) draft that
a scene's `targets` all receive identical `global` values — that doc still describes the v0.1 struct shape
faithfully (now including `instanceOverrides`) but doesn't yet describe the preset-library layer above it; that's
v0.2+ work, tracked in [technical_spec_checklist.md](technical_spec_checklist.md).

## Current Architecture ↔ Roadmap Concepts

| Roadmap concept | Current code | Status |
|---|---|---|
| Launcher instance / channel targeting | [`model/Instance.h`](../src/model/Instance.h), [`routing/InstanceRegistry`](../src/routing/InstanceRegistry.h) | Implemented |
| CC output, per-instance channel targeting | [`midi/CCDispatcher`](../src/midi/CCDispatcher.h), [`midi/CCMapping.h`](../src/midi/CCMapping.h), [`routing/Router`](../src/routing/Router.h) | Implemented — see [routing_policy_v0_1.md](routing_policy_v0_1.md) |
| Scene = coordinated parameter snapshot | [`model/Scene.h`](../src/model/Scene.h) | Implemented (flat, single-scene-at-a-time; no persistence yet) |
| Mutation = one targeted parameter change | [`model/Mutation.h`](../src/model/Mutation.h) | Implemented, routed directly (no budget/governor gate yet) |
| Bar-quantized timing | [`scheduling/Scheduler`](../src/scheduling/Scheduler.h), playhead wiring in [`plugin/PluginProcessor.cpp`](../src/plugin/PluginProcessor.cpp) | Scaffolded — nothing schedules events yet, only manual UI-triggered sends work today |
| Musical role per layer (anchor/motif/counterpoint/…) | `Instance::role` (free string) | Field exists, unused by any logic yet |
| Scene sets, blueprints, JSON library | `state/SceneLibrary`, `state/StateSerializer`, `util/JsonHelpers` | Empty stubs |
| Cognitive-load governor, mutation budget | `policy/PolicyEngine`, `policy/MutationPolicy`, `policy/SceneAdvancePolicy` | Empty stubs |
| Composer UI (scene chain, arcs, library) | `ui/*` | Empty stubs (current editor is a minimal engine test panel only, see [`plugin/PluginEditor.cpp`](../src/plugin/PluginEditor.cpp)) |

In short: the **wiring** (instances → channels → CC → MPL) is real and tested. The **intelligence** (roles, arcs,
budgets, blueprints, persistence) is entirely unbuilt. That ordering matches the roadmap's own recommended sequence
(§21.2): get the conductor talking to the horses before teaching it to compose.

## CC Map Discrepancy — Flag This

The roadmap's §15.1 default CC map includes CC 21 (Target Pattern), CC 23 (Editor View Mode), and CC 60-64
(Target Step editing: step/note/velocity/duration/enabled). **MPL's actual current code
(`..\Source\PluginProcessor.cpp`, `handleExternalControlCC`) only implements CC 20/22/24 and the 30s/40s/50s
per-pattern blocks** — the rest are documented intent, not live behavior. `CCMapping.h` in this project intentionally
mirrors only what MPL actually does today. Before building anything that assumes CC 21/23/60-64 work, MPL needs
v1.19.0/v1.20.0 from its own roadmap (§21.1) implemented and verified first — otherwise Composer Mastermind would be
sending CC that MPL silently ignores.

## Roadmap (adapted from the source doc's §21.2, grounded in this repo's folders)

### v0.1 — Conductor Wiring (done, verified in Bitwig 2026-08-14)
Instance registry, channel-targeted CC dispatch, scene → CC and mutation → CC encoding matching MPL's protocol,
minimal manual-trigger UI. Confirmed end-to-end against 3 real MPL instances in Bitwig: per-channel targeting,
global scene send, and single-instance mutation send all land correctly (e.g. a -5 semitone transpose mutation
encoded to CC 30 value 57, and MPL's own debug panel confirmed receiving exactly that on the intended channel).

### v0.2 — Scene Persistence (done, compiles, not yet Bitwig-verified as of 2026-08-14)
- `state/SceneLibrary` + `state/StateSerializer`: named scenes serialize to/from `juce::var` via `util/JsonHelpers`,
  per [scene_format_v0_1.md](scene_format_v0_1.md). The hierarchical layer-based scene from §10.2 is still not
  implemented — this covers the flat `Scene` struct only, as scoped.
- `state/StateSnapshotStore` bundles instances + scene library + current scene id into one JSON string, embedded
  on the existing APVTS XML in `getStateInformation`/`setStateInformation` — no separate APVTS parameters were
  added; Composer Mastermind stays purely CC-driven and UI/JSON-driven, as it already was.
- Editor UI: save/load/remove named scenes, export/import the whole snapshot to a `.json` file.
- **Still needs a live Bitwig test**: add instances + save a scene, save the Bitwig project, reload it (or
  remove/re-add the plugin instance), confirm instances and the scene library come back. This is the actual proof
  the persistence round-trips correctly, not just that it compiles.

### v0.3 — Scene Chain & Bar-Quantized Advancement (done, compiles, not yet Bitwig-verified as of 2026-08-15)
- `policy/SceneAdvancePolicy::shouldAdvance` — pure, stateless decision function (`Scene::nextSceneId` +
  `durationBars` vs. elapsed bar-ticks). `ComposerCore` owns the actual "which bar did the active scene start on"
  state and performs the switch + immediate `Router::routeScene` call from `processBar`. Ended up **not** routed
  through `scheduling/Scheduler` as originally sketched — chain advancement needs to know *when the current thing
  started*, which the generic due-event queue doesn't model; `Scheduler` stays reserved for future one-off
  scheduled sends and remains otherwise inert. Full reasoning in
  [scheduling_policy_v0_1.md](scheduling_policy_v0_1.md).
- Any scene set as current (manually via the editor, or by the chain itself) is chain-eligible if it has a
  `nextSceneId` — no separate "start the chain" action needed. Editor UI: Duration Bars slider + Next Scene combo
  in the Global Scene panel, so "Send Scene Now" doubles as a chain test without needing to save to the library
  first.
- Transport stop/restart mid-scene is handled (`ComposerCore::notifyTransportReset`, re-anchors the active
  scene's elapsed-bar clock rather than counting against stale bar numbers from before the stop). Looping while
  playing turns out to need no special handling — see the scheduling doc for why.
- **Confirmed working in Bitwig (2026-08-15)**: a 3-scene chain advanced correctly during playback.

### v0.4 — Roles & Cognitive-Load Governor
- Give `Instance::role` real meaning: role presets (anchor/motif/counterpoint/…) with default activity/mutation
  ranges (§7).
- `policy/PolicyEngine` becomes the single gate all mutation routing passes through — see
  [mutation_policy_v0_1.md](mutation_policy_v0_1.md) for the change-weight and budget model (§8-9).
- `Router::routeMutation` currently applies every mutation unconditionally; this version makes that the last step
  of a pipeline, not the whole pipeline.
- **Concrete governance model to adopt (2026-08-15):** rather than a flat "max N changes per bar" counter, budget
  by category and key it to formal position and `Instance::role` — a `noveltyBudget` per section (low at
  presentation/coda, high at climax) plus explicit "preserve this parameter / this parameter may transform"
  lists per role, so the governor and role system are tied together from the start. Mined from
  `atonal_phrase_engine`'s Formal Memory Map mechanism — see
  [atonal_phrase_engine_concepts.md](atonal_phrase_engine_concepts.md).
- **New gap to close here, not deferred:** a lightweight mutation-authorization check — before a `Mutation`
  dispatches, verify it isn't touching a protected instance/role/bar combination (e.g. don't let anything mutate
  the anchor instance during a climax bar). `Router::routeMutation` currently has no such concept at all.

### v0.5 — Arcs & Blueprints
- Generic `Arc` evaluator (piecewise-linear breakpoints over bar position) reused for energy/tension/density/
  complexity (§12) — one evaluator, not bespoke logic per arc type.
- **Coherence / convergence arc (new, 2026-08-14, not in the source roadmap's §12 list):** how correlated the
  instances' parameter states are with each other, independent of how much is changing or how intense it is.
  Low coherence = instances diverge, each running its own trajectory via distinct `SceneInstanceOverride` entries
  (rich, multi-voice, already validated musically — differentiated grid-mode/active-pattern overrides tested live
  in Bitwig, see v0.1 above). High coherence = instances converge toward `Scene::global`'s shared values, so a
  dense/loud moment still reads as *one* clear event rather than several independent ones — this is the technical
  shape of a climax: not more information, the same information stated by everyone at once. Mechanically this arc
  doesn't need new plumbing — it's the v0.4 governor deciding, over a section, how many `instanceOverrides` exist
  and how far they diverge from `global`, driven by this arc's value at the current bar. **Concrete formula to
  adopt (2026-08-15):** mined from `atonal_phrase_engine`'s Parameter Concordance Diagnostics — sample each
  instance's current CC-parameter state, count how many are simultaneously near shared target extremes, derive a
  scalar; separately validate, as a diagnostic pass over the scene chain, whether the scene actually marked as
  the climax turns out to be the real convergence point. See
  [atonal_phrase_engine_concepts.md](atonal_phrase_engine_concepts.md).
- **Cross-scene apex exclusivity (new, 2026-08-15):** nothing today stops an earlier scene's `instanceOverrides`
  or arc targets from prematurely reaching values reserved for a later climax scene down the `nextSceneId` chain.
  The Arc evaluator needs to know about reserved ceiling values downstream and clamp earlier scenes accordingly —
  a cross-scene constraint, not a per-scene one. Mined from the same source project's Arrival Reservation /
  Global Apex Exclusivity mechanisms (see the concepts doc linked above) — without this, an earlier scene can
  silently spend the climax's peak values before the climax scene itself plays.
- Blueprint JSON (§11, §18) loads a full section list, each section setting foreground/support/background layers
  and a mutation budget, consumed by the v0.4 governor.

### v0.6 — Motivic Variation Engine
- Pass-based variation (§13.2) — this is the point where step-level mutation matters, which depends on MPL's
  v1.20.0 safe external step mutation existing first (see CC map discrepancy above).

### v1.0 — Narrative Blueprint Composer
- Full loop: JSON blueprint library, Save/Recall presets, mature UI exposing arcs/roles/budgets directly
  (§19-20.2), not just the current debug panel. This is the source roadmap's endpoint — a blueprint *player*.

### v1.1 — Generative Blueprint Proposal
- The scope commitment described above ("Player vs. Composer"): assemble candidate blueprints from role/arc
  presets rather than requiring a fully hand-authored one, subject to the same governor/budget rules, always
  inspectable and editable before playback. This is Composer Mastermind's actual endpoint, beyond what the
  source roadmap itself scopes.

## New Ideas (beyond the source roadmap)

1. **Instance capability negotiation.** `CCMapping::kMaxPatterns = 3` and the 30/40/50 CC blocks hard-code "MPL has
   exactly 3 patterns." If a future MPL version changes that, every Composer Mastermind instance silently
   miscalculates CC values. Consider letting `Instance` optionally declare capabilities (`numPatterns`,
   `hasStepMutation`, `hasTargetStep`) so routing adapts instead of assuming a fixed protocol version forever.

2. **Change-budget ledger as infrastructure, not policy.** Rather than leaving §8.3's "no more than 1 major change
   per bar" as a rule blueprint authors have to respect by hand, make it a runtime object
   (`policy/PolicyEngine`) that every mutation — scheduled *or* manually triggered from the UI — passes through.
   That way the cognitive-load governor can't be bypassed by a stray manual "Send Mutation" click, and the same
   enforcement code backs both the debug UI and the real scene-chain playback.

3. **Sent-CC debug mirror.** MPL's own debug panel already tracks "last CC received" (`getDebugLastCCNumber/
   Value/Channel/Accepted`). Composer Mastermind's editor could keep a small ring buffer of the last N CC messages
   *sent* per instance and display it — giving a sent/received pair across the two plugins' UIs for verifying the
   whole pipeline without opening a MIDI monitor.

4. **Arc curves as data, not code.** Directly adopts the roadmap's JSON breakpoint format (§18.2) but as the *only*
   arc mechanism from day one, rather than growing bespoke per-arc logic later — keeps `PolicyEngine` from
   accumulating an `if (arcType == ...)` ladder as new arc types get added.

5. **Role presets pre-fill mutation budgets.** Since `Instance::role` already exists as a free string, ship a small
   built-in table (anchor/motif/counterpoint/texture/pulse/accent/drone/response, per §23.6) that pre-fills
   sensible default activity/mutation ranges for that role — so a blueprint author assigns a role and gets
   reasonable behavior immediately, only overriding specifics when they want to.

## Design Principles (carried over from the source roadmap, §23)

- Don't clutter MPL — anything about form, arrangement, or multi-instance coordination belongs here, not there.
- Prefer MIDI CC + channel targeting over host-specific modulation tricks, to stay DAW-portable.
- One clear change at a time; randomness must be *intentional*, constrained by section/role/budget, not raw chance.
- Every layer has a role; roles should behave differently, not identically.
- Departure only means something if there's a recognizable return.
