#include "ModulatorTargetLibrary.h"

bool ModulatorTargetLibrary::addOrReplaceTarget(const ModulatorTarget& target)
{
    if (target.id.empty())
        return false;

    std::lock_guard<std::mutex> lock(targetsMutex);

    for (auto& existing : targets)
    {
        if (existing.id == target.id)
        {
            existing = target;
            return true;
        }
    }

    targets.push_back(target);
    return true;
}

bool ModulatorTargetLibrary::removeTarget(const std::string& targetId)
{
    std::lock_guard<std::mutex> lock(targetsMutex);

    for (auto it = targets.begin(); it != targets.end(); ++it)
    {
        if (it->id == targetId)
        {
            targets.erase(it);
            return true;
        }
    }

    return false;
}

bool ModulatorTargetLibrary::getTargetById(const std::string& targetId, ModulatorTarget& outTarget) const
{
    std::lock_guard<std::mutex> lock(targetsMutex);

    for (const auto& target : targets)
    {
        if (target.id == targetId)
        {
            outTarget = target;
            return true;
        }
    }

    return false;
}

std::vector<ModulatorTarget> ModulatorTargetLibrary::getAllTargets() const
{
    std::lock_guard<std::mutex> lock(targetsMutex);
    return targets;
}
