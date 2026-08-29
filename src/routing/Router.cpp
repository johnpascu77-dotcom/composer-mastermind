#include "Router.h"
#include "../midi/CCMapping.h"
#include <algorithm>
#include <cmath>

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

    // Rate is global per-instance (not per-pattern), so it never touches
    // patternBaseCC/MutationOffset - handled entirely separately here,
    // mirroring how Swing is special-cased ahead of the pattern-scoped path
    // in routeContinuousParameter above. Absolute-set, not a delta - see
    // Mutation.h's own comment for why.
    if (mutation.type == "rate")
    {
        RoleBudget rateOverrideBudget;
        const RoleBudget* rateOverrideBudgetPtr = nullptr;
        if (budgetOverrideResolver && budgetOverrideResolver(instance.role, currentBar, rateOverrideBudget))
            rateOverrideBudgetPtr = &rateOverrideBudget;

        if (!policyEngine.authorize(mutation, instance.role, currentBar, rateOverrideBudgetPtr))
            return false;

        InstanceParameterState currentState;
        stateTracker.getState(instance.id, currentState); // no prior state -> currentState keeps its defaults
        const int newRate = CCMapping::rateStateFromMutationAmount(mutation.amount);

        if (reservedValueChecker
            && reservedValueChecker(instance.id, mutation.patternIndex, mutation.type, newRate, currentBar))
            return false;

        ccDispatcher.sendCC(instance.midiChannel, CCMapping::kRate, CCMapping::encodeRate(newRate));
        stateTracker.recordGlobal(instance.id, currentState.activePattern, currentState.gridMode,
                                   currentState.swing, newRate);
        return true;
    }

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
    // musically unrelated +14 before this fix). Inversion, Retrograde, and
    // M7 are the exception: a boolean has no sensible "delta", so
    // amount != 0 stays an absolute on/off toggle.
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
        case CCMapping::MutationOffset::Retrograde:
            updatedPattern.retrograde = (mutation.amount != 0);
            ccValue = CCMapping::encodeRetrograde(updatedPattern.retrograde);
            break;
        case CCMapping::MutationOffset::M7:
            updatedPattern.m7 = (mutation.amount != 0);
            ccValue = CCMapping::encodeM7(updatedPattern.m7);
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
        case CCMapping::MutationOffset::Inversion:  resultingValue = updatedPattern.inversion ? 1 : 0;  break;
        case CCMapping::MutationOffset::Retrograde: resultingValue = updatedPattern.retrograde ? 1 : 0; break;
        case CCMapping::MutationOffset::M7:         resultingValue = updatedPattern.m7 ? 1 : 0;         break;
    }

    if (reservedValueChecker
        && reservedValueChecker(instance.id, mutation.patternIndex, mutation.type, resultingValue, currentBar))
        return false;

    const int ccNumber = baseCC + static_cast<int>(offset);
    ccDispatcher.sendCC(instance.midiChannel, ccNumber, ccValue);

    stateTracker.recordPattern(instance.id, mutation.patternIndex, updatedPattern.transpose,
                                updatedPattern.rotation, updatedPattern.length, updatedPattern.inversion,
                                updatedPattern.retrograde, updatedPattern.m7);
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
                                updatedPattern.length, updatedPattern.inversion, updatedPattern.retrograde,
                                updatedPattern.m7);
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

    stateTracker.recordGlobal(instance.id, currentState.activePattern, currentState.gridMode, clampedSwing, currentState.rate);
}

// Rate is global per-instance and already a clean 3-state int (no percent
// domain to snap, unlike Swing) - value here is the arc-mapped raw state,
// still banded to 0..2 defensively in case a caller passes something
// unbanded. Mirrors routeContinuousSwing's shape exactly.
void Router::routeContinuousRate(const std::string& targetInstance, int absoluteRateState)
{
    Instance instance;
    if (!instanceRegistry.getInstanceById(targetInstance, instance) || !instance.enabled)
        return;

    const int clampedRate = std::clamp(absoluteRateState, 0, 2);

    InstanceParameterState currentState;
    stateTracker.getState(instance.id, currentState); // no prior state -> currentState keeps its defaults

    ccDispatcher.sendCC(instance.midiChannel, CCMapping::kRate, CCMapping::encodeRate(clampedRate));

    stateTracker.recordGlobal(instance.id, currentState.activePattern, currentState.gridMode, currentState.swing, clampedRate);
}

void Router::routeContinuousParameter(const std::string& targetInstance, int patternIndex,
                                       ModulationParameter parameter, float value)
{
    if (parameter == ModulationParameter::Swing)
    {
        routeContinuousSwing(targetInstance, value);
        return;
    }

    if (parameter == ModulationParameter::Rate)
    {
        routeContinuousRate(targetInstance, static_cast<int>(std::lround(value)));
        return;
    }

    Instance instance;
    if (!instanceRegistry.getInstanceById(targetInstance, instance) || !instance.enabled)
        return;

    const int baseCC = CCMapping::patternBaseCC(patternIndex);
    if (baseCC < 0)
        return;

    InstanceParameterState currentState;
    stateTracker.getState(instance.id, currentState); // no prior state -> currentState keeps its defaults
    auto updatedPattern = currentState.patterns[patternIndex];

    switch (parameter)
    {
        case ModulationParameter::Transpose:
        {
            updatedPattern.transpose = std::clamp(static_cast<int>(std::lround(value)),
                                                    -CCMapping::kMaxTranspose, CCMapping::kMaxTranspose);
            const int ccNumber = baseCC + static_cast<int>(CCMapping::MutationOffset::Transpose);
            ccDispatcher.sendCC(instance.midiChannel, ccNumber, CCMapping::encodeTranspose(updatedPattern.transpose));
            break;
        }
        case ModulationParameter::Rotation:
        {
            updatedPattern.rotation = CCMapping::wrapRotation(static_cast<int>(std::lround(value)));
            const int ccNumber = baseCC + static_cast<int>(CCMapping::MutationOffset::Rotation);
            ccDispatcher.sendCC(instance.midiChannel, ccNumber, CCMapping::encodeRotation(updatedPattern.rotation));
            break;
        }
        case ModulationParameter::Length:
        {
            updatedPattern.length = std::clamp(static_cast<int>(std::lround(value)),
                                                CCMapping::kMinPatternLoopLength, CCMapping::kPatternSteps);
            const int ccNumber = baseCC + static_cast<int>(CCMapping::MutationOffset::Length);
            ccDispatcher.sendCC(instance.midiChannel, ccNumber, CCMapping::encodeLength(updatedPattern.length));
            break;
        }
        default:
            return; // not a continuous parameter - caller error, nothing sent
    }

    stateTracker.recordPattern(instance.id, patternIndex, updatedPattern.transpose, updatedPattern.rotation,
                                updatedPattern.length, updatedPattern.inversion, updatedPattern.retrograde,
                                updatedPattern.m7);
}

void Router::routeThresholdParameter(const std::string& targetInstance, int patternIndex,
                                      ModulationParameter parameter, int bandedValue)
{
    Instance instance;
    if (!instanceRegistry.getInstanceById(targetInstance, instance) || !instance.enabled)
        return;

    InstanceParameterState currentState;
    stateTracker.getState(instance.id, currentState); // no prior state -> currentState keeps its defaults

    if (parameter == ModulationParameter::ActivePattern)
    {
        ccDispatcher.sendCC(instance.midiChannel, CCMapping::kActivePattern,
                             CCMapping::encodeActivePattern(bandedValue));
        stateTracker.recordGlobal(instance.id, bandedValue, currentState.gridMode, currentState.swing, currentState.rate);
        return;
    }

    if (parameter == ModulationParameter::GridMode)
    {
        const int gridMode = bandedValue != 0 ? 1 : 0;
        ccDispatcher.sendCC(instance.midiChannel, CCMapping::kGridMode, CCMapping::encodeGridMode(gridMode));
        stateTracker.recordGlobal(instance.id, currentState.activePattern, gridMode, currentState.swing, currentState.rate);
        return;
    }

    if (parameter != ModulationParameter::Inversion && parameter != ModulationParameter::Retrograde
        && parameter != ModulationParameter::M7)
        return; // not a threshold parameter - caller error, nothing sent

    const int baseCC = CCMapping::patternBaseCC(patternIndex);
    if (baseCC < 0)
        return;

    auto updatedPattern = currentState.patterns[patternIndex];
    const bool on = bandedValue != 0;

    CCMapping::MutationOffset offset = CCMapping::MutationOffset::Inversion;
    int ccValue = 0;

    if (parameter == ModulationParameter::Inversion)
    {
        updatedPattern.inversion = on;
        offset = CCMapping::MutationOffset::Inversion;
        ccValue = CCMapping::encodeInversion(on);
    }
    else if (parameter == ModulationParameter::Retrograde)
    {
        updatedPattern.retrograde = on;
        offset = CCMapping::MutationOffset::Retrograde;
        ccValue = CCMapping::encodeRetrograde(on);
    }
    else
    {
        updatedPattern.m7 = on;
        offset = CCMapping::MutationOffset::M7;
        ccValue = CCMapping::encodeM7(on);
    }

    const int ccNumber = baseCC + static_cast<int>(offset);
    ccDispatcher.sendCC(instance.midiChannel, ccNumber, ccValue);

    stateTracker.recordPattern(instance.id, patternIndex, updatedPattern.transpose, updatedPattern.rotation,
                                updatedPattern.length, updatedPattern.inversion, updatedPattern.retrograde,
                                updatedPattern.m7);
}

void Router::stopAllInstances()
{
    for (const auto& instance : instanceRegistry.getAllInstances())
    {
        if (!instance.enabled)
            continue;

        InstanceParameterState currentState;
        stateTracker.getState(instance.id, currentState); // no prior state -> currentState keeps its defaults

        ccDispatcher.sendCC(instance.midiChannel, CCMapping::kActivePattern, CCMapping::encodeActivePattern(0));
        stateTracker.recordGlobal(instance.id, 0, currentState.gridMode, currentState.swing, currentState.rate);
    }
}

void Router::sendSceneToInstance(const Scene& scene, const Instance& instance)
{
    int activePattern = scene.global.activePattern;
    int gridMode = scene.global.gridMode;
    float swing = scene.global.swing;
    int rate = scene.global.rate;

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
        if (override.rate >= 0)
            rate = override.rate;
        break;
    }

    ccDispatcher.sendCC(instance.midiChannel, CCMapping::kActivePattern,
                         CCMapping::encodeActivePattern(activePattern));
    ccDispatcher.sendCC(instance.midiChannel, CCMapping::kGridMode,
                         CCMapping::encodeGridMode(gridMode));
    ccDispatcher.sendCC(instance.midiChannel, CCMapping::kSwing,
                         CCMapping::encodeSwing(swing));
    ccDispatcher.sendCC(instance.midiChannel, CCMapping::kRate,
                         CCMapping::encodeRate(rate));

    stateTracker.recordGlobal(instance.id, activePattern, gridMode, swing, rate);
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
    ccDispatcher.sendCC(instance.midiChannel,
                         baseCC + static_cast<int>(CCMapping::MutationOffset::Retrograde),
                         CCMapping::encodeRetrograde(pattern.retrograde));
    ccDispatcher.sendCC(instance.midiChannel,
                         baseCC + static_cast<int>(CCMapping::MutationOffset::M7),
                         CCMapping::encodeM7(pattern.m7));

    stateTracker.recordPattern(instance.id, pattern.patternIndex,
                                pattern.transpose, pattern.rotation, pattern.length, pattern.inversion,
                                pattern.retrograde, pattern.m7);
}
