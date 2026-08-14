#pragma once

#include <mutex>
#include "../routing/InstanceRegistry.h"
#include "../routing/Router.h"
#include "../midi/CCDispatcher.h"
#include "../scheduling/Scheduler.h"
#include "../state/SceneLibrary.h"
#include "../model/Scene.h"

class ComposerCore
{
public:
    ComposerCore();

    InstanceRegistry& getInstanceRegistry();
    CCDispatcher& getCCDispatcher();
    Router& getRouter();
    Scheduler& getScheduler();
    SceneLibrary& getSceneLibrary();

    // Callable from the UI thread; safe to call concurrently with processBar.
    // Setting a scene re-anchors the scene-chain clock: if it has a
    // nextSceneId, its durationBars starts counting from whichever bar
    // processBar next runs on.
    void setCurrentScene(const Scene& scene);
    bool getCurrentScene(Scene& outScene) const;

    // Audio-thread bar tick: dispatches due scheduled events and advances
    // the scene chain (Scene::nextSceneId) if the active scene's
    // durationBars has elapsed.
    void processBar(int currentBar);

    // Immediately routes the current scene (used for manual "send now" UI actions).
    void fullRefresh();

    // Called when the host transport stops/restarts, so a scene's elapsed
    // duration is counted from the new playback start rather than stale bar
    // numbers from before the stop.
    void notifyTransportReset();

private:
    InstanceRegistry instanceRegistry;
    CCDispatcher ccDispatcher;
    Router router;
    Scheduler scheduler;
    SceneLibrary sceneLibrary;

    mutable std::mutex sceneMutex;
    Scene currentSceneData;
    bool hasCurrentScene = false;
    bool sceneStartPending = false;
    int currentSceneStartBar = -1;

    void dispatchSceneIfNeeded(int currentBar);
    void advanceSceneChainIfNeeded(int currentBar);
};
