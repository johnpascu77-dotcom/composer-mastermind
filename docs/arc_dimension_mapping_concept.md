# Arc Dimension → Parameter Mapping — Design Concept

**Status (2026-08-25): all 5 dimensions now have a real consumer.** Branched off [score_timeline_ui_concept.md](score_timeline_ui_concept.md)'s
open item 1 (curve-based blueprint/preset authoring) once it became clear the real prerequisite isn't the
authoring UI — it's that most of the 5 arc dimensions don't drive anything yet. This doc is that design pass:
giving each dimension a real, audible consumer. Now part of the phased roadmap's v1.2 Track B
([composer_mastermind_design.md](composer_mastermind_design.md)) — Energy/Tension/Density/Complexity built
2026-08-22, Coherence built 2026-08-25 (see its own section below — a simpler mapping than the one originally
sketched here, not yet live-tested).

## Where this started

`ArcSet` (`policy/ArcSet.cpp`) has carried 5 named dimensions since v0.5 — energy, tension, density, complexity,
coherence — but only **density** is baked from a real decision (the fraction of instances not backgrounded per
section, `BlueprintGenerator.cpp`'s `derivedValue`). Energy, tension, complexity, and coherence are flat
per-archetype baselines with **no consumer at playback time at all** — energy is only ever used as the "driving
arc" that decides section *boundaries* at generation time, and even that doesn't touch the arc's actual bar-by-bar
value once a piece is playing. This is decoration the graph editor can draw, not something the piece actually
follows.

## Continuous vs. impulse — the governing distinction

`routing_policy_v0_1.md`'s CC map splits cleanly into continuous 7-bit values (Transpose, Swing) and categorical
ones (Grid Mode, Active Pattern, Inversion). The v1.1 design already established that a categorical CC parameter
can't "glide" — confirmed again here: continuous targets can track a curve's value every bar, for real; categorical
targets can only ever jump between states, but *when* they jump can be curve-driven (threshold-crossing) instead
of a flat interval like today's `kPassIntervalBars`/`kPhraseChainIntervalBars`.

## Proposed mapping

| Dimension | Continuous target (updates every bar) | Impulse target (curve-timed, not flat-interval) |
|---|---|---|
| **Energy** | **Built 2026-08-22.** Melodic average curve's *amplitude* — replaced the flat `kMelodicCurveAmplitudeSemitones`/`kPresentationTransposeAmplitudeSemitones` split with one continuous mapping (`kMinMelodicCurveAmplitudeSemitones`..`kMaxMelodicCurveAmplitudeSemitones`, 1.0-6.0) from `Arc::evaluate`, so presentation's gentler feel falls out naturally from its own lower Energy baseline instead of a special-cased constant. | — |
| **Tension** | **Built 2026-08-22.** Melodic average curve's *register center* — instead of a pure sine wobble around a fixed point, rising tension (build's baseline already climbs 0.5→0.9 toward peak) now pulls the shared tonal center upward, 0..`kMaxTensionRegisterPullSemitones` (12, one octave). | — |
| **Density** | **Built 2026-08-22.** Swing amount (CC 24), via new `Router::routeContinuousSwing` — `ComposerCore::applyContinuousSwing` maps Density's 0..1 value linearly onto Swing's full 0..`CCMapping::kMaxSwing` (75%) range every bar, for every archetype-eligible instance regardless of whether it's currently resting (Swing is a global groove setting, not tied to any one pattern, so it should already be right the moment an instance resumes). Replaces "100% static-per-scene, never touched again." See the resolved notation caveat below. | — |
| **Complexity** | **Built 2026-08-22.** Mutation budget *ceiling* scaling — `ComposerCore::resolveSectionBudgetOverride` now always scales whatever base budget applies (explicit section override, else `MutationPolicy::budgetForRole`'s static table) by `1.0 + complexityValue` (1x..2x, ceil'd so a small budget visibly grows), rather than only ever returning the unscaled static table when no section explicitly overrides a role. Unlimited (-1) budgets stay unlimited. Doubles up Complexity's role alongside phrase-chain banding rather than inventing a 6th dimension. | **Built 2026-08-22.** Phrase-chain target banding (P1/P2/P3), **threshold-crossing**: `firePhraseChainIfDue` now runs every bar, sampling the complexity arc and banding it into three even thirds → P1/P2/P3 (matching `seedPhraseChainPatterns`' own base/rotated/inverted assignment), changing an instance's home pattern only when the band itself differs from last time (`lastPhraseChainBand`) — replaced the old flat `kPhraseChainIntervalBars`=8 metronome entirely, which is now removed. |
| **Coherence** | — | **Built 2026-08-25**, see "Coherence" section below — threshold-crossing onto MPL's Retrograde/M7 toggles (own CC each, added the same session), not the harder mapping originally sketched here. **Second consumer added 2026-08-27**: authored-curve Rate divergence via `ModulationRoute`, no new engine code — see "Coherence's second consumer" below. |

**Richer role vocabulary for the phrase-chain target — built 2026-08-22, inspired by a commercial JUCE plugin,
Stellarizer's Pattern Playground.** Stellarizer generates 8 named roles from one source (Anchor, Sparse Signal,
Echo Field, Syncopated, Contour Arc, Fragmented, Reframed, Wildcard), with a single **Discovery Distance** knob
(Near → Far → Deep Space) controlling how far each variation strays from source — independently arrived at, this
turned out structurally identical to Complexity's own phrase-target banding above, good validation. Their
vocabulary was richer than the original fixed 3-way split (P1=base, P2=rotated, P3=inverted) it inspired
extending: `MotifEngine::PhraseRole` is now a 5-way vocabulary (Base, Rotated, Inverted, Retrograde,
InvertedRetrograde), built entirely from the existing rotate/invert/retrograde primitives (`transformForPass`
already cycled the same three for single-pattern nudging) — no new transform logic invented, just a 5th
combination (`retrograde(invert(base))`) as the natural "most transformed" state. `phraseRoleFromComplexity` bands
Complexity into five even fifths instead of three thirds.

Since MPL still only has 3 physical pattern slots, `firePhraseChainIfDue` (`ComposerCore.cpp`) now decouples
"which slot" from "which role": it still advances through the same 1→2→3→1 physical rotation as before (always
landing on a slot that isn't the currently-active one, so nothing gets rewritten out from under live playback),
but first **re-stamps that slot fresh** with the target role's shape (`MotifEngine::restampPhraseChainSlot`,
reusing `stampOnePattern`'s existing register-continuity/Length-window logic) rather than trusting whatever
`seedPhraseChainPatterns` originally wrote there at section entry. The role vocabulary is genuinely richer than
the slot count now — informing *which* transform gets written into whichever slot is next due for a change,
exactly as originally proposed.

## Two architecture constraints to respect when building this

1. **Density must stay one-directional.** It's derived *from* the foreground/background layer-role split at
   generation time (a one-time bake), not authored directly. Reading it during playback to drive Swing is safe;
   nothing should ever write back into layer-role assignment from a live-sampled Density value, or it becomes
   circular (density reads roles, roles read density).
2. **Continuous automation needs its own dispatch path, separate from the mutation budget — built 2026-08-22.**
   `PolicyEngine::authorize` gates discrete `Mutation`s at minor/medium/major per bar per role — that's for real
   authored decisions. If Swing/register-center re-encode every bar, that's automation, not a discrete decision,
   and competing for the same budget would silently starve legitimate mutations. `Router::routeContinuousTranspose`
   (Energy/Tension's melodic-curve consumers) and `Router::routeContinuousSwing` (Density's consumer) are that
   third dispatch mode, both bypassing `PolicyEngine`/`ReservedValueChecker` entirely the way `routeScene` already
   does, while still updating `InstanceStateTracker`.

**How it's wired (2026-08-22):** `ComposerCore::applyContinuousMelodicCurve` runs every bar a section is active —
from `enterSection` for the boundary bar, and `advanceBlueprintIfNeeded`'s "still active" branch thereafter —
deliberately *not* gated by `kPassIntervalBars` the way `applyRhythmForSection`/`applyMotifForSection` are, since
this needs to be continuous, not pass-based. Transpose was removed entirely from `applyRhythmForSection`'s old
passIndex-cycling rotation (which now only alternates Rotation/Length, `passIndex % 2`) and from Presentation's
old special case (`applyRhythmForSection` is now a no-op for Presentation — its register drift comes entirely
from the continuous curve, which runs for every archetype including Presentation, unlike the pass-based
mechanism). Not logged to the Activity Log — every-bar-per-instance would spam a log meant to stay "deliberately
coarse"; the effect is audible/observable via a live transpose readout instead (e.g. the Awareness tab).

**Content-aware taper (2026-08-27):** the register-center math above is purely time/arc-driven and was originally
applied identically to every touched instance regardless of where that instance's pattern already sat pitch-wise
— found live to push an already high-voiced pattern another +10 semitones up via Transpose. Before dispatch, the
raw `curveTargetTranspose` is now passed through `MotifEngine::taperTransposeForPatternContent`, which bounds the
*combined* result (the pattern's own current cached-content center plus the raw Transpose) to the same
`boundedHomeCenter` clamp `stampOnePattern`/`applyForSection` already use for generative writes, and back-solves
the actually-safe Transpose from that. Only affects this built-in curve, not user-authored `ModulationRoute`s.

## Coherence — the harder mapping is still deferred; a simpler one is built

Everything above is "sample a curve, set a parameter." Coherence-as-*diagnostic* (`policy/CoherenceEvaluator`, how
much instances already agree right now) is a different thing from the `ArcSet` "coherence" *dimension* (an
authored/baked curve, same shape as energy/tension/density/complexity) — this section is about the latter.
Turning the diagnostic into a real target needs an active clamping/nudge mechanism pulling instances toward
measured agreement, which `composer_mastermind_design.md`'s v0.5 section already flagged as unfinished
("coherence-as-target still isn't built, only coherence-as-diagnostic") — **that direction is still deferred**,
deliberately, as originally written here.

A different, much simpler mapping was built instead (2026-08-25), once MPL gained two new toggles that are
naturally *about* divergence: **Retrograde** and **M7** (CC 34/35, 44/45, 54/55 — see
[routing_policy_v0_1.md](routing_policy_v0_1.md)). `ComposerCore::applyCoherenceDivergenceIfDue` samples the
Coherence arc every bar a section is active and threshold-crosses it exactly the way Complexity drives
phrase-chain banding above (`firePhraseChainIfDue`) — not a continuous glide (these are categorical CC
parameters, same "impulse target" reasoning as Inversion always had), and not an active nudge toward the
diagnostic's measured agreement either. Two thresholds stage the divergence: below 0.35, Retrograde turns on;
below 0.15, M7 also turns on. Both sit below every archetype's baseline coherence value (Presentation 0.6, Build
0.5, Peak 0.8, Release 0.45 — `BlueprintGenerator.cpp`'s `coherenceBaseline`), so picking an archetype alone never
triggers this; it only fires when a section's coherence curve is deliberately authored/hand-curved down past
either point — the "intentional CC curve, not incidental side effect" framing the user specifically wanted
(confirmed by hand first, via two independent Bitwig modulators driving these same toggles directly on two MPL
instances, described as producing exactly the kind of development-section motion this mapping now automates from
an authored curve instead of manual modulator setup).

### Coherence's second consumer — authored-curve Rate divergence, built 2026-08-27, no new engine code

The "harder mapping" above (an active nudge toward `CoherenceEvaluator`'s *measured* agreement) is still exactly
as deferred as when this section was first written - genuinely different, harder problem, not reopened here. But
once [[project_mpl_rate_status]]'s Rate landed with full `ModulationRoute` citizenship, a second, much simpler
Coherence idea became available for free: the *authored* Coherence curve (same shape Energy/Tension/Density
already sample) driving Rate divergence, exactly the way `tension → rate` already does for the cadence device -
just a second `ModulationRoute`, zero new `ComposerCore` code.

Demonstrated on "Slack Tide" (`docs/example_score_slack_tide.json`): MPL1 (anchor) and MPL3 (counterpoint)
already have Rate spoken for by Tension - MPL1 capped to Normal/Augmented, MPL3 free to reach Diminished at
Tension's peak (the orchestration fix from earlier the same day - see `docs/advanced_workflow_example.md`'s
troubleshooting entry). MPL2 (motif) had no Rate automation at all, so it's a clean target: `coherence_to_rate_MPL2`
(`outputMin: 0, outputMax: 1, invert: false`) pulls MPL2 toward **Augmented** as Coherence falls - the *opposite*
pole from MPL3's Tension-driven **Diminished** at the same moment (Surge, where Coherence bottoms out and the
existing Retrograde/M7 divergence consumer above already fires). The result: at the piece's point of maximum
disagreement, the voices don't just diverge in pitch-transform (Retrograde/M7) - they pull apart rhythmically in
opposite directions too, then reconverge together as Coherence recovers into Slack. Confirms the general pattern:
**a new arc-dimension "consumer" doesn't need a new hardcoded `ComposerCore` function once the target parameter
already has generic `ModulationRoute` support - it can just be authored**, the same realization `BarCycle`'s dual
macro/micro use already demonstrated for Rate itself.

## Swing vs. notation: the exact problem, and the fix — built 2026-08-22, superseded 2026-08-25, refined 2026-08-26

Continuous, curve-driven Swing (Density's original 2026-08-22 consumer) turned out to be expressive for live
audition but genuinely dangerous for the notation-export path, because most swing values don't correspond to any
clean notated rhythm. Worked out exactly from MPL's own formula (`Source/PluginProcessor.cpp:2160` in the sibling
project — only alternating/odd-indexed steps get delayed, by `gridStepLength × 0.5 × swing%/100`):

- **0% → 1:1 (straight)** — clean.
- **66.67% → exactly 2:1 (true triplet feel)** — clean, notates as genuine eighth-note triplets or swung eighths.
- **100% → exactly 3:1** — the true dotted-eighth-plus-sixteenth shuffle ratio. This is the delay formula's actual
  mathematical ceiling: at 100%, the swung note lands precisely at the midpoint between its neighbours, and going
  any higher would overtake the following step's timing and invert step order. Not an arbitrary round number —
  the genuine boundary.

**2026-08-25: this stopped being a notation-export-only quirk once the user actually listened to it.** Live
testing found that the *performed* audio was sloppy, not just its notation - a moderate swing value like 20%
never sounded like a deliberate groove, only 0% (straight) and something near the clean ratios ever read as
musical. Rather than keep working around this at export time, MPL's own Swing knob (CC 24) was narrowed at the
source from a continuous slider to a genuine **3-state choice: Off (0%) / Triplet (66.67%) / Shuffle** - see
`Source/PluginProcessor.cpp`'s `globalSwingParam`. It is no longer possible to land on an ambiguous in-between
value at all, live or in notation.

**2026-08-26: Shuffle's own percent moved from 75% to 100%.** The first build set Shuffle to MPL's old historical
maximum (75%, a 2.2:1 ratio) without questioning whether that number still meant anything now that Swing was a
named state rather than a slider ceiling - live testing immediately found Triplet and Shuffle indistinguishable
by ear, since 2.2:1 is barely different from 2:1. Raising Shuffle to 100% (the true 3:1 ratio worked out above)
fixed this: `CCMapping::kShuffleSwingPercent` is now `100.0f`, and `kMaxSwing` is simply an alias for it rather
than a separately-tracked ceiling - the two were only ever different by historical accident, not by design.

Composer Mastermind mirrors this everywhere Swing is touched, while keeping its own model's `swing` field a plain
percent (not an index) for minimal ripple - `CCMapping::swingStateForPercent`/`swingPercentForState` are the
shared conversion, used by:

- **`CCMapping::encodeSwing`** - snaps any percent to the nearest of the 3 legal states before encoding the CC,
  robust to old scenes/presets that might still carry an arbitrary value.
- **`ComposerCore::applyContinuousSwing`** (Density's consumer) - no longer glides linearly across the full range;
  bands Density's 0..1 value into one of the 3 states directly (`CCMapping::swingStateForNormalized`, even
  thirds), the same threshold-crossing shape as Complexity's phrase-chain banding and the Coherence divergence
  consumer above, so `InstanceStateTracker`'s recorded value is always exactly legal, not silently disagreeing
  with what was actually sent.
- **UI**: `ui/SceneListComponent`'s scene-builder and `ui/PresetLibraryContent`'s role/rhythmic-relationship
  preset builders all replaced their Swing slider with a 3-item combo (Off/Triplet/Shuffle), matching MPL's own
  UI change - a scene/preset author can no longer author an illegal swing value by hand either.
- **`policy/CoherenceEvaluator`** and **`util/Validation::isValidSwing`** both read `CCMapping::kMaxSwing` instead
  of a locally-duplicated 75.0f literal, so raising Shuffle's percent moved their ceilings automatically.

**Quantized-for-notation export still exists** (`ui/PrimaryView.cpp`'s recorder), now snapping to whichever of
all 3 states (not just straight/triplet) the tracked swing value is nearest -
`CCMapping::swingPercentForState(CCMapping::swingStateForPercent(...))`, the same shared helpers used everywhere
else. Shuffle now gets its own true 3:1 onset position here too, rather than folding into Triplet's 2:1 - closed
2026-08-26, same day as the 100% correction, once the whole point of a 3:1 ratio was that it's genuinely
notatable. Worth being precise about what this actually does: a Standard MIDI File carries no tuplet/dotted-
rhythm markup at all, only onset ticks and durations - Composer Mastermind's job ends at placing each note at the
mathematically exact position its target rhythm implies (already true for Triplet, now true for Shuffle too);
whether Dorico/Sibelius render that as a literal dotted-eighth-plus-sixteenth figure depends on the notation
software's own MIDI-import quantization/rhythm-detection, the same as it already did for Triplet.

## Still open: the graph ↔ preset-library round-trip

Separate, smaller gap noticed while discussing this: dragging points on `ui/ArcGraphView` edits the *live*
`ArcSet` directly, but saving that as a reusable `ArcPreset` currently requires re-entering the breakpoints by
hand via separate sliders in `ui/PresetLibraryContent.cpp` (`breakpointPositionSlider`/`breakpointValueSlider` →
"Add breakpoint" → "Save Preset") — two disconnected input paths for what should be one motion: drag on the
graph, hit "Save as preset." Fix is contained: read the currently-edited `ArcSet`'s breakpoints for the selected
dimension instead of the separate pending-breakpoints list. Not yet built.

## Relationship to score_timeline_ui_concept.md's open item 1

That doc's remaining open question (curve-based blueprint/preset authoring, showing a loaded blueprint's
"intended shape" as an editable curve when some pieces have no arc data at all) is still unresolved — this doc
doesn't answer it, but makes it more urgent: once these dimensions carry real musical weight, seeing a
hand-composed piece's *implied* shape (derived from its section/archetype sequence, since it has no real
`ArcSet` data) matters more than when the curves were purely decorative.
