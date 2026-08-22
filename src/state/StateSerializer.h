#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "../model/Instance.h"
#include "../model/Mutation.h"
#include "../model/Scene.h"
#include "../model/Blueprint.h"
#include "../model/Preset.h"
#include "../model/ModulatorTarget.h"

// Converts the model structs to/from juce::var, per docs/scene_format_v0_1.md.
// This is the JUCE boundary: model/, routing/, midi/, scheduling/ stay
// JUCE-free by design (unit testable without a plugin host) - only state/
// touches JUCE's JSON machinery. Missing fields fall back to the struct's
// own default via JsonHelpers, so an older/partial JSON file degrades
// gracefully instead of failing to parse.
namespace StateSerializer
{
    juce::var instanceToVar(const Instance& instance);
    Instance varToInstance(const juce::var& value);

    juce::var mutationToVar(const Mutation& mutation);
    Mutation varToMutation(const juce::var& value);

    juce::var scenePatternToVar(const ScenePattern& pattern);
    ScenePattern varToScenePattern(const juce::var& value);

    juce::var sceneInstanceOverrideToVar(const SceneInstanceOverride& override);
    SceneInstanceOverride varToSceneInstanceOverride(const juce::var& value);

    juce::var sceneToVar(const Scene& scene);
    Scene varToScene(const juce::var& value);

    juce::var instancesToVar(const std::vector<Instance>& instances);
    std::vector<Instance> varToInstances(const juce::var& value);

    juce::var scenesToVar(const std::vector<Scene>& scenes);
    std::vector<Scene> varToScenes(const juce::var& value);

    juce::var sectionLayerRoleToVar(const SectionLayerRole& layerRole);
    SectionLayerRole varToSectionLayerRole(const juce::var& value);

    juce::var sectionBudgetOverrideToVar(const SectionBudgetOverride& budgetOverride);
    SectionBudgetOverride varToSectionBudgetOverride(const juce::var& value);

    juce::var reservedValueToVar(const ReservedValue& reservedValue);
    ReservedValue varToReservedValue(const juce::var& value);

    juce::var sectionModulatorValueToVar(const SectionModulatorValue& modulatorValue);
    SectionModulatorValue varToSectionModulatorValue(const juce::var& value);

    juce::var blueprintSectionToVar(const BlueprintSection& section);
    BlueprintSection varToBlueprintSection(const juce::var& value);

    juce::var blueprintArcPointToVar(const BlueprintArcPoint& point);
    BlueprintArcPoint varToBlueprintArcPoint(const juce::var& value);

    juce::var blueprintArcCurveToVar(const BlueprintArcCurve& curve);
    BlueprintArcCurve varToBlueprintArcCurve(const juce::var& value);

    juce::var blueprintToVar(const Blueprint& blueprint);
    Blueprint varToBlueprint(const juce::var& value);

    juce::var blueprintsToVar(const std::vector<Blueprint>& blueprints);
    std::vector<Blueprint> varToBlueprints(const juce::var& value);

    juce::var rolePresetToVar(const RolePreset& preset);
    RolePreset varToRolePreset(const juce::var& value);

    juce::var rolePresetsToVar(const std::vector<RolePreset>& presets);
    std::vector<RolePreset> varToRolePresets(const juce::var& value);

    juce::var rhythmicRelationshipRoleSlotToVar(const RhythmicRelationshipRoleSlot& slot);
    RhythmicRelationshipRoleSlot varToRhythmicRelationshipRoleSlot(const juce::var& value);

    juce::var rhythmicRelationshipPresetToVar(const RhythmicRelationshipPreset& preset);
    RhythmicRelationshipPreset varToRhythmicRelationshipPreset(const juce::var& value);

    juce::var rhythmicRelationshipPresetsToVar(const std::vector<RhythmicRelationshipPreset>& presets);
    std::vector<RhythmicRelationshipPreset> varToRhythmicRelationshipPresets(const juce::var& value);

    juce::var arcPresetBreakpointToVar(const ArcPresetBreakpoint& breakpoint);
    ArcPresetBreakpoint varToArcPresetBreakpoint(const juce::var& value);

    juce::var arcPresetToVar(const ArcPreset& preset);
    ArcPreset varToArcPreset(const juce::var& value);

    juce::var arcPresetsToVar(const std::vector<ArcPreset>& presets);
    std::vector<ArcPreset> varToArcPresets(const juce::var& value);

    juce::var motifNoteToVar(const MotifNote& note);
    MotifNote varToMotifNote(const juce::var& value);

    juce::var motifPresetToVar(const MotifPreset& preset);
    MotifPreset varToMotifPreset(const juce::var& value);

    juce::var motifPresetsToVar(const std::vector<MotifPreset>& presets);
    std::vector<MotifPreset> varToMotifPresets(const juce::var& value);

    juce::var modulatorTargetToVar(const ModulatorTarget& target);
    ModulatorTarget varToModulatorTarget(const juce::var& value);

    juce::var modulatorTargetsToVar(const std::vector<ModulatorTarget>& targets);
    std::vector<ModulatorTarget> varToModulatorTargets(const juce::var& value);
}
