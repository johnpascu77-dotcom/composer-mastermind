#pragma once

#include <vector>
#include <string>
#include <mutex>
#include "../model/Instance.h"

// The instance list is edited from the UI thread (add/remove instance) and
// read while routing (audio thread, once scheduled routing lands), so all
// access goes through a mutex and returns copies rather than pointers into
// the internal vector.
class InstanceRegistry
{
public:
    // Upserts by id - replaces an existing instance with this id in place
    // (e.g. to change its role/channel) rather than rejecting the call,
    // matching every other library's addOrReplace* convention in this
    // codebase (SceneLibrary, PresetLibrary, BlueprintLibrary,
    // ModulatorTargetLibrary). Also enforces MIDI channel uniqueness
    // (2026-08-27): any other registered instance already on this
    // instance's midiChannel is evicted first - a channel can only ever
    // reach one real physical MPL instance, so a collision is always a
    // stale leftover, never a legitimate second occupant. Always returns
    // true.
    bool addInstance(const Instance& instance);
    bool removeInstance(const std::string& instanceId);

    bool getInstanceById(const std::string& instanceId, Instance& outInstance) const;

    std::vector<Instance> getInstancesByRole(const std::string& role) const;
    std::vector<Instance> getEnabledInstances() const;

    std::vector<Instance> getAllInstances() const;

    void clear();

private:
    std::vector<Instance> instances;
    mutable std::mutex instancesMutex;
};
