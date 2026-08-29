#include "InstanceRegistry.h"
#include <algorithm>

bool InstanceRegistry::addInstance(const Instance& instance)
{
    std::lock_guard<std::mutex> lock(instancesMutex);

    for (auto& existing : instances)
    {
        if (existing.id == instance.id)
        {
            existing = instance;
            return true;
        }
    }

    // 2026-08-27: a MIDI channel can only ever reach one real physical MPL
    // instance - two registry entries claiming the same channel under
    // different ids is never a legitimate setup, just an accumulated
    // leftover (Load Score / a Bitwig preset recall both upsert-by-id
    // without ever clearing first, so an earlier piece's instances stick
    // around indefinitely - see docs/composer_mastermind_design.md's
    // instance-registry entry). The instance actually being registered now
    // is definitionally what the caller wants on that channel, so evict
    // whichever stale entry was squatting on it rather than let both
    // silently receive identical CC traffic.
    instances.erase(std::remove_if(instances.begin(), instances.end(),
                                    [&instance](const Instance& existing)
                                    { return existing.midiChannel == instance.midiChannel; }),
                     instances.end());

    instances.push_back(instance);
    return true;
}

bool InstanceRegistry::removeInstance(const std::string& instanceId)
{
    std::lock_guard<std::mutex> lock(instancesMutex);

    for (auto it = instances.begin(); it != instances.end(); ++it)
    {
        if (it->id == instanceId)
        {
            instances.erase(it);
            return true;
        }
    }

    return false;
}

bool InstanceRegistry::getInstanceById(const std::string& instanceId, Instance& outInstance) const
{
    std::lock_guard<std::mutex> lock(instancesMutex);

    for (const auto& instance : instances)
    {
        if (instance.id == instanceId)
        {
            outInstance = instance;
            return true;
        }
    }

    return false;
}

std::vector<Instance> InstanceRegistry::getInstancesByRole(const std::string& role) const
{
    std::lock_guard<std::mutex> lock(instancesMutex);

    std::vector<Instance> result;
    for (const auto& instance : instances)
    {
        if (instance.role == role)
            result.push_back(instance);
    }
    return result;
}

std::vector<Instance> InstanceRegistry::getEnabledInstances() const
{
    std::lock_guard<std::mutex> lock(instancesMutex);

    std::vector<Instance> result;
    for (const auto& instance : instances)
    {
        if (instance.enabled)
            result.push_back(instance);
    }
    return result;
}

std::vector<Instance> InstanceRegistry::getAllInstances() const
{
    std::lock_guard<std::mutex> lock(instancesMutex);
    return instances;
}

void InstanceRegistry::clear()
{
    std::lock_guard<std::mutex> lock(instancesMutex);
    instances.clear();
}
