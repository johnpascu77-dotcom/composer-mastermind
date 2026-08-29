#include "CompositionBundleStore.h"
#include "StateSerializer.h"
#include "../util/Validation.h"
#include <algorithm>

namespace CompositionBundleStore
{
    juce::String createBundle(ComposerCore& core, const std::string& blueprintId, std::string& errorMessage)
    {
        Blueprint blueprint;
        if (!core.getBlueprintLibrary().getBlueprintById(blueprintId, blueprint))
        {
            errorMessage = "blueprint '" + blueprintId + "' not found";
            return {};
        }

        std::vector<std::string> referencedSceneIds;
        for (const auto& section : blueprint.sections)
        {
            if (section.sceneId.empty())
                continue;
            if (std::find(referencedSceneIds.begin(), referencedSceneIds.end(), section.sceneId)
                == referencedSceneIds.end())
                referencedSceneIds.push_back(section.sceneId);
        }

        std::vector<Scene> referencedScenes;
        for (const auto& sceneId : referencedSceneIds)
        {
            Scene scene;
            if (!core.getSceneLibrary().getSceneById(sceneId, scene))
            {
                errorMessage = "blueprint '" + blueprintId + "' references scene '" + sceneId
                                + "' which is no longer in the library";
                return {};
            }
            referencedScenes.push_back(scene);
        }

        // Every currently-registered instance, not a computed "referenced"
        // subset - role-based archetype eligibility (build/peak/release
        // select instances by role, not by anything named in the Scene/
        // Blueprint JSON) makes a narrower subset genuinely unsafe, not
        // just conservative. See this file's header comment.
        const std::vector<Instance> instances = core.getInstanceRegistry().getAllInstances();

        // Every distinct non-empty archetype this blueprint's sections
        // actually use, then every motif preset tagged with any of them -
        // the whole matching pool per archetype, not just whichever one
        // findPresetForArchetype would resolve to right now, so novelty-
        // aware cycling between multiple same-tagged presets still works
        // after import.
        std::vector<std::string> usedArchetypes;
        for (const auto& section : blueprint.sections)
        {
            if (section.archetype.empty())
                continue;
            if (std::find(usedArchetypes.begin(), usedArchetypes.end(), section.archetype)
                == usedArchetypes.end())
                usedArchetypes.push_back(section.archetype);
        }

        std::vector<MotifPreset> motifPresets;
        for (const auto& preset : core.getPresetLibrary().getAllMotifPresets())
        {
            const bool matchesUsedArchetype = std::any_of(preset.tags.begin(), preset.tags.end(),
                [&](const std::string& tag)
                {
                    return std::find(usedArchetypes.begin(), usedArchetypes.end(), tag) != usedArchetypes.end();
                });

            if (matchesUsedArchetype)
                motifPresets.push_back(preset);
        }

        // Every modulator target a section actually sends a value to,
        // deduped, skipped if no longer registered - the same tolerance
        // ComposerCore::sendSectionModulatorValues already has at playback
        // time, so a stale reference doesn't block export.
        std::vector<std::string> referencedModulatorTargetIds;
        for (const auto& section : blueprint.sections)
        {
            for (const auto& modulatorValue : section.modulatorValues)
            {
                if (std::find(referencedModulatorTargetIds.begin(), referencedModulatorTargetIds.end(),
                        modulatorValue.modulatorTargetId)
                    == referencedModulatorTargetIds.end())
                    referencedModulatorTargetIds.push_back(modulatorValue.modulatorTargetId);
            }
        }

        std::vector<ModulatorTarget> modulatorTargets;
        for (const auto& targetId : referencedModulatorTargetIds)
        {
            ModulatorTarget target;
            if (core.getModulatorTargetLibrary().getTargetById(targetId, target))
                modulatorTargets.push_back(target);
        }

        // Every ModulationRoute (v3) whose target is either every instance
        // ("*") or one of the instances already gathered above - unlike
        // ModulatorTarget, a route isn't referenced indirectly through a
        // section's own fields, so inclusion is instance-membership-based
        // rather than section-reference-based.
        std::vector<ModulationRoute> modulationRoutes;
        for (const auto& route : core.getModulationRouteLibrary().getAllRoutes())
        {
            const bool targetsIncludedInstance = route.targetInstance == "*"
                || std::any_of(instances.begin(), instances.end(), [&](const Instance& instance)
                   {
                       return instance.id == route.targetInstance;
                   });

            if (targetsIncludedInstance)
                modulationRoutes.push_back(route);
        }

        auto* obj = new juce::DynamicObject();
        obj->setProperty("schema", "ComposerMastermindComposition.v3");
        obj->setProperty("blueprint", StateSerializer::blueprintToVar(blueprint));
        obj->setProperty("scenes", StateSerializer::scenesToVar(referencedScenes));
        obj->setProperty("instances", StateSerializer::instancesToVar(instances));
        obj->setProperty("motifPresets", StateSerializer::motifPresetsToVar(motifPresets));
        obj->setProperty("modulatorTargets", StateSerializer::modulatorTargetsToVar(modulatorTargets));
        obj->setProperty("modulationRoutes", StateSerializer::modulationRoutesToVar(modulationRoutes));
        return juce::JSON::toString(juce::var(obj));
    }

    bool importBundle(const juce::String& json, ComposerCore& core, std::string& errorMessage)
    {
        juce::var parsed;
        const auto parseResult = juce::JSON::parse(json, parsed);
        if (!parseResult.wasOk() || !parsed.isObject())
        {
            errorMessage = "failed to parse composition JSON: " + parseResult.getErrorMessage().toStdString();
            return false;
        }

        const Blueprint blueprint = StateSerializer::varToBlueprint(parsed["blueprint"]);
        std::string blueprintError;
        if (!Validation::isValidBlueprint(blueprint, blueprintError))
        {
            errorMessage = "blueprint invalid: " + blueprintError;
            return false;
        }

        const std::vector<Scene> scenes = StateSerializer::varToScenes(parsed["scenes"]);
        for (const auto& scene : scenes)
        {
            std::string sceneError;
            if (!Validation::isValidScene(scene, sceneError))
            {
                errorMessage = "scene '" + scene.id + "' invalid: " + sceneError;
                return false;
            }
        }

        for (const auto& section : blueprint.sections)
        {
            if (section.sceneId.empty())
                continue;

            const bool included = std::any_of(scenes.begin(), scenes.end(), [&](const Scene& scene)
            {
                return scene.id == section.sceneId;
            });

            if (!included)
            {
                errorMessage = "blueprint section '" + section.id + "' references scene '" + section.sceneId
                                + "' which isn't included in this file";
                return false;
            }
        }

        // v2/v3: instances/motifPresets/modulatorTargets/modulationRoutes are
        // all optional keys - an older file simply has none, and varTo*
        // already defaults a missing key to an empty array, so this stays
        // backward compatible without any schema-string branching.
        const std::vector<Instance> instances = StateSerializer::varToInstances(parsed["instances"]);
        for (const auto& instance : instances)
        {
            std::string instanceError;
            if (!Validation::isValidInstance(instance, instanceError))
            {
                errorMessage = "instance '" + instance.id + "' invalid: " + instanceError;
                return false;
            }
        }

        const std::vector<MotifPreset> motifPresets = StateSerializer::varToMotifPresets(parsed["motifPresets"]);
        for (const auto& preset : motifPresets)
        {
            std::string presetError;
            if (!Validation::isValidMotifPreset(preset, presetError))
            {
                errorMessage = "motif preset '" + preset.id + "' invalid: " + presetError;
                return false;
            }
        }

        const std::vector<ModulatorTarget> modulatorTargets =
            StateSerializer::varToModulatorTargets(parsed["modulatorTargets"]);
        for (const auto& target : modulatorTargets)
        {
            std::string targetError;
            if (!Validation::isValidModulatorTarget(target, targetError))
            {
                errorMessage = "modulator target '" + target.id + "' invalid: " + targetError;
                return false;
            }
        }

        const std::vector<ModulationRoute> modulationRoutes =
            StateSerializer::varToModulationRoutes(parsed["modulationRoutes"]);
        for (const auto& route : modulationRoutes)
        {
            std::string routeError;
            if (!Validation::isValidModulationRoute(route, routeError))
            {
                errorMessage = "modulation route '" + route.id + "' invalid: " + routeError;
                return false;
            }
        }

        for (const auto& instance : instances)
            core.getInstanceRegistry().addInstance(instance);

        for (const auto& preset : motifPresets)
            core.getPresetLibrary().addOrReplaceMotifPreset(preset);

        for (const auto& target : modulatorTargets)
            core.getModulatorTargetLibrary().addOrReplaceTarget(target);

        for (const auto& route : modulationRoutes)
            core.getModulationRouteLibrary().addOrReplaceRoute(route);

        for (const auto& scene : scenes)
            core.getSceneLibrary().addOrReplaceScene(scene);

        core.getBlueprintLibrary().addOrReplaceBlueprint(blueprint);
        core.setCurrentBlueprint(blueprint);

        return true;
    }
}
