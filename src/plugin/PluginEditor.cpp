#include "PluginEditor.h"
#include "../model/Instance.h"
#include "../model/Scene.h"
#include "../model/Mutation.h"
#include "../util/Validation.h"
#include "../state/StateSnapshotStore.h"

namespace
{
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 4;
    constexpr int kMargin = 10;

    void styleSlider(juce::Slider& slider, double minValue, double maxValue, double step, double initial)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, kRowHeight);
        slider.setRange(minValue, maxValue, step);
        slider.setValue(initial, juce::dontSendNotification);
    }
}

ComposerMastermindAudioProcessorEditor::ComposerMastermindAudioProcessorEditor(ComposerMastermindAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(640, 860);

    // --- Instance manager ---
    addAndMakeVisible(instanceSectionLabel);
    instanceSectionLabel.setFont(juce::Font(16.0f, juce::Font::bold));

    addAndMakeVisible(instanceIdInput);
    instanceIdInput.setTextToShowWhenEmpty("instance id, e.g. mpl_1", juce::Colours::grey);

    addAndMakeVisible(instanceChannelSlider);
    styleSlider(instanceChannelSlider, 1.0, 16.0, 1.0, 1.0);
    addAndMakeVisible(instanceChannelLabel);

    addAndMakeVisible(addInstanceButton);
    addInstanceButton.addListener(this);

    addAndMakeVisible(removeInstanceButton);
    removeInstanceButton.addListener(this);

    addAndMakeVisible(instanceListDisplay);
    instanceListDisplay.setMultiLine(true);
    instanceListDisplay.setReadOnly(true);
    instanceListDisplay.setScrollbarsShown(true);
    instanceListDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    // --- Global scene test ---
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

    addAndMakeVisible(overrideInstanceCombo);
    overrideInstanceCombo.setTextWhenNothingSelected("(none - all instances use Grid Mode above)");
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

    addAndMakeVisible(sendGlobalSceneButton);
    sendGlobalSceneButton.addListener(this);

    // --- Mutation test ---
    addAndMakeVisible(mutationSectionLabel);
    mutationSectionLabel.setFont(juce::Font(16.0f, juce::Font::bold));

    addAndMakeVisible(mutationTargetCombo);
    addAndMakeVisible(mutationTargetLabel);

    addAndMakeVisible(mutationPatternCombo);
    mutationPatternCombo.addItem("Pattern 1", 1);
    mutationPatternCombo.addItem("Pattern 2", 2);
    mutationPatternCombo.addItem("Pattern 3", 3);
    mutationPatternCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(mutationPatternLabel);

    addAndMakeVisible(mutationTypeCombo);
    mutationTypeCombo.addItem("Transpose", 1);
    mutationTypeCombo.addItem("Rotation", 2);
    mutationTypeCombo.addItem("Length", 3);
    mutationTypeCombo.addItem("Inversion", 4);
    mutationTypeCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(mutationTypeLabel);

    addAndMakeVisible(mutationAmountSlider);
    styleSlider(mutationAmountSlider, -48.0, 48.0, 1.0, 0.0);
    addAndMakeVisible(mutationAmountLabel);

    addAndMakeVisible(sendMutationButton);
    sendMutationButton.addListener(this);

    // --- Scene library ---
    addAndMakeVisible(librarySectionLabel);
    librarySectionLabel.setFont(juce::Font(16.0f, juce::Font::bold));

    addAndMakeVisible(sceneNameInput);
    sceneNameInput.setTextToShowWhenEmpty("scene id/name, e.g. intro", juce::Colours::grey);

    addAndMakeVisible(saveSceneButton);
    saveSceneButton.addListener(this);

    addAndMakeVisible(savedScenesCombo);

    addAndMakeVisible(loadSceneButton);
    loadSceneButton.addListener(this);

    addAndMakeVisible(removeSceneButton);
    removeSceneButton.addListener(this);

    addAndMakeVisible(sceneListDisplay);
    sceneListDisplay.setMultiLine(true);
    sceneListDisplay.setReadOnly(true);
    sceneListDisplay.setScrollbarsShown(true);
    sceneListDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    addAndMakeVisible(saveSnapshotButton);
    saveSnapshotButton.addListener(this);

    addAndMakeVisible(loadSnapshotButton);
    loadSnapshotButton.addListener(this);

    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setFont(juce::Font(13.0f, juce::Font::italic));

    refreshInstanceList();
    refreshSceneList();
}

ComposerMastermindAudioProcessorEditor::~ComposerMastermindAudioProcessorEditor()
{
}

void ComposerMastermindAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void ComposerMastermindAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto nextRow = [&](int height) -> juce::Rectangle<int>
    {
        auto row = area.removeFromTop(height);
        area.removeFromTop(kRowGap);
        return row;
    };

    instanceSectionLabel.setBounds(nextRow(kRowHeight));

    auto instanceControlsRow = nextRow(kRowHeight);
    instanceIdInput.setBounds(instanceControlsRow.removeFromLeft(200));
    instanceControlsRow.removeFromLeft(kMargin);
    instanceChannelLabel.setBounds(instanceControlsRow.removeFromLeft(60));
    instanceChannelSlider.setBounds(instanceControlsRow.removeFromLeft(150));
    instanceControlsRow.removeFromLeft(kMargin);
    addInstanceButton.setBounds(instanceControlsRow.removeFromLeft(100));
    instanceControlsRow.removeFromLeft(kMargin);
    removeInstanceButton.setBounds(instanceControlsRow.removeFromLeft(80));

    instanceListDisplay.setBounds(nextRow(100));

    area.removeFromTop(kMargin);
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

    auto overrideRow = nextRow(kRowHeight);
    overrideInstanceLabel.setBounds(overrideRow.removeFromLeft(140));
    overrideInstanceCombo.setBounds(overrideRow.removeFromLeft(160));
    overrideRow.removeFromLeft(kMargin);
    overrideGridModeCombo.setBounds(overrideRow.removeFromLeft(120));
    overrideRow.removeFromLeft(kMargin);
    overrideActivePatternCombo.setBounds(overrideRow.removeFromLeft(120));

    sendGlobalSceneButton.setBounds(nextRow(kRowHeight).removeFromLeft(150));

    area.removeFromTop(kMargin);
    mutationSectionLabel.setBounds(nextRow(kRowHeight));

    auto mutationTargetRow = nextRow(kRowHeight);
    mutationTargetLabel.setBounds(mutationTargetRow.removeFromLeft(180));
    mutationTargetCombo.setBounds(mutationTargetRow);

    auto mutationPatternRow = nextRow(kRowHeight);
    mutationPatternLabel.setBounds(mutationPatternRow.removeFromLeft(180));
    mutationPatternCombo.setBounds(mutationPatternRow.removeFromLeft(150));

    auto mutationTypeRow = nextRow(kRowHeight);
    mutationTypeLabel.setBounds(mutationTypeRow.removeFromLeft(180));
    mutationTypeCombo.setBounds(mutationTypeRow.removeFromLeft(150));

    auto mutationAmountRow = nextRow(kRowHeight);
    mutationAmountLabel.setBounds(mutationAmountRow.removeFromLeft(180));
    mutationAmountSlider.setBounds(mutationAmountRow);

    sendMutationButton.setBounds(nextRow(kRowHeight).removeFromLeft(150));

    area.removeFromTop(kMargin);
    librarySectionLabel.setBounds(nextRow(kRowHeight));

    auto saveSceneRow = nextRow(kRowHeight);
    sceneNameInput.setBounds(saveSceneRow.removeFromLeft(250));
    saveSceneRow.removeFromLeft(kMargin);
    saveSceneButton.setBounds(saveSceneRow.removeFromLeft(180));

    auto loadSceneRow = nextRow(kRowHeight);
    savedScenesCombo.setBounds(loadSceneRow.removeFromLeft(250));
    loadSceneRow.removeFromLeft(kMargin);
    loadSceneButton.setBounds(loadSceneRow.removeFromLeft(120));
    loadSceneRow.removeFromLeft(kMargin);
    removeSceneButton.setBounds(loadSceneRow.removeFromLeft(80));

    sceneListDisplay.setBounds(nextRow(90));

    auto snapshotRow = nextRow(kRowHeight);
    saveSnapshotButton.setBounds(snapshotRow.removeFromLeft(200));
    snapshotRow.removeFromLeft(kMargin);
    loadSnapshotButton.setBounds(snapshotRow.removeFromLeft(200));

    area.removeFromTop(kMargin);
    statusLabel.setBounds(nextRow(kRowHeight));
}

void ComposerMastermindAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &addInstanceButton)
        addInstanceClicked();
    else if (button == &removeInstanceButton)
        removeInstanceClicked();
    else if (button == &sendGlobalSceneButton)
        sendGlobalSceneClicked();
    else if (button == &sendMutationButton)
        sendMutationClicked();
    else if (button == &saveSceneButton)
        saveSceneClicked();
    else if (button == &loadSceneButton)
        loadSceneClicked();
    else if (button == &removeSceneButton)
        removeSceneClicked();
    else if (button == &saveSnapshotButton)
        saveSnapshotToFileClicked();
    else if (button == &loadSnapshotButton)
        loadSnapshotFromFileClicked();
}

void ComposerMastermindAudioProcessorEditor::addInstanceClicked()
{
    const auto id = instanceIdInput.getText().trim().toStdString();
    const int channel = static_cast<int>(instanceChannelSlider.getValue());

    Instance instance(id, id, channel, "mpl");

    std::string errorMessage;
    if (!Validation::isValidInstance(instance, errorMessage))
    {
        statusLabel.setText("Add instance failed: " + errorMessage, juce::dontSendNotification);
        return;
    }

    if (!processorRef.getComposerCore().getInstanceRegistry().addInstance(instance))
    {
        statusLabel.setText("Add instance failed: id already exists", juce::dontSendNotification);
        return;
    }

    statusLabel.setText("Added instance '" + juce::String(id) + "' on channel " + juce::String(channel),
                         juce::dontSendNotification);
    instanceIdInput.clear();
    refreshInstanceList();
}

void ComposerMastermindAudioProcessorEditor::removeInstanceClicked()
{
    const auto id = instanceIdInput.getText().trim().toStdString();

    if (processorRef.getComposerCore().getInstanceRegistry().removeInstance(id))
        statusLabel.setText("Removed instance '" + juce::String(id) + "'", juce::dontSendNotification);
    else
        statusLabel.setText("Remove failed: no instance '" + juce::String(id) + "'", juce::dontSendNotification);

    refreshInstanceList();
}

Scene ComposerMastermindAudioProcessorEditor::buildSceneFromPanel(const std::string& id) const
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

    for (const auto& instance : processorRef.getComposerCore().getInstanceRegistry().getAllInstances())
        scene.targets.push_back(instance.id);

    const bool overrideGridMode = overrideGridModeCombo.getSelectedId() > 1;
    const bool overrideActivePattern = overrideActivePatternCombo.getSelectedId() > 1;

    if (overrideInstanceCombo.getSelectedId() > 0 && (overrideGridMode || overrideActivePattern))
    {
        SceneInstanceOverride instanceOverride;
        instanceOverride.targetInstance = overrideInstanceCombo.getText().toStdString();

        if (overrideGridMode)
            instanceOverride.gridMode = (overrideGridModeCombo.getSelectedId() == 2) ? 0 : 1;

        if (overrideActivePattern)
            instanceOverride.activePattern = overrideActivePatternCombo.getSelectedId() - 2;

        scene.instanceOverrides.push_back(instanceOverride);
    }

    return scene;
}

void ComposerMastermindAudioProcessorEditor::sendGlobalSceneClicked()
{
    if (processorRef.getComposerCore().getInstanceRegistry().getAllInstances().empty())
    {
        statusLabel.setText("Send scene skipped: no instances registered", juce::dontSendNotification);
        return;
    }

    const auto scene = buildSceneFromPanel("editor_test_scene");

    std::string errorMessage;
    if (!Validation::isValidScene(scene, errorMessage))
    {
        statusLabel.setText("Send scene failed: " + errorMessage, juce::dontSendNotification);
        return;
    }

    processorRef.getComposerCore().setCurrentScene(scene);
    processorRef.getComposerCore().fullRefresh();

    juce::String overrideNote;
    if (!scene.instanceOverrides.empty())
        overrideNote = " (with override on '" + juce::String(scene.instanceOverrides.front().targetInstance) + "')";

    juce::String chainNote;
    if (!scene.nextSceneId.empty())
        chainNote = ", chains to '" + juce::String(scene.nextSceneId) + "' after "
                        + juce::String(scene.durationBars) + " bar(s)";

    statusLabel.setText("Sent scene to " + juce::String((int) scene.targets.size()) + " instance(s)" + overrideNote
                             + chainNote,
                         juce::dontSendNotification);
}

void ComposerMastermindAudioProcessorEditor::sendMutationClicked()
{
    if (mutationTargetCombo.getSelectedId() <= 0)
    {
        statusLabel.setText("Send mutation skipped: no target instance", juce::dontSendNotification);
        return;
    }

    static const char* typeNames[] = { "transpose", "rotation", "length", "inversion" };
    const int typeIndex = juce::jlimit(0, 3, mutationTypeCombo.getSelectedItemIndex());

    Mutation mutation;
    mutation.type = typeNames[typeIndex];
    mutation.targetInstance = mutationTargetCombo.getText().toStdString();
    mutation.patternIndex = mutationPatternCombo.getSelectedId() - 1;
    mutation.amount = static_cast<int>(mutationAmountSlider.getValue());

    std::string errorMessage;
    if (!Validation::isValidMutation(mutation, errorMessage))
    {
        statusLabel.setText("Send mutation failed: " + errorMessage, juce::dontSendNotification);
        return;
    }

    processorRef.getComposerCore().getRouter().routeMutation(mutation);

    statusLabel.setText("Sent " + juce::String(mutation.type) + " mutation to '"
                             + juce::String(mutation.targetInstance) + "'",
                         juce::dontSendNotification);
}

void ComposerMastermindAudioProcessorEditor::refreshInstanceList()
{
    const auto instances = processorRef.getComposerCore().getInstanceRegistry().getAllInstances();

    juce::String text;
    for (const auto& instance : instances)
    {
        text << instance.id << "  ch=" << instance.midiChannel
             << "  " << (instance.enabled ? "enabled" : "disabled") << "\n";
    }

    if (text.isEmpty())
        text = "(no instances registered)";

    instanceListDisplay.setText(text, juce::dontSendNotification);

    auto repopulate = [&instances](juce::ComboBox& combo, bool autoSelectFirst)
    {
        const auto previousSelection = combo.getText();
        combo.clear(juce::dontSendNotification);

        int itemId = 1;
        int selectId = 0;
        for (const auto& instance : instances)
        {
            combo.addItem(instance.id, itemId);
            if (instance.id == previousSelection.toStdString())
                selectId = itemId;
            ++itemId;
        }

        if (selectId == 0 && autoSelectFirst && !instances.empty())
            selectId = 1;

        combo.setSelectedId(selectId, juce::dontSendNotification);
    };

    repopulate(mutationTargetCombo, true);
    repopulate(overrideInstanceCombo, false);
}

void ComposerMastermindAudioProcessorEditor::saveSceneClicked()
{
    const auto sceneId = sceneNameInput.getText().trim().toStdString();
    if (sceneId.empty())
    {
        statusLabel.setText("Save scene failed: enter a scene name/id first", juce::dontSendNotification);
        return;
    }

    const auto scene = buildSceneFromPanel(sceneId);

    std::string errorMessage;
    if (!Validation::isValidScene(scene, errorMessage))
    {
        statusLabel.setText("Save scene failed: " + errorMessage, juce::dontSendNotification);
        return;
    }

    processorRef.getComposerCore().getSceneLibrary().addOrReplaceScene(scene);
    statusLabel.setText("Saved scene '" + juce::String(sceneId) + "' to library", juce::dontSendNotification);
    refreshSceneList();
}

void ComposerMastermindAudioProcessorEditor::loadSceneClicked()
{
    if (savedScenesCombo.getSelectedId() <= 0)
    {
        statusLabel.setText("Load scene skipped: no saved scene selected", juce::dontSendNotification);
        return;
    }

    const auto sceneId = savedScenesCombo.getText().toStdString();

    Scene scene;
    if (!processorRef.getComposerCore().getSceneLibrary().getSceneById(sceneId, scene))
    {
        statusLabel.setText("Load scene failed: '" + juce::String(sceneId) + "' not found", juce::dontSendNotification);
        return;
    }

    processorRef.getComposerCore().setCurrentScene(scene);
    processorRef.getComposerCore().fullRefresh();
    statusLabel.setText("Loaded and sent scene '" + juce::String(sceneId) + "'", juce::dontSendNotification);
}

void ComposerMastermindAudioProcessorEditor::removeSceneClicked()
{
    if (savedScenesCombo.getSelectedId() <= 0)
    {
        statusLabel.setText("Remove scene skipped: no saved scene selected", juce::dontSendNotification);
        return;
    }

    const auto sceneId = savedScenesCombo.getText().toStdString();
    processorRef.getComposerCore().getSceneLibrary().removeScene(sceneId);
    statusLabel.setText("Removed scene '" + juce::String(sceneId) + "' from library", juce::dontSendNotification);
    refreshSceneList();
}

void ComposerMastermindAudioProcessorEditor::saveSnapshotToFileClicked()
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
            statusLabel.setText("Saved snapshot to " + file.getFullPathName(), juce::dontSendNotification);
        else
            statusLabel.setText("Failed to save snapshot to " + file.getFullPathName(), juce::dontSendNotification);
    });
}

void ComposerMastermindAudioProcessorEditor::loadSnapshotFromFileClicked()
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
            statusLabel.setText("Loaded snapshot from " + file.getFullPathName(), juce::dontSendNotification);
            refreshInstanceList();
            refreshSceneList();
        }
        else
        {
            statusLabel.setText("Failed to load snapshot: " + juce::String(errorMessage), juce::dontSendNotification);
        }
    });
}

void ComposerMastermindAudioProcessorEditor::refreshSceneList()
{
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

    auto repopulate = [&scenes](juce::ComboBox& combo, bool autoSelectFirst)
    {
        const auto previousSelection = combo.getText();
        combo.clear(juce::dontSendNotification);

        int itemId = 1;
        int selectId = 0;
        for (const auto& scene : scenes)
        {
            combo.addItem(scene.id, itemId);
            if (scene.id == previousSelection.toStdString())
                selectId = itemId;
            ++itemId;
        }

        if (selectId == 0 && autoSelectFirst && !scenes.empty())
            selectId = 1;

        combo.setSelectedId(selectId, juce::dontSendNotification);
    };

    repopulate(savedScenesCombo, false);
    repopulate(nextSceneCombo, false);
}
