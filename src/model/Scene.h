#pragma once

#include <string>
#include <vector>
#include "Mutation.h"

struct ScenePattern
{
    std::string targetInstance;
    int patternIndex = 0;
    int transpose = 0;
    int rotation = 0;
    int length = 16;
    bool inversion = false;
};

struct SceneGlobal
{
    int activePattern = 1;
    int gridMode = 0;
    float swing = 0.0f;
};

// Per-instance override of the instance-level parameters (Active Pattern,
// Grid Mode, Swing) that SceneGlobal would otherwise send identically to
// every target. "Send the same value to everyone" is the default, not the
// only option: a scene can hold e.g. one instance in binary while another
// runs ternary at the same time, for intentional poly/iso-rhythmic
// relationships between instances rather than uniform behaviour.
// A field value of -1 (or -1.0f for swing) means "inherit SceneGlobal".
struct SceneInstanceOverride
{
    std::string targetInstance;
    int activePattern = -1;
    int gridMode = -1;
    float swing = -1.0f;
};

struct Scene
{
    std::string id;
    std::string name;
    int durationBars = 4;
    std::string quantize = "bar";
    std::vector<std::string> targets;
    SceneGlobal global;
    std::vector<SceneInstanceOverride> instanceOverrides;
    std::vector<ScenePattern> patterns;
    std::vector<Mutation> mutations;
    std::string nextSceneId;
    std::string transitionStyle = "hard";
    int rampBars = 0;
};
