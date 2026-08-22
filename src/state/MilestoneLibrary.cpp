#include "MilestoneLibrary.h"
#include <algorithm>

void MilestoneLibrary::capture(Milestone milestone)
{
    std::lock_guard<std::mutex> lock(mutex_);

    milestones.push_back(std::move(milestone));
    if (milestones.size() > kMaxMilestones)
        milestones.erase(milestones.begin());
}

std::vector<Milestone> MilestoneLibrary::getAll() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Newest first - matches ActivityLogView's own convention, and index 0
    // is what a UI's top list row should mean.
    std::vector<Milestone> result(milestones.rbegin(), milestones.rend());
    return result;
}

bool MilestoneLibrary::getByIndex(size_t index, Milestone& outMilestone) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (index >= milestones.size())
        return false;

    // index 0 = newest, matching getAll()'s ordering.
    outMilestone = milestones[milestones.size() - 1 - index];
    return true;
}
