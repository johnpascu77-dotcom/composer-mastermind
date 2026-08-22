# Arc Dimension → Parameter Mapping — Design Concept

**Status (2026-08-22): Track B underway.** Branched off [score_timeline_ui_concept.md](score_timeline_ui_concept.md)'s
open item 1 (curve-based blueprint/preset authoring) once it became clear the real prerequisite isn't the
authoring UI — it's that most of the 5 arc dimensions don't drive anything yet. This doc is that design pass:
giving each dimension a real, audible consumer. Now part of the phased roadmap's v1.2 Track B
([composer_mastermind_design.md](composer_mastermind_design.md)) — the continuous-automation dispatch path plus
the Energy/Tension consumers are built (below); Complexity, Density, and Coherence are not yet.

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
| **Coherence** | — | Deliberately not mapped this pass — see below. |

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

## Coherence — deliberately deferred

Everything above is "sample a curve, set a parameter." Coherence today is a live *diagnostic* (how much instances
already agree, `policy/CoherenceEvaluator`), not a single knob to set — turning it into a real target needs an
active clamping/nudge mechanism pulling instances toward agreement, which `composer_mastermind_design.md`'s v0.5
section already flagged as unfinished ("coherence-as-target still isn't built, only coherence-as-diagnostic").
Left out of this pass rather than forcing a shallow mapping onto it.

## Swing vs. notation: the exact problem, and the fix — built 2026-08-22

Continuous, curve-driven Swing is expressive for live audition but genuinely dangerous for the notation-export
path, because most swing values don't correspond to any clean notated rhythm. Worked out exactly from MPL's own
formula (`Source/PluginProcessor.cpp:2160` in the sibling project — only alternating/odd-indexed steps get
delayed, by `gridStepLength × 0.5 × swing%/100`):

- **0% → 1:1 (straight)** — clean.
- **66.67% → exactly 2:1 (true triplet feel)** — clean, notates as genuine eighth-note triplets or swung eighths.
- **75% (MPL's actual maximum)** → **2.2:1**, not the 3:1 a "dotted shuffle" would need. A true dotted-eighth+
  sixteenth ratio requires swing = 100%, outside MPL's 0-75% range entirely — MPL cannot produce an exact dotted
  shuffle at all. Its ceiling is an in-between value in disguise, not a third clean state.

So MPL's real swing range contains exactly **two** notation-legal anchor points, not three. Resolution, layered on
top of the existing as-performed/quantized-for-notation export split from
[score_timeline_ui_concept.md](score_timeline_ui_concept.md):

- **As-performed / live audition**: Density drives Swing continuously across the full 0-75% range, unrestricted —
  it never touches notation. `PrimaryView`'s recorder buffer captures this as `RecordedNote::performedOnsetPpq`,
  the real swing delay in effect at that step, unchanged.
- **Quantized-for-notation export**: at each note, snap the swing value in effect to whichever of the two clean
  anchors is nearer — **0% below a ~33% crossover (roughly the midpoint to 66.67%), else 66.67%**. MPL's own
  ceiling (75%) folds into the triplet bucket rather than being treated as its own state, since it was never a
  clean ratio to begin with. Same value-banding technique as the phrase-chain/archetype classification above,
  captured as `RecordedNote::quantizedOnsetPpq`
  (`ui/PrimaryView.cpp`'s `kSwingNotationCrossoverPercent`/`kSwingNotationTripletPercent`), computed at the same
  moment as the performed onset so the drag-out export's variant toggle needs no separate quantization pass.
  (This replaced an interim simplification from the drag-out export's own build, 2026-08-22 same day: with Swing
  still static at that point, "just drop the swing offset entirely" was correct and sufficient; once Density made
  Swing genuinely continuous immediately after, that simplification would have silently flattened real swung
  passages to straight, so it was replaced with the actual banding rule described here.)

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
