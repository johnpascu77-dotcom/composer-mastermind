#include "SceneListComponent.h"
#include "../plugin/PluginProcessor.h"
#include "../util/Validation.h"
#include "../state/StateSnapshotStore.h"
#include "../midi/CCMapping.h"
#include <algorithm>

namespace
{
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 4;
    constexpr int kMargin = 10;

    // Sum of every row/gap laid out in resized(), kept in sync with it by
    // hand - see SceneListComponent::getPreferredHeight().
    constexpr int kPreferredHeight = 620;

    void styleSlider(juce::Slider& slider, double minValue, double maxValue, double step, double initial)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, kRowHeight);
        slider.setRange(minValue, maxValue, step);
        slider.setValue(initial, juce::dontSendNotification);
    }

    // Repopulates a combo from a list of ids, preserving the current
    // selection by text if it's still present.
    void repopulate(juce::ComboBox& combo, const std::vector<std::string>& ids, bool autoSelectFirst)
    {
        const auto previousSelection = combo.getText();
        combo.clear(juce::dontSendNotification);

        int itemId = 1;
        int selectId = 0;
        for (const auto& id : ids)
        {
            combo.addItem(id, itemId);
            if (id == previousSelection.toStdString())
                selectId = itemId;
            ++itemId;
        }

        if (selectId == 0 && autoSelectFirst && !ids.empty())
            selectId = 1;

        combo.setSelectedId(selectId, juce::dontSendNotification);
    }

    // Selects the item whose displayed text matches - used to restore a
    // combo's selection from a model value (nextSceneCombo's item text IS
    // the scene id) rather than an item id, which isn't stable across
    // repopulate() calls. Clears the selection if nothing matches (e.g. an
    // empty nextSceneId).
    void selectComboByText(juce::ComboBox& combo, const juce::String& text)
    {
        if (text.isEmpty())
        {
            combo.setSelectedId(0, juce::dontSendNotification);
            return;
        }

        for (int i = 0; i < combo.getNumItems(); ++i)
        {
            if (combo.getItemText(i) == text)
            {
                combo.setSelectedItemIndex(i, juce::dontSendNotification);
                return;
            }
        }
    }

    juce::String joinIds(const std::vector<std::string>& ids)
    {
        if (ids.empty())
            return "(none)";

        juce::String text;
        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (i > 0)
                text << ", ";
            text << ids[i];
        }
        return text;
    }

    juce::String describeOverride(const SceneInstanceOverride& override)
    {
        juce::String text = juce::String(override.targetInstance) + " (";
        juce::StringArray parts;
        if (override.gridMode >= 0)
            parts.add(override.gridMode == 0 ? "binary" : "ternary");
        if (override.activePattern >= 0)
            parts.add("pattern " + juce::String(override.activePattern));
        text << parts.joinIntoString(", ") << ")";
        return text;
    }
}

SceneListComponent::SceneListComponent(ComposerMastermindAudioProcessor& processor,
                                        std::function<void(const juce::String&)> status)
    : processorRef(processor), setStatus(std::move(status))
{
    addAndMakeVisible(sceneSectionLabel);
    sceneSectionLabel.setFont(juce::Font(16.0f, juce::Font::bold));

    addAndMakeVisible(activePatternSlider);
    styleSlider(activePatternSlider, 0.0, 3.0, 1.0, 1.0);
    addAndMakeVisible(activePatternLabel);

    addAndMakeVisible(gridModeCombo);
    gridModeCombo.addItem("Binary", 1);
    gridModeCombo.addItem("Ternary", 2);
    gridModeCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(gridModeLabel);

    addAndMakeVisible(swingCombo);
    swingCombo.addItem("Off", 1);
    swingCombo.addItem("Triplet", 2);
    swingCombo.addItem("Shuffle", 3);
    swingCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(swingLabel);

    addAndMakeVisible(durationBarsSlider);
    styleSlider(durationBarsSlider, 1.0, 32.0, 1.0, 4.0);
    addAndMakeVisible(durationBarsLabel);

    addAndMakeVisible(nextSceneCombo);
    nextSceneCombo.setTextWhenNothingSelected("(none - stays on this scene)");
    addAndMakeVisible(nextSceneLabel);

    addAndMakeVisible(targetInstanceCombo);
    targetInstanceCombo.setTextWhenNothingSelected("(pick instance)");
    addAndMakeVisible(targetInstanceLabel);

    addAndMakeVisible(addTargetButton);
    addTargetButton.onClick = [this] { addTargetClicked(); };

    addAndMakeVisible(targetAllButton);
    targetAllButton.onClick = [this] { targetAllClicked(); };

    addAndMakeVisible(pendingTargetsLabel);
    pendingTargetsLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    pendingTargetsLabel.setMinimumHorizontalScale(1.0f);

    addAndMakeVisible(overrideInstanceCombo);
    overrideInstanceCombo.setTextWhenNothingSelected("(pick instance)");
    addAndMakeVisible(overrideInstanceLabel);

    addAndMakeVisible(overrideGridModeCombo);
    overrideGridModeCombo.addItem("No Override", 1);
    overrideGridModeCombo.addItem("Binary", 2);
    overrideGridModeCombo.addItem("Ternary", 3);
    overrideGridModeCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(overrideActivePatternCombo);
    overrideActivePatternCombo.addItem("No Override", 1);
    overrideActivePatternCombo.addItem("Stop", 2);
    overrideActivePatternCombo.addItem("Pattern 1", 3);
    overrideActivePatternCombo.addItem("Pattern 2", 4);
    overrideActivePatternCombo.addItem("Pattern 3", 5);
    overrideActivePatternCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(addOverrideButton);
    addOverrideButton.onClick = [this] { addOverrideClicked(); };

    addAndMakeVisible(pendingOverridesLabel);
    pendingOverridesLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    pendingOverridesLabel.setMinimumHorizontalScale(1.0f);

    addAndMakeVisible(sendGlobalSceneButton);
    sendGlobalSceneButton.onClick = [this] { sendGlobalSceneClicked(); };

    addAndMakeVisible(clearPendingButton);
    clearPendingButton.onClick = [this] { clearPendingClicked(); };

    addAndMakeVisible(librarySectionLabel);
    librarySectionLabel.setFont(juce::Font(16.0f, juce::Font::bold));

    addAndMakeVisible(sceneNameInput);
    sceneNameInput.setTextToShowWhenEmpty("scene id/name, e.g. intro", juce::Colours::grey);

    addAndMakeVisible(saveSceneButton);
    saveSceneButton.onClick = [this] { saveSceneClicked(); };

    addAndMakeVisible(savedScenesCombo);

    addAndMakeVisible(loadSceneButton);
    loadSceneButton.onClick = [this] { loadSceneClicked(); };

    addAndMakeVisible(removeSceneButton);
    removeSceneButton.onClick = [this] { removeSceneClicked(); };

    addAndMakeVisible(sceneListDisplay);
    sceneListDisplay.setMultiLine(true);
    sceneListDisplay.setReadOnly(true);
    sceneListDisplay.setScrollbarsShown(true);
    sceneListDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    addAndMakeVisible(saveSnapshotButton);
    saveSnapshotButton.onClick = [this] { saveSnapshotToFileClicked(); };

    addAndMakeVisible(loadSnapshotButton);
    loadSnapshotButton.onClick = [this] { loadSnapshotFromFileClicked(); };

    addAndMakeVisible(contentModeLabel);
    addAndMakeVisible(contentModeCombo);
    contentModeCombo.addItem("Generative", 1);
    contentModeCombo.addItem("Absolute", 2);
    contentModeCombo.setSelectedId(
        processorRef.getComposerCore().getContentMode() == ContentMode::Absolute ? 2 : 1,
        juce::dontSendNotification);
    contentModeCombo.onChange = [this] { contentModeChanged(); };

    // Seed pendingTargets once with every currently-registered instance -
    // matches the old default behaviour (always targeted everyone) for
    // anyone who doesn't customize the target list themselves.
    for (const auto& instance : processorRef.getComposerCore().getInstanceRegistry().getAllInstances())
        pendingTargets.push_back(instance.id);

    refreshPendingDisplay();
    refreshAll();

    // [tooltips] generated by tools/plugin_guides/inject_tips.py from SceneListComponent.json
    sceneSectionLabel.setTooltip("The settings of the scene being built: sent to the targeted instances all at once.");
    activePatternSlider.setTooltip("Which MPL pattern plays: 0 stops the instance, 1 to 3 launch that pattern (switched on the bar line).");
    activePatternLabel.setTooltip("Which MPL pattern plays: 0 stops, 1-3 launch that pattern.");
    gridModeCombo.setTooltip("Binary (16 steps) or Ternary (12 steps) for the targeted instances.");
    gridModeLabel.setTooltip("Binary 16 or Ternary 12 steps.");
    swingCombo.setTooltip("Off, Triplet or Shuffle for the targeted instances. Three clean states, as in MPL.");
    swingLabel.setTooltip("Off, Triplet or Shuffle.");
    durationBarsSlider.setTooltip("How many bars a scene lasts when it is chained to a next scene. A section is what controls how long a scene plays when you use a blueprint.");
    durationBarsLabel.setTooltip("How long the scene lasts in a scene chain, in bars.");
    nextSceneCombo.setTooltip("Scene chain: after this scene's Duration Bars the named scene is sent automatically. None: the scene stays until something else changes it.");
    nextSceneLabel.setTooltip("The scene that follows this one in a chain.");
    targetInstanceCombo.setTooltip("Pick an instance, then Add Target: the scene is sent to the targeted instances only.");
    targetInstanceLabel.setTooltip("Which instances receive the scene.");
    addTargetButton.setTooltip("Add the picked instance to the pending targets.");
    targetAllButton.setTooltip("Target every registered instance (use it after you added instances).");
    pendingTargetsLabel.setTooltip("The instances the scene will be sent to.");
    overrideInstanceCombo.setTooltip("Pick an instance to give its own grid mode or pattern in this scene, different from the rest (a poly-rhythm).");
    overrideInstanceLabel.setTooltip("An instance that differs from the scene in this scene.");
    overrideGridModeCombo.setTooltip("The grid mode this instance uses instead of the scene's (for example Ternary against the others' Binary).");
    overrideActivePatternCombo.setTooltip("The pattern this instance plays instead of the scene's.");
    addOverrideButton.setTooltip("Add the override for the picked instance. Check Pending overrides before saving.");
    pendingOverridesLabel.setTooltip("The per-instance differences saved with this scene.");
    sendGlobalSceneButton.setTooltip("Send what is in this panel to the targets now, to try it without touching the saved library.");
    clearPendingButton.setTooltip("Empty the pending targets and overrides.");
    librarySectionLabel.setTooltip("The scenes saved in this project.");
    sceneNameInput.setTooltip("The name for the scene. Saving a name that exists updates that scene in place.");
    saveSceneButton.setTooltip("Save the settings in this panel (with its targets and overrides) in the library under the name typed.");
    savedScenesCombo.setTooltip("The saved scenes. Pick one, then Load.");
    loadSceneButton.setTooltip("Fill this panel with the picked scene (targets and overrides too). Nothing is sent.");
    removeSceneButton.setTooltip("Delete the picked scene from the library.");
    sceneListDisplay.setTooltip("The scene library: each scene with its number of targets.");
    saveSnapshotButton.setTooltip("Write everything Composer Mastermind holds (instances, scenes, blueprints, presets, targets) to one file, to keep or move to another project.");
    loadSnapshotButton.setTooltip("Restore a snapshot file: replaces what Composer Mastermind holds with its content.");
    contentModeLabel.setTooltip("Generative or Absolute content.");
    contentModeCombo.setTooltip("Generative: sections stamp patterns from Motif presets and mutations evolve them. Absolute: a section plays literal captured notes, written into MPL when it starts.");
    // [/tooltips]
}

void SceneListComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

int SceneListComponent::getPreferredHeight()
{
    return kPreferredHeight;
}

void SceneListComponent::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto nextRow = [&](int height) -> juce::Rectangle<int>
    {
        auto row = area.removeFromTop(height);
        area.removeFromTop(kRowGap);
        return row;
    };

    sceneSectionLabel.setBounds(nextRow(kRowHeight));

    auto patternRow = nextRow(kRowHeight);
    activePatternLabel.setBounds(patternRow.removeFromLeft(180));
    activePatternSlider.setBounds(patternRow);

    auto gridRow = nextRow(kRowHeight);
    gridModeLabel.setBounds(gridRow.removeFromLeft(180));
    gridModeCombo.setBounds(gridRow.removeFromLeft(150));

    auto swingRow = nextRow(kRowHeight);
    swingLabel.setBounds(swingRow.removeFromLeft(180));
    swingCombo.setBounds(swingRow.removeFromLeft(150));

    auto durationBarsRow = nextRow(kRowHeight);
    durationBarsLabel.setBounds(durationBarsRow.removeFromLeft(180));
    durationBarsSlider.setBounds(durationBarsRow);

    auto nextSceneRow = nextRow(kRowHeight);
    nextSceneLabel.setBounds(nextSceneRow.removeFromLeft(180));
    nextSceneCombo.setBounds(nextSceneRow);

    auto targetRow = nextRow(kRowHeight);
    targetInstanceLabel.setBounds(targetRow.removeFromLeft(80));
    targetInstanceCombo.setBounds(targetRow.removeFromLeft(160));
    targetRow.removeFromLeft(kMargin);
    addTargetButton.setBounds(targetRow.removeFromLeft(90));
    targetRow.removeFromLeft(kMargin);
    targetAllButton.setBounds(targetRow.removeFromLeft(90));

    pendingTargetsLabel.setBounds(nextRow(18));

    auto overrideRow = nextRow(kRowHeight);
    overrideInstanceLabel.setBounds(overrideRow.removeFromLeft(80));
    overrideInstanceCombo.setBounds(overrideRow.removeFromLeft(140));
    overrideRow.removeFromLeft(kMargin);
    overrideGridModeCombo.setBounds(overrideRow.removeFromLeft(110));
    overrideRow.removeFromLeft(kMargin);
    overrideActivePatternCombo.setBounds(overrideRow.removeFromLeft(110));
    overrideRow.removeFromLeft(kMargin);
    addOverrideButton.setBounds(overrideRow.removeFromLeft(100));

    pendingOverridesLabel.setBounds(nextRow(18));

    auto sendRow = nextRow(kRowHeight);
    sendGlobalSceneButton.setBounds(sendRow.removeFromLeft(150));
    sendRow.removeFromLeft(kMargin);
    clearPendingButton.setBounds(sendRow.removeFromLeft(120));

    area.removeFromTop(kMargin - kRowGap);
    librarySectionLabel.setBounds(nextRow(kRowHeight));

    auto saveSceneRow = nextRow(kRowHeight);
    sceneNameInput.setBounds(saveSceneRow.removeFromLeft(250));
    saveSceneRow.removeFromLeft(kMargin);
    saveSceneButton.setBounds(saveSceneRow.removeFromLeft(180));

    auto loadSceneRow = nextRow(kRowHeight);
    savedScenesCombo.setBounds(loadSceneRow.removeFromLeft(250));
    loadSceneRow.removeFromLeft(kMargin);
    loadSceneButton.setBounds(loadSceneRow.removeFromLeft(90));
    loadSceneRow.removeFromLeft(kMargin);
    removeSceneButton.setBounds(loadSceneRow.removeFromLeft(80));

    sceneListDisplay.setBounds(area.removeFromTop(90));
    area.removeFromTop(kRowGap);

    auto snapshotRow = nextRow(kRowHeight);
    saveSnapshotButton.setBounds(snapshotRow.removeFromLeft(200));
    snapshotRow.removeFromLeft(kMargin);
    loadSnapshotButton.setBounds(snapshotRow.removeFromLeft(200));
    snapshotRow.removeFromLeft(kMargin);
    contentModeLabel.setBounds(snapshotRow.removeFromLeft(85));
    contentModeCombo.setBounds(snapshotRow.removeFromLeft(120));
}

Scene SceneListComponent::buildSceneFromPanel(const std::string& id) const
{
    Scene scene;
    scene.id = id;
    scene.name = id;
    scene.durationBars = static_cast<int>(durationBarsSlider.getValue());
    scene.global.activePattern = static_cast<int>(activePatternSlider.getValue());
    scene.global.gridMode = gridModeCombo.getSelectedId() - 1;
    scene.global.swing = CCMapping::swingPercentForState(swingCombo.getSelectedId() - 1);

    if (nextSceneCombo.getSelectedId() > 0)
        scene.nextSceneId = nextSceneCombo.getText().toStdString();

    scene.targets = pendingTargets;
    scene.instanceOverrides = pendingOverrides;

    return scene;
}

void SceneListComponent::addTargetClicked()
{
    if (targetInstanceCombo.getSelectedId() <= 0)
    {
        setStatus("Add target skipped: pick an instance first");
        return;
    }

    const auto instanceId = targetInstanceCombo.getText().toStdString();
    if (std::find(pendingTargets.begin(), pendingTargets.end(), instanceId) == pendingTargets.end())
        pendingTargets.push_back(instanceId);

    refreshPendingDisplay();
}

void SceneListComponent::targetAllClicked()
{
    pendingTargets.clear();
    for (const auto& instance : processorRef.getComposerCore().getInstanceRegistry().getAllInstances())
        pendingTargets.push_back(instance.id);

    refreshPendingDisplay();
    setStatus("Targets set to all " + juce::String((int) pendingTargets.size()) + " registered instance(s)");
}

void SceneListComponent::addOverrideClicked()
{
    if (overrideInstanceCombo.getSelectedId() <= 0)
    {
        setStatus("Add override skipped: pick an instance first");
        return;
    }

    SceneInstanceOverride override;
    override.targetInstance = overrideInstanceCombo.getText().toStdString();

    if (overrideGridModeCombo.getSelectedId() > 1)
        override.gridMode = (overrideGridModeCombo.getSelectedId() == 2) ? 0 : 1;

    if (overrideActivePatternCombo.getSelectedId() > 1)
        override.activePattern = overrideActivePatternCombo.getSelectedId() - 2;

    for (auto& existing : pendingOverrides)
    {
        if (existing.targetInstance == override.targetInstance)
        {
            existing = override;
            refreshPendingDisplay();
            return;
        }
    }

    pendingOverrides.push_back(override);
    refreshPendingDisplay();
}

void SceneListComponent::clearPendingClicked()
{
    pendingTargets.clear();
    pendingOverrides.clear();
    refreshPendingDisplay();
    setStatus("Cleared pending targets and overrides");
}

void SceneListComponent::refreshPendingDisplay()
{
    pendingTargetsLabel.setText("Pending targets: " + joinIds(pendingTargets), juce::dontSendNotification);

    if (pendingOverrides.empty())
    {
        pendingOverridesLabel.setText("Pending overrides: (none)", juce::dontSendNotification);
        return;
    }

    juce::StringArray parts;
    for (const auto& override : pendingOverrides)
        parts.add(describeOverride(override));
    pendingOverridesLabel.setText("Pending overrides: " + parts.joinIntoString("; "), juce::dontSendNotification);
}

void SceneListComponent::sendGlobalSceneClicked()
{
    if (pendingTargets.empty())
    {
        setStatus("Send scene skipped: no targets - Add Target or Target All first");
        return;
    }

    const auto scene = buildSceneFromPanel("editor_test_scene");

    std::string errorMessage;
    if (!Validation::isValidScene(scene, errorMessage))
    {
        setStatus("Send scene failed: " + errorMessage);
        return;
    }

    processorRef.getComposerCore().setCurrentScene(scene);
    processorRef.getComposerCore().fullRefresh();

    juce::String overrideNote;
    if (!scene.instanceOverrides.empty())
        overrideNote = " (" + juce::String((int) scene.instanceOverrides.size()) + " override(s))";

    juce::String chainNote;
    if (!scene.nextSceneId.empty())
        chainNote = ", chains to '" + juce::String(scene.nextSceneId) + "' after "
                        + juce::String(scene.durationBars) + " bar(s)";

    setStatus("Sent scene to " + juce::String((int) scene.targets.size()) + " instance(s)" + overrideNote + chainNote);
}

void SceneListComponent::saveSceneClicked()
{
    const auto sceneId = sceneNameInput.getText().trim().toStdString();
    if (sceneId.empty())
    {
        setStatus("Save scene failed: enter a scene name/id first");
        return;
    }

    const auto scene = buildSceneFromPanel(sceneId);

    std::string errorMessage;
    if (!Validation::isValidScene(scene, errorMessage))
    {
        setStatus("Save scene failed: " + errorMessage);
        return;
    }

    processorRef.getComposerCore().getSceneLibrary().addOrReplaceScene(scene);
    setStatus("Saved scene '" + juce::String(sceneId) + "' to library");
    refreshAll();
}

void SceneListComponent::loadSceneClicked()
{
    if (savedScenesCombo.getSelectedId() <= 0)
    {
        setStatus("Load scene skipped: no saved scene selected");
        return;
    }

    const auto sceneId = savedScenesCombo.getText().toStdString();

    Scene scene;
    if (!processorRef.getComposerCore().getSceneLibrary().getSceneById(sceneId, scene))
    {
        setStatus("Load scene failed: '" + juce::String(sceneId) + "' not found");
        return;
    }

    // Populates the whole panel - global fields plus both pending lists -
    // from an already-saved scene, so editing one field doesn't mean
    // rebuilding the target list and every override from scratch. Doesn't
    // send anything; use Send Scene Now for that once loaded (or after
    // editing).
    sceneNameInput.setText(scene.id, juce::dontSendNotification);
    activePatternSlider.setValue(scene.global.activePattern, juce::dontSendNotification);
    gridModeCombo.setSelectedId(scene.global.gridMode + 1, juce::dontSendNotification);
    swingCombo.setSelectedId(CCMapping::swingStateForPercent(scene.global.swing) + 1, juce::dontSendNotification);
    durationBarsSlider.setValue(scene.durationBars, juce::dontSendNotification);
    selectComboByText(nextSceneCombo, juce::String(scene.nextSceneId));

    pendingTargets = scene.targets;
    pendingOverrides = scene.instanceOverrides;
    refreshPendingDisplay();

    setStatus("Loaded scene '" + juce::String(sceneId) + "' for editing (" + juce::String((int) scene.targets.size())
                  + " target(s), " + juce::String((int) scene.instanceOverrides.size())
                  + " override(s)) - change fields, then Save Current Scene to update it or Send Scene Now to route it");
}

void SceneListComponent::removeSceneClicked()
{
    if (savedScenesCombo.getSelectedId() <= 0)
    {
        setStatus("Remove scene skipped: no saved scene selected");
        return;
    }

    const auto sceneId = savedScenesCombo.getText().toStdString();
    processorRef.getComposerCore().getSceneLibrary().removeScene(sceneId);
    setStatus("Removed scene '" + juce::String(sceneId) + "' from library");
    refreshAll();
}

void SceneListComponent::saveSnapshotToFileClicked()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Composer Mastermind Snapshot",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.json");

    constexpr auto chooserFlags = juce::FileBrowserComponent::saveMode
                                   | juce::FileBrowserComponent::canSelectFiles
                                   | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file == juce::File{})
            return;

        const auto json = StateSnapshotStore::createSnapshot(processorRef.getComposerCore());
        if (file.replaceWithText(json))
            setStatus("Saved snapshot to " + file.getFullPathName());
        else
            setStatus("Failed to save snapshot to " + file.getFullPathName());
    });
}

void SceneListComponent::loadSnapshotFromFileClicked()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Composer Mastermind Snapshot",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.json");

    constexpr auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file == juce::File{})
            return;

        std::string errorMessage;
        if (StateSnapshotStore::restoreSnapshot(file.loadFileAsString(), processorRef.getComposerCore(), errorMessage))
        {
            setStatus("Loaded snapshot from " + file.getFullPathName());
            refreshAll();
        }
        else
        {
            setStatus("Failed to load snapshot: " + juce::String(errorMessage));
        }
    });
}

void SceneListComponent::contentModeChanged()
{
    const auto mode = contentModeCombo.getSelectedId() == 2 ? ContentMode::Absolute : ContentMode::Generative;
    processorRef.getComposerCore().setContentMode(mode);
    setStatus(mode == ContentMode::Absolute
                   ? "Content Mode: Absolute - sections with captured content now play back verbatim and frozen"
                   : "Content Mode: Generative - sections stamp from motif presets as usual");
}

void SceneListComponent::refreshAll()
{
    // Re-synced here (2026-08-23) rather than only at construction - Content
    // Mode is now also changeable from the Score View's own mirrored header
    // control (PrimaryView), and EditorView's 300ms refresh timer already
    // calls this unconditionally, so this is the natural place to pick up a
    // change made from the other side without adding a second polling path.
    contentModeCombo.setSelectedId(
        processorRef.getComposerCore().getContentMode() == ContentMode::Absolute ? 2 : 1,
        juce::dontSendNotification);

    std::vector<std::string> instanceIds;
    for (const auto& instance : processorRef.getComposerCore().getInstanceRegistry().getAllInstances())
        instanceIds.push_back(instance.id);
    repopulate(overrideInstanceCombo, instanceIds, false);
    repopulate(targetInstanceCombo, instanceIds, false);

    const auto scenes = processorRef.getComposerCore().getSceneLibrary().getAllScenes();

    juce::String text;
    for (const auto& scene : scenes)
    {
        text << scene.id << "  targets=" << (int) scene.targets.size()
             << "  overrides=" << (int) scene.instanceOverrides.size();

        if (!scene.nextSceneId.empty())
            text << "  -> " << scene.nextSceneId << " after " << scene.durationBars << " bar(s)";

        text << "\n";
    }

    if (text.isEmpty())
        text = "(no saved scenes)";

    sceneListDisplay.setText(text, juce::dontSendNotification);

    std::vector<std::string> sceneIds;
    for (const auto& scene : scenes)
        sceneIds.push_back(scene.id);

    repopulate(savedScenesCombo, sceneIds, false);
    repopulate(nextSceneCombo, sceneIds, false);
}
