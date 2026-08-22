#pragma once

#include <string>
#include <vector>
#include <mutex>

// One protected step - a hand-authored note (placed via ui/PatternSetupView's
// Setup mode) that automated engine writes must never touch until explicitly
// unlocked. Composer-Mastermind-side only; MPL has no concept of this and
// never sees it - enforcement happens entirely on this side, before a write
// ever reaches PatternSyncServer (see policy/MotifEngine.cpp's
// writeStepDirect/stampOnePattern and ComposerCore::resetPresentationBaseline,
// the only three places automated writes originate).
struct LockedStep
{
    std::string instanceId;
    int patternIndex = 0;
    int stepIndex = 0;
};

// Session-local (not yet persisted across project save/reload - a known gap,
// same "built the mechanism first, persistence later" sequencing v0.2 scene
// persistence itself followed). Mutex-guarded and JUCE-free, matching
// InstancePatternCache's own style - this is authoring metadata, not a
// second source of truth for step content itself.
class LockedStepLibrary
{
public:
    void setLocked(const std::string& instanceId, int patternIndex, int stepIndex, bool locked);
    bool isLocked(const std::string& instanceId, int patternIndex, int stepIndex) const;

    // All currently-locked steps for one instance/pattern - what Setup
    // mode's piano roll needs to know which cells to render as locked.
    std::vector<int> getLockedStepIndices(const std::string& instanceId, int patternIndex) const;

private:
    mutable std::mutex mutex_;
    std::vector<LockedStep> locked;
};
