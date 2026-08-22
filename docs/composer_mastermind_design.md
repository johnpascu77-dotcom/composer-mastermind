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

**Role presets built (2026-08-17), the other two categories not yet:** `model/Preset.h`'s `RolePreset` +
`state/PresetLibrary` + `policy/PresetResolver` + the new **Presets** tab (`ui/PresetLibraryView`) implement the
first bullet below for the *role-preset* category only — see v1.1 further down for the full status. The plan
(extending v0.2 scene persistence and v0.4/v0.5 roles/arcs):

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

### v0.4 — Roles & Cognitive-Load Governor (done, confirmed working live in Bitwig 2026-08-16)
- `Instance::role` now has real meaning: `policy/MutationPolicy::budgetForRole` gives anchor/motif/counterpoint
  each a per-bar minor/medium/major mutation allowance (§7), plus a system-wide "max 1 major mutation per bar"
  cap independent of role. Unrecognized/empty roles stay unrestricted — governance is opt-in per instance, not a
  silent behavior change for instances set up before this milestone existed.
- `policy/PolicyEngine::authorize` is the single gate — added *inside* `Router::routeMutation` rather than
  requiring every caller to remember to check first, so the scene chain, `Scene::mutations`, and the manual UI
  "Send Mutation" button all pass through it automatically, none of them bypassable. `routeMutation` now returns
  `bool` (dispatched vs. blocked) so the UI can actually show when the governor did something.
- Change-weight classification (`MutationPolicy::classifyWeight`) implemented per
  [mutation_policy_v0_1.md](mutation_policy_v0_1.md) — inversion/octave-transpose = major, smaller transpose/
  length = medium, subtle rotation = minor.
- **What shipped vs. what was scoped:** the "noveltyBudget keyed to formal position" idea from
  `atonal_phrase_engine`'s Formal Memory Map (mined 2026-08-15) needs section/arc awareness that doesn't exist
  until v0.5 — a flat per-bar-reset budget can't express "rare" as anything other than "0 per bar," which is
  what `anchor`'s medium/major budget is set to as the closest available approximation for now. Revisit once
  arcs can modulate a budget over a section instead of resetting flatly every bar. Full reasoning and the exact
  budget table are in [mutation_policy_v0_1.md](mutation_policy_v0_1.md) — read that for the numbers, not this
  bullet.
- **Explicitly not done, correctly deferred:** the "protected instance/role/bar combination" authorization
  gap flagged 2026-08-15 turned out to collapse into the same mechanism once built — an anchor instance's 0
  medium/major budget *is* its protection, there's no separate "protected zone" concept needed at this scope.
  A literal "protected during climax bar" rule still needs v0.5's arc/section awareness to know what a climax
  bar even is.
- `ComposerCore::getCurrentBar()` (new, atomic) gives UI-thread actions a bar number to authorize against, since
  message-thread button clicks aren't otherwise aware of "now."
- Editor UI: Role dropdown added to the existing Instances row (Unrestricted/Anchor/Motif/Counterpoint, default
  Unrestricted) rather than a new row, respecting the height-ceiling note from v1.0's checklist entry.

### v0.5 — Arcs & Blueprints (foundation confirmed working live in Bitwig 2026-08-16; Blueprint JSON and apex-exclusivity clamping still pending)

**Built this pass — infrastructure, not yet consumed by a Blueprint (nothing authors breakpoints/sections yet):**

- `policy/Arc` — generic piecewise-linear breakpoint evaluator (`ArcBreakpoint{bar,value}` →
  `ArcSample{value,delta,distanceToNextBreakpoint}`), reused for any named dimension rather than bespoke logic
  per type, per the original plan. Unconsumed for now, same as `Scheduler` sat unused for two milestones before
  `SceneAdvancePolicy` used it — this is here so Blueprint JSON has something to plug into rather than needing
  to invent it later.
- `routing/InstanceStateTracker` (new, not originally scoped as its own item — turned out to be a genuine
  prerequisite): nothing previously remembered what CC value was actually last sent to each instance. `Router`
  now records it after every successful send. Needed by anything that measures instances against each other,
  not just coherence.
- **Coherence / convergence — implemented as a live diagnostic, not yet as an arc.** `policy/CoherenceEvaluator`
  gives a concrete 0..1 score: averages grid-mode agreement, active-pattern agreement, and swing closeness across
  currently-registered instances, adapted from `atonal_phrase_engine`'s Parameter Concordance Diagnostics (see
  [atonal_phrase_engine_concepts.md](atonal_phrase_engine_concepts.md)). `ComposerCore::getCurrentCoherence()`
  exposes it; the editor shows it as a live-updating line in the instance list. **Scope note:** only covers the
  three instance-level dimensions (`SceneInstanceOverride`'s territory) — per-pattern transpose/rotation/length/
  inversion (`Mutation`'s territory) isn't folded into the score yet, so sending mutations updates tracked state
  but won't move the displayed number. This is a measurement of what's actually happening, not yet a *target*
  a scene can author ("this section should reach coherence 0.9") — that direction-setting half is what turns it
  into a true arc, and needs Blueprint JSON's section concept to mean anything (a coherence *target* only makes
  sense attached to a section of the piece, which doesn't exist as a concept yet).
- **Explicitly still not done:**
  - **Cross-scene apex exclusivity (the clamping half)** — built 2026-08-17 as its own v1.0 slice
    (`model/Blueprint.h`'s `ReservedValue`, `Router::ReservedValueChecker`, `ComposerCore::
    isValueReservedByLaterSection`) — see v1.0 below. Note this ended up scoped to per-*parameter* value
    reservation (a mutation's resulting transpose/rotation/length/inversion value), not the coherence arc's
    *target* values the original phrasing here anticipated — coherence-as-target still isn't built, only
    coherence-as-diagnostic (see above).
  - **Blueprint JSON** (§11, §18) — sections, foreground/support/background, per-section mutation budget. Built
    2026-08-16 as its own v1.0 slice (schema, persistence, and the Sections authoring UI) — see v1.0 below.
    `PolicyEngine` consuming `SectionBudgetOverride` is what turns this from authored data into the clamping
    half of apex exclusivity; not wired yet.

### v0.6 — Motivic Variation Engine
- **MPL-side blocker cleared 2026-08-18** (CC 21/60-64, "Option A"), scoped further 2026-08-19 into a real
  motif/rule engine, and **built 2026-08-20, confirmed working live same day**: `policy/MotifEngine` fires per
  blueprint section boundary, picks a `MotifPreset` (new preset category — relative pitch/rhythm cell) by
  archetype tag, decides which instances to touch by role, applies a deterministic transform, and writes via the
  CC 21/60-64 encoders in `CCMapping.h` — informed by real cross-instance pattern state via the IPC awareness
  channel (see below), not assumed state. Global Nudge/Phrase application mode, user's explicit request.
- **Pass-based variation sequencing built 2026-08-20** (§13.2, the "fires once and goes static" gap this section
  used to flag): a still-active section now fires an additional motif pass every ~4 bars (`ComposerCore::
  kPassIntervalBars`/`fireMotifPassIfDue`), not just once at its start. Each pass cycles through the full
  transform vocabulary (rotate/invert/retrograde on top of the archetype's own base transform,
  `MotifEngine::transformForPass`) and rotates which steps get touched, so a long section keeps developing. Built
  and installed clean; not yet live-tested. See `docs/technical_spec_checklist.md`'s "Motif/rule engine" section
  for the full mechanism and remaining honestly-scoped limitations (pattern index 0 only, no budget gating yet,
  one preset per archetype).
- **MCP bridge grew a compose layer, and became the actual mastermind (2026-08-20/21):** past read/write actions
  (resync, mutation, scene set, etc.), the bridge gained `createScene`/`createBlueprint`/`createMotifPreset`/
  `setArc` — reusing the exact save/load deserializers and validators, so a bridge-authored scene is validated
  identically to a hand-authored one. User's own framing: "would you be the actual mastermind composer, showing
  me what our tools can make?" — answered by authoring and pushing a complete original piece ("Vers la flamme,"
  56 bars, Scriabin-referenced) entirely through the bridge, then live-testing it across several rounds. That
  testing surfaced two real bugs (a preset tag-collision letting an old test preset shadow the real composition's
  presets; a Length-window interaction leaving a shrunk pattern's audible window silent) and one real gap in the
  engine itself: it could only ever *nudge* existing content, never author it.
- **"Preparatory phase" — `MotifEngine::stampMotifForSection` (2026-08-21, user's own framing):** a section's
  real seed content is now authored once at section entry (including "presentation," previously a total no-op),
  rather than the engine forever nudging whatever sparse steps happened to survive from manual programming.
- **Step-content writes moved off MIDI CC entirely, onto direct IPC (2026-08-21):** live-testing the stamp (many
  steps written per instance at once, more than anything before) exposed that MPL's CC 60-64 protocol commits
  through a once-per-block parameter poll built for one human turning one knob at a time — several step-edits
  landing in the same block silently collapse to the last one. Fixed properly, not paced around: `PatternSyncServer`
  (the same duplex local-socket connection every resync already uses) gained `writeStep`/`writeFullPattern`
  message types, committing directly into MPL's real pattern storage with no MIDI, no parameter polling, and no
  burst-timing risk at all. Confirmed live: stamped content now matches exactly between the Awareness cache and a
  real MPL resync. Full mechanism in `docs/technical_spec_checklist.md`'s "v0.6.1" section.
- **Replay-drift fix, Presentation gentle transpose, "Prime for Playback," and P1/P2/P3 phrase-chaining, all
  built and confirmed live in one continuous arc (2026-08-21):** a replay from bar 1 no longer inherits the
  previous playthrough's pitch/parameter drift; Presentation now walks a small, gentle register drift instead of
  staying frozen or fully mutating; a new **Prime for Playback** button runs a section's full entry sequence
  while transport is stopped (user's own idea — these writes land instantly regardless of transport state, so
  there's no reason setup has to wait for playback, and priming while genuinely stopped sidesteps the exact kind
  of MPL-engine race every CC-burst bug this session turned out to be some version of); and Build/Peak/Release now
  cycle an instance through three related, seeded patterns (base/rotated/inverted) every 8 bars instead of
  nudging one pattern forever — real phrase structure. User's verdict: "I really think this was a missing link."
  Full detail in `docs/technical_spec_checklist.md`'s "v0.6.1" section.

### v1.0 — Narrative Blueprint Composer (full loop + apex exclusivity confirmed working live in Bitwig 2026-08-17; layerRole consumption built same day, not yet Bitwig-verified)
- Full loop: JSON blueprint library, Save/Recall presets, mature UI exposing arcs/roles/budgets directly
  (§19-20.2), not just the current debug panel. This is the source roadmap's endpoint — a blueprint *player*.
- **Tabbed UI shell built:** the single ~860px scrolling panel is gone. `ui/EditorView` now hosts four tabs
  (Instances / Scenes / Mutations / Blueprint) via `juce::TabbedComponent`, each tab's content moved into its own
  `Component` (`ui/InstanceView`, `ui/SceneListComponent`, `ui/DebugPanel` — no longer empty stubs). A shared
  status bar and a 300ms refresh timer keep every tab in sync with state changed from any other tab, without
  cross-page callback wiring. Full detail in [technical_spec_checklist.md](technical_spec_checklist.md)'s v1.0
  section.
- **Arc authoring is a visual, directly-editable graph, not a form:** `ui/BlueprintView` is now that graph — a
  combo box picks which of the 5 `ArcSet` arcs is being edited, its points render as draggable circles on a
  bar-vs-value grid (the other 4 arcs dim into the background for context), and dragging/double-clicking/
  right-clicking a point moves/adds/removes it directly, no bar/value form involved. This *is* the authoring
  interface for Blueprint JSON's arcs, not an extra feature alongside it: `policy/Arc`'s breakpoint model is
  what this page reads and writes, via a new `Arc::getBreakpoints()` accessor. Confirmed working live in
  Bitwig 2026-08-16.
- **Blueprint JSON schema + authoring UI built:** `model/Blueprint.h` defines the sections/layer-role/
  budget-override schema described below; `state/BlueprintLibrary` + `StateSerializer` + `StateSnapshotStore`
  persist it the same way scenes already work; the Blueprint tab's new "Sections" inner tab
  (`ui/BlueprintSectionsView`, alongside the arc graph editor now living in `ui/ArcGraphView`) authors it.
  Confirmed working live in Bitwig (2026-08-17).
- **`SectionBudgetOverride` now consumed by the policy gate:** `Router` holds an injected
  `BudgetOverrideResolver`; `ComposerCore::resolveSectionBudgetOverride` finds the current blueprint's section
  covering `currentBar` and hands its per-role override to `PolicyEngine::authorize`/`MutationPolicy::
  tryConsume` in place of the static `budgetForRole` table. Both mutation dispatch paths (scene-chain
  `Scene::mutations`, and `DebugPanel`'s manual test button) get it for free since both already flow through
  `Router::routeMutation`. Confirmed working live in Bitwig (2026-08-17).
- **The "full loop" — sections now drive playback themselves:** `ComposerCore::advanceBlueprintIfNeeded` runs
  every bar tick alongside the existing scene chain, finds the active blueprint's section covering the current
  bar, and routes that section's scene the moment the active section changes — deliberately decoupled from
  (not replacing) the older `Scene::nextSceneId`/`durationBars` chain, so hand-authored chains and manual "Send
  Scene Now" still work unchanged even while a blueprint is active. Any blueprint with sections now starts
  driving playback automatically as soon as it's saved or loaded (`setCurrentBlueprint` is what marks it
  current) — no separate activation step. The Sections tab shows a live "now playing" line. Confirmed working
  live in Bitwig (2026-08-17): a 2-section blueprint switched sections and routed the new scene automatically
  as the transport crossed the boundary bar.
- **Cross-scene apex exclusivity (the clamping half) built:** `model/Blueprint.h`'s `ReservedValue`
  (targetInstance/patternIndex/type/value) marks a resulting mutation value as belonging to a later section.
  `Router` holds an injected `ReservedValueChecker`, mirroring the budget-override resolver's pattern exactly;
  `ComposerCore::isValueReservedByLaterSection` implements it, blocking a mutation in `Router::routeMutation`
  if a strictly-later section reserves the value it would reach. Same scope as the budget-override wiring:
  only `routeMutation` is gated, not `routeScene`'s direct pattern/global sends. The section that owns a
  reserved value is always free to reach it — the check never fires during or after that section itself.
  Confirmed working live in Bitwig (2026-08-17). The Sections tab's form grew past the tab's
  available height once this was added on top of layer roles + budget overrides, so `ui/BlueprintSectionsView`
  is now a thin `juce::Viewport` host around the actual controls (`ui/BlueprintSectionsContent`, new).
- **`SectionLayerRole` consumed:** `ComposerCore::applyLayerRoleOverrides` applies each `background`-tagged
  instance as `SceneInstanceOverride(activePattern=0)` on a copy of the section's scene right before routing
  it, unless that scene already authored an explicit override for the instance. `foreground`/`support` get no
  forced behavior - MPL's CC protocol has no volume/velocity dimension to differentiate "prominent" from
  "supporting" at, only engaged-vs-stopped, so only `background` gets real teeth. This closes out Blueprint
  JSON's last authoring-only field. Compiled and installed clean; not yet live-tested in Bitwig.
  Full detail in [technical_spec_checklist.md](technical_spec_checklist.md)'s v1.0 section and
  [mutation_policy_v0_1.md](mutation_policy_v0_1.md).

### v1.1 — Generative Blueprint Proposal (all 3 preset categories built + the generative assembly layer itself built 2026-08-18, not yet Bitwig-verified)
- The scope commitment described above ("Player vs. Composer"): assemble candidate blueprints from role/arc
  presets rather than requiring a fully hand-authored one, subject to the same governor/budget rules, always
  inspectable and editable before playback. This is Composer Mastermind's actual endpoint, beyond what the
  source roadmap itself scopes.
- **Preset library, role presets only, built:** `model/Preset.h`'s `RolePreset` (id, name, `targetRole`, tags,
  instance-level `activePattern`/`gridMode`/`swing` matching `SceneInstanceOverride`'s existing -1/-1.0f
  "inherit" sentinel) is the first of the three preset categories from "'Send To All' Is One Preset Among Many"
  above. `state/PresetLibrary` persists it exactly like `SceneLibrary`/`BlueprintLibrary`; `policy/
  PresetResolver::applyRolePreset` (pure, JUCE-free) resolves a role-addressed preset into concrete
  `SceneInstanceOverride` entries for whichever currently-registered instances actually hold that role - the
  "resolved to actual instance ids at apply-time via each `Instance::role`" mechanism from the plan above,
  built for real rather than staying aspirational. The new **Presets** tab (`ui/PresetLibraryView`) builds/
  saves/loads/removes presets and applies a saved preset directly onto a saved scene in `SceneLibrary`,
  persisting the resolved result - a one-shot authoring-time "stamp," not a live reference re-resolved at
  playback, matching "always inspectable and editable before playback" above rather than an opaque runtime
  dependency. Confirmed working live in Bitwig (2026-08-17): a swing-75 preset applied to a saved scene
  correctly delivered CC 24 value 127 (=75%) to the matching instance on `Load and Send`, verified via MPL's
  own "Last CC" debug readout.
- **Rhythmic-relationship presets built:** `model/Preset.h`'s `RhythmicRelationshipPreset` is the second
  category — a joint, deliberately-correlated choice across 2+ roles applied together in one call (e.g. role
  `motif` Binary while role `counterpoint` Ternary, chosen together on purpose for a specific groove, not two
  independent `RolePreset` applications that happen to coincide). `Validation` requires at least 2 role slots,
  since a single-slot "relationship" isn't one. `policy/PresetResolver::applyRhythmicRelationshipPreset`
  resolves each slot the same way `applyRolePreset` does (both share a private merge helper now). The Presets
  tab gained a second authoring section for it; the combined tab outgrew its available height, so `ui/
  PresetLibraryView` is now a `juce::Viewport` host (same fix as the Sections tab) around new `ui/
  PresetLibraryContent`. Confirmed working live in Bitwig (2026-08-17): a 2-slot preset (`motif`=Binary,
  `counterpoint`=Ternary) applied to a saved scene and landed correctly on both instances together.
- **Arc presets built — the third and last category:** `model/Preset.h`'s `ArcPreset` is a reusable
  *normalized* shape (`ArcPresetBreakpoint{position 0..1, value 0..1}`) rather than an absolute-bar curve -
  "swell"/"plateau"/"arch" stampable onto any bar range without redrawing it, and deliberately decoupled from
  which of `ArcSet`'s 5 dimensions it targets (chosen at apply-time, so one shape works across energy, tension,
  or any other). `policy/PresetResolver::applyArcPreset` scales the points into the chosen range and replaces
  any existing breakpoints strictly inside it (a preset deterministically owns its scope, same as the other two
  categories), writing straight into the same `ArcSet` the Arcs tab's graph editor reads/writes - the result
  shows up there immediately, no separate consumption path needed. `ArcSet::setArc` silently no-ops on an
  unrecognized dimension name, confirmed by reading it before writing the resolver, so the resolver validates
  against `ArcSet::getArcNames()` rather than trusting the caller blindly. Compiled and installed clean; not
  yet live-tested in Bitwig. All three preset categories the design doc named are now built.
- **Per-pattern preset fields still not built:** transpose/rotation/length/inversion aren't included in any of
  the three categories - `ScenePattern` has no "leave this field untouched" sentinel the way
  `SceneInstanceOverride` already does.
- **The generative assembly layer itself, built:** `policy/BlueprintGenerator::generate` is the actual answer
  to "assemble candidate blueprints from role/arc presets" above - deterministic and explainable throughout, on
  purpose (the design principles below already warn against undisciplined randomness). Design settled through
  direct back-and-forth rather than assumed: **one arc drives the section structure** (its consecutive
  breakpoints become section boundaries - draw the shape once, structure falls out of it, no separate
  boundary-authoring step); each section is classified into one of **four archetypes** purely from its
  breakpoint values - `presentation`/`build`/`peak`/`release`, mirroring the `presentation`/
  `local_climax_approach`/`global_climax`/`coda_echo_aftermath` vocabulary mined from `atonal_phrase_engine`
  back in v0.4 and finally implementable now that roles/budgets/arcs/presets all exist; each archetype maps to
  a **small fixed rule** reusing every mechanism this session built (`peak` foregrounds every instance and
  reserves each one's own extreme values - apex exclusivity, automatically; `release` backgrounds
  `counterpoint`; `build` loosens `motif`/`counterpoint` budgets); **presets are selected by tag**, not
  invented - the `tags` field every preset type has carried since it first shipped, and that nothing had ever
  read until now, is what a section's archetype name gets matched against; the **other 4 arc dimensions are
  baked from the actual generated decisions** (density from the real foreground/background split, not a guess)
  rather than left blank or hand-drawn separately.
  - **Two design decisions locked in directly, both honored exactly:** (1) a section still sends its scene once
    at the boundary, unchanged from the existing mechanism - no new mid-section CC re-sampling was built, and
    "morphing" was scoped explicitly to the arc curves themselves (already free via `Arc::evaluate`'s
    interpolation) rather than pretending categorical CC parameters like grid mode can glide continuously.
    (2) **preview-before-commit**, chosen as the cheapest correct implementation: `ui/ArcGraphView` now takes
    any `ArcSet&` rather than always reaching into the live one, so the exact same drag/add/delete interactions
    work unmodified on a throwaway candidate - no new drawing code needed, just a different object handed to an
    existing one. `ArcSet` needed an explicit copy constructor/assignment for this (a `std::mutex` member
    deletes the implicit ones; each side locks its own).
  - New **Generate** tab (`ui/GenerateView`, 6th outer tab): pick a base scene + driving arc, **Generate**
    builds the whole proposal in memory only - nothing touches `BlueprintLibrary`/`SceneLibrary`/the live
    `ArcSet` - and shows a section/archetype summary alongside the candidate curves in the reused graph.
    **Commit** saves it for real (and activates it, matching every other "Save" in this app); **Discard**
    throws it away untouched.
  - Compiled and installed clean; not yet live-tested in Bitwig.
  - **A user-suggested emergent-complexity technique, needing no new code:** since each Composer Mastermind
    instance owns its own independent `ComposerCore` (its own registry, blueprint library, section timing),
    running two instances with deliberately mismatched section lengths already produces overlapping,
    non-aligned transitions between them - "morphing" at the ensemble level, for free, from the architecture
    that already exists. Untested but worth trying.
  - **Explicitly out of scope:** per-pattern preset fields (see above) mean generated sections only
    differentiate instances on activePattern/gridMode/swing. Multi-arc blending (letting more than one arc
    influence section boundaries) was considered and deliberately rejected in favor of one clean driving arc.

### v1.2 — Score-Timeline UI & Real Arc Consumers (Track A + Track B mapping complete — 2026-08-22)

**Track A status: complete, confirmed working live.** Phase 0 (Expert-button shell + graph/preset round-trip
fix), Phase 1 (static Overlay/Lane piano roll), and all of Phase 2's Track A items (Setup mode, Locked Notes,
Live sync's playhead reconstruction, drag-out MIDI export) are built, installed, and live-confirmed — see
[score_timeline_ui_concept.md](score_timeline_ui_concept.md) for exact mechanisms and named limitations (the
live playhead's phase-anchor edge case; the recorder buffer captures a playhead-position reconstruction, not yet
the originally-envisioned full scrolling-timeline view).

**Track B status: complete except Coherence (deliberately deferred).** `Router::routeContinuousTranspose`/
`routeContinuousSwing` are the continuous-automation dispatch mode (bypasses `PolicyEngine` entirely, matching
`routeScene`'s ungated pattern). `ComposerCore::applyContinuousMelodicCurve` (Energy→amplitude,
Tension→register-center) and `applyContinuousSwing` (Density→Swing) run every bar a section is active, replacing
the old flat-constant melodic curve and Presentation's special case, and Swing's old 100%-static-per-scene
behavior. Complexity drives phrase-chain target banding (`firePhraseChainIfDue`, threshold-crossing, replacing
the old flat 8-bar metronome) and mutation budget ceiling scaling (`resolveSectionBudgetOverride`, 1x-2x on
whatever base budget applies). The swing/notation quantization rule (drag-out export's "for notation" variant
snapping to the two clean straight/triplet ratios) is built, replacing an interim same-day simplification that
would have gone stale the moment Swing became continuous. The richer phrase-role vocabulary is also built —
`MotifEngine::PhraseRole`'s 5-way set (Base/Rotated/Inverted/Retrograde/InvertedRetrograde), with
`firePhraseChainIfDue` re-stamping whichever physical slot is next due rather than trusting a fixed P1/P2/P3
seed. See [arc_dimension_mapping_concept.md](arc_dimension_mapping_concept.md) for exact formulas. Phase 3's two
small independents (novelty-aware preset selection, milestone-snapshot safety net) remain untouched — the only
items left in the whole v1.2 phase besides Coherence.

A full design pass, not yet touched in code — see [score_timeline_ui_concept.md](score_timeline_ui_concept.md)
and [arc_dimension_mapping_concept.md](arc_dimension_mapping_concept.md) for the actual design content; this
section is the implementation order only. Two previously-separate threads converged into one phase: a unified,
score-like piano-roll UI (replacing the "expert mode" numbers/sliders as the primary surface, and the three
floating MPL windows needed today to see pattern content), and giving the 5 arc dimensions
(energy/tension/density/complexity/coherence) real playback-time consumers instead of mostly-decorative baselines.

**Explicit constraint carried through every phase below, user's own requirement (2026-08-22):** the existing
tabbed UI (`ui/EditorView`'s Instances/Scenes/Mutations/Blueprint/Presets/Generate/Sections/Modulators/Activity
Log tabs) must remain fully available, unchanged, never at risk of regression — reachable via an "Expert" button
from the new UI, not replaced by it.

- **Phase 0 — Safety net + housekeeping.** The new score-timeline UI becomes the plugin's default top-level view;
  an "Expert" button swaps to the existing `TabbedComponent`, completely unchanged — built first so every later
  phase adds new content inside the new shell rather than ever touching the working tabs. Bundled alongside: the
  graph↔preset-library round-trip fix (drag a point on `ui/ArcGraphView`, save it as an `ArcPreset` directly,
  instead of re-entering breakpoints by hand in `ui/PresetLibraryContent`'s separate sliders) — small, contained,
  useful immediately as the primary curve-authoring surface before Setup mode exists.
- **Phase 1 — Piano-roll foundation, read-only and static.** Overlay + Lane view rendering from
  `InstancePatternCache`'s last-dumped snapshot only, no playhead or live reconstruction yet — proves the
  rendering approach (per-instance color, C3=60 note names, grid) in isolation first.
- **Phase 2 — two independent tracks, different files, parallelizable:**
  - **Track A (piano-roll):** Setup mode (editable lane view, writing via the already-built
    `PatternSyncServer::sendWriteFullPattern`) before Live sync, since it directly replaces the 3-floating-MPL-
    windows pain point and needs no new architecture beyond Phase 1 — then locked notes (small addition), then
    Live sync (playhead + recorder buffer + reconstruction, the heavier piece), then Drag-out MIDI export
    (stopped-transport only, per-track for voice separation, tempo map, section/archetype labels as MIDI
    markers, as-performed/quantized-for-notation toggle, deliberate-drag gesture threshold), which depends on the
    recorder buffer existing.
  - **Track B (arc dimensions):** the continuous-automation dispatch path first (third mode alongside
    `routeScene`/`routeMutation`, bypassing `PolicyEngine`'s mutation budget — foundational, everything else in
    this track needs it) — then Energy→melodic-curve-amplitude and Tension→register-center together (same code
    area in `ComposerCore.cpp`) — then Complexity→phrase-chain threshold-crossing and
    Complexity→mutation-budget-ceiling (discrete-target work, independent of the continuous path) — then
    Density→swing (needs the continuous path) and the swing/notation quantization rule (needs Density→swing, and
    needs Track A's drag-out export to land it in) — then the richer phrase-role vocabulary
    (Stellarizer-inspired), lowest priority in this track.
- **Phase 3 — small independents, slot in anytime:** novelty-aware preset/motif selection (deprioritize whatever
  preset the immediately preceding section already used); the milestone-snapshot safety net (a bounded,
  session-local, browsable/restorable checkpoint ring buffer, distinct from Undo/Redo, from `StateSnapshotStore`,
  and from the permanent preset/blueprint libraries).

**Deliberately not scheduled into this phase:** curve-based blueprint/preset authoring (showing a loaded
blueprint's "intended shape" as an editable curve when some pieces have no `ArcSet` data at all) still needs its
own design conversation before any UI work — the data-model question (derive a curve from section/archetype
sequence, vs. making arc authorship mandatory going forward) isn't resolved. Coherence-as-target is flagged as a
bigger lift than the other 4 dimensions and deliberately left unmapped this pass.

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

6. **Milestone-snapshot safety net (2026-08-22, inspired by a commercial JUCE plugin's "Recordings" atlas). Built
   2026-08-22, compiled clean for both VST3 and Standalone, not yet live-tested in Bitwig.** Distinct from
   everything persistence-related that already exists: `Undo`/`Redo` doesn't exist yet at all,
   `state/StateSnapshotStore` only ever holds *current* state, and saving to `BlueprintLibrary`/`SceneLibrary`/
   `PresetLibrary` is a deliberate, permanent authoring act, not a quick checkpoint. `state/MilestoneLibrary`
   is a bounded (20-entry), session-local ring buffer capturing every registered instance's tracked CC state
   (`InstanceParameterState`) plus any cached pattern content (`PatternSnapshot`), captured once per section entry
   in `ComposerCore::enterSection` (after stamping/seeding/rhythm has already run, so the snapshot reflects what
   the section actually starts out sounding like). Browsable and restorable via the new "Milestones" tab
   (`ui/MilestoneView`) — a read-only list plus a combo + Restore button. `ComposerCore::restoreMilestone` re-sends
   every captured CC (global + per-pattern) and any captured pattern content, bypassing `PolicyEngine` entirely
   (a restore, not a discrete decision — same reasoning as the continuous arc-consumer dispatch above). Not
   persisted across project save/reload, matching the "Recordings" inspiration being session-local too.

7. **Novelty-aware preset/motif selection (2026-08-22, same source). Built 2026-08-22, compiled clean for both
   VST3 and Standalone, not yet live-tested in Bitwig.** `policy/BlueprintGenerator`'s archetype→preset matching
   (see v1.1 below) picks by tag alone, with no memory of what the *immediately preceding* section already used —
   two adjacent sections of the same archetype could coincidentally land on the identical tagged preset, reading
   as a mistake rather than a deliberate callback. Fixed via a section-lifetime-frozen "avoid" id
   (`ComposerCore::avoidMotifPresetIdForSection`, carried forward from the previous section's resolved choice at
   the top of `enterSection`) distinct from the "just resolved this section" id (`currentSectionMotifPresetId`) —
   deliberately two members, not one updated immediately, since `stampMotifForSection` and
   `seedPhraseChainPatterns` both run within the same section and must agree on what to avoid without one
   invalidating the other's choice mid-section. `MotifEngine::findPresetForArchetype` deprioritizes (not
   forbids — a genuine return can be its own device) the avoided id: prefers a different tagged match, falls back
   to the avoided one only if it's the sole match.

## Design Principles (carried over from the source roadmap, §23)

- Don't clutter MPL — anything about form, arrangement, or multi-instance coordination belongs here, not there.
- Prefer MIDI CC + channel targeting over host-specific modulation tricks, to stay DAW-portable.
- One clear change at a time; randomness must be *intentional*, constrained by section/role/budget, not raw chance.
- Every layer has a role; roles should behave differently, not identically.
- Departure only means something if there's a recognizable return.
