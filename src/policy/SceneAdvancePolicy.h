#pragma once

#include "../model/Scene.h"

// Pure decision logic for scene-chain advancement: given the currently
// active scene, which bar it started on, and the current bar, decide
// whether it's time to move to Scene::nextSceneId. Stateless and JUCE-free
// so it's unit testable; ComposerCore owns the actual mutable "what bar did
// this scene start" state and performs the switch.
//
// A scene with an empty nextSceneId or a non-positive durationBars never
// auto-advances - it behaves exactly like a static scene from before v0.3.
namespace SceneAdvancePolicy
{
    bool shouldAdvance(const Scene& currentScene, int sceneStartBar, int currentBar);
}
