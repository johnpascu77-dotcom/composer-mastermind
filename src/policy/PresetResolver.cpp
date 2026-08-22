#include "PresetResolver.h"
#include <algorithm>
#include <cmath>

namespace
{
    // Shared merge step: applies activePattern/gridMode/swing (each -1/-1.0f
    // = "leave untouched") onto scene's SceneInstanceOverride entry for
    // targetInstanceId, creating one if it doesn't exist yet, and ensures
    // the instance is actually in scene.targets (an override is inert
    // otherwise - see Router::routeScene).
    void applyFieldsToInstance(const std::string& targetInstanceId, int activePattern, int gridMode, float swing,
                                Scene& scene)
    {
        if (std::find(scene.targets.begin(), scene.targets.end(), targetInstanceId) == scene.targets.end())
            scene.targets.push_back(targetInstanceId);

        for (auto& override : scene.instanceOverrides)
        {
            if (override.targetInstance != targetInstanceId)
                continue;

            if (activePattern >= 0)
                override.activePattern = activePattern;
            if (gridMode >= 0)
                override.gridMode = gridMode;
            if (swing >= 0.0f)
                override.swing = swing;
            return;
        }

        SceneInstanceOverride newOverride;
        newOverride.targetInstance = targetInstanceId;
        newOverride.activePattern = activePattern;
        newOverride.gridMode = gridMode;
        newOverride.swing = swing;
        scene.instanceOverrides.push_back(newOverride);
    }
}

namespace PresetResolver
{
    int applyRolePreset(const RolePreset& preset, const std::vector<Instance>& allInstances, Scene& scene)
    {
        int affectedCount = 0;

        for (const auto& instance : allInstances)
        {
            if (instance.role != preset.targetRole)
                continue;

            applyFieldsToInstance(instance.id, preset.activePattern, preset.gridMode, preset.swing, scene);
            ++affectedCount;
        }

        return affectedCount;
    }

    int applyRhythmicRelationshipPreset(const RhythmicRelationshipPreset& preset,
                                         const std::vector<Instance>& allInstances, Scene& scene)
    {
        int affectedCount = 0;

        for (const auto& slot : preset.roleSlots)
        {
            for (const auto& instance : allInstances)
            {
                if (instance.role != slot.targetRole)
                    continue;

                applyFieldsToInstance(instance.id, slot.activePattern, slot.gridMode, slot.swing, scene);
                ++affectedCount;
            }
        }

        return affectedCount;
    }

    int applyArcPreset(const ArcPreset& preset, const std::string& targetArcName, int startBar, int endBar,
                        ArcSet& arcSet)
    {
        if (preset.breakpoints.empty() || endBar <= startBar)
            return 0;

        const auto arcNames = arcSet.getArcNames();
        if (std::find(arcNames.begin(), arcNames.end(), targetArcName) == arcNames.end())
            return 0;

        Arc arc = arcSet.getArc(targetArcName);

        std::vector<ArcBreakpoint> merged;
        for (const auto& existing : arc.getBreakpoints())
            if (existing.bar < startBar || existing.bar > endBar)
                merged.push_back(existing);

        for (const auto& presetBreakpoint : preset.breakpoints)
        {
            const float clampedPosition = std::min(1.0f, std::max(0.0f, presetBreakpoint.position));
            const float clampedValue = std::min(1.0f, std::max(0.0f, presetBreakpoint.value));
            const int bar = startBar + static_cast<int>(std::lround(clampedPosition * (endBar - startBar)));

            ArcBreakpoint newBreakpoint;
            newBreakpoint.bar = bar;
            newBreakpoint.value = clampedValue;
            merged.push_back(newBreakpoint);
        }

        arc.setBreakpoints(merged);
        arcSet.setArc(targetArcName, arc);

        return static_cast<int>(preset.breakpoints.size());
    }
}
