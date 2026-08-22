#include "StateSnapshotStore.h"
#include "StateSerializer.h"
#include "../util/Validation.h"

namespace StateSnapshotStore
{
    juce::String createSnapshot(ComposerCore& core)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("schema", "ComposerMastermindSnapshot.v1");
        obj->setProperty("instances", StateSerializer::instancesToVar(core.getInstanceRegistry().getAllInstances()));
        obj->setProperty("sceneLibrary", StateSerializer::scenesToVar(core.getSceneLibrary().getAllScenes()));
        obj->setProperty("blueprintLibrary", StateSerializer::blueprintsToVar(core.getBlueprintLibrary().getAllBlueprints()));
        obj->setProperty("presetLibrary", StateSerializer::rolePresetsToVar(core.getPresetLibrary().getAllRolePresets()));
        obj->setProperty("rhythmicRelationshipPresetLibrary",
                          StateSerializer::rhythmicRelationshipPresetsToVar(core.getPresetLibrary().getAllRhythmicRelationshipPresets()));
        obj->setProperty("arcPresetLibrary", StateSerializer::arcPresetsToVar(core.getPresetLibrary().getAllArcPresets()));
        obj->setProperty("motifPresetLibrary", StateSerializer::motifPresetsToVar(core.getPresetLibrary().getAllMotifPresets()));
        obj->setProperty("modulatorTargetLibrary",
                          StateSerializer::modulatorTargetsToVar(core.getModulatorTargetLibrary().getAllTargets()));

        Scene currentScene;
        if (core.getCurrentScene(currentScene))
            obj->setProperty("currentSceneId", juce::String(currentScene.id));

        Blueprint currentBlueprint;
        if (core.getCurrentBlueprint(currentBlueprint))
            obj->setProperty("currentBlueprintId", juce::String(currentBlueprint.id));

        return juce::JSON::toString(juce::var(obj));
    }

    bool restoreSnapshot(const juce::String& json, ComposerCore& core, std::string& errorMessage)
    {
        if (json.isEmpty())
            return true;

        juce::var parsed;
        const auto parseResult = juce::JSON::parse(json, parsed);
        if (!parseResult.wasOk() || !parsed.isObject())
        {
            errorMessage = "Failed to parse snapshot JSON: " + parseResult.getErrorMessage().toStdString();
            return false;
        }

        auto& registry = core.getInstanceRegistry();
        for (const auto& instance : StateSerializer::varToInstances(parsed["instances"]))
        {
            std::string instanceError;
            if (Validation::isValidInstance(instance, instanceError))
                registry.addInstance(instance);
        }

        auto& library = core.getSceneLibrary();
        for (const auto& scene : StateSerializer::varToScenes(parsed["sceneLibrary"]))
        {
            std::string sceneError;
            if (Validation::isValidScene(scene, sceneError))
                library.addOrReplaceScene(scene);
        }

        const auto currentSceneId = parsed["currentSceneId"].toString().toStdString();
        if (!currentSceneId.empty())
        {
            Scene currentScene;
            if (library.getSceneById(currentSceneId, currentScene))
                core.setCurrentScene(currentScene);
        }

        auto& blueprintLibrary = core.getBlueprintLibrary();
        for (const auto& blueprint : StateSerializer::varToBlueprints(parsed["blueprintLibrary"]))
        {
            std::string blueprintError;
            if (Validation::isValidBlueprint(blueprint, blueprintError))
                blueprintLibrary.addOrReplaceBlueprint(blueprint);
        }

        const auto currentBlueprintId = parsed["currentBlueprintId"].toString().toStdString();
        if (!currentBlueprintId.empty())
        {
            Blueprint currentBlueprint;
            if (blueprintLibrary.getBlueprintById(currentBlueprintId, currentBlueprint))
                core.setCurrentBlueprint(currentBlueprint);
        }

        auto& presetLibrary = core.getPresetLibrary();
        for (const auto& preset : StateSerializer::varToRolePresets(parsed["presetLibrary"]))
        {
            std::string presetError;
            if (Validation::isValidRolePreset(preset, presetError))
                presetLibrary.addOrReplaceRolePreset(preset);
        }

        for (const auto& preset : StateSerializer::varToRhythmicRelationshipPresets(parsed["rhythmicRelationshipPresetLibrary"]))
        {
            std::string presetError;
            if (Validation::isValidRhythmicRelationshipPreset(preset, presetError))
                presetLibrary.addOrReplaceRhythmicRelationshipPreset(preset);
        }

        for (const auto& preset : StateSerializer::varToArcPresets(parsed["arcPresetLibrary"]))
        {
            std::string presetError;
            if (Validation::isValidArcPreset(preset, presetError))
                presetLibrary.addOrReplaceArcPreset(preset);
        }

        for (const auto& preset : StateSerializer::varToMotifPresets(parsed["motifPresetLibrary"]))
        {
            std::string presetError;
            if (Validation::isValidMotifPreset(preset, presetError))
                presetLibrary.addOrReplaceMotifPreset(preset);
        }

        auto& modulatorTargetLibrary = core.getModulatorTargetLibrary();
        for (const auto& target : StateSerializer::varToModulatorTargets(parsed["modulatorTargetLibrary"]))
        {
            std::string targetError;
            if (Validation::isValidModulatorTarget(target, targetError))
                modulatorTargetLibrary.addOrReplaceTarget(target);
        }

        return true;
    }
}
