#include "ComposerCore.h"
#include "../policy/SceneAdvancePolicy.h"
#include "../policy/CoherenceEvaluator.h"
#include "../policy/BlueprintGenerator.h"
#include "../policy/PresetResolver.h"
#include "../midi/CCMapping.h"
#include "../scheduling/StepClock.h"
#include "../util/Validation.h"
#include "../state/StateSerializer.h"
#include <cmath>
#include <algorithm>

ComposerCore::ComposerCore()
    : router(instanceRegistry, ccDispatcher, policyEngine, instanceStateTracker)
{
    router.setBudgetOverrideResolver([this](const std::string& role, int currentBar, RoleBudget& outBudget)
    {
        return resolveSectionBudgetOverride(role, currentBar, outBudget);
    });

    router.setReservedValueChecker([this](const std::string& targetInstance, int patternIndex,
                                            const std::string& type, int value, int currentBar)
    {
        return isValueReservedByLaterSection(targetInstance, patternIndex, type, value, currentBar);
    });

    patternSyncServer.setChannelResolver([this](int channel, std::string& outInstanceId)
    {
        for (const auto& instance : instanceRegistry.getAllInstances())
        {
            if (instance.midiChannel == channel)
            {
                outInstanceId = instance.id;
                return true;
            }
        }

        return false;
    });

    patternSyncServer.setCurrentBarProvider([this]
    {
        return getCurrentBar();
    });

    mcpBridgeServer.setRequestHandler([this](const juce::var& request)
    {
        return handleMcpBridgeRequest(request);
    });
}

InstanceRegistry& ComposerCore::getInstanceRegistry()
{
    return instanceRegistry;
}

CCDispatcher& ComposerCore::getCCDispatcher()
{
    return ccDispatcher;
}

Router& ComposerCore::getRouter()
{
    return router;
}

Scheduler& ComposerCore::getScheduler()
{
    return scheduler;
}

SceneLibrary& ComposerCore::getSceneLibrary()
{
    return sceneLibrary;
}

PolicyEngine& ComposerCore::getPolicyEngine()
{
    return policyEngine;
}

InstanceStateTracker& ComposerCore::getInstanceStateTracker()
{
    return instanceStateTracker;
}

ArcSet& ComposerCore::getArcSet()
{
    return arcSet;
}

BlueprintLibrary& ComposerCore::getBlueprintLibrary()
{
    return blueprintLibrary;
}

PresetLibrary& ComposerCore::getPresetLibrary()
{
    return presetLibrary;
}

ModulatorTargetLibrary& ComposerCore::getModulatorTargetLibrary()
{
    return modulatorTargetLibrary;
}

ModulationRouteLibrary& ComposerCore::getModulationRouteLibrary()
{
    return modulationRouteLibrary;
}

PatternSyncServer& ComposerCore::getPatternSyncServer()
{
    return patternSyncServer;
}

McpBridgeServer& ComposerCore::getMcpBridgeServer()
{
    return mcpBridgeServer;
}

LockedStepLibrary& ComposerCore::getLockedStepLibrary()
{
    return lockedStepLibrary;
}

MilestoneLibrary& ComposerCore::getMilestoneLibrary()
{
    return milestoneLibrary;
}

void ComposerCore::captureMilestone(const std::string& label, int currentBar)
{
    Milestone milestone;
    milestone.bar = currentBar;
    milestone.label = label;

    auto& cache = patternSyncServer.getCache();

    for (const auto& instance : instanceRegistry.getAllInstances())
    {
        if (!instance.enabled)
            continue;

        MilestoneInstanceState instanceState;
        instanceState.instanceId = instance.id;
        instanceStateTracker.getState(instance.id, instanceState.trackerState);

        for (int patternIndex = 0; patternIndex < CCMapping::kMaxPatterns; ++patternIndex)
        {
            CachedPattern cached;
            if (cache.get(instance.id, patternIndex, cached))
                instanceState.patterns.push_back(cached.snapshot);
        }

        milestone.instances.push_back(std::move(instanceState));
    }

    milestoneLibrary.capture(std::move(milestone));
}

bool ComposerCore::restoreMilestone(size_t index)
{
    Milestone milestone;
    if (!milestoneLibrary.getByIndex(index, milestone))
        return false;

    for (const auto& instanceState : milestone.instances)
    {
        Instance instance;
        if (!instanceRegistry.getInstanceById(instanceState.instanceId, instance) || !instance.enabled)
            continue;

        const auto& state = instanceState.trackerState;

        ccDispatcher.sendCC(instance.midiChannel, CCMapping::kActivePattern,
                             CCMapping::encodeActivePattern(state.activePattern));
        ccDispatcher.sendCC(instance.midiChannel, CCMapping::kGridMode, CCMapping::encodeGridMode(state.gridMode));
        ccDispatcher.sendCC(instance.midiChannel, CCMapping::kSwing, CCMapping::encodeSwing(state.swing));
        instanceStateTracker.recordGlobal(instance.id, state.activePattern, state.gridMode, state.swing, state.rate);

        for (int patternIndex = 0; patternIndex < CCMapping::kMaxPatterns; ++patternIndex)
        {
            const int baseCC = CCMapping::patternBaseCC(patternIndex);
            if (baseCC < 0)
                continue;

            const auto& pattern = state.patterns[patternIndex];
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

            instanceStateTracker.recordPattern(instance.id, patternIndex, pattern.transpose, pattern.rotation,
                                                pattern.length, pattern.inversion, pattern.retrograde, pattern.m7);
        }

        for (const auto& patternSnapshot : instanceState.patterns)
        {
            patternSyncServer.sendWriteFullPattern(instance.midiChannel, patternSnapshot.patternIndex,
                                                    patternSnapshot.steps);
            patternSyncServer.getCache().store(instance.id, patternSnapshot, getCurrentBar());
        }
    }

    logActivity(getCurrentBar(),
                "Restored milestone: " + milestone.label + " (bar " + std::to_string(milestone.bar) + ")");
    return true;
}

MotifEngine::ApplicationMode ComposerCore::getMotifApplicationMode() const
{
    return motifApplicationMode.load();
}

void ComposerCore::setMotifApplicationMode(MotifEngine::ApplicationMode mode)
{
    motifApplicationMode.store(mode);
}

ContentMode ComposerCore::getContentMode() const
{
    return contentMode.load();
}

void ComposerCore::setContentMode(ContentMode mode)
{
    contentMode.store(mode);
}

void ComposerCore::setCurrentBlueprint(const Blueprint& blueprint)
{
    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        currentBlueprintData = blueprint;
        hasCurrentBlueprint = true;
        currentActiveSectionId.clear();
        motifPassCountForSection = 0;
        lastMotifPassBar = -1;
    }

    // Sync the live ArcSet (Track B's continuous consumers and any
    // "arc"-mode ModulatorTarget both read this every bar) from whatever
    // this blueprint actually says its arc is - stored curves verbatim,
    // anything else derived from its own sections. Previously nothing did
    // this: the live ArcSet just kept whatever the Arcs tab or the last
    // GenerateView commit happened to leave it at, decoupled from which
    // blueprint was actually active. See BlueprintGenerator::resolveBlueprintArcSet.
    std::vector<std::string> derivedDimensions; // not needed here, only the UI cares which dimensions are derived
    const auto resolvedArcSet =
        BlueprintGenerator::resolveBlueprintArcSet(blueprint, instanceRegistry.getAllInstances(), derivedDimensions);
    for (const auto& dimensionName : resolvedArcSet.getArcNames())
        arcSet.setArc(dimensionName, resolvedArcSet.getArc(dimensionName));
}

bool ComposerCore::getCurrentBlueprint(Blueprint& outBlueprint) const
{
    std::lock_guard<std::mutex> lock(blueprintMutex);
    if (!hasCurrentBlueprint)
        return false;

    outBlueprint = currentBlueprintData;
    return true;
}

std::string ComposerCore::getActiveSectionId() const
{
    std::lock_guard<std::mutex> lock(blueprintMutex);
    return currentActiveSectionId;
}

bool ComposerCore::resolveSectionBudgetOverride(const std::string& role, int currentBar, RoleBudget& outBudget) const
{
    RoleBudget baseBudget;
    bool sectionCoversBar = false;

    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        if (!hasCurrentBlueprint)
            return false;

        for (const auto& section : currentBlueprintData.sections)
        {
            if (currentBar < section.startBar || currentBar >= section.startBar + section.durationBars)
                continue;

            sectionCoversBar = true;
            baseBudget = MutationPolicy::budgetForRole(role);

            for (const auto& budgetOverride : section.budgetOverrides)
            {
                if (budgetOverride.role != role)
                    continue;

                baseBudget.maxMinorPerBar = budgetOverride.maxMinorPerBar;
                baseBudget.maxMediumPerBar = budgetOverride.maxMediumPerBar;
                baseBudget.maxMajorPerBar = budgetOverride.maxMajorPerBar;
                break;
            }

            break;
        }
    }

    if (!sectionCoversBar)
        return false; // no active section - Router falls back to the unscaled static table directly

    // Complexity scales whatever base budget applies (an explicit section
    // override, or the static per-role table) - v1.2 Track B, "more
    // transformation authorized as complexity rises," doubling up
    // Complexity's role alongside its phrase-chain-banding use
    // (firePhraseChainIfDue) rather than inventing a 6th arc dimension. 1x
    // at complexity 0 up to 2x at complexity 1; ceil so even a small base
    // budget visibly grows rather than rounding back down to itself.
    // Unlimited (-1) stays unlimited regardless.
    const float complexityValue = arcSet.getArc("complexity").evaluate(currentBar).value;
    const double multiplier = 1.0 + static_cast<double>(std::clamp(complexityValue, 0.0f, 1.0f));

    const auto scale = [multiplier](int value)
    {
        return value < 0 ? value : static_cast<int>(std::ceil(static_cast<double>(value) * multiplier));
    };

    outBudget.maxMinorPerBar = scale(baseBudget.maxMinorPerBar);
    outBudget.maxMediumPerBar = scale(baseBudget.maxMediumPerBar);
    outBudget.maxMajorPerBar = scale(baseBudget.maxMajorPerBar);
    return true;
}

bool ComposerCore::isValueReservedByLaterSection(const std::string& targetInstance, int patternIndex,
                                                   const std::string& type, int value, int currentBar) const
{
    std::lock_guard<std::mutex> lock(blueprintMutex);
    if (!hasCurrentBlueprint)
        return false;

    for (const auto& section : currentBlueprintData.sections)
    {
        if (section.startBar <= currentBar)
            continue; // this section already started (or is the one playing now) - not "later" from here

        for (const auto& reserved : section.reservedValues)
        {
            if (reserved.targetInstance == targetInstance && reserved.patternIndex == patternIndex
                && reserved.type == type && reserved.value == value)
                return true;
        }
    }

    return false;
}

float ComposerCore::getCurrentCoherence() const
{
    return CoherenceEvaluator::evaluate(instanceStateTracker.getAllStates());
}

std::vector<ActivityLogEntry> ComposerCore::getRecentActivityLog() const
{
    std::lock_guard<std::mutex> lock(activityLogMutex);
    return { activityLog.begin(), activityLog.end() }; // newest first, see logActivity
}

void ComposerCore::logActivity(int bar, const std::string& message)
{
    std::lock_guard<std::mutex> lock(activityLogMutex);
    activityLog.push_front(ActivityLogEntry { bar, message }); // newest first

    if (activityLog.size() > kMaxActivityLogEntries)
        activityLog.pop_back();
}

int ComposerCore::getCurrentBar() const
{
    return currentBarValue.load();
}

void ComposerCore::setCurrentScene(const Scene& scene)
{
    std::lock_guard<std::mutex> lock(sceneMutex);
    currentSceneData = scene;
    hasCurrentScene = true;
    sceneStartPending = true;
    currentSceneStartBar = -1;
}

bool ComposerCore::getCurrentScene(Scene& outScene) const
{
    std::lock_guard<std::mutex> lock(sceneMutex);
    if (!hasCurrentScene)
        return false;

    outScene = currentSceneData;
    return true;
}

void ComposerCore::processBar(int currentBar, double barStartPpq)
{
    currentBarValue.store(currentBar);
    dispatchSceneIfNeeded(currentBar);
    advanceSceneChainIfNeeded(currentBar);
    advanceBlueprintIfNeeded(currentBar, barStartPpq);
    sendModulatorTargetUpdates(currentBar);
    sendPitchFieldBroadcast(currentBar);
}

void ComposerCore::fullRefresh()
{
    Scene scene;
    if (getCurrentScene(scene))
        router.routeScene(scene, currentBarValue.load());
}

void ComposerCore::notifyTransportReset()
{
    std::lock_guard<std::mutex> lock(sceneMutex);
    sceneStartPending = true;
}

void ComposerCore::dispatchSceneIfNeeded(int currentBar)
{
    const auto dueEvents = scheduler.popDueEvents(currentBar);
    if (dueEvents.empty())
        return;

    Scene scene;
    if (!getCurrentScene(scene))
        return;

    for (const auto& event : dueEvents)
    {
        if (event.type == "scene")
            router.routeScene(scene, currentBar);
    }
}

void ComposerCore::advanceSceneChainIfNeeded(int currentBar)
{
    Scene scene;
    int startBar = -1;

    {
        std::lock_guard<std::mutex> lock(sceneMutex);
        if (!hasCurrentScene)
            return;

        if (sceneStartPending)
        {
            currentSceneStartBar = currentBar;
            sceneStartPending = false;
        }

        scene = currentSceneData;
        startBar = currentSceneStartBar;
    }

    if (!SceneAdvancePolicy::shouldAdvance(scene, startBar, currentBar))
        return;

    Scene nextScene;
    if (!sceneLibrary.getSceneById(scene.nextSceneId, nextScene))
        return; // dangling nextSceneId: chain stops here, current scene keeps playing

    {
        std::lock_guard<std::mutex> lock(sceneMutex);
        currentSceneData = nextScene;
        currentSceneStartBar = currentBar;
        sceneStartPending = false;
    }

    router.routeScene(nextScene, currentBar);
}

void ComposerCore::advanceBlueprintIfNeeded(int currentBar, double barStartPpq)
{
    Blueprint blueprint;
    std::string previousSectionId;

    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        if (!hasCurrentBlueprint)
            return;

        blueprint = currentBlueprintData;
        previousSectionId = currentActiveSectionId;
    }

    const BlueprintSection* activeSection = nullptr;
    for (const auto& section : blueprint.sections)
    {
        if (currentBar >= section.startBar && currentBar < section.startBar + section.durationBars)
        {
            activeSection = &section;
            break;
        }
    }

    const std::string newSectionId = activeSection != nullptr ? activeSection->id : std::string();

    if (newSectionId == previousSectionId)
    {
        // Still in the same section (or still outside every section) -
        // routing hasn't changed, but a long-running section may still be
        // due for its next motif pass (see fireMotifPassIfDue) or its next
        // phrase-chain advance (see firePhraseChainIfDue).
        if (activeSection != nullptr)
        {
            // A section played back from captured (Absolute) content is
            // frozen for its whole duration - see enterSection's matching
            // branch. Same condition, recomputed fresh each bar rather than
            // cached, since ContentMode can change live mid-section.
            const bool isFrozen =
                getContentMode() == ContentMode::Absolute && !activeSection->capturedContent.empty();

            if (!isFrozen)
            {
                firePhraseChainIfDue(*activeSection, currentBar);
                fireMotifPassIfDue(*activeSection, currentBar);
                applyContinuousMelodicCurve(*activeSection, currentBar);
                applyContinuousSwing(*activeSection, currentBar);
                applyCoherenceDivergenceIfDue(*activeSection, currentBar);
                sendModulationRouteUpdates(currentBar);
                firePhraseCadenceIfDue(*activeSection, currentBar);
            }
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        currentActiveSectionId = newSectionId;
        motifPassCountForSection = 0;
        lastMotifPassBar = currentBar;
        hasPhraseChainRole = false; // sentinel - forces the first check this section to sync immediately
        hasCoherenceDivergenceState = false; // same sentinel shape, see its own comment
        modulationRouteThresholdState.clear(); // same sentinel shape - forces every route to resync
        currentSectionPpqAnchor = barStartPpq; // phase-zero for every Sequence-mode route's loop-cycle count
        sequenceRouteLastFiredIndex.clear(); // fresh section - every sequence route resyncs from cycle 0
        phraseCadenceLastFiredIndex.clear(); // fresh section - every BarCycle route resyncs from phrase 0
    }

    if (activeSection == nullptr)
    {
        // Reached past the blueprint's last section (2026-08-25, user's own
        // spec, found by actually listening to a demo piece run past its
        // end): explicitly silence every instance rather than leaving
        // whatever the last section wrote looping forever - Bitwig's own
        // playhead may well keep moving, but nothing should still be
        // sounding once the piece itself has finished. Reached exactly once
        // per transition (this branch only runs when newSectionId differs
        // from previousSectionId, same one-shot shape as enterSection
        // below), not resent every bar while past the end.
        router.stopAllInstances();

        // Transport-companion stop signal, fired exactly once here (same
        // one-shot shape as the branch itself). A Bitwig controller script
        // turns this into transport.stop() so the clip-launcher "wash" ends
        // with the composition - the plugin can't reach the host transport.
        transportCompanionClient.signalBlueprintEnd();

        logActivity(currentBar, "Blueprint '" + blueprint.name + "' reached its end - stopped all instances, sent transport-stop signal");
        return; // nothing to enter
    }

    enterSection(*activeSection, currentBar);
}

void ComposerCore::enterSection(const BlueprintSection& section, int currentBar)
{
    if (section.sceneId.empty())
        return;

    // Novelty-aware preset selection (Phase 3, 2026-08-22) - carry forward
    // whatever the OUTGOING section actually used as the new frozen avoid-id
    // for this section's entire lifetime, before this section's own stamp
    // overwrites currentSectionMotifPresetId with its own choice. See
    // avoidMotifPresetIdForSection's own comment in ComposerCore.h for why
    // this can't just be one member updated in place.
    avoidMotifPresetIdForSection = currentSectionMotifPresetId;
    currentSectionMotifPresetId.clear();

    Scene scene;
    if (!sceneLibrary.getSceneById(section.sceneId, scene))
        return;

    applyLayerRoleOverrides(section, scene);
    router.routeScene(scene, currentBar);
    sendSectionModulatorValues(section);

    logActivity(currentBar, "Entered section '" + section.name + "' (" + section.archetype
                                 + "), scene '" + section.sceneId + "'");

    // Snapshot every registered instance's just-routed Active Pattern as
    // this section's "home" - what the motif engine's call-and-response
    // (CC20 stop/resume) should treat as this instance's real intent for
    // the section, captured once rather than re-read live (see
    // ComposerCore.h's motifHomePatternForSection comment).
    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        motifHomePatternForSection.clear();
        for (const auto& registeredInstance : instanceRegistry.getAllInstances())
        {
            InstanceParameterState state;
            instanceStateTracker.getState(registeredInstance.id, state);
            motifHomePatternForSection[registeredInstance.id] = state.activePattern;
        }
    }

    resetRhythmBaselineForSection(section);

    // Absolute content mode (2026-08-23, user's own request - see
    // ComposerCore.h's ContentMode doc comment): a section with real captured
    // content, played back verbatim instead of generatively stamped, and
    // exempt from every ongoing per-bar modification for as long as it plays
    // (see the matching branch in advanceBlueprintIfNeeded). A section with
    // no captured content always behaves generatively, regardless of the
    // global mode - there's nothing "absolute" to prefer over the generative
    // path unless this specific section actually has something captured.
    if (getContentMode() == ContentMode::Absolute && !section.capturedContent.empty())
    {
        for (const auto& entry : section.capturedContent)
        {
            Instance instance;
            if (!instanceRegistry.getInstanceById(entry.targetInstance, instance))
                continue;

            patternSyncServer.sendWriteFullPattern(instance.midiChannel, entry.patternIndex, entry.steps);

            // Update Composer Mastermind's own record of this pattern's content
            // too, not just MPL's real storage - every other content-write path in
            // this file pairs the two (see e.g. restoreMilestone,
            // resetRhythmBaselineForSection), but this one didn't, which is why the
            // Score View's Overlay piano-roll (PrimaryView.cpp, reads straight from
            // this cache) rendered nothing for a frozen section despite the audio
            // being completely correct - the cache genuinely never learned what was
            // written, found live 2026-08-27.
            PatternSnapshot capturedSnapshot;
            capturedSnapshot.patternIndex = entry.patternIndex;
            capturedSnapshot.steps = entry.steps;
            patternSyncServer.getCache().store(instance.id, capturedSnapshot, currentBar);
        }

        logActivity(currentBar, "Wrote captured (Absolute) content for section '" + section.name + "' - frozen for its duration");
    }
    else
    {
        stampMotifForSection(section);
        seedPhraseChainPatterns(section);
        applyRhythmForSection(section, 0);
        applyContinuousMelodicCurve(section, currentBar);
        applyContinuousSwing(section, currentBar);
        applyCoherenceDivergenceIfDue(section, currentBar);
        sendModulationRouteUpdates(currentBar);
        firePhraseCadenceIfDue(section, currentBar);
    }

    // Milestone capture (Phase 3, 2026-08-22) - after everything above has
    // run, so this reflects what the section actually starts out sounding
    // like (post-stamp/seed/rhythm), not the bare pre-entry state.
    captureMilestone("Entered section '" + section.name + "' (" + section.archetype + ")", currentBar);
}

void ComposerCore::primeForPlayback()
{
    Blueprint blueprint;
    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        if (!hasCurrentBlueprint)
            return;
        blueprint = currentBlueprintData;
    }

    const BlueprintSection* firstSection = nullptr;
    for (const auto& section : blueprint.sections)
        if (firstSection == nullptr || section.startBar < firstSection->startBar)
            firstSection = &section;

    if (firstSection == nullptr)
        return; // no sections to prime

    logActivity(firstSection->startBar, "Prime for Playback pressed (transport stopped)");
    enterSection(*firstSection, firstSection->startBar);
}

void ComposerCore::resetToFactoryDefaults()
{
    instanceRegistry.clear();
    sceneLibrary.clear();
    blueprintLibrary.clear();
    presetLibrary.clear();
    modulatorTargetLibrary.clear();
    modulationRouteLibrary.clear();

    {
        std::lock_guard<std::mutex> lock(sceneMutex);
        currentSceneData = Scene{};
        hasCurrentScene = false;
        sceneStartPending = false;
        currentSceneStartBar = -1;
    }

    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        currentBlueprintData = Blueprint{};
        hasCurrentBlueprint = false;
        currentActiveSectionId.clear();
        motifPassCountForSection = 0;
        lastMotifPassBar = -1;
        avoidMotifPresetIdForSection.clear();
        currentSectionMotifPresetId.clear();
        motifHomePatternForSection.clear();
        modulationRouteThresholdState.clear();
        currentSectionPpqAnchor = 0.0;
        sequenceRouteLastFiredIndex.clear();
    }

    logActivity(getCurrentBar(), "Factory reset - every library cleared");
}

void ComposerCore::fireMotifPassIfDue(const BlueprintSection& section, int currentBar)
{
    int passIndex = 0;

    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        if (currentBar - lastMotifPassBar < kPassIntervalBars)
            return;

        ++motifPassCountForSection;
        lastMotifPassBar = currentBar;
        passIndex = motifPassCountForSection;
    }

    applyMotifForSection(section, passIndex);
    applyRhythmForSection(section, passIndex);
}

void ComposerCore::firePhraseChainIfDue(const BlueprintSection& section, int currentBar)
{
    if (section.archetype != "build" && section.archetype != "peak" && section.archetype != "release")
        return;

    const float complexityValue = arcSet.getArc("complexity").evaluate(currentBar).value;
    const MotifEngine::PhraseRole targetRole = MotifEngine::phraseRoleFromComplexity(complexityValue);

    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        if (hasPhraseChainRole && lastPhraseChainRole == targetRole)
            return; // no role crossing since last check - hold the current phrase
        hasPhraseChainRole = true;
        lastPhraseChainRole = targetRole;
    }

    const auto touchedInstances =
        MotifEngine::eligibleInstancesForArchetype(section.archetype, instanceRegistry.getAllInstances());
    const auto motifPresets = presetLibrary.getAllMotifPresets();

    for (const auto& instance : touchedInstances)
    {
        if (hasActiveRouteOverride(instance.id, 0, ModulationParameter::ActivePattern))
            continue; // a user-authored modulation route already owns Active Pattern here

        int nextPattern = 0;
        {
            std::lock_guard<std::mutex> lock(blueprintMutex);
            const auto it = motifHomePatternForSection.find(instance.id);
            if (it == motifHomePatternForSection.end() || it->second <= 0)
                continue; // section wants this instance silent throughout - no chain position to advance

            // Same 1->2->3->1 physical rotation as before, chosen purely to
            // land on a slot that ISN'T the currently-active one (so nothing
            // gets rewritten out from under live playback) - the *role*
            // written into it, not which slot number it happens to be, is
            // what the Complexity curve is actually driving now.
            nextPattern = (it->second % 3) + 1;
            it->second = nextPattern;
        }

        MotifEngine::restampPhraseChainSlot(instance, nextPattern - 1, section.archetype, motifPresets, targetRole,
                                             patternSyncServer, instanceStateTracker, currentBar, lockedStepLibrary,
                                             avoidMotifPresetIdForSection);

        InstanceParameterState trackedState;
        instanceStateTracker.getState(instance.id, trackedState);

        const std::string roleSuffix = " (role " + MotifEngine::phraseRoleName(targetRole) + ")";

        if (trackedState.activePattern != 0)
        {
            // Only re-send if currently audible - an instance CC20's own
            // call-and-response has rested this pass keeps its rest; its
            // chain position still just advanced silently, so it resumes to
            // the new pattern next time whoPlaysThisPass lets it play,
            // rather than un-resting it out of turn.
            ccDispatcher.sendCC(instance.midiChannel, CCMapping::kActivePattern,
                                 CCMapping::encodeActivePattern(nextPattern));
            instanceStateTracker.recordGlobal(instance.id, nextPattern, trackedState.gridMode, trackedState.swing, trackedState.rate);
            logActivity(currentBar, instance.id + ": phrase chain advanced to pattern "
                                         + std::to_string(nextPattern) + roleSuffix);
        }
        else
        {
            logActivity(currentBar, instance.id + ": phrase chain position advanced to pattern "
                                         + std::to_string(nextPattern) + roleSuffix + " (currently resting)");
        }
    }
}

void ComposerCore::applyLayerRoleOverrides(const BlueprintSection& section, Scene& scene) const
{
    for (const auto& layerRole : section.layerRoles)
    {
        if (layerRole.layerRole != "background")
            continue;

        bool found = false;
        for (auto& override : scene.instanceOverrides)
        {
            if (override.targetInstance != layerRole.targetInstance)
                continue;

            found = true;
            if (override.activePattern < 0) // scene didn't already say what this instance should do
                override.activePattern = 0;
            break;
        }

        if (!found)
        {
            SceneInstanceOverride backgroundOverride;
            backgroundOverride.targetInstance = layerRole.targetInstance;
            backgroundOverride.activePattern = 0;
            scene.instanceOverrides.push_back(backgroundOverride);
        }
    }
}

void ComposerCore::applyMotifForSection(const BlueprintSection& section, int passIndex)
{
    std::map<std::string, int> homePatterns;
    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        homePatterns = motifHomePatternForSection;
    }

    MotifEngine::applyForSection(section,
        instanceRegistry.getAllInstances(),
        presetLibrary.getAllMotifPresets(),
        patternSyncServer,
        instanceStateTracker,
        homePatterns,
        motifApplicationMode.load(),
        passIndex,
        ccDispatcher,
        lockedStepLibrary,
        avoidMotifPresetIdForSection);

    logActivity(getCurrentBar(), "Motif pass " + std::to_string(passIndex) + " fired for section '"
                                      + section.name + "'");
}

void ComposerCore::stampMotifForSection(const BlueprintSection& section)
{
    std::map<std::string, int> homePatterns;
    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        homePatterns = motifHomePatternForSection;
    }

    // avoidMotifPresetIdForSection was frozen in enterSection just before
    // this call, from whatever the OUTGOING section used - this call is
    // what actually decides THIS section's preset; every later call this
    // section makes (seedPhraseChainPatterns below, and applyForSection/
    // restampPhraseChainSlot on subsequent bars) reuses the same frozen
    // avoid-id so they all agree with this exact resolution, rather than
    // reading currentSectionMotifPresetId (which would make them try to
    // avoid the very preset this section is using).
    currentSectionMotifPresetId = MotifEngine::stampMotifForSection(section,
        instanceRegistry.getAllInstances(),
        presetLibrary.getAllMotifPresets(),
        patternSyncServer,
        instanceStateTracker,
        homePatterns,
        getCurrentBar(),
        lockedStepLibrary,
        avoidMotifPresetIdForSection);

    logActivity(getCurrentBar(), "Stamped section '" + section.name + "' home patterns with preset '"
                                      + (currentSectionMotifPresetId.empty() ? "(none)" : currentSectionMotifPresetId)
                                      + "'");
}

void ComposerCore::seedPhraseChainPatterns(const BlueprintSection& section)
{
    if (section.archetype != "build" && section.archetype != "peak" && section.archetype != "release")
        return;

    std::map<std::string, int> homePatterns;
    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        homePatterns = motifHomePatternForSection;
    }

    MotifEngine::seedPhraseChainPatterns(section,
        instanceRegistry.getAllInstances(),
        presetLibrary.getAllMotifPresets(),
        patternSyncServer,
        instanceStateTracker,
        homePatterns,
        getCurrentBar(),
        lockedStepLibrary,
        avoidMotifPresetIdForSection);

    logActivity(getCurrentBar(), "Seeded phrase-chain patterns (P1/P2/P3) for section '" + section.name + "'");
}

void ComposerCore::resetRhythmBaselineForSection(const BlueprintSection& section)
{
    if (section.archetype != "presentation")
        return;

    std::map<std::string, int> homePatterns;
    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        homePatterns = motifHomePatternForSection;
    }

    const auto touchedInstances =
        MotifEngine::eligibleInstancesForArchetype(section.archetype, instanceRegistry.getAllInstances());

    for (const auto& instance : touchedInstances)
    {
        const auto homeIt = homePatterns.find(instance.id);
        const int homePattern = (homeIt != homePatterns.end()) ? homeIt->second : 0;
        if (homePattern <= 0)
            continue; // section wants this instance silent throughout - nothing to reset

        const int patternIndex = homePattern - 1;
        const int baseCC = CCMapping::patternBaseCC(patternIndex);
        if (baseCC < 0)
            continue;

        ccDispatcher.sendCC(instance.midiChannel, baseCC + static_cast<int>(CCMapping::MutationOffset::Transpose),
                             CCMapping::encodeTranspose(0));
        ccDispatcher.sendCC(instance.midiChannel, baseCC + static_cast<int>(CCMapping::MutationOffset::Rotation),
                             CCMapping::encodeRotation(0));
        ccDispatcher.sendCC(instance.midiChannel, baseCC + static_cast<int>(CCMapping::MutationOffset::Length),
                             CCMapping::encodeLength(CCMapping::kPatternSteps));

        instanceStateTracker.recordPattern(instance.id, patternIndex, 0, 0, CCMapping::kPatternSteps,
                                            false, false, false);

        // Also clear the pattern's real step content and the cache's belief
        // about it - not just the pattern-level parameters above. Without
        // this, stampMotifForSection's own register-continuity math (stamp
        // around the pattern's *existing* pitch center, so later sections
        // drift organically rather than snapping to a fixed default) instead
        // recenters around whatever the cache last believed this instance's
        // content was - which, on a replay after a prior heavily-drifted
        // playthrough, is itself already far out of range. Confirmed live,
        // 2026-08-21: the Transpose/Rotation/Length reset alone still left
        // one instance's stamped notes "sky-high," since only its parameters
        // had actually been reset, not its remembered pitch center. Clearing
        // here makes stampMotifForSection's own "existingEnabled.empty() ->
        // fall back to middle C" branch the one that actually fires.
        std::vector<StepSnapshot> clearedSteps(static_cast<size_t>(CCMapping::kPatternSteps), StepSnapshot {});

        // Locked steps (Setup mode, v1.2 Phase 2) survive even this reset -
        // "protected from automated writes" has to mean every automated
        // write, not most of them, or locking would be unreliable. Splice
        // in whatever the cache currently believes is there for each locked
        // index before sending, using the pre-clear cache read below.
        CachedPattern preClearCached;
        const bool hadPriorCache = patternSyncServer.getCache().get(instance.id, patternIndex, preClearCached);
        for (int lockedStepIndex : lockedStepLibrary.getLockedStepIndices(instance.id, patternIndex))
        {
            if (hadPriorCache && lockedStepIndex >= 0
                && (size_t) lockedStepIndex < preClearCached.snapshot.steps.size()
                && (size_t) lockedStepIndex < clearedSteps.size())
            {
                clearedSteps[(size_t) lockedStepIndex] = preClearCached.snapshot.steps[(size_t) lockedStepIndex];
            }
        }

        patternSyncServer.sendWriteFullPattern(instance.midiChannel, patternIndex, clearedSteps);

        PatternSnapshot clearedSnapshot;
        clearedSnapshot.patternIndex = patternIndex;
        clearedSnapshot.steps = clearedSteps;
        patternSyncServer.getCache().store(instance.id, clearedSnapshot, getCurrentBar());

        logActivity(getCurrentBar(), instance.id + ": Presentation baseline reset (Tr/Rot/Len + content cleared)");
    }
}

void ComposerCore::applyRhythmForSection(const BlueprintSection& section, int passIndex)
{
    if (section.archetype.empty())
        return;

    // Presentation's Rotation/Length and step content stay completely
    // frozen (user's own framing, 2026-08-21) - its gentle register drift
    // now comes entirely from applyContinuousMelodicCurve, which runs
    // regardless of this function, so there's nothing left for this pass-
    // based mechanism to do here at all.
    if (section.archetype == "presentation")
        return;

    const auto touchedInstances =
        MotifEngine::eligibleInstancesForArchetype(section.archetype, instanceRegistry.getAllInstances());
    if (touchedInstances.empty())
        return;

    std::map<std::string, int> homePatterns;
    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        homePatterns = motifHomePatternForSection;
    }

    // Rotation/Length alternate nudge direction by a different bit of
    // passIndex so the groove/loop length "breathes" instead of marching
    // toward an extreme and staying there (same restraint the note
    // transform rotation already has). Amount stays within MutationPolicy::
    // classifyWeight's Minor threshold (|amount| <= 2) - deliberately gentle,
    // matching this whole pass cadence's restraint.
    const int dimension = passIndex % 2;
    const int sign = ((passIndex / 2) % 2 == 0) ? 1 : -1;
    const int currentBar = getCurrentBar();

    for (const auto& instance : touchedInstances)
    {
        const auto homeIt = homePatterns.find(instance.id);
        const int homePattern = (homeIt != homePatterns.end()) ? homeIt->second : 0;
        if (homePattern <= 0)
            continue; // section wants this instance silent throughout - nothing to nudge

        Mutation mutation;
        mutation.targetInstance = instance.id;
        mutation.patternIndex = homePattern - 1;
        mutation.strength = "light";

        if (dimension == 0)
        {
            mutation.type = "rotation";
            mutation.amount = sign * 2;
        }
        else
        {
            mutation.type = "length";
            mutation.amount = sign * 1;
        }

        const bool applied = router.routeMutation(mutation, currentBar);
        const std::string signPrefix = mutation.amount >= 0 ? "+" : "";
        logActivity(currentBar, instance.id + ": " + mutation.type + " "
                                     + signPrefix + std::to_string(mutation.amount)
                                     + (applied ? "" : " (blocked by policy)"));
    }
}

void ComposerCore::applyContinuousMelodicCurve(const BlueprintSection& section, int currentBar)
{
    if (section.archetype.empty())
        return;

    const auto touchedInstances =
        MotifEngine::eligibleInstancesForArchetype(section.archetype, instanceRegistry.getAllInstances());
    if (touchedInstances.empty())
        return;

    std::map<std::string, int> homePatterns;
    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        homePatterns = motifHomePatternForSection;
    }

    // Energy -> amplitude, Tension -> register center, both sampled fresh
    // this bar (v1.2 Track B, docs/arc_dimension_mapping_concept.md) -
    // replaces the old flat constants entirely, including Presentation's
    // former special case, which now falls out naturally from its own
    // typically-lower Energy/Tension baseline instead of a separate
    // hardcoded amplitude.
    const float energyValue = arcSet.getArc("energy").evaluate(currentBar).value;
    const float tensionValue = arcSet.getArc("tension").evaluate(currentBar).value;

    const double amplitude = kMinMelodicCurveAmplitudeSemitones
        + static_cast<double>(energyValue)
              * (kMaxMelodicCurveAmplitudeSemitones - kMinMelodicCurveAmplitudeSemitones);
    const double registerCenter = static_cast<double>(tensionValue) * kMaxTensionRegisterPullSemitones;

    const double phase =
        (2.0 * juce::MathConstants<double>::pi * static_cast<double>(currentBar)) / kMelodicCurvePeriodBars;
    const int curveTargetTranspose = static_cast<int>(std::lround(registerCenter + amplitude * std::sin(phase)));

    for (const auto& instance : touchedInstances)
    {
        const auto homeIt = homePatterns.find(instance.id);
        const int homePattern = (homeIt != homePatterns.end()) ? homeIt->second : 0;
        if (homePattern <= 0)
            continue; // section wants this instance silent throughout - nothing to walk

        if (hasActiveRouteOverride(instance.id, homePattern - 1, ModulationParameter::Transpose))
            continue; // a user-authored modulation route already owns Transpose here

        const int taperedTranspose = MotifEngine::taperTransposeForPatternContent(
            patternSyncServer, instanceStateTracker, instance, homePattern - 1, curveTargetTranspose);
        router.routeContinuousTranspose(instance.id, homePattern - 1, taperedTranspose);
    }
}

void ComposerCore::applyContinuousSwing(const BlueprintSection& section, int currentBar)
{
    if (section.archetype.empty())
        return;

    const auto touchedInstances =
        MotifEngine::eligibleInstancesForArchetype(section.archetype, instanceRegistry.getAllInstances());
    if (touchedInstances.empty())
        return;

    // v1.28.0: Density no longer glides Swing continuously across the full
    // 0..75% range - MPL's own Swing knob narrowed to 3 states (Off/Triplet/
    // Shuffle, see CCMapping.h), so a linear multiply would just compute an
    // illegal in-between percent that gets silently snapped away at
    // encodeSwing time, leaving InstanceStateTracker's recorded value
    // disagreeing with what actually got sent. Bands Density into the same
    // 3 states directly instead, so the tracked value is always exactly one
    // of the three, matching reality.
    const float densityValue = arcSet.getArc("density").evaluate(currentBar).value;
    const float targetSwing =
        CCMapping::swingPercentForState(CCMapping::swingStateForNormalized(densityValue));

    // Unlike applyContinuousMelodicCurve, deliberately doesn't check
    // motifHomePatternForSection/skip resting instances - Swing is a global
    // per-instance groove setting, not tied to any one pattern, and reflects
    // this section's overall density character regardless of which specific
    // instance happens to be resting this particular bar. Keeping it current
    // means an instance already has the right feel the moment it resumes.
    for (const auto& instance : touchedInstances)
    {
        if (hasActiveRouteOverride(instance.id, 0, ModulationParameter::Swing)) // patternIndex ignored - global parameter
            continue; // a user-authored modulation route already owns Swing here

        router.routeContinuousSwing(instance.id, targetSwing);
    }
}

void ComposerCore::applyCoherenceDivergenceIfDue(const BlueprintSection& section, int currentBar)
{
    if (section.archetype.empty())
        return;

    const float coherenceValue = arcSet.getArc("coherence").evaluate(currentBar).value;
    const bool targetRetrograde = coherenceValue < kCoherenceRetrogradeThreshold;
    const bool targetM7 = coherenceValue < kCoherenceM7Threshold;

    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        if (hasCoherenceDivergenceState && lastCoherenceRetrograde == targetRetrograde
            && lastCoherenceM7 == targetM7)
            return; // no threshold crossing since last check - hold the current state
        hasCoherenceDivergenceState = true;
        lastCoherenceRetrograde = targetRetrograde;
        lastCoherenceM7 = targetM7;
    }

    const auto touchedInstances =
        MotifEngine::eligibleInstancesForArchetype(section.archetype, instanceRegistry.getAllInstances());
    if (touchedInstances.empty())
        return;

    std::map<std::string, int> homePatterns;
    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        homePatterns = motifHomePatternForSection;
    }

    for (const auto& instance : touchedInstances)
    {
        const auto homeIt = homePatterns.find(instance.id);
        const int homePattern = (homeIt != homePatterns.end()) ? homeIt->second : 0;
        if (homePattern <= 0)
            continue; // section wants this instance silent throughout - nothing to diverge

        const int patternIndex = homePattern - 1;
        const int baseCC = CCMapping::patternBaseCC(patternIndex);
        if (baseCC < 0)
            continue;

        InstanceParameterState trackedState;
        instanceStateTracker.getState(instance.id, trackedState);
        const auto& pattern = trackedState.patterns[patternIndex];

        // A user-authored modulation route can own Retrograde and/or M7
        // independently for this instance/pattern - only send (and only
        // record) whichever of the two this consumer still owns.
        const bool retrogradeOverridden =
            hasActiveRouteOverride(instance.id, patternIndex, ModulationParameter::Retrograde);
        const bool m7Overridden = hasActiveRouteOverride(instance.id, patternIndex, ModulationParameter::M7);

        if (!retrogradeOverridden)
            ccDispatcher.sendCC(instance.midiChannel,
                                 baseCC + static_cast<int>(CCMapping::MutationOffset::Retrograde),
                                 CCMapping::encodeRetrograde(targetRetrograde));
        if (!m7Overridden)
            ccDispatcher.sendCC(instance.midiChannel,
                                 baseCC + static_cast<int>(CCMapping::MutationOffset::M7),
                                 CCMapping::encodeM7(targetM7));

        instanceStateTracker.recordPattern(instance.id, patternIndex, pattern.transpose, pattern.rotation,
                                            pattern.length, pattern.inversion,
                                            retrogradeOverridden ? pattern.retrograde : targetRetrograde,
                                            m7Overridden ? pattern.m7 : targetM7);

        if (!retrogradeOverridden || !m7Overridden)
            logActivity(currentBar, instance.id + ": coherence divergence -> retrograde "
                                         + std::string(targetRetrograde ? "on" : "off") + ", m7 "
                                         + std::string(targetM7 ? "on" : "off") + " (coherence "
                                         + juce::String(coherenceValue, 2).toStdString() + ")");
    }
}

bool ComposerCore::hasActiveRouteOverride(const std::string& instanceId, int patternIndex,
                                           ModulationParameter parameter) const
{
    const bool isGlobalParameter = isGlobalModulationParameter(parameter);

    for (const auto& route : modulationRouteLibrary.getAllRoutes())
    {
        if (!route.enabled || route.parameter != parameter)
            continue;

        if (route.targetInstance != "*" && route.targetInstance != instanceId)
            continue;

        if (!isGlobalParameter && route.patternIndex != patternIndex)
            continue;

        return true;
    }

    return false;
}

void ComposerCore::sendModulationRouteUpdates(int currentBar)
{
    for (const auto& route : modulationRouteLibrary.getAllRoutes())
    {
        if (!route.enabled)
            continue;

        if (route.dispatchMode == ModulationDispatchMode::Sequence
            || route.dispatchMode == ModulationDispatchMode::BarCycle)
            continue; // driven by processStepTick / firePhraseCadenceIfDue instead, not this bar-sampled path

        float sample = arcSet.getArc(route.arcDimension).evaluate(currentBar).value;
        if (route.invert)
            sample = 1.0f - sample;
        sample = std::clamp(sample, 0.0f, 1.0f);

        std::vector<std::string> targetInstanceIds;
        if (route.targetInstance == "*")
        {
            for (const auto& instance : instanceRegistry.getAllInstances())
                targetInstanceIds.push_back(instance.id);
        }
        else
        {
            targetInstanceIds.push_back(route.targetInstance);
        }

        if (isContinuousModulationParameter(route.parameter))
        {
            float outputMin = route.outputMin;
            float outputMax = route.outputMax;
            if (outputMax <= outputMin)
            {
                // 0/0 (the struct's own default) means "use the parameter's
                // full domain" - resolved here, at dispatch time, so a route
                // saved before a domain constant changes doesn't silently
                // mean something else.
                switch (route.parameter)
                {
                    case ModulationParameter::Transpose:
                        outputMin = -static_cast<float>(CCMapping::kMaxTranspose);
                        outputMax = static_cast<float>(CCMapping::kMaxTranspose);
                        break;
                    case ModulationParameter::Rotation:
                        outputMin = 0.0f;
                        outputMax = static_cast<float>(CCMapping::kPatternSteps - 1);
                        break;
                    case ModulationParameter::Length:
                        outputMin = static_cast<float>(CCMapping::kMinPatternLoopLength);
                        outputMax = static_cast<float>(CCMapping::kPatternSteps);
                        break;
                    case ModulationParameter::Rate:
                        // Explicit, not left to fall through to Swing's 0..100
                        // domain below - Rate is a 3-state 0..2 int, not a
                        // percent, and would be silently corrupted otherwise.
                        outputMin = 0.0f;
                        outputMax = 2.0f;
                        break;
                    default: // Swing
                        outputMin = 0.0f;
                        outputMax = CCMapping::kMaxSwing;
                        break;
                }
            }

            const float value = outputMin + sample * (outputMax - outputMin);
            for (const auto& instanceId : targetInstanceIds)
                router.routeContinuousParameter(instanceId, route.patternIndex, route.parameter, value);
        }
        else
        {
            // ActivePattern bands into 4 states (0 = stop, 1-3 = pattern);
            // every other threshold parameter here is a plain on/off crossing.
            const int bandedValue = route.parameter == ModulationParameter::ActivePattern
                ? CCMapping::bandNormalized(sample, CCMapping::kMaxPatterns + 1)
                : (sample >= route.threshold ? 1 : 0);

            bool shouldSend = false;
            {
                std::lock_guard<std::mutex> lock(blueprintMutex);
                const auto it = modulationRouteThresholdState.find(route.id);
                const int previousValue = (it != modulationRouteThresholdState.end()) ? it->second : -1;
                if (previousValue != bandedValue)
                {
                    modulationRouteThresholdState[route.id] = bandedValue;
                    shouldSend = true;
                }
            }

            if (!shouldSend)
                continue;

            for (const auto& instanceId : targetInstanceIds)
                router.routeThresholdParameter(instanceId, route.patternIndex, route.parameter, bandedValue);
        }
    }
}

void ComposerCore::firePhraseCadenceIfDue(const BlueprintSection& section, int currentBar)
{
    for (const auto& route : modulationRouteLibrary.getAllRoutes())
    {
        if (!route.enabled || route.dispatchMode != ModulationDispatchMode::BarCycle)
            continue;

        if (route.sequenceValues.empty() || !isContinuousModulationParameter(route.parameter))
            continue; // nothing authored to step through, or not a parameter this mode applies to

        const int clampedPhraseLength = std::max(1, route.phraseLengthBars);
        // currentBar >= section.startBar always holds here (the caller only ever
        // passes the currently active section), so plain integer division floors
        // correctly with no need for the negative-aware wrap wrapRotation-style
        // helpers use elsewhere.
        const int phraseIndex = (currentBar - section.startBar) / clampedPhraseLength;

        std::vector<std::string> targetInstanceIds;
        if (route.targetInstance == "*")
        {
            for (const auto& instance : instanceRegistry.getAllInstances())
                targetInstanceIds.push_back(instance.id);
        }
        else
        {
            targetInstanceIds.push_back(route.targetInstance);
        }

        const size_t valueIndex = static_cast<size_t>(phraseIndex) % route.sequenceValues.size();
        const float value = route.sequenceValues[valueIndex];

        for (const auto& instanceId : targetInstanceIds)
        {
            const std::string stateKey = route.id + "|" + instanceId;

            bool shouldSend = false;
            {
                std::lock_guard<std::mutex> lock(blueprintMutex);
                const auto it = phraseCadenceLastFiredIndex.find(stateKey);
                const int lastFiredIndex = (it != phraseCadenceLastFiredIndex.end()) ? it->second : -1;
                if (lastFiredIndex != phraseIndex)
                {
                    phraseCadenceLastFiredIndex[stateKey] = phraseIndex;
                    shouldSend = true;
                }
            }

            if (shouldSend)
                router.routeContinuousParameter(instanceId, route.patternIndex, route.parameter, value);
        }
    }
}

void ComposerCore::processStepTick(double currentPpq)
{
    // Only meaningful while a blueprint is actually driving playback - find
    // the section covering the current bar exactly the way
    // advanceBlueprintIfNeeded does, and respect the same frozen (Absolute
    // content) gate every other continuous/threshold consumer here does.
    Blueprint blueprint;
    double sectionAnchor = 0.0;
    {
        std::lock_guard<std::mutex> lock(blueprintMutex);
        if (!hasCurrentBlueprint)
            return;
        blueprint = currentBlueprintData;
        sectionAnchor = currentSectionPpqAnchor;
    }

    const int currentBar = getCurrentBar();
    const BlueprintSection* activeSection = nullptr;
    for (const auto& section : blueprint.sections)
    {
        if (currentBar >= section.startBar && currentBar < section.startBar + section.durationBars)
        {
            activeSection = &section;
            break;
        }
    }

    if (activeSection == nullptr)
        return;

    const bool isFrozen = getContentMode() == ContentMode::Absolute && !activeSection->capturedContent.empty();
    if (isFrozen)
        return;

    for (const auto& route : modulationRouteLibrary.getAllRoutes())
    {
        if (!route.enabled || route.dispatchMode != ModulationDispatchMode::Sequence)
            continue;

        if (route.sequenceValues.empty() || !isContinuousModulationParameter(route.parameter))
            continue; // nothing authored to step through, or not a parameter Sequence mode applies to

        std::vector<std::string> targetInstanceIds;
        if (route.targetInstance == "*")
        {
            for (const auto& instance : instanceRegistry.getAllInstances())
                targetInstanceIds.push_back(instance.id);
        }
        else
        {
            targetInstanceIds.push_back(route.targetInstance);
        }

        for (const auto& instanceId : targetInstanceIds)
        {
            InstanceParameterState trackedState;
            instanceStateTracker.getState(instanceId, trackedState); // no prior state -> defaults (Length 16, Binary)

            const int trackedLength = isGlobalModulationParameter(route.parameter)
                ? CCMapping::effectiveStepCount(trackedState.gridMode)
                : trackedState.patterns[static_cast<size_t>(route.patternIndex)].length;
            const bool ternary = trackedState.gridMode == 1;
            const double stepLengthInPpq = StepClock::gridStepLengthInPpq(ternary, trackedState.rate);
            const double lookaheadPpq = kSequenceLookaheadStepFraction * stepLengthInPpq;

            // currentTrueIndex: the strict, already-elapsed loop-cycle index
            // (no lookahead) - the ground truth used to snap forward
            // correctly if playback jumped further than one cycle since the
            // last check (a host seek/relocate, or this route's very first
            // tick this section). candidateIndex: the NEXT index after
            // whatever was last fired - fired early, within
            // kSequenceLookaheadStepFraction of its own boundary, rather
            // than only after that boundary has passed (see this class's own
            // comment on kSequenceLookaheadStepFraction for why).
            const int currentTrueIndex = StepClock::loopCycleIndex(sectionAnchor, currentPpq, ternary, trackedLength,
                                                                     trackedState.rate);

            const std::string stateKey = route.id + "|" + instanceId;
            int indexToFire = -1;
            {
                std::lock_guard<std::mutex> lock(blueprintMutex);
                const auto it = sequenceRouteLastFiredIndex.find(stateKey);
                const int lastFiredIndex = (it != sequenceRouteLastFiredIndex.end()) ? it->second : -1;
                const int candidateIndex = lastFiredIndex + 1;

                if (currentTrueIndex > candidateIndex)
                {
                    indexToFire = currentTrueIndex; // jumped ahead - snap to the real current cycle directly
                }
                else
                {
                    const double candidateBoundaryPpq =
                        sectionAnchor + static_cast<double>(candidateIndex) * trackedLength * stepLengthInPpq;
                    if (currentPpq >= candidateBoundaryPpq - lookaheadPpq)
                        indexToFire = candidateIndex;
                }

                if (indexToFire < 0 || indexToFire == lastFiredIndex)
                    continue; // nothing due yet for this instance

                sequenceRouteLastFiredIndex[stateKey] = indexToFire;
            }

            const size_t valueIndex = static_cast<size_t>(indexToFire) % route.sequenceValues.size();
            const float value = route.sequenceValues[valueIndex];
            router.routeContinuousParameter(instanceId, route.patternIndex, route.parameter, value);
        }
    }
}

void ComposerCore::sendSectionModulatorValues(const BlueprintSection& section)
{
    for (const auto& modulatorValue : section.modulatorValues)
    {
        ModulatorTarget target;
        if (!modulatorTargetLibrary.getTargetById(modulatorValue.modulatorTargetId, target))
            continue; // authored value references a target that no longer exists - skip, don't guess

        ccDispatcher.sendCC(target.midiChannel, target.ccNumber, modulatorValue.value);
    }
}

float ComposerCore::getNarrativePositionAt(int currentBar) const
{
    std::lock_guard<std::mutex> lock(blueprintMutex);

    if (!hasCurrentBlueprint || currentBlueprintData.sections.empty())
        return 0.0f;

    int firstStart = currentBlueprintData.sections.front().startBar;
    int lastEnd = firstStart;

    for (const auto& section : currentBlueprintData.sections)
    {
        firstStart = std::min(firstStart, section.startBar);
        lastEnd = std::max(lastEnd, section.startBar + section.durationBars);
    }

    const int span = lastEnd - firstStart;

    if (span <= 0)
        return 0.0f;

    const float position = static_cast<float>(currentBar - firstStart) / static_cast<float>(span);
    return juce::jlimit(0.0f, 1.0f, position);
}

bool ComposerCore::isPitchFieldBroadcastEnabled() const { return pitchFieldBroadcastEnabled.load(); }

void ComposerCore::setPitchFieldBroadcastEnabled(bool enabled)
{
    pitchFieldBroadcastEnabled.store(enabled);
    // Always forget the last mask AND the last-broadcast bar: toggling off/on OR
    // re-calling this while already enabled both force a fresh re-broadcast next
    // bar (past the min-bars hold), so a late-joining OrchNoteFilter can be
    // brought current.
    lastBroadcastPitchFieldMask.store(-1);
    lastPitchFieldBroadcastBar.store(-1);
}

int ComposerCore::getPitchFieldBroadcastBaseCc() const { return pitchFieldBroadcastBaseCc.load(); }

void ComposerCore::setPitchFieldBroadcastBaseCc(int cc)
{
    pitchFieldBroadcastBaseCc.store(juce::jlimit(0, 118, cc));
    lastBroadcastPitchFieldMask.store(-1);
}

int ComposerCore::getPitchFieldBroadcastChannel() const { return pitchFieldBroadcastChannel.load(); }

void ComposerCore::setPitchFieldBroadcastChannel(int channel)
{
    pitchFieldBroadcastChannel.store(juce::jlimit(1, 16, channel));
    lastBroadcastPitchFieldMask.store(-1);
}

int ComposerCore::getLastBroadcastPitchFieldMask() const { return lastBroadcastPitchFieldMask.load(); }

int ComposerCore::getPitchFieldBroadcastMinBars() const { return pitchFieldBroadcastMinBars.load(); }

void ComposerCore::setPitchFieldBroadcastMinBars(int bars)
{
    pitchFieldBroadcastMinBars.store(juce::jlimit(0, 32, bars));
}

void ComposerCore::sendPitchFieldBroadcast(int currentBar)
{
    if (!pitchFieldBroadcastEnabled.load())
        return;

    const int baseCc = juce::jlimit(0, 118, pitchFieldBroadcastBaseCc.load());
    if (baseCc == 0)
        return;

    // Union of the pitch classes each registered instance's currently-active
    // pattern is sounding - its confirmed step content (InstancePatternCache)
    // put through the same M7 / Inversion / Transpose the pattern is carrying,
    // reduced to pitch class. Matches MPL's applyPatternTransformsToNote order.
    auto& cache = patternSyncServer.getCache();
    int mask = 0;

    for (const auto& instance : instanceRegistry.getAllInstances())
    {
        InstanceParameterState state;
        if (!instanceStateTracker.getState(instance.id, state))
            continue;

        const int activePattern = state.activePattern; // 1..3, 0 = section wants it silent
        if (activePattern < 1 || activePattern > 3)
            continue;

        const int patternIndex = activePattern - 1;

        CachedPattern cached;
        if (!cache.get(instance.id, patternIndex, cached))
            continue;

        const auto& patternState = state.patterns[static_cast<size_t>(patternIndex)];
        const int transpose = patternState.transpose;
        const bool m7 = patternState.m7;
        const bool inversion = patternState.inversion;
        // Retrograde and rotation only reorder steps - the pitch-class set is
        // unchanged - so they're deliberately not consulted here.

        for (const auto& step : cached.snapshot.steps)
        {
            if (!step.enabled)
                continue;

            // Mirror MPL's own transform order (applyPatternTransformsToNote):
            // stored note -> M7 -> Inversion -> Transpose, reduced to pitch class.
            int pc = ((step.note % 12) + 12) % 12;
            if (m7)
                pc = (pc * 7) % 12;
            if (inversion)
                pc = (12 - pc) % 12; // MPL inverts about note 60; 120 % 12 == 0
            pc = ((pc + transpose) % 12 + 12) % 12;
            mask |= (1 << pc);
        }
    }

    if (mask == 0)
        return; // no confirmed content yet - keep whatever was last broadcast

    if (mask == lastBroadcastPitchFieldMask.load())
        return;

    // Rate limit: hold a changed field for at least minBars bars, so the
    // per-bar transpose micro-nudges don't make the field flicker. The next
    // eligible bar picks up whatever the mask has become by then.
    const int minBars = juce::jlimit(0, 32, pitchFieldBroadcastMinBars.load());
    const int lastBar = lastPitchFieldBroadcastBar.load();
    if (minBars > 0 && lastBar >= 0 && (currentBar - lastBar) < minBars)
        return;

    lastBroadcastPitchFieldMask.store(mask);
    lastPitchFieldBroadcastBar.store(currentBar);

    // The 12-bit mask is packed into two CCs, not one-per-pitch-class: a
    // 12-CC block starting at any usable base runs into CC120/CC121 (All Sound
    // Off / Reset All Controllers, MIDI Channel Mode messages) which hard-mute
    // every downstream synth voice. baseCc carries pitch classes 0-6 (low 7
    // bits), baseCc+1 carries 7-11 (high 5 bits). OrchNoteFilter reassembles.
    const int channel = juce::jlimit(1, 16, pitchFieldBroadcastChannel.load());
    ccDispatcher.sendCC(channel, baseCc,     mask & 0x7F);
    ccDispatcher.sendCC(channel, baseCc + 1, (mask >> 7) & 0x1F);
}

void ComposerCore::sendModulatorTargetUpdates(int currentBar)
{
    for (const auto& target : modulatorTargetLibrary.getAllTargets())
    {
        if (target.mode != "arc")
            continue; // "section" mode is driven by sendSectionModulatorValues instead

        const float value = target.arcDimension == kNarrativePositionDimension
            ? getNarrativePositionAt(currentBar)
            : arcSet.getArc(target.arcDimension).evaluate(currentBar).value;

        const int ccValue = CCMapping::encodeFloat(value, 0.0f, 1.0f);
        ccDispatcher.sendCC(target.midiChannel, target.ccNumber, ccValue);
    }
}

namespace
{
    juce::var mcpOk(juce::DynamicObject* result)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("ok", true);
        obj->setProperty("result", juce::var(result));
        return juce::var(obj);
    }

    juce::var mcpError(const juce::String& message)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("ok", false);
        obj->setProperty("error", message);
        return juce::var(obj);
    }
}

juce::var ComposerCore::handleMcpBridgeRequest(const juce::var& request)
{
    const auto action = request["action"].toString();

    if (action == "getInstances")
    {
        auto* result = new juce::DynamicObject();

        juce::Array<juce::var> instancesArray;
        for (const auto& instance : instanceRegistry.getAllInstances())
        {
            auto* instanceObj = new juce::DynamicObject();
            instanceObj->setProperty("id", juce::String(instance.id));
            instanceObj->setProperty("channel", instance.midiChannel);
            instanceObj->setProperty("role",
                instance.role.empty() ? juce::String("unrestricted") : juce::String(instance.role));
            instanceObj->setProperty("enabled", instance.enabled);
            instancesArray.add(juce::var(instanceObj));
        }

        result->setProperty("instances", instancesArray);
        result->setProperty("coherence", getCurrentCoherence());
        return mcpOk(result);
    }

    if (action == "getAwareness")
    {
        auto* result = new juce::DynamicObject();

        juce::Array<juce::var> patternsArray;
        for (const auto& cached : patternSyncServer.getCache().getAll())
        {
            auto* patternObj = new juce::DynamicObject();
            patternObj->setProperty("instanceId", juce::String(cached.instanceId));
            patternObj->setProperty("patternIndex", cached.snapshot.patternIndex);
            patternObj->setProperty("capturedAtBar", cached.capturedAtBar);

            juce::Array<juce::var> stepsArray;
            for (size_t i = 0; i < cached.snapshot.steps.size(); ++i)
            {
                const auto& step = cached.snapshot.steps[i];
                auto* stepObj = new juce::DynamicObject();
                stepObj->setProperty("index", static_cast<int>(i));
                stepObj->setProperty("enabled", step.enabled);
                stepObj->setProperty("note", step.note);
                stepObj->setProperty("velocity", step.velocity);
                stepObj->setProperty("duration", step.duration);
                stepsArray.add(juce::var(stepObj));
            }
            patternObj->setProperty("steps", stepsArray);

            patternsArray.add(juce::var(patternObj));
        }

        result->setProperty("patterns", patternsArray);
        return mcpOk(result);
    }

    if (action == "getBlueprintStatus")
    {
        auto* result = new juce::DynamicObject();

        Blueprint blueprint;
        const bool hasBlueprint = getCurrentBlueprint(blueprint);
        result->setProperty("hasActiveBlueprint", hasBlueprint);
        result->setProperty("currentBar", getCurrentBar());
        result->setProperty("activeSectionId", juce::String(getActiveSectionId()));

        if (hasBlueprint)
        {
            result->setProperty("blueprintId", juce::String(blueprint.id));
            result->setProperty("blueprintName", juce::String(blueprint.name));

            juce::Array<juce::var> sectionsArray;
            for (const auto& section : blueprint.sections)
            {
                auto* sectionObj = new juce::DynamicObject();
                sectionObj->setProperty("id", juce::String(section.id));
                sectionObj->setProperty("name", juce::String(section.name));
                sectionObj->setProperty("sceneId", juce::String(section.sceneId));
                sectionObj->setProperty("startBar", section.startBar);
                sectionObj->setProperty("durationBars", section.durationBars);
                sectionObj->setProperty("archetype", juce::String(section.archetype));
                sectionsArray.add(juce::var(sectionObj));
            }
            result->setProperty("sections", sectionsArray);
        }

        return mcpOk(result);
    }

    if (action == "getMotifPresets")
    {
        auto* result = new juce::DynamicObject();

        juce::Array<juce::var> presetsArray;
        for (const auto& preset : presetLibrary.getAllMotifPresets())
        {
            auto* presetObj = new juce::DynamicObject();
            presetObj->setProperty("id", juce::String(preset.id));
            presetObj->setProperty("name", juce::String(preset.name));

            juce::Array<juce::var> tagsArray;
            for (const auto& tag : preset.tags)
                tagsArray.add(juce::var(juce::String(tag)));
            presetObj->setProperty("tags", tagsArray);

            juce::Array<juce::var> notesArray;
            for (const auto& note : preset.notes)
            {
                auto* noteObj = new juce::DynamicObject();
                noteObj->setProperty("semitoneOffset", note.semitoneOffset);
                noteObj->setProperty("relativeDuration", note.relativeDuration);
                noteObj->setProperty("relativeVelocity", note.relativeVelocity);
                notesArray.add(juce::var(noteObj));
            }
            presetObj->setProperty("notes", notesArray);

            presetsArray.add(juce::var(presetObj));
        }

        result->setProperty("presets", presetsArray);
        result->setProperty("applicationMode",
            getMotifApplicationMode() == MotifEngine::ApplicationMode::Phrase ? "Phrase" : "Nudge");
        return mcpOk(result);
    }

    if (action == "getStatus")
    {
        auto* result = new juce::DynamicObject();
        result->setProperty("currentBar", getCurrentBar());
        result->setProperty("coherence", getCurrentCoherence());
        result->setProperty("activeSectionId", juce::String(getActiveSectionId()));
        result->setProperty("registeredInstanceCount", static_cast<int>(instanceRegistry.getAllInstances().size()));
        result->setProperty("pitchFieldBroadcastEnabled", isPitchFieldBroadcastEnabled());
        result->setProperty("pitchFieldBroadcastMask", getLastBroadcastPitchFieldMask());
        return mcpOk(result);
    }

    if (action == "listBlueprints")
    {
        auto* result = new juce::DynamicObject();
        juce::Array<juce::var> blueprintsArray;
        for (const auto& blueprint : blueprintLibrary.getAllBlueprints())
        {
            auto* blueprintObj = new juce::DynamicObject();
            blueprintObj->setProperty("id", juce::String(blueprint.id));
            blueprintObj->setProperty("name", juce::String(blueprint.name));
            blueprintObj->setProperty("sectionCount", static_cast<int>(blueprint.sections.size()));
            blueprintsArray.add(juce::var(blueprintObj));
        }
        result->setProperty("blueprints", blueprintsArray);
        return mcpOk(result);
    }

    if (action == "listScenes")
    {
        auto* result = new juce::DynamicObject();
        juce::Array<juce::var> scenesArray;
        for (const auto& scene : sceneLibrary.getAllScenes())
        {
            auto* sceneObj = new juce::DynamicObject();
            sceneObj->setProperty("id", juce::String(scene.id));
            sceneObj->setProperty("name", juce::String(scene.name));
            sceneObj->setProperty("targetCount", static_cast<int>(scene.targets.size()));
            scenesArray.add(juce::var(sceneObj));
        }
        result->setProperty("scenes", scenesArray);
        return mcpOk(result);
    }

    if (action == "resyncInstance")
    {
        const auto instanceId = request["instanceId"].toString().toStdString();
        const int patternIndex = static_cast<int>(request["patternIndex"]);

        Instance instance;
        if (!instanceRegistry.getInstanceById(instanceId, instance))
            return mcpError("resyncInstance: unknown instance '" + juce::String(instanceId) + "'");

        const bool connected = patternSyncServer.isChannelConnected(instance.midiChannel);
        patternSyncServer.requestSync(instance.midiChannel, patternIndex);

        auto* result = new juce::DynamicObject();
        result->setProperty("requested", true);
        result->setProperty("channelConnected", connected);
        if (!connected)
            result->setProperty("note", "no IPC connection on this channel yet - request was sent but likely won't be answered");
        return mcpOk(result);
    }

    if (action == "writePattern")
    {
        const auto instanceId = request["instanceId"].toString().toStdString();
        const int patternIndex = static_cast<int>(request["patternIndex"]);

        Instance instance;
        if (!instanceRegistry.getInstanceById(instanceId, instance))
            return mcpError("writePattern: unknown instance '" + juce::String(instanceId) + "'");

        if (patternIndex < 0 || patternIndex >= CCMapping::kMaxPatterns)
            return mcpError("writePattern: patternIndex must be 0-2");

        // Mirrors ui/PatternSetupView::commitClicked's own Setup-mode write
        // exactly - direct IPC into MPL's real pattern storage, same as a
        // human dragging notes in the piano roll and clicking Commit. Fixed
        // 16-slot array, matching MPL's own patternLength - any array
        // shorter than that just leaves the remaining steps disabled.
        std::vector<StepSnapshot> steps(static_cast<size_t>(CCMapping::kPatternSteps));
        if (auto* stepsArray = request["steps"].getArray())
        {
            for (int i = 0; i < stepsArray->size() && i < CCMapping::kPatternSteps; ++i)
            {
                const auto& stepVar = (*stepsArray)[i];
                StepSnapshot step;
                step.enabled = static_cast<bool>(stepVar["enabled"]);
                step.note = juce::jlimit(0, 127, static_cast<int>(stepVar["note"]));
                step.velocity = juce::jlimit(0, 127, static_cast<int>(stepVar["velocity"]));
                step.duration = juce::jlimit(0, CCMapping::kPatternSteps, static_cast<int>(stepVar["duration"]));
                steps[static_cast<size_t>(i)] = step;
            }
        }

        if (!patternSyncServer.isChannelConnected(instance.midiChannel))
            return mcpError("writePattern: '" + juce::String(instanceId)
                + "' has no IPC channel connected yet - open it in MPL first");

        patternSyncServer.sendWriteFullPattern(instance.midiChannel, patternIndex, steps);
        patternSyncServer.requestSync(instance.midiChannel, patternIndex);

        auto* result = new juce::DynamicObject();
        result->setProperty("sent", true);
        result->setProperty("stepCount", static_cast<int>(steps.size()));
        result->setProperty("note", "resync requested - call get_awareness in a moment to confirm it landed");
        return mcpOk(result);
    }

    if (action == "setMotifApplicationMode")
    {
        const auto mode = request["mode"].toString();
        if (mode != "Nudge" && mode != "Phrase")
            return mcpError("setMotifApplicationMode: mode must be 'Nudge' or 'Phrase', got '" + mode + "'");

        setMotifApplicationMode(mode == "Phrase" ? MotifEngine::ApplicationMode::Phrase
                                                  : MotifEngine::ApplicationMode::Nudge);

        auto* result = new juce::DynamicObject();
        result->setProperty("applicationMode", mode);
        return mcpOk(result);
    }

    if (action == "setPitchFieldBroadcast")
    {
        if (request.hasProperty("minBars"))
            setPitchFieldBroadcastMinBars(static_cast<int>(request["minBars"]));
        if (request.hasProperty("baseCc"))
            setPitchFieldBroadcastBaseCc(static_cast<int>(request["baseCc"]));
        if (request.hasProperty("channel"))
            setPitchFieldBroadcastChannel(static_cast<int>(request["channel"]));
        // enabled last: its setter clears the rate-limit state so a re-enable
        // re-broadcasts immediately regardless of the other fields.
        if (request.hasProperty("enabled"))
            setPitchFieldBroadcastEnabled(static_cast<bool>(request["enabled"]));

        auto* result = new juce::DynamicObject();
        result->setProperty("enabled", isPitchFieldBroadcastEnabled());
        result->setProperty("baseCc", getPitchFieldBroadcastBaseCc());
        result->setProperty("channel", getPitchFieldBroadcastChannel());
        result->setProperty("minBars", getPitchFieldBroadcastMinBars());
        result->setProperty("lastMask", getLastBroadcastPitchFieldMask());
        return mcpOk(result);
    }

    if (action == "sendTestCC")
    {
        const auto instanceId = request["instanceId"].toString().toStdString();
        const int cc = static_cast<int>(request["cc"]);
        const int value = static_cast<int>(request["value"]);

        Instance instance;
        if (!instanceRegistry.getInstanceById(instanceId, instance))
            return mcpError("sendTestCC: unknown instance '" + juce::String(instanceId) + "'");

        if (cc < 0 || cc > 127 || value < 0 || value > 127)
            return mcpError("sendTestCC: cc and value must be 0-127");

        ccDispatcher.sendCC(instance.midiChannel, cc, value);

        auto* result = new juce::DynamicObject();
        result->setProperty("sent", true);
        result->setProperty("channel", instance.midiChannel);
        return mcpOk(result);
    }

    if (action == "sendMutation")
    {
        Mutation mutation;
        mutation.targetInstance = request["instanceId"].toString().toStdString();
        mutation.patternIndex = static_cast<int>(request["patternIndex"]);
        mutation.type = request["type"].toString().toStdString();
        mutation.amount = static_cast<int>(request["amount"]);

        std::string errorMessage;
        if (!Validation::isValidMutation(mutation, errorMessage))
            return mcpError("sendMutation: " + juce::String(errorMessage));

        const bool sent = router.routeMutation(mutation, getCurrentBar());

        auto* result = new juce::DynamicObject();
        result->setProperty("sent", sent);
        if (!sent)
            result->setProperty("reason", "blocked by policy budget for this bar, or unknown target instance");
        return mcpOk(result);
    }

    if (action == "commitBlueprint")
    {
        const auto blueprintId = request["blueprintId"].toString().toStdString();

        Blueprint blueprint;
        if (!blueprintLibrary.getBlueprintById(blueprintId, blueprint))
            return mcpError("commitBlueprint: unknown blueprint '" + juce::String(blueprintId) + "'");

        setCurrentBlueprint(blueprint);

        auto* result = new juce::DynamicObject();
        result->setProperty("blueprintId", juce::String(blueprint.id));
        result->setProperty("sectionCount", static_cast<int>(blueprint.sections.size()));
        return mcpOk(result);
    }

    if (action == "createScene")
    {
        const Scene scene = StateSerializer::varToScene(request["scene"]);

        std::string errorMessage;
        if (!Validation::isValidScene(scene, errorMessage))
            return mcpError("createScene: " + juce::String(errorMessage));

        sceneLibrary.addOrReplaceScene(scene);

        auto* result = new juce::DynamicObject();
        result->setProperty("sceneId", juce::String(scene.id));
        return mcpOk(result);
    }

    if (action == "createBlueprint")
    {
        const Blueprint blueprint = StateSerializer::varToBlueprint(request["blueprint"]);

        std::string errorMessage;
        if (!Validation::isValidBlueprint(blueprint, errorMessage))
            return mcpError("createBlueprint: " + juce::String(errorMessage));

        blueprintLibrary.addOrReplaceBlueprint(blueprint);

        auto* result = new juce::DynamicObject();
        result->setProperty("blueprintId", juce::String(blueprint.id));
        result->setProperty("sectionCount", static_cast<int>(blueprint.sections.size()));
        return mcpOk(result);
    }

    if (action == "createMotifPreset")
    {
        const MotifPreset preset = StateSerializer::varToMotifPreset(request["preset"]);

        std::string errorMessage;
        if (!Validation::isValidMotifPreset(preset, errorMessage))
            return mcpError("createMotifPreset: " + juce::String(errorMessage));

        presetLibrary.addOrReplaceMotifPreset(preset);

        auto* result = new juce::DynamicObject();
        result->setProperty("presetId", juce::String(preset.id));
        result->setProperty("noteCount", static_cast<int>(preset.notes.size()));
        return mcpOk(result);
    }

    if (action == "createRolePreset")
    {
        const RolePreset preset = StateSerializer::varToRolePreset(request["preset"]);

        std::string errorMessage;
        if (!Validation::isValidRolePreset(preset, errorMessage))
            return mcpError("createRolePreset: " + juce::String(errorMessage));

        presetLibrary.addOrReplaceRolePreset(preset);

        auto* result = new juce::DynamicObject();
        result->setProperty("presetId", juce::String(preset.id));
        return mcpOk(result);
    }

    if (action == "createRhythmicRelationshipPreset")
    {
        const RhythmicRelationshipPreset preset =
            StateSerializer::varToRhythmicRelationshipPreset(request["preset"]);

        std::string errorMessage;
        if (!Validation::isValidRhythmicRelationshipPreset(preset, errorMessage))
            return mcpError("createRhythmicRelationshipPreset: " + juce::String(errorMessage));

        presetLibrary.addOrReplaceRhythmicRelationshipPreset(preset);

        auto* result = new juce::DynamicObject();
        result->setProperty("presetId", juce::String(preset.id));
        result->setProperty("roleSlotCount", static_cast<int>(preset.roleSlots.size()));
        return mcpOk(result);
    }

    if (action == "createArcPreset")
    {
        const ArcPreset preset = StateSerializer::varToArcPreset(request["preset"]);

        std::string errorMessage;
        if (!Validation::isValidArcPreset(preset, errorMessage))
            return mcpError("createArcPreset: " + juce::String(errorMessage));

        presetLibrary.addOrReplaceArcPreset(preset);

        auto* result = new juce::DynamicObject();
        result->setProperty("presetId", juce::String(preset.id));
        result->setProperty("breakpointCount", static_cast<int>(preset.breakpoints.size()));
        return mcpOk(result);
    }

    if (action == "getPresets")
    {
        auto* result = new juce::DynamicObject();

        juce::Array<juce::var> roleArray;
        for (const auto& preset : presetLibrary.getAllRolePresets())
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("id", juce::String(preset.id));
            obj->setProperty("name", juce::String(preset.name));
            obj->setProperty("targetRole", juce::String(preset.targetRole));
            roleArray.add(juce::var(obj));
        }
        result->setProperty("rolePresets", roleArray);

        juce::Array<juce::var> rhythmicArray;
        for (const auto& preset : presetLibrary.getAllRhythmicRelationshipPresets())
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("id", juce::String(preset.id));
            obj->setProperty("name", juce::String(preset.name));
            obj->setProperty("roleSlotCount", static_cast<int>(preset.roleSlots.size()));
            rhythmicArray.add(juce::var(obj));
        }
        result->setProperty("rhythmicRelationshipPresets", rhythmicArray);

        juce::Array<juce::var> arcArray;
        for (const auto& preset : presetLibrary.getAllArcPresets())
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("id", juce::String(preset.id));
            obj->setProperty("name", juce::String(preset.name));
            obj->setProperty("breakpointCount", static_cast<int>(preset.breakpoints.size()));
            arcArray.add(juce::var(obj));
        }
        result->setProperty("arcPresets", arcArray);

        return mcpOk(result);
    }

    if (action == "applyRolePreset")
    {
        const auto presetId = request["presetId"].toString().toStdString();
        const auto sceneId = request["sceneId"].toString().toStdString();

        RolePreset preset;
        if (!presetLibrary.getRolePresetById(presetId, preset))
            return mcpError("applyRolePreset: unknown preset '" + juce::String(presetId) + "'");

        Scene scene;
        if (!sceneLibrary.getSceneById(sceneId, scene))
            return mcpError("applyRolePreset: unknown scene '" + juce::String(sceneId) + "'");

        const int affected =
            PresetResolver::applyRolePreset(preset, instanceRegistry.getAllInstances(), scene);
        sceneLibrary.addOrReplaceScene(scene);

        auto* result = new juce::DynamicObject();
        result->setProperty("sceneId", juce::String(sceneId));
        result->setProperty("affectedInstances", affected);
        return mcpOk(result);
    }

    if (action == "applyRhythmicRelationshipPreset")
    {
        const auto presetId = request["presetId"].toString().toStdString();
        const auto sceneId = request["sceneId"].toString().toStdString();

        RhythmicRelationshipPreset preset;
        if (!presetLibrary.getRhythmicRelationshipPresetById(presetId, preset))
            return mcpError("applyRhythmicRelationshipPreset: unknown preset '" + juce::String(presetId) + "'");

        Scene scene;
        if (!sceneLibrary.getSceneById(sceneId, scene))
            return mcpError("applyRhythmicRelationshipPreset: unknown scene '" + juce::String(sceneId) + "'");

        const int affected =
            PresetResolver::applyRhythmicRelationshipPreset(preset, instanceRegistry.getAllInstances(), scene);
        sceneLibrary.addOrReplaceScene(scene);

        auto* result = new juce::DynamicObject();
        result->setProperty("sceneId", juce::String(sceneId));
        result->setProperty("affectedInstances", affected);
        return mcpOk(result);
    }

    if (action == "applyArcPreset")
    {
        const auto presetId = request["presetId"].toString().toStdString();
        const auto targetArcName = request["targetArcName"].toString().toStdString();
        const int startBar = static_cast<int>(request["startBar"]);
        const int endBar = static_cast<int>(request["endBar"]);

        ArcPreset preset;
        if (!presetLibrary.getArcPresetById(presetId, preset))
            return mcpError("applyArcPreset: unknown preset '" + juce::String(presetId) + "'");

        const int stamped = PresetResolver::applyArcPreset(preset, targetArcName, startBar, endBar, arcSet);

        auto* result = new juce::DynamicObject();
        result->setProperty("targetArcName", juce::String(targetArcName));
        result->setProperty("breakpointsStamped", stamped);
        if (stamped == 0)
            result->setProperty("note", "check the preset has breakpoints, startBar < endBar, and targetArcName is a real dimension");
        return mcpOk(result);
    }

    if (action == "createModulatorTarget")
    {
        const ModulatorTarget target = StateSerializer::varToModulatorTarget(request["target"]);

        std::string errorMessage;
        if (!Validation::isValidModulatorTarget(target, errorMessage))
            return mcpError("createModulatorTarget: " + juce::String(errorMessage));

        modulatorTargetLibrary.addOrReplaceTarget(target);

        auto* result = new juce::DynamicObject();
        result->setProperty("targetId", juce::String(target.id));
        result->setProperty("note",
            "registered on Composer Mastermind's side only - pairing this CC/channel to an actual Bitwig "
            "modulator still needs a one-time 'Learn CC' gesture in Bitwig itself, done by a human");
        return mcpOk(result);
    }

    if (action == "getModulatorTargets")
    {
        auto* result = new juce::DynamicObject();
        juce::Array<juce::var> targetsArray;
        for (const auto& target : modulatorTargetLibrary.getAllTargets())
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("id", juce::String(target.id));
            obj->setProperty("ccNumber", target.ccNumber);
            obj->setProperty("midiChannel", target.midiChannel);
            obj->setProperty("mode", juce::String(target.mode));
            obj->setProperty("arcDimension", juce::String(target.arcDimension));
            targetsArray.add(juce::var(obj));
        }
        result->setProperty("targets", targetsArray);
        return mcpOk(result);
    }

    if (action == "createModulationRoute")
    {
        const ModulationRoute route = StateSerializer::varToModulationRoute(request["route"]);

        std::string errorMessage;
        if (!Validation::isValidModulationRoute(route, errorMessage))
            return mcpError("createModulationRoute: " + juce::String(errorMessage));

        modulationRouteLibrary.addOrReplaceRoute(route);

        auto* result = new juce::DynamicObject();
        result->setProperty("routeId", juce::String(route.id));
        return mcpOk(result);
    }

    if (action == "getModulationRoutes")
    {
        auto* result = new juce::DynamicObject();
        result->setProperty("routes", StateSerializer::modulationRoutesToVar(modulationRouteLibrary.getAllRoutes()));
        return mcpOk(result);
    }

    if (action == "generateBlueprint")
    {
        const auto blueprintId = request["blueprintId"].toString().toStdString();
        const auto drivingArcName = request["drivingArcName"].toString().toStdString();
        const auto baseSceneId = request["baseSceneId"].toString().toStdString();
        const bool commit = request.hasProperty("commit") && static_cast<bool>(request["commit"]);

        Scene baseScene;
        if (!sceneLibrary.getSceneById(baseSceneId, baseScene))
            return mcpError("generateBlueprint: unknown base scene '" + juce::String(baseSceneId) + "'");

        auto proposal = BlueprintGenerator::generate(blueprintId, drivingArcName, arcSet, baseScene,
            instanceRegistry.getAllInstances(), presetLibrary.getAllRolePresets(),
            presetLibrary.getAllRhythmicRelationshipPresets());

        if (proposal.blueprint.id.empty())
            return mcpError("generateBlueprint: '" + juce::String(drivingArcName)
                + "' needs at least 2 breakpoints to derive sections from - call setArc first");

        auto* result = new juce::DynamicObject();
        result->setProperty("blueprintId", juce::String(proposal.blueprint.id));
        result->setProperty("committed", commit);

        juce::Array<juce::var> sectionsArray;
        for (const auto& section : proposal.blueprint.sections)
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("id", juce::String(section.id));
            obj->setProperty("sceneId", juce::String(section.sceneId));
            obj->setProperty("startBar", section.startBar);
            obj->setProperty("durationBars", section.durationBars);
            obj->setProperty("archetype", juce::String(section.archetype));
            sectionsArray.add(juce::var(obj));
        }
        result->setProperty("sections", sectionsArray);
        result->setProperty("newSceneCount", static_cast<int>(proposal.newScenes.size()));

        if (commit)
        {
            for (const auto& scene : proposal.newScenes)
                sceneLibrary.addOrReplaceScene(scene);

            proposal.blueprint.arcCurves.clear();
            for (const auto& dimensionName : proposal.candidateArcSet.getArcNames())
            {
                const Arc arc = proposal.candidateArcSet.getArc(dimensionName);

                BlueprintArcCurve curve;
                curve.dimension = dimensionName;
                for (const auto& breakpoint : arc.getBreakpoints())
                    curve.points.push_back({ breakpoint.bar, breakpoint.value });
                proposal.blueprint.arcCurves.push_back(curve);
            }

            blueprintLibrary.addOrReplaceBlueprint(proposal.blueprint);
            setCurrentBlueprint(proposal.blueprint);
        }

        return mcpOk(result);
    }

    if (action == "setArc")
    {
        const auto dimension = request["dimension"].toString().toStdString();

        std::vector<ArcBreakpoint> points;
        if (auto* pointsArray = request["breakpoints"].getArray())
        {
            for (const auto& pointVar : *pointsArray)
            {
                ArcBreakpoint point;
                point.bar = static_cast<int>(pointVar["bar"]);
                point.value = static_cast<float>(static_cast<double>(pointVar["value"]));
                points.push_back(point);
            }
        }

        if (points.empty())
            return mcpError("setArc: 'breakpoints' must be a non-empty array of {bar, value}");

        Arc arc;
        arc.setBreakpoints(points);
        arcSet.setArc(dimension, arc);

        auto* result = new juce::DynamicObject();
        result->setProperty("dimension", juce::String(dimension));
        result->setProperty("breakpointCount", static_cast<int>(points.size()));
        return mcpOk(result);
    }

    if (action == "setScene")
    {
        const auto sceneId = request["sceneId"].toString().toStdString();

        Scene scene;
        if (!sceneLibrary.getSceneById(sceneId, scene))
            return mcpError("setScene: unknown scene '" + juce::String(sceneId) + "'");

        setCurrentScene(scene);
        fullRefresh();

        auto* result = new juce::DynamicObject();
        result->setProperty("sceneId", juce::String(scene.id));
        result->setProperty("durationBars", scene.durationBars);
        return mcpOk(result);
    }

    return mcpError("unknown action: " + action);
}
