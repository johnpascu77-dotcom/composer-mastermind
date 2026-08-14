#pragma once

#include <string>

struct ScheduledEvent
{
    std::string type;   // "scene", "mutation", "refresh"
    int targetBar = 0;
    int priority = 0;
};
