#include "PresetLibrary.h"

bool PresetLibrary::addOrReplaceRolePreset(const RolePreset& preset)
{
    if (preset.id.empty())
        return false;

    std::lock_guard<std::mutex> lock(presetsMutex);

    for (auto& existing : presets)
    {
        if (existing.id == preset.id)
        {
            existing = preset;
            return true;
        }
    }

    presets.push_back(preset);
    return true;
}

bool PresetLibrary::removeRolePreset(const std::string& presetId)
{
    std::lock_guard<std::mutex> lock(presetsMutex);

    for (auto it = presets.begin(); it != presets.end(); ++it)
    {
        if (it->id == presetId)
        {
            presets.erase(it);
            return true;
        }
    }

    return false;
}

bool PresetLibrary::getRolePresetById(const std::string& presetId, RolePreset& outPreset) const
{
    std::lock_guard<std::mutex> lock(presetsMutex);

    for (const auto& preset : presets)
    {
        if (preset.id == presetId)
        {
            outPreset = preset;
            return true;
        }
    }

    return false;
}

std::vector<RolePreset> PresetLibrary::getAllRolePresets() const
{
    std::lock_guard<std::mutex> lock(presetsMutex);
    return presets;
}

bool PresetLibrary::addOrReplaceRhythmicRelationshipPreset(const RhythmicRelationshipPreset& preset)
{
    if (preset.id.empty())
        return false;

    std::lock_guard<std::mutex> lock(presetsMutex);

    for (auto& existing : rhythmicRelationshipPresets)
    {
        if (existing.id == preset.id)
        {
            existing = preset;
            return true;
        }
    }

    rhythmicRelationshipPresets.push_back(preset);
    return true;
}

bool PresetLibrary::removeRhythmicRelationshipPreset(const std::string& presetId)
{
    std::lock_guard<std::mutex> lock(presetsMutex);

    for (auto it = rhythmicRelationshipPresets.begin(); it != rhythmicRelationshipPresets.end(); ++it)
    {
        if (it->id == presetId)
        {
            rhythmicRelationshipPresets.erase(it);
            return true;
        }
    }

    return false;
}

bool PresetLibrary::getRhythmicRelationshipPresetById(const std::string& presetId,
                                                        RhythmicRelationshipPreset& outPreset) const
{
    std::lock_guard<std::mutex> lock(presetsMutex);

    for (const auto& preset : rhythmicRelationshipPresets)
    {
        if (preset.id == presetId)
        {
            outPreset = preset;
            return true;
        }
    }

    return false;
}

std::vector<RhythmicRelationshipPreset> PresetLibrary::getAllRhythmicRelationshipPresets() const
{
    std::lock_guard<std::mutex> lock(presetsMutex);
    return rhythmicRelationshipPresets;
}

bool PresetLibrary::addOrReplaceArcPreset(const ArcPreset& preset)
{
    if (preset.id.empty())
        return false;

    std::lock_guard<std::mutex> lock(presetsMutex);

    for (auto& existing : arcPresets)
    {
        if (existing.id == preset.id)
        {
            existing = preset;
            return true;
        }
    }

    arcPresets.push_back(preset);
    return true;
}

bool PresetLibrary::removeArcPreset(const std::string& presetId)
{
    std::lock_guard<std::mutex> lock(presetsMutex);

    for (auto it = arcPresets.begin(); it != arcPresets.end(); ++it)
    {
        if (it->id == presetId)
        {
            arcPresets.erase(it);
            return true;
        }
    }

    return false;
}

bool PresetLibrary::getArcPresetById(const std::string& presetId, ArcPreset& outPreset) const
{
    std::lock_guard<std::mutex> lock(presetsMutex);

    for (const auto& preset : arcPresets)
    {
        if (preset.id == presetId)
        {
            outPreset = preset;
            return true;
        }
    }

    return false;
}

std::vector<ArcPreset> PresetLibrary::getAllArcPresets() const
{
    std::lock_guard<std::mutex> lock(presetsMutex);
    return arcPresets;
}

bool PresetLibrary::addOrReplaceMotifPreset(const MotifPreset& preset)
{
    if (preset.id.empty())
        return false;

    std::lock_guard<std::mutex> lock(presetsMutex);

    for (auto& existing : motifPresets)
    {
        if (existing.id == preset.id)
        {
            existing = preset;
            return true;
        }
    }

    motifPresets.push_back(preset);
    return true;
}

bool PresetLibrary::removeMotifPreset(const std::string& presetId)
{
    std::lock_guard<std::mutex> lock(presetsMutex);

    for (auto it = motifPresets.begin(); it != motifPresets.end(); ++it)
    {
        if (it->id == presetId)
        {
            motifPresets.erase(it);
            return true;
        }
    }

    return false;
}

bool PresetLibrary::getMotifPresetById(const std::string& presetId, MotifPreset& outPreset) const
{
    std::lock_guard<std::mutex> lock(presetsMutex);

    for (const auto& preset : motifPresets)
    {
        if (preset.id == presetId)
        {
            outPreset = preset;
            return true;
        }
    }

    return false;
}

std::vector<MotifPreset> PresetLibrary::getAllMotifPresets() const
{
    std::lock_guard<std::mutex> lock(presetsMutex);
    return motifPresets;
}

void PresetLibrary::clear()
{
    std::lock_guard<std::mutex> lock(presetsMutex);
    presets.clear();
    rhythmicRelationshipPresets.clear();
    arcPresets.clear();
    motifPresets.clear();
}
