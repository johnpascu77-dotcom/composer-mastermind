#pragma once

#include <vector>
#include <string>
#include <mutex>
#include "../model/Blueprint.h"

// A named set of Blueprints, mirroring SceneLibrary's shape exactly:
// JUCE-free, mutex-guarded, copy-returning rather than handing out pointers
// into the vector, so it stays unit testable and safe to touch from both
// the UI thread and (eventually) the audio thread.
class BlueprintLibrary
{
public:
    bool addOrReplaceBlueprint(const Blueprint& blueprint);
    bool removeBlueprint(const std::string& blueprintId);

    bool getBlueprintById(const std::string& blueprintId, Blueprint& outBlueprint) const;
    std::vector<Blueprint> getAllBlueprints() const;

    void clear();

private:
    std::vector<Blueprint> blueprints;
    mutable std::mutex blueprintsMutex;
};
