#pragma once

#include <string>
#include "../model/Scene.h"
#include "../model/Mutation.h"
#include "../model/Instance.h"
#include "../model/Blueprint.h"
#include "../model/Preset.h"
#include "../model/ModulatorTarget.h"

namespace Validation
{
    bool isValidMidiChannel(int channel);
    bool isValidPatternIndex(int index);
    bool isValidGridMode(int mode);
    bool isValidSwing(float swing);
    bool isValidCCNumber(int ccNumber);

    bool isValidScene(const Scene& scene, std::string& errorMessage);
    bool isValidMutation(const Mutation& mutation, std::string& errorMessage);
    bool isValidInstance(const Instance& instance, std::string& errorMessage);
    bool isValidBlueprint(const Blueprint& blueprint, std::string& errorMessage);
    bool isValidRolePreset(const RolePreset& preset, std::string& errorMessage);
    bool isValidRhythmicRelationshipPreset(const RhythmicRelationshipPreset& preset, std::string& errorMessage);
    bool isValidArcPreset(const ArcPreset& preset, std::string& errorMessage);
    bool isValidMotifPreset(const MotifPreset& preset, std::string& errorMessage);
    bool isValidModulatorTarget(const ModulatorTarget& target, std::string& errorMessage);
}
