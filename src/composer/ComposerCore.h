#pragma once

#include <mutex>
#include <atomic>
#include <map>
#include <deque>
#include <string>
#include <vector>
#include "../routing/InstanceRegistry.h"
#include "../routing/InstanceStateTracker.h"
#include "../routing/Router.h"
#include "../midi/CCDispatcher.h"
#include "../scheduling/Scheduler.h"
#include "../state/SceneLibrary.h"
#include "../state/BlueprintLibrary.h"
#include "../state/PresetLibrary.h"
#include "../state/ModulatorTargetLibrary.h"
#include "../state/ModulationRouteLibrary.h"
#include "../state/LockedStepLibrary.h"
#include "../state/MilestoneLibrary.h"
#include "PatternSyncServer.h"
#include "McpBridgeServer.h"
#include "../policy/PolicyEngine.h"
#include "../policy/MotifEngine.h"
#include "../policy/ArcSet.h"
#include "../model/Scene.h"
#include "../model/Blueprint.h"

// One human-readable line in ComposerCore's activity log (2026-08-21, user's
// own request: "I need at least some visual reference, or a log of whatever
// combination of parameters/values have changed and when" - a real gap once
// the engine started making several structural decisions per section
// [stamp, baseline reset, phrase-chain seed/advance, rhythm curve] that
// weren't otherwise visible without cross-referencing Awareness or just
// trusting the audio). Deliberately coarse - section-level/instance-level
// decisions, not every individual CC; see ComposerCore::logActivity's call
// sites for exactly what's logged.
struct ActivityLogEntry
{
    int bar = 0;
    std::string message;
};

// Global, session-level switch (2026-08-23, user's own request) for how a
// section's real note content gets onto an instrument at section entry.
// "Generative" is the existing, only-ever behavior: MotifEngine derives
// content from a tagged MotifPreset's relative shape plus whatever's
// currently cached from the live instrument - never stored verbatim,
// different every time depending on what was already there. "Absolute" is
// the new alternative: if a section carries BlueprintSection::capturedContent
// (literal step data, captured once via the Sections tab's "Capture Current"
// button), that gets written verbatim instead, and the whole section is then
// exempt from every subsequent per-bar automatic modification for as long as
// it plays - a true "load this file, hear exactly this every time" piece, the
// mechanical-piano-roll/demo-song reading the user asked for. A section with
// no captured content behaves generatively regardless of this switch - the
// mode only ever governs sections that actually have something to be
// absolute about.
enum class ContentMode
{
    Generative,
    Absolute
};

class ComposerCore
{
public:
    ComposerCore();

    InstanceRegistry& getInstanceRegistry();
    CCDispatcher& getCCDispatcher();
    Router& getRouter();
    Scheduler& getScheduler();
    SceneLibrary& getSceneLibrary();
    BlueprintLibrary& getBlueprintLibrary();
    PresetLibrary& getPresetLibrary();
    ModulatorTargetLibrary& getModulatorTargetLibrary();
    ModulationRouteLibrary& getModulationRouteLibrary();
    PatternSyncServer& getPatternSyncServer();
    McpBridgeServer& getMcpBridgeServer();

    // Steps hand-authored via Setup mode and marked protected - never
    // touched by MotifEngine's automated writes or a Presentation baseline
    // reset (see state/LockedStepLibrary.h). Session-local, not yet
    // persisted across project save/reload.
    LockedStepLibrary& getLockedStepLibrary();

    // Playback milestones (Phase 3, 2026-08-22, see state/MilestoneLibrary.h) -
    // captured automatically at every section entry, browsable/restorable
    // from the UI. Session-local, not persisted across project save/reload.
    MilestoneLibrary& getMilestoneLibrary();

    // Re-sends everything a captured milestone remembers - each instance's
    // Active Pattern/Grid Mode/Swing and per-pattern Transpose/Rotation/
    // Length/Inversion/Retrograde/M7 (direct CC, bypassing PolicyEngine/Router entirely -
    // this is a restore, not a discrete authored decision, the same
    // reasoning as v1.2's continuous-automation dispatch path), plus a full
    // writeFullPattern re-send for whichever pattern indices the milestone
    // actually had confirmed content for. Updates InstanceStateTracker and
    // InstancePatternCache to match, so the plugin's own belief of state
    // agrees with what was just restored. Returns false (does nothing) if
    // index is out of range.
    bool restoreMilestone(size_t index);

    // Global, session-level - governs how MotifEngine::applyForSection
    // writes a chosen motif (see policy/MotifEngine.h), not per-preset or
    // per-section.
    MotifEngine::ApplicationMode getMotifApplicationMode() const;
    void setMotifApplicationMode(MotifEngine::ApplicationMode mode);

    // See ContentMode's own doc comment above.
    ContentMode getContentMode() const;
    void setContentMode(ContentMode mode);

    PolicyEngine& getPolicyEngine();
    InstanceStateTracker& getInstanceStateTracker();
    ArcSet& getArcSet();

    // Diagnostic 0..1 score over currently-registered instances' last-sent
    // CC state - see policy/CoherenceEvaluator.h.
    float getCurrentCoherence() const;

    // Synthetic "arc dimension" name a ModulatorTarget can select to emit the
    // blueprint playhead's own 0..1 progress (elapsed / total span) instead of
    // a real authored ArcSet dimension - the MC side of the OrchConductor
    // Narrative Scan bridge (docs/composer_mastermind_design.md). Not a member
    // of ArcSet::getArcNames(); sendModulatorTargetUpdates special-cases it.
    static constexpr const char* kNarrativePositionDimension = "narrative-position";

    // 0..1 progress of currentBar through the current blueprint's full section
    // span (min startBar .. max startBar+durationBars). 0 if no blueprint is
    // current, it has no sections, or the span is degenerate. Clamped.
    float getNarrativePositionAt(int currentBar) const;

    // Pitch-field broadcast: emit the pitch classes the structural voices are
    // currently sounding (each registered instance's active-pattern content
    // put through the M7 / Inversion / Transpose it carries, matching MPL's
    // own transform order) as a 12-bit mask in two CCs from a base number, so
    // an OrchNoteFilter wash can track the composition's harmony instead of a
    // hand-authored scale. Sent every processBar tick, but only when the mask
    // actually changes. Session-level, not persisted (like ContentMode /
    // MotifApplicationMode).
    bool isPitchFieldBroadcastEnabled() const;
    void setPitchFieldBroadcastEnabled(bool enabled);
    int getPitchFieldBroadcastBaseCc() const;
    void setPitchFieldBroadcastBaseCc(int cc);
    int getPitchFieldBroadcastChannel() const;
    void setPitchFieldBroadcastChannel(int channel);
    // Hold a changed field for at least this many bars before re-broadcasting,
    // so per-bar transpose nudges don't make the wash's field flicker. 0 = send
    // every bar it changes.
    int getPitchFieldBroadcastMinBars() const;
    void setPitchFieldBroadcastMinBars(int bars);
    // The 12-bit mask most recently broadcast (bit i set = pitch class i on).
    // -1 = nothing broadcast yet.
    int getLastBroadcastPitchFieldMask() const;

    // Most recent activity log entries, newest first (see ActivityLogEntry
    // above) - capped at kMaxActivityLogEntries regardless of how much has
    // actually happened, so this never grows unbounded over a long session.
    std::vector<ActivityLogEntry> getRecentActivityLog() const;

    // Bar most recently passed to processBar (0 before playback starts).
    // Lets UI-triggered actions (which don't otherwise know "now") pass a
    // bar to the policy gate / scene chain so it isn't only audio-thread-aware.
    int getCurrentBar() const;

    // Callable from the UI thread; safe to call concurrently with processBar.
    // Setting a scene re-anchors the scene-chain clock: if it has a
    // nextSceneId, its durationBars starts counting from whichever bar
    // processBar next runs on.
    void setCurrentScene(const Scene& scene);
    bool getCurrentScene(Scene& outScene) const;

    // The blueprint currently being authored/played, if any. Clears the
    // tracked active section so the very next processBar tick re-evaluates
    // from scratch and routes whichever section actually covers the current
    // bar (see advanceBlueprintIfNeeded) - matters when the blueprint was
    // just edited and the previously-active section id may no longer mean
    // what it used to.
    void setCurrentBlueprint(const Blueprint& blueprint);
    bool getCurrentBlueprint(Blueprint& outBlueprint) const;

    // Id of the BlueprintSection currently covering getCurrentBar(), or
    // empty if no blueprint is active or no section covers the current bar.
    // For UI display (ui/BlueprintSectionsView).
    std::string getActiveSectionId() const;

    // Audio-thread bar tick: dispatches due scheduled events, advances the
    // scene chain (Scene::nextSceneId) if the active scene's durationBars
    // has elapsed, and advances the active blueprint's section timeline if
    // a blueprint is current (see advanceBlueprintIfNeeded). barStartPpq is
    // the host's own ppq position for this bar (PluginProcessor's already-
    // captured getPpqPositionOfLastBarStart() value) - captured as the active
    // section's phase-zero anchor on every section entry, for
    // processStepTick's loop-cycle math below.
    void processBar(int currentBar, double barStartPpq);

    // Audio-thread per-block tick (2026-08-27, "fragment sequencer" -
    // docs/composer_mastermind_design.md): called every block while playing,
    // NOT edge-detected like processBar - a ModulationRoute in Sequence mode
    // needs sub-bar granularity (once per pattern LOOP CYCLE, not once per
    // bar) to produce real melodic sequencing on a Length-shrunk pattern.
    // currentPpq is the host's live running ppq position
    // (AudioPlayHead::PositionInfo::getPpqPosition(), distinct from
    // getPpqPositionOfLastBarStart() - processBar's own timing source). See
    // scheduling/StepClock.h for the underlying tempo-invariant math and
    // ComposerCore.cpp's implementation for the frozen-section gate and
    // per-(route,instance) last-fired-index bookkeeping.
    void processStepTick(double currentPpq);

    // Immediately routes the current scene (used for manual "send now" UI actions).
    void fullRefresh();

    // Called when the host transport stops/restarts, so a scene's elapsed
    // duration is counted from the new playback start rather than stale bar
    // numbers from before the stop.
    void notifyTransportReset();

    // "Prepare while stopped" (user's own idea, 2026-08-21): runs the
    // current blueprint's first section's full entry sequence (scene route,
    // Presentation baseline reset, section-entry stamp) right now, callable
    // from the UI/bridge with the transport stopped - not gated behind
    // processBar/playback like every other blueprint effect. All these CC/
    // IPC writes land instantly regardless of transport state (confirmed by
    // the user manually wiggling MPL's own knobs while stopped), so there's
    // no reason content has to wait for playback to actually begin before
    // it's prepared - and priming while genuinely stopped sidesteps any
    // chance of racing MPL's own playback engine reading pattern data at the
    // same moment audio starts, which every CC-burst bug chased this session
    // was some version of. Deliberately doesn't touch currentActiveSectionId/
    // motifPassCountForSection/lastMotifPassBar - those stay whatever they
    // already were, so advanceBlueprintIfNeeded's own bookkeeping re-enters
    // the first section normally (and harmlessly re-applies the same clean
    // state) the moment real playback reaches it, rather than priming trying
    // to fake being "already there." No-op if no blueprint is current or it
    // has no sections.
    void primeForPlayback();

    // Factory reset (2026-08-23, user's own request): clears every library
    // (instances, scenes, blueprints, all four preset categories, modulator
    // targets) and every "current scene/blueprint" tracking member back to a
    // freshly-constructed ComposerCore's own starting state. Destructive and
    // immediate - the UI's own reset button is responsible for confirming
    // with the user first; this method itself never asks. Session-local
    // state (locked steps, milestones, activity log) is deliberately left
    // alone - those aren't part of "the piece," they're this session's own
    // bookkeeping, and clearing the activity log would erase the very record
    // of the reset having happened.
    void resetToFactoryDefaults();

private:
    InstanceRegistry instanceRegistry;
    CCDispatcher ccDispatcher;
    PolicyEngine policyEngine;
    InstanceStateTracker instanceStateTracker;
    Router router;
    Scheduler scheduler;
    SceneLibrary sceneLibrary;
    BlueprintLibrary blueprintLibrary;
    PresetLibrary presetLibrary;
    ModulatorTargetLibrary modulatorTargetLibrary;
    ModulationRouteLibrary modulationRouteLibrary;
    PatternSyncServer patternSyncServer;
    McpBridgeServer mcpBridgeServer;
    ArcSet arcSet;
    LockedStepLibrary lockedStepLibrary;
    MilestoneLibrary milestoneLibrary;

    // Builds and captures one Milestone for the current instant - reads
    // InstanceStateTracker + InstancePatternCache for every registered
    // instance. Called once from enterSection; factored out since restore
    // needs no part of this, only capture does.
    void captureMilestone(const std::string& label, int currentBar);

    std::atomic<MotifEngine::ApplicationMode> motifApplicationMode { MotifEngine::ApplicationMode::Nudge };
    std::atomic<ContentMode> contentMode { ContentMode::Generative };

    // Pitch-field broadcast (MC -> OrchNoteFilter): the 12-bit pitch-class mask
    // is sent as TWO CCs - baseCc = pitch classes 0-6 (low 7 bits), baseCc+1 =
    // 7-11 (high 5 bits). A 12-CC one-per-class block would collide with
    // CC120/CC121 (All Sound Off / Reset All Controllers).
    std::atomic<bool> pitchFieldBroadcastEnabled { false };
    std::atomic<int> pitchFieldBroadcastBaseCc { 110 };
    std::atomic<int> pitchFieldBroadcastChannel { 1 };
    std::atomic<int> pitchFieldBroadcastMinBars { 0 };
    std::atomic<int> lastBroadcastPitchFieldMask { -1 };
    std::atomic<int> lastPitchFieldBroadcastBar { -1 };

    std::atomic<int> currentBarValue { 0 };

    mutable std::mutex sceneMutex;
    Scene currentSceneData;
    bool hasCurrentScene = false;
    bool sceneStartPending = false;
    int currentSceneStartBar = -1;

    // How often (in bars) a still-active section fires its next motif pass
    // after the first (section-boundary) one - see fireMotifPassIfDue.
    // Lowered from 4 (2026-08-20): user feedback that a 16-step pattern
    // looping 3+ times between passes read as mechanical, not gently
    // developing - halved so change lands roughly every one-and-a-quarter
    // loops instead of every three.
    static constexpr int kPassIntervalBars = 2;

    // The "melodic average curve" applyContinuousMelodicCurve walks each
    // touched instance's Transpose toward every bar (v1.2 Track B,
    // docs/arc_dimension_mapping_concept.md) - a slow sine wave over the
    // current bar, one full up-down cycle every kMelodicCurvePeriodBars, now
    // riding on a register center and amplitude sampled fresh from the
    // Energy/Tension arcs each bar rather than fixed constants. Not a
    // per-instance value (deliberately - every instance shares the same
    // curve, walking toward wherever it currently sits from its own current
    // transpose, so instances drift toward a shared tonal center without
    // ever being forced to unison). Period stays fixed on purpose - see
    // arc_dimension_mapping_concept.md's "one thing at a time" scoping note.
    static constexpr double kMelodicCurvePeriodBars = 16.0;

    // Energy maps onto the curve's amplitude (0..1 -> this range), replacing
    // the old flat 5.0/2.0-semitone split between normal sections and
    // Presentation - a low-energy Presentation section now narrows
    // naturally from its own low Energy baseline instead of needing its own
    // special-cased constant (still gentler in practice, just no longer a
    // separate code path). Ceiling raised slightly past the old 5.0 so a
    // genuinely high-energy peak has real room above every other section.
    static constexpr double kMinMelodicCurveAmplitudeSemitones = 1.0;
    static constexpr double kMaxMelodicCurveAmplitudeSemitones = 6.0;

    // Tension maps onto the curve's register *center* (0..1 -> 0..this many
    // semitones, upward only - "pull the shared tonal center upward" as
    // tension rises, not a symmetric wobble). One octave ceiling.
    static constexpr double kMaxTensionRegisterPullSemitones = 12.0;

    // Coherence -> Retrograde/M7 divergence thresholds (2026-08-25,
    // see applyCoherenceDivergenceIfDue). Deliberately set BELOW every
    // archetype's baseline coherence value (policy/BlueprintGenerator.cpp's
    // coherenceBaseline: Presentation 0.6, Build 0.5, Peak 0.8, Release
    // 0.45) so picking an archetype alone never triggers divergence - it
    // only fires when a section's coherence arc is deliberately authored/
    // hand-curved down past these points, matching the "impulse target,
    // curve-timed" character every other threshold-crossing consumer here
    // has (see firePhraseChainIfDue). Two thresholds, not one, so falling
    // coherence escalates in two stages: Retrograde alone at moderate
    // divergence, M7 added on top at severe divergence - matching Section
    // Release's baseline (0.45) staying just above both, so it takes real
    // authored intent, not just picking Release, to cross either.
    static constexpr float kCoherenceRetrogradeThreshold = 0.35f;
    static constexpr float kCoherenceM7Threshold = 0.15f;

    // Phrase-chain target banding (v1.2 Track B, 2026-08-22,
    // docs/arc_dimension_mapping_concept.md): replaced the old flat
    // kPhraseChainIntervalBars metronome (advance every 8 bars, blind
    // 1->2->3->1 cycling) with threshold-crossing off the Complexity arc -
    // see firePhraseChainIfDue. Checked every bar (called from the same
    // site fireMotifPassIfDue/applyContinuousMelodicCurve already are), but
    // only actually re-stamps and switches when the target role changes -
    // makes the curve feel like it's driving something, not a timer
    // relabeled.
    //
    // Extended same day to MotifEngine::PhraseRole's 5-way vocabulary
    // (richer than MPL's 3 physical pattern slots - see
    // arc_dimension_mapping_concept.md's "richer role vocabulary" note):
    // whichever slot is about to become active gets re-stamped fresh with
    // the target role's shape first (MotifEngine::restampPhraseChainSlot),
    // rather than assuming a fixed P1=base/P2=rotated/P3=inverted seeded
    // once at section entry still holds the right content.

    static constexpr size_t kMaxActivityLogEntries = 500;
    mutable std::mutex activityLogMutex;
    std::deque<ActivityLogEntry> activityLog;

    // Appends one entry, trimming the oldest if over kMaxActivityLogEntries.
    // Called from every structural decision point below (section entry,
    // baseline reset, stamp, phrase-chain seed/advance, rhythm mutation) -
    // deliberately NOT from inside policy/MotifEngine's own per-step note
    // edits, which stay too granular to be useful here (Awareness already
    // shows exact step content for that level of detail).
    void logActivity(int bar, const std::string& message);

    mutable std::mutex blueprintMutex;
    Blueprint currentBlueprintData;
    bool hasCurrentBlueprint = false;
    std::string currentActiveSectionId;
    int motifPassCountForSection = 0;
    int lastMotifPassBar = -1;

    // Novelty-aware preset selection (Phase 3, 2026-08-22,
    // composer_mastermind_design.md's "New Ideas" #7). Two members, not one -
    // easy to get this subtly wrong, worth spelling out why. avoidPresetId
    // has to stay FROZEN for a section's entire lifetime: if it were updated
    // to "what this section just chose" right after stampMotifForSection,
    // the very next call (seedPhraseChainPatterns, moments later, same
    // section) would ask findPresetForArchetype to avoid the preset the
    // section is actively using - and if a second matching preset exists,
    // it would switch to it, contradicting stampMotifForSection's own
    // choice. So:
    //
    // - avoidMotifPresetIdForSection: frozen for the current section's whole
    //   lifetime (set once, in enterSection, from whatever the PREVIOUS
    //   section left in currentSectionMotifPresetId). Every MotifEngine call
    //   this section ever makes - stampMotifForSection, seedPhraseChainPatterns,
    //   applyForSection's ongoing passes, restampPhraseChainSlot - passes
    //   this same unchanging value, so they all agree on the same resolved
    //   preset for as long as this section stays active.
    // - currentSectionMotifPresetId: what THIS section's stampMotifForSection
    //   actually resolved to (empty if nothing matched) - becomes the next
    //   section's avoidMotifPresetIdForSection when enterSection next runs.
    std::string avoidMotifPresetIdForSection;
    std::string currentSectionMotifPresetId;

    // hasPhraseChainRole false = sentinel ("no role observed yet this
    // section"), forcing the very first firePhraseChainIfDue check after a
    // section change to sync immediately to wherever the Complexity arc
    // currently sits, rather than waiting for an actual crossing.
    bool hasPhraseChainRole = false;
    MotifEngine::PhraseRole lastPhraseChainRole = MotifEngine::PhraseRole::Base;

    // Same sentinel shape as hasPhraseChainRole above, for
    // applyCoherenceDivergenceIfDue's own threshold-crossing: false forces
    // the first check after a section change to sync immediately to
    // wherever the Coherence arc currently sits.
    bool hasCoherenceDivergenceState = false;
    bool lastCoherenceRetrograde = false;
    bool lastCoherenceM7 = false;

    // Modulation matrix (ModulationRoute, model/ModulationRoute.h) threshold-
    // parameter state: one entry per route id, holding the last banded/
    // threshold value actually dispatched (-1 = "never sent yet", the same
    // sentinel role hasPhraseChainRole/hasCoherenceDivergenceState play for
    // their own hardcoded consumers) - see sendModulationRouteUpdates.
    // Cleared on every section change alongside those two, so a route syncs
    // immediately to wherever its arc currently sits rather than waiting for
    // an actual crossing. Guarded by blueprintMutex, same as
    // motifHomePatternForSection.
    std::map<std::string, int> modulationRouteThresholdState;

    // Fragment sequencer (2026-08-27) support - see processStepTick's own
    // comment. currentSectionPpqAnchor is the ppq at which the ACTIVE
    // section's own first bar began (captured from processBar's barStartPpq
    // argument on every section entry, alongside the other per-section
    // sentinel resets) - the phase-zero point every Sequence-mode route's
    // loop-cycle counting is measured from. sequenceRouteLastFiredIndex is
    // keyed by "routeId|instanceId" (a route can broadcast to several
    // instances, each with its own independently-tracked pattern content and
    // therefore its own loop-cycle phase) holding the last loop-cycle index
    // actually dispatched, so a route only fires again once the index
    // genuinely advances - same restraint modulationRouteThresholdState
    // gives the threshold-crossing routes. Both guarded by blueprintMutex,
    // both cleared on every section change.
    double currentSectionPpqAnchor = 0.0;
    std::map<std::string, int> sequenceRouteLastFiredIndex;

    // BarCycle phrase-cadence support (2026-08-27) - see firePhraseCadenceIfDue's own
    // comment. Same "routeId|instanceId" -> last-fired-index shape as
    // sequenceRouteLastFiredIndex above, just counted in whole bars (phraseIndex =
    // (currentBar - section.startBar) / phraseLengthBars) instead of ppq loop cycles -
    // no anchor member needed here since section.startBar is already a plain int
    // available at every call site. Guarded by blueprintMutex, cleared on every section
    // change alongside sequenceRouteLastFiredIndex.
    std::map<std::string, int> phraseCadenceLastFiredIndex;

    // Keyswitch-style lookahead (2026-08-27, user's own framing - a CC change
    // is only audible on the step it arrives in time for, not the one it was
    // conceptually "meant for," the same discipline a sample library's
    // keyswitch needs sent slightly ahead of the note it gates): fire a
    // Sequence-mode route's next value once currentPpq is within this many
    // grid-steps of the upcoming loop-cycle boundary, not only after that
    // boundary has already passed. Absorbs this plugin's own per-block CC
    // queuing quantization plus the host's inter-plugin MIDI routing latency,
    // both of which would otherwise risk landing the new value one step late
    // at the destination. Expressed as a fraction of one grid-step's own ppq
    // length (not a fixed millisecond value) to stay tempo-invariant, the
    // same principle scheduling/StepClock.h's own math already commits to -
    // 10% of a step is small enough to be inaudible as "early" at any
    // reasonable tempo, while still comfortably covering a typical few-block
    // round trip.
    static constexpr double kSequenceLookaheadStepFraction = 0.1;

    // Each registered instance's Active Pattern (InstanceStateTracker's
    // 1-indexed convention, 0 = section wants it silent) as of the moment
    // the current section's scene finished routing - the motif engine's
    // call-and-response (CC20 stop/resume) resumes to this value, not
    // whatever InstanceStateTracker currently shows, since the engine's
    // own toggling makes the live value an unreliable "home" once it
    // starts fluctuating pass to pass. Reset/repopulated only on section
    // change - see advanceBlueprintIfNeeded.
    std::map<std::string, int> motifHomePatternForSection;

    // The "preparatory phase" (2026-08-21): authors this section's real seed
    // content once via MotifEngine::stampMotifForSection, which writes each
    // eligible instance's whole pattern in one atomic PatternSyncServer IPC
    // message (writeFullPattern) - see that function's own comment. Called
    // INSTEAD OF applyMotifForSection's passIndex-0 call at section entry,
    // not alongside it; ordinary per-pass development then resumes from
    // passIndex 1 via the normal fireMotifPassIfDue cadence. Unlike
    // applyMotifForSection, this also runs for "presentation" - the opening
    // theme deserves the same deliberate authorship as every later section.
    void stampMotifForSection(const BlueprintSection& section);

    // Phrase-chaining seed (2026-08-21, user's own idea): seeds all three of
    // each eligible instance's patterns at once, via
    // MotifEngine::seedPhraseChainPatterns - P1 the section's base shape, P2
    // rotated, P3 inverted. Called once at section entry, right after
    // stampMotifForSection (in addition to it, not instead - that call still
    // establishes whichever pattern the section starts on as home). Scoped
    // to build/peak/release only, same archetypes firePhraseChainIfDue
    // itself is scoped to - Presentation stays on its single held pattern.
    void seedPhraseChainPatterns(const BlueprintSection& section);

    void dispatchSceneIfNeeded(int currentBar);
    void advanceSceneChainIfNeeded(int currentBar);

    // The "full loop": if a blueprint is active, finds the BlueprintSection
    // (by list order - first match wins if sections overlap, authors are
    // expected to keep them non-overlapping) whose [startBar,
    // startBar+durationBars) covers currentBar, and routes its scene the
    // moment the active section changes. Fully decoupled from the existing
    // Scene::nextSceneId/durationBars chain (advanceSceneChainIfNeeded) -
    // both call router.routeScene but neither touches the other's tracked
    // state, so manual "Send Scene Now" and the older chain mechanism keep
    // working exactly as before even while a blueprint is active.
    void advanceBlueprintIfNeeded(int currentBar, double barStartPpq);

    // The actual "enter this section" sequence - routes its scene (with
    // layer-role overrides applied), sends its modulator values, snapshots
    // motifHomePatternForSection, then resets/stamps/first-passes it via
    // resetRhythmBaselineForSection/stampMotifForSection/applyRhythmForSection.
    // Factored out of advanceBlueprintIfNeeded (2026-08-21) so primeForPlayback
    // can run the identical sequence on demand, decoupled from playback -
    // both callers get exactly the same effects, no separate "priming" code
    // path to keep in sync by hand. No-op if the section has no sceneId or
    // the referenced scene no longer exists.
    void enterSection(const BlueprintSection& section, int currentBar);

    // Applies section.layerRoles's "background" entries as a default
    // SceneInstanceOverride(activePattern=0) onto the section's about-to-be-
    // routed scene copy, unless the scene already authored an explicit
    // override for that instance. See model/Blueprint.h's SectionLayerRole.
    void applyLayerRoleOverrides(const BlueprintSection& section, Scene& scene) const;

    // Sends each of the newly-active section's SectionModulatorValue entries
    // once, alongside its scene (see model/Blueprint.h) - resolves each
    // entry's ModulatorTarget by id for its CC number/channel, and skips any
    // entry whose target id isn't registered.
    void sendSectionModulatorValues(const BlueprintSection& section);

    // Fires the v0.6 motif/rule engine for the given section and pass
    // index - see policy/MotifEngine.h. Gathers registered instances,
    // motif presets, and the live pattern-awareness cache, then delegates
    // the actual decision/write logic entirely to
    // MotifEngine::applyForSection.
    void applyMotifForSection(const BlueprintSection& section, int passIndex);

    // Resets Transpose/Rotation/Length to a clean baseline (0/0/kPatternSteps)
    // AND clears step content (real writeFullPattern + cache) for every
    // instance Presentation touches - direct CC/IPC + InstanceStateTracker
    // update (bypasses Mutation/PolicyEngine - this is a structural reset,
    // like stampMotifForSection's own step-content stamp, not a budget-gated
    // musical change). Called only when Presentation becomes active, right
    // BEFORE stampMotifForSection (order matters: clearing content here is
    // what makes that function's own "no existing content -> middle C"
    // fallback the branch that actually fires). Exists because replaying a
    // piece from the start otherwise inherits whatever the *previous*
    // playthrough left Transpose/Rotation/Length AND the pattern's own
    // remembered pitch center sitting at - confirmed live, 2026-08-21, in
    // two passes: the parameter reset alone still left one instance's
    // stamped notes "sky-high" (stampMotifForSection's register-continuity
    // math was recentering around already-drifted cached content); clearing
    // step content here too is what actually closed it.
    void resetRhythmBaselineForSection(const BlueprintSection& section);

    // Called every processBar tick while a section stays active (i.e. the
    // section itself didn't just change). Fires the next motif pass once
    // kPassIntervalBars have elapsed since the section became active or
    // its last pass fired - this is what keeps a long section developing
    // instead of only ever getting the one nudge it got at its boundary.
    void fireMotifPassIfDue(const BlueprintSection& section, int currentBar);

    // Phrase-chaining scheduler (2026-08-21, user's own idea; rebuilt onto
    // Complexity threshold-crossing 2026-08-22, see lastPhraseChainRole's own
    // comment; extended same day to the 5-way PhraseRole vocabulary). Called
    // every bar; only actually changes anything when the Complexity-derived
    // role differs from last time. Advances each touched instance to the
    // next physical slot in the same 1->2->3->1 rotation as before (never
    // the currently-active one, so nothing gets rewritten out from under
    // live playback), but first re-stamps that slot fresh with the target
    // role's shape (MotifEngine::restampPhraseChainSlot) rather than trusting
    // whatever seedPhraseChainPatterns originally put there (mutating
    // motifHomePatternForSection directly - every consumer of that map
    // already re-reads it fresh each call, so this is the only place that
    // needs to know the chain exists) and sends the real CC20 Active
    // Pattern switch, but ONLY if the instance is currently audible
    // (InstanceStateTracker's tracked activePattern != 0) - an instance
    // CC20's own call-and-response has rested this pass keeps its rest; its
    // chain position still advances silently underneath, so it resumes to
    // the new pattern next time whoPlaysThisPass lets it play, rather than
    // un-resting it out of turn. Scoped to build/peak/release - Presentation
    // stays on its single held pattern (see resetRhythmBaselineForSection/
    // applyRhythmForSection's own Presentation handling). No-op for an
    // instance the section wants silent throughout (home pattern already 0 -
    // never given a chain position to advance).
    void firePhraseChainIfDue(const BlueprintSection& section, int currentBar);

    // Rhythm involvement (third of the agreed musical-refinement round,
    // alongside policy/MotifEngine's bounded pitch movement and CC20
    // call-and-response): fires on the exact same pass cadence as
    // applyMotifForSection, through the *existing* Mutation/Router/
    // PolicyEngine pipeline (Rotation/Length, CC 30-53) rather than
    // reimplementing rhythm logic inside MotifEngine, which stays scoped to
    // CC 60-64/CC20 by design. Rotates between the two by passIndex % 2,
    // alternating nudge direction by a different bit of passIndex so the
    // groove/loop length "breathes" instead of marching toward an extreme
    // and staying there (same restraint the note transform rotation already
    // has).
    //
    // Transpose moved OUT of this pass-based mechanism entirely (v1.2 Track
    // B, 2026-08-22, docs/arc_dimension_mapping_concept.md) - it's
    // continuous now (applyContinuousMelodicCurve, every bar, Energy/Tension-
    // driven), not a discrete budget-gated Mutation, so it no longer belongs
    // on this passIndex-cycling rotation at all. No-op for Presentation
    // (user's own framing, 2026-08-21: Rotation/Length and step content stay
    // completely frozen there, since those genuinely change the motif's
    // identity in a way the continuous register drift doesn't - the drift
    // itself keeps running regardless, from applyContinuousMelodicCurve).
    void applyRhythmForSection(const BlueprintSection& section, int passIndex);

    // The continuous melodic-average-curve (v1.2 Track B): every bar a
    // section is active (called from enterSection for the boundary bar and
    // advanceBlueprintIfNeeded's "still active" branch thereafter, NOT
    // gated by kPassIntervalBars the way applyRhythmForSection/
    // applyMotifForSection are), samples the live ArcSet's energy/tension
    // arcs at currentBar and walks each touched instance's Transpose toward
    // a shared sine wave whose amplitude comes from Energy
    // (kMinMelodicCurveAmplitudeSemitones..kMaxMelodicCurveAmplitudeSemitones)
    // and whose register center comes from Tension
    // (0..kMaxTensionRegisterPullSemitones, upward-only pull). Before
    // dispatch, the raw curve value is tapered per-instance by
    // MotifEngine::taperTransposeForPatternContent (2026-08-27) - a pattern
    // already voiced far from the home register gets less of the curve's
    // pull instead of it stacking on top unconditionally (found live: an
    // already high-voiced pattern got pushed another +10 semitones up).
    // Dispatches via Router::routeContinuousTranspose - deliberately bypasses PolicyEngine's
    // mutation budget, since this is automation re-sampled every bar, not a
    // discrete authored decision (see arc_dimension_mapping_concept.md's
    // "continuous automation needs its own dispatch path" note). Runs for
    // every archetype including Presentation, unlike applyRhythmForSection -
    // Presentation's gentler feel now falls out of its own naturally lower
    // Energy/Tension baseline rather than a separate hardcoded constant.
    void applyContinuousMelodicCurve(const BlueprintSection& section, int currentBar);

    // Density's consumer (v1.2 Track B, docs/arc_dimension_mapping_concept.md):
    // every bar a section is active (same call sites as
    // applyContinuousMelodicCurve), bands the live Density arc's 0..1 value
    // into one of Swing's 3 legal states (CCMapping::swingStateForNormalized -
    // Off/Triplet/Shuffle, v1.28.0, replacing the old full-range linear
    // glide once MPL's own Swing knob narrowed to a 3-state choice) and
    // dispatches via Router::routeContinuousSwing for every touched
    // instance - a fuller/busier texture pushes toward more swing. Density
    // is safe to *read* here even though it's baked from the foreground/
    // background layer-role split at generation time (see
    // arc_dimension_mapping_concept.md's "Density must stay one-directional"
    // note) - this never writes back into layer-role assignment, only reads
    // the already-baked curve.
    void applyContinuousSwing(const BlueprintSection& section, int currentBar);

    // Coherence's first real consumer (2026-08-25, docs/arc_dimension_mapping_concept.md's
    // "Coherence - deliberately deferred" note stays true for the harder
    // "active nudge instances toward agreement" direction described there;
    // this is a different, simpler mapping, structurally identical to
    // firePhraseChainIfDue's Complexity threshold-crossing rather than that
    // deferred idea). Every bar a section is active (same call sites as
    // applyContinuousMelodicCurve/applyContinuousSwing), samples the live
    // Coherence arc and bands it against kCoherenceRetrogradeThreshold/
    // kCoherenceM7Threshold: falling coherence (instances meant to diverge)
    // crosses Retrograde on first, then M7 on top of it as divergence
    // deepens; rising back past a threshold turns the corresponding one back
    // off. Only actually sends CC when the banded (retrograde, m7) pair
    // differs from last time (lastCoherenceRetrograde/lastCoherenceM7), same
    // "curve driving something, not resent every bar" restraint as
    // firePhraseChainIfDue. Sends CC 34/35 (offset within baseCC) directly
    // and updates InstanceStateTracker - bypasses PolicyEngine/Router's
    // Mutation budget entirely, same reasoning as every other continuous/
    // threshold-crossing arc consumer here: this is curve-driven automation,
    // not a discrete authored decision competing for budget.
    void applyCoherenceDivergenceIfDue(const BlueprintSection& section, int currentBar);

    // Modulation matrix conflict guard: true if an enabled ModulationRoute
    // already targets exactly this (instance, parameter [, patternIndex for
    // a pattern-scoped parameter - ignored for a global one, see
    // isGlobalModulationParameter]). Checked by applyContinuousMelodicCurve
    // (Transpose), applyContinuousSwing (Swing), applyCoherenceDivergenceIfDue
    // (Retrograde/M7), and firePhraseChainIfDue (ActivePattern) so a
    // user-authored route can cleanly take over a specific instance's
    // parameter from its built-in curve rather than the two dispatching
    // conflicting values every bar - see docs/composer_mastermind_design.md's
    // modulation matrix entry. A broadcast route ("*") matches every instance.
    bool hasActiveRouteOverride(const std::string& instanceId, int patternIndex, ModulationParameter parameter) const;

    // Modulation matrix per-bar dispatch (ModulationRoute, model/ModulationRoute.h):
    // for every registered, enabled route, evaluates its arc dimension at
    // currentBar, applies invert, then either linearly maps the 0..1 sample
    // into [outputMin, outputMax] (defaulting to the parameter's own full
    // domain when outputMax <= outputMin) and dispatches via
    // Router::routeContinuousParameter every bar (Transpose/Rotation/Length/
    // Swing), or bands/thresholds it and dispatches via
    // Router::routeThresholdParameter only when the banded value actually
    // changes since last time (Inversion/Retrograde/M7/GridMode via
    // `threshold`, ActivePattern via a 4-way band - 0 = stop, 1-3 = pattern) -
    // same "curve driving something, not resent every bar" restraint as
    // firePhraseChainIfDue/applyCoherenceDivergenceIfDue. Called from the
    // same sites as those two (advanceBlueprintIfNeeded's "still active"
    // branch and enterSection), NOT unconditionally from processBar the way
    // sendModulatorTargetUpdates is - routes touch real MPL content, so they
    // must respect the same frozen-section (Absolute content mode) gate the
    // built-in consumers do, unlike ModulatorTarget's external-CC dispatch,
    // which is deliberately unrelated to MPL content at all. Deliberately
    // NOT filtered by section archetype/motifHomePatternForSection eligibility
    // the way the built-in consumers are - a route names its own patternIndex
    // explicitly and is meant to be direct, unmediated control, unlike the
    // built-ins' archetype-aware musical behaviour.
    void sendModulationRouteUpdates(int currentBar);

    // BarCycle phrase-cadence dispatch (2026-08-27) - the third ModulationRoute
    // dispatch mode, alongside Bar/Sequence. For every enabled route with
    // dispatchMode == BarCycle and a continuous parameter, computes
    // phraseIndex = (currentBar - section.startBar) / phraseLengthBars (plain
    // int division - no ppq/anchor tracking needed, unlike Sequence mode's
    // processStepTick, since section.startBar and currentBar are already
    // available here) and, once per route+instance the index genuinely
    // advances (see phraseCadenceLastFiredIndex above), dispatches
    // sequenceValues[phraseIndex % sequenceValues.size()] through the same
    // Router::routeContinuousParameter every Bar/Sequence route already uses.
    // Deliberately bar-granularity only (checked once per bar, same as
    // firePhraseChainIfDue/applyCoherenceDivergenceIfDue) - no sub-bar
    // lookahead machinery needed the way Sequence mode's ppq clock requires,
    // since every other bar-boundary CC send in this codebase already fires
    // plainly at the bar tick. Called from the same two sites as
    // sendModulationRouteUpdates (advanceBlueprintIfNeeded's "still active"
    // branch and enterSection), same frozen-section gate.
    void firePhraseCadenceIfDue(const BlueprintSection& section, int currentBar);

    // Continuous half of ModulatorTarget: for every registered target in
    // "arc" mode, evaluates its named ArcSet dimension at currentBar and
    // sends the result as raw CC (0..127, via CCMapping::encodeFloat) -
    // called every processBar tick, independent of whether a blueprint is
    // active. See model/ModulatorTarget.h.
    void sendModulatorTargetUpdates(int currentBar);

    // See isPitchFieldBroadcastEnabled(). Called every processBar tick; emits
    // the 12 pitch-class CCs only when the computed mask changes (and, if a
    // min-bars hold is set, not more often than that).
    void sendPitchFieldBroadcast(int currentBar);

    // Router::BudgetOverrideResolver implementation: does the current
    // blueprint have a section covering currentBar? If so, always returns
    // true (v1.2 Track B, 2026-08-22 - previously only when that section
    // explicitly authored a budget override for this role, otherwise false
    // and Router fell back to MutationPolicy::budgetForRole directly). The
    // base budget is still resolved the same way (explicit section override,
    // else the static per-role table), but is now always scaled by the
    // Complexity arc's current value before being returned - "more
    // transformation authorized as complexity rises," see this method's own
    // definition for the exact formula. See docs/mutation_policy_v0_1.md and
    // docs/arc_dimension_mapping_concept.md.
    bool resolveSectionBudgetOverride(const std::string& role, int currentBar, RoleBudget& outBudget) const;

    // Answers one MCP bridge request (see McpBridgeServer.h). Read actions:
    // getInstances, getAwareness, getBlueprintStatus, getMotifPresets,
    // getStatus (all params-free), plus listBlueprints/listScenes (id/name
    // discovery, so a write action's id parameter can be chosen from real
    // data rather than guessed). Write actions - the second slice, all
    // routed through the exact same pathways the UI itself uses, no new
    // logic invented: resyncInstance (instanceId, patternIndex - via
    // PatternSyncServer::requestSync), setMotifApplicationMode (mode -
    // "Nudge"/"Phrase"), sendTestCC (instanceId, cc, value - raw
    // CCDispatcher send, bypasses budget gating same as the UI's own Send
    // Test CC diagnostic does), sendMutation (instanceId, patternIndex,
    // type, amount - via Router::routeMutation, real budget gating
    // applies), commitBlueprint (blueprintId - looked up in
    // BlueprintLibrary, then setCurrentBlueprint, mirrors GenerateView's
    // Commit button), setScene (sceneId - looked up in SceneLibrary, then
    // setCurrentScene + fullRefresh, mirrors SceneListComponent's Send
    // Scene Now). Compose actions - author a new library entry from a full JSON payload, reusing
    // StateSerializer's existing deserializers (the same ones project save/load already trusts,
    // not new parsing logic): createScene ({"scene":{...}}, via varToScene), createBlueprint
    // ({"blueprint":{...}}, via varToBlueprint), createMotifPreset ({"preset":{...}}, via
    // varToMotifPreset), setArc ({"dimension":"energy","breakpoints":[{"bar":1,"value":0.2}, ...]}
    // - no existing serializer for a raw ArcSet dimension, built inline). Each create action
    // validates via the same util/Validation checks the UI's own Save buttons use before adding
    // to the library. Unknown actions return {"ok":false,"error":"..."}.
    juce::var handleMcpBridgeRequest(const juce::var& request);

    // Router::ReservedValueChecker implementation: does any section of the
    // current blueprint that starts *after* currentBar reserve this exact
    // (instance, pattern, type, value) combination? See model/Blueprint.h's
    // ReservedValue - the section currently playing (or already past) is
    // never "later", so it's always free to reach its own reserved values.
    bool isValueReservedByLaterSection(const std::string& targetInstance, int patternIndex,
                                        const std::string& type, int value, int currentBar) const;
};
