#include "Router.h"
#include "../midi/CCMapping.h"

Router::Router(InstanceRegistry& registry, CCDispatcher& dispatcher)
    : instanceRegistry(registry), ccDispatcher(dispatcher)
{
}

void Router::routeScene(const Scene& scene)
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
        routeMutation(mutation);
}

void Router::routeMutation(const Mutation& mutation)
{
    Instance instance;
    if (!instanceRegistry.getInstanceById(mutation.targetInstance, instance) || !instance.enabled)
        return;

    const int baseCC = CCMapping::patternBaseCC(mutation.patternIndex);
    if (baseCC < 0)
        return;

    CCMapping::MutationOffset offset;
    if (!CCMapping::mutationOffsetForType(mutation.type, offset))
        return;

    const int ccNumber = baseCC + static_cast<int>(offset);
    const int ccValue = CCMapping::encodeMutationAmount(offset, mutation.amount);

    ccDispatcher.sendCC(instance.midiChannel, ccNumber, ccValue);
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
}
