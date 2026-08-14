#pragma once

#include <vector>
#include <string>
#include <mutex>
#include "../model/Scene.h"

// A named set of Scenes. JUCE-free like InstanceRegistry, for the same
// reason: it's read/written from the UI thread and should stay unit
// testable without a plugin host. Mirrors InstanceRegistry's copy-returning,
// mutex-guarded style rather than handing out pointers into the vector.
class SceneLibrary
{
public:
    bool addOrReplaceScene(const Scene& scene);
    bool removeScene(const std::string& sceneId);

    bool getSceneById(const std::string& sceneId, Scene& outScene) const;
    std::vector<Scene> getAllScenes() const;

    void clear();

private:
    std::vector<Scene> scenes;
    mutable std::mutex scenesMutex;
};
