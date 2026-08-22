#pragma once

#include "../model/Scene.h"
#include "../model/Mutation.h"
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
