#pragma once

#include <vector>

// One step's actual content, as read back from MPL via the Composer Bridge
// SysEx protocol (Docs/ComposerBridgeProtocol.md in the MPL project) - not
// what Composer Mastermind last intended to write, but what MPL just said is
// really there. Mirrors that protocol's own step record shape exactly.
struct StepSnapshot
{
    bool enabled = false;
    int note = 0;
    int velocity = 0;
    int duration = 0;
};

// One pattern's full content, as read back via a Composer Bridge "Request
// Pattern Dump" (0x06 response). This is ground truth, deliberately kept
// separate from anything Composer Mastermind has itself sent - see
// state/InstancePatternCache.h.
struct PatternSnapshot
{
    int patternIndex = 0;
    std::vector<StepSnapshot> steps;
};
