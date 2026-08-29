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
- **Bridge grew 10 more actions, closing the Presets/Generate/Modulators gap (2026-08-23):**
  `createRolePreset`/`createRhythmicRelationshipPreset`/`createArcPreset`, `applyRolePreset`/
  `applyRhythmicRelationshipPreset`/`applyArcPreset`, `createModulatorTarget`/`getModulatorTargets`,
  `getPresets`, `generateBlueprint` (preview-then-commit, matching `ui/GenerateView`'s own flow exactly) — added
  after a computer-use attempt to drive Bitwig's own window directly hit an unresolved screen-capture wall.
  Reusing existing UI-layer logic (`PresetLibraryContent.cpp`'s apply handlers, `GenerateView.cpp`'s commit
  sequence) made this a same-session fix rather than a new subsystem. Still missing: any bridge action for direct
  pattern-step writes (Score View's core capability has zero bridge coverage), and `getArc` (no read-back for
  arc curves at all, a gap flagged since 2026-08-20 and still open).
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
- **Grid-mode-blind content placement, found and fixed (2026-08-23):** both content-authoring functions
  (`stampOnePattern`, and the per-pass nudge function) hardcoded their note-placement window and
  duration-clamping boundary to `CCMapping::kPatternSteps` (16), with no reference to the target instance's
  actual grid mode — for a Ternary instance, MPL only ever plays back the first 12 raw steps, so content authored
  against a 16-step assumption either landed proportionally wrong or was silently dropped past index 11. Fixed via
  a new `effectiveStepCount(gridMode)` helper (`MotifEngine.cpp`) reading `InstanceStateTracker`'s already-tracked
  `gridMode` (it just wasn't being consulted here), mirrored as `CCMapping::kTernaryGridSteps = 12`. Confirmed live
  via the Awareness channel. Found while building `docs/advanced_workflow_example.md`, chasing a "ternary sounds
  binary" report that turned out to have two false alarms and one real cause outside either plugin entirely (a
  Bitwig-native Arp/Chord downstream of MPL) — the distilled user-facing lesson ("same note count at different
  grid resolutions isn't a polyrhythm"; "check your whole signal chain") made it into that doc's troubleshooting
  section; the full debugging narrative lives only here, not in the user-facing doc itself.

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

### v1.3 — Absolute/Generative Section Content + Factory Reset (built 2026-08-23, confirmed live in Standalone; full Bitwig round-trip not yet verified)

Direct answer to a real gap found while clarifying JSON persistence with the user: a saved Blueprint/Scene never
carried literal note content, only CC-encodable parameters - `MotifEngine`'s stamping is generative, deriving
notes from a relative `MotifPreset` shape plus whatever's live on the instrument at stamp time, never stored
verbatim. "Load this JSON, get exactly this piece every time" (the user's own demo-song/mechanical-piano-roll
framing) genuinely wasn't possible before this. Rather than replace the generative engine, kept both: a
`BlueprintSection` can now optionally carry `capturedContent` (literal `StepSnapshot` data, captured from live MPL
state via a new "Capture Current" button on the Sections tab), and a global `ComposerCore::ContentMode`
(Generative/Absolute, Scenes tab combo) decides whether a section with captured content plays it back verbatim
instead of stamping generatively. Confirmed with the user: once an Absolute section with real content starts, it
stays **frozen for the whole section** - `enterSection` and `advanceBlueprintIfNeeded`'s per-bar continuation both
gate on the same condition, so motif passes/phrase-chaining/continuous melodic-curve/continuous-swing all skip
entirely for such a section, not just the initial stamp. A section with no captured content always behaves
generatively regardless of the global mode.

Also built the same pass: a real factory-reset action (`ComposerCore::resetToFactoryDefaults`, "New Project"
button on Instances tab) - every library had a `clear()` method already except `InstanceRegistry`/
`ModulatorTargetLibrary` (added), but none were ever wired to anything before this.

**A real bug found via live testing on the Standalone build (computer-use), not assumed away**:
`juce::NativeMessageBox::showAsync`'s callback button index is 0-based in the order passed to
`makeOptionsOkCancel`, not 1-based - the reset confirmation's "Reset" button is index 0. First cut checked
`result != 1`, so clicking Reset silently did nothing. Confirmed by reading the actual JUCE source
(`juce_NativeMessageBox_windows.cpp`'s `buttonIndex` construction) rather than guessing, fixed, rebuilt,
reconfirmed live.

**Confirmed live (Standalone/computer-use)**: reset button + dialog + real clearing across tabs; Content Mode
combo; the Captured Content row's "won't write blind" refusal when no confirmed pattern content exists yet
(matches `stampOnePattern`'s own established restraint). **Not yet verified**: a real Absolute-mode section
actually playing frozen content against a live MPL instance end-to-end - needs Bitwig (or a Standalone session
with a real MPL instance loaded), not testable in a bare Standalone session with no real instrument attached.

### v1.3 follow-up — cold-start controls mirrored into the Score View header (built + confirmed live 2026-08-23)

User's own framing of the real first-run flow: open Composer Mastermind, see the Score View (the actual first
thing anyone sees) empty, press Resync, choose Absolute or Generative, load a Blueprint - and have that whole loop
work without detouring into the Expert tab. Added one new header row to `ui/PrimaryView` (both Live and Setup
mode) mirroring four existing Expert-UI actions with zero new backend logic: **Resync All** (ports
`PatternAwarenessView::resyncAllClicked`), **Content Mode** combo (same `ComposerCore::ContentMode` the Scenes tab
already exposes), **Blueprint** combo + **Load**/**Remove** (ports `BlueprintSectionsContent`'s equivalents), and
**Prime for Playback**. One deliberate behavior addition beyond a straight port: **Load** here also calls
`primeForPlayback()` immediately - "load a Blueprint... the window gets populated" happens in one action rather
than requiring a second click, matching the user's own description of the flow. A new `refreshHeaderControls()`
keeps the row's combos live (constructor, the 60ms Live-mode timer via `refreshFromCore()`, and
`visibilityChanged()`/`updateLiveSetupVisibility()` for the outer-shell-switch staleness class already fixed once
this session for Setup mode's instance list).

**A real two-way-sync bug found via live verification, not assumed away**: `SceneListComponent`'s own Content Mode
combo (Scenes tab) only ever read `ComposerCore::getContentMode()` once, at construction - changing the mode from
the new Score View header (or from anywhere else, after the fact) never propagated back into that already-built
combo, despite `EditorView`'s 300ms timer calling `SceneListComponent::refreshAll()` unconditionally the whole
time. Fixed by re-syncing `contentModeCombo`'s selection inside `refreshAll()` itself - the natural place, since
that timer already runs regardless of which Expert tab is showing. Confirmed live: toggling Content Mode from the
Score View header updates the Scenes tab's combo within one refresh tick, and vice versa.

**Confirmed live end-to-end (Standalone/computer-use, via the MCP bridge to author a real scene+blueprint)**:
Resync All's live status count; Content Mode two-way sync (both directions); Blueprint combo populating from
`BlueprintLibrary`; Load correctly loading *and* priming in one click (status: "Loaded and primed..."); Remove
correctly clearing the library and the combo. One environment-only false alarm along the way, not a code bug:
launching the Standalone build (however it's launched - even without `&`-backgrounding) can spawn two real OS
processes, and only one binds the fixed MCP bridge port (47824) - `computer-use`'s `open_application` isn't
guaranteed to front the port-owning one, so a blueprint authored via the bridge can appear to "not exist" in the
window being screenshotted when it's actually sitting in a second, invisible process's library. Resolved by
checking `Get-NetTCPConnection -LocalPort 47824` for the true owning PID and killing the other one - worth
checking that every time this class of "the UI isn't seeing what the bridge just wrote" symptom shows up again,
before assuming a real desync bug.

### v1.2 follow-up — Retrograde/M7 protocol additions + Coherence's first consumer (2026-08-25; MPL toggles live-tested in Bitwig, Composer Mastermind's own Coherence consumer built same session, NOT yet live-tested)

MPL gained two new per-pattern toggles, own knob + own CC each, exactly like Inversion: **Retrograde** (CC
34/44/54, time-domain - reflects the loop-relative playback position before Rotation's offset, in
`getRotatedSourceStepIndex`) and **M7** (CC 35/45/55, pitch-domain - interval multiplication by 7 mod 12 on each
note's pitch class, ahead of Inversion in `applyPatternTransformsToNote`; its own inverse, same self-cancelling
character as Inversion). Combining Retrograde + Inversion gives the classic twelve-tone Retrograde-Inversion (RI)
row form for free. User live-tested both by hand in Bitwig (two independent modulators at half-note rate driving
two MPL instances) and confirmed the result: described it as what a development section would do to initial
motifs, and cited it as concrete evidence that driving these parameters through intentional curves - not just ad
hoc mutations - is the right direction. See [docs/routing_policy_v0_1.md](routing_policy_v0_1.md)'s sibling doc
for the protocol table.

Composer Mastermind mirrors both as full `Mutation` types, touching every layer Inversion already touched
(`CCMapping.h`, `Scene.h`/`ComposerState.h`'s pattern-state fields, `InstanceStateTracker::recordPattern`,
`Router::routeMutation`/`sendScenePattern`, `ComposerCore::restoreMilestone`, `MutationPolicy` weight
classification, JSON round-trip, both mutation-editing UIs). `ui/PrimaryView.cpp`'s own hand-written mirrors of
MPL's rotation/inversion math (used by the score-timeline live recorder / drag-out export) needed the identical
retrograde-reflection and M7-multiplication logic too, or the composer's own preview/export would have silently
diverged from what MPL actually plays.

**Closes part of v1.2's "Coherence deliberately deferred" gap** - not the harder direction
[arc_dimension_mapping_concept.md](arc_dimension_mapping_concept.md) originally described (an active
clamping/nudge mechanism pulling instances toward measured agreement, `policy/CoherenceEvaluator`'s diagnostic
territory, still unbuilt), but a different, simpler mapping structurally identical to Complexity's own
threshold-crossing phrase-chain consumer: `ComposerCore::applyCoherenceDivergenceIfDue` samples the live
Coherence *arc* (the `ArcSet` dimension, not the diagnostic) every bar a section is active and bands it against
two thresholds (0.35 for Retrograde, 0.15 for M7, both deliberately below every archetype's baseline coherence
value so picking an archetype alone never triggers this - only a deliberately authored/hand-curved dip does),
turning divergence on/off per instance as coherence crosses each point. Falling coherence escalates in two
stages: Retrograde alone, then M7 added on top as divergence deepens. Bypasses PolicyEngine/Router's Mutation
budget entirely, same as every other continuous/threshold-crossing arc consumer.

### v1.2 follow-up — Swing narrowed to a 3-state choice, then Shuffle corrected to 100% (built 2026-08-26)

Direct consequence of actually listening to the Coherence/Retrograde/M7 work above: user flagged that a moderate
Density-driven swing value (~20%) read as sloppy rather than musical, while the 75% ceiling was "clearly useful"
by contrast - see [arc_dimension_mapping_concept.md](arc_dimension_mapping_concept.md)'s "Swing vs. notation"
section for the full mechanism. Root cause was already half-documented there: MPL's swing range only contains two
notation-clean ratios (0% straight, 66.67% true triplet), so a continuous value spent most of its time in neither.
First fix, at the user's own suggestion: replace MPL's Swing knob (CC 24) with a genuine 3-state choice - Off /
Triplet (66.67%) / Shuffle - rather than trying to reshape a continuous mapping to avoid the middle. Composer
Mastermind mirrors this at every layer that used to assume a continuous percent (`CCMapping`'s new
`swingStateForPercent`/`swingPercentForState`/`swingStateForNormalized` are the shared conversion), including
converting three separate UI sliders (`SceneListComponent`, `PresetLibraryContent`'s role and rhythmic-
relationship builders) to matching 3-item combos, so a scene/preset can no longer author an illegal swing value
by hand either.

**Live-tested same day, found incomplete**: Shuffle was initially set to 75% (the old slider's historical
maximum) and was indistinguishable from Triplet by ear - both are close to a 2:1 ratio (2.2:1 vs. exactly 2:1).
Corrected to **100%**, the swing delay formula's actual mathematical ceiling, which produces a genuinely
different 3:1 ratio - the real dotted-shuffle feel. `CCMapping::kMaxSwing` is now just an alias for
`kShuffleSwingPercent` rather than a separately-tracked value, so `CoherenceEvaluator`'s spread normalizer and
`Validation::isValidSwing`'s ceiling moved automatically. Both plugins compile clean. **CONFIRMED LIVE 2026-08-26**:
Triplet/Shuffle read as genuinely distinct in Bitwig at 100%, and the notation-export fix (giving Shuffle its own
dotted-rhythm onset position instead of folding into Triplet's) was verified correct in Dorico for both.

### v1.2 follow-up — Score View Load/Save Score + composition bundle v2 (Instances/Motif Presets/Modulator Targets) (built 2026-08-27)

`CompositionBundleStore` extended to genuinely self-contained "score" files - a v1 bundle only carried a Blueprint
plus its referenced Scenes, so importing into a session without the matching Motif Preset already registered
stamped nothing and played silence (the exact bug the next entry below traces further). Now also bundles every
registered Instance (role-based archetype eligibility makes a "referenced-only" subset unsafe) and every Motif
Preset tagged with an archetype the blueprint's sections use. Score View (the simple UI) also gained its own
**Load Score.../Save Score...** buttons on this same backend, after the Scenes tab's *different*, unrelated
"Load Snapshot" button was mistaken for it once live. See [score_timeline_ui_concept.md](score_timeline_ui_concept.md)
for the full writeup, including a delivered example score ("Tides") built to exercise all 5 arc-dimension
consumers at once.

### v1.2 follow-up — Compose can generate a blueprint from a blank curve; register-drift bug found and fixed (built 2026-08-27)

Compose's "New Blueprint" button now wires directly to `policy/BlueprintGenerator::generate` (curve shape → real
sections, already-working code that was only reachable from a separate Expert tab before) - pick a bar length,
draw one curve, Generate, done in one motion. See [score_timeline_ui_concept.md](score_timeline_ui_concept.md).

Testing that feature surfaced a real, pre-existing bug: `policy/MotifEngine`'s register-continuity math
(`patternCenterNote`, used by every stamp/phrase-chain/Nudge-pass write) recomputed its pitch center fresh from
whatever was last written, every single pass, with nothing pulling it back toward anywhere - a genuine unbounded
random walk the code's own comment had already named as a risk without ever fixing. Combined with Tension's
by-design upward-only register pull staying elevated for a long stretch, stored note content walked all the way
to MIDI's hard 127 ceiling and stuck there - reproduced in both a curve-driven piece and a plain Generative one
with reasonable starting notes. Fixed with `boundedHomeCenter`: every computed center now gets clamped to within
`kMaxDriftFromHomeSemitones` (2 octaves) of a fixed `kHomeRegisterNote` (middle C) before being used - "organic
drift" stays intentional and bounded instead of only being stopped by MIDI's own ceiling/floor. No per-instance/
role "natural register" concept exists yet to give different instances different homes; noted as a possible
future refinement, not built now. Compiled clean. **Not yet live-tested.**

### v1.4 — Modulation Matrix: arc-driven routes to any MPL parameter (built 2026-08-27)

Prompted by the user comparing Composer Mastermind's actual output against a Bitwig screenshot showing 13+
independently-automated CC lanes per instance and finding MC's output "far from satisfactory... a slow, rusty
machinery" by comparison. Honest diagnosis: only 2 of the 5 arc dimensions drove anything continuously (Energy/
Tension → Transpose, Density → Swing), plus 2 threshold-crossing consumers (Complexity → phrase role, Coherence
→ Retrograde/M7) - everything else MPL exposes (Rotation, Length, Inversion, Active Pattern, Grid Mode, and full
per-pattern independence across up to 3 instances × 3 patterns) sat idle unless hand-authored. The gap versus raw
automation was coverage, not musical intelligence.

New `ModulationRoute` model (`src/model/ModulationRoute.h`) lets the user wire any arc dimension to any of 9 MPL
parameters, on a specific instance/pattern or broadcast to every registered instance (`targetInstance == "*"`) -
a genuine modulation matrix, authored via a new "Instance Modulation Routes" panel on the existing Modulators tab
(`ui/ModulatorTargetView`, now Viewport-wrapped to fit two full panels). Deliberately a new model, not a
repurposed `ModulatorTarget` - that struct's own doc comment ties it explicitly to CCs *outside* the Instance/MPL
model (a raw channel/CC pair for pairing with a Bitwig modulator's Learn CC), with no instance concept and no
per-parameter encoding; `ModulationRoute` addresses a real `Instance` by id and dispatches through `CCMapping`'s
own per-parameter encoders via two new `Router` methods (`routeContinuousParameter`/`routeThresholdParameter`),
the same `targetInstance` + `patternIndex` targeting shape `Mutation`/`ReservedValue` already use.

Two dispatch shapes, reusing the two patterns already established by the built-in consumers rather than inventing
a third: Transpose/Rotation/Length/Swing are continuous (linear-mapped into the route's own `[outputMin,
outputMax]`, re-sent every bar); Inversion/Retrograde/M7/Grid Mode are threshold-crossing (a configurable 0..1
crossing point) and Active Pattern bands into 4 states (0 = stop, 1-3 = pattern) - all threshold routes only
dispatch on an actual state change, same restraint as `firePhraseChainIfDue`/`applyCoherenceDivergenceIfDue`.
Dispatch (`ComposerCore::sendModulationRouteUpdates`) is called from the same call sites as the built-in
continuous/threshold consumers (`advanceBlueprintIfNeeded`'s "still active" branch, `enterSection`) rather than
unconditionally every bar the way `ModulatorTarget`'s external-CC dispatch is - routes touch real MPL content, so
they correctly inherit the frozen-section (Absolute content mode) gate the built-ins already respect.

Conflict handling: a new `ComposerCore::hasActiveRouteOverride` guard lets a user-authored route cleanly take over
a specific instance's parameter from its built-in curve (checked inside `applyContinuousMelodicCurve`,
`applyContinuousSwing`, `applyCoherenceDivergenceIfDue` - independently per Retrograde/M7 - and
`firePhraseChainIfDue`) rather than the two dispatching conflicting values every bar. The built-in consumers
themselves are untouched otherwise - the guard is a one-line skip, not a rewrite.

Routes persist through every existing mechanism: `StateSerializer`/`StateSnapshotStore` (full-library round-trip)
and `CompositionBundleStore` (now v3 - routes included by instance membership, since a route isn't referenced
indirectly through a section's fields the way a `ModulatorTarget` is), plus two new MCP bridge actions
(`createModulationRoute`/`getModulationRoutes`) mirroring the existing `createModulatorTarget`/
`getModulatorTargets` pair. Compiled clean (VST3 + Standalone), installs clean, Standalone launches without
crashing. **Not yet live-tested in Bitwig** - worth confirming a route's dispatch is audible/visible on real MPL
instances, that the override guard actually prevents flicker when a route and a built-in consumer target the same
parameter, and that a piece with several routes assigned sounds noticeably more independently-varied than before,
addressing the originally-reported gap.

### v1.4 follow-up — content-aware continuous Transpose curve (built 2026-08-27)

Found live while testing the modulation matrix above: the built-in Energy/Tension → Transpose curve
(`ComposerCore::applyContinuousMelodicCurve`) computed its register-pull purely from the Tension arc's value and
applied it identically to every touched instance, with zero knowledge of where that instance's pattern already
sat pitch-wise - pushed an already high-voiced pattern (notes in the 78-91 MIDI range) up by another +10
semitones. User's own framing: Transpose should mostly "walk" a pattern around its own natural register, not
stack an unconditional pull on top of whatever's already there; bigger deliberate octave moves belong in
Bitwig-side devices, not this curve.

Fixed by extending the same bounding policy `boundedHomeCenter`/`patternCenterNote` already give generative
stamping (see the register-drift fix above) to this second, independent pitch-affecting layer: new
`MotifEngine::taperTransposeForPatternContent` (`policy/MotifEngine.h/.cpp`) reads a pattern's own current bounded
center from its cached content (same Length-window-bounded average `applyForSection` already trusts), bounds the
*combined* result (that center plus the curve's desired raw Transpose) to the same home-register clamp, then
back-solves the Transpose that actually achieves the bounded combined result - a graceful taper, not a hard
reject: a pattern near the home register is unaffected, one voiced far from it gets less pull (or a gentle pull
back). One new call in `ComposerCore::applyContinuousMelodicCurve`'s per-instance loop, no changes to
`stampOnePattern`/`applyForSection`'s own tested `boundedHomeCenter` usage, no changes to `Router::
routeContinuousTranspose`'s flat ±48 safety clamp (right layer for an absolute-value backstop, wrong layer for
content-awareness - it has no pattern-cache access). Deliberately scoped to the built-in curve only, not
user-authored `ModulationRoute`s targeting Transpose (those already have their own user-set, bounded range).

Compiled clean, VST3 reinstalled without a Bitwig file-lock this time. **Not yet re-tested live** - the
already-running Bitwig plugin instance has the old code resident in memory; needs a reload (remove/re-add the
instance, or restart Bitwig) before the fix is actually active in that session.

### v1.4 follow-up — loop-cycle-accurate melodic sequencing, "fragment sequencer" (built 2026-08-27)

Grew directly out of analyzing a real Bach invention (BWV 773) live this session. Two corrected understandings
this feature rests on: MPL applies its Transpose transform fresh, per note, at output time - not baked into
stored content once per loop - confirmed by reading the sibling project's `applyPatternTransformsToNote`; and
every CC this plugin exposes is "a knob-string," identical in kind whether moved by a human, a Bitwig LFO, or one
of this plugin's own consumers - there's no architectural ceiling on update rate, only a *clock choice* per
consumer. `ModulationRoute`'s existing dispatch ticks once per bar because that's the right clock for slow
structural drift; real melodic sequencing (a short fragment restated at a new pitch level every repetition -
empirically confirmed as one of Bach's most-used devices, dozens of recurring transposed interval-patterns found
via `music21` analysis of BWV 773) needs a loop-cycle clock instead.

New `ModulationDispatchMode` on `ModulationRoute` (`model/ModulationRoute.h`): `Bar` (existing behavior, default,
zero change for every route built earlier this session) or `Sequence` - steps through an authored
`sequenceValues[]` list once per pattern LOOP CYCLE rather than once per bar. The loop-cycle clock itself
(`scheduling/StepClock.h`, new) is lifted from `ui/PrimaryView.cpp`'s already-proven Live Sync playhead
reconstruction math (same tempo-invariant ppq arithmetic, same 4/4-bar assumption MPL itself makes) - extracted
into a shared, audio-thread-safe header rather than duplicated ad hoc; `PrimaryView.cpp`'s own UI-thread copy is
untouched. `ComposerCore::processBar` now carries the bar's host ppq through (`barStartPpq`), captured as
`currentSectionPpqAnchor` on every section entry (phase-zero for loop-cycle counting); a new
`ComposerCore::processStepTick(currentPpq)`, called from `PluginProcessor::processBlock` every block while playing
(not edge-detected like the bar tick - needs sub-bar granularity), detects each loop-cycle boundary a
Sequence-mode route's target pattern crosses and dispatches the next value via the *existing*
`Router::routeContinuousParameter` - same knob-string as always, just a different clock triggering it.
`hasActiveRouteOverride` needed no changes at all to correctly make the built-in curves step aside for a Sequence
route, confirming the model already generalized cleanly.

Validated: `Validation::isValidModulationRoute` requires Sequence mode to have a non-empty `sequenceValues` and a
continuous `parameter`. Persisted through the existing `StateSerializer`/bundle/snapshot machinery (two new
fields, backward-compatible defaults). MCP bridge's `create_modulation_route`/`get_modulation_routes` extended to
match - the C++ side needed zero new code for the read action once it was switched to reuse
`StateSerializer::modulationRoutesToVar` instead of hand-building its own (slightly stale) JSON, a small
cleanup that fell out of this change. UI: the Modulators tab's routes panel gained a Dispatch combo and a
comma-separated sequence-values field, shown only for continuous parameters in Sequence mode.

Compiled clean, VST3 reinstalled without a Bitwig file-lock. **Confirmed working live in Bitwig same day** - a
4-step pattern with a 4-value Sequence route produced, in the user's own words, "a full 16 steps melodic 'loop'"
- the fragment/value-count product giving an effective longer melodic period before true repetition, exactly the
intended device.

**Keyswitch-style lookahead, same-day follow-up.** User's own framing, precisely correct: a CC change is only
audible on the step it arrives in time for, not the one it was conceptually meant for - the same discipline a
sample library's keyswitch needs, sent slightly ahead of the note it gates. `processStepTick`'s original
`loopCycleIndex`-based detection was purely reactive - it only fired *after* the true ppq boundary had already
passed, with no margin for this plugin's own per-block CC-queuing quantization or the host's inter-plugin MIDI
routing latency, both of which could in principle land a value one step late at the destination. Fixed with a new
`kSequenceLookaheadStepFraction` (10% of one grid-step's own ppq length, tempo-invariant by the same principle
`StepClock.h` already commits to): the loop now fires a route's next value once `currentPpq` is within that margin
of the *upcoming* boundary, not only after it, while still snapping straight to the true current cycle index if
playback jumps further than one cycle since the last check (a host seek, or a route's very first tick). Compiled
clean, reinstalled. **Confirmed working live same day** - user's own report after re-testing: "I think it is
working fine now."

**Absolute-mode cache bugfix, same-day.** Building a fuller piece (`momentum_piece`, mixing composed Absolute
sections with the fragment sequencer) surfaced a separate, real bug: the Score View's Overlay piano-roll
(`ui/PrimaryView.cpp`, reads note content straight from `InstancePatternCache`) showed nothing for an Absolute
section despite the audio being completely correct. Root cause: `ComposerCore::enterSection`'s captured-content
write path sends the real IPC write to MPL but never updated the cache afterward, unlike every other content-write
path in this file (milestone restore, generative stamping), which all pair the two. Fixed with one
`cache.store(...)` call mirroring the existing pattern. Confirmed fixed live via a user screenshot showing real
note blocks rendering correctly post-fix.

### v1.5 — Rate: augmentation/diminution as a real, JSON-scriptable device (built 2026-08-27)

Grew out of researching Cateia Games' Fugue Machine (independent per-voice playhead direction/rate/length) -
"independent per-voice rate" was the single most musically attractive idea to carry over: a rhythmic variation
reachable with one CC impulse rather than re-authoring a whole pattern's step durations. Built MPL-side first
(the sibling project, matching how Retrograde/M7 were proven there before being mirrored here): a global, 3-state
`AudioParameterChoice` (Augmented=0.5x/Normal=1x/Diminished=2x, CC 23, the one previously-unused global slot
between Grid Mode=22 and Swing=24), with the one substantive engine change being a single divisor -
`effectiveGridStepLengthInPpq = gridStepLengthInPpq / getGlobalRateMultiplier()` - substituted into every
timing (not step-index) expression in `processBlock`'s block-scan loop, so onsets AND durations both scale
together, a real augmentation/diminution rather than a delayed-onset illusion. Also added the missing custom-
editor UI control (host generic panels picked up the parameter automatically; the custom editor needed a new
`Rate` dropdown next to `Swing`, which surfaced and fixed a pre-existing row-overflow bug in that editor).

Manual live testing immediately surfaced a real musical payoff, not just a technical curiosity: switching an
instance to Augmented for the last 1-2 bars of a phrase reads as a genuine cadential/closing gesture - agogic
accent (deceleration reads as arrival even with no harmonic resolution available), which matters specifically for
atonal writing, where tonal cadences aren't available at all. The user's own generalization, confirmed sound: the
same device works fractally, at a macro (whole-section) scale and a micro (last-few-notes) scale, since both are
just the same bar-scheduled mechanism at different time-windows.

That motivated immediately giving Rate the same two citizenships every other parameter has on this side:
- **Mutation type `"rate"`** (`model/Mutation.h`, `routing/Router.cpp::routeMutation`) - deliberately an
  **absolute set, not a delta** (unlike Rotation/Transpose), following the existing boolean-exception convention
  Inversion/Retrograde/M7 use but centered on `0`=Normal for safety: `amount` clamped to `[-1, 1]` maps directly
  to state via `state = amount + 1` (`CCMapping::rateStateFromMutationAmount`). This is the direct authoring
  mechanism for the cadential-flag use case: `{"type": "rate", "amount": -1, "applyAtBar": N}` sets Augmented at
  bar N, deterministically, regardless of whatever state came before.
- **`ModulationParameter::Rate`** (`model/ModulationRoute.h`) - global like Swing (not per-pattern), continuous
  like Swing (banded to 3 states from an arc's 0..1 sample via a new `CCMapping::rateStateForNormalized`), usable
  in both `Bar` and `Sequence` dispatch mode. Required an explicit `case` in three separate allowlists that would
  otherwise have silently mis-handled it (`isContinuousModulationParameter`/`isGlobalModulationParameter`, a new
  `Router::routeContinuousRate` mirroring `routeContinuousSwing`, and `ComposerCore::sendModulationRouteUpdates`'s
  domain-default `switch` - without that last one, Rate would have silently inherited Swing's 0..100 domain).

Storage: `int rate` on `InstanceParameterState`/`SceneGlobal`/`SceneInstanceOverride`/`RolePreset`/
`RhythmicRelationshipRoleSlot` (0=Augmented/1=Normal/2=Diminished) - an int, not a float-percent like Swing,
since the model is already a clean 3-state domain with no percent metaphor to invent; reuses `activePattern`/
`gridMode`'s existing `-1` = "inherit" sentinel convention rather than Swing's `-1.0f`. `InstanceStateTracker::
recordGlobal` grew a 5th parameter, threaded through all 10 existing call sites across 4 files.

**Fixed the Live Sync/StepClock desync in the same pass, not deferred again.** This was explicitly flagged as a
known gap while Rate was MPL-only ("nobody can script it yet, so it barely comes up"); making Rate JSON/arc-
schedulable means the desync between MPL's real (rate-scaled) playback and this plugin's own un-rated
`scheduling/StepClock.h`/`ui/PrimaryView.cpp` reconstructions would now trigger routinely instead of only when a
human manually turned the MPL knob. New `CCMapping::rateMultiplierForState` (mirrors MPL's own
`getGlobalRateMultiplier()` exactly) threaded into both `StepClock::gridStepLengthInPpq`/`loopCycleIndex` (used by
`processStepTick`'s Sequence-mode cadence) and `PrimaryView.cpp`'s own `gridStepLengthInPpqFor`/
`computeLivePlayheadFraction` plus its inlined take-recorder copy - all now read the relevant instance's tracked
`rate` from state already in scope at each call site.

Compiled clean on the first attempt (both the MPL-side engine/UI change and the Composer Mastermind-side mirroring
phase), VST3s reinstalled without a Bitwig file-lock. Docs (`docs/composition_bundle_format.md`) and the MCP
bridge's tool docstrings (`send_mutation`/`create_modulation_route`/`create_scene`/`create_role_preset`/
`create_rhythmic_relationship_preset`) updated to match.

**Confirmed working live in Bitwig same day**, both mechanisms independently: a direct `send_mutation` with
`type="rate"` landed correctly (CC 23 confirmed reaching MPL, user's own words: "This is the missing puzzle
piece"); a live `tension→Rate` `ModulationRoute` (`invert: false` - low tension eases toward Augmented, high
tension pushes toward Diminished, the actual cadence-vs-intensity pairing this was built for) also confirmed. A
full demonstration piece, **"Slack Tide"** (`docs/example_score_slack_tide.json`, 5 sections/~40 bars), was
composed and played live specifically to close the loop: two `tension→rate` routes carry the *macro*, fully
arc-automatic deceleration into the final section (no manual input once loaded), while one live `send_mutation`
fired at the arranged moment (bar 38) on the melodic voice supplies the *micro*, deliberate flourish - the same
macro/micro split the user described. Real limitation surfaced along the way, not new: a `Scene`'s own
`mutations[]` array still isn't auto-dispatched during blueprint playback (`docs/composition_bundle_format.md`'s
own pre-existing caveat), so the micro gesture had to be fired live over the MCP bridge rather than authored
into the JSON directly - noted as a real follow-up, not solved here.

### v1.6 — BarCycle: multi-bar phrase-cadence dispatch (built 2026-08-27)

Grew directly out of framing v1.5's Rate work in terms of musical form: phrase-internal breathing (micro) and
section-boundary softening (macro) turned out to be the same device - a periodic dip toward Augmented - just at
different timescales, matching the "fractal" pattern the user had already described. `Sequence` mode almost
covers this, but its loop cycle is measured in MPL pattern steps, hard-capped by `CCMapping::kPatternSteps = 16`
- at most one bar, never a real multi-bar phrase. The user also specifically asked that this not be mechanical:
different instances should be able to breathe on different periods (2 bars, 3 bars, 4 bars) so an ensemble's
phrasing drifts in and out of alignment rather than landing in lockstep every time - deterministic variety (a
different fixed number per route), not randomness, consistent with `MotifEngine`'s own established "no ML, no
raw randomness" stance.

Third `ModulationDispatchMode`, **`BarCycle`**, alongside `Bar`/`Sequence`. New `int phraseLengthBars` field on
`ModulationRoute` (default 4). Cycle math is genuinely simpler than `Sequence` mode's ppq arithmetic - no new
anchor tracking needed at all, since `currentBar` and the active section's own `startBar` are already plain ints
available at the call site:
```cpp
const int phraseIndex = (currentBar - section.startBar) / std::max(1, route.phraseLengthBars);
```
New `ComposerCore::firePhraseCadenceIfDue(section, currentBar)` mirrors `sequenceRouteLastFiredIndex`'s exact
`"routeId|instanceId" -> last-fired-index` map shape (new `phraseCadenceLastFiredIndex` member, same
`blueprintMutex` guard, cleared on the same section-boundary reset), and dispatches through the *existing*
`Router::routeContinuousParameter` - the same endpoint `Bar` and `Sequence` already use, so **no `Router.cpp`
changes were needed at all**. Bar-granularity by construction (checked once per bar, from the same two call sites
`sendModulationRouteUpdates` already uses), so unlike `Sequence` mode's ppq-based `processStepTick`, no sub-bar
lookahead machinery is needed - every other bar-boundary CC send in this codebase already fires plainly at the
bar tick.

One mode elegantly covers both of v1.5's use cases: a short `phraseLengthBars` (2-4) gives phrase-internal
breathing; set equal to a whole section's `durationBars`, it reproduces the exact "once, near the end" macro
cadence that previously needed a live-fired `send_mutation` (Slack Tide's bar-38 gesture) - now fully
JSON-authorable, no live input required. Closing `Scene.mutations[]`'s own `applyAtBar` dispatch gap (noted at
the end of v1.5 above) remains a real, separate follow-up for a genuinely irregular one-off event, but is no
longer needed for the cadence-breathing pattern this session was actually building toward.

Compiled clean on the first attempt, VST3 reinstalled without a Bitwig file-lock. `Validation::isValidModulationRoute`
extended (shares `Sequence`'s parameter/non-empty-`sequenceValues` check, plus a `phraseLengthBars >= 1` floor).
Docs (`docs/composition_bundle_format.md`) and the MCP bridge's `create_modulation_route` docstring updated to
match. **Confirmed working live in Bitwig same day** - user's own words: "I think it is working well. Clearly an
improvement."

Live testing immediately surfaced a real orchestration lesson, not a bug: letting every voice's `tension → rate`
route reach Diminished together at a section's peak, combined with a fragment-sequencer route also running fast
content on the same voice at the same moment, produced an unintelligible blur at normal tempo - two
density-increasing devices compounding rather than adding. Fixed live by capping one voice's `outputMax` at `1`
(never reaches Diminished, holds the ensemble's steady pulse) while leaving another's at the full `0..2` range
(carries the peak intensity alone) - no engine changes, pure authoring. Written up as a permanent guideline in
`docs/advanced_workflow_example.md`'s "Troubleshooting quick hits," and `docs/example_score_slack_tide.json`
updated to match the fix rather than the version that needed a tempo workaround.

**Follow-up, same day: Modulators tab UI closed the JSON-only gap.** `BarCycle` (and, it turned out, `Rate`
itself - `kParameterOptions` never had a "Rate" entry, so the whole parameter was JSON/MCP-only until now) were
unreachable from the Expert UI's route form entirely; the dispatch-mode combo only had two items and the save
logic hardcoded `== 2 ? Sequence : Bar`, silently collapsing any third option to `Bar`. Added the "Rate" parameter
option, a third "Bar Cycle (phrase cadence)" dispatch item, a `phraseLengthBars` slider (its own row, since
BarCycle needs `sequenceValues` and `phraseLengthBars` visible together, unlike range/threshold/sequence which
are always mutually exclusive), and a proper `0..2` range preset for Rate in Bar mode (previously silently
defaulted to Transpose's `-48..48`). Compiled clean, installed.

**Follow-up, same day: `InstanceRegistry` channel-uniqueness enforcement.** Real problem hit live earlier this
session: `v1`-`v4` (from an earlier piece, `Grand Invention`) were still registered alongside `MPL1`-`MPL3`,
sharing the same MIDI channels - a genuine collision, both silently receiving identical CC traffic. Root cause,
confirmed by reading the code: `InstanceRegistry::addInstance` only ever upserted by id, with no channel check at
all, and neither Load Score's composition-bundle import nor a Bitwig preset recall ever clears the registry
first (both merge/upsert everything, `docs/composer_mastermind_design.md`'s own "fully reconfigures" language
notwithstanding - true for blueprint/scenes/routes, not for instances). Fixed with the smaller of the two options
presented earlier (the larger one - making the MIDI channel itself the addressing key everywhere, replacing the
free-string `id` - remains a real but genuinely separate, breaking migration, deliberately not attempted):
`addInstance` now evicts any other registered entry already on the incoming instance's `midiChannel` before
adding it, since a channel can only ever reach one real physical MPL instance - a collision is always a stale
leftover, never a legitimate second occupant. Self-healing: the next time anything re-registers `MPL1`-`MPL3`
(a fresh Load Score, or manually via Add Instance), the colliding `v1`-`v3` entries are evicted automatically;
`v4` (channel 4, no current collision) stays registered until removed by hand or superseded by something new on
channel 4. Compiled clean, installed - not yet re-verified live (the reinstall itself drops the MCP bridge's
socket connection to the reloaded plugin).

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
