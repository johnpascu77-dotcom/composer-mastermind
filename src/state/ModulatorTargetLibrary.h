#pragma once

#include <vector>
#include <string>
#include <mutex>
#include "../model/ModulatorTarget.h"

// A named set of ModulatorTargets. Mirrors SceneLibrary's copy-returning,
// mutex-guarded style, JUCE-free for the same reason (unit testable without
// a plugin host, and read/written from the UI thread while ComposerCore's
// audio-thread bar tick also reads it to drive live CC).
class ModulatorTargetLibrary
{
public:
    bool addOrReplaceTarget(const ModulatorTarget& target);
    bool removeTarget(const std::string& targetId);

    bool getTargetById(const std::string& targetId, ModulatorTarget& outTarget) const;
    std::vector<ModulatorTarget> getAllTargets() const;

private:
    std::vector<ModulatorTarget> targets;
    mutable std::mutex targetsMutex;
};
