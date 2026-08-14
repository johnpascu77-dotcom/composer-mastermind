#include "StateSerializer.h"
#include "../util/JsonHelpers.h"

namespace StateSerializer
{
    juce::var instanceToVar(const Instance& instance)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("id", juce::String(instance.id));
        obj->setProperty("name", juce::String(instance.name));
        obj->setProperty("midiChannel", instance.midiChannel);
        obj->setProperty("role", juce::String(instance.role));
        obj->setProperty("enabled", instance.enabled);
        obj->setProperty("lastKnownScene", juce::String(instance.lastKnownScene));
        return juce::var(obj);
    }

    Instance varToInstance(const juce::var& value)
    {
        Instance instance;
        instance.id = JsonHelpers::getString(value, "id");
        instance.name = JsonHelpers::getString(value, "name");
        instance.midiChannel = JsonHelpers::getInt(value, "midiChannel", 1);
        instance.role = JsonHelpers::getString(value, "role");
        instance.enabled = JsonHelpers::getBool(value, "enabled", true);
        instance.lastKnownScene = JsonHelpers::getString(value, "lastKnownScene");
        return instance;
    }

    juce::var mutationToVar(const Mutation& mutation)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("type", juce::String(mutation.type));
        obj->setProperty("targetInstance", juce::String(mutation.targetInstance));
        obj->setProperty("patternIndex", mutation.patternIndex);
        obj->setProperty("amount", mutation.amount);
        obj->setProperty("applyAtBar", mutation.applyAtBar);
        obj->setProperty("strength", juce::String(mutation.strength));
        return juce::var(obj);
    }

    Mutation varToMutation(const juce::var& value)
    {
        Mutation mutation;
        mutation.type = JsonHelpers::getString(value, "type");
        mutation.targetInstance = JsonHelpers::getString(value, "targetInstance");
        mutation.patternIndex = JsonHelpers::getInt(value, "patternIndex", 0);
        mutation.amount = JsonHelpers::getInt(value, "amount", 0);
        mutation.applyAtBar = JsonHelpers::getInt(value, "applyAtBar", 0);
        mutation.strength = JsonHelpers::getString(value, "strength");
        return mutation;
    }

    juce::var scenePatternToVar(const ScenePattern& pattern)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("targetInstance", juce::String(pattern.targetInstance));
        obj->setProperty("patternIndex", pattern.patternIndex);
        obj->setProperty("transpose", pattern.transpose);
        obj->setProperty("rotation", pattern.rotation);
        obj->setProperty("length", pattern.length);
        obj->setProperty("inversion", pattern.inversion);
        return juce::var(obj);
    }

    ScenePattern varToScenePattern(const juce::var& value)
    {
        ScenePattern pattern;
        pattern.targetInstance = JsonHelpers::getString(value, "targetInstance");
        pattern.patternIndex = JsonHelpers::getInt(value, "patternIndex", 0);
        pattern.transpose = JsonHelpers::getInt(value, "transpose", 0);
        pattern.rotation = JsonHelpers::getInt(value, "rotation", 0);
        pattern.length = JsonHelpers::getInt(value, "length", 16);
        pattern.inversion = JsonHelpers::getBool(value, "inversion", false);
        return pattern;
    }

    juce::var sceneInstanceOverrideToVar(const SceneInstanceOverride& override)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("targetInstance", juce::String(override.targetInstance));
        obj->setProperty("activePattern", override.activePattern);
        obj->setProperty("gridMode", override.gridMode);
        obj->setProperty("swing", override.swing);
        return juce::var(obj);
    }

    SceneInstanceOverride varToSceneInstanceOverride(const juce::var& value)
    {
        SceneInstanceOverride override;
        override.targetInstance = JsonHelpers::getString(value, "targetInstance");
        override.activePattern = JsonHelpers::getInt(value, "activePattern", -1);
        override.gridMode = JsonHelpers::getInt(value, "gridMode", -1);
        override.swing = JsonHelpers::getFloat(value, "swing", -1.0f);
        return override;
    }

    juce::var sceneToVar(const Scene& scene)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("schema", "ComposerMastermindScene.v1");
        obj->setProperty("id", juce::String(scene.id));
        obj->setProperty("name", juce::String(scene.name));
        obj->setProperty("durationBars", scene.durationBars);
        obj->setProperty("quantize", juce::String(scene.quantize));

        juce::Array<juce::var> targets;
        for (const auto& target : scene.targets)
            targets.add(juce::String(target));
        obj->setProperty("targets", targets);

        auto* globalObj = new juce::DynamicObject();
        globalObj->setProperty("activePattern", scene.global.activePattern);
        globalObj->setProperty("gridMode", scene.global.gridMode);
        globalObj->setProperty("swing", scene.global.swing);
        obj->setProperty("global", juce::var(globalObj));

        juce::Array<juce::var> instanceOverrides;
        for (const auto& override : scene.instanceOverrides)
            instanceOverrides.add(sceneInstanceOverrideToVar(override));
        obj->setProperty("instanceOverrides", instanceOverrides);

        juce::Array<juce::var> patterns;
        for (const auto& pattern : scene.patterns)
            patterns.add(scenePatternToVar(pattern));
        obj->setProperty("patterns", patterns);

        juce::Array<juce::var> mutations;
        for (const auto& mutation : scene.mutations)
            mutations.add(mutationToVar(mutation));
        obj->setProperty("mutations", mutations);

        obj->setProperty("nextSceneId", juce::String(scene.nextSceneId));
        obj->setProperty("transitionStyle", juce::String(scene.transitionStyle));
        obj->setProperty("rampBars", scene.rampBars);

        return juce::var(obj);
    }

    Scene varToScene(const juce::var& value)
    {
        Scene scene;
        scene.id = JsonHelpers::getString(value, "id");
        scene.name = JsonHelpers::getString(value, "name");
        scene.durationBars = JsonHelpers::getInt(value, "durationBars", 4);
        scene.quantize = JsonHelpers::getString(value, "quantize", "bar");

        for (const auto& target : JsonHelpers::getArray(value, "targets"))
            scene.targets.push_back(target.toString().toStdString());

        const auto& globalValue = value["global"];
        scene.global.activePattern = JsonHelpers::getInt(globalValue, "activePattern", 1);
        scene.global.gridMode = JsonHelpers::getInt(globalValue, "gridMode", 0);
        scene.global.swing = JsonHelpers::getFloat(globalValue, "swing", 0.0f);

        for (const auto& item : JsonHelpers::getArray(value, "instanceOverrides"))
            scene.instanceOverrides.push_back(varToSceneInstanceOverride(item));

        for (const auto& item : JsonHelpers::getArray(value, "patterns"))
            scene.patterns.push_back(varToScenePattern(item));

        for (const auto& item : JsonHelpers::getArray(value, "mutations"))
            scene.mutations.push_back(varToMutation(item));

        scene.nextSceneId = JsonHelpers::getString(value, "nextSceneId");
        scene.transitionStyle = JsonHelpers::getString(value, "transitionStyle", "hard");
        scene.rampBars = JsonHelpers::getInt(value, "rampBars", 0);

        return scene;
    }

    juce::var instancesToVar(const std::vector<Instance>& instances)
    {
        juce::Array<juce::var> array;
        for (const auto& instance : instances)
            array.add(instanceToVar(instance));
        return juce::var(array);
    }

    std::vector<Instance> varToInstances(const juce::var& value)
    {
        std::vector<Instance> instances;
        if (auto* array = value.getArray())
            for (const auto& item : *array)
                instances.push_back(varToInstance(item));
        return instances;
    }

    juce::var scenesToVar(const std::vector<Scene>& scenes)
    {
        juce::Array<juce::var> array;
        for (const auto& scene : scenes)
            array.add(sceneToVar(scene));
        return juce::var(array);
    }

    std::vector<Scene> varToScenes(const juce::var& value)
    {
        std::vector<Scene> scenes;
        if (auto* array = value.getArray())
            for (const auto& item : *array)
                scenes.push_back(varToScene(item));
        return scenes;
    }
}
