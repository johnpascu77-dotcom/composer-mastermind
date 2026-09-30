#include "Validation.h"
#include "../midi/CCMapping.h"
#include <cmath>

namespace Validation
{
    bool isValidMidiChannel(int channel)
    {
        return channel >= 1 && channel <= 16;
    }

    bool isValidPatternIndex(int index)
    {
        return index >= 0 && index <= 3;
    }

    bool isValidGridMode(int mode)
    {
        return mode == 0 || mode == 1;
    }

    bool isValidSwing(float swing)
    {
        return swing >= 0.0f && swing <= CCMapping::kMaxSwing;
    }

    bool isValidCCNumber(int ccNumber)
    {
        return ccNumber >= 0 && ccNumber <= 127;
    }

    bool isValidScene(const Scene& scene, std::string& errorMessage)
    {
        if (scene.id.empty())
        {
            errorMessage = "Scene id is empty";
            return false;
        }

        if (scene.name.empty())
        {
            errorMessage = "Scene name is empty";
            return false;
        }

        if (scene.durationBars <= 0)
        {
            errorMessage = "Scene durationBars must be greater than 0";
            return false;
        }

        if (!isValidGridMode(scene.global.gridMode))
        {
            errorMessage = "Invalid grid mode";
            return false;
        }

        if (!isValidSwing(scene.global.swing))
        {
            errorMessage = "Invalid swing value";
            return false;
        }

        for (const auto& override : scene.instanceOverrides)
        {
            if (override.targetInstance.empty())
            {
                errorMessage = "Scene instance override has an empty target instance";
                return false;
            }

            if (override.gridMode != -1 && !isValidGridMode(override.gridMode))
            {
                errorMessage = "Invalid grid mode override for '" + override.targetInstance + "'";
                return false;
            }

            if (override.swing != -1.0f && !isValidSwing(override.swing))
            {
                errorMessage = "Invalid swing override for '" + override.targetInstance + "'";
                return false;
            }
        }

        return true;
    }

    bool isValidMutation(const Mutation& mutation, std::string& errorMessage)
    {
        if (mutation.type.empty())
        {
            errorMessage = "Mutation type is empty";
            return false;
        }

        if (mutation.targetInstance.empty())
        {
            errorMessage = "Mutation target instance is empty";
            return false;
        }

        return true;
    }

    bool isValidInstance(const Instance& instance, std::string& errorMessage)
    {
        if (instance.id.empty())
        {
            errorMessage = "Instance id is empty";
            return false;
        }

        if (!isValidMidiChannel(instance.midiChannel))
        {
            errorMessage = "Invalid MIDI channel";
            return false;
        }

        return true;
    }

    bool isValidBlueprint(const Blueprint& blueprint, std::string& errorMessage)
    {
        if (blueprint.id.empty())
        {
            errorMessage = "Blueprint id is empty";
            return false;
        }

        if (blueprint.name.empty())
        {
            errorMessage = "Blueprint name is empty";
            return false;
        }

        for (const auto& section : blueprint.sections)
        {
            if (section.id.empty())
            {
                errorMessage = "Blueprint has a section with an empty id";
                return false;
            }

            if (section.durationBars <= 0)
            {
                errorMessage = "Section '" + section.id + "' durationBars must be greater than 0";
                return false;
            }

            if (!section.archetype.empty() && section.archetype != "presentation" && section.archetype != "build"
                && section.archetype != "peak" && section.archetype != "release")
            {
                errorMessage = "Section '" + section.id + "' has an unknown archetype";
                return false;
            }

            for (const auto& layerRole : section.layerRoles)
            {
                if (layerRole.targetInstance.empty())
                {
                    errorMessage = "Section '" + section.id + "' has a layer role with an empty target instance";
                    return false;
                }
            }

            for (const auto& budgetOverride : section.budgetOverrides)
            {
                if (budgetOverride.role.empty())
                {
                    errorMessage = "Section '" + section.id + "' has a budget override with an empty role";
                    return false;
                }
            }

            for (const auto& reservedValue : section.reservedValues)
            {
                if (reservedValue.targetInstance.empty())
                {
                    errorMessage = "Section '" + section.id + "' has a reserved value with an empty target instance";
                    return false;
                }

                if (reservedValue.type.empty())
                {
                    errorMessage = "Section '" + section.id + "' has a reserved value with an empty type";
                    return false;
                }
            }

            for (const auto& modulatorValue : section.modulatorValues)
            {
                if (modulatorValue.modulatorTargetId.empty())
                {
                    errorMessage = "Section '" + section.id + "' has a modulator value with an empty target id";
                    return false;
                }

                if (!isValidCCNumber(modulatorValue.value))
                {
                    errorMessage = "Section '" + section.id + "' has a modulator value outside 0..127";
                    return false;
                }
            }

            for (const auto& capturedContent : section.capturedContent)
            {
                if (capturedContent.targetInstance.empty())
                {
                    errorMessage = "Section '" + section.id + "' has captured content with an empty target instance";
                    return false;
                }

                if (!isValidPatternIndex(capturedContent.patternIndex))
                {
                    errorMessage = "Section '" + section.id + "' has captured content with an invalid pattern index";
                    return false;
                }

                if ((int) capturedContent.steps.size() != CCMapping::kPatternSteps)
                {
                    errorMessage = "Section '" + section.id + "' has captured content that isn't exactly "
                                    + std::to_string(CCMapping::kPatternSteps) + " steps";
                    return false;
                }

                for (const auto& step : capturedContent.steps)
                {
                    if (!isValidCCNumber(step.note) || !isValidCCNumber(step.velocity))
                    {
                        errorMessage = "Section '" + section.id + "' has captured content with a note/velocity "
                                        "outside 0..127";
                        return false;
                    }

                    if (step.duration < 0 || step.duration > CCMapping::kPatternSteps)
                    {
                        errorMessage = "Section '" + section.id + "' has captured content with a duration outside "
                                        "0.." + std::to_string(CCMapping::kPatternSteps);
                        return false;
                    }
                }
            }
        }

        for (const auto& curve : blueprint.arcCurves)
        {
            if (curve.dimension != "energy" && curve.dimension != "tension" && curve.dimension != "density"
                && curve.dimension != "complexity" && curve.dimension != "coherence")
            {
                errorMessage = "Blueprint has an arc curve with an unknown dimension '" + curve.dimension + "'";
                return false;
            }

            for (const auto& point : curve.points)
            {
                if (point.value < 0.0f || point.value > 1.0f)
                {
                    errorMessage = "Blueprint's '" + curve.dimension + "' arc curve has a point value outside 0..1";
                    return false;
                }
            }
        }

        return true;
    }

    bool isValidRolePreset(const RolePreset& preset, std::string& errorMessage)
    {
        if (preset.id.empty())
        {
            errorMessage = "Preset id is empty";
            return false;
        }

        if (preset.name.empty())
        {
            errorMessage = "Preset name is empty";
            return false;
        }

        if (preset.targetRole.empty())
        {
            errorMessage = "Preset target role is empty";
            return false;
        }

        if (preset.gridMode != -1 && !isValidGridMode(preset.gridMode))
        {
            errorMessage = "Invalid grid mode in preset '" + preset.id + "'";
            return false;
        }

        if (preset.swing != -1.0f && !isValidSwing(preset.swing))
        {
            errorMessage = "Invalid swing value in preset '" + preset.id + "'";
            return false;
        }

        return true;
    }

    bool isValidRhythmicRelationshipPreset(const RhythmicRelationshipPreset& preset, std::string& errorMessage)
    {
        if (preset.id.empty())
        {
            errorMessage = "Preset id is empty";
            return false;
        }

        if (preset.name.empty())
        {
            errorMessage = "Preset name is empty";
            return false;
        }

        if (preset.roleSlots.size() < 2)
        {
            errorMessage = "Preset '" + preset.id + "' needs at least 2 role slots - a joint choice across a "
                            "single role isn't a rhythmic relationship";
            return false;
        }

        for (const auto& slot : preset.roleSlots)
        {
            if (slot.targetRole.empty())
            {
                errorMessage = "Preset '" + preset.id + "' has a role slot with an empty target role";
                return false;
            }

            if (slot.gridMode != -1 && !isValidGridMode(slot.gridMode))
            {
                errorMessage = "Invalid grid mode in preset '" + preset.id + "', role '" + slot.targetRole + "'";
                return false;
            }

            if (slot.swing != -1.0f && !isValidSwing(slot.swing))
            {
                errorMessage = "Invalid swing value in preset '" + preset.id + "', role '" + slot.targetRole + "'";
                return false;
            }
        }

        return true;
    }

    bool isValidArcPreset(const ArcPreset& preset, std::string& errorMessage)
    {
        if (preset.id.empty())
        {
            errorMessage = "Preset id is empty";
            return false;
        }

        if (preset.name.empty())
        {
            errorMessage = "Preset name is empty";
            return false;
        }

        if (preset.breakpoints.empty())
        {
            errorMessage = "Preset '" + preset.id + "' has no breakpoints";
            return false;
        }

        for (const auto& breakpoint : preset.breakpoints)
        {
            if (breakpoint.position < 0.0f || breakpoint.position > 1.0f)
            {
                errorMessage = "Preset '" + preset.id + "' has a breakpoint position outside 0..1";
                return false;
            }

            if (breakpoint.value < 0.0f || breakpoint.value > 1.0f)
            {
                errorMessage = "Preset '" + preset.id + "' has a breakpoint value outside 0..1";
                return false;
            }
        }

        return true;
    }

    bool isValidMotifPreset(const MotifPreset& preset, std::string& errorMessage)
    {
        if (preset.id.empty())
        {
            errorMessage = "Preset id is empty";
            return false;
        }

        if (preset.name.empty())
        {
            errorMessage = "Preset name is empty";
            return false;
        }

        if (preset.notes.empty())
        {
            errorMessage = "Preset '" + preset.id + "' has no notes";
            return false;
        }

        for (const auto& note : preset.notes)
        {
            if (note.relativeDuration <= 0.0f)
            {
                errorMessage = "Preset '" + preset.id + "' has a note with a non-positive relative duration";
                return false;
            }

            // A rest's relativeVelocity is unused (nothing sounds there) -
            // don't reject a rest just because it carries whatever default/
            // leftover velocity value it happened to be built with.
            if (!note.isRest && note.relativeVelocity <= 0.0f)
            {
                errorMessage = "Preset '" + preset.id + "' has a note with a non-positive relative velocity";
                return false;
            }
        }

        return true;
    }

    bool isValidModulatorTarget(const ModulatorTarget& target, std::string& errorMessage)
    {
        if (target.id.empty())
        {
            errorMessage = "Modulator target id is empty";
            return false;
        }

        if (!isValidCCNumber(target.ccNumber))
        {
            errorMessage = "Invalid CC number in modulator target '" + target.id + "'";
            return false;
        }

        if (!isValidMidiChannel(target.midiChannel))
        {
            errorMessage = "Invalid MIDI channel in modulator target '" + target.id + "'";
            return false;
        }

        if (target.mode != "arc" && target.mode != "section")
        {
            errorMessage = "Modulator target '" + target.id + "' has an unknown mode (expected 'arc' or 'section')";
            return false;
        }

        if (target.mode == "arc" && target.arcDimension.empty())
        {
            errorMessage = "Modulator target '" + target.id + "' is in 'arc' mode but has no arc dimension set";
            return false;
        }

        return true;
    }

    bool isValidModulationRoute(const ModulationRoute& route, std::string& errorMessage)
    {
        if (route.id.empty())
        {
            errorMessage = "Modulation route id is empty";
            return false;
        }

        if (route.dispatchMode == ModulationDispatchMode::Bar && route.arcDimension.empty())
        {
            errorMessage = "Modulation route '" + route.id + "' has no arc dimension set";
            return false;
        }

        if (route.dispatchMode == ModulationDispatchMode::Sequence
            || route.dispatchMode == ModulationDispatchMode::BarCycle)
        {
            const char* modeName = route.dispatchMode == ModulationDispatchMode::Sequence ? "Sequence" : "BarCycle";

            if (!isContinuousModulationParameter(route.parameter))
            {
                errorMessage = "Modulation route '" + route.id + "' is in " + modeName
                    + " mode but its parameter isn't continuous (transpose/rotation/length/swing/rate only)";
                return false;
            }

            if (route.sequenceValues.empty())
            {
                errorMessage = "Modulation route '" + route.id + "' is in " + modeName
                    + " mode but has no sequence values";
                return false;
            }

            for (float value : route.sequenceValues)
            {
                if (!std::isfinite(value))
                {
                    errorMessage = "Modulation route '" + route.id + "' has a non-finite sequence value";
                    return false;
                }
            }
        }

        if (route.dispatchMode == ModulationDispatchMode::BarCycle && route.phraseLengthBars < 1)
        {
            errorMessage = "Modulation route '" + route.id + "' has a phraseLengthBars below 1";
            return false;
        }

        if (route.targetInstance.empty())
        {
            errorMessage = "Modulation route '" + route.id + "' has no target instance set (use '*' to broadcast)";
            return false;
        }

        // 0-2, matching CCMapping::patternBaseCC's own valid range (MPL only
        // exposes 3 patterns) - ignored at dispatch time for the global
        // parameters (Swing/ActivePattern/GridMode), but still validated
        // here so a stray out-of-range value doesn't silently mean something
        // else if the route is later repointed at a pattern-scoped parameter.
        if (route.patternIndex < 0 || route.patternIndex > 2)
        {
            errorMessage = "Modulation route '" + route.id + "' has an invalid pattern index (expected 0-2)";
            return false;
        }

        if (!std::isfinite(route.outputMin) || !std::isfinite(route.outputMax) || !std::isfinite(route.threshold))
        {
            errorMessage = "Modulation route '" + route.id + "' has a non-finite range/threshold value";
            return false;
        }

        if (route.threshold < 0.0f || route.threshold > 1.0f)
        {
            errorMessage = "Modulation route '" + route.id + "' threshold must be between 0 and 1";
            return false;
        }

        return true;
    }
}
