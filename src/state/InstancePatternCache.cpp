#include "InstancePatternCache.h"
#include "../policy/MonophonicOverlap.h"

void InstancePatternCache::store(const std::string& instanceId, const PatternSnapshot& snapshot, int capturedAtBar)
{
    PatternSnapshot normalized = snapshot;
    normalized.steps = MonophonicOverlap::normalizePattern(std::move(normalized.steps));

    std::lock_guard<std::mutex> lock(cacheMutex);

    for (auto& existing : cached)
    {
        if (existing.instanceId == instanceId && existing.snapshot.patternIndex == normalized.patternIndex)
        {
            existing.snapshot = normalized;
            existing.capturedAtBar = capturedAtBar;
            return;
        }
    }

    CachedPattern entry;
    entry.instanceId = instanceId;
    entry.snapshot = normalized;
    entry.capturedAtBar = capturedAtBar;
    cached.push_back(entry);
}

bool InstancePatternCache::get(const std::string& instanceId, int patternIndex, CachedPattern& outCached) const
{
    std::lock_guard<std::mutex> lock(cacheMutex);

    for (const auto& entry : cached)
    {
        if (entry.instanceId == instanceId && entry.snapshot.patternIndex == patternIndex)
        {
            outCached = entry;
            return true;
        }
    }

    return false;
}

std::vector<CachedPattern> InstancePatternCache::getAll() const
{
    std::lock_guard<std::mutex> lock(cacheMutex);
    return cached;
}
