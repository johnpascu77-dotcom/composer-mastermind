#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "../model/PatternSnapshot.h"

// One instance's pattern, as last confirmed via a Composer Bridge dump
// response, plus the bar it was captured at (for staleness display - see
// ui/PatternAwarenessView).
struct CachedPattern
{
    std::string instanceId;
    PatternSnapshot snapshot;
    int capturedAtBar = -1;
};

// Ground truth per (instance, pattern) - populated *only* from Composer
// Bridge dump responses (state/PatternSyncCoordinator), never from anything
// Composer Mastermind itself has sent. Deliberately kept separate from
// routing/InstanceStateTracker, which tracks intended/sent CC state - one is
// "what we believe we told it," this is "what MPL just told us is actually
// there." That separation is what makes hand-editing in MPL's own UI safe:
// the next resync overwrites the cache with reality, not with a stale
// assumption. Mirrors SceneLibrary's copy-returning, mutex-guarded style.
//
// One deliberate exception to "never modified": store() runs every incoming
// snapshot through policy/MonophonicOverlap::normalizePattern first, so an
// overlapping-duration pattern - however it got that way (hand-edited
// directly in MPL, written by an older build before this constraint
// existed, or any future write path) - never survives into the cache that
// every consumer (the piano roll, MotifEngine's own reasoning) reads from.
// This is the single choke point all real pattern content passes through,
// so it's the one place this invariant can be guaranteed regardless of
// source, rather than something every writer has to individually remember
// (see docs/technical_spec_checklist.md's "monophonic duration overlap"
// entry for why relying on writers alone wasn't enough).
class InstancePatternCache
{
public:
    void store(const std::string& instanceId, const PatternSnapshot& snapshot, int capturedAtBar);

    bool get(const std::string& instanceId, int patternIndex, CachedPattern& outCached) const;

    std::vector<CachedPattern> getAll() const;

private:
    std::vector<CachedPattern> cached;
    mutable std::mutex cacheMutex;
};
