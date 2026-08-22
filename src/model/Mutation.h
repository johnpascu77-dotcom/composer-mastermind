#pragma once

#include <string>

struct Mutation
{
    std::string type;          // "transpose", "rotation", "length", "inversion"
    std::string targetInstance;
    int patternIndex = 0;

    // A nudge relative to the target instance's last known state for this
    // pattern (per routing/InstanceStateTracker), not an absolute value -
    // e.g. rotation -2 means "2 steps back from wherever it currently is",
    // wrapping cyclically, not "set rotation to the literal value -2".
    // Exception: for type "inversion" (a boolean, no sensible delta),
    // amount != 0 is an absolute on/off toggle instead.
    int amount = 0;

    int applyAtBar = 0;
    std::string strength;      // "light", "medium", "strong"

    Mutation() = default;
};
