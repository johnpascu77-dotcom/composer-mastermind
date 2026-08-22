#include "LockedStepLibrary.h"
#include <algorithm>

void LockedStepLibrary::setLocked(const std::string& instanceId, int patternIndex, int stepIndex, bool shouldLock)
{
    std::lock_guard<std::mutex> lock(mutex_);

    const auto matches = [&](const LockedStep& entry)
    {
        return entry.instanceId == instanceId && entry.patternIndex == patternIndex && entry.stepIndex == stepIndex;
    };

    if (shouldLock)
    {
        if (std::none_of(locked.begin(), locked.end(), matches))
            locked.push_back({ instanceId, patternIndex, stepIndex });
        return;
    }

    locked.erase(std::remove_if(locked.begin(), locked.end(), matches), locked.end());
}

bool LockedStepLibrary::isLocked(const std::string& instanceId, int patternIndex, int stepIndex) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return std::any_of(locked.begin(), locked.end(), [&](const LockedStep& entry)
    {
        return entry.instanceId == instanceId && entry.patternIndex == patternIndex && entry.stepIndex == stepIndex;
    });
}

std::vector<int> LockedStepLibrary::getLockedStepIndices(const std::string& instanceId, int patternIndex) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<int> result;
    for (const auto& entry : locked)
        if (entry.instanceId == instanceId && entry.patternIndex == patternIndex)
            result.push_back(entry.stepIndex);

    return result;
}
