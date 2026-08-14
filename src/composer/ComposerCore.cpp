#include "ComposerCore.h"
#include "../policy/SceneAdvancePolicy.h"

ComposerCore::ComposerCore()
    : router(instanceRegistry, ccDispatcher)
{
}

InstanceRegistry& ComposerCore::getInstanceRegistry()
{
    return instanceRegistry;
}

CCDispatcher& ComposerCore::getCCDispatcher()
{
    return ccDispatcher;
}

Router& ComposerCore::getRouter()
{
    return router;
}

Scheduler& ComposerCore::getScheduler()
{
    return scheduler;
}

SceneLibrary& ComposerCore::getSceneLibrary()
{
    return sceneLibrary;
}

void ComposerCore::setCurrentScene(const Scene& scene)
{
    std::lock_guard<std::mutex> lock(sceneMutex);
    currentSceneData = scene;
    hasCurrentScene = true;
    sceneStartPending = true;
    currentSceneStartBar = -1;
}

bool ComposerCore::getCurrentScene(Scene& outScene) const
{
    std::lock_guard<std::mutex> lock(sceneMutex);
    if (!hasCurrentScene)
        return false;

    outScene = currentSceneData;
    return true;
}

void ComposerCore::processBar(int currentBar)
{
    dispatchSceneIfNeeded(currentBar);
    advanceSceneChainIfNeeded(currentBar);
}

void ComposerCore::fullRefresh()
{
    Scene scene;
    if (getCurrentScene(scene))
        router.routeScene(scene);
}

void ComposerCore::notifyTransportReset()
{
    std::lock_guard<std::mutex> lock(sceneMutex);
    sceneStartPending = true;
}

void ComposerCore::dispatchSceneIfNeeded(int currentBar)
{
    const auto dueEvents = scheduler.popDueEvents(currentBar);
    if (dueEvents.empty())
        return;

    Scene scene;
    if (!getCurrentScene(scene))
        return;

    for (const auto& event : dueEvents)
    {
        if (event.type == "scene")
            router.routeScene(scene);
    }
}

void ComposerCore::advanceSceneChainIfNeeded(int currentBar)
{
    Scene scene;
    int startBar = -1;

    {
        std::lock_guard<std::mutex> lock(sceneMutex);
        if (!hasCurrentScene)
            return;

        if (sceneStartPending)
        {
            currentSceneStartBar = currentBar;
            sceneStartPending = false;
        }

        scene = currentSceneData;
        startBar = currentSceneStartBar;
    }

    if (!SceneAdvancePolicy::shouldAdvance(scene, startBar, currentBar))
        return;

    Scene nextScene;
    if (!sceneLibrary.getSceneById(scene.nextSceneId, nextScene))
        return; // dangling nextSceneId: chain stops here, current scene keeps playing

    {
        std::lock_guard<std::mutex> lock(sceneMutex);
        currentSceneData = nextScene;
        currentSceneStartBar = currentBar;
        sceneStartPending = false;
    }

    router.routeScene(nextScene);
}
