#include "BlueprintGenerator.h"
#include "PresetResolver.h"
#include "NarrativeLaneSuggester.h"
#include <algorithm>

namespace
{
    using namespace BlueprintGenerator;

    struct SectionPlan
    {
        int startBar = 0;
        int endBar = 0;
        float startValue = 0.0f;
        float endValue = 0.0f;
    };

    constexpr float kRiseThreshold = 0.05f;

    // Each consecutive pair of breakpoints on the driving arc becomes one
    // section - draw the shape once, the structure falls out of it.
    std::vector<SectionPlan> buildSectionPlans(const std::vector<ArcBreakpoint>& breakpoints)
    {
        std::vector<SectionPlan> plans;

        if (breakpoints.size() < 2)
            return plans;

        for (size_t i = 0; i + 1 < breakpoints.size(); ++i)
        {
            SectionPlan plan;
            plan.startBar = breakpoints[i].bar;
            plan.endBar = breakpoints[i + 1].bar;
            plan.startValue = breakpoints[i].value;
            plan.endValue = breakpoints[i + 1].value;
            plans.push_back(plan);
        }

        return plans;
    }

    // Exactly one section is Peak: the one ending at the curve's global
    // maximum, or the very first section if the curve already opens at its
    // own maximum (a curve that only ever falls). Everything before it
    // is Build (rising more than kRiseThreshold) or Presentation
    // (flat/establishing); everything after is Release.
    std::vector<Archetype> classifyArchetypes(const std::vector<SectionPlan>& plans)
    {
        size_t peakSectionIndex = 0;
        float peakValue = plans[0].startValue;

        for (size_t i = 0; i < plans.size(); ++i)
        {
            if (plans[i].endValue > peakValue)
            {
                peakValue = plans[i].endValue;
                peakSectionIndex = i;
            }
        }

        std::vector<Archetype> archetypes(plans.size(), Archetype::Presentation);

        for (size_t i = 0; i < plans.size(); ++i)
        {
            if (i == peakSectionIndex)
            {
                archetypes[i] = Archetype::Peak;
            }
            else if (i < peakSectionIndex)
            {
                const float rise = plans[i].endValue - plans[i].startValue;
                archetypes[i] = (rise > kRiseThreshold) ? Archetype::Build : Archetype::Presentation;
            }
            else
            {
                archetypes[i] = Archetype::Release;
            }
        }

        return archetypes;
    }

    // Deterministic per-archetype lookup - no randomness, no model. Reuses
    // every mechanism built this session: layer roles (background->stopped,
    // see ComposerCore::applyLayerRoleOverrides), budget overrides (see
    // ComposerCore::resolveSectionBudgetOverride), and reserved values
    // (apex exclusivity, see ComposerCore::isValueReservedByLaterSection).
    void applyArchetypeRules(Archetype archetype, const std::vector<Instance>& allInstances, BlueprintSection& section)
    {
        switch (archetype)
        {
            case Archetype::Presentation:
                // No forced layering or overrides - let the static role
                // budgets and the scene's own authored values speak for
                // themselves.
                break;

            case Archetype::Build:
            {
                // Loosen motif/counterpoint so the piece can visibly gather
                // energy; anchor is left untouched - its "rare by design"
                // character (docs/mutation_policy_v0_1.md) shouldn't erode
                // just because things are building toward the peak.
                SectionBudgetOverride motifOverride;
                motifOverride.role = "motif";
                motifOverride.maxMediumPerBar = 2;
                motifOverride.maxMajorPerBar = 1;
                section.budgetOverrides.push_back(motifOverride);

                SectionBudgetOverride counterpointOverride;
                counterpointOverride.role = "counterpoint";
                counterpointOverride.maxMediumPerBar = 3;
                counterpointOverride.maxMajorPerBar = 2;
                section.budgetOverrides.push_back(counterpointOverride);
                break;
            }

            case Archetype::Peak:
            {
                // "climax_all_active" - every registered instance goes
                // foreground, and reserves its own inversion=on so no
                // earlier section can spend it first. Budgets loosen for
                // everyone, including anchor - the one section where even
                // the steadiest layer is allowed to move.
                for (const auto& instance : allInstances)
                {
                    SectionLayerRole layerRole;
                    layerRole.targetInstance = instance.id;
                    layerRole.layerRole = "foreground";
                    section.layerRoles.push_back(layerRole);

                    ReservedValue reserved;
                    reserved.targetInstance = instance.id;
                    reserved.patternIndex = 0;
                    reserved.type = "inversion";
                    reserved.value = 1;
                    section.reservedValues.push_back(reserved);
                }

                SectionBudgetOverride anchorOverride;
                anchorOverride.role = "anchor";
                anchorOverride.maxMediumPerBar = 1;
                anchorOverride.maxMajorPerBar = 1;
                section.budgetOverrides.push_back(anchorOverride);

                SectionBudgetOverride motifOverride;
                motifOverride.role = "motif";
                motifOverride.maxMajorPerBar = 2;
                section.budgetOverrides.push_back(motifOverride);

                SectionBudgetOverride counterpointOverride;
                counterpointOverride.role = "counterpoint";
                counterpointOverride.maxMajorPerBar = 2;
                section.budgetOverrides.push_back(counterpointOverride);
                break;
            }

            case Archetype::Release:
            {
                // Thin the texture out: counterpoint (the "development"
                // voice) drops to background. Budgets are left at their
                // static defaults - nothing to loosen on the way down.
                for (const auto& instance : allInstances)
                {
                    if (instance.role != "counterpoint")
                        continue;

                    SectionLayerRole layerRole;
                    layerRole.targetInstance = instance.id;
                    layerRole.layerRole = "background";
                    section.layerRoles.push_back(layerRole);
                }
                break;
            }
        }
    }

    // A preset matches a section's archetype if its tags include the
    // archetype name, OR (fallback, for a preset the author simply named
    // after the archetype - e.g. id "peak" - without separately tagging
    // it) its own id equals the archetype name. The tags path is checked
    // first and is the documented mechanism; the id fallback exists
    // because naming a preset "peak" is the intuitive first thing an
    // author tries, and there's no reason to require both.
    bool presetMatchesArchetype(const std::string& presetId, const std::vector<std::string>& tags,
                                 const std::string& archetypeTag)
    {
        if (std::find(tags.begin(), tags.end(), archetypeTag) != tags.end())
            return true;

        return presetId == archetypeTag;
    }

    // Clones the base scene, applies every role/rhythmic-relationship
    // preset that matches this section's archetype (by tag, or by id as a
    // fallback - see presetMatchesArchetype).
    Scene buildSectionScene(const std::string& sceneId, const Scene& baseScene, const std::string& archetypeTag,
                             const std::vector<Instance>& allInstances,
                             const std::vector<RolePreset>& rolePresets,
                             const std::vector<RhythmicRelationshipPreset>& rhythmicPresets)
    {
        Scene scene = baseScene;
        scene.id = sceneId;
        scene.name = sceneId;
        scene.nextSceneId.clear(); // sequenced by the blueprint's own sections, not a hand-authored chain
        scene.durationBars = 4; // not consulted by blueprint-driven playback; kept only so Validation::isValidScene passes

        for (const auto& preset : rolePresets)
            if (presetMatchesArchetype(preset.id, preset.tags, archetypeTag))
                PresetResolver::applyRolePreset(preset, allInstances, scene);

        for (const auto& preset : rhythmicPresets)
            if (presetMatchesArchetype(preset.id, preset.tags, archetypeTag))
                PresetResolver::applyRhythmicRelationshipPreset(preset, allInstances, scene);

        return scene;
    }

    // Baseline values per archetype for the dimensions that aren't computed
    // directly from the section's own decisions - simple and explainable
    // on purpose, not a precise musical model. See density below for the
    // one dimension baked directly from an actual decision instead.
    float tensionBaseline(Archetype archetype)
    {
        switch (archetype)
        {
            case Archetype::Presentation: return 0.2f;
            case Archetype::Build:        return 0.5f;
            case Archetype::Peak:         return 0.9f;
            case Archetype::Release:      return 0.35f;
        }
        return 0.2f;
    }

    float complexityBaseline(Archetype archetype)
    {
        switch (archetype)
        {
            case Archetype::Presentation: return 0.2f;
            case Archetype::Build:        return 0.5f;
            case Archetype::Peak:         return 0.8f;
            case Archetype::Release:      return 0.3f;
        }
        return 0.2f;
    }

    float coherenceBaseline(Archetype archetype)
    {
        switch (archetype)
        {
            case Archetype::Presentation: return 0.6f;
            case Archetype::Build:        return 0.5f;
            case Archetype::Peak:         return 0.8f;
            case Archetype::Release:      return 0.45f;
        }
        return 0.5f;
    }

    float densityBaseline(Archetype archetype)
    {
        switch (archetype)
        {
            case Archetype::Presentation: return 0.3f;
            case Archetype::Build:        return 0.5f;
            case Archetype::Peak:         return 0.9f;
            case Archetype::Release:      return 0.4f;
        }
        return 0.3f;
    }

    // Only reached by deriveArcSetFromSections - generate() never calls
    // derivedValue for "energy" itself, since energy is normally the driving
    // dimension (its curve is the human-authored input, not a derived
    // output). A blueprint with no driving arc at all (the hand-authored
    // case) still needs a sensible baseline for it.
    float energyBaseline(Archetype archetype)
    {
        switch (archetype)
        {
            case Archetype::Presentation: return 0.3f;
            case Archetype::Build:        return 0.6f;
            case Archetype::Peak:         return 1.0f;
            case Archetype::Release:      return 0.3f;
        }
        return 0.3f;
    }

    // Inverse of archetypeName - an unrecognized or empty string (a section
    // with no archetype assigned) falls back to Presentation, the calmest/
    // flattest option, matching how the motif engine already treats an
    // empty archetype as "leave this section untouched."
    Archetype archetypeFromName(const std::string& name)
    {
        if (name == "build")   return Archetype::Build;
        if (name == "peak")    return Archetype::Peak;
        if (name == "release") return Archetype::Release;
        return Archetype::Presentation;
    }

    // Density is the one dimension baked directly from an actual decision
    // rather than a flat baseline: the fraction of registered instances
    // this section did NOT background (see applyArchetypeRules's
    // "background" layer roles) - a real readout of how full the texture
    // actually is, not decoration.
    float derivedValue(const std::string& dimensionName, Archetype archetype, const BlueprintSection& section,
                        const std::vector<Instance>& allInstances)
    {
        if (dimensionName == "density")
        {
            if (allInstances.empty())
                return densityBaseline(archetype);

            int backgroundCount = 0;
            for (const auto& layerRole : section.layerRoles)
                if (layerRole.layerRole == "background")
                    ++backgroundCount;

            return 1.0f - (static_cast<float>(backgroundCount) / static_cast<float>(allInstances.size()));
        }

        if (dimensionName == "tension")
            return tensionBaseline(archetype);

        if (dimensionName == "complexity")
            return complexityBaseline(archetype);

        if (dimensionName == "coherence")
            return coherenceBaseline(archetype);

        if (dimensionName == "energy")
            return energyBaseline(archetype);

        return 0.3f; // unreachable for the 5 known dimension names
    }

    void bakeDerivedArcs(const std::vector<SectionPlan>& plans, const std::vector<Archetype>& archetypes,
                          const std::vector<BlueprintSection>& sections, const std::string& drivingArcName,
                          const std::vector<Instance>& allInstances, ArcSet& candidateArcSet)
    {
        for (const auto* dimensionNameLiteral : { "energy", "tension", "density", "complexity", "coherence" })
        {
            const std::string dimensionName = dimensionNameLiteral;
            if (dimensionName == drivingArcName)
                continue; // untouched - this is the human-authored input, not a derived output

            std::vector<ArcBreakpoint> breakpoints;
            for (size_t i = 0; i < plans.size(); ++i)
                breakpoints.push_back({ plans[i].startBar, derivedValue(dimensionName, archetypes[i], sections[i], allInstances) });

            breakpoints.push_back({ plans.back().endBar,
                                     derivedValue(dimensionName, archetypes.back(), sections.back(), allInstances) });

            Arc arc;
            arc.setBreakpoints(breakpoints);
            candidateArcSet.setArc(dimensionName, arc);
        }
    }
}

namespace BlueprintGenerator
{
    std::string archetypeName(Archetype archetype)
    {
        switch (archetype)
        {
            case Archetype::Presentation: return "presentation";
            case Archetype::Build:        return "build";
            case Archetype::Peak:         return "peak";
            case Archetype::Release:      return "release";
        }
        return "presentation";
    }

    Scene generateSeedScene(const std::string& sceneId,
                             const std::vector<Instance>& allInstances,
                             const std::vector<RolePreset>& /*rolePresets*/)
    {
        Scene scene;
        scene.id = sceneId;
        scene.name = sceneId;

        int index = 0;
        for (const auto& instance : allInstances)
        {
            scene.targets.push_back(instance.id);

            ScenePattern pattern;
            pattern.targetInstance = instance.id;
            pattern.patternIndex = 0;
            pattern.rotation = (index * 3) % 16; // deterministic, not raw random - see header comment
            scene.patterns.push_back(pattern);

            ++index;
        }

        return scene;
    }

    GeneratedBlueprintProposal generate(const std::string& blueprintId,
                                         const std::string& drivingArcName,
                                         const ArcSet& liveArcSet,
                                         const Scene& baseScene,
                                         const std::vector<Instance>& allInstances,
                                         const std::vector<RolePreset>& rolePresets,
                                         const std::vector<RhythmicRelationshipPreset>& rhythmicPresets)
    {
        GeneratedBlueprintProposal proposal;
        proposal.candidateArcSet = liveArcSet; // seed with a full copy so untouched dimensions/context survive

        const auto drivingArc = liveArcSet.getArc(drivingArcName);
        const auto plans = buildSectionPlans(drivingArc.getBreakpoints());

        if (plans.empty())
            return proposal; // blueprint.id stays empty - caller treats this as "could not generate"

        const auto archetypes = classifyArchetypes(plans);

        proposal.blueprint.id = blueprintId;
        proposal.blueprint.name = blueprintId;

        for (size_t i = 0; i < plans.size(); ++i)
        {
            const auto& plan = plans[i];
            const auto archetype = archetypes[i];
            const auto tag = archetypeName(archetype);

            BlueprintSection section;
            section.id = blueprintId + "_section" + std::to_string(i + 1);
            section.name = section.id + "_" + tag;
            section.startBar = plan.startBar;
            section.durationBars = plan.endBar - plan.startBar;
            section.sceneId = section.id + "_scene";
            section.archetype = tag;
            section.suggestedNarrativeLane = NarrativeLaneSuggester::suggestLane(
                tag, derivedValue("energy", archetype, section, allInstances),
                derivedValue("tension", archetype, section, allInstances));

            applyArchetypeRules(archetype, allInstances, section);

            proposal.newScenes.push_back(
                buildSectionScene(section.sceneId, baseScene, tag, allInstances, rolePresets, rhythmicPresets));

            proposal.blueprint.sections.push_back(section);
        }

        bakeDerivedArcs(plans, archetypes, proposal.blueprint.sections, drivingArcName, allInstances,
                        proposal.candidateArcSet);

        return proposal;
    }

    ArcSet deriveArcSetFromSections(const std::vector<BlueprintSection>& sections,
                                     const std::vector<Instance>& allInstances)
    {
        ArcSet result; // default-constructed: flat 0.3 for every dimension, the "no sections" fallback

        if (sections.empty())
            return result;

        for (const auto* dimensionNameLiteral : { "energy", "tension", "density", "complexity", "coherence" })
        {
            const std::string dimensionName = dimensionNameLiteral;

            std::vector<ArcBreakpoint> breakpoints;
            for (const auto& section : sections)
            {
                const auto archetype = archetypeFromName(section.archetype);
                breakpoints.push_back({ section.startBar, derivedValue(dimensionName, archetype, section, allInstances) });
            }

            const auto& lastSection = sections.back();
            const auto lastArchetype = archetypeFromName(lastSection.archetype);
            breakpoints.push_back({ lastSection.startBar + lastSection.durationBars,
                                     derivedValue(dimensionName, lastArchetype, lastSection, allInstances) });

            Arc arc;
            arc.setBreakpoints(breakpoints);
            result.setArc(dimensionName, arc);
        }

        return result;
    }

    ArcSet resolveBlueprintArcSet(const Blueprint& blueprint, const std::vector<Instance>& allInstances,
                                   std::vector<std::string>& outDerivedDimensions)
    {
        ArcSet result = deriveArcSetFromSections(blueprint.sections, allInstances);

        std::vector<std::string> storedDimensions;
        for (const auto& curve : blueprint.arcCurves)
        {
            // A curve with zero saved points isn't real authored data - never
            // let it overwrite the (always non-empty) derived fallback above,
            // and don't count it as "stored" either. Without this guard, one
            // empty curve entry - however it got there - would silently blank
            // that dimension forever after, and keep propagating into every
            // future "Save as new blueprint" clone made from it.
            if (curve.points.empty())
                continue;

            std::vector<ArcBreakpoint> breakpoints;
            for (const auto& point : curve.points)
                breakpoints.push_back({ point.bar, point.value });

            Arc arc;
            arc.setBreakpoints(breakpoints);
            result.setArc(curve.dimension, arc);
            storedDimensions.push_back(curve.dimension);
        }

        outDerivedDimensions.clear();
        for (const auto* dimensionNameLiteral : { "energy", "tension", "density", "complexity", "coherence" })
        {
            const std::string dimensionName = dimensionNameLiteral;
            if (std::find(storedDimensions.begin(), storedDimensions.end(), dimensionName) == storedDimensions.end())
                outDerivedDimensions.push_back(dimensionName);
        }

        return result;
    }
}
