# Technical Spec Checklist

Living status list. Update this when a checkbox's underlying code changes — it's meant to be a faster read than
diffing the whole `src/` tree. See [composer_mastermind_design.md](composer_mastermind_design.md) for the narrative
version of this same information.

## v0.1 — Conductor Wiring

- [x] `Instance` model (id, name, midiChannel, role, enabled) — [`model/Instance.h`](../src/model/Instance.h)
- [x] `InstanceRegistry` (add/remove/query, mutex-guarded) — [`routing/InstanceRegistry`](../src/routing/InstanceRegistry.h)
- [x] `CCMapping` matching MPL's live External Control CC protocol — [`midi/CCMapping.h`](../src/midi/CCMapping.h)
- [x] `CCDispatcher` (queued, mutex-guarded, drained on audio thread) — [`midi/CCDispatcher`](../src/midi/CCDispatcher.h)
- [x] `Router::routeScene` — global CCs to `Scene::targets`, per-pattern CCs to `Scene::patterns`
- [x] `Router::routeMutation` — single mutation → CC
- [x] `ComposerCore` wiring (owns registry/dispatcher/router/scheduler, thread-safe scene storage)
- [x] `PluginProcessor` produces real MIDI CC output, bar-boundary detection from host playhead
- [x] Minimal functional editor UI (add instance, send test scene, send test mutation)
- [x] Verified against real MPL instances in Bitwig 6 (2026-08-14): 3 instances registered on channels 1-3,
      global scene send and single-instance mutation send both confirmed landing correctly via MPL's own
      "Last CC" debug readout
- [x] `SceneInstanceOverride` — per-instance divergence of Active Pattern/Grid Mode/Swing from `Scene::global`
      (2026-08-14), fixing the gap where every targeted instance was forced to identical instance-level values.
      See [composer_mastermind_design.md](composer_mastermind_design.md), "'Send To All' Is One Preset Among Many".
      Editor UI does not yet expose authoring overrides — only the underlying model/Router support exists

## v0.2 — Scene Persistence

- [x] `util/JsonHelpers` — safe `juce::var` accessors with defaults, so partial/older JSON degrades gracefully
      instead of failing to parse
- [x] `state/StateSerializer` — `Scene`/`Instance`/`Mutation`/`ScenePattern`/`SceneInstanceOverride` ↔ `juce::var`
      per [scene_format_v0_1.md](scene_format_v0_1.md); compiles, not yet exercised against a real Bitwig
      save/reload by the user
- [x] `state/SceneLibrary` — mutex-guarded named scene set (`addOrReplaceScene`/`removeScene`/`getSceneById`/
      `getAllScenes`), owned by `ComposerCore`
- [x] `state/StateSnapshotStore` — full snapshot (instances + scene library + current scene id) as one JSON
      string, embedded as an attribute on the existing APVTS XML in `PluginProcessor::getStateInformation`/
      `setStateInformation` — instances/scenes now survive a DAW project save/reload. Invalid entries in a
      snapshot are skipped individually rather than aborting the whole restore
- [x] `Scene::mutations` actually consumed by routing — `Router::routeScene` now calls `routeMutation` for each
- [x] Editor UI: save/load/remove named scenes in the library, plus export/import the whole snapshot to a
      `.json` file via `juce::FileChooser` (async, non-blocking)
- [ ] Preset library (new scope, 2026-08-14): role-addressed, taggable, reusable presets (role/arc/rhythmic-
      relationship categories) that resolve to `SceneInstanceOverride`/`ScenePattern`/`Mutation` at apply-time —
      see [composer_mastermind_design.md](composer_mastermind_design.md), "'Send To All' Is One Preset Among Many"

## v0.3 — Scene Chain & Bar-Quantized Advancement

- [x] `policy/SceneAdvancePolicy::shouldAdvance` — pure decision function (`Scene::nextSceneId` + `durationBars`
      vs. elapsed bar-ticks), JUCE-free
- [x] `ComposerCore::advanceSceneChainIfNeeded` — owns the active scene's start-bar state, switches scenes via
      `SceneLibrary`, routes immediately from `processBar` on the audio thread
- [x] `ComposerCore::notifyTransportReset` + `PluginProcessor` wiring — re-anchors the active scene's clock on
      transport stop, fixing the previously-flagged restart gap; looping while playing needs no special handling
      (see [scheduling_policy_v0_1.md](scheduling_policy_v0_1.md) for why)
- [x] Editor UI: Duration Bars slider + Next Scene combo in the Global Scene panel
- [ ] `Scheduler::scheduleEvent` actually called by something — still not; `Scheduler` stays reserved for a
      future one-off-scheduled-send feature, deliberately not used by chain advancement (see design doc)
- [ ] `Scene::quantize` field actually consulted (currently unused)
- [x] **Live Bitwig test** (2026-08-15): user set up and sent a 3-scene chain (`intro3` → `intro`, 4-bar
      duration) and confirmed "it works as expected." Chain setup/send and the general mechanism are confirmed;
      the specific stop/restart-mid-chain re-anchoring case wasn't separately narrated as tested

## v0.4 — Roles & Cognitive-Load Governor

- [ ] Role presets (anchor/motif/counterpoint/…) — new, pre-fill mutation budgets from `Instance::role`
- [ ] `policy/MutationPolicy` — per-instance budget bookkeeping, modeled on `noveltyBudget` +
      preserve/transform-parameter lists keyed to role and formal position — see
      [atonal_phrase_engine_concepts.md](atonal_phrase_engine_concepts.md)
- [ ] `policy/PolicyEngine` — single gate all mutation/scene routing passes through, including manual UI sends
- [ ] Change-weight classification (minor/medium/major) per [mutation_policy_v0_1.md](mutation_policy_v0_1.md)
- [ ] Mutation-authorization check (new, 2026-08-15): reject/skip a `Mutation` that touches a protected
      instance/role/bar combination (e.g. anchor instance during a climax bar) — `Router::routeMutation` has no
      such concept today

## v0.5 — Arcs & Blueprints

- [ ] Generic `Arc` evaluator (piecewise-linear breakpoints), API should expose direction/delta and
      distance-to-next-breakpoint, not just interpolated value (see atonal_phrase_engine_concepts.md)
- [ ] Coherence/convergence evaluator — concrete formula to adapt from Parameter Concordance Diagnostics (see
      atonal_phrase_engine_concepts.md): normalize each instance's CC-parameter state, count near-shared-extreme
      convergence, derive a scalar; validate the designated climax scene is actually the convergence point
- [ ] Cross-scene apex exclusivity (new, 2026-08-15): clamp earlier scenes' arc targets against reserved ceiling
      values of a downstream climax scene in the `nextSceneId` chain
- [ ] Blueprint JSON schema + loader (sections, foreground/support/background, mutation budget per section)
- [ ] `model/ComposerState.h` — currently empty; likely home for "current section / current arc values" runtime state

## v0.6 — Motivic Variation Engine

- [ ] Blocked on MPL's own v1.20.0 (safe external step mutation) — see CC map discrepancy in
      [routing_policy_v0_1.md](routing_policy_v0_1.md)
- [ ] Pass-based variation sequencing

## v1.0 — Narrative Blueprint Composer

- [ ] Full blueprint library UI (`ui/EditorView`, `ui/SceneListComponent`, `ui/TransportView`, `ui/InstanceView`,
      `ui/DebugPanel` — all currently empty stubs; today's editor is a flat debug panel, not this)
- [ ] **UI has hit its ceiling as a single flat panel** (2026-08-15): the v0.1-v0.3 debug editor is now ~860px
      tall (4 stacked test sections) and the user flagged it's reached a practical height limit. When the real
      UI gets built here, it needs actual navigation (tabs/pages — e.g. "Instances," "Scene/Blueprint Editor,"
      "Debug") rather than one more panel appended to the bottom of a single scrolling column
- [ ] Save/Recall presets (DAW preset + JSON, per source roadmap §19)
- [ ] `debug/Logger`, `debug/DebugMonitor` — currently empty; useful before this milestone too, not just at it

## v1.1 — Generative Blueprint Proposal

- [ ] Not started. Scope commitment recorded in [composer_mastermind_design.md](composer_mastermind_design.md)
      ("Player vs. Composer," 2026-08-14) — depends on v0.4/v0.5 existing first.

## Housekeeping (not roadmap-versioned, currently empty/unused)

- [ ] `scripts/build.sh`, `scripts/format.sh`, `scripts/run_tests.sh` — all empty
- [ ] `tests/unit`, `tests/integration` — no tests exist yet; `InstanceRegistry`, `Router`, `CCMapping`, `Scheduler`,
      and `Validation` are all JUCE-free by design specifically so they're unit-testable without a plugin host —
      worth using that before the policy layer (v0.4) adds real logic worth breaking
- [ ] `util/MathUtils.h`, `util/StringUtils.h` — empty, unclear yet if/when needed
- [x] `midi/CCMapping.h` added to `CMakeLists.txt` `source_group` for IDE visibility alongside the other files
