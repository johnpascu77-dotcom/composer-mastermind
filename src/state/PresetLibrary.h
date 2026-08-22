#pragma once

#include <vector>
#include <string>
#include <mutex>
#include "../model/Preset.h"

// A named set of RolePresets, mirroring SceneLibrary/BlueprintLibrary's
// shape exactly: JUCE-free, mutex-guarded, copy-returning rather than
// handing out pointers into the vector.
class PresetLibrary
{
public:
    bool addOrReplaceRolePreset(const RolePreset& preset);
    bool removeRolePreset(const std::string& presetId);

    bool getRolePresetById(const std::string& presetId, RolePreset& outPreset) const;
    std::vector<RolePreset> getAllRolePresets() const;

    bool addOrReplaceRhythmicRelationshipPreset(const RhythmicRelationshipPreset& preset);
    bool removeRhythmicRelationshipPreset(const std::string& presetId);

    bool getRhythmicRelationshipPresetById(const std::string& presetId, RhythmicRelationshipPreset& outPreset) const;
    std::vector<RhythmicRelationshipPreset> getAllRhythmicRelationshipPresets() const;

    bool addOrReplaceArcPreset(const ArcPreset& preset);
    bool removeArcPreset(const std::string& presetId);

    bool getArcPresetById(const std::string& presetId, ArcPreset& outPreset) const;
    std::vector<ArcPreset> getAllArcPresets() const;

    bool addOrReplaceMotifPreset(const MotifPreset& preset);
    bool removeMotifPreset(const std::string& presetId);

    bool getMotifPresetById(const std::string& presetId, MotifPreset& outPreset) const;
    std::vector<MotifPreset> getAllMotifPresets() const;

    void clear();

private:
    std::vector<RolePreset> presets;
    std::vector<RhythmicRelationshipPreset> rhythmicRelationshipPresets;
    std::vector<ArcPreset> arcPresets;
    std::vector<MotifPreset> motifPresets;
    mutable std::mutex presetsMutex;
};
