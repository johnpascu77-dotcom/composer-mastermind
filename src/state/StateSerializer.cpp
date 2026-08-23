#include "StateSerializer.h"
#include "../util/JsonHelpers.h"

namespace StateSerializer
{
    juce::var instanceToVar(const Instance& instance)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("id", juce::String(instance.id));
        obj->setProperty("name", juce::String(instance.name));
        obj->setProperty("midiChannel", instance.midiChannel);
        obj->setProperty("role", juce::String(instance.role));
        obj->setProperty("enabled", instance.enabled);
        obj->setProperty("lastKnownScene", juce::String(instance.lastKnownScene));
        return juce::var(obj);
    }

    Instance varToInstance(const juce::var& value)
    {
        Instance instance;
        instance.id = JsonHelpers::getString(value, "id");
        instance.name = JsonHelpers::getString(value, "name");
        instance.midiChannel = JsonHelpers::getInt(value, "midiChannel", 1);
        instance.role = JsonHelpers::getString(value, "role");
        instance.enabled = JsonHelpers::getBool(value, "enabled", true);
        instance.lastKnownScene = JsonHelpers::getString(value, "lastKnownScene");
        return instance;
    }

    juce::var mutationToVar(const Mutation& mutation)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("type", juce::String(mutation.type));
        obj->setProperty("targetInstance", juce::String(mutation.targetInstance));
        obj->setProperty("patternIndex", mutation.patternIndex);
        obj->setProperty("amount", mutation.amount);
        obj->setProperty("applyAtBar", mutation.applyAtBar);
        obj->setProperty("strength", juce::String(mutation.strength));
        return juce::var(obj);
    }

    Mutation varToMutation(const juce::var& value)
    {
        Mutation mutation;
        mutation.type = JsonHelpers::getString(value, "type");
        mutation.targetInstance = JsonHelpers::getString(value, "targetInstance");
        mutation.patternIndex = JsonHelpers::getInt(value, "patternIndex", 0);
        mutation.amount = JsonHelpers::getInt(value, "amount", 0);
        mutation.applyAtBar = JsonHelpers::getInt(value, "applyAtBar", 0);
        mutation.strength = JsonHelpers::getString(value, "strength");
        return mutation;
    }

    juce::var scenePatternToVar(const ScenePattern& pattern)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("targetInstance", juce::String(pattern.targetInstance));
        obj->setProperty("patternIndex", pattern.patternIndex);
        obj->setProperty("transpose", pattern.transpose);
        obj->setProperty("rotation", pattern.rotation);
        obj->setProperty("length", pattern.length);
        obj->setProperty("inversion", pattern.inversion);
        return juce::var(obj);
    }

    ScenePattern varToScenePattern(const juce::var& value)
    {
        ScenePattern pattern;
        pattern.targetInstance = JsonHelpers::getString(value, "targetInstance");
        pattern.patternIndex = JsonHelpers::getInt(value, "patternIndex", 0);
        pattern.transpose = JsonHelpers::getInt(value, "transpose", 0);
        pattern.rotation = JsonHelpers::getInt(value, "rotation", 0);
        pattern.length = JsonHelpers::getInt(value, "length", 16);
        pattern.inversion = JsonHelpers::getBool(value, "inversion", false);
        return pattern;
    }

    juce::var sceneInstanceOverrideToVar(const SceneInstanceOverride& override)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("targetInstance", juce::String(override.targetInstance));
        obj->setProperty("activePattern", override.activePattern);
        obj->setProperty("gridMode", override.gridMode);
        obj->setProperty("swing", override.swing);
        return juce::var(obj);
    }

    SceneInstanceOverride varToSceneInstanceOverride(const juce::var& value)
    {
        SceneInstanceOverride override;
        override.targetInstance = JsonHelpers::getString(value, "targetInstance");
        override.activePattern = JsonHelpers::getInt(value, "activePattern", -1);
        override.gridMode = JsonHelpers::getInt(value, "gridMode", -1);
        override.swing = JsonHelpers::getFloat(value, "swing", -1.0f);
        return override;
    }

    juce::var sceneToVar(const Scene& scene)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("schema", "ComposerMastermindScene.v1");
        obj->setProperty("id", juce::String(scene.id));
        obj->setProperty("name", juce::String(scene.name));
        obj->setProperty("durationBars", scene.durationBars);
        obj->setProperty("quantize", juce::String(scene.quantize));

        juce::Array<juce::var> targets;
        for (const auto& target : scene.targets)
            targets.add(juce::String(target));
        obj->setProperty("targets", targets);

        auto* globalObj = new juce::DynamicObject();
        globalObj->setProperty("activePattern", scene.global.activePattern);
        globalObj->setProperty("gridMode", scene.global.gridMode);
        globalObj->setProperty("swing", scene.global.swing);
        obj->setProperty("global", juce::var(globalObj));

        juce::Array<juce::var> instanceOverrides;
        for (const auto& override : scene.instanceOverrides)
            instanceOverrides.add(sceneInstanceOverrideToVar(override));
        obj->setProperty("instanceOverrides", instanceOverrides);

        juce::Array<juce::var> patterns;
        for (const auto& pattern : scene.patterns)
            patterns.add(scenePatternToVar(pattern));
        obj->setProperty("patterns", patterns);

        juce::Array<juce::var> mutations;
        for (const auto& mutation : scene.mutations)
            mutations.add(mutationToVar(mutation));
        obj->setProperty("mutations", mutations);

        obj->setProperty("nextSceneId", juce::String(scene.nextSceneId));
        obj->setProperty("transitionStyle", juce::String(scene.transitionStyle));
        obj->setProperty("rampBars", scene.rampBars);

        return juce::var(obj);
    }

    Scene varToScene(const juce::var& value)
    {
        Scene scene;
        scene.id = JsonHelpers::getString(value, "id");
        scene.name = JsonHelpers::getString(value, "name");
        scene.durationBars = JsonHelpers::getInt(value, "durationBars", 4);
        scene.quantize = JsonHelpers::getString(value, "quantize", "bar");

        for (const auto& target : JsonHelpers::getArray(value, "targets"))
            scene.targets.push_back(target.toString().toStdString());

        const auto& globalValue = value["global"];
        scene.global.activePattern = JsonHelpers::getInt(globalValue, "activePattern", 1);
        scene.global.gridMode = JsonHelpers::getInt(globalValue, "gridMode", 0);
        scene.global.swing = JsonHelpers::getFloat(globalValue, "swing", 0.0f);

        for (const auto& item : JsonHelpers::getArray(value, "instanceOverrides"))
            scene.instanceOverrides.push_back(varToSceneInstanceOverride(item));

        for (const auto& item : JsonHelpers::getArray(value, "patterns"))
            scene.patterns.push_back(varToScenePattern(item));

        for (const auto& item : JsonHelpers::getArray(value, "mutations"))
            scene.mutations.push_back(varToMutation(item));

        scene.nextSceneId = JsonHelpers::getString(value, "nextSceneId");
        scene.transitionStyle = JsonHelpers::getString(value, "transitionStyle", "hard");
        scene.rampBars = JsonHelpers::getInt(value, "rampBars", 0);

        return scene;
    }

    juce::var instancesToVar(const std::vector<Instance>& instances)
    {
        juce::Array<juce::var> array;
        for (const auto& instance : instances)
            array.add(instanceToVar(instance));
        return juce::var(array);
    }

    std::vector<Instance> varToInstances(const juce::var& value)
    {
        std::vector<Instance> instances;
        if (auto* array = value.getArray())
            for (const auto& item : *array)
                instances.push_back(varToInstance(item));
        return instances;
    }

    juce::var scenesToVar(const std::vector<Scene>& scenes)
    {
        juce::Array<juce::var> array;
        for (const auto& scene : scenes)
            array.add(sceneToVar(scene));
        return juce::var(array);
    }

    std::vector<Scene> varToScenes(const juce::var& value)
    {
        std::vector<Scene> scenes;
        if (auto* array = value.getArray())
            for (const auto& item : *array)
                scenes.push_back(varToScene(item));
        return scenes;
    }

    juce::var sectionLayerRoleToVar(const SectionLayerRole& layerRole)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("targetInstance", juce::String(layerRole.targetInstance));
        obj->setProperty("layerRole", juce::String(layerRole.layerRole));
        return juce::var(obj);
    }

    SectionLayerRole varToSectionLayerRole(const juce::var& value)
    {
        SectionLayerRole layerRole;
        layerRole.targetInstance = JsonHelpers::getString(value, "targetInstance");
        layerRole.layerRole = JsonHelpers::getString(value, "layerRole");
        return layerRole;
    }

    juce::var sectionBudgetOverrideToVar(const SectionBudgetOverride& budgetOverride)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("role", juce::String(budgetOverride.role));
        obj->setProperty("maxMinorPerBar", budgetOverride.maxMinorPerBar);
        obj->setProperty("maxMediumPerBar", budgetOverride.maxMediumPerBar);
        obj->setProperty("maxMajorPerBar", budgetOverride.maxMajorPerBar);
        return juce::var(obj);
    }

    SectionBudgetOverride varToSectionBudgetOverride(const juce::var& value)
    {
        SectionBudgetOverride budgetOverride;
        budgetOverride.role = JsonHelpers::getString(value, "role");
        budgetOverride.maxMinorPerBar = JsonHelpers::getInt(value, "maxMinorPerBar", -1);
        budgetOverride.maxMediumPerBar = JsonHelpers::getInt(value, "maxMediumPerBar", -1);
        budgetOverride.maxMajorPerBar = JsonHelpers::getInt(value, "maxMajorPerBar", -1);
        return budgetOverride;
    }

    juce::var reservedValueToVar(const ReservedValue& reservedValue)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("targetInstance", juce::String(reservedValue.targetInstance));
        obj->setProperty("patternIndex", reservedValue.patternIndex);
        obj->setProperty("type", juce::String(reservedValue.type));
        obj->setProperty("value", reservedValue.value);
        return juce::var(obj);
    }

    ReservedValue varToReservedValue(const juce::var& value)
    {
        ReservedValue reservedValue;
        reservedValue.targetInstance = JsonHelpers::getString(value, "targetInstance");
        reservedValue.patternIndex = JsonHelpers::getInt(value, "patternIndex", 0);
        reservedValue.type = JsonHelpers::getString(value, "type");
        reservedValue.value = JsonHelpers::getInt(value, "value", 0);
        return reservedValue;
    }

    juce::var sectionModulatorValueToVar(const SectionModulatorValue& modulatorValue)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("modulatorTargetId", juce::String(modulatorValue.modulatorTargetId));
        obj->setProperty("value", modulatorValue.value);
        return juce::var(obj);
    }

    SectionModulatorValue varToSectionModulatorValue(const juce::var& value)
    {
        SectionModulatorValue modulatorValue;
        modulatorValue.modulatorTargetId = JsonHelpers::getString(value, "modulatorTargetId");
        modulatorValue.value = JsonHelpers::getInt(value, "value", 0);
        return modulatorValue;
    }

    juce::var stepSnapshotToVar(const StepSnapshot& step)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("enabled", step.enabled);
        obj->setProperty("note", step.note);
        obj->setProperty("velocity", step.velocity);
        obj->setProperty("duration", step.duration);
        return juce::var(obj);
    }

    StepSnapshot varToStepSnapshot(const juce::var& value)
    {
        StepSnapshot step;
        step.enabled = JsonHelpers::getBool(value, "enabled", false);
        step.note = JsonHelpers::getInt(value, "note", 0);
        step.velocity = JsonHelpers::getInt(value, "velocity", 0);
        step.duration = JsonHelpers::getInt(value, "duration", 0);
        return step;
    }

    juce::var sectionCapturedContentToVar(const SectionCapturedContent& capturedContent)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("targetInstance", juce::String(capturedContent.targetInstance));
        obj->setProperty("patternIndex", capturedContent.patternIndex);

        juce::Array<juce::var> steps;
        for (const auto& step : capturedContent.steps)
            steps.add(stepSnapshotToVar(step));
        obj->setProperty("steps", steps);

        return juce::var(obj);
    }

    SectionCapturedContent varToSectionCapturedContent(const juce::var& value)
    {
        SectionCapturedContent capturedContent;
        capturedContent.targetInstance = JsonHelpers::getString(value, "targetInstance");
        capturedContent.patternIndex = JsonHelpers::getInt(value, "patternIndex", 0);

        for (const auto& item : JsonHelpers::getArray(value, "steps"))
            capturedContent.steps.push_back(varToStepSnapshot(item));

        return capturedContent;
    }

    juce::var blueprintSectionToVar(const BlueprintSection& section)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("id", juce::String(section.id));
        obj->setProperty("name", juce::String(section.name));
        obj->setProperty("sceneId", juce::String(section.sceneId));
        obj->setProperty("startBar", section.startBar);
        obj->setProperty("durationBars", section.durationBars);
        obj->setProperty("archetype", juce::String(section.archetype));

        juce::Array<juce::var> layerRoles;
        for (const auto& layerRole : section.layerRoles)
            layerRoles.add(sectionLayerRoleToVar(layerRole));
        obj->setProperty("layerRoles", layerRoles);

        juce::Array<juce::var> budgetOverrides;
        for (const auto& budgetOverride : section.budgetOverrides)
            budgetOverrides.add(sectionBudgetOverrideToVar(budgetOverride));
        obj->setProperty("budgetOverrides", budgetOverrides);

        juce::Array<juce::var> reservedValues;
        for (const auto& reservedValue : section.reservedValues)
            reservedValues.add(reservedValueToVar(reservedValue));
        obj->setProperty("reservedValues", reservedValues);

        juce::Array<juce::var> modulatorValues;
        for (const auto& modulatorValue : section.modulatorValues)
            modulatorValues.add(sectionModulatorValueToVar(modulatorValue));
        obj->setProperty("modulatorValues", modulatorValues);

        juce::Array<juce::var> capturedContent;
        for (const auto& entry : section.capturedContent)
            capturedContent.add(sectionCapturedContentToVar(entry));
        obj->setProperty("capturedContent", capturedContent);

        return juce::var(obj);
    }

    BlueprintSection varToBlueprintSection(const juce::var& value)
    {
        BlueprintSection section;
        section.id = JsonHelpers::getString(value, "id");
        section.name = JsonHelpers::getString(value, "name");
        section.sceneId = JsonHelpers::getString(value, "sceneId");
        section.startBar = JsonHelpers::getInt(value, "startBar", 0);
        section.durationBars = JsonHelpers::getInt(value, "durationBars", 4);
        section.archetype = JsonHelpers::getString(value, "archetype");

        for (const auto& item : JsonHelpers::getArray(value, "layerRoles"))
            section.layerRoles.push_back(varToSectionLayerRole(item));

        for (const auto& item : JsonHelpers::getArray(value, "budgetOverrides"))
            section.budgetOverrides.push_back(varToSectionBudgetOverride(item));

        for (const auto& item : JsonHelpers::getArray(value, "reservedValues"))
            section.reservedValues.push_back(varToReservedValue(item));

        for (const auto& item : JsonHelpers::getArray(value, "modulatorValues"))
            section.modulatorValues.push_back(varToSectionModulatorValue(item));

        for (const auto& item : JsonHelpers::getArray(value, "capturedContent"))
            section.capturedContent.push_back(varToSectionCapturedContent(item));

        return section;
    }

    juce::var blueprintArcPointToVar(const BlueprintArcPoint& point)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("bar", point.bar);
        obj->setProperty("value", point.value);
        return juce::var(obj);
    }

    BlueprintArcPoint varToBlueprintArcPoint(const juce::var& value)
    {
        BlueprintArcPoint point;
        point.bar = JsonHelpers::getInt(value, "bar", 0);
        point.value = JsonHelpers::getFloat(value, "value", 0.0f);
        return point;
    }

    juce::var blueprintArcCurveToVar(const BlueprintArcCurve& curve)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("dimension", juce::String(curve.dimension));

        juce::Array<juce::var> points;
        for (const auto& point : curve.points)
            points.add(blueprintArcPointToVar(point));
        obj->setProperty("points", points);

        return juce::var(obj);
    }

    BlueprintArcCurve varToBlueprintArcCurve(const juce::var& value)
    {
        BlueprintArcCurve curve;
        curve.dimension = JsonHelpers::getString(value, "dimension");

        for (const auto& item : JsonHelpers::getArray(value, "points"))
            curve.points.push_back(varToBlueprintArcPoint(item));

        return curve;
    }

    juce::var blueprintToVar(const Blueprint& blueprint)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("schema", "ComposerMastermindBlueprint.v1");
        obj->setProperty("id", juce::String(blueprint.id));
        obj->setProperty("name", juce::String(blueprint.name));

        juce::Array<juce::var> sections;
        for (const auto& section : blueprint.sections)
            sections.add(blueprintSectionToVar(section));
        obj->setProperty("sections", sections);

        juce::Array<juce::var> arcCurves;
        for (const auto& curve : blueprint.arcCurves)
            arcCurves.add(blueprintArcCurveToVar(curve));
        obj->setProperty("arcCurves", arcCurves);

        return juce::var(obj);
    }

    Blueprint varToBlueprint(const juce::var& value)
    {
        Blueprint blueprint;
        blueprint.id = JsonHelpers::getString(value, "id");
        blueprint.name = JsonHelpers::getString(value, "name");

        for (const auto& item : JsonHelpers::getArray(value, "sections"))
            blueprint.sections.push_back(varToBlueprintSection(item));

        for (const auto& item : JsonHelpers::getArray(value, "arcCurves"))
            blueprint.arcCurves.push_back(varToBlueprintArcCurve(item));

        return blueprint;
    }

    juce::var blueprintsToVar(const std::vector<Blueprint>& blueprints)
    {
        juce::Array<juce::var> array;
        for (const auto& blueprint : blueprints)
            array.add(blueprintToVar(blueprint));
        return juce::var(array);
    }

    std::vector<Blueprint> varToBlueprints(const juce::var& value)
    {
        std::vector<Blueprint> blueprints;
        if (auto* array = value.getArray())
            for (const auto& item : *array)
                blueprints.push_back(varToBlueprint(item));
        return blueprints;
    }

    juce::var rolePresetToVar(const RolePreset& preset)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("schema", "ComposerMastermindRolePreset.v1");
        obj->setProperty("id", juce::String(preset.id));
        obj->setProperty("name", juce::String(preset.name));
        obj->setProperty("targetRole", juce::String(preset.targetRole));

        juce::Array<juce::var> tags;
        for (const auto& tag : preset.tags)
            tags.add(juce::String(tag));
        obj->setProperty("tags", tags);

        obj->setProperty("activePattern", preset.activePattern);
        obj->setProperty("gridMode", preset.gridMode);
        obj->setProperty("swing", preset.swing);

        return juce::var(obj);
    }

    RolePreset varToRolePreset(const juce::var& value)
    {
        RolePreset preset;
        preset.id = JsonHelpers::getString(value, "id");
        preset.name = JsonHelpers::getString(value, "name");
        preset.targetRole = JsonHelpers::getString(value, "targetRole");

        for (const auto& tag : JsonHelpers::getArray(value, "tags"))
            preset.tags.push_back(tag.toString().toStdString());

        preset.activePattern = JsonHelpers::getInt(value, "activePattern", -1);
        preset.gridMode = JsonHelpers::getInt(value, "gridMode", -1);
        preset.swing = JsonHelpers::getFloat(value, "swing", -1.0f);

        return preset;
    }

    juce::var rolePresetsToVar(const std::vector<RolePreset>& presets)
    {
        juce::Array<juce::var> array;
        for (const auto& preset : presets)
            array.add(rolePresetToVar(preset));
        return juce::var(array);
    }

    std::vector<RolePreset> varToRolePresets(const juce::var& value)
    {
        std::vector<RolePreset> presets;
        if (auto* array = value.getArray())
            for (const auto& item : *array)
                presets.push_back(varToRolePreset(item));
        return presets;
    }

    juce::var rhythmicRelationshipRoleSlotToVar(const RhythmicRelationshipRoleSlot& slot)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("targetRole", juce::String(slot.targetRole));
        obj->setProperty("activePattern", slot.activePattern);
        obj->setProperty("gridMode", slot.gridMode);
        obj->setProperty("swing", slot.swing);
        return juce::var(obj);
    }

    RhythmicRelationshipRoleSlot varToRhythmicRelationshipRoleSlot(const juce::var& value)
    {
        RhythmicRelationshipRoleSlot slot;
        slot.targetRole = JsonHelpers::getString(value, "targetRole");
        slot.activePattern = JsonHelpers::getInt(value, "activePattern", -1);
        slot.gridMode = JsonHelpers::getInt(value, "gridMode", -1);
        slot.swing = JsonHelpers::getFloat(value, "swing", -1.0f);
        return slot;
    }

    juce::var rhythmicRelationshipPresetToVar(const RhythmicRelationshipPreset& preset)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("schema", "ComposerMastermindRhythmicRelationshipPreset.v1");
        obj->setProperty("id", juce::String(preset.id));
        obj->setProperty("name", juce::String(preset.name));

        juce::Array<juce::var> tags;
        for (const auto& tag : preset.tags)
            tags.add(juce::String(tag));
        obj->setProperty("tags", tags);

        juce::Array<juce::var> roleSlots;
        for (const auto& slot : preset.roleSlots)
            roleSlots.add(rhythmicRelationshipRoleSlotToVar(slot));
        obj->setProperty("roleSlots", roleSlots);

        return juce::var(obj);
    }

    RhythmicRelationshipPreset varToRhythmicRelationshipPreset(const juce::var& value)
    {
        RhythmicRelationshipPreset preset;
        preset.id = JsonHelpers::getString(value, "id");
        preset.name = JsonHelpers::getString(value, "name");

        for (const auto& tag : JsonHelpers::getArray(value, "tags"))
            preset.tags.push_back(tag.toString().toStdString());

        for (const auto& item : JsonHelpers::getArray(value, "roleSlots"))
            preset.roleSlots.push_back(varToRhythmicRelationshipRoleSlot(item));

        return preset;
    }

    juce::var rhythmicRelationshipPresetsToVar(const std::vector<RhythmicRelationshipPreset>& presets)
    {
        juce::Array<juce::var> array;
        for (const auto& preset : presets)
            array.add(rhythmicRelationshipPresetToVar(preset));
        return juce::var(array);
    }

    std::vector<RhythmicRelationshipPreset> varToRhythmicRelationshipPresets(const juce::var& value)
    {
        std::vector<RhythmicRelationshipPreset> presets;
        if (auto* array = value.getArray())
            for (const auto& item : *array)
                presets.push_back(varToRhythmicRelationshipPreset(item));
        return presets;
    }

    juce::var arcPresetBreakpointToVar(const ArcPresetBreakpoint& breakpoint)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("position", breakpoint.position);
        obj->setProperty("value", breakpoint.value);
        return juce::var(obj);
    }

    ArcPresetBreakpoint varToArcPresetBreakpoint(const juce::var& value)
    {
        ArcPresetBreakpoint breakpoint;
        breakpoint.position = JsonHelpers::getFloat(value, "position", 0.0f);
        breakpoint.value = JsonHelpers::getFloat(value, "value", 0.0f);
        return breakpoint;
    }

    juce::var arcPresetToVar(const ArcPreset& preset)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("schema", "ComposerMastermindArcPreset.v1");
        obj->setProperty("id", juce::String(preset.id));
        obj->setProperty("name", juce::String(preset.name));

        juce::Array<juce::var> tags;
        for (const auto& tag : preset.tags)
            tags.add(juce::String(tag));
        obj->setProperty("tags", tags);

        juce::Array<juce::var> breakpoints;
        for (const auto& breakpoint : preset.breakpoints)
            breakpoints.add(arcPresetBreakpointToVar(breakpoint));
        obj->setProperty("breakpoints", breakpoints);

        return juce::var(obj);
    }

    ArcPreset varToArcPreset(const juce::var& value)
    {
        ArcPreset preset;
        preset.id = JsonHelpers::getString(value, "id");
        preset.name = JsonHelpers::getString(value, "name");

        for (const auto& tag : JsonHelpers::getArray(value, "tags"))
            preset.tags.push_back(tag.toString().toStdString());

        for (const auto& item : JsonHelpers::getArray(value, "breakpoints"))
            preset.breakpoints.push_back(varToArcPresetBreakpoint(item));

        return preset;
    }

    juce::var arcPresetsToVar(const std::vector<ArcPreset>& presets)
    {
        juce::Array<juce::var> array;
        for (const auto& preset : presets)
            array.add(arcPresetToVar(preset));
        return juce::var(array);
    }

    std::vector<ArcPreset> varToArcPresets(const juce::var& value)
    {
        std::vector<ArcPreset> presets;
        if (auto* array = value.getArray())
            for (const auto& item : *array)
                presets.push_back(varToArcPreset(item));
        return presets;
    }

    juce::var motifNoteToVar(const MotifNote& note)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("semitoneOffset", note.semitoneOffset);
        obj->setProperty("relativeDuration", note.relativeDuration);
        obj->setProperty("relativeVelocity", note.relativeVelocity);
        return juce::var(obj);
    }

    MotifNote varToMotifNote(const juce::var& value)
    {
        MotifNote note;
        note.semitoneOffset = JsonHelpers::getInt(value, "semitoneOffset", 0);
        note.relativeDuration = JsonHelpers::getFloat(value, "relativeDuration", 1.0f);
        note.relativeVelocity = JsonHelpers::getFloat(value, "relativeVelocity", 1.0f);
        return note;
    }

    juce::var motifPresetToVar(const MotifPreset& preset)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("schema", "ComposerMastermindMotifPreset.v1");
        obj->setProperty("id", juce::String(preset.id));
        obj->setProperty("name", juce::String(preset.name));

        juce::Array<juce::var> tags;
        for (const auto& tag : preset.tags)
            tags.add(juce::String(tag));
        obj->setProperty("tags", tags);

        juce::Array<juce::var> notes;
        for (const auto& note : preset.notes)
            notes.add(motifNoteToVar(note));
        obj->setProperty("notes", notes);

        return juce::var(obj);
    }

    MotifPreset varToMotifPreset(const juce::var& value)
    {
        MotifPreset preset;
        preset.id = JsonHelpers::getString(value, "id");
        preset.name = JsonHelpers::getString(value, "name");

        for (const auto& tag : JsonHelpers::getArray(value, "tags"))
            preset.tags.push_back(tag.toString().toStdString());

        for (const auto& item : JsonHelpers::getArray(value, "notes"))
            preset.notes.push_back(varToMotifNote(item));

        return preset;
    }

    juce::var motifPresetsToVar(const std::vector<MotifPreset>& presets)
    {
        juce::Array<juce::var> array;
        for (const auto& preset : presets)
            array.add(motifPresetToVar(preset));
        return juce::var(array);
    }

    std::vector<MotifPreset> varToMotifPresets(const juce::var& value)
    {
        std::vector<MotifPreset> presets;
        if (auto* array = value.getArray())
            for (const auto& item : *array)
                presets.push_back(varToMotifPreset(item));
        return presets;
    }

    juce::var modulatorTargetToVar(const ModulatorTarget& target)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("schema", "ComposerMastermindModulatorTarget.v1");
        obj->setProperty("id", juce::String(target.id));
        obj->setProperty("ccNumber", target.ccNumber);
        obj->setProperty("midiChannel", target.midiChannel);
        obj->setProperty("mode", juce::String(target.mode));
        obj->setProperty("arcDimension", juce::String(target.arcDimension));
        return juce::var(obj);
    }

    ModulatorTarget varToModulatorTarget(const juce::var& value)
    {
        ModulatorTarget target;
        target.id = JsonHelpers::getString(value, "id");
        target.ccNumber = JsonHelpers::getInt(value, "ccNumber", 70);
        target.midiChannel = JsonHelpers::getInt(value, "midiChannel", 1);
        target.mode = JsonHelpers::getString(value, "mode", "arc");
        target.arcDimension = JsonHelpers::getString(value, "arcDimension");
        return target;
    }

    juce::var modulatorTargetsToVar(const std::vector<ModulatorTarget>& targets)
    {
        juce::Array<juce::var> array;
        for (const auto& target : targets)
            array.add(modulatorTargetToVar(target));
        return juce::var(array);
    }

    std::vector<ModulatorTarget> varToModulatorTargets(const juce::var& value)
    {
        std::vector<ModulatorTarget> targets;
        if (auto* array = value.getArray())
            for (const auto& item : *array)
                targets.push_back(varToModulatorTarget(item));
        return targets;
    }
}
