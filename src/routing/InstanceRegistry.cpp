#include "InstanceRegistry.h"

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
