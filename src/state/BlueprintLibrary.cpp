#include "BlueprintLibrary.h"

bool BlueprintLibrary::addOrReplaceBlueprint(const Blueprint& blueprint)
{
    if (blueprint.id.empty())
        return false;

    std::lock_guard<std::mutex> lock(blueprintsMutex);

    for (auto& existing : blueprints)
    {
        if (existing.id == blueprint.id)
        {
            existing = blueprint;
            return true;
        }
    }

    blueprints.push_back(blueprint);
    return true;
}

bool BlueprintLibrary::removeBlueprint(const std::string& blueprintId)
{
    std::lock_guard<std::mutex> lock(blueprintsMutex);

    for (auto it = blueprints.begin(); it != blueprints.end(); ++it)
    {
        if (it->id == blueprintId)
        {
            blueprints.erase(it);
            return true;
        }
    }

    return false;
}

bool BlueprintLibrary::getBlueprintById(const std::string& blueprintId, Blueprint& outBlueprint) const
{
    std::lock_guard<std::mutex> lock(blueprintsMutex);

    for (const auto& blueprint : blueprints)
    {
        if (blueprint.id == blueprintId)
        {
            outBlueprint = blueprint;
            return true;
        }
    }

    return false;
}

std::vector<Blueprint> BlueprintLibrary::getAllBlueprints() const
{
    std::lock_guard<std::mutex> lock(blueprintsMutex);
    return blueprints;
}

void BlueprintLibrary::clear()
{
    std::lock_guard<std::mutex> lock(blueprintsMutex);
    blueprints.clear();
}
