#pragma once

#include <algorithm>
#include <cmath>
#include "../midi/CCMapping.h"

// Pure, stateless, tempo-invariant loop-cycle math - lifted from ui/PrimaryView.cpp's
// proven Live Sync playhead reconstruction (computeLivePlayheadFraction, and the
// stepsSinceLaunch/floor/mod catch-up loop in refreshFromCore()), which itself mirrors
// MPL's own step clock exactly (see the sibling project's PluginProcessor.cpp).
// PrimaryView.cpp's own copy is left untouched (UI-thread, GUI-lifetime-scoped state) -
// this header exists so audio-thread code (ComposerCore::processStepTick) can use the
// same formulas without depending on that UI component. Unifying the two into one
// shared implementation is a reasonable future cleanup, not required for this to work.
//
// Deliberately ppq-only, no tempo/time-signature reads - a 4/4 bar is always
// kBarLengthInPpq (4.0) quarter notes regardless of BPM, and MPL itself makes the same
// 4/4 assumption (see PrimaryView.cpp's own comment on this), so this stays
// tempo-change-safe for free the same way PluginProcessor.cpp's bar-boundary detection
// already is via getPpqPositionOfLastBarStart().
namespace StepClock
{
    constexpr double kBarLengthInPpq = 4.0;

    inline int gridStepCount(bool ternaryGridMode)
    {
        return ternaryGridMode ? 12 : 16;
    }

    // v1.29.0: rateState (0=Augmented/1=Normal/2=Diminished, defaults to
    // Normal) mirrors MPL's own real engine change - Rate divides MPL's
    // actual ppq-per-step value (see PluginProcessor.cpp's
    // effectiveGridStepLengthInPpq), so this reconstruction must apply the
    // same divisor or it silently desyncs from real playback the moment an
    // instance's Rate != Normal - see CCMapping::rateMultiplierForState.
    inline double gridStepLengthInPpq(bool ternaryGridMode, int rateState = 1)
    {
        return kBarLengthInPpq / static_cast<double>(gridStepCount(ternaryGridMode))
             / CCMapping::rateMultiplierForState(rateState);
    }

    // How many full loop cycles of `loopLength` steps have elapsed between anchorPpq
    // (the ppq at which cycle counting started - e.g. the active section's own first
    // bar) and currentPpq. Never negative - currentPpq before anchorPpq (shouldn't
    // happen in practice, since the anchor is captured at or before any subsequent
    // tick) clamps to 0 rather than returning a negative cycle index.
    inline int loopCycleIndex(double anchorPpq, double currentPpq, bool ternaryGridMode, int loopLength,
                               int rateState = 1)
    {
        const int clampedLoopLength = std::max(1, loopLength);
        const double stepsSinceAnchor = (currentPpq - anchorPpq) / gridStepLengthInPpq(ternaryGridMode, rateState);
        const int cyclesElapsed = static_cast<int>(std::floor(stepsSinceAnchor / static_cast<double>(clampedLoopLength)));
        return std::max(0, cyclesElapsed);
    }
}
