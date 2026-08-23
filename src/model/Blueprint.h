#pragma once

#include <string>
#include <vector>
#include "PatternSnapshot.h"

// Foreground/support/background prominence for one instance during one
// section. Consumed by ComposerCore::advanceBlueprintIfNeeded (2026-08-17):
// MPL's CC protocol has no volume/velocity control, so the only lever this
// can pull is whether an instance's pattern is engaged at all - "background"
// instances default to stopped (Active Pattern = 0) for the section unless
// the section's own scene already authored an explicit override for that
// instance, which always wins. "foreground"/"support" don't get a forced
// override - without a volume dimension there's no honest way to make them
// behave differently from each other at the protocol level, so this only
// gives "background" real teeth for now.
struct SectionLayerRole
{
    std::string targetInstance;
    std::string layerRole; // "foreground" / "support" / "background"
};

// Per-role override of MutationPolicy::budgetForRole's static table, active
// only while this section is playing. A role not listed here falls back to
// the global static default for that role - this only overrides roles the
// author explicitly wants different for this section (e.g. loosen anchor's
// budget during a climax section). -1 means unlimited, matching
// policy/MutationPolicy's RoleBudget sentinel. Authoring-only for now -
// PolicyEngine doesn't consult this yet, see docs/mutation_policy_v0_1.md.
struct SectionBudgetOverride
{
    std::string role;
    int maxMinorPerBar = -1;
    int maxMediumPerBar = -1;
    int maxMajorPerBar = -1;
};

// A fixed value (0..127, raw and CC-ready - see model/ModulatorTarget.h) for
// one ModulatorTarget while this section plays, sent once at the section
// boundary alongside its Scene, mirroring how the section's own scene is
// routed once on change (ComposerCore::advanceBlueprintIfNeeded). This is
// the "mode == section" half of ModulatorTarget; a target not listed here
// for a given section simply isn't sent when that section starts - it keeps
// whatever value it last held.
struct SectionModulatorValue
{
    std::string modulatorTargetId;
    int value = 0;
};

// A specific per-pattern parameter value "reserved" for this section: no
// earlier section (by startBar) is allowed to also reach this exact value
// for the same instance/pattern/parameter, so a later climax still reads as
// a genuine peak rather than something an earlier section already spent -
// the "apex exclusivity" clamping half described in
// docs/mutation_policy_v0_1.md, deferred since v0.5. `type` mirrors
// Mutation::type's vocabulary ("transpose"/"rotation"/"length"/"inversion");
// `value` is the resulting absolute value (not a delta) - for "inversion",
// 0 = off, nonzero = on, matching how Router::routeMutation already resolves
// a mutation's absolute resulting value before encoding it as CC.
struct ReservedValue
{
    std::string targetInstance;
    int patternIndex = 0;
    std::string type;
    int value = 0;
};

// Literal, verbatim step content for one instance/pattern, captured directly
// from what's actually on the instrument at capture time (see
// ui/BlueprintSectionsContent's "Capture Current" button) - the "Absolute"
// alternative to the motif engine's generative stamp for this section (see
// composer/ComposerCore.h's ContentMode). Always CCMapping::kPatternSteps
// (16) entries, matching StepSnapshot's own storage convention everywhere
// else in this codebase. Consumed by ComposerCore::enterSection: written
// verbatim via PatternSyncServer::sendWriteFullPattern, bypassing MotifEngine
// entirely for this instance/pattern, and the whole section is then exempt
// from every subsequent per-bar automatic modification (motif passes,
// phrase-chain, continuous melodic curve, continuous swing, rhythm
// mutations) for as long as it plays - deliberately frozen, matching a
// mechanical-piano-roll/demo-song reading of "load this file, hear exactly
// this every time," not just a seeded-then-left-to-drift starting point.
struct SectionCapturedContent
{
    std::string targetInstance;
    int patternIndex = 0;
    std::vector<StepSnapshot> steps;
};

// One formal section of a piece (e.g. "intro", "buildup", "climax"): a bar
// range, which Scene from state/SceneLibrary plays during it, and the
// layering/budget/reservation intentions authored for that stretch.
// Sections drive playback themselves once a blueprint is current (see
// ComposerCore::advanceBlueprintIfNeeded) - the older Scene::nextSceneId/
// durationBars chain (policy/SceneAdvancePolicy) still exists independently
// for hand-authored scene chains and manual sends, but a section's own
// startBar/durationBars is what the blueprint player actually follows.
struct BlueprintSection
{
    std::string id;
    std::string name;
    std::string sceneId;
    int startBar = 0;
    int durationBars = 4;

    // "presentation"/"build"/"peak"/"release", or empty for "no archetype
    // assigned." Auto-set by BlueprintGenerator::generate for generated
    // blueprints (matching BlueprintGenerator::archetypeName exactly);
    // also hand-authorable for loaded/manually-built blueprints via
    // ui/BlueprintSectionsContent, so the motif/rule engine
    // (policy/MotifEngine) works the same way regardless of how a
    // blueprint came to exist. An empty archetype means the motif engine
    // leaves that section untouched, the same as "presentation" does.
    std::string archetype;

    std::vector<SectionLayerRole> layerRoles;
    std::vector<SectionBudgetOverride> budgetOverrides;
    std::vector<ReservedValue> reservedValues;
    std::vector<SectionModulatorValue> modulatorValues;
    std::vector<SectionCapturedContent> capturedContent;
};

// One breakpoint of an explicitly-saved arc dimension curve for a blueprint:
// at this bar, the dimension has this value (conventionally 0..1, matching
// policy/Arc.h's ArcBreakpoint - kept as an independent plain-data type here
// rather than including policy/ from model/, matching this codebase's
// existing model-stays-below-policy layering, e.g. StateSerializer.h pulls
// in every model/ header but no model/ header pulls in policy/).
struct BlueprintArcPoint
{
    int bar = 0;
    float value = 0.0f;
};

// An explicitly-authored arc dimension for this blueprint, saved once a user
// edits what would otherwise be a display-only curve derived from this
// blueprint's own section/archetype sequence (see
// BlueprintGenerator::deriveArcSetFromSections / resolveBlueprintArcSet).
// Absence of an entry for a given dimension name means "still derived,
// nothing saved yet" - not "flat zero" - so an unedited blueprint doesn't
// carry five redundant curves, and a blueprint composed before this feature
// existed (e.g. "Vers la flamme", hand-composed as explicit sections with no
// arc data at all) still gets a sensible curve without a migration step.
struct BlueprintArcCurve
{
    std::string dimension; // one of ArcSet's names: energy/tension/density/complexity/coherence
    std::vector<BlueprintArcPoint> points;
};

// The whole-piece narrative structure: an ordered list of sections. This is
// "Blueprint JSON" per docs/composer_mastermind_design.md's v1.0 section -
// the schema/model half of it. Authored and persisted (state/BlueprintLibrary,
// state/StateSerializer) but not yet the thing that drives autonomous
// playback end-to-end; that's the "full loop" v1.0 still has left.
struct Blueprint
{
    std::string id;
    std::string name;
    std::vector<BlueprintSection> sections;
    std::vector<BlueprintArcCurve> arcCurves;
};
