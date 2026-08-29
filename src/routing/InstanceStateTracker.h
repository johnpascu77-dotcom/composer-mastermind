#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "../model/ComposerState.h"

// Records the last CC value actually sent to each instance for each
// tracked parameter. Updated by Router right after a successful send, read
// by policy/CoherenceEvaluator to measure how much instances currently
// agree with each other. Mutex-guarded and JUCE-free, matching
// InstanceRegistry's style - this is routing-adjacent bookkeeping, not a
// second source of truth (Scene/Mutation remain the authored intent).
class InstanceStateTracker
{
public:
    void recordGlobal(const std::string& instanceId, int activePattern, int gridMode, float swing, int rate);
    void recordPattern(const std::string& instanceId, int patternIndex,
                        int transpose, int rotation, int length, bool inversion,
                        bool retrograde, bool m7);

    bool getState(const std::string& instanceId, InstanceParameterState& outState) const;

    // One entry per instance that has ever had state recorded.
    std::vector<InstanceParameterState> getAllStates() const;

private:
    struct Entry
    {
        std::string instanceId;
        InstanceParameterState state;
    };

    mutable std::mutex stateMutex;
    std::vector<Entry> entries;

    Entry& findOrCreate(const std::string& instanceId);
};
