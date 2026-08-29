#pragma once

#include "../model/Scene.h"
#include "../model/Mutation.h"
#include "../model/ModulationRoute.h"
#include "InstanceRegistry.h"
#include "InstanceStateTracker.h"
#include "../midi/CCDispatcher.h"
#include "../policy/PolicyEngine.h"
#include "../policy/MutationPolicy.h"
#include <functional>

// Resolves a per-role budget override for the given bar, if the caller has
// one (e.g. a blueprint section covering that bar overrides this role's
// budget). Returns false and leaves outBudget untouched if there's no
// override - Router then falls back to MutationPolicy's static table, same
// as before blueprints existed. Kept as an injected function rather than a
// direct BlueprintLibrary/ComposerCore dependency so Router doesn't need to
// know the Blueprint model exists - ComposerCore supplies the real logic
// (see ComposerCore::resolveSectionBudgetOverride) after construction.
using BudgetOverrideResolver = std::function<bool(const std::string& role, int currentBar, RoleBudget& outBudget)>;

// Returns true if (targetInstance, patternIndex, type, value) is "reserved"
// by a later blueprint section - i.e. this exact resulting value is meant
// to be spent only once playback reaches that later section, so an earlier
// mutation reaching it now would blunt that section's impact. See
// model/Blueprint.h's ReservedValue and ComposerCore::isValueReservedByLaterSection.
using ReservedValueChecker = std::function<bool(const std::string& targetInstance, int patternIndex,
                                                  const std::string& type, int value, int currentBar)>;

class Router
{
public:
    Router(InstanceRegistry& registry, CCDispatcher& dispatcher, PolicyEngine& policy, InstanceStateTracker& tracker);

    void setBudgetOverrideResolver(BudgetOverrideResolver resolver);
    void setReservedValueChecker(ReservedValueChecker checker);

    void routeScene(const Scene& scene, int currentBar);

    // Returns true if the mutation passed the policy gate and was
    // dispatched as CC, false if it was blocked (unknown/disabled instance,
    // unknown pattern/mutation type, budget exceeded, or the resulting
    // value is reserved by a later blueprint section - see
    // ReservedValueChecker).
    bool routeMutation(const Mutation& mutation, int currentBar);

    // Continuous automation (v1.2 Track B, docs/arc_dimension_mapping_concept.md):
    // writes an absolute Transpose value directly, deliberately bypassing
    // policyEngine/reservedValueChecker entirely - this is a curve re-sampled
    // every bar (Energy/Tension driving the melodic-average-curve's amplitude
    // and register center in ComposerCore), not a discrete authored decision,
    // so it must never compete with routeMutation's per-bar budget for count.
    // Still updates InstanceStateTracker, same as every other dispatch path,
    // so coherence/reconstruction/UI keep seeing accurate state.
    void routeContinuousTranspose(const std::string& targetInstance, int patternIndex, int absoluteTranspose);

    // Continuous automation for the global Swing parameter (v1.2 Track B,
    // Density's consumer) - same bypass-PolicyEngine reasoning as
    // routeContinuousTranspose. Swing is global per instance (CC 24, not
    // per-pattern), so this preserves whatever Active Pattern/Grid Mode the
    // instance is currently tracked at rather than touching them.
    void routeContinuousSwing(const std::string& targetInstance, float absoluteSwingPercent);

    // Continuous automation for the global Rate parameter (v1.29.0) - same
    // shape as routeContinuousSwing above, just an already-int 3-state
    // domain (0=Augmented/1=Normal/2=Diminished) rather than a percent.
    void routeContinuousRate(const std::string& targetInstance, int absoluteRateState);

    // Modulation matrix (ModulationRoute, model/ModulationRoute.h): generic
    // continuous dispatch for Transpose/Rotation/Length/Swing/Rate - `value` is
    // already resolved to the parameter's own absolute domain by the caller
    // (ComposerCore::sendModulationRouteUpdates), this just clamps/encodes/
    // sends/tracks it, same bypass-PolicyEngine reasoning as
    // routeContinuousTranspose/routeContinuousSwing above (curve-driven
    // automation, not a discrete authored decision). No-op for a parameter
    // that isn't one of these four, an unknown/disabled instance, or an
    // out-of-range patternIndex (pattern-scoped parameters only).
    void routeContinuousParameter(const std::string& targetInstance, int patternIndex,
                                   ModulationParameter parameter, float value);

    // Modulation matrix: generic threshold-crossing dispatch for
    // Inversion/Retrograde/M7/ActivePattern/GridMode - `bandedValue` is
    // already resolved by the caller (0/1 for the boolean ones, the target
    // pattern 0..kMaxPatterns for ActivePattern, 0/1 for GridMode). Only
    // ever called on a caller-detected crossing, never every bar. Same
    // PolicyEngine-bypass reasoning as applyCoherenceDivergenceIfDue's own
    // direct CC dispatch.
    void routeThresholdParameter(const std::string& targetInstance, int patternIndex,
                                  ModulationParameter parameter, int bandedValue);

    // Explicit end-of-piece silence (2026-08-25, user's own spec): sends
    // Active Pattern = 0 (stop) to every enabled registered instance,
    // leaving Grid Mode/Swing exactly as tracked - same "touch only what
    // you mean to touch" reasoning as routeContinuousSwing above, just for
    // the one moment playback runs past a blueprint's last section rather
    // than a per-bar curve. Called once per transition, not every bar - see
    // ComposerCore::advanceBlueprintIfNeeded.
    void stopAllInstances();

private:
    InstanceRegistry& instanceRegistry;
    CCDispatcher& ccDispatcher;
    PolicyEngine& policyEngine;
    InstanceStateTracker& stateTracker;
    BudgetOverrideResolver budgetOverrideResolver;
    ReservedValueChecker reservedValueChecker;

    void sendSceneToInstance(const Scene& scene, const Instance& instance);
    void sendScenePattern(const ScenePattern& pattern, const Instance& instance);
};
