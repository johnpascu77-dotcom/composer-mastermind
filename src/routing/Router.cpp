#include "Router.h"
#include "../midi/CCMapping.h"
#include <algorithm>

Router::Router(InstanceRegistry& registry, CCDispatcher& dispatcher, PolicyEngine& policy, InstanceStateTracker& tracker)
    : instanceRegistry(registry), ccDispatcher(dispatcher), policyEngine(policy), stateTracker(tracker)
{
}

void Router::setBudgetOverrideResolver(BudgetOverrideResolver resolver)
{
    budgetOverrideResolver = std::move(resolver);
}

void Router::setReservedValueChecker(ReservedValueChecker checker)
{
    reservedValueChecker = std::move(checker);
}

void Router::routeScene(const Scene& scene, int currentBar)
{
    for (const auto& instanceRef : instanceRegistry.getAllInstances())
    {
        if (!instanceRef.enabled)
            continue;

        bool targeted = false;
        for (const auto& target : scene.targets)
        {
            if (target == instanceRef.id)
            {
                targeted = true;
                break;
            }
        }

        if (targeted)
            sendSceneToInstance(scene, instanceRef);
    }

    for (const auto& pattern : scene.patterns)
    {
        Instance instance;
        if (instanceRegistry.getInstanceById(pattern.targetInstance, instance) && instance.enabled)
            sendScenePattern(pattern, instance);
    }

    for (const auto& mutation : scene.mutations)
        routeMutation(mutation, currentBar);
}

bool Router::routeMutation(const Mutation& mutation, int currentBar)
{
    Instance instance;
    if (!instanceRegistry.getInstanceById(mutation.targetInstance, instance) || !instance.enabled)
        return false;

    const int baseCC = CCMapping::patternBaseCC(mutation.patternIndex);
    if (baseCC < 0)
        return false;

    CCMapping::MutationOffset offset;
    if (!CCMapping::mutationOffsetForType(mutation.type, offset))
        return false;

    RoleBudget overrideBudget;
    const RoleBudget* overrideBudgetPtr = nullptr;
    if (budgetOverrideResolver && budgetOverrideResolver(instance.role, currentBar, overrideBudget))
        overrideBudgetPtr = &overrideBudget;

    if (!policyEngine.authorize(mutation, instance.role, currentBar, overrideBudgetPtr))
        return false;

    // A mutation is a nudge relative to the instance's last known state, not
    // a full re-specification - "rotate by -2" should mean "2 steps back
    // from wherever it currently is", not "set rotation to the literal
    // value -2" (which, for a cyclic 0-15 parameter, silently wrapped to a
    // musically unrelated +14 before this fix). Inversion is the one
    // exception: a boolean has no sensible "delta", so amount != 0 stays an
    // absolute on/off toggle.
    InstanceParameterState currentState;
    stateTracker.getState(instance.id, currentState); // no prior state -> currentState keeps its defaults
    InstancePatternState updatedPattern = currentState.patterns[mutation.patternIndex];

    int ccValue = 0;

    switch (offset)
    {
        case CCMapping::MutationOffset::Transpose:
            updatedPattern.transpose = std::clamp(updatedPattern.transpose + mutation.amount,
                                                    -CCMapping::kMaxTranspose, CCMapping::kMaxTranspose);
            ccValue = CCMapping::encodeTranspose(updatedPattern.transpose);
            break;
        case CCMapping::MutationOffset::Rotation:
            updatedPattern.rotation = CCMapping::wrapRotation(updatedPattern.rotation + mutation.amount);
            ccValue = CCMapping::encodeRotation(updatedPattern.rotation);
            break;
        case CCMapping::MutationOffset::Length:
            updatedPattern.length = std::clamp(updatedPattern.length + mutation.amount,
                                                CCMapping::kMinPatternLoopLength, CCMapping::kPatternSteps);
            ccValue = CCMapping::encodeLength(updatedPattern.length);
            break;
        case CCMapping::MutationOffset::Inversion:
            updatedPattern.inversion = (mutation.amount != 0);
            ccValue = CCMapping::encodeInversion(updatedPattern.inversion);
            break;
    }

    // Apex exclusivity: don't let this mutation reach a value a later
    // blueprint section has reserved for itself - resolve the resulting
    // absolute value per type (not the delta) since that's what's actually
    // "reached" from here on.
    int resultingValue = 0;
    switch (offset)
    {
        case CCMapping::MutationOffset::Transpose: resultingValue = updatedPattern.transpose; break;
        case CCMapping::MutationOffset::Rotation:  resultingValue = updatedPattern.rotation; break;
        case CCMapping::MutationOffset::Length:    resultingValue = updatedPattern.length; break;
        case CCMapping::MutationOffset::Inversion: resultingValue = updatedPattern.inversion ? 1 : 0; break;
    }

    if (reservedValueChecker
        && reservedValueChecker(instance.id, mutation.patternIndex, mutation.type, resultingValue, currentBar))
        return false;

    const int ccNumber = baseCC + static_cast<int>(offset);
    ccDispatcher.sendCC(instance.midiChannel, ccNumber, ccValue);

    stateTracker.recordPattern(instance.id, mutation.patternIndex, updatedPattern.transpose,
                                updatedPattern.rotation, updatedPattern.length, updatedPattern.inversion);
    return true;
}

void Router::routeContinuousTranspose(const std::string& targetInstance, int patternIndex, int absoluteTranspose)
{
    Instance instance;
    if (!instanceRegistry.getInstanceById(targetInstance, instance) || !instance.enabled)
        return;

    const int baseCC = CCMapping::patternBaseCC(patternIndex);
    if (baseCC < 0)
        return;

    const int clampedTranspose =
        std::clamp(absoluteTranspose, -CCMapping::kMaxTranspose, CCMapping::kMaxTranspose);

    InstanceParameterState currentState;
    stateTracker.getState(instance.id, currentState); // no prior state -> currentState keeps its defaults
    auto updatedPattern = currentState.patterns[patternIndex];
    updatedPattern.transpose = clampedTranspose;

    const int ccNumber = baseCC + static_cast<int>(CCMapping::MutationOffset::Transpose);
    ccDispatcher.sendCC(instance.midiChannel, ccNumber, CCMapping::encodeTranspose(clampedTranspose));

    stateTracker.recordPattern(instance.id, patternIndex, updatedPattern.transpose, updatedPattern.rotation,
                                updatedPattern.length, updatedPattern.inversion);
}

void Router::routeContinuousSwing(const std::string& targetInstance, float absoluteSwingPercent)
{
    Instance instance;
    if (!instanceRegistry.getInstanceById(targetInstance, instance) || !instance.enabled)
        return;

    const float clampedSwing = std::clamp(absoluteSwingPercent, 0.0f, CCMapping::kMaxSwing);

    InstanceParameterState currentState;
    stateTracker.getState(instance.id, currentState); // no prior state -> currentState keeps its defaults

    ccDispatcher.sendCC(instance.midiChannel, CCMapping::kSwing, CCMapping::encodeSwing(clampedSwing));

    stateTracker.recordGlobal(instance.id, currentState.activePattern, currentState.gridMode, clampedSwing);
}

void Router::sendSceneToInstance(const Scene& scene, const Instance& instance)
{
    int activePattern = scene.global.activePattern;
    int gridMode = scene.global.gridMode;
    float swing = scene.global.swing;

    for (const auto& override : scene.instanceOverrides)
    {
        if (override.targetInstance != instance.id)
            continue;

        if (override.activePattern >= 0)
            activePattern = override.activePattern;
        if (override.gridMode >= 0)
            gridMode = override.gridMode;
        if (override.swing >= 0.0f)
            swing = override.swing;
        break;
    }

    ccDispatcher.sendCC(instance.midiChannel, CCMapping::kActivePattern,
                         CCMapping::encodeActivePattern(activePattern));
    ccDispatcher.sendCC(instance.midiChannel, CCMapping::kGridMode,
                         CCMapping::encodeGridMode(gridMode));
    ccDispatcher.sendCC(instance.midiChannel, CCMapping::kSwing,
                         CCMapping::encodeSwing(swing));

    stateTracker.recordGlobal(instance.id, activePattern, gridMode, swing);
}

void Router::sendScenePattern(const ScenePattern& pattern, const Instance& instance)
{
    const int baseCC = CCMapping::patternBaseCC(pattern.patternIndex);
    if (baseCC < 0)
        return;

    ccDispatcher.sendCC(instance.midiChannel,
                         baseCC + static_cast<int>(CCMapping::MutationOffset::Transpose),
                         CCMapping::encodeTranspose(pattern.transpose));
    ccDispatcher.sendCC(instance.midiChannel,
                         baseCC + static_cast<int>(CCMapping::MutationOffset::Rotation),
                         CCMapping::encodeRotation(pattern.rotation));
    ccDispatcher.sendCC(instance.midiChannel,
                         baseCC + static_cast<int>(CCMapping::MutationOffset::Length),
                         CCMapping::encodeLength(pattern.length));
    ccDispatcher.sendCC(instance.midiChannel,
                         baseCC + static_cast<int>(CCMapping::MutationOffset::Inversion),
                         CCMapping::encodeInversion(pattern.inversion));

    stateTracker.recordPattern(instance.id, pattern.patternIndex,
                                pattern.transpose, pattern.rotation, pattern.length, pattern.inversion);
}
