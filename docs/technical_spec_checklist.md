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
- [x] **Preset library, role presets only (built 2026-08-17):** `model/Preset.h`'s `RolePreset` (id, name,
      `targetRole`, tags, instance-level `activePattern`/`gridMode`/`swing` matching `SceneInstanceOverride`'s
      -1/-1.0f "inherit" sentinel exactly) is the first of the design doc's three preset categories
      ("'Send To All' Is One Preset Among Many"'s role/arc/rhythmic-relationship split) — only role presets are
      built, arc presets and rhythmic-relationship presets are not. `state/PresetLibrary` mirrors
      `SceneLibrary`/`BlueprintLibrary` exactly; `StateSerializer`/`Validation::isValidRolePreset`/
      `StateSnapshotStore` follow the same pattern as scenes/blueprints. `policy/PresetResolver::
      applyRolePreset` (pure, JUCE-free, same style as `SceneAdvancePolicy`/`MutationPolicy`) resolves a
      role-addressed preset into concrete `SceneInstanceOverride` entries for whichever currently-registered
      instances hold that role — and adds them to the scene's `targets` if not already present, since an
      override is inert for an untargeted instance. UI: new **Presets** tab (`ui/PresetLibraryView`, 5th outer
      tab in `ui/EditorView`) builds/saves/loads/removes `RolePreset`s, and an "Apply to Scene" action resolves
      a saved preset directly onto a saved `Scene` in `SceneLibrary` and persists the result — presets modify
      library scenes at authoring time (a one-shot "stamp"), not a live runtime reference resolved every route
      call, matching the design doc's "always inspectable and editable before playback" framing. Confirmed
      working live in Bitwig (2026-08-17): applied a swing-75 role preset to a saved scene, `Load and Send`
      delivered the correct CC (24, value 127 = 75%) to the matching instance — verified via MPL's own "Last
      CC" readout, not just Composer Mastermind's side.
  - **Rhythmic-relationship presets built (2026-08-17):** `model/Preset.h`'s `RhythmicRelationshipPreset`
    (id, name, tags, `vector<RhythmicRelationshipRoleSlot>` — each slot the same activePattern/gridMode/swing
    shape as `RolePreset`, minus id/name/tags which belong to the whole joint preset) is the second of the
    three categories — a *deliberately correlated* choice across 2+ roles applied together in one call (e.g.
    role `motif` Binary while role `counterpoint` Ternary, chosen together on purpose for a specific groove).
    `Validation::isValidRhythmicRelationshipPreset` requires at least 2 role slots — a single-slot "joint"
    preset isn't a relationship. `PresetLibrary` now holds both preset categories (two vectors, same
    mutex-guarded shape). `policy/PresetResolver::applyRhythmicRelationshipPreset` resolves each slot the same
    way `applyRolePreset` does (both now share a private `applyFieldsToInstance` merge step) — once per role
    slot, so the whole joint choice lands together. UI: the Presets tab gained a second authoring section
    (build role slots one at a time via **Add Role Slot**, then **Save Preset** bundles the pending slots) plus
    its own library and "Apply to Scene" action. The combined tab outgrew its available height, so
    `ui/PresetLibraryView` is now a thin `juce::Viewport` host around new `ui/PresetLibraryContent` (same fix
    as the Sections tab). Confirmed working live in Bitwig (2026-08-17): a 2-slot preset (`motif`=Binary,
    `counterpoint`=Ternary) applied to a saved scene and landed correctly on both instances via `Load and Send`.
  - **Arc presets built (2026-08-18) — the third and last category:** `model/Preset.h`'s `ArcPreset`
    (id, name, tags, `vector<ArcPresetBreakpoint>`) is a reusable *normalized* shape — each breakpoint's
    `position`/`value` are both 0..1, so the same "swell"/"plateau"/"arch" shape can be stamped onto any bar
    range without redrawing it. Deliberately doesn't record which of `ArcSet`'s 5 dimensions it targets — that's
    chosen at apply-time, so one shape is reusable across energy, tension, or any other dimension.
    `Validation::isValidArcPreset` requires at least one breakpoint and checks both fields stay within 0..1.
    `PresetLibrary` now holds all three preset categories. `policy/PresetResolver::applyArcPreset` scales the
    preset's normalized points into `[startBar, endBar]`, replaces any existing breakpoints strictly inside
    that range on the target dimension (a preset deterministically sets its own scope, same philosophy as the
    other two categories overwriting fields rather than merging around stale state), and validates the target
    name against `ArcSet::getArcNames()` rather than trusting the caller (`ArcSet::setArc` silently no-ops on
    an unrecognized name — confirmed by reading it before writing the resolver, not left as a latent bug).
    UI: the Presets tab gained a third authoring section (breakpoint builder + library + an "Apply to Arc"
    action that writes straight into the same `ArcSet` the Arcs tab's graph editor already reads/writes, so the
    result is visible there immediately). `kPreferredHeight` bumped again for the growing Viewport content.
    Compiled and installed clean; not yet live-tested in Bitwig.
  - **Explicitly not done:** per-pattern preset fields (transpose/rotation/length/inversion) aren't included in
    any of the three categories — `ScenePattern` has no established "leave this field untouched" sentinel the
    way `SceneInstanceOverride` already does, so adding them needs a new convention, deliberately deferred
    rather than invented under time pressure.
- [x] **v1.1 generative assembly layer built (2026-08-18):** `policy/BlueprintGenerator::generate` assembles a
    full candidate `Blueprint` from the preset library — a deterministic, explainable rule table over one arc's
    breakpoint shape, not a model and not randomness (per the design doc's own principle: "randomness must be
    intentional, constrained... not raw chance"). Mechanism:
    - **One arc drives the section structure.** Each consecutive pair of breakpoints on the chosen "driving"
      arc (e.g. Energy) becomes one `BlueprintSection` — draw the shape once, the structure falls out of it.
      No separate section-count/boundary UI needed.
    - **Four archetypes**, classified purely from consecutive breakpoint values: `presentation` (flat),
      `build` (rising), `peak` (the one section ending — or starting, if the curve opens at its own max — at
      the driving arc's global maximum, guaranteeing exactly one true climax), `release` (falling after it).
      Mirrors the `presentation`/`local_climax_approach`/`global_climax`/`coda_echo_aftermath` vocabulary mined
      from `atonal_phrase_engine` back in v0.4 (`docs/atonal_phrase_engine_concepts.md`), finally implementable
      now that roles/budgets/arcs/presets all exist.
    - **Each archetype maps to a small fixed rule**, reusing every mechanism built this session: `peak` puts
      every registered instance in foreground and reserves each one's `inversion=on` so no earlier section can
      spend it first (apex exclusivity, automatic); `release` backgrounds `counterpoint` instances; `build`
      loosens `motif`/`counterpoint` budgets; `presentation` leaves static defaults untouched.
    - **Presets are selected by tag**, not invented — every role/rhythmic-relationship preset whose `tags`
      include the archetype's name (e.g. `"peak"`) gets applied (via the existing `PresetResolver` functions)
      onto a scene cloned from one user-chosen base scene. `RolePreset`/`RhythmicRelationshipPreset::tags`
      (present since the first preset shipped, never consumed by anything) is what makes this work.
    - **The other 4 arc dimensions are baked, not invented** — `density` is computed directly from the actual
      generated layer-role decisions (fraction of instances not backgrounded per section); tension/complexity/
      coherence use simple per-archetype baselines. All are written into a candidate `ArcSet`, never the live
      one, until commit.
    - **`ArcSet` gained an explicit copy constructor/assignment** (`src/policy/ArcSet.h/.cpp`) — it holds a
      `std::mutex`, which deletes the implicit copy operations; needed once a full-object snapshot (the
      candidate) became necessary. Each side locks its own mutex; no behavior change for existing callers.
    - **Preview-before-commit**, cheaply: `ui/ArcGraphView` was refactored to take any `ArcSet&` instead of
      always reaching into the live one via `ComposerCore` (`ArcGraphView::refreshFromArcSet()` added so it can
      be told to re-pull after the underlying `ArcSet` is swapped wholesale from outside) — the exact same
      drag/add/delete interactions now work unmodified on a draft. New **Generate** tab (`ui/GenerateView`, 6th
      outer tab): pick a base scene + driving arc, **Generate** builds the proposal entirely in memory (nothing
      touches `BlueprintLibrary`/`SceneLibrary`/the live `ArcSet`), shows a section/archetype summary plus the
      candidate curves in a reused `ArcGraphView`, and only **Commit** writes it for real (same as any other
      hand-saved blueprint — including auto-activating it, for consistency with every other "Save" in this
      app). **Discard** throws it away untouched.
    - Compiled and installed clean.
    - **Bug found and fixed in live testing (2026-08-18):** the user's first real test produced no audible
      change at all across sections. Root cause: they'd (very reasonably) named presets `"peak"`/
      `"presentation"` expecting the id itself to match, but `buildSectionScene` only checked `preset.tags`,
      which they'd left empty (the tags field's own placeholder literally says "optional," which was the
      trap). Every scene's `overrides=0` in the Scene Library display confirmed no preset ever applied.
      **Fixed:** `presetMatchesArchetype` now falls back to `preset.id == archetypeTag` when tags don't match,
      so a preset simply named after an archetype (no tagging required) now works exactly as intuitively
      tried. Zero re-authoring needed for presets already saved this way. Compiled and installed clean; not yet
      re-tested in Bitwig.
    - **Explicitly not done:** per-pattern preset fields still don't exist (see above), so generated sections
      only differentiate instances on activePattern/gridMode/swing, not transpose/rotation/length/inversion
      (aside from the Peak archetype's own reserved inversion value, which routing enforces, not presets).
      Multi-arc blending (e.g. letting a second arc influence section boundaries too) was explicitly scoped out
      in favor of one clean driving arc, per direct guidance. Running multiple Composer Mastermind instances
      with deliberately offset section timing to get emergent ensemble-level transition overlap needs no new
      code — already possible today, untested.

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

- [x] Role presets (anchor/motif/counterpoint/unrecognized=unrestricted) — `MutationPolicy::budgetForRole`,
      exact table in [mutation_policy_v0_1.md](mutation_policy_v0_1.md)
- [x] `policy/MutationPolicy` — `classifyWeight` (pure) + mutex-guarded per-instance per-bar ledger
      (`tryConsume`) plus a system-wide max-1-major-per-bar cap. The `noveltyBudget`-keyed-to-formal-position
      refinement from `atonal_phrase_engine_concepts.md` is explicitly **not** in this version — needs v0.5
      arc/section awareness a flat per-bar reset can't express; see design doc's v0.4 entry for why
- [x] `policy/PolicyEngine` — `authorize()`, added inside `Router::routeMutation` itself so scene-chain,
      `Scene::mutations`, and manual UI sends all pass through it with no per-caller opt-in needed
- [x] Change-weight classification (minor/medium/major) per [mutation_policy_v0_1.md](mutation_policy_v0_1.md)
- [x] `Router::routeMutation`/`routeScene` gained a `currentBar` parameter; `routeMutation` now returns `bool`
      (dispatched vs. blocked) instead of `void`; `ComposerCore::getCurrentBar()` added (atomic) so UI-thread
      manual sends have a bar to authorize against
- [x] Editor UI: Role combo added to the existing Instances row (no new row); "Send Mutation" status message
      now reports "blocked" vs. "sent" using the new bool return
- [x] Mutation-authorization/"protected zone" concern from 2026-08-15 — resolved by design, not built
      separately: a role's 0-budget for a weight class *is* its protection at this scope. A literal
      "protected during climax bar" rule still needs v0.5's section/arc awareness
- [x] **Live Bitwig test, bug found and fixed (2026-08-16)**: user hit "Add instance failed: id already exists"
      with the instance list showing "(no instances registered)" — two bugs, both fixed:
      1. `addInstanceClicked()`'s "already exists" failure path `return`ed before calling `refreshInstanceList()`,
         so any failed add left the visible list stale regardless of cause.
      2. `PluginProcessor::setStateInformation` (project reload restoring a previously-saved instance) had no
         way to tell an already-open editor its lists were now stale — nothing called back into the UI. Fixed:
         `PluginEditor::refreshAll()` (new, public) is now called from `setStateInformation` via
         `getActiveEditor()` when an editor is currently open.
      Root cause was #2; #1 just made the symptom worse by hiding it further.
- [x] **Mutation-budget gating confirmed working live (2026-08-16)**: after the fix above, instance `"1"`
      (Anchor role) correctly blocked a Transpose amount −16 mutation on the first attempt — `|amount| >= 12`
      classifies as Major, Anchor's major budget is 0, status label reported "blocked (policy budget exceeded
      for this bar, or unknown target)" exactly as designed. v0.4 is confirmed working, not just compiling.

## v0.5 — Arcs & Blueprints

- [x] `model/ComposerState.h` — `InstancePatternState`/`InstanceParameterState` (plain data)
- [x] `routing/InstanceStateTracker` — mutex-guarded last-known-CC-value per instance, recorded by `Router`
      after every successful send (new prerequisite, not originally its own checklist item)
- [x] Generic `Arc` evaluator (`policy/Arc`) — piecewise-linear breakpoints, `ArcSample` exposes value/delta/
      distance-to-next-breakpoint per the original spec. Compiles; **unconsumed** — nothing authors breakpoints
      yet, this is infrastructure for Blueprint JSON
- [x] Coherence/convergence evaluator (`policy/CoherenceEvaluator`) — averages grid-mode agreement,
      active-pattern agreement, swing closeness across registered instances. `ComposerCore::getCurrentCoherence()`
      + live display in the editor's instance list. Scope: instance-level dimensions only
      (`SceneInstanceOverride`'s territory) — per-pattern `Mutation` fields aren't folded in yet
- [ ] Coherence-as-*target* (an arc a section can author, not just measure) — needs Blueprint JSON's section
      concept to attach a target to; only the measurement half exists so far
- [ ] "Validate the designated climax scene is actually the convergence point" diagnostic pass over a scene
      chain — not built; needs a way to mark a scene as the intended climax first
- [ ] Cross-scene apex exclusivity (clamping half) — still needs scenes/sections to carry authored arc-target
      values to clamp against; not started
- [ ] Blueprint JSON schema + loader (sections, foreground/support/background, mutation budget per section) —
      deliberately not started this pass, wants real UI (tabs/pages) the editor doesn't have yet
- [x] **Live Bitwig test, confirmed working (2026-08-16)**: user watched the coherence score move from 1.0 down
      to 0.89 as instances (including a newly-added one not yet in sync) diverged — matches the formula exactly:
      with 3 instances, `(1.0 + 1.0 + 0.667) / 3 ≈ 0.89` when exactly one of the three tracked dimensions has a
      2-to-1 split. Confirmed live, not just compiling.

## v0.6 — Motivic Variation Engine

- [x] **MPL-side blocker cleared (2026-08-18):** CC 21 (Target Pattern) + CC 60-64 (Target Step/Note/Velocity/
      Duration/Enabled) wired into MPL's `handleExternalControlCC`, reusing MPL's existing Target-parameter
      system ("Option A"). Confirmed live in Bitwig: driving CC 21/60/61/62 selected a step and committed a new
      note/velocity to it. See [routing_policy_v0_1.md](routing_policy_v0_1.md)'s CC map section for the
      mechanism and the one documented residual risk.
- [x] **Motif/rule engine built (2026-08-20)** — `CCMapping.h` now has `encodeTargetPattern`/`encodeTargetStep`/
      `encodeTargetNote`/`encodeTargetVelocity`/`encodeTargetDuration`/`encodeTargetEnabled` (CC 21/60-64), the
      deliberately-deferred piece finally added. New `policy/MotifEngine` fires once per blueprint section
      boundary (from `ComposerCore::advanceBlueprintIfNeeded`, alongside the existing per-archetype effects):
      deterministic, no ML/randomness, matching `BlueprintGenerator`'s own philosophy. Picks a `MotifPreset` by
      tag-matching the section's `archetype` (same fallback-to-id convention `BlueprintGenerator` already uses),
      decides which registered instances to touch by role (`build`→motif/counterpoint, `peak`→everyone,
      `release`→counterpoint only, mirroring `BlueprintGenerator::applyArchetypeRules`'s own reach exactly),
      applies a deterministic per-archetype transform (`release`→retrograde, others plain), and writes via
      `CCDispatcher` using the newly-added CC 21/60-64 encoders — bypassing `Router`/`PolicyEngine`'s budget
      gating for now (that pipeline is pattern-level, has no step-index concept; threading step-level addressing
      through it is a bigger redesign deliberately deferred, not attempted this slice).
- [x] **`BlueprintSection.archetype` added** (`model/Blueprint.h`) — the archetype classification `BlueprintGenerator`
      already computed transiently now persists on the section itself (`"presentation"`/`"build"`/`"peak"`/
      `"release"`, or empty), settable by `BlueprintGenerator::generate` automatically or by hand via a new combo
      in `ui/BlueprintSectionsContent` — so motif rules work identically whether a blueprint was generated or
      hand-authored, not just generated ones. Serialized, validated (unknown values rejected).
- [x] **Cross-instance reasoning uses real state**: `MotifEngine` reads each touched instance's actual cached
      pattern content (`state/InstancePatternCache`, populated via the IPC awareness channel confirmed live
      yesterday) rather than assumed state — `"nudge"` mode adjusts existing enabled steps relative to their real
      current note/velocity/duration; `"phrase"` mode writes the full motif cell starting at the first enabled
      step, scaled from that step's real values. One concrete cross-instance rule implemented: notes landing on a
      pitch class another instance already used *in the same application pass* get nudged by a semitone to avoid
      exact doubling — skipped for `"peak"`, where doubling across voices is the desired convergence, not a
      collision to avoid. The engine updates the cache in place as it writes, so later instances in the same pass
      see earlier ones' results rather than stale pre-write content.
- [x] **Global `MotifApplicationMode` toggle** (Nudge/Phrase, session-level per the user's explicit request) —
      `ComposerCore::getMotifApplicationMode`/`setMotifApplicationMode`, exposed as a combo at the top of the new
      Motif Preset Builder section (`ui/PresetLibraryContent`) since it governs *how* any saved motif gets
      applied, not a property of one preset.
- [x] **New preset category: `MotifPreset`** (`model/Preset.h`) — a short relative pitch/rhythm cell
      (`MotifNote{semitoneOffset, relativeDuration, relativeVelocity}`), same CRUD/serialization/validation/UI
      pattern as the other three categories. Built and confirmed working (UI) 2026-08-20, before the engine that
      consumes it.
- [x] **Confirmed working live (2026-08-20)**: before/after Awareness-tab snapshots showed real note/velocity/
      duration changes on MPL1/MPL2 after playback crossed into a Peak section. Root-caused why it took ~40 bars
      and why MPL3 hadn't changed yet: `eligibleInstances()` only touches `motif`/`counterpoint` roles for
      Build/Release (mirroring `BlueprintGenerator`'s own reach), and the test instances were all role
      "unrestricted" - only Peak touches everyone regardless of role. Not a bug; flagged as a real semantic
      inconsistency worth revisiting ("unrestricted" means *no limits* in the budget system, but meant *excluded*
      here) - user's resolution was assigning real roles rather than changing the engine.
- [x] **`InstanceRegistry`/`ui/InstanceView` fixed along the way**: `addInstance` was rejecting duplicate ids
      instead of upserting (the only library in the codebase not following the established `addOrReplace*`
      convention), which is why changing an instance's role required Remove-then-re-Add. Fixed to upsert.
      `InstanceView`'s Remove also required retyping the exact id (same friction already fixed on
      `ModulatorTargetView` the day before) - fixed with an existing-instances combo for Remove, plus a new Load
      button that populates id/channel/role for editing.
- [x] **Pass-based variation sequencing built (2026-08-20)** — the "fires once and goes static" limitation the
      user flagged after live-testing ("one change at bar 30, one change at bar 50... a bit lazy"). `MotifEngine::
      applyForSection` gained an `int passIndex` parameter; `ComposerCore` now fires an additional pass every
      `kPassIntervalBars` (4) bars for as long as a section stays active, not just once at its boundary
      (`fireMotifPassIfDue`, called from `advanceBlueprintIfNeeded` on ticks where the active section didn't
      change; `motifPassCountForSection`/`lastMotifPassBar` track state under `blueprintMutex`, reset whenever the
      active section changes or a new blueprint is set). Each successive pass now also rotates through the full
      transform vocabulary — `MotifEngine.cpp` gained `invert()` (negate every note's semitone offset) and
      `rotate()` (cyclic shift) alongside the existing `retrograde()`; `transformForPass(archetype, notes,
      passIndex)` applies the archetype's own base transform (unchanged: retrograde for Release, plain otherwise)
      then cycles `passIndex % 4` through `{base, rotate, invert, retrograde}` — and rotates which enabled step
      each pass starts writing from (`passIndex % enabledStepIndices.size()`), so consecutive passes spread across
      the whole pattern instead of repeatedly hammering the first few steps. Phrase mode's step placement switched
      from clamping to modulo-wrapping (`wrapStep`) so a phrase starting near the end of a pattern doesn't just
      clip. Builds clean; not yet live-verified in Bitwig this session.
- [x] **Pattern-index bug found and fixed (2026-08-20)**: `MotifEngine` hardcoded `patternIndex = 0` for every
      read and write, on the (wrong) assumption that instances mostly sit on P1. Live-tested and confirmed a real
      symptom: Awareness tab showed cache changes after entering the Peak section, but nothing was audible -
      because the cache updates optimistically the instant `MotifEngine` *sends* a write, regardless of whether
      that write landed anywhere real, and the actual writes were landing in P1 while the touched instances were
      really playing P2/P3 for that section's scene. Fixed: `applyForSection` now takes a `const
      InstanceStateTracker&`, looks up each touched instance's real last-set Active Pattern (recorded by every
      `Router::routeScene` call) and targets that pattern instead of a fixed index (`activePattern - 1`, since the
      tracker's convention is 1-indexed with 0=stopped; an instance explicitly stopped, e.g. via a background
      layer role, is skipped rather than written to). Still depends on the Awareness tab having a real resync for
      whichever pattern is actually active per instance/section - the engine still won't write blind to a pattern
      it has no cached content for, even the genuinely correct one. Built, installed, not yet live-tested.
- [x] **MPL-side commit bug found and fixed (2026-08-20)** — after the pattern-index fix above, live-testing
      still showed zero audible change despite Awareness updating. MPL's own UI showed CC 21/60-64 arriving and
      "accepted", but Target Step selection never advanced past step 1 on any instance. Root cause in MPL's
      `syncEngineFromParameters()` (`Source/PluginProcessor.cpp`): its once-per-block poll did
      `if (selectionChanged) browse(); else if (valuesChanged) commit();` - mutually exclusive. `MotifEngine`
      always sends a step selection and new note/velocity/duration/enabled together in one burst, so both flags
      were true by the next poll and browse always won, silently reloading the newly-selected step's *existing*
      content back over the incoming values - every write that also changed step selection (i.e. nearly all of
      them) was discarded with no error anywhere. Fixed by making commit take priority: `if (valuesChanged)
      commit(); else if (selectionChanged) browse();`. Confirmed safe for normal human UI use (clicking a step
      calls `setTargetPatternAndStep()` directly, bypassing this poll entirely via `suppressParameterSync`).
      Also fixed in passing: `PluginEditor.cpp`'s status line duplicated its own "Last CC: " prefix.
      `docs/routing_policy_v0_1.md`'s residual-risk note updated to reflect what was actually found.
- [x] **CONFIRMED WORKING LIVE (2026-08-20)**, after both bugs above: "sparse but clearly audible melodic
      mutations." Closes out the v0.6 first-build arc end to end. Two honest, expected gaps flagged from
      listening: sparse (by design - `kPassIntervalBars=4`, one preset's worth of notes per pass), and rhythm
      untouched (real - `MotifEngine` only ever writes CC 60-64, deliberately never touches the existing
      Rotation/Length mutation system on CC 30-53). Follow-up musical-refinement plan agreed: bounded/centered
      pitch movement first, then CC20-driven call-and-response between instances, then rhythm involvement via the
      existing mutation system - none built yet.
- [x] **Bounded, centered pitch movement (2026-08-20)** — first of the agreed musical-refinement round. Every new
      note is now clamped to within one octave (`kMaxDeviationFromCenterSemitones`) of its pattern's own current
      average pitch (`patternCenterNote`, average of enabled steps' notes, recomputed fresh each pass from the
      live cache - so the pattern's own center can still drift slowly and organically over many passes, but no
      single pass can throw an outlier far from wherever it actually sits). Re-clamped after collision avoidance
      too, since that can nudge a note beyond the bound while searching for a free pitch class. Preventive, not a
      response to an observed failure: Nudge mode's delta-from-cached-note had nothing else pulling it back.
- [x] **CC20 call-and-response (2026-08-20)** — second of the agreed round. `MotifEngine` now decides per pass,
      per touched instance, whether it should be audibly playing or resting (real CC20 stop/resume, not just
      skipping note edits) - deterministic by `passIndex`, no randomness: Peak never rests anyone (convergence);
      Release lets only one instance play at a time, rotating (genuine solo passages, matching its existing
      thinning-texture identity); Build/other 2+-candidate archetypes rest exactly one per pass (a duo trading
      off). An instance's "home" pattern for the section (what to resume to) is captured once when the section
      becomes active (`ComposerCore::motifHomePatternForSection`, snapshotted right after the section's scene
      routes) rather than re-read live, since the engine's own toggling makes live `InstanceStateTracker` state
      unreliable as a "home" once it starts fluctuating pass to pass; home pattern 0 means the section itself
      wants that instance silent throughout (e.g. a background layer role) - never revived. `applyForSection`'s
      `InstanceStateTracker&` is non-const now, since a toggle calls `recordGlobal` directly, the same way
      `Router` does after a real scene route.
- [x] **Rhythm involvement (2026-08-20)** — third and final item of the agreed musical-refinement round.
      Deliberately *not* built inside `MotifEngine` (which stays scoped to CC 60-64/CC20, its documented
      boundary) - instead a new `ComposerCore::applyRhythmForSection(section, passIndex)`, fired on the exact
      same pass cadence as `applyMotifForSection` (both call sites in `advanceBlueprintIfNeeded`/
      `fireMotifPassIfDue`), going through the *existing* Mutation/Router/PolicyEngine pipeline (Rotation/Length,
      CC 30-53) rather than reimplementing rhythm logic - so rhythm changes are budget-gated and apex-exclusivity-
      aware the way note edits still deliberately aren't. Alternates which parameter moves (Rotation vs Length by
      pass parity) and which direction (a different parity bit), so groove/loop-length "breathes" rather than
      marching to an extreme and staying there - same restraint the note transform rotation already has.
      Rotation amount stays within `MutationPolicy::classifyWeight`'s Minor threshold (|amount| <= 2) on purpose.
      New public `MotifEngine::eligibleInstancesForArchetype` exposes the existing role-eligibility rule (single
      source of truth, reused rather than duplicated) so rhythm touches the same instances note-editing does per
      archetype. Reuses `motifHomePatternForSection` (not live tracked state) for the same reason
      `applyMotifForSection` does. Built and installed clean, MPL-side unchanged (existing CC 30-53 protocol).
- [ ] Known, deliberate scope limits for this first build, not oversights: no budget/PolicyEngine gating on the
      CC 60-64 note-writing path specifically (rhythm mutations above *are* now gated); only one motif preset can
      be chosen per archetype (first tag match wins, no weighting/rotation).
- [x] **Mutation preview readout (2026-08-20)** — unrelated to the Motivic Variation Engine, prompted by user
      feedback while manually testing it: the Mutations tab's Amount slider is a delta from the target instance's
      currently-tracked value (kept deliberately, not reverted to absolute - see `docs/routing_policy_v0_1.md`'s
      rotation-wrap history for why absolute mutation values are the wrong default), but nothing showed *what*
      the current value actually was, so predicting the outcome meant doing the arithmetic by hand or checking
      Awareness first. `ui/DebugPanel` gained a live "current -> new" label under the Amount slider
      (`describeMutationPreview`/`refreshMutationPreview`), computed via `InstanceStateTracker::getState` using
      the exact same clamp/wrap-per-type logic `Router::routeMutation` itself applies, refreshed on every control
      change and on the shared 300ms tab timer. `sendMutationClicked`'s status message now echoes the same
      preview text so the confirmation reads as "sent transpose to X: current 9 -> new 7", not just "sent".
- [x] **Send Test CC diagnostic (2026-08-19):** unrelated to the Motivic Variation Engine itself, but motivated by
      the same CC 60-64 work — the Mutations/Debug tab gained a "Send Test CC" control group: pick an instance and
      a named CC (new `CCMapping::kNamedCCs` table, all 21 CCs in the protocol), set a value, fire one raw CC via
      `CCDispatcher` immediately, bypassing Mutation/Router/PolicyEngine. Built to pair Bitwig's `MIDI Modulator`
      device's own "Learn CC" to a known CC number on demand — see
      [routing_policy_v0_1.md](routing_policy_v0_1.md) and the Bitwig-advisor memory notes for why Bitwig's
      general "Map to Controller or Key" can't be used for this (hardware-CC-only restriction). Built and
      installed clean; not yet tested against a real Learn-CC pairing in Bitwig.

## v0.6 Composer Bridge read channel — SysEx attempt superseded by local IPC (2026-08-19)

"Read channel + cache + awareness UI first" — the first of the two agreed build slices for the motif/rule engine
(the rule engine itself, motif-library presets, and the write half are the second slice, not started). See
`docs/composer_mastermind_design.md`'s v0.6 section and `docs/routing_policy_v0_1.md`'s Composer Bridge update.

**First attempt (SysEx over MPL's Composer Bridge protocol) built, tested, and then found to be blocked by
Bitwig itself, same day.** MPL's protocol was bumped to v2 (per-instance channel filtering) and confirmed working
via its self-test suite - but live testing in Bitwig showed dump requests never got a response, even after ruling
out a stale build and confirming the routing mechanism (loopMIDI) itself worked fine for CC. Root cause,
corroborated by multiple independent community reports: **Bitwig does not deliver incoming SysEx to a hosted VST
instrument's plugin code at all**, regardless of routing path (track-to-track, hardware, or virtual cable) - a
host-level limitation, not a bug in anything built here. `midi/ComposerBridgeCodec` and the SysEx-based
`composer/PatternSyncCoordinator` were removed from Composer Mastermind entirely (not kept as dead/unused code).
MPL's own SysEx Composer Bridge protocol (including its v2 channel-filtering fix) is untouched and stays useful
for non-Bitwig contexts (other hosts, hardware, the Standalone test harness it was validated against) - it just
can't be this specific read channel while both plugins run inside Bitwig.

**Replaced with direct local-socket IPC between the two plugins** - user's explicit call ("this sounds like the
engineering level only you can be capable of... option B is very appealing"), choosing real duplex awareness
over falling back to a self-tracked/assumed model. Since MPL and Composer Mastermind are both JUCE plugins in the
same Bitwig process, they talk directly over `juce::InterprocessConnection`/`InterprocessConnectionServer` (a
local TCP loopback socket, fixed port 47823) - bypassing Bitwig's MIDI graph and its SysEx limitation entirely.

- [x] `model/PatternSnapshot.h` — unchanged from the SysEx attempt; `StepSnapshot{enabled,note,velocity,duration}`,
      `PatternSnapshot{patternIndex, steps}`.
- [x] `state/InstancePatternCache` (unchanged) — ground truth per (instance, pattern), written only from
      confirmed responses, still deliberately separate from `InstanceStateTracker`.
- [x] `composer/PatternSyncServer` (new, replaces `PatternSyncCoordinator`) — a `juce::InterprocessConnectionServer`
      on Composer Mastermind's side. Each connecting MPL instance gets its own persistent `PatternSyncConnection`,
      self-identified via a `{"type":"hello","channel":N}` JSON message on connect - a real duplex connection
      already disambiguates which instance a message came from, so (unlike the SysEx attempt) there's no
      channel-echo-per-response bookkeeping needed at all. `requestSync(channel, patternIndex)` finds the
      matching live connection and sends a JSON request directly; responses are handled on the JUCE message
      thread (safe to touch mutex-guarded state directly, no audio-thread buffer draining needed this time).
      Channel→instanceId resolution and "what bar is it" are injected via `ChannelResolver`/`CurrentBarProvider`
      callbacks, mirroring `routing/Router.h`'s existing resolver-injection pattern.
- [x] MPL side: new `Source/ComposerBridgeIpcClient` (`juce::InterprocessConnection` client, reconnects on a
      2-second timer if not connected), built entirely on MPL's existing public step accessors
      (`stepHasNote`/`getStepNote`/`getStepVelocity`/`getStepDurationSteps`) - no new internals exposed. Respects
      the same "Composer Bridge Enabled" toggle the SysEx protocol already uses.
- [x] `PluginProcessor::processBlock`'s SysEx receive/send wiring reverted - IPC doesn't touch the MIDI buffer at
      all, so the bug fix that made Composer Mastermind actually read incoming MIDI (previously discarded
      unconditionally) is now moot for this purpose, though worth remembering if MIDI input is ever needed again.
- [x] **Awareness** tab (`ui/PatternAwarenessView`) updated to match: shows "N of M registered instances connected
      via IPC" (persistent-connection status, replacing the old pending-SysEx-request count) alongside the same
      cached-pattern-content display as before.
- [x] **Confirmed live in Bitwig (2026-08-19), all 3 instances:** Awareness tab reads "3 of 3 registered
      instance(s) connected via IPC," with real captured content for MPL1/MPL2/MPL3's patterns (distinct
      note/velocity/duration per step, matching what's actually in each instance). Two real bugs found and fixed
      along the way, both now resolved: (1) MPL's own `externalControlChannelParam` must be set to the *specific*
      channel matching its Composer Mastermind registration, not "All" - "All" broke channel-based identification
      even though the socket connected fine; (2) `ComposerBridgeIpcClient` now re-sends its `hello` every
      reconnect-timer tick (not just once at connect) so a live channel change self-heals instead of leaving
      Composer Mastermind holding a stale value. One practical simplification this whole pivot unlocks: the
      loopMIDI routing set up for the *return* leg (MPL output → CM input) during the abandoned SysEx attempt is
      no longer needed for anything and can be removed - the forward leg (CM → MPL, for CC 20-64 and Modulator
      Target CC 70+) is unaffected and still required.

## Modulator Targets — driving Bitwig's own modulators (2026-08-19)

Grew out of the Bitwig-advisor exploration, not the Motivic Variation Engine — a new, independent system for
letting Composer Mastermind's arcs/sections drive Bitwig modulator parameters (Random/Steps/Curve Rate/Depth/
Shape) directly, via CC numbers paired in Bitwig itself (loopMIDI + a modulator's own "Learn CC", confirmed
working live 2026-08-19 - see the Bitwig-advisor memory notes for the full mechanism and why Bitwig's general
"Map to Controller or Key" doesn't work for plugin-generated CC).

- [x] `model/ModulatorTarget.h`: `{id, ccNumber, midiChannel, mode ("arc"|"section"), arcDimension}`. Raw CC
      0..127 only - Bitwig's own Learn-CC binding auto-scales onto the mapped parameter's real range, so there's
      no min/max/encode concept here, unlike `CCMapping.h`'s MPL-specific encoders.
- [x] `model/Blueprint.h`: new `SectionModulatorValue {modulatorTargetId, value}`, `BlueprintSection.modulatorValues`.
- [x] `state/ModulatorTargetLibrary` (mirrors `SceneLibrary`), wired into `ComposerCore`, `StateSerializer`,
      `Validation::isValidModulatorTarget`, `StateSnapshotStore`.
- [x] **Arc mode (continuous):** `ComposerCore::sendModulatorTargetUpdates(currentBar)`, called every
      `processBar` tick regardless of blueprint state - for each "arc"-mode target, evaluates the named
      `ArcSet` dimension and sends the raw CC via `CCDispatcher` every bar. Encoding reuses
      `CCMapping::encodeFloat(value, 0.0f, 1.0f)` (Arc values are conventionally 0..1).
- [x] **Section mode (stepped):** `ComposerCore::sendSectionModulatorValues`, called once from
      `advanceBlueprintIfNeeded` alongside the section's own scene, the same "fire once on section-boundary
      change" pattern everything else in the blueprint player already uses. A target with no authored value for
      the active section simply isn't sent - it keeps whatever value it last held.
- [x] New **Modulators** tab (`ui/ModulatorTargetView`, 7th `EditorView` tab): register/remove targets.
- [x] Blueprint tab's Sections page (`ui/BlueprintSectionsContent`) gained an "Add Modulator Value" pending-list
      row, mirroring the existing Reserved Value UI exactly - picks a registered target + a 0..127 value, included
      in `BlueprintSection.modulatorValues` when the section is added/updated.
- [x] Built and installed clean (`CMakeLists.txt` updated with `ModulatorTargetView.cpp`/`ModulatorTargetLibrary.cpp`).
- [x] **First live test (2026-08-19) found a real UX gap, fixed same session:** pairing succeeded, but with an
      "arc"-mode target already live (sending its CC every bar while playing), a *new* Learn-CC pairing attempt
      would race against that live stream - and the only way to remove/re-send a specific target was retyping its
      id into the add-row text field. Fixed: `ui/ModulatorTargetView` gained a proper existing-targets combo
      (mirrors `ui/SceneListComponent`'s pattern) with **Remove** and **Send for Pairing** (sends one raw CC via
      `CCDispatcher`, arbitrary test value - Bitwig's Learn CC only needs to see the CC *number* arrive) acting on
      the selected entry, not retyped text. Confirmed `processBar` - and therefore all automatic arc-mode CC - only
      fires on live bar-boundary changes during playback, so pairing with the transport stopped avoids the race
      entirely; "Send for Pairing" is for pairing while playing, or when transport is stopped and nothing else is
      firing. Built and installed clean.

## MCP Bridge — read-only introspection (2026-08-20)

After a full session of debugging v0.6 by screenshot round-trip (pattern-index bug, MPL's commit-priority bug,
preset-tag coverage gaps), user asked for a custom MCP server exposing Composer Mastermind's own internal state
directly, distinct from WigAI (which only sees generic Bitwig device parameters, not this plugin's own UI text
fields). Scoped as two halves, same "transport vs. logic" split as `PatternSyncServer`:

- [x] **`src/composer/McpBridgeServer.h`/`.cpp`** — local JSON request/response socket on `127.0.0.1:47824`,
      built on the exact same `juce::InterprocessConnection`/`InterprocessConnectionServer` pattern as
      `PatternSyncServer`, one port higher. Pure transport - no knowledge of `ComposerCore`, just a
      `RequestHandler` injected the same way `PatternSyncServer`'s resolvers are. Protocol:
      `{"action":"<name>"}` in, `{"ok":true,"result":{...}}` or `{"ok":false,"error":"..."}` out.
- [x] **`ComposerCore::handleMcpBridgeRequest`** — the actual action table, read-only first slice: `getStatus`
      (bar/coherence/active section/instance count), `getInstances` (id/channel/role/enabled + coherence),
      `getAwareness` (the full pattern cache, same data `ui/PatternAwarenessView` shows), `getBlueprintStatus`
      (active blueprint id/name/current bar/active section + every section's id/scene/bar-range/archetype),
      `getMotifPresets` (id/tags/notes + current Nudge/Phrase mode). Unknown actions return a clean error, not a
      crash. `CMakeLists.txt` updated with the new source file.
- [x] **`mcp-bridge/server.py`** (new top-level folder, Python, `mcp` SDK's `mcp.server.mcpserver.MCPServer`) -
      the actual MCP-speaking half. Runs `--http` (streamable-http on `127.0.0.1:8765/mcp`, `TransportSecuritySettings
      (enable_dns_rebinding_protection=False)` - the SDK's DNS-rebinding guard has no true glob, only exact-match
      or `:*` suffix, so it rejects a tunnel hostname otherwise) tunneled via `cloudflared tunnel --url
      http://127.0.0.1:8765` and registered as a custom HTTPS connector - **not** `claude mcp add` as originally
      planned, since this environment is not the Claude Code CLI (no local `claude` binary, no stdio MCP
      support) and its connector UI only accepts remote HTTPS, same constraint WigAI already has. Tunnel URLs
      are ephemeral and need regenerating + re-pairing each session. See `mcp-bridge/README.md`.
- [x] **Verified end to end, not just compiling**: (1) raw socket test (hand-rolled Python client replicating
      JUCE's exact 8-byte little-endian `[magic, size]` frame header) against a running Standalone build -
      confirmed all 5 read actions return well-formed JSON, plus the unknown-action error path; (2) full MCP
      round trip over the real HTTP+tunnel connector from a live session - confirmed live 2026-08-20.
- [x] **Write actions added (2026-08-20):** `resyncInstance`, `setMotifApplicationMode`, `sendTestCC`,
      `sendMutation`, `commitBlueprint`, `setScene` - full read+write control per explicit user request ("You
      are allowed to take full control over it. Don't hold off").
- [x] **Compose actions added (2026-08-20):** `createScene`, `createBlueprint`, `createMotifPreset`, `setArc` -
      reuse the exact same `StateSerializer::varToScene`/`varToBlueprint`/`varToMotifPreset` deserializers and
      `Validation::isValidScene`/`isValidBlueprint`/`isValidMotifPreset` checks project-save/load already uses,
      so a bridge-authored scene/blueprint/preset is validated identically to a hand-authored one. Built
      specifically so an original composition could be authored entirely through the bridge and pushed into live
      state - see "Vers la flamme" below. Known gap: no `getArc` read action yet, so a `setArc` call can't be
      read back to confirm it landed short of a live audible/visual check.
- [ ] **Known, deliberate scope limits, not oversights**: fixed port only, no multi-instance-of-the-plugin
      support; no auth (fine for a local single-user dev tool, not for anything beyond localhost).

### "Vers la flamme" — an original composition authored entirely through the bridge (2026-08-20/21)

User's explicit ask: given everything built, could the assistant act as the actual composer - pick a mood, and
express a full piece as blueprint/scene/preset JSON pushed live through the bridge, not just described? Built and
pushed live: a 56-bar, 4-section piece (Scriabin's *Vers la flamme* as reference), roles MPL1=anchor/MPL2=motif/
MPL3=counterpoint. Presentation (1-8) holds MPL3 in reserve (background layer role + `activePattern=0` override).
Build (9-32) rises via `flame_rise` (ascending `[0,2,5,9]`, crescendo). Peak (33-48) switches to ternary grid +
20% swing (a real structural gesture) and converges via `flame_converge` (tight oscillating `[0,1,0,-1]`, high
sustained values); MPL2's transpose=5 is reserved at Peak (apex exclusivity). Release (49-56) silences MPL1+MPL2
(background layer role + override), leaving only MPL3's `ember_fall` (falling mirror `[0,-2,-5,-9]`, diminuendo).
Live-tested across many rounds this session (see the two entries below) - most of tonight's real bug-finding
happened while listening to this specific piece run.

### Preset tag-collision bug found and fixed (2026-08-21)

Live testing found Peak audibly using the wrong content. Root cause: `MotifEngine`'s preset-matching (tag match,
id-fallback) checked each preset in list order and stopped at the first hit - a leftover test preset literally
named `"peak"` (from early bridge testing, tagged `"build"`) matched Peak's `presetId == archetype` fallback
before `flame_converge` (correctly tagged `"peak"`) was ever reached, and for Build, `"peak"`'s own stray
`"build"` tag beat `flame_rise` by pure list order even though both were genuine tag matches. Fixed the
fallback-vs-tag ordering (`findPresetForArchetype` now does two real passes: every preset's tags first, id only
if no tag matched anywhere) - which resolved Peak, but a genuine tag-vs-tag collision (both presets legitimately
tagged the same archetype) isn't something matching-priority logic can resolve at all. The real fix was data
hygiene: the three leftover test presets (`peak`/`peak2`/`motif1`) were retagged to `"_deprecated"` via
`create_motif_preset` (create-or-replace) so they can never match a real archetype again.

### Length-window / CC64 interaction bug found and fixed (2026-08-21)

Live testing at Peak found MPL1 showing zero active steps within its audible window despite the Awareness cache
believing content existed. Root cause, two-part: (1) `toggleOneStep`'s own "keep >= 1 enabled" safeguard operated
across the full 16-slot array regardless of a since-shrunk tracked Length, so the one surviving enabled step
could land outside the now-shorter audible window; (2) `applyForSection`'s own top-of-loop "is this pattern
empty" check (which decides whether to even call `toggleOneStep`'s recovery path) *also* scanned the full
16-slot array, so a straggler step outside the window was wrongly treated as "real content," and every pass kept
nudging that inaudible step in place instead of ever recovering something in-window. Fixed by bounding both the
`enabledStepIndices` scan and `toggleOneStep`'s own candidate scan to the tracked Length window consistently,
with an active recovery branch (force-enable one in-window step) for both "shrunk window" and "already fully
silent" cases.

## v0.6.1 — Section-Entry Stamping & Direct Pattern-Content Writes (2026-08-21)

User's framing ("preparatory phase"): the motif engine could only ever *nudge* whatever sparse content already
happened to exist in MPL - it never authored a section's actual seed content. Two related builds, both same day:

- [x] **`MotifEngine::stampMotifForSection`** - authors a section's real seed content once, at section entry,
      instead of leaving `applyForSection` to nudge leftover manual steps forever. Writes the matched
      `MotifPreset`'s base shape (no pass-cycling - that's what the *following* per-pass calls are for) evenly
      spaced across each eligible instance's tracked Length window, fully enabled, every other step silenced.
      Unlike `applyForSection`, "presentation" is a real archetype here (`eligibleInstances` gained a
      `"presentation"` branch - touches everyone, matching `"peak"`'s reach) - the opening theme gets the same
      deliberate authorship as every later section. Called once at section entry INSTEAD OF
      `applyForSection`'s own passIndex-0 call (double-applying would compound the relativeVelocity/
      relativeDuration scaling); ordinary per-pass development resumes from passIndex 1. A new preset,
      `flame_theme` (tags `["presentation"]`, `flame_rise`'s own `[0,2,5,9]` cell held back - calmer velocity/
      longer duration), was authored via the bridge so "Vers la flamme"'s Presentation actually has real content
      to stamp.
- [x] **Direct pattern-content writes over `PatternSyncServer` IPC, replacing CC 60-64 (2026-08-21):** live
      testing of the stamp (writing 4-5 steps per instance in one call, more than anything written before this
      session) found real MPL content diverging badly from what Composer Mastermind's cache believed it wrote -
      a fresh resync showed only 1-2 of 4-5 intended steps landed, sometimes with corrupted values. Root cause,
      found by reading MPL's own source: CC 60-64 (Target Step Editing) doesn't write pattern data directly -
      it sets ordinary APVTS parameters, committed via MPL's `syncEngineFromParameters()` polling *once per
      audio block* with edge-detection ("selection changed -> browse, value changed -> commit"), built for one
      human turning one knob at a time. Several full step-edits landing within the same block collapse to
      whatever the last one left the parameters at before that poll ever runs - every earlier write in the
      block is silently lost. A first fix paced writes to one step per audio block (`ComposerCore::
      drainPendingStampWrites`, a `PendingStepWrite` queue) - worked, but was superseded same session by a
      better fix: MPL's own `Docs/ComposerBridgeProtocol.md` SysEx commands 0x01 (Write Step)/0x04 (Write Full
      Pattern) already commit directly via `setStepValues`/`clearStep`, bypassing the parameter/poll pipeline
      entirely - but raw SysEx can't reach a hosted plugin in Bitwig at all (the same limitation that forced
      `PatternSyncServer`'s IPC reads to exist - see the v0.6 Composer Bridge section above). Since
      `PatternSyncServer` already has a proven-reliable duplex local-socket connection to each MPL instance,
      it was extended with two new JSON message types instead: `writeStep`/`writeFullPattern`
      (`PatternSyncServer::sendWriteStep`/`sendWriteFullPattern`, MPL's `ComposerBridgeIpcClient::
      messageReceived` handling them by calling `setStepValues`/`clearStep` directly - the same calls the SysEx
      commands make). `MotifEngine` now takes a `PatternSyncServer&` instead of a bare `InstancePatternCache&`
      for all step-content writes (`writeStepDirect`, `toggleOneStep`, `stampMotifForSection`'s one
      `sendWriteFullPattern` per instance); `CCDispatcher` is now only used for CC20 (Active Pattern rest/
      resume), a genuine single scalar value with no polling ambiguity. The pacing scaffolding
      (`PendingStepWrite`/`drainPendingStampWrites`) was removed again once this landed - atomic IPC writes
      don't need per-block pacing at all. Confirmed working live in Bitwig (2026-08-21): stamped content matched
      exactly between Awareness and a real MPL resync, no partial/corrupted steps.
- [ ] **Not yet built**: an ACK response for `writeStep`/`writeFullPattern` (currently fire-and-forget, same as
      the old CC path was) - would let Composer Mastermind confirm a write actually landed rather than trusting
      its own optimistic cache update, closing the same class of gap `getArc` above is missing for `setArc`.

### Presentation baseline reset, gentle transpose, and "Prime for Playback" (2026-08-21)

Live-testing "Vers la flamme" (see above) surfaced two more real gaps, both closed same day:

- [x] **Replay-drift bug found and fixed:** clearing a pattern's steps and replaying from bar 1 produced correct
      results, but replaying with patterns already in a *drifted* state from a previous playthrough stacked new
      mutations on top of old ones ("everything too high in range"). Two-part cause, both now reset at Presentation
      entry via `ComposerCore::resetRhythmBaselineForSection` (direct CC/IPC, bypasses Mutation/PolicyEngine -
      structural, not a budget-gated musical change): (1) Transpose/Rotation/Length CCs from the prior run were
      never reset - fixed by sending them back to 0/0/`kPatternSteps`; (2) `stampMotifForSection`'s own register-
      continuity math (deliberately re-centers around a pattern's *existing* pitch center, so later sections drift
      organically from earlier ones) was recentering around the cache's belief about the *previous* playthrough's
      already-drifted content - fixed by also clearing step content (`writeFullPattern` all-disabled + matching
      cache update) immediately before the stamp runs, so its own "no existing content -> middle C" fallback is
      the branch that actually fires. Found in two passes: the parameter reset alone wasn't sufficient, confirmed
      live before the step-content clear was added.
- [x] **Presentation gentle transpose:** per user's own framing ("a motif can be recognized even when transposed
      slightly," so an exact 1-bar repeat isn't required to preserve the "no mutation during Presentation" rule's
      intent), `applyRhythmForSection` no longer fully skips `"presentation"` - it now runs *only* the transpose-
      curve dimension, at a new, much gentler `kPresentationTransposeAmplitudeSemitones` (2, vs. the full curve's
      5) on the same shared sine wave. Rotation/Length and step content stay completely frozen during Presentation
      - only whole-pattern register drifts, which preserves motif identity rather than changing it.
- [x] **"Prime for Playback" (user's own idea):** since these writes land instantly regardless of transport state,
      there's no real reason blueprint setup has to wait for playback to actually begin - and priming while
      genuinely stopped sidesteps any race with MPL's own playback engine reading pattern data at the same moment
      audio starts (the root cause behind most of this session's CC-burst bugs). `ComposerCore::advanceBlueprintIfNeeded`'s
      "enter this section" sequence (scene route, baseline reset, stamp, first rhythm pass) was factored out into
      a new `ComposerCore::enterSection`, reused unchanged by both real section-boundary crossings and by a new
      `ComposerCore::primeForPlayback()` (finds the current blueprint's first section by lowest `startBar`, calls
      `enterSection` on it directly) - one code path, not two to keep in sync by hand. Deliberately leaves
      `currentActiveSectionId`/`motifPassCountForSection`/`lastMotifPassBar` untouched, so real playback's own
      section-change bookkeeping re-enters the first section normally the moment it's reached, harmlessly
      re-applying the same already-clean state rather than priming trying to fake "already there." New **Prime
      for Playback** button, Blueprint tab's Sections page, next to the "now playing" label. Confirmed working
      live in Bitwig (2026-08-21): pressing it while stopped immediately showed fresh Presentation content in
      MPL's own UI; playback then started clean from bar 1 with no settling period needed.

### P1/P2/P3 phrase-chaining (2026-08-21, user's own idea from earlier in the session)

Real gap the user identified while discussing the CC list directly: the engine only ever worked with one "home"
pattern per instance per section - real musical phrases (antecedent/consequent, ABA) need to move between an
instance's three patterns, not just nudge one forever. Presented as a design proposal before building, since it
touches every existing consumer of `motifHomePatternForSection` (call-and-response rest/resume, note-editing,
rhythm mutation) - approved as described.

- [x] **Seeding, `MotifEngine::seedPhraseChainPatterns`:** at section entry, seeds all three of an eligible
      instance's patterns at once with related transforms of the section's matched preset - P1 the base shape,
      P2 rotated, P3 inverted (the same `rotate`/`invert` vocabulary `transformForPass` already cycles pass to
      pass, here distinguishing the three chained patterns instead). Refactored the shared "compute + atomically
      write one pattern" logic out of `stampMotifForSection` into a new `stampOnePattern` helper, reused by both -
      no duplicated register-continuity/Length-window math between the two callers. Called once at section entry,
      right after `stampMotifForSection` (in addition to it, not instead - that call still establishes whichever
      pattern the section starts on as "home"). Same "won't write blind" restraint as everywhere else in this
      file: a pattern index with no confirmed cache entry is silently skipped, which in practice means P2/P3 need
      at least one Resync Now (for that specific pattern index) before this does anything audible for an instance
      that's never used them before - a real one-time setup step, not a bug.
- [x] **Chaining scheduler, `ComposerCore::firePhraseChainIfDue`:** new `kPhraseChainIntervalBars` (8, deliberately
      slower than `kPassIntervalBars`'s 2 - a phrase change is a bigger gesture than a single-pass nudge). Every
      interval, advances each touched instance's home pattern to the next in a 1->2->3->1 cycle by mutating
      `motifHomePatternForSection` directly (every existing consumer already re-reads that map fresh each call, so
      this is the *only* place that needs to know the chain exists), then sends the real CC20 Active Pattern
      switch - but only if the instance is currently audible (`InstanceStateTracker`'s tracked `activePattern !=
      0`). An instance CC20's own call-and-response has rested keeps its rest; its chain position still advances
      silently underneath, so it resumes to the new pattern next time `whoPlaysThisPass` lets it play, rather than
      un-resting it out of turn. Scoped to build/peak/release only, same archetypes the seeding is scoped to -
      Presentation stays on its single held pattern.
- [x] **Confirmed working live in Bitwig (2026-08-21)** after resyncing P2/P3 for the relevant instances and
      pressing Prime for Playback: distinct seeded content on all three patterns, audible switching on the
      8-bar cadence during Build/Peak/Release. Closes out all three items from this session's agreed next-steps
      plan (Presentation reset/gentle-transpose/Prime for Playback, then phrase-chaining).

### Resync All, and Activity Log (2026-08-21, both user-requested)

- [x] **"Resync All" button, Awareness tab:** phrase-chaining made "resync every pattern of every instance"
      a real, frequent setup step (up to 9 manual instance+pattern+click combinations before). One click now
      requests every registered instance's all 3 patterns via `PatternSyncServer::requestSync` - still just
      requests (results land asynchronously via the same IPC responses a single Resync Now uses), reports
      how many were requested vs. skipped for instances not yet connected.
- [x] **Activity Log tab (`ui/ActivityLogView`), new `ComposerCore::logActivity`/`getRecentActivityLog`:**
      user's own request, arising directly from the "living proof that a full composition can be planned ahead
      as numbers" conversation - "I need at least some visual reference, or a log of whatever combination of
      parameters/values have changed and when." A mutex-guarded `std::deque<ActivityLogEntry>` (bar + message,
      capped at `kMaxActivityLogEntries`=500, newest-first), logged from every structural decision point in
      `ComposerCore.cpp`: section entry (`enterSection`), Presentation baseline reset (per instance), the
      section-entry stamp, phrase-chain seeding, phrase-chain advances (including the "advanced silently while
      resting" case), rhythm mutations (transpose/rotation/length, noting when policy blocked one), and Prime
      for Playback being pressed. Deliberately coarse - individual step/note edits inside `policy/MotifEngine`
      stay unlogged (that's Awareness's territory); this is the higher-level narrative of what the engine
      decided and when, the missing piece between "I can hear it changed" and "I can see what changed."

## v1.0 — Narrative Blueprint Composer

- [x] **Tabbed UI shell (done 2026-08-16, confirmed working live in Bitwig via the Mutations tab):** the
      height-ceiling problem is resolved. `ui/EditorView` now owns a `juce::TabbedComponent` (Instances / Scenes / Mutations / Blueprint)
      plus a status bar shared by all tabs via an injected `std::function<void(juce::String)>` callback, and a
      300ms `juce::Timer` that refreshes every page regardless of which tab is active — this is what keeps e.g.
      the Instances tab's coherence score in sync after a scene is sent from the Scenes tab, without wiring
      explicit callbacks between page components. `ui/InstanceView`, `ui/SceneListComponent`, `ui/DebugPanel` are
      no longer empty stubs — they hold the content the old single-panel `PluginEditor` used to (instance
      manager+coherence / global-scene-test+library / mutation test, respectively), each using modern
      `onClick` lambdas instead of `Button::Listener`. `ui/BlueprintView` is a new placeholder tab reserving the
      spot for the arc graph editor below. `PluginEditor` itself is now a thin `AudioProcessorEditor` wrapper
      (owns `EditorView`, forwards `refreshAll()` for `PluginProcessor`'s state-restore callback). Window shrunk
      from ~860px to 560px since tabs no longer stack. `ui/TransportView` remains an empty stub, unused for now.
- [x] **Interactive arc/breakpoint graph page (built 2026-08-16):** `ui/BlueprintView` is now the graph editor,
      not a placeholder — a `juce::ComboBox` selects which of the 5 `ArcSet` arcs (energy/tension/density/
      complexity/coherence) is being edited; the selected arc draws bold with draggable circular control points
      over a bar-position grid (1-64), the other 4 draw dimmed for context, with a colour-coded legend. Drag a
      point to move it, double-click empty graph space to add one, right-click a point to remove it (minimum 1
      point enforced) — edits go through a `workingBreakpoints` copy committed back into `ComposerCore`'s
      `ArcSet` via `Arc::setBreakpoints`/`ArcSet::setArc` on mouse-up/add/delete, then re-read from the
      canonical (sorted) result. `Arc` gained a `getBreakpoints()` accessor for this. Built, compiled, and
      installed clean (no warnings beyond pre-existing JUCE `Font` deprecation notices); not yet live-tested in
      Bitwig. Still authoring-only: nothing reads `ArcSet` values into scene/mutation routing yet — that's
      Blueprint JSON, still pending. The "Formal Field Target Curves" mockup shown
      earlier in the project is close to a literal sketch of this page's look.
- [x] **Blueprint JSON schema/model + authoring UI (built 2026-08-16):** `model/Blueprint.h` — `Blueprint` (id,
      name, ordered `vector<BlueprintSection>`), `BlueprintSection` (id, name, `sceneId` reference into
      `SceneLibrary`, startBar, durationBars, `vector<SectionLayerRole>`, `vector<SectionBudgetOverride>`),
      `SectionLayerRole` (targetInstance + "foreground"/"support"/"background"), `SectionBudgetOverride`
      (role + per-role minor/medium/major overrides of `MutationPolicy::budgetForRole`'s static table, -1 =
      unlimited, same sentinel as `RoleBudget`). `state/BlueprintLibrary` mirrors `SceneLibrary` exactly
      (mutex-guarded, copy-returning). `StateSerializer` gained the full (de)serialization chain; `Validation::
      isValidBlueprint` checks id/name/section-id/durationBars/layerRole-target/budgetOverride-role.
      `StateSnapshotStore` round-trips `blueprintLibrary` + `currentBlueprintId` alongside the existing scene
      data. `ComposerCore` gained `getBlueprintLibrary()` and `setCurrentBlueprint`/`getCurrentBlueprint`
      (mutex-guarded, mirrors `currentSceneData`'s shape but doesn't re-anchor any chain clock - sections don't
      drive playback yet). UI: the Blueprint tab (`ui/BlueprintView`) is now a shell hosting two inner tabs -
      "Arcs" (the v1.0 graph editor, moved unchanged into the new `ui/ArcGraphView`) and "Sections"
      (`ui/BlueprintSectionsView`, new) - build a section from a scene reference + bar range + per-instance
      layer-role picks + per-role budget-override sliders, add/remove sections into a working blueprint, then
      save/load/remove named blueprints to/from the library, mirroring `SceneListComponent`'s build-panel +
      library shape. Confirmed working live in Bitwig (2026-08-17): add/update section, pending layer-role/
      budget-override preview lines, remove section, save/load/remove blueprint all verified.
  - **`SectionBudgetOverride` consumption wired (2026-08-17):** `MutationPolicy::tryConsume`/`PolicyEngine::
    authorize` now accept an optional override `RoleBudget`; `Router` holds an injected `BudgetOverrideResolver`
    (kept decoupled from the `Blueprint` model by design) that `ComposerCore::resolveSectionBudgetOverride`
    implements - finds the current blueprint's section covering `currentBar`, returns its override for the
    mutation's target role if one exists. Both dispatch paths (`Scene::mutations` via the scene chain, and
    `DebugPanel`'s manual "Send Mutation" button) get this automatically since both already flowed through
    `Router::routeMutation`, the single existing choke point - no call site needed touching. Confirmed working
    live in Bitwig (2026-08-17): an Anchor instance's normally-blocked transpose mutation succeeded while a
    section overriding Anchor's budget covered the current bar. Full reasoning in
    [mutation_policy_v0_1.md](mutation_policy_v0_1.md).
  - **`SectionLayerRole` still authoring-only:** foreground/support/background remains recorded but unconsumed
    - nothing reads it yet, same status Arc/ArcSet had before anything used them.
  - **The "full loop" — sections drive playback themselves (built 2026-08-17):** `ComposerCore::
    advanceBlueprintIfNeeded`, called every `processBar` tick alongside the existing scene chain, finds the
    active blueprint's section whose `[startBar, startBar+durationBars)` covers `currentBar` (first match wins
    if sections overlap - authors are expected to keep them non-overlapping) and routes that section's
    `sceneId` via `router.routeScene` the moment the active section changes. `ComposerCore::
    getActiveSectionId()` exposes which section (if any) is currently playing, shown live on the Sections tab
    (`ui/BlueprintSectionsView`'s new "now playing" label, refreshed on the shared 300ms timer). Deliberately
    decoupled from the older `Scene::nextSceneId`/`durationBars` chain (`policy/SceneAdvancePolicy`,
    `ComposerCore::advanceSceneChainIfNeeded`) rather than replacing it - both mechanisms call
    `router.routeScene` but track fully separate state, so manual "Send Scene Now" and hand-authored scene
    chains keep working exactly as before even while a blueprint is active; a manual override simply persists
    until the active section itself changes (blueprint playback doesn't fight it every bar). `setCurrentBlueprint`
    now clears the tracked active section so saving/loading a blueprint always re-evaluates from bar 0. Since
    saving *or* loading a blueprint from the Sections tab already calls `setCurrentBlueprint`, **any blueprint
    with sections now starts driving playback automatically the moment it becomes current** - no separate
    "activate" step. `workingSections` is now kept sorted by `startBar` on every add, so the Sections display
    and playback order both read top-to-bottom. Confirmed working live in Bitwig (2026-08-17): a 2-section
    blueprint ("intro" bars 0-8, "climax" bars 8-24) correctly switched its "now playing" label and routed the
    climax scene as the transport crossed bar 8, with no manual trigger.
  - **Cross-scene apex exclusivity built (2026-08-17):** `model/Blueprint.h`'s new `ReservedValue`
    (targetInstance/patternIndex/type/value) marks a resulting parameter value as belonging to a later section;
    `Router` holds an injected `ReservedValueChecker` (mirrors `BudgetOverrideResolver`'s pattern exactly),
    implemented by `ComposerCore::isValueReservedByLaterSection`. `Router::routeMutation` computes the
    mutation's resulting absolute value and blocks the send if a strictly-later section reserves it. Scoped
    identically to the budget-override wiring: only `routeMutation` is gated, not `routeScene`'s direct
    `ScenePattern`/`SceneGlobal` sends. Confirmed working live in Bitwig (2026-08-17).
  - **Sections tab restructured for growth:** the "Sections" inner tab's form outgrew the tab's available
    height once reserved-value authoring was added on top of layer roles + budget overrides. `ui/
    BlueprintSectionsView` is now a thin `juce::Viewport` host; the actual controls moved unchanged (plus the
    new reserved-value row) into new `ui/BlueprintSectionsContent`, sized to a fixed preferred height
    (`getPreferredHeight()`, currently 580px) taller than the tab's roughly 450-470px available space, so it
    scrolls instead of silently clipping.
  - **`SectionLayerRole` consumed (2026-08-17):** `ComposerCore::applyLayerRoleOverrides`, called from
    `advanceBlueprintIfNeeded` right before routing a section's scene, applies each `background`-tagged
    instance as a `SceneInstanceOverride(activePattern=0)` on a copy of that scene - unless the scene already
    authored an explicit override for that instance, which always wins. `foreground`/`support` get no forced
    behavior: MPL's CC protocol has no volume/velocity control, so "engaged vs. stopped" is the only lever
    available, and there's no honest way to differentiate "prominent" from "supporting" at that level. Scoped
    to blueprint-driven routing only, same as the budget-override/reserved-value wiring - the older
    `Scene::nextSceneId` chain and manual "Send Scene Now" don't consult layer roles (they have no section
    context to read one from). Compiled and installed clean; not yet live-tested in Bitwig.
- [ ] Save/Recall presets (DAW preset + JSON, per source roadmap §19)
- [ ] `debug/Logger`, `debug/DebugMonitor` — currently empty; useful before this milestone too, not just at it

## v1.1 — Generative Blueprint Proposal

- [ ] Not started. Scope commitment recorded in [composer_mastermind_design.md](composer_mastermind_design.md)
      ("Player vs. Composer," 2026-08-14) — depends on v0.4/v0.5 existing first.

  **Note (2026-08-22): this header is stale** — v1.1's generative assembly layer (`policy/BlueprintGenerator`,
  `ui/GenerateView`) is actually built and confirmed working live; see the "v1.1 generative assembly layer built"
  entry under v0.2 above for the real write-up. Left as-is rather than restructured, since untangling this
  checklist's v1.1/v1.2 organization is its own separate cleanup, not part of the curve-authoring work below.

## v1.2 — Curve-Based Blueprint Authoring ("the cockpit") — built 2026-08-22 evening

Part of the score-timeline UI redesign; see [score_timeline_ui_concept.md](score_timeline_ui_concept.md)'s
"Curve-based blueprint authoring" section for the full design writeup (data-model resolution, the live-ArcSet-sync
gap found and fixed, exact mechanism). Summary here for the fast-scan checklist format:

- [x] `model/Blueprint.h`: `BlueprintArcPoint`/`BlueprintArcCurve`, `Blueprint::arcCurves` — a dimension with no
      entry means "derived from sections," not "flat zero."
- [x] `policy/BlueprintGenerator::deriveArcSetFromSections`/`resolveBlueprintArcSet` — derives a display curve
      from a blueprint's own section/archetype sequence (reusing `generate()`'s existing per-archetype baseline
      tables, now also reachable without a driving arc), merges it with any explicitly-saved `arcCurves`.
- [x] `ComposerCore::setCurrentBlueprint` now syncs the live `ArcSet` from the activated blueprint via
      `resolveBlueprintArcSet` — previously nothing did this at all; the live `ArcSet` was decoupled from
      whichever blueprint was actually active and was never persisted anywhere.
- [x] `ui/BlueprintArcCurveView` (new "Compose" toggle in `ui/MainShellView`, a third peer view alongside
      `PrimaryView`/`EditorView`) — blueprint picker, derived/custom badge, `ui/ArcGraphView` reused unchanged
      for the graph itself (gained `onSelectionChanged`/`onEdited` callbacks + `getSelectedArcName()` for the
      badge). Save always clones onto a new blueprint id, never overwrites in place.
- [x] `GenerateView::commitClicked` simplified to persist its generated `ArcSet` onto `Blueprint::arcCurves`
      instead of a one-off manual push into the live `ArcSet`; `BlueprintSectionsContent::saveBlueprintClicked`
      now preserves existing `arcCurves` when resaving a blueprint's sections, so it can't silently discard them.
- [x] Compiled clean for both `ComposerMastermind_VST3` and `ComposerMastermind_Standalone`.
- [x] **Confirmed working live (2026-08-22)** — after 3 rounds of live-testing found and fixed real bugs: an
      empty-curve-trust gap in `resolveBlueprintArcSet`/`saveClicked`, a badge-refresh bug in `ArcGraphView::
      onEdited`, and the actual root cause — `ArcSet::getArc()` returns `Arc` by value, and both `saveClicked`
      and `GenerateView::commitClicked` chained straight into `.getArc(name).getBreakpoints()`, binding a
      reference into a temporary that was already destroyed by the time it was read (a dangling reference,
      silently reading back empty data on this toolchain rather than crashing). Fixed by naming the `Arc` as a
      real local before reading its breakpoints in both places. User confirmed: "Problem solved."
- [x] **Monophonic duration-overlap clamp, built 2026-08-22** — new `policy/MonophonicOverlap` (`rangeOverlapsExisting`/
      `maxNonOverlappingDuration`), extracted from `ui/PianoRollView`'s own Setup-mode overlap logic (built for
      manual edits, matching MPL's `melodyStepRangeOverlapsExisting`/`getMaxNonOverlappingMelodyDuration`) so it's
      one shared, JUCE-free implementation instead of two that could drift apart. `PianoRollView`'s own private
      methods now just delegate to it — zero behavior change for the already-confirmed-working manual editing
      path. Newly applied to `policy/MotifEngine`'s automated writes, which previously only clamped duration to
      `[1, kPatternSteps]` with no awareness of a neighboring step: `stampOnePattern` (covers
      `stampMotifForSection`/`seedPhraseChainPatterns`/`restampPhraseChainSlot`, all three call it) now places
      every note as a 1-step placeholder first, then grows each duration only up to the next note's still-
      placeholder start; `applyForSection`'s per-note nudge/phrase write clamps against `cached.snapshot.steps`
      (already reflecting everything this pass has written so far). Applied unconditionally, not behind a
      per-instance flag — MPL instances are monophonic by design, not configurably so, matching
      `PianoRollView`'s own existing unconditional behavior. Compiled clean, both targets.
- [x] **Cache-level normalization filter, built 2026-08-22 (same evening, second pass)** — live testing found
      overlapping notes persisting even after the write-time clamps above, from patterns not written by
      Composer Mastermind itself in this session (hand-edited in MPL, or written before this constraint
      existed). User's own framing: the guard should work "like a polyphonic restriction to 1... no matter what
      a blueprint says," not depend on every writer remembering to clamp. New
      `MonophonicOverlap::normalizePattern` (sequential left-to-right trim: every enabled step's duration is cut
      to end before the *next* enabled step starts) now runs inside `state/InstancePatternCache::store` - the
      single choke point every real pattern snapshot passes through (MPL dump responses via
      `PatternSyncServer::handlePatternDumpResponse`, and every one of `MotifEngine`'s own self-consistency cache
      updates). This guarantees the invariant regardless of source, on top of (not instead of) the write-time
      clamps, which still prevent Composer Mastermind's own writes from ever needing correction in the first
      place. Compiled clean, both targets.

## Housekeeping (not roadmap-versioned, currently empty/unused)

- [ ] `scripts/build.sh`, `scripts/format.sh`, `scripts/run_tests.sh` — all empty
- [ ] `tests/unit`, `tests/integration` — no tests exist yet; `InstanceRegistry`, `Router`, `CCMapping`, `Scheduler`,
      and `Validation` are all JUCE-free by design specifically so they're unit-testable without a plugin host —
      worth using that before the policy layer (v0.4) adds real logic worth breaking
- [ ] `util/MathUtils.h`, `util/StringUtils.h` — empty, unclear yet if/when needed
- [x] `midi/CCMapping.h` added to `CMakeLists.txt` `source_group` for IDE visibility alongside the other files
