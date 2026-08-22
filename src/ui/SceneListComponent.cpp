#include "SceneListComponent.h"
#include "../plugin/PluginProcessor.h"
#include "../util/Validation.h"
#include "../state/StateSnapshotStore.h"
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

    addAndMakeVisible(swingSlider);
    styleSlider(swingSlider, 0.0, 75.0, 1.0, 0.0);
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

    // Seed pendingTargets once with every currently-registered instance -
    // matches the old default behaviour (always targeted everyone) for
    // anyone who doesn't customize the target list themselves.
    for (const auto& instance : processorRef.getComposerCore().getInstanceRegistry().getAllInstances())
        pendingTargets.push_back(instance.id);

    refreshPendingDisplay();
    refreshAll();
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
    swingSlider.setBounds(swingRow);

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
}

Scene SceneListComponent::buildSceneFromPanel(const std::string& id) const
{
    Scene scene;
    scene.id = id;
    scene.name = id;
    scene.durationBars = static_cast<int>(durationBarsSlider.getValue());
    scene.global.activePattern = static_cast<int>(activePatternSlider.getValue());
    scene.global.gridMode = gridModeCombo.getSelectedId() - 1;
    scene.global.swing = static_cast<float>(swingSlider.getValue());

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
    swingSlider.setValue(scene.global.swing, juce::dontSendNotification);
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

void SceneListComponent::refreshAll()
{
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
