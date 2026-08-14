#pragma once

#include <string>

struct Instance
{
    std::string id;
    std::string name;
    int midiChannel = 1;
    std::string role;
    bool enabled = true;
    std::string lastKnownScene;

    Instance() = default;

    Instance(const std::string& instanceId,
             const std::string& instanceName,
             int channel,
             const std::string& instanceRole = "")
        : id(instanceId),
          name(instanceName),
          midiChannel(channel),
          role(instanceRole)
    {
    }
};
