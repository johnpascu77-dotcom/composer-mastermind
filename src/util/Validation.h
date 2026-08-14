#pragma once

#include <string>
#include "../model/Scene.h"
#include "../model/Mutation.h"
#include "../model/Instance.h"

namespace Validation
{
    bool isValidMidiChannel(int channel);
    bool isValidPatternIndex(int index);
    bool isValidGridMode(int mode);
    bool isValidSwing(float swing);

    bool isValidScene(const Scene& scene, std::string& errorMessage);
    bool isValidMutation(const Mutation& mutation, std::string& errorMessage);
    bool isValidInstance(const Instance& instance, std::string& errorMessage);
}
