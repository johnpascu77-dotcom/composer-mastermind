#include "SceneLibrary.h"

bool SceneLibrary::addOrReplaceScene(const Scene& scene)
{
    if (scene.id.empty())
        return false;

    std::lock_guard<std::mutex> lock(scenesMutex);

    for (auto& existing : scenes)
    {
        if (existing.id == scene.id)
        {
            existing = scene;
            return true;
        }
    }

    scenes.push_back(scene);
    return true;
}

bool SceneLibrary::removeScene(const std::string& sceneId)
{
    std::lock_guard<std::mutex> lock(scenesMutex);

    for (auto it = scenes.begin(); it != scenes.end(); ++it)
    {
        if (it->id == sceneId)
        {
            scenes.erase(it);
            return true;
        }
    }

    return false;
}

bool SceneLibrary::getSceneById(const std::string& sceneId, Scene& outScene) const
{
    std::lock_guard<std::mutex> lock(scenesMutex);

    for (const auto& scene : scenes)
    {
        if (scene.id == sceneId)
        {
            outScene = scene;
            return true;
        }
    }

    return false;
}

std::vector<Scene> SceneLibrary::getAllScenes() const
{
    std::lock_guard<std::mutex> lock(scenesMutex);
    return scenes;
}

void SceneLibrary::clear()
{
    std::lock_guard<std::mutex> lock(scenesMutex);
    scenes.clear();
}
