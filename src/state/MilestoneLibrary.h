#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "../model/ComposerState.h"
#include "../model/PatternSnapshot.h"

// One instance's captured state at a milestone - both the tracked CC belief
// (InstanceParameterState) and whatever pattern content was actually in the
// cache at that moment (only the pattern indices that had a confirmed dump -
// same "won't restore blind" restraint as everywhere else this project
// avoids writing unconfirmed content).
struct MilestoneInstanceState
{
    std::string instanceId;
    InstanceParameterState trackerState;
    std::vector<PatternSnapshot> patterns;
};

// One playback milestone - bar, a human-readable label (matching the
// Activity Log's own wording for the same event), and every registered
// instance's state at that moment.
struct Milestone
{
    int bar = 0;
    std::string label;
    std::vector<MilestoneInstanceState> instances;
};

// Session-local, bounded ring buffer of playback milestones (Phase 3,
// composer_mastermind_design.md's "New Ideas" #6) - a quick "what did this
// sound like a few sections ago" safety net while composing, distinct from
// Undo/Redo (doesn't exist in this project), from state/StateSnapshotStore
// (only ever holds *current* state), and from the permanent Blueprint/Scene/
// Preset libraries (a deliberate authoring act, not a quick checkpoint).
// Captured only at section entry (ComposerCore::enterSection) - the single
// most meaningful "before I kept tweaking" checkpoint, deliberately coarser
// than every structural decision the Activity Log already logs as text (a
// full state snapshot, including pattern content, at every phrase-chain
// advance or rhythm mutation would be far too heavy for what this is meant
// to be - a handful of real checkpoints, not a replay log). Not persisted
// across project save/reload, matching Recordings-style tools this was
// inspired by (Stellarizer's Recordings atlas) being session-local too.
class MilestoneLibrary
{
public:
    // Drops the oldest entry once at capacity - bounded, not unbounded
    // growth over a long session.
    void capture(Milestone milestone);

    // Newest first, matching the Activity Log's own convention.
    std::vector<Milestone> getAll() const;

    bool getByIndex(size_t index, Milestone& outMilestone) const;

private:
    static constexpr size_t kMaxMilestones = 20;

    mutable std::mutex mutex_;
    std::vector<Milestone> milestones; // oldest at front, newest at back
};
