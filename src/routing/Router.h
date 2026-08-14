#pragma once

#include "../model/Scene.h"
#include "../model/Mutation.h"
#include "InstanceRegistry.h"
#include "../midi/CCDispatcher.h"

class Router
{
public:
    Router(InstanceRegistry& registry, CCDispatcher& dispatcher);

    void routeScene(const Scene& scene);
    void routeMutation(const Mutation& mutation);

private:
    InstanceRegistry& instanceRegistry;
    CCDispatcher& ccDispatcher;

    void sendSceneToInstance(const Scene& scene, const Instance& instance);
    void sendScenePattern(const ScenePattern& pattern, const Instance& instance);
};
