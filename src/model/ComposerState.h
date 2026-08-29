#pragma once

// Runtime snapshot of what was actually last sent to one instance - not
// authored/intended state (that's Scene/Mutation), but "what CC value does
// this instance currently believe it has." Needed by the coherence
// evaluator (policy/CoherenceEvaluator.h), which measures how much
// instances currently agree with each other, not what a scene asked for.
struct InstancePatternState
{
    int transpose = 0;
    int rotation = 0;
    int length = 16;
    bool inversion = false;
    bool retrograde = false;
    bool m7 = false;
};

struct InstanceParameterState
{
    int activePattern = 1;
    int gridMode = 0;
    float swing = 0.0f;
    int rate = 1;  // 0=Augmented, 1=Normal (default), 2=Diminished - see CCMapping::kRate
    InstancePatternState patterns[3];
};
