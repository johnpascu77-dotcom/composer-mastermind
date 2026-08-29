#include "PatternSetupView.h"
#include "../plugin/PluginProcessor.h"
#include "InstanceColours.h"

namespace
{
    constexpr int kRowHeight = 26;
    constexpr int kMargin = 8;
    constexpr int kGap = 8;

    void repopulateInstances(juce::ComboBox& combo, const std::vector<Instance>& instances)
    {
        const auto previousSelection = combo.getText();
        combo.clear(juce::dontSendNotification);

        int itemId = 1;
        int selectId = 0;
        for (const auto& instance : instances)
        {
            const auto label = instance.name.empty() ? instance.id : instance.name;
            combo.addItem(label, itemId);
            if (label == previousSelection.toStdString())
                selectId = itemId;
            ++itemId;
        }

        if (selectId == 0 && !instances.empty())
            selectId = 1;

        combo.setSelectedId(selectId, juce::dontSendNotification);
    }
}

PatternSetupView::PatternSetupView(ComposerMastermindAudioProcessor& processor)
    : processorRef(processor)
{
    addAndMakeVisible(instanceCombo);
    instanceCombo.onChange = [this] { selectionChanged(); };

    addAndMakeVisible(patternCombo);
    patternCombo.addItem("P1", 1);
    patternCombo.addItem("P2", 2);
    patternCombo.addItem("P3", 3);
    patternCombo.setSelectedId(1, juce::dontSendNotification);
    patternCombo.onChange = [this] { selectionChanged(); };

    addAndMakeVisible(gridModeLabel);
    addAndMakeVisible(gridModeCombo);
    gridModeCombo.addItem("Binary", 1);
    gridModeCombo.addItem("Ternary", 2);
    gridModeCombo.setSelectedId(1, juce::dontSendNotification);
    gridModeCombo.onChange = [this] { gridModeChanged(); };

    addAndMakeVisible(commitButton);
    commitButton.onClick = [this] { commitClicked(); };

    addAndMakeVisible(discardButton);
    discardButton.onClick = [this] { discardClicked(); };

    addAndMakeVisible(statusLabel);
    statusLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Italic")));

    addAndMakeVisible(pianoRoll);
    pianoRoll.setMode(PianoRollView::Mode::Lane);
    pianoRoll.setEditable(true);
    pianoRoll.onStepEdited = [this] { setDirty(true); };
    pianoRoll.onLockToggleRequested = [this](int stepIndex) { lockToggleRequested(stepIndex); };

    refreshInstanceList();
    setDirty(false);
}

void PatternSetupView::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto pickerRow = area.removeFromTop(kRowHeight);
    instanceCombo.setBounds(pickerRow.removeFromLeft(180));
    pickerRow.removeFromLeft(kGap);
    patternCombo.setBounds(pickerRow.removeFromLeft(70));
    pickerRow.removeFromLeft(kGap);
    gridModeLabel.setBounds(pickerRow.removeFromLeft(32));
    gridModeCombo.setBounds(pickerRow.removeFromLeft(90));

    area.removeFromTop(kGap);

    auto actionRow = area.removeFromTop(kRowHeight);
    commitButton.setBounds(actionRow.removeFromLeft(130));
    actionRow.removeFromLeft(kGap);
    discardButton.setBounds(actionRow.removeFromLeft(130));
    actionRow.removeFromLeft(kGap);
    statusLabel.setBounds(actionRow);

    area.removeFromTop(kGap);
    pianoRoll.setBounds(area);
}

void PatternSetupView::refreshInstanceList()
{
    const auto instances = processorRef.getComposerCore().getInstanceRegistry().getAllInstances();
    repopulateInstances(instanceCombo, instances);
    loadCurrentSelection();
}

void PatternSetupView::selectionChanged()
{
    if (dirty)
    {
        statusLabel.setText(
            "Switched selection with unsaved changes still pending on the previous pattern - Commit or Discard first "
            "next time to avoid losing edits.",
            juce::dontSendNotification);
    }

    loadCurrentSelection();
}

void PatternSetupView::loadCurrentSelection()
{
    if (instanceCombo.getSelectedId() <= 0)
    {
        pianoRoll.setLanes({});
        setDirty(false);
        return;
    }

    const auto instances = processorRef.getComposerCore().getInstanceRegistry().getAllInstances();
    const auto instanceIndex = instanceCombo.getSelectedItemIndex();
    if (instanceIndex < 0 || instanceIndex >= (int) instances.size())
        return;

    const auto& instance = instances[(size_t) instanceIndex];
    const int patternIndex = juce::jlimit(0, 2, patternCombo.getSelectedItemIndex());

    PianoRollLane lane;
    lane.instanceId = instance.id;
    lane.displayName = instance.name.empty() ? juce::String(instance.id) : juce::String(instance.name);
    lane.colour = instanceColourForIndex(instanceIndex);
    lane.hasData = true;      // editing a never-dumped pattern is a valid blank starting point, not an error state
    lane.activePatternNumber = patternIndex + 1; // just so PianoRollView doesn't render the "stopped" placeholder

    // Grid mode: default the toggle to whatever's currently tracked for this
    // instance (a sensible starting point - probably what you'd want anyway)
    // but the toggle, not this tracked value, is what actually governs the
    // lane from here on - see gridModeCombo's own doc comment in the header.
    InstanceParameterState trackedState;
    processorRef.getComposerCore().getInstanceStateTracker().getState(instance.id, trackedState);
    gridModeCombo.setSelectedId(trackedState.gridMode == 1 ? 2 : 1, juce::dontSendNotification);
    lane.gridMode = trackedState.gridMode;

    CachedPattern cached;
    if (processorRef.getComposerCore().getPatternSyncServer().getCache().get(instance.id, patternIndex, cached))
        lane.steps = cached.snapshot.steps;

    if (lane.steps.size() < (size_t) CCMapping::kPatternSteps)
        lane.steps.resize((size_t) CCMapping::kPatternSteps);

    lane.lockedStepIndices =
        processorRef.getComposerCore().getLockedStepLibrary().getLockedStepIndices(instance.id, patternIndex);

    pianoRoll.setLanes({ lane });
    setDirty(false);
}

void PatternSetupView::gridModeChanged()
{
    if (pianoRoll.getLanes().empty())
        return;

    // Re-renders the existing in-memory lane at the new resolution without
    // touching a single note already drawn - notes at raw indices past the
    // new grid mode's real step count just become temporarily hidden/
    // unreachable (PianoRollView's own behavior, see its class comment),
    // not discarded; switching back reveals them again. This is a pending
    // change like any other - it isn't real until Commit pushes it to MPL.
    auto lane = pianoRoll.getLanes()[0];
    lane.gridMode = gridModeCombo.getSelectedId() == 2 ? 1 : 0;
    pianoRoll.setLanes({ lane });
    setDirty(true);
}

void PatternSetupView::lockToggleRequested(int stepIndex)
{
    if (instanceCombo.getSelectedId() <= 0 || pianoRoll.getLanes().empty())
        return;

    const auto instances = processorRef.getComposerCore().getInstanceRegistry().getAllInstances();
    const auto instanceIndex = instanceCombo.getSelectedItemIndex();
    if (instanceIndex < 0 || instanceIndex >= (int) instances.size())
        return;

    const auto& instance = instances[(size_t) instanceIndex];
    const int patternIndex = juce::jlimit(0, 2, patternCombo.getSelectedItemIndex());

    auto& lockedStepLibrary = processorRef.getComposerCore().getLockedStepLibrary();
    const bool currentlyLocked = lockedStepLibrary.isLocked(instance.id, patternIndex, stepIndex);
    lockedStepLibrary.setLocked(instance.id, patternIndex, stepIndex, !currentlyLocked);

    // Preserve any in-progress unsaved edits - re-read the lane as it
    // currently stands rather than reloading from the cache, which would
    // silently discard them.
    auto lane = pianoRoll.getLanes()[0];
    lane.lockedStepIndices = lockedStepLibrary.getLockedStepIndices(instance.id, patternIndex);
    pianoRoll.setLanes({ lane });

    statusLabel.setText((currentlyLocked ? juce::String("Unlocked") : juce::String("Locked")) + " step "
                             + juce::String(stepIndex + 1),
                         juce::dontSendNotification);
}

void PatternSetupView::commitClicked()
{
    if (instanceCombo.getSelectedId() <= 0 || pianoRoll.getLanes().empty())
        return;

    const auto instances = processorRef.getComposerCore().getInstanceRegistry().getAllInstances();
    const auto instanceIndex = instanceCombo.getSelectedItemIndex();
    if (instanceIndex < 0 || instanceIndex >= (int) instances.size())
        return;

    const auto& instance = instances[(size_t) instanceIndex];
    const juce::String instanceLabel = instance.name.empty() ? juce::String(instance.id) : juce::String(instance.name);
    const int patternIndex = juce::jlimit(0, 2, patternCombo.getSelectedItemIndex());
    const auto& lane = pianoRoll.getLanes()[0];
    const auto& steps = lane.steps;
    const int gridMode = lane.gridMode;

    auto& composerCore = processorRef.getComposerCore();
    auto& patternSync = composerCore.getPatternSyncServer();

    if (!patternSync.isChannelConnected(instance.midiChannel))
    {
        statusLabel.setText("Commit failed: '" + instanceLabel + "' is not connected (open it in MPL first)",
                             juce::dontSendNotification);
        return;
    }

    // The grid mode chosen in Setup mode (gridModeCombo) is authoritative
    // for what was just drawn - push it to MPL as a real CC alongside the
    // step content, rather than only ever reflecting whatever MPL happened
    // to already be in (user's own design, 2026-08-23: "MPL should obey
    // this from whatever state it is at that moment"). Sent unconditionally,
    // even if unchanged - idempotent on MPL's side, and simpler/more
    // reliable than tracking whether it actually differs. InstanceStateTracker
    // updated to match immediately after, same as every other CC send path
    // in this plugin (Router::routeScene, etc.) - preserves the instance's
    // other tracked fields (activePattern/swing) rather than clobbering them.
    auto& stateTracker = composerCore.getInstanceStateTracker();
    InstanceParameterState trackedState;
    stateTracker.getState(instance.id, trackedState);
    composerCore.getCCDispatcher().sendCC(instance.midiChannel, CCMapping::kGridMode,
                                           CCMapping::encodeGridMode(gridMode));
    stateTracker.recordGlobal(instance.id, trackedState.activePattern, gridMode, trackedState.swing, trackedState.rate);

    patternSync.sendWriteFullPattern(instance.midiChannel, patternIndex, steps);
    patternSync.requestSync(instance.midiChannel, patternIndex);

    setDirty(false);
    statusLabel.setText("Committed to '" + instanceLabel + "' P" + juce::String(patternIndex + 1) + " ("
                             + (gridMode == 1 ? juce::String("Ternary") : juce::String("Binary"))
                             + ") - resyncing to confirm",
                         juce::dontSendNotification);
}

void PatternSetupView::discardClicked()
{
    loadCurrentSelection();
    statusLabel.setText("Discarded - reloaded from the last known pattern content", juce::dontSendNotification);
}

void PatternSetupView::setDirty(bool isDirty)
{
    dirty = isDirty;
    instanceCombo.setEnabled(!dirty);
    patternCombo.setEnabled(!dirty);
    commitButton.setEnabled(dirty);
    discardButton.setEnabled(dirty);

    if (dirty)
        statusLabel.setText("Unsaved changes - Commit to write them to MPL, or Discard to reload", juce::dontSendNotification);
}
