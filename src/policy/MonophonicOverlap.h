#pragma once

#include <vector>
#include "../model/PatternSnapshot.h"

// Shared step-range overlap logic for MPL's monophonic instances (one voice
// per pattern - see docs/score_timeline_ui_concept.md's "why the overlay
// works cleanly" section). Extracted from ui/PianoRollView's own
// stepRangeOverlapsExisting/maxNonOverlappingDuration (built for Setup
// mode's hand-editing gestures, matching MPL's own melodyStepRangeOverlaps
// Existing/getMaxNonOverlappingMelodyDuration) so policy/MotifEngine's
// *automated* writes enforce the exact same invariant a human editing the
// same pattern by hand already gets - previously the two write paths
// diverged: a manual edit in Setup mode could never overlap an existing
// note, but MotifEngine's stamping/nudging only clamped duration to
// [1, kPatternSteps], with no awareness of where a neighboring step's
// occupied range actually ended (docs/technical_spec_checklist.md's
// "monophonic duration overlap" entry). Pure/JUCE-free (like the rest of
// policy/), even though its first caller here (PianoRollView) is a JUCE
// component - std:: only.
//
// Applied unconditionally, not gated behind a per-instance flag - MPL
// instances are monophonic by design, not configurably so (see the design
// doc reference above), so there's no "polyphonic-capable" case to spare.
namespace MonophonicOverlap
{
    // True if the candidate range [startStep, startStep+duration-1] (clamped
    // to totalSteps) overlaps any *other* enabled step's own occupied range
    // (that step's index to index+its own duration-1, also clamped).
    // ignoredStep excludes one index from the comparison - typically the
    // step being written/moved itself, so it never collides with its own
    // prior content.
    bool rangeOverlapsExisting(const std::vector<StepSnapshot>& steps, int startStep, int duration, int ignoredStep,
                                int totalSteps);

    // The largest duration <= requestedDuration (itself clamped to fit
    // within totalSteps from startStep) that doesn't overlap any other
    // enabled step, per rangeOverlapsExisting. Never returns less than 1 -
    // callers that need to refuse a still-conflicting edit outright (rather
    // than silently shrink it) should re-check rangeOverlapsExisting on the
    // result themselves, the same way ui/PianoRollView::applyNoteEditSafely
    // does.
    int maxNonOverlappingDuration(const std::vector<StepSnapshot>& steps, int startStep, int requestedDuration,
                                   int ignoredStep, int totalSteps);

    // Full-pattern normalization, distinct from the single-candidate-edit
    // functions above: trims every enabled step's duration so it never
    // extends past the *next* enabled step's start (or past the end of the
    // array for the last one), sequentially, left to right. Unlike
    // rangeOverlapsExisting/maxNonOverlappingDuration (used interactively,
    // one edit at a time, comparing a candidate against everything else
    // already there), this fixes up a whole pattern that may already
    // contain multiple overlaps at once - the case a single edit's
    // overlap-check was never meant to catch. Used by
    // state/InstancePatternCache::store so the monophonic invariant holds
    // for *any* incoming pattern data regardless of source (a pattern
    // hand-edited directly in MPL, one written by an older build before
    // this constraint existed, or any future write path) - a filter applied
    // at the one place all real pattern content passes through, not
    // something every writer has to individually remember. Steps already
    // disabled, or a duration that already fits, are left untouched. Does
    // not wrap the last note around to check against the first - matches
    // the pattern's own hard step-array boundary, same as
    // rangeOverlapsExisting.
    std::vector<StepSnapshot> normalizePattern(std::vector<StepSnapshot> steps);
}
