#include "SceneAdvancePolicy.h"

namespace SceneAdvancePolicy
{
    bool shouldAdvance(const Scene& currentScene, int sceneStartBar, int currentBar)
    {
        if (currentScene.nextSceneId.empty())
            return false;

        if (currentScene.durationBars <= 0)
            return false;

        if (sceneStartBar < 0)
            return false;

        return (currentBar - sceneStartBar) >= currentScene.durationBars;
    }
}
