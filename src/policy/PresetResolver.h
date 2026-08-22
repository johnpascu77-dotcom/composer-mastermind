#pragma once

#include <vector>
#include "../model/Preset.h"
#include "../model/Instance.h"
#include "../model/Scene.h"
#include "ArcSet.h"

// Resolves a role-addressed RolePreset into concrete SceneInstanceOverride
// entries - the "resolved to actual instance ids at apply-time via each
// Instance::role" mechanism from docs/composer_mastermind_design.md's
// "'Send To All' Is One Preset Among Many" section. Pure/JUCE-free, same
// style as policy/SceneAdvancePolicy and policy/MutationPolicy, so it stays
// unit testable without a plugin host.
namespace PresetResolver
{
    // For every instance in allInstances whose role matches preset.targetRole,
    // merges the preset's activePattern/gridMode/swing fields into scene's
    // instanceOverrides (adding a new override entry if that instance
    // doesn't already have one, updating only the fields the preset actually
    // sets otherwise - matching SceneInstanceOverride's own -1/-1.0f
    // "inherit" convention rather than clobbering fields the preset leaves
    // untouched). Also adds the instance to scene.targets if it isn't
    // already there, since an instanceOverride is inert for an instance the
    // scene doesn't target (see Router::routeScene). Returns how many
    // instances were affected.
    int applyRolePreset(const RolePreset& preset, const std::vector<Instance>& allInstances, Scene& scene);

    // Same resolution, once per role slot - applies every slot in
    // preset.roleSlots to whichever currently-registered instances hold
    // that slot's role, so the whole joint/correlated choice lands together
    // in one call. Returns the total instances affected across all slots.
    int applyRhythmicRelationshipPreset(const RhythmicRelationshipPreset& preset,
                                         const std::vector<Instance>& allInstances, Scene& scene);

    // Stamps preset's normalized breakpoints onto targetArcName's curve
    // within [startBar, endBar]: any existing breakpoints strictly inside
    // that range are replaced (a preset deterministically sets its own
    // scope, same philosophy as applyRolePreset overwriting a field rather
    // than leaving stale state alongside it), then the preset's points are
    // scaled in (position 0..1 -> that bar range, value passed through
    // as-is since it already matches ArcBreakpoint's 0..1 convention).
    // targetArcName must be one of arcSet.getArcNames() - unrecognized
    // names are rejected rather than silently no-opping (see
    // ArcSet::setArc). Returns the number of breakpoints stamped, or 0 if
    // the preset is empty, the range is invalid, or the name is unrecognized.
    int applyArcPreset(const ArcPreset& preset, const std::string& targetArcName, int startBar, int endBar,
                        ArcSet& arcSet);
}
