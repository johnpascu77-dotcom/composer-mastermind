# Score-Timeline UI — Design Concept

**Status (2026-08-22): design conversation only, nothing built.** This consolidates a first design pass —
condensed-score piano roll, live sync, and MIDI drag-out — done via sketches in chat, not yet committed to code or
to the phased roadmap in [composer_mastermind_design.md](composer_mastermind_design.md). Treat this as the design
brief for whichever session picks the work up, not as a spec to build against blindly — several open questions at
the end are still unresolved.

## Motivation

Composer Mastermind's current authoring UI is numbers/sliders/text combos across tabs (Instances, Scenes,
Mutations, Blueprint, Presets, Generate). Seeing what's actually musically happening — the three instances'
pattern content, together — currently means opening three separate floating MPL plugin windows, which is
cluttered and doesn't show them as one texture. This redesign is about a unified, visual, score-like view of what
the three instances are playing, both live and as an authoring surface.

## View modes

Two complementary modes over the same colored-by-instance note data:

1. **Overlay / condensed score** — all 3 instances' active pattern content on one piano-roll grid, notes
   color-coded per instance. Reads as a 3-voice reduction (like a hand-written counterpoint short score), not a
   generic polyphonic piano roll.
2. **Lane view** — the same data as 3 stacked grids, one per instance, for inspecting one voice in isolation
   (closer to the original "9 pattern lanes" sketch from the earlier design conversation, collapsed here to one
   lane per instance's *currently active* pattern rather than all 3 patterns per instance at once).

Both modes share one color key (one color per instance, consistent everywhere the UI shows instance identity).

## Why the overlay works cleanly: the monophonic constraint

MPL outputs one row per instance, and MPL instances are monophonic — a row's own steps never overlap in time.
That means the overlay never needs collision/stacking logic *within* one instance's line; it only ever needs to
place 3 independent, individually non-overlapping lines on a shared grid. Overlap *between* instances (two voices
landing on the same pitch/time — a unison, or forming a harmonic interval) is expected and fine; it's not a data
problem, just what counterpoint looks like.

This also matters for what comes out the other end: because each instance already produces exactly one clean
monophonic line, and the eventual target is notation software (Sibelius/Dorico) — good for monophonic/legato
orchestral patches too — voice identity has to survive as **separate MIDI tracks or channels**, not just as UI
color. That's a requirement on the export path, not just the on-screen view (see Drag-out export below).

**Resolved (2026-08-22):** this doesn't depend on Bitwig routing at all, and doesn't need confirming — each MPL
instance already runs on its own Bitwig track per [routing_policy_v0_1.md](routing_policy_v0_1.md), but more to
the point, the recorder buffer below is inherently keyed per instance (built from `InstancePatternCache`, which is
already per-instance), so per-track voice separation in the exported MIDI file falls out of the data model
automatically. No DAW-side dependency either way.

## Data foundation (already exists)

[`model/PatternSnapshot.h`](../src/model/PatternSnapshot.h) and
[`state/InstancePatternCache.h`](../src/state/InstancePatternCache.h) already give per-instance, per-step ground
truth (`note`, `velocity`, `duration`, `enabled`) as last confirmed via a Composer Bridge pattern dump. This is
real data, not a new capability — the piano-roll view is a rendering problem over data Composer Mastermind already
holds, not a new data-acquisition problem.

## Live sync: reconstruction, not MIDI capture

Composer Mastermind cannot see MPL's actual audio-thread note output — CC and IPC pattern writes go *out* to MPL;
the notes MPL triggers go straight from MPL's own instrument track into the DAW mix, with no path back into
Composer Mastermind. Capturing that would require new DAW-side MIDI routing (each MPL instance's note output
routed back into Composer Mastermind), which is fragile, host-dependent, and wouldn't work in Standalone at all.

Instead: **reconstruct** what's sounding from what Composer Mastermind already knows — current playhead position
(`getPlayHead()`/`getPosition()`, already wired in
[`plugin/PluginProcessor.cpp`](../src/plugin/PluginProcessor.cpp) for bar-quantized scheduling), the last-known
pattern content per instance (`InstancePatternCache`), and the mutation/stamp decisions Composer Mastermind itself
just made (already bar-stamped in the Activity Log — see `ComposerCore::logActivity`/`getRecentActivityLog`). This
is architecturally much smaller than real MIDI capture, and "accepted latency" here means bounded by the
pattern-dump/reconciliation cycle, not audio buffer size.

**Master grid resolved (2026-08-22): absolute host time, not per-instance step grid.** Instances can run different
grid modes/pattern lengths at once (poly-rhythmic scenes are deliberate — see
[composer_mastermind_design.md](composer_mastermind_design.md)'s "Send To All Is One Preset Among Many"), so the
overlay can't address notes by step index. It doesn't need to: reconstruction already converts each instance's own
step position into a real onset time (in host ppq/bars), accounting for that instance's own grid mode/swing, before
it reaches the buffer. Differing subdivisions between instances just interleave correctly on the shared time axis;
visible bar/beat gridlines in the view are a display aid, not the addressing scheme.

**Recorder buffer, shared by both consumers — still pending, not built this pass.** Because this system mutates
live (governor/mutation policy make real-time decisions — see [mutation_policy_v0_1.md](mutation_policy_v0_1.md)),
"the pattern as authored" and "what actually got played this take" can diverge run to run. The full vision:
accumulate into one running buffer of actual triggered notes, timestamped to host ppq, as playback happens — not
just recompute a snapshot on demand — so that one buffer is what both the live scrolling view draws (its recent
tail) and what the drag-out export bounces (the whole accumulated take). What shipped instead this pass (below) is
a smaller, real slice of this: a live *playhead position*, not yet a historical buffer. Drag-out export still
depends on the buffer half being built.

**Live sync v1 — built 2026-08-22: a reconstructed playhead over the existing static grid, not the buffer/
scrolling-timeline vision yet.** `PrimaryView` reads `getPlayHead()`/`getPosition()` directly on the message
thread (safe off the audio thread — it only reads the host's own lock-free published transport snapshot, so no
new plumbing through `PluginProcessor::processBlock` was needed) and, for each playing instance, computes a
moving playhead position by mirroring MPL's own step-clock math exactly (`Source/PluginProcessor.cpp`'s
`getGridStepCount`/`gridStepLengthInPpq`/`playbackStepIndex` — grid mode picks 12 or 16 steps per bar, tracked
Length clamps the loop, both already tracked in `InstanceStateTracker`). `PianoRollView` draws it as a moving
vertical line over the same step-grid Phase 1 already renders (in both Overlay and Lane mode) — not yet a
scrolling "whole piece so far" timeline; the grid still only ever shows one loop cycle, as it did before this
pass, just now with a synced-to-host-tempo playhead sweeping across it.

**Phase anchor: exact from the moment Composer Mastermind sees a pattern become active, approximate before
that — a real, named limitation, not glossed over.** MPL exposes no "which bar did you launch this pattern"
field to read back, so the reconstruction anchors each instance's loop phase to the most recent host bar-start at
the moment Composer Mastermind's own polling first observes that instance's Active Pattern (`InstanceStateTracker`)
at its current value. This is byte-exact for any switch Composer Mastermind itself causes or has been running
since. For a pattern that was *already* looping before the Live view started watching (or before playback even
began), the reconstructed phase can be off by up to one loop cycle if the tracked Length doesn't evenly divide
the grid's step count (12 or 16) — powers-of-two lengths (1/2/4/8/16 binary, 1/2/3/4/6/12 ternary) are unaffected,
since any bar-aligned anchor produces an identical phase for those. Tracking (`lastSeenActivePattern`/
`launchPpqByInstance` in `ui/PrimaryView.h`) clears entirely on transport stop, so every fresh play start
re-anchors cleanly rather than trusting a bar number from a since-ended playthrough.

## Drag-out MIDI export

**Built 2026-08-22.** `ComposerMastermindAudioProcessorEditor` now inherits `juce::DragAndDropContainer` (the
standard JUCE place for this); `PianoRollView` detects a deliberate drag (past an 8px threshold, gated to
read-only/Live mode only so it never fights Setup mode's own gestures) and calls back into `PrimaryView` to build
and write a temp `.mid` file, then performs the OS-level drag itself.

- **Stopped-transport only**, as resolved — `PrimaryView::requestExportFile` returns an invalid `File{}` while
  `lastKnownIsPlaying` is true, which `PianoRollView` treats as "don't start a drag."
- **Format 1, one track per instance** — built from the recorder buffer's `RecordedNote::instanceId` grouping,
  preserving voice separation into the DAW/notation app for free (the monophonic constraint above), plus a track-
  name meta-event per instance.
- **Tempo map included**, from `lastKnownTempoBpm` (captured from the playhead the same way Live sync's own
  reconstruction is).
- **Section labels as MIDI marker meta-events (type 6)**, from the active blueprint's sections
  (`ComposerCore::getCurrentBlueprint`) — `section.startBar` is already relative to the take's own start (the same
  bar-0-at-play-start origin `takeStartPpq` anchors to), so no extra offset math was needed.
- **Export variant toggle, built as a header button** (`PrimaryView::exportVariantButton`, "Export: As Performed" /
  "Export: For Notation") — cycles which of `RecordedNote::performedOnsetPpq` (real timing, including swing) or
  `quantizedOnsetPpq` (grid-aligned, swing removed) gets written. Both values are captured at record time
  (see the recorder buffer below), so the toggle needs no separate quantization pass at export time.
- **Deliberate-drag threshold** — matches Stellarizer's own drag-out implementation, cited when this was designed:
  their changelog documents fixing exactly this failure mode ("Pattern cards now require a deliberate drag before
  starting MIDI export... card-drag release cannot accidentally activate SEND TO SHAPE"). `PianoRollView`'s
  `kDragThresholdPixels` (8px) is that same guard, ported rather than treated as an afterthought.

**Recorder buffer — built 2026-08-22, alongside Live sync's playhead reconstruction, not a separate later pass.**
`PrimaryView::recordedNotes` accumulates every step actually reconstructed as sounding (see the Live sync section
above for the exact step-clock math this reuses), catching up on every step boundary crossed since the last
60ms tick — not just the current one, so a slow poll tick can never silently skip a note. Resets on every fresh
play start (a new take), not on stop, matching "the buffer only ever needs to hold the most recent take" from the
stopped-transport-only design above. Each `RecordedNote` carries both a performed and a quantized onset, computed
together at capture time (the same swing-delay formula from the arc-dimension mapping work), so drag-out's variant
toggle is just a field selection at export time, no separate quantization logic needed. **Swing-banding rule not
yet needed:** the {0%, 66.67%} snap rule from `arc_dimension_mapping_concept.md` was designed for a *continuously
curve-driven* Swing value (Density→Swing, Track B, not yet built) — today Swing is still a static per-scene value,
so plain quantization (drop the swing offset entirely) is already correct and sufficient; revisit this once Track B
lands.

## Setup mode: authoring P1/P2/P3 content

**Added 2026-08-22.** Answers the "how do we author each instance's initial pattern content" question raised
alongside the piano-roll design — not a separate tab with new plumbing, but a second mode of the same component:

- **Live mode** (everything above) — read-only, reconstructed from playback, overlay or lane view, scrolling.
- **Setup mode** — editable, always lane view (one instance/pattern at a time; an overlay doesn't make sense for
  authoring, since a new note needs to know which instance it belongs to), with a P1/P2/P3 selector per lane.
  Click/drag to place notes; a "commit" action writes straight through `PatternSyncServer::sendWriteFullPattern` —
  the exact IPC mechanism already built and proven in v0.6 for `MotifEngine`'s stamping. No new backend work, only
  a new editable rendering mode reusing the same visual language (colors, grid, note-name convention) as Live mode.

**Locked notes — built 2026-08-22, from Stellarizer's Locked Notes feature.** A per-step lock, Composer-
Mastermind-side only (MPL never sees it): `state/LockedStepLibrary` (session-local, not yet persisted across
project save/reload) tracks locked (instanceId, patternIndex, stepIndex) tuples. Enforced at every automated
write path — `policy/MotifEngine.cpp`'s `writeStepDirect` (per-step nudges, refuses the whole write so the cache
never claims a value that wasn't actually sent) and `stampOnePattern` (splices the locked index's current cached
value back into a full-pattern stamp/seed write), plus `ComposerCore`'s Presentation baseline reset (same splice,
since "protected from automation" has to mean *every* automated write or it isn't reliable). `PianoRollView` gates
Setup mode's own editing gestures on lock state too, matching Stellarizer's own semantics ("unlock them whenever
you want to edit them again") rather than only protecting against the engine: Ctrl+click toggles a lock (a new
gesture, since MPL's own Melody view has no equivalent to match), and every mutating gesture (erase, move,
resize, pitch-change) refuses on a locked note until it's unlocked. `PatternSetupView`'s own Commit deliberately
bypasses locks — direct authorship is how a lock would get intentionally overridden in the first place. Locked
notes render with a white outline in both Live and Setup mode.

## Pitch axis

**Resolved (2026-08-22): real MIDI pitch/note names, MIDI note 60 displayed as C3.** Not the mockup's coarse
12-row contour — the real view shows actual note names, since that directly serves the notation-export goal. The
C3=60 convention is a deliberate choice (rather than the C4=60 convention some other software/notation defaults
use) to match Bitwig's own note naming, so labels agree with what the user already sees in Bitwig and MPL's UI. Any
note-name rendering and MIDI-file export labeling in this feature should use this convention consistently. A
piano-key gutter alongside the note names is a nice-to-have, not required.

## Rolling window + zoom

**Resolved (2026-08-22): window = whole piece so far**, not a fixed bar count or current-section-only — the live
view accumulates and shows everything played since transport start, matching the recorder buffer's own scope (see
above) rather than being a narrower sub-window of it. Two independent zoom axes on top of that:

- **Horizontal (time)** — pixels-per-beat, changes how much of the accumulated piece is visible at once. Standard
  piano-roll scroll-to-zoom convention (ctrl/cmd + scroll, or a slider).
- **Vertical (pitch)** — default auto-fit to the pitch range actually in use (3 monophonic lines rarely span a
  full keyboard), with manual zoom/pan to override — e.g. to check register collisions between instances
  deliberately.

Neither axis touches the reconstruction/recorder mechanism underneath; both are view-layer only, so they can be
built or changed independently of the live-sync and drag-out work.

## Open questions

All the piano-roll/live-sync/drag-out questions from the first design pass are resolved above (2026-08-22).

## Curve-based blueprint authoring ("the cockpit") — built 2026-08-22 evening

The one item left open above: showing a loaded blueprint's "intended shape" as an editable arc, from the original
2026-08-21 sketch. Designed via a `visualize`-tool mockup pass, approved, then built the same session. This is the
user's own most-wanted piece of the whole redesign — their framing when handing it off: everything up to this
point was "under the hood," this is "the cockpit."

**Data-model question, resolved (hybrid):** a blueprint with a real, explicitly-saved arc uses it verbatim; a
blueprint with no arc data at all (e.g. "Vers la flamme," hand-composed as explicit sections) gets a display
curve computed live from its section/archetype sequence instead — a real value is never invented and written
without the user asking for it. The moment a derived curve is edited, it's promoted to real saved data. No
existing blueprint needed a migration; a curve is never unavailable.

- `model/Blueprint.h`: `BlueprintArcPoint` (bar, value) and `BlueprintArcCurve` (dimension name + points),
  `Blueprint::arcCurves` — a dimension absent from this list means "still derived," not "flat zero."
- `policy/BlueprintGenerator::deriveArcSetFromSections` — the derivation itself, reusing the exact per-archetype
  baseline table (`tensionBaseline`/`complexityBaseline`/`coherenceBaseline`/`densityBaseline`, plus a new
  `energyBaseline`) that `generate()`'s `bakeDerivedArcs` already used for a *generated* blueprint's non-driving
  dimensions — now also reachable from a plain section list, not just a driving arc's own breakpoints, so a
  hand-authored blueprint gets the same derivation logic a generated one always had.
- `policy/BlueprintGenerator::resolveBlueprintArcSet` — merges stored `arcCurves` over the derived fallback,
  dimension by dimension, and reports which dimensions are still derived. Single source of truth used both by
  `ComposerCore::setCurrentBlueprint` (syncs the live `ArcSet` — Track B's continuous consumers and any
  `arc`-mode `ModulatorTarget` read this every bar) and by the new authoring UI.

**Real gap found and fixed along the way:** before this, activating a blueprint never touched the live `ArcSet` at
all — it just kept whatever the Arcs tab or the last `GenerateView` commit happened to leave it at, decoupled from
which blueprint was actually active. `ArcSet` itself was never persisted anywhere (not in `StateSerializer`, not
in the snapshot). `setCurrentBlueprint` now always resyncs the live `ArcSet` from whichever blueprint it's given,
via `resolveBlueprintArcSet` — the single sync point every activation path (`GenerateView::commitClicked`, the MCP
bridge's `commitBlueprint`, `BlueprintSectionsContent`'s Load button, project-reload restore) now shares.
`GenerateView::commitClicked` was simplified to match: it now writes its full generated 5-curve `ArcSet` onto
`Blueprint::arcCurves` before committing, instead of a one-off manual push into the live `ArcSet` that left no
persisted trace. `BlueprintSectionsContent::saveBlueprintClicked` (which rebuilds a fresh `Blueprint` from its
working section list) now preserves any `arcCurves` already saved on that blueprint id — otherwise resaving from
the Sections tab would have silently wiped out curve data authored in the new view.

**UI: `ui/BlueprintArcCurveView`**, reached via a new "Compose" toggle in `ui/MainShellView` (a third peer view
alongside `PrimaryView` and `EditorView`, following the same "never touches the other two internally" rule
`MainShellView` already keeps for `EditorView`). Blueprint picker, a "Derived from sections" / "Custom arc" badge,
and `ui/ArcGraphView` reused unchanged for the actual graph (drag/add/delete) — bound to a scratch `ArcSet`
resolved via `resolveBlueprintArcSet` rather than the live one, the same "preview a draft, touch nothing live
until an explicit action" shape `GenerateView` already established for its own candidate `ArcSet`. `ArcGraphView`
gained two small callbacks (`onSelectionChanged`, `onEdited`) and a `getSelectedArcName()` getter so the badge can
track derived-vs-custom per dimension — its first new consumer since `GenerateView`. Save always clones: it
writes every dimension currently in the scratch `ArcSet` onto a **new** blueprint (same sections/scene
references, new id/name) rather than overwriting the original in place — matching the user's own framing,
"saved as new use presets, basically new musical pieces."

Compiled clean for both `ComposerMastermind_VST3` and `ComposerMastermind_Standalone`.

**Confirmed working live (2026-08-22), after three real bugs found and fixed during testing:**
1. `resolveBlueprintArcSet` trusted any stored curve unconditionally, including one with zero points, letting it
   silently overwrite the always-non-empty derived fallback — fixed by skipping empty stored curves (falls back
   to derived instead).
2. `ArcGraphView::onEdited` only repainted the badge from a list computed once at load time, so it never actually
   reflected a live edit — fixed by removing the edited dimension from that list right in the callback.
3. **The actual root cause**, found only after the first two fixes still didn't resolve the reported symptom:
   `ArcSet::getArc()` returns an `Arc` by value, and both `BlueprintArcCurveView::saveClicked` and
   `GenerateView::commitClicked` chained straight into `.getArc(name).getBreakpoints()` — binding a reference
   into a temporary `Arc` that C++ does not lifetime-extend through the intermediate method call. The temporary
   was destroyed before the reference was read, and on this toolchain that reliably read back as a silently
   empty vector rather than crashing — meaning **every save this feature ever performed had been writing empty
   curves from its very first build**, with the first two bugs only changing how that emptiness manifested.
   Fixed by naming the `Arc` as a real local before reading its breakpoints in both places.

**"New Blueprint" - start from a blank curve, built 2026-08-27.** The cockpit could only ever *reshape* an
existing blueprint's curves (pick one, edit, "Save as new blueprint" clones its same sections) - there was no way
to start from nothing, choose a length, and draw a whole piece here. Meanwhile `policy/BlueprintGenerator::generate`
(the actual curve-shape→sections engine: one Peak at the driving arc's global maximum, Presentation/Build before
it, Release after, real `startBar`/`durationBars` taken straight from the breakpoints) already existed and worked
- just reachable only from the separate Expert → Generate tab, reading its driving curve from yet a *third*,
independent live-ArcSet editor (`BlueprintView`'s "Arcs" inner tab). Three disconnected curve surfaces; the one
meant to be primary couldn't reach the one that actually turns a curve into a piece.

Fixed by wiring `BlueprintArcCurveView` directly to `BlueprintGenerator::generate`: a "New Blueprint" button resets
to a blank scratch `ArcSet` sized to a chosen bar-length slider (a flat curve per dimension at the requested length,
not `ArcSet{}`'s hardcoded 1..64 default, which would otherwise put the second point off-axis for any other
length); draw one curve, name it, "Generate Blueprint" (the same button, relabeled - one control doing two jobs
depending on mode, not a second competing button). Unlike `GenerateView`'s explicit preview/Commit/Discard, this
commits in one motion - the curve just drawn already served as the preview, matching the user's own "draw here,
then commit" framing. Base scene handling was the one open design question, resolved by the user when asked: auto-
create a minimal default scene (every registered instance, Pattern 1/Binary/Swing Off) rather than requiring one
picked first, so a genuinely empty session can go straight to a playable piece. Compiled clean. **Not yet live-
tested.**

**Monophonic duration-overlap fix, built 2026-08-22.** Turned out not to need the curve-authoring UI as a
precondition after all — the actual place step content gets directly edited is `ui/PatternSetupView`'s Setup mode
(v1.2 Phase 2), which already enforced this exact invariant for hand edits (`ui/PianoRollView`'s
`stepRangeOverlapsExisting`/`maxNonOverlappingDuration`, ported from MPL's own melody-editor logic). What was
still missing was `policy/MotifEngine`'s *automated* writes (stamping/nudging) honoring the same rule. Extracted
the shared logic into `policy/MonophonicOverlap` (JUCE-free), had `PianoRollView` delegate to it unchanged, and
applied it to `MotifEngine::stampOnePattern` (covers `stampMotifForSection`/`seedPhraseChainPatterns`/
`restampPhraseChainSlot`) and `applyForSection`'s per-note write. Applied unconditionally — MPL instances are
monophonic by design, not per-instance-configurable, so no new flag was needed. Compiled clean, both targets.

Live testing found this still wasn't enough: overlaps persisted for content this session's writes never touched
(hand-edited in MPL, or written before the constraint existed). Added a second layer — `MonophonicOverlap::
normalizePattern` runs inside `state/InstancePatternCache::store` itself, the one place every real pattern
snapshot passes through regardless of source, guaranteeing the invariant on the data itself rather than depending
on every writer remembering to clamp (the user's own framing: "a filter, no matter what a blueprint says").
**Confirmed working live.**

**Custom blueprint length, built 2026-08-22.** The graph was hardcoded to a fixed 1-64 bar axis — not every piece
is 64 bars. `ArcGraphView::kMaxBar` became a settable instance member (`setMaxBar`, still defaulting to 64 so the
live Arcs tab and `GenerateView`'s preview — neither tied to one specific blueprint's length — are unaffected).
`BlueprintArcCurveView` now sizes the graph to each selected blueprint's own real span (furthest section end, or
furthest saved arc point) on load, falling back to 64 only for a genuinely empty/new blueprint. Gridline spacing
is adaptive too, picking the smallest "nice" step that keeps the count readable at any scale. Compiled clean,
both targets — not yet live-confirmed.
