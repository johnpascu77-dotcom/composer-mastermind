#pragma once

#include <string>
#include <vector>
#include "Mutation.h"

struct Transition
{
    std::string fromSceneId;
    std::string toSceneId;
    std::string trigger;       // "bar_boundary", "manual", etc.
    std::string quantizeMode;   // "bar", "phrase", "immediate"
    int rampBars = 0;
    std::vector<Mutation> mutations;

    Transition() = default;
};
