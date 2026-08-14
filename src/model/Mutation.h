#pragma once

#include <string>

struct Mutation
{
    std::string type;          // "transpose", "rotation", "length", "inversion"
    std::string targetInstance;
    int patternIndex = 0;
    int amount = 0;
    int applyAtBar = 0;
    std::string strength;      // "light", "medium", "strong"

    Mutation() = default;
};
