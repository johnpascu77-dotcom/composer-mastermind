#include "MotifEngine.h"
#include "../midi/CCMapping.h"
#include "MonophonicOverlap.h"
#include <algorithm>
#include <cmath>

namespace
{
    using namespace MotifEngine;

    // Which registered instances this archetype touches at all - mirrors
    // the roles BlueprintGenerator's own applyArchetypeRules already
    // singles out for the same archetype, so the motif engine's reach
    // matches whatever else that archetype is already doing to
    // layering/budgets.
    std::vector<Instance> eligibleInstances(const std::string& archetype, const std::vector<Instance>& allInstances)
    {
        std::vector<Instance> result;

        for (const auto& instance : allInstances)
        {
            if (archetype == "peak" || archetype == "presentation")
            {
                // Presentation touches everyone too - stampMotifForSection
                // is what actually uses this branch (applyForSection itself
                // still no-ops on "presentation" before ever reaching
                // eligibility), and the opening theme is stated by whichever
                // instances the section wants playing at all, not just a role
                // subset.
                result.push_back(instance);
            }
            else if (archetype == "build")
            {
                if (instance.role == "motif" || instance.role == "counterpoint")
                    result.push_back(instance);
            }
            else if (archetype == "release")
            {
                if (instance.role == "counterpoint")
                    result.push_back(instance);
            }
        }

        return result;
    }

    // Two real passes, not one interleaved scan - tag match must win over
    // every id-fallback match regardless of library order, or a preset that
    // merely happens to be named after an archetype (e.g. an old test preset
    // literally called "peak") can shadow a properly tagged one that appears
    // later in the list (confirmed live, 2026-08-21: this silently
    // substituted an unrelated leftover preset for Vers la flamme's own
    // flame_converge throughout Peak). Shared by applyForSection and
    // stampMotifForSection so both pick the identical preset for a section.
    // avoidPresetId (Phase 3, 2026-08-22, composer_mastermind_design.md's
    // "New Ideas" #7): among presets tagged for this archetype, prefers the
    // first one that ISN'T avoidPresetId - deprioritizes a repeat rather
    // than forbidding it, so if every match happens to be the avoided one
    // (including the common case of only one preset authored for this
    // archetype at all), it's still used rather than returning nothing.
    const MotifPreset* findPresetForArchetype(const std::vector<MotifPreset>& motifPresets,
                                                const std::string& archetype, const std::string& avoidPresetId)
    {
        const MotifPreset* firstMatch = nullptr;
        for (const auto& preset : motifPresets)
        {
            if (std::find(preset.tags.begin(), preset.tags.end(), archetype) == preset.tags.end())
                continue;

            if (firstMatch == nullptr)
                firstMatch = &preset;
            if (preset.id != avoidPresetId)
                return &preset;
        }

        if (firstMatch != nullptr)
            return firstMatch; // only match(es) were the one we're avoiding - use it anyway

        for (const auto& preset : motifPresets)
            if (preset.id == archetype)
                return &preset;

        return nullptr;
    }

    // The full transform vocabulary, each independent of the others -
    // pass-based sequencing (below) cycles through these across successive
    // passes within one section, rather than a section only ever getting
    // one fixed look.
    std::vector<MotifNote> retrograde(const std::vector<MotifNote>& notes)
    {
        return std::vector<MotifNote>(notes.rbegin(), notes.rend());
    }

    std::vector<MotifNote> invert(const std::vector<MotifNote>& notes)
    {
        auto result = notes;
        for (auto& note : result)
            note.semitoneOffset = -note.semitoneOffset;
        return result;
    }

    std::vector<MotifNote> rotate(const std::vector<MotifNote>& notes, int amount)
    {
        if (notes.empty())
            return notes;

        auto result = notes;
        const int n = static_cast<int>(result.size());
        const int wrapped = ((amount % n) + n) % n;
        std::rotate(result.begin(), result.begin() + wrapped, result.end());
        return result;
    }

    // Richer phrase-chain role vocabulary (v1.2 Track B extension,
    // 2026-08-22) - built entirely from the three primitives above, no new
    // transform logic. InvertedRetrograde is the "most transformed" state,
    // the natural 5th role once base/rotated/inverted/retrograde are already
    // spoken for individually.
    std::vector<MotifNote> shapeForPhraseRole(MotifEngine::PhraseRole role, const std::vector<MotifNote>& base)
    {
        switch (role)
        {
            case MotifEngine::PhraseRole::Base:               return base;
            case MotifEngine::PhraseRole::Rotated:             return rotate(base, 1);
            case MotifEngine::PhraseRole::Inverted:            return invert(base);
            case MotifEngine::PhraseRole::Retrograde:          return retrograde(base);
            case MotifEngine::PhraseRole::InvertedRetrograde:  return retrograde(invert(base));
        }
        return base;
    }

    // Deterministic per-archetype base transform - no randomness. Peak
    // stays plain on its own base: applying the *same* shape identically
    // across every touched instance is what makes them converge. Release's
    // base is retrograde, the "unwinding" gesture matching the thinning
    // texture BlueprintGenerator's own Release rules already apply. Build's
    // base is plain - a rising section reaching toward the peak, not yet
    // unwinding anything.
    std::vector<MotifNote> baseTransformForArchetype(const std::string& archetype, const std::vector<MotifNote>& notes)
    {
        if (archetype == "release")
            return retrograde(notes);

        return notes;
    }

    // Pass-based variation sequencing: each successive pass within the same
    // section applies one more step of the transform vocabulary on top of
    // the archetype's own base, cycling every 4 passes (plain -> rotate ->
    // invert -> retrograde -> repeat) so a long section keeps developing
    // instead of repeating its first pass's shape indefinitely. Convergent
    // archetypes (peak) still apply this identically across every touched
    // instance in the same pass - the "same shape" behind convergence is
    // now the current pass's shape, not always pass 0's.
    std::vector<MotifNote> transformForPass(const std::string& archetype, const std::vector<MotifNote>& notes,
                                             int passIndex)
    {
        auto result = baseTransformForArchetype(archetype, notes);

        switch (passIndex % 4)
        {
            case 1: result = rotate(result, 1); break;
            case 2: result = invert(result); break;
            case 3: result = retrograde(result); break;
            default: break; // 0 - base transform only
        }

        return result;
    }

    // Deterministic call-and-response: which of `count` touched instances
    // should be audibly playing THIS pass vs. resting - rotates who rests
    // via passIndex, the same cadence that already drives the transform/
    // step rotation, no randomness. Peak never rests anyone (convergence
    // is the point); Release only ever lets ONE instance play at a time
    // (a genuine rotating solo, matching its existing thinning-texture
    // identity); everything else with 2+ candidates rests exactly one (a
    // duo trading off, not a full silence).
    std::vector<bool> whoPlaysThisPass(const std::string& archetype, size_t count, int passIndex)
    {
        std::vector<bool> playing(count, true);
        if (count <= 1 || archetype == "peak")
            return playing;

        if (archetype == "release")
        {
            std::fill(playing.begin(), playing.end(), false);
            playing[static_cast<size_t>(passIndex) % count] = true;
            return playing;
        }

        playing[static_cast<size_t>(passIndex) % count] = false;
        return playing;
    }

    int wrapStep(int stepIndex)
    {
        const int n = CCMapping::kPatternSteps;
        return ((stepIndex % n) + n) % n;
    }

    int clampNote(int note)
    {
        return std::clamp(note, 0, 127);
    }

    // How far a single note is allowed to land from its pattern's own
    // current pitch center in one pass - a starting, tunable bound (one
    // octave). This alone only bounds one note's leap *within* a pass; it
    // does nothing to stop the center itself (see patternCenterNote/
    // boundedHomeCenter below) from drifting further pass after pass -
    // that's what boundedHomeCenter's own kMaxDriftFromHomeSemitones fixes.
    // Named for what it protects, not how it's computed.
    constexpr int kMaxDeviationFromCenterSemitones = 12;

    // The pattern's own current pitch center - average note across its
    // enabled steps, as of the top of this pass (computed once per
    // instance, before this pass's own writes touch anything - see the
    // call site). Deliberately not a fixed value captured once per section
    // and remembered across passes: recomputing it fresh each pass from
    // whatever the cache currently holds lets the *pattern's* center drift
    // slowly and organically over many passes (that's real melodic
    // development), while kMaxDeviationFromCenterSemitones still keeps any
    // *single* note from leaping far from its neighbors within one pass.
    int patternCenterNote(const PatternSnapshot& snapshot, const std::vector<int>& enabledStepIndices)
    {
        int sum = 0;
        for (int stepIndex : enabledStepIndices)
            sum += snapshot.steps[static_cast<size_t>(stepIndex)].note;

        return sum / static_cast<int>(enabledStepIndices.size());
    }

    // 2026-08-27 fix: patternCenterNote recomputes fresh from whatever the
    // cache currently holds every single pass (every stampOnePattern call -
    // section entry, phrase-chain seed/restamp, and every ongoing Nudge/
    // Phrase pass via applyForSection, roughly every kPassIntervalBars
    // bars). With nothing pulling it back toward anywhere, that's a genuine
    // unbounded random walk - confirmed live: a piece with a Tension curve
    // that stayed elevated for a long stretch (Tension's own register-
    // center pull is deliberately upward-only, stacking with this) walked
    // stored note content all the way to MIDI's hard 127 ceiling and stuck
    // there, in both curve-driven and plain Generative pieces. No per-
    // instance/role "natural register" concept exists yet to anchor a
    // *different* home per instance, so kHomeRegisterNote is one fixed
    // point (middle C) for everyone for now - every centerNote computation
    // should route through here instead of using patternCenterNote's
    // result directly, so "organic drift" stays bounded to a musically
    // sane range regardless of how many passes have compounded, rather
    // than only being stopped by the MIDI ceiling/floor.
    constexpr int kHomeRegisterNote = 60;
    constexpr int kMaxDriftFromHomeSemitones = 24; // 2 octaves either side of kHomeRegisterNote

    int boundedHomeCenter(int liveCenterNote)
    {
        return kHomeRegisterNote
               + std::clamp(liveCenterNote - kHomeRegisterNote, -kMaxDriftFromHomeSemitones, kMaxDriftFromHomeSemitones);
    }

    int clampToCenter(int note, int centerNote)
    {
        return std::clamp(note, centerNote - kMaxDeviationFromCenterSemitones,
                           centerNote + kMaxDeviationFromCenterSemitones);
    }

    int clampVelocity(int velocity)
    {
        return std::clamp(velocity, 1, 127);
    }

    int clampDuration(int duration)
    {
        return std::clamp(duration, 1, CCMapping::kPatternSteps);
    }

    // Writes one step directly into MPL's real pattern storage via the
    // PatternSyncServer IPC channel - atomic, no MIDI CC/APVTS-polling
    // involved (see MotifEngine.h's own top comment for why this replaced
    // the earlier CC 60-64 Target Step Editing path). Defense-in-depth lock
    // check (callers should already skip locked indices before reaching
    // here, so their own cache-update bookkeeping stays consistent with
    // what was actually sent) - never silently proceeds past this even if a
    // future call site forgets its own check.
    void writeStepDirect(const Instance& instance, int patternIndex, int stepIndex, int note, int velocity,
                          int duration, bool enabled, PatternSyncServer& patternSync,
                          const LockedStepLibrary& lockedSteps)
    {
        if (lockedSteps.isLocked(instance.id, patternIndex, stepIndex))
            return;

        patternSync.sendWriteStep(instance.midiChannel, patternIndex, stepIndex, enabled, note, velocity, duration);
    }

    // The pattern's on/off rhythm grid as a genuine variational device, not
    // a permanently-fixed backdrop: every pass, swaps which steps are
    // active - enables one currently-silent step, disables one currently-
    // active one - so the grid itself keeps shifting every pass, not just
    // the note content sitting on an unchanging grid (the "mechanical after
    // 3 loops" problem this is meant to fix directly). Deterministic by
    // passIndex, no randomness, matching the rest of this engine. Leaves at
    // least one step enabled always (won't silence an instance entirely -
    // that's CC20's job, in ComposerCore's dialogue, not this).
    // patternLength restricts candidates to the pattern's *current* tracked
    // Length (from InstanceStateTracker, which applyRhythmForSection's own
    // Length mutation keeps up to date) - not the raw 16-slot array. Found
    // necessary via live testing, not preventively: this function's own
    // "keep >= 1 enabled" guarantee was being satisfied by a step that had
    // fallen outside a since-shrunk Length, leaving the pattern's actual
    // audible window completely silent while the cache still believed
    // steps were enabled somewhere. Two independently-correct mechanisms
    // (this toggle, and rhythm's own Length mutation) interacting badly.
    void toggleOneStep(const Instance& instance, int patternIndex, int passIndex, int centerNote, int patternLength,
                        InstancePatternCache& cache, CachedPattern& cached, PatternSyncServer& patternSync,
                        const LockedStepLibrary& lockedSteps)
    {
        const size_t limit = static_cast<size_t>(std::clamp(patternLength, 1, CCMapping::kPatternSteps));

        std::vector<int> currentlyEnabled;
        std::vector<int> currentlyDisabled;
        for (size_t i = 0; i < limit && i < cached.snapshot.steps.size(); ++i)
        {
            if (cached.snapshot.steps[i].enabled)
                currentlyEnabled.push_back(static_cast<int>(i));
            else
                currentlyDisabled.push_back(static_cast<int>(i));
        }

        if (currentlyEnabled.empty())
        {
            if (currentlyDisabled.empty())
                return; // no steps in range at all - nothing to do

            // Recovery, not prevention: the audible window has already
            // gone silent (exactly the bug above) - force one step back on
            // within range rather than waiting for a "toggle" that never
            // fires because there's nothing left to toggle from.
            const int stepToRecover = currentlyDisabled[static_cast<size_t>(passIndex) % currentlyDisabled.size()];
            if (lockedSteps.isLocked(instance.id, patternIndex, stepToRecover))
                return; // the one candidate is protected - leave the window as-is this pass

            writeStepDirect(instance, patternIndex, stepToRecover, centerNote, 100, 1, true, patternSync, lockedSteps);

            auto recoveredSnapshot = cached.snapshot;
            recoveredSnapshot.steps[static_cast<size_t>(stepToRecover)] = StepSnapshot { true, centerNote, 100, 1 };
            cache.store(instance.id, recoveredSnapshot, cached.capturedAtBar);
            cached.snapshot = recoveredSnapshot;
            return;
        }

        if (currentlyDisabled.empty() || currentlyEnabled.size() <= 1)
            return; // nothing to enable, or already down to one step - don't silence it entirely

        const int stepToEnable = currentlyDisabled[static_cast<size_t>(passIndex) % currentlyDisabled.size()];
        const int stepToDisable = currentlyEnabled[static_cast<size_t>(passIndex / 2) % currentlyEnabled.size()];
        if (stepToEnable == stepToDisable)
            return; // degenerate coincidence - skip this pass rather than force a collision

        const bool enableLocked = lockedSteps.isLocked(instance.id, patternIndex, stepToEnable);
        const bool disableLocked = lockedSteps.isLocked(instance.id, patternIndex, stepToDisable);
        if (enableLocked && disableLocked)
            return; // both this pass's candidates are protected - skip the shift entirely

        auto updatedSnapshot = cached.snapshot;

        if (!enableLocked)
        {
            writeStepDirect(instance, patternIndex, stepToEnable, centerNote, 100, 1, true, patternSync, lockedSteps);
            updatedSnapshot.steps[static_cast<size_t>(stepToEnable)] = StepSnapshot { true, centerNote, 100, 1 };
        }

        if (!disableLocked)
        {
            writeStepDirect(instance, patternIndex, stepToDisable, 0, 1, 1, false, patternSync, lockedSteps);
            updatedSnapshot.steps[static_cast<size_t>(stepToDisable)].enabled = false;
        }

        cache.store(instance.id, updatedSnapshot, cached.capturedAtBar);
        cached.snapshot = updatedSnapshot;
    }

    // Avoids exact pitch-class collisions with notes other instances in
    // this same application pass already landed on - the one concrete use
    // of real cross-instance state this first build makes. Not called for
    // "peak", where matching pitch classes across voices is the point
    // (convergence), not a problem to avoid.
    int avoidCollision(int note, const std::vector<int>& takenPitchClasses)
    {
        int candidate = note;
        int attempts = 0;

        while (std::find(takenPitchClasses.begin(), takenPitchClasses.end(), candidate % 12) != takenPitchClasses.end()
               && attempts < 12)
        {
            candidate = clampNote(candidate + 1);
            ++attempts;
        }

        return candidate;
    }

    // Computes and atomically writes one pattern's full stamped content -
    // the shared core of stampMotifForSection (one pattern, the section's
    // "home") and seedPhraseChainPatterns (all three, one shape each).
    // Register continuity (stamp around the pattern's *own* existing pitch
    // center if it has real content already, middle C if genuinely empty)
    // and the Length-window spacing are computed fresh per pattern index -
    // each of an instance's three patterns can have its own tracked Length
    // and its own prior content, so there's no shared state to reuse across
    // calls. Silently does nothing if this pattern has no confirmed cache
    // entry yet (won't write blind, same restraint as everywhere else in
    // this file).
    void stampOnePattern(const Instance& instance, int patternIndex, const std::vector<MotifNote>& shapeNotes,
                          PatternSyncServer& patternSync, InstanceStateTracker& stateTracker, int currentBar,
                          const LockedStepLibrary& lockedSteps)
    {
        auto& cache = patternSync.getCache();

        CachedPattern cached;
        if (!cache.get(instance.id, patternIndex, cached))
            return; // no confirmed real state for this pattern yet - don't write blind

        InstanceParameterState trackedState;
        stateTracker.getState(instance.id, trackedState);
        const int trackedLength = trackedState.patterns[static_cast<size_t>(patternIndex)].length;
        const int effectiveSteps = CCMapping::effectiveStepCount(trackedState.gridMode);
        const size_t windowLimit = static_cast<size_t>(std::clamp(trackedLength, 1, effectiveSteps));

        std::vector<int> existingEnabled;
        for (size_t i = 0; i < cached.snapshot.steps.size(); ++i)
            if (cached.snapshot.steps[i].enabled)
                existingEnabled.push_back(static_cast<int>(i));
        const int centerNote = existingEnabled.empty() ? 60 : boundedHomeCenter(patternCenterNote(cached.snapshot, existingEnabled));

        const size_t noteCount = shapeNotes.size();
        const size_t stepCount = static_cast<size_t>(CCMapping::kPatternSteps);
        std::vector<StepSnapshot> freshSteps(stepCount, StepSnapshot {});

        // Two passes so MonophonicOverlap can see every note's position
        // before any of them claim more than a single step: pass 1 places
        // every note as a 1-step placeholder (its final note/velocity, but
        // not yet its real duration); pass 2 then grows each one's duration
        // only as far as the *next* note's still-unplaced-duration start,
        // never overlapping it. Building the real duration directly in pass
        // 1 (the old behavior) had no way to know where the *next* note
        // would land, so a long relativeDuration could freely run past it -
        // monophonic patches can't render that sensibly (see
        // MonophonicOverlap.h). Target indices are monotonically
        // non-decreasing in i (windowLimit*i/noteCount), so pass 2 always
        // compares an earlier note against later notes still at their
        // placeholder duration - exactly "clamp to the distance until the
        // next active step."
        std::vector<size_t> targetIndices(noteCount, stepCount);

        for (size_t i = 0; i < noteCount; ++i)
        {
            const auto& motifNote = shapeNotes[i];
            const size_t targetStepIndex = (windowLimit * i) / noteCount;
            if (targetStepIndex >= stepCount)
                continue;

            targetIndices[i] = targetStepIndex;
            const int newNote = clampToCenter(clampNote(centerNote + motifNote.semitoneOffset), centerNote);
            const int newVelocity = clampVelocity(static_cast<int>(std::lround(100.0 * motifNote.relativeVelocity)));

            freshSteps[targetStepIndex] = StepSnapshot { true, newNote, newVelocity, 1 };
        }

        for (size_t i = 0; i < noteCount; ++i)
        {
            if (targetIndices[i] >= stepCount)
                continue;

            const auto& motifNote = shapeNotes[i];
            const int targetStepIndex = static_cast<int>(targetIndices[i]);
            const int requestedDuration = clampDuration(static_cast<int>(std::lround(1.0 * motifNote.relativeDuration)));
            freshSteps[(size_t) targetStepIndex].duration = MonophonicOverlap::maxNonOverlappingDuration(
                freshSteps, targetStepIndex, requestedDuration, targetStepIndex, effectiveSteps);
        }

        // Locked steps (Setup mode) keep whatever the cache already had at
        // that index - splice over the fresh shape's own claim there, same
        // as every other automated write path in this file.
        for (int lockedStepIndex : lockedSteps.getLockedStepIndices(instance.id, patternIndex))
            if (lockedStepIndex >= 0 && (size_t) lockedStepIndex < freshSteps.size()
                && (size_t) lockedStepIndex < cached.snapshot.steps.size())
                freshSteps[(size_t) lockedStepIndex] = cached.snapshot.steps[(size_t) lockedStepIndex];

        // One atomic message writes the whole pattern - every step this
        // shape didn't claim comes through as disabled (freshSteps' default),
        // a clean authored restatement rather than a patchwork of whatever
        // survived from before (exactly the kind of out-of-window straggler
        // this session's earlier Length-window bug came from). No burst-
        // pacing concern here (see MotifEngine.h's own top comment) - one
        // writeFullPattern call, not sixteen separate CC bursts.
        patternSync.sendWriteFullPattern(instance.midiChannel, patternIndex, freshSteps);

        PatternSnapshot freshSnapshot;
        freshSnapshot.patternIndex = patternIndex;
        freshSnapshot.steps = freshSteps;
        cache.store(instance.id, freshSnapshot, currentBar);
    }
}

namespace MotifEngine
{
    std::vector<Instance> eligibleInstancesForArchetype(const std::string& archetype,
                                                          const std::vector<Instance>& allInstances)
    {
        return eligibleInstances(archetype, allInstances); // single source of truth - see the anonymous-namespace definition above
    }

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
                          const std::string& avoidPresetId)
    {
        if (section.archetype.empty() || section.archetype == "presentation")
            return;

        const MotifPreset* chosenPreset = findPresetForArchetype(motifPresets, section.archetype, avoidPresetId);

        if (chosenPreset == nullptr || chosenPreset->notes.empty())
            return; // nothing authored for this archetype yet - leave the section untouched, not an error

        auto& cache = patternSync.getCache();

        const auto transformedNotes = transformForPass(section.archetype, chosenPreset->notes, passIndex);
        const auto touchedInstances = eligibleInstances(section.archetype, allInstances);
        const bool convergent = (section.archetype == "peak");
        const auto playingFlags = whoPlaysThisPass(section.archetype, touchedInstances.size(), passIndex);

        std::vector<int> takenPitchClasses;

        for (size_t instanceIdx = 0; instanceIdx < touchedInstances.size(); ++instanceIdx)
        {
            const auto& instance = touchedInstances[instanceIdx];

            // The section's own intent for this instance - captured once
            // when the section became active (see MotifEngine.h's
            // call-and-response comment), not re-read live, since live
            // state now fluctuates as *this* engine toggles CC20 pass to
            // pass. 0 means the section wants this instance silent
            // throughout (e.g. a background layer role) - never revive it.
            const auto homeIt = homePatternPerInstance.find(instance.id);
            const int homePattern = (homeIt != homePatternPerInstance.end()) ? homeIt->second : 0;
            if (homePattern <= 0)
                continue;

            InstanceParameterState trackedState;
            stateTracker.getState(instance.id, trackedState);

            const bool shouldPlay = playingFlags[instanceIdx];

            if (!shouldPlay)
            {
                if (trackedState.activePattern != 0)
                {
                    dispatcher.sendCC(instance.midiChannel, CCMapping::kActivePattern, CCMapping::encodeActivePattern(0));
                    stateTracker.recordGlobal(instance.id, 0, trackedState.gridMode, trackedState.swing, trackedState.rate);
                }
                continue; // resting this pass - nothing to edit
            }

            if (trackedState.activePattern != homePattern)
            {
                dispatcher.sendCC(instance.midiChannel, CCMapping::kActivePattern,
                                   CCMapping::encodeActivePattern(homePattern));
                stateTracker.recordGlobal(instance.id, homePattern, trackedState.gridMode, trackedState.swing, trackedState.rate);
            }

            const int patternIndex = homePattern - 1;

            CachedPattern cached;
            if (!cache.get(instance.id, patternIndex, cached))
                continue; // no confirmed real state for this instance yet - don't write blind

            // Bound to the tracked Length window, same as toggleOneStep -
            // a step enabled outside the current window isn't real content
            // from the pattern's audible point of view (confirmed via live
            // testing: a straggler step left enabled outside a since-shrunk
            // Length gets treated as "the pattern's content" here otherwise,
            // so every pass keeps nudging that inaudible step in place and
            // the recovery branch below never sees an empty window to repair).
            const int trackedLength = trackedState.patterns[static_cast<size_t>(patternIndex)].length;
            const int effectiveSteps = CCMapping::effectiveStepCount(trackedState.gridMode);
            const size_t windowLimit = static_cast<size_t>(std::clamp(trackedLength, 1, effectiveSteps));

            std::vector<int> enabledStepIndices;
            for (size_t i = 0; i < windowLimit && i < cached.snapshot.steps.size(); ++i)
                if (cached.snapshot.steps[i].enabled)
                    enabledStepIndices.push_back(static_cast<int>(i));

            if (enabledStepIndices.empty())
            {
                // Fully silent within the audible window - nothing to nudge,
                // and Phrase mode still needs an anchor to scale from, but
                // this is exactly the state toggleOneStep's own recovery
                // branch exists to repair (confirmed via live testing: two
                // passes firing back to back can leave a pattern with zero
                // enabled steps anywhere, not just outside a shrunk Length).
                // No real content to average a center from, so fall back to
                // middle C rather than skip recovery entirely.
                toggleOneStep(instance, patternIndex, passIndex, 60, trackedLength, cache, cached, patternSync,
                               lockedSteps);
                continue;
            }

            const int centerNote = boundedHomeCenter(patternCenterNote(cached.snapshot, enabledStepIndices));

            // Rotate which enabled step this pass starts from, so successive
            // passes spread across the whole pattern instead of repeatedly
            // hammering the same first few steps - the other half of
            // pass-based variation, alongside transformForPass's changing
            // shape.
            const size_t startOffset = static_cast<size_t>(passIndex) % enabledStepIndices.size();
            const int phraseStartStep = enabledStepIndices[startOffset];
            const auto anchorStep = cached.snapshot.steps[static_cast<size_t>(phraseStartStep)];

            const size_t noteCount = (mode == ApplicationMode::Nudge)
                                          ? std::min(transformedNotes.size(), enabledStepIndices.size())
                                          : transformedNotes.size();

            for (size_t i = 0; i < noteCount; ++i)
            {
                const auto& motifNote = transformedNotes[i];

                int targetStepIndex;
                int baseNote;
                int baseVelocity;
                int baseDuration;

                if (mode == ApplicationMode::Nudge)
                {
                    targetStepIndex = enabledStepIndices[(startOffset + i) % enabledStepIndices.size()];
                    const auto& existing = cached.snapshot.steps[static_cast<size_t>(targetStepIndex)];
                    baseNote = existing.note;
                    baseVelocity = existing.velocity;
                    baseDuration = existing.duration;
                }
                else
                {
                    targetStepIndex = wrapStep(phraseStartStep + static_cast<int>(i));
                    baseNote = anchorStep.note;
                    baseVelocity = anchorStep.velocity;
                    baseDuration = anchorStep.duration;
                }

                if (lockedSteps.isLocked(instance.id, patternIndex, targetStepIndex))
                    continue; // hand-authored, protected - not this note's collision/write/cache-update either

                int newNote = clampNote(baseNote + motifNote.semitoneOffset);
                newNote = clampToCenter(newNote, centerNote);
                const int newVelocity =
                    clampVelocity(static_cast<int>(std::lround(baseVelocity * motifNote.relativeVelocity)));
                const int requestedDuration =
                    clampDuration(static_cast<int>(std::lround(baseDuration * motifNote.relativeDuration)));
                // cached.snapshot reflects every write this loop has already
                // made this pass (see the cache.store below), so this bounds
                // the new duration against both pre-existing content and
                // anything this same pass just wrote earlier - same
                // monophonic invariant Setup mode's manual edits already
                // enforce, see MonophonicOverlap.h.
                const int newDuration = MonophonicOverlap::maxNonOverlappingDuration(
                    cached.snapshot.steps, targetStepIndex, requestedDuration, targetStepIndex,
                    effectiveSteps);

                if (!convergent)
                {
                    newNote = avoidCollision(newNote, takenPitchClasses);
                    // avoidCollision can walk a note beyond the center bound
                    // while searching for a free pitch class - re-clamp
                    // rather than let a rare collision override the range
                    // guarantee.
                    newNote = clampToCenter(newNote, centerNote);
                }

                takenPitchClasses.push_back(newNote % 12);

                writeStepDirect(instance, patternIndex, targetStepIndex, newNote, newVelocity, newDuration, true,
                                 patternSync, lockedSteps);

                // Keep the cache self-consistent within this same pass (and
                // until a real resync happens) - later instances in this
                // loop should see this write when checking for collisions,
                // not stale pre-write content.
                auto updatedSnapshot = cached.snapshot;
                if (static_cast<size_t>(targetStepIndex) < updatedSnapshot.steps.size())
                {
                    auto& step = updatedSnapshot.steps[static_cast<size_t>(targetStepIndex)];
                    step.enabled = true;
                    step.note = newNote;
                    step.velocity = newVelocity;
                    step.duration = newDuration;
                }
                cache.store(instance.id, updatedSnapshot, cached.capturedAtBar);
                cached.snapshot = updatedSnapshot;
            }

            toggleOneStep(instance, patternIndex, passIndex, centerNote, trackedLength, cache, cached, patternSync,
                           lockedSteps);
        }
    }

    std::string stampMotifForSection(const BlueprintSection& section,
                                      const std::vector<Instance>& allInstances,
                                      const std::vector<MotifPreset>& motifPresets,
                                      PatternSyncServer& patternSync,
                                      InstanceStateTracker& stateTracker,
                                      const std::map<std::string, int>& homePatternPerInstance,
                                      int currentBar,
                                      const LockedStepLibrary& lockedSteps,
                                      const std::string& avoidPresetId)
    {
        if (section.archetype.empty())
            return {};

        const MotifPreset* chosenPreset = findPresetForArchetype(motifPresets, section.archetype, avoidPresetId);
        if (chosenPreset == nullptr || chosenPreset->notes.empty())
            return {}; // nothing authored for this archetype yet - leave whatever's already there untouched

        const auto shapeNotes = baseTransformForArchetype(section.archetype, chosenPreset->notes);
        const auto touchedInstances = eligibleInstances(section.archetype, allInstances);

        for (const auto& instance : touchedInstances)
        {
            const auto homeIt = homePatternPerInstance.find(instance.id);
            const int homePattern = (homeIt != homePatternPerInstance.end()) ? homeIt->second : 0;
            if (homePattern <= 0)
                continue; // section wants this instance silent throughout - nothing to author

            stampOnePattern(instance, homePattern - 1, shapeNotes, patternSync, stateTracker, currentBar,
                             lockedSteps);
        }

        return chosenPreset->id;
    }

    void seedPhraseChainPatterns(const BlueprintSection& section,
                                  const std::vector<Instance>& allInstances,
                                  const std::vector<MotifPreset>& motifPresets,
                                  PatternSyncServer& patternSync,
                                  InstanceStateTracker& stateTracker,
                                  const std::map<std::string, int>& homePatternPerInstance,
                                  int currentBar,
                                  const LockedStepLibrary& lockedSteps,
                                  const std::string& avoidPresetId)
    {
        if (section.archetype.empty())
            return;

        const MotifPreset* chosenPreset = findPresetForArchetype(motifPresets, section.archetype, avoidPresetId);
        if (chosenPreset == nullptr || chosenPreset->notes.empty())
            return;

        const auto base = baseTransformForArchetype(section.archetype, chosenPreset->notes);
        const std::vector<MotifNote> shapesByPatternIndex[3] = { base, rotate(base, 1), invert(base) };

        const auto touchedInstances = eligibleInstances(section.archetype, allInstances);

        for (const auto& instance : touchedInstances)
        {
            const auto homeIt = homePatternPerInstance.find(instance.id);
            if (homeIt == homePatternPerInstance.end() || homeIt->second <= 0)
                continue; // section wants this instance silent throughout - nothing to chain

            for (int patternIndex = 0; patternIndex < 3; ++patternIndex)
                stampOnePattern(instance, patternIndex, shapesByPatternIndex[static_cast<size_t>(patternIndex)],
                                 patternSync, stateTracker, currentBar, lockedSteps);
        }
    }

    PhraseRole phraseRoleFromComplexity(float complexityValue)
    {
        const float clamped = std::clamp(complexityValue, 0.0f, 1.0f);
        if (clamped < 0.2f)
            return PhraseRole::Base;
        if (clamped < 0.4f)
            return PhraseRole::Rotated;
        if (clamped < 0.6f)
            return PhraseRole::Inverted;
        if (clamped < 0.8f)
            return PhraseRole::Retrograde;
        return PhraseRole::InvertedRetrograde;
    }

    std::string phraseRoleName(PhraseRole role)
    {
        switch (role)
        {
            case PhraseRole::Base:              return "base";
            case PhraseRole::Rotated:            return "rotated";
            case PhraseRole::Inverted:           return "inverted";
            case PhraseRole::Retrograde:         return "retrograde";
            case PhraseRole::InvertedRetrograde: return "inverted+retrograde";
        }
        return "base";
    }

    void restampPhraseChainSlot(const Instance& instance, int patternIndex, const std::string& archetype,
                                 const std::vector<MotifPreset>& motifPresets, PhraseRole role,
                                 PatternSyncServer& patternSync, InstanceStateTracker& stateTracker, int currentBar,
                                 const LockedStepLibrary& lockedSteps, const std::string& avoidPresetId)
    {
        const MotifPreset* chosenPreset = findPresetForArchetype(motifPresets, archetype, avoidPresetId);
        if (chosenPreset == nullptr || chosenPreset->notes.empty())
            return;

        const auto base = baseTransformForArchetype(archetype, chosenPreset->notes);
        const auto shape = shapeForPhraseRole(role, base);

        stampOnePattern(instance, patternIndex, shape, patternSync, stateTracker, currentBar, lockedSteps);
    }

    int taperTransposeForPatternContent(PatternSyncServer& patternSync, const InstanceStateTracker& stateTracker,
                                         const Instance& instance, int patternIndex, int rawTranspose)
    {
        auto& cache = patternSync.getCache();

        CachedPattern cached;
        if (!cache.get(instance.id, patternIndex, cached))
            return rawTranspose; // no confirmed real state for this instance yet - don't taper blind

        InstanceParameterState trackedState;
        stateTracker.getState(instance.id, trackedState);

        // Same Length-window bounding as applyForSection's own centerNote
        // computation - a step enabled outside the current window isn't
        // real content from the pattern's audible point of view.
        const int trackedLength = trackedState.patterns[static_cast<size_t>(patternIndex)].length;
        const int effectiveSteps = CCMapping::effectiveStepCount(trackedState.gridMode);
        const size_t windowLimit = static_cast<size_t>(std::clamp(trackedLength, 1, effectiveSteps));

        std::vector<int> enabledStepIndices;
        for (size_t i = 0; i < windowLimit && i < cached.snapshot.steps.size(); ++i)
            if (cached.snapshot.steps[i].enabled)
                enabledStepIndices.push_back(static_cast<int>(i));

        if (enabledStepIndices.empty())
            return rawTranspose; // fully silent within the audible window - nothing to taper against

        const int patternCenter = boundedHomeCenter(patternCenterNote(cached.snapshot, enabledStepIndices));
        const int boundedCombinedTarget = boundedHomeCenter(patternCenter + rawTranspose);
        return boundedCombinedTarget - patternCenter;
    }
}
