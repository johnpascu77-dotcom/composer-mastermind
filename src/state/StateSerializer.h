#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "../model/Instance.h"
#include "../model/Mutation.h"
#include "../model/Scene.h"

// Converts the model structs to/from juce::var, per docs/scene_format_v0_1.md.
// This is the JUCE boundary: model/, routing/, midi/, scheduling/ stay
// JUCE-free by design (unit testable without a plugin host) - only state/
// touches JUCE's JSON machinery. Missing fields fall back to the struct's
// own default via JsonHelpers, so an older/partial JSON file degrades
// gracefully instead of failing to parse.
namespace StateSerializer
{
    juce::var instanceToVar(const Instance& instance);
    Instance varToInstance(const juce::var& value);

    juce::var mutationToVar(const Mutation& mutation);
    Mutation varToMutation(const juce::var& value);

    juce::var scenePatternToVar(const ScenePattern& pattern);
    ScenePattern varToScenePattern(const juce::var& value);

    juce::var sceneInstanceOverrideToVar(const SceneInstanceOverride& override);
    SceneInstanceOverride varToSceneInstanceOverride(const juce::var& value);

    juce::var sceneToVar(const Scene& scene);
    Scene varToScene(const juce::var& value);

    juce::var instancesToVar(const std::vector<Instance>& instances);
    std::vector<Instance> varToInstances(const juce::var& value);

    juce::var scenesToVar(const std::vector<Scene>& scenes);
    std::vector<Scene> varToScenes(const juce::var& value);
}
