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

        Scene currentScene;
        if (core.getCurrentScene(currentScene))
            obj->setProperty("currentSceneId", juce::String(currentScene.id));

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

        return true;
    }
}
