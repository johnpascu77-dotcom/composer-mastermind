#pragma once

#include <string>
#include <vector>
#include "Arc.h"
#include "ArcSet.h"
#include "../model/Instance.h"
#include "../model/Scene.h"
#include "../model/Blueprint.h"
#include "../model/Preset.h"

// The v1.1 "Generative Blueprint Proposal" entry point: assembles a
// candidate Blueprint from the existing preset library rather than
// requiring one fully hand-authored, per
// docs/composer_mastermind_design.md's v1.1 section. Deliberately not a
// model, not randomness ("randomness must be intentional, constrained by
// section/role/budget, not raw chance" - see the design doc's principles)
// - a small, fixed, inspectable rule table over one arc's own breakpoint
// shape. Pure/JUCE-free, same style as policy/SceneAdvancePolicy and
// policy/PresetResolver.
namespace BlueprintGenerator
{
    // One generated section's classified character, derived purely from
    // the driving arc's consecutive breakpoint values. Exactly one section
    // per generated blueprint is Peak (the one ending at - or starting at,
    // if the curve opens at its own maximum - the driving arc's global
    // maximum); everything before it is Presentation/Build, everything
    // after is Release. Mirrors the presentation/local_climax_approach/
    // global_climax/coda_echo_aftermath vocabulary mined from
    // atonal_phrase_engine (docs/atonal_phrase_engine_concepts.md),
    // finally implementable now that roles/budgets/arcs/presets all exist.
    enum class Archetype
    {
        Presentation,
        Build,
        Peak,
        Release
    };

    // Lowercase name, also the preset tag matched against when selecting
    // which library presets apply to a section of this archetype.
    std::string archetypeName(Archetype archetype);

    // The full result of one generation pass - entirely in-memory, no side
    // effects on any live state (BlueprintLibrary/SceneLibrary/the real
    // ArcSet are all untouched until the caller explicitly commits this).
    // See ui/GenerateView for the preview-before-commit flow this exists
    // for: candidateArcSet starts as a copy of the live ArcSet with the
    // driving dimension left exactly as authored and the other 4
    // dimensions' curves re-baked from the generated structure, so the
    // whole 5-curve picture stays visibly consistent with what was decided.
    struct GeneratedBlueprintProposal
    {
        Blueprint blueprint;
        std::vector<Scene> newScenes; // one per section, cloned from the base scene + tag-matched presets applied
        ArcSet candidateArcSet;
    };

    // Pure: takes copies/const-refs of everything it reads and returns a
    // full proposal with no side effects. If drivingArcName's arc has
    // fewer than 2 breakpoints (no sections derivable, including an
    // unrecognized dimension name - ArcSet::getArc returns an empty
    // default Arc for those), returns a proposal with an empty
    // blueprint.id - callers treat that as "could not generate."
    GeneratedBlueprintProposal generate(const std::string& blueprintId,
                                         const std::string& drivingArcName,
                                         const ArcSet& liveArcSet,
                                         const Scene& baseScene,
                                         const std::vector<Instance>& allInstances,
                                         const std::vector<RolePreset>& rolePresets,
                                         const std::vector<RhythmicRelationshipPreset>& rhythmicPresets);

    // Generates the one seed Scene generate() needs as its baseScene input,
    // for the "Propose a Piece" flow (policy/DurationCalculator.h,
    // policy/ArcShapeLibrary.h) where there is no hand-authored scene to
    // start from at all. Closes the one gap this file's own top comment
    // already names - "assembles a candidate Blueprint from the existing
    // preset library rather than requiring one fully hand-authored" was
    // only ever true for the section layer; the seed scene itself still
    // had to be hand-built. Never invents Instances - only ever addresses
    // whichever ones are already in allInstances (which instances exist is
    // a human Bitwig-routing decision, never a generator's to make - the
    // same principle the sibling plugin OrchConductor's own design holds
    // for its note path). Targets every given instance with exactly one
    // ScenePattern (patternIndex 0, MPL's "home" pattern slot), a small
    // deterministic per-instance rotation offset (index * 3, wrapped to a
    // 16-step pattern) so instances don't all start in unison lockstep -
    // not a musical judgment call, just enough starting variety for
    // MotifEngine and the routing/Mutation pipeline to develop from.
    // Transpose/inversion/retrograde/m7 stay at their neutral defaults -
    // register and transform decisions are already this codebase's other
    // machinery's job (MotifEngine's home-register clamp, the Mutation/
    // Router/PolicyEngine budget pipeline), not this function's.
    // rolePresets is accepted for signature symmetry with generate() but
    // not consulted here - RolePreset only ever touches
    // instanceOverrides/targets (see policy/PresetResolver.h), which
    // buildSectionScene's own preset-matching pass already applies per
    // section after this seed is cloned.
    Scene generateSeedScene(const std::string& sceneId,
                             const std::vector<Instance>& allInstances,
                             const std::vector<RolePreset>& rolePresets);

    // Derives a full 5-dimension ArcSet directly from a blueprint's own
    // section/archetype sequence, for a blueprint that has no explicitly
    // authored arc data at all (e.g. hand-composed as explicit sections,
    // never generated) - the "curve-based blueprint authoring" UI's
    // fallback display curve. Reuses the exact same per-archetype baseline
    // table generate()'s bakeDerivedArcs uses for a generated blueprint's
    // non-driving dimensions (one flat value per section boundary; density
    // computed from actual layer-role decisions, not a table), just entered
    // from an existing section list instead of a driving arc's breakpoints.
    // A section with an empty/unrecognized archetype is treated as
    // Presentation. Pure/JUCE-free. Returns a default (flat 0.3) ArcSet for
    // an empty section list.
    ArcSet deriveArcSetFromSections(const std::vector<BlueprintSection>& sections,
                                     const std::vector<Instance>& allInstances);

    // Resolves the full ArcSet Composer Mastermind should treat as "this
    // blueprint's arc": any dimension the blueprint has explicitly saved
    // (Blueprint::arcCurves) is used verbatim; any dimension it hasn't gets
    // deriveArcSetFromSections's display curve instead. outDerivedDimensions
    // is filled with the names of dimensions that came from derivation, not
    // saved data - the authoring UI's "derived vs custom" indicator. This is
    // the single function both ComposerCore::setCurrentBlueprint (to sync
    // the live ArcSet on activation) and the curve-authoring UI (to preview
    // any blueprint, not just the active one) should call, so both stay in
    // agreement about what a blueprint's arc actually is.
    ArcSet resolveBlueprintArcSet(const Blueprint& blueprint, const std::vector<Instance>& allInstances,
                                   std::vector<std::string>& outDerivedDimensions);
}
