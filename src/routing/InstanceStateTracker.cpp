#include "InstanceStateTracker.h"

InstanceStateTracker::Entry& InstanceStateTracker::findOrCreate(const std::string& instanceId)
{
    for (auto& entry : entries)
    {
        if (entry.instanceId == instanceId)
            return entry;
    }

    entries.push_back({ instanceId, InstanceParameterState{} });
    return entries.back();
}

void InstanceStateTracker::recordGlobal(const std::string& instanceId, int activePattern, int gridMode, float swing, int rate)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    auto& entry = findOrCreate(instanceId);
    entry.state.activePattern = activePattern;
    entry.state.gridMode = gridMode;
    entry.state.swing = swing;
    entry.state.rate = rate;
}

void InstanceStateTracker::recordPattern(const std::string& instanceId, int patternIndex,
                                          int transpose, int rotation, int length, bool inversion,
                                          bool retrograde, bool m7)
{
    if (patternIndex < 0 || patternIndex > 2)
        return;

    std::lock_guard<std::mutex> lock(stateMutex);
    auto& pattern = findOrCreate(instanceId).state.patterns[patternIndex];
    pattern.transpose = transpose;
    pattern.rotation = rotation;
    pattern.length = length;
    pattern.inversion = inversion;
    pattern.retrograde = retrograde;
    pattern.m7 = m7;
}

bool InstanceStateTracker::getState(const std::string& instanceId, InstanceParameterState& outState) const
{
    std::lock_guard<std::mutex> lock(stateMutex);

    for (const auto& entry : entries)
    {
        if (entry.instanceId == instanceId)
        {
            outState = entry.state;
            return true;
        }
    }

    return false;
}

std::vector<InstanceParameterState> InstanceStateTracker::getAllStates() const
{
    std::lock_guard<std::mutex> lock(stateMutex);

    std::vector<InstanceParameterState> result;
    result.reserve(entries.size());
    for (const auto& entry : entries)
        result.push_back(entry.state);

    return result;
}
