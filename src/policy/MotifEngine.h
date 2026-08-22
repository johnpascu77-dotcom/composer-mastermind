#pragma once

#include <vector>
#include <string>
#include <map>
#include "../model/Instance.h"
#include "../model/Blueprint.h"
#include "../model/Preset.h"
#include "../midi/CCDispatcher.h"
#include "../routing/InstanceStateTracker.h"
#include "../composer/PatternSyncServer.h"
#include "../state/LockedStepLibrary.h"

// The v0.6 motif/rule engine: fires in passes across a blueprint section's
// duration (not just once at its boundary - see ComposerCore's
// kPassIntervalBars), deterministically decides whether/how to apply a
// MotifPreset to each role-eligible instance's current pattern. No ML, no
// raw randomness - a small fixed rule table over section archetype,
// matching policy/BlueprintGenerator's own philosophy exactly, now with
// real cross-instance pattern content available (state/InstancePatternCache,
// via PatternSyncServer::getCache()) to reason about actual note
// relationships instead of assumed state. Each successive pass within one
// section cycles through the transform vocabulary (rotate/invert/
// retrograde on top of the archetype's own base transform) and rotates
// which steps get touched, so a section keeps developing across its whole
// duration instead of nudging once and going static.
//
// Step-content writes (note/velocity/duration/enabled) go through
// `patternSync` - PatternSyncServer's writeStep/writeFullPattern IPC
// messages (added 2026-08-21), which commit directly into MPL's real
// pattern storage, atomically, with no MIDI involved. This replaced an
// earlier build that wrote the same content via MPL's CC 60-64 Target Step
// Editing protocol: that path drives ordinary APVTS parameters, committed
// via MPL's own once-per-block polling/edge-detection (built for one human
// turning one knob at a time), and when this engine's new section-entry
// "stamp" (see stampMotifForSection) wrote several steps in quick
// succession, every write but the last silently collapsed to whatever the
// last one left the parameters at before that poll ever ran - confirmed
// live, 2026-08-21, via a real resync showing content that didn't match
// what the engine believed it had written. `dispatcher` (CCDispatcher) is
// still used for everything that IS a genuine single scalar automation
// value with no such ambiguity: CC20 (Active Pattern, call-and-response
// rest/resume).
//
// Deliberately bypasses routing/Router's Mutation/PolicyEngine budget-
// gating pipeline for step content - Router's model is pattern-level
// (transpose/rotation/length/inversion have no concept of a step index),
// and threading step-level addressing through it is a bigger redesign than
// this first build attempts. Pass-interval firing (see kPassIntervalBars)
// is the only restraint on frequency for now; real budget integration is a
// natural future extension once the pattern of use is better understood,
// not built yet.
//
// Targets whichever pattern InstanceStateTracker last recorded as each
// instance's Active Pattern (set by every Router::routeScene call) rather
// than a fixed pattern index - confirmed necessary, not just theoretical:
// an earlier hardcoded-to-pattern-0 build silently wrote real note/
// velocity/duration changes into a pattern that wasn't the one actually
// playing whenever a section's scene set an instance to P2/P3, so the
// Awareness cache showed changes (it updates optimistically on every
// write, not on confirmed reality) while nothing was audible. Still
// depends on the Awareness tab having a real resync for whatever pattern
// is actually active for each touched instance - the engine won't write
// blind to a pattern it has no cached content for, even if that pattern
// is the genuinely correct one.
//
// Bounded, centered pitch movement: every new note is clamped to within
// one octave (policy/MotifEngine.cpp's kMaxDeviationFromCenterSemitones)
// of its pattern's own current average pitch, recomputed fresh each pass
// from the live cache - so the pattern's center can still drift slowly and
// organically over many passes, but no single pass can throw an outlier
// far from wherever the pattern actually sits right now. Added as a
// preventive fix, not a response to an observed failure: Nudge mode's
// plain "delta from current cached note" has nothing else pulling it
// back, and compounds pass after pass with only the raw MIDI 0..127 range
// as a backstop - not the "subtle nudge" this engine is meant to produce.
//
// Call-and-response via CC20 (Active Pattern): each pass now also decides,
// per touched instance, whether it should be audibly playing or resting
// (stopped) - not every archetype has every touched instance playing
// constantly. Deterministic by passIndex, same as the note-transform
// rotation (no randomness): Peak never rests anyone (convergence is the
// point); Release lets only one instance play at a time, rotating
// (genuine solo passages, matching its existing thinning-texture
// identity); Build rests exactly one instance per pass if it has 2+
// candidates (a duo trading off). Resting is real - CC20 is actually sent
// - and an instance's "home" pattern for the section (what to resume to)
// is captured once when the section becomes active, via
// `homePatternPerInstance` (1-indexed, matching InstanceParameterState's
// convention; 0 means the section itself wants this instance silent
// throughout, e.g. a background layer role - the engine will never try to
// "revive" that, only ever toggle instances the section actually intends
// to have playing). `stateTracker` is now non-const because a CC20 toggle
// updates it directly (`recordGlobal`), the same way Router does after a
// real scene route - so InstanceStateTracker stays the single source of
// truth for "what does this instance currently believe," not just a
// read-only reference.
//
// The pattern's rhythm grid (which steps are enabled) as a genuine
// variational device, not a fixed backdrop: every pass, one currently-
// silent step gets enabled and one currently-active step gets disabled
// (MotifEngine.cpp's toggleOneStep) - the on/off rhythm grid itself keeps
// shifting pass to pass, not just the note content sitting on top of an
// unchanging grid. Added directly in response to user feedback that the
// engine went audibly mechanical after 3+ loop repetitions between passes -
// note-level nudges alone weren't enough since the underlying rhythm never
// moved. Always leaves at least one step enabled (full silence is CC20's
// job, not this).
namespace MotifEngine
{
    // Which registered instances a given archetype touches at all - the
    // same role-based reach `applyForSection` uses internally for note
    // editing and call-and-response (peak/presentation->everyone, build->
    // motif/counterpoint, release->counterpoint only), exposed so
    // ComposerCore's rhythm-involvement pass (Rotation/Length via the
    // existing Mutation/Router/PolicyEngine pipeline - a deliberately
    // separate mechanism from this file's own step-content/CC20 writes) can
    // reuse the exact same eligibility rather than maintaining a second,
    // potentially-diverging copy of the same rule.
    std::vector<Instance> eligibleInstancesForArchetype(const std::string& archetype,
                                                          const std::vector<Instance>& allInstances);

    // Governs how a chosen motif actually gets written once the rule table
    // decides to apply one - global, session-level (see ComposerCore::
    // getMotifApplicationMode/setMotifApplicationMode), not per-preset or
    // per-section.
    enum class ApplicationMode
    {
        // Adjusts a handful of the instance's already-enabled steps
        // relative to what's already there (delta, not replacement) - the
        // same restraint the transpose/rotation/length/inversion mutations
        // have always had.
        Nudge,

        // Writes the full motif cell as a genuine short phrase into a
        // contiguous run of steps starting at the first enabled step,
        // replacing what was there - more expressive, a bigger claim on
        // authorship.
        Phrase
    };

    // Called from ComposerCore both when a section first becomes active
    // (passIndex 0) and again every kPassIntervalBars bars thereafter for
    // as long as the section stays active (passIndex 1, 2, 3, ...).
    // passIndex selects both the transform (transformForPass, cycling
    // every 4 passes) and which enabled step this pass starts writing
    // from, so consecutive passes visibly develop rather than repeat.
    // No-op if the section's archetype is empty or "presentation" (that
    // archetype's own seed content comes from stampMotifForSection instead
    // - see below), if no registered MotifPreset matches the archetype (by
    // tag, same convention as BlueprintGenerator's preset matching), or if
    // an eligible instance has no confirmed pattern content in the cache
    // yet (won't write blind - resync it first via the Awareness tab).
    // Updates the cache in place with what it wrote, so cross-instance
    // collision checks within the same call see each other's results.
    // lockedSteps (v1.2 Phase 2, docs/score_timeline_ui_concept.md): steps
    // hand-authored via Setup mode and marked protected - skipped entirely
    // by every write this function makes (both the per-step nudge/phrase
    // loop and toggleOneStep's rhythm-grid shifting), cache untouched for
    // those indices too so the cache never claims to hold a value that was
    // never actually sent.
    // avoidPresetId (Phase 3, 2026-08-22, composer_mastermind_design.md's
    // "New Ideas" #7): when more than one MotifPreset matches this
    // archetype's tag, findPresetForArchetype deprioritizes (not forbids)
    // whichever preset id this names - see stampMotifForSection's own
    // comment for where this actually gets decided; this call and
    // seedPhraseChainPatterns/restampPhraseChainSlot just need to agree with
    // that same choice for the section's whole lifetime, not re-roll
    // independently each call. Pass an empty string for "no preference."
    void applyForSection(const BlueprintSection& section,
                          const std::vector<Instance>& allInstances,
                          const std::vector<MotifPreset>& motifPresets,
                          PatternSyncServer& patternSync,
                          InstanceStateTracker& stateTracker,
                          const std::map<std::string, int>& homePatternPerInstance,
                          ApplicationMode mode,
                          int passIndex,
                          CCDispatcher& dispatcher,
                          const LockedStepLibrary& lockedSteps,
                          const std::string& avoidPresetId);

    // The "preparatory phase" (user's own framing, 2026-08-21): authors a
    // section's actual seed content once, when the section first becomes
    // active, rather than leaving applyForSection to forever nudge whatever
    // sparse steps happened to already exist in MPL. Writes the matched
    // MotifPreset's own base shape (baseTransformForArchetype - no rotate/
    // invert/retrograde cycling, that's what the *following* per-pass calls
    // are for) evenly spaced across each eligible instance's currently
    // tracked Length window, fully enabled, and every other step silenced -
    // sent as one atomic writeFullPattern message per instance
    // (PatternSyncServer), so there's no burst-pacing concern here at all
    // (see this header's own top comment). Unlike applyForSection,
    // "presentation" is a real archetype here, not a no-op - the opening
    // theme deserves the same deliberate authorship as every later section,
    // not whatever was last hand-programmed in MPL. Silently does nothing
    // for an instance/section with no matching preset (by tag, id-fallback)
    // or no confirmed cache entry yet - same "won't write blind" restraint
    // as applyForSection. Call this once at section entry INSTEAD OF
    // applyForSection's own passIndex-0 call (not in addition to it - the
    // stamp already establishes pass 0's shape; double-applying would
    // compound the relativeVelocity/relativeDuration scaling). Normal
    // per-pass development resumes from passIndex 1 onward via the usual
    // fireMotifPassIfDue cadence.
    // lockedSteps: any locked index in a touched pattern keeps its current
    // cached value in the stamp's own writeFullPattern message, splicing
    // over whatever the fresh shape would otherwise have placed there.
    // avoidPresetId: novelty-aware selection (Phase 3, 2026-08-22) - this is
    // the one call that actually DECIDES a section's preset (the other three
    // callers just need to agree with it, see applyForSection's own
    // comment); pass whatever the previous section's chosen preset id was,
    // empty string for "no preference." Returns the preset id this section
    // actually resolved to (empty if none matched at all), so the caller
    // (ComposerCore::stampMotifForSection) can remember it for the *next*
    // section's own avoidPresetId - deliberately deprioritizes rather than
    // forbids a repeat, since a genuine return to a preset can be its own
    // musical device, not always a mistake.
    std::string stampMotifForSection(const BlueprintSection& section,
                                      const std::vector<Instance>& allInstances,
                                      const std::vector<MotifPreset>& motifPresets,
                                      PatternSyncServer& patternSync,
                                      InstanceStateTracker& stateTracker,
                                      const std::map<std::string, int>& homePatternPerInstance,
                                      int currentBar,
                                      const LockedStepLibrary& lockedSteps,
                                      const std::string& avoidPresetId);

    // Phrase-chaining seed (2026-08-21, user's own idea): seeds ALL THREE of
    // an eligible instance's patterns at once with related transforms of the
    // section's matched preset - P1 the base shape, P2 rotated, P3 inverted
    // (the same vocabulary transformForPass already cycles pass to pass,
    // here distinguishing the three chained patterns instead of successive
    // passes on one). Real phrase material to move between via
    // ComposerCore's phrase-chain scheduler (which cycles Active Pattern
    // 1->2->3->1 on its own cadence), not just one pattern nudged forever.
    // Call once at section entry, in addition to stampMotifForSection (not
    // instead of - stampMotifForSection's own single-pattern stamp still
    // establishes whichever pattern the section starts on as its home).
    // Silently skips any pattern index with no confirmed cache entry yet
    // (same "won't write blind" restraint as everything else in this file) -
    // in practice this means P2/P3 need at least one Resync Now for that
    // specific pattern index before phrase-chaining does anything audible
    // for an instance that's never used them before.
    // lockedSteps: same protection as stampMotifForSection's, applied to
    // each of the three patterns this seeds. avoidPresetId: must be the same
    // choice stampMotifForSection just resolved for this section (see its
    // own comment) - passed through, not re-resolved independently.
    void seedPhraseChainPatterns(const BlueprintSection& section,
                                  const std::vector<Instance>& allInstances,
                                  const std::vector<MotifPreset>& motifPresets,
                                  PatternSyncServer& patternSync,
                                  InstanceStateTracker& stateTracker,
                                  const std::map<std::string, int>& homePatternPerInstance,
                                  int currentBar,
                                  const LockedStepLibrary& lockedSteps,
                                  const std::string& avoidPresetId);

    // Richer phrase-chain role vocabulary (v1.2 Track B extension,
    // 2026-08-22, docs/arc_dimension_mapping_concept.md's "richer role
    // vocabulary" note - inspired by a commercial JUCE plugin's Pattern
    // Playground, independently structurally identical to Complexity's own
    // phrase-target banding). Five named transforms built entirely from the
    // existing rotate/invert/retrograde vocabulary (transformForPass already
    // cycles the same primitives for single-pattern nudging) - richer than
    // MPL's 3 physical pattern slots, so ComposerCore's phrase-chain
    // scheduler re-stamps whichever slot is about to become active with the
    // current target role's shape (restampPhraseChainSlot) rather than
    // assuming a fixed P1=base/P2=rotated/P3=inverted forever.
    enum class PhraseRole { Base, Rotated, Inverted, Retrograde, InvertedRetrograde };

    // Complexity (0..1) -> PhraseRole, five even fifths.
    PhraseRole phraseRoleFromComplexity(float complexityValue);

    // For Activity Log messages.
    std::string phraseRoleName(PhraseRole role);

    // Re-stamps ONE pattern slot with the given role's shape - reuses the
    // same register-continuity/Length-window logic stampOnePattern already
    // applies internally, just parameterized by role instead of always
    // base/rotated/inverted. Silently does nothing if no preset matches the
    // archetype or no confirmed cache entry exists yet for this pattern
    // (same "won't write blind" restraint as everywhere else in this file).
    // avoidPresetId: same section-lifetime choice as stampMotifForSection's -
    // passed through, not re-resolved independently.
    void restampPhraseChainSlot(const Instance& instance, int patternIndex, const std::string& archetype,
                                 const std::vector<MotifPreset>& motifPresets, PhraseRole role,
                                 PatternSyncServer& patternSync, InstanceStateTracker& stateTracker, int currentBar,
                                 const LockedStepLibrary& lockedSteps, const std::string& avoidPresetId);
}
