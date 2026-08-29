#pragma once

#include <string>

struct Mutation
{
    std::string type;          // "transpose", "rotation", "length", "inversion", "retrograde", "m7", "rate"
    std::string targetInstance;
    int patternIndex = 0;      // ignored for "rate" - it's global per-instance, not per-pattern

    // A nudge relative to the target instance's last known state for this
    // pattern (per routing/InstanceStateTracker), not an absolute value -
    // e.g. rotation -2 means "2 steps back from wherever it currently is",
    // wrapping cyclically, not "set rotation to the literal value -2".
    // Exception: for types "inversion", "retrograde", "m7" (booleans, no
    // sensible delta), amount != 0 is an absolute on/off toggle instead.
    // Exception: for type "rate" (3 states - Augmented/Normal/Diminished),
    // amount is clamped to [-1, 1] and used as an absolute target state via
    // state = amount + 1 (-1=Augmented, 0=Normal, +1=Diminished), NOT a
    // delta - deliberately centered on 0=Normal so an omitted/default amount
    // stays inert rather than silently jumping to Augmented. Sign matches
    // musical intuition: negative = slower/longer, positive = faster/shorter.
    // See CCMapping::rateStateFromMutationAmount().
    int amount = 0;

    int applyAtBar = 0;
    std::string strength;      // "light", "medium", "strong"

    Mutation() = default;
};
