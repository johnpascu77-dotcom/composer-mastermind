#include "PresetLibraryContent.h"
#include "../plugin/PluginProcessor.h"
#include "../util/Validation.h"
#include "../policy/PresetResolver.h"
#include "../policy/MotifEngine.h"
#include "../midi/CCMapping.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 4;
    constexpr int kMargin = 10;
    constexpr int kSmallGap = 5;

    // Sum of every row/gap laid out in resized(), kept in sync by hand -
    // see PresetLibraryContent::getPreferredHeight().
    constexpr int kPreferredHeight = 1744;

    void styleSlider(juce::Slider& slider, double minValue, double maxValue, double step, double initial)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, kRowHeight);
        slider.setRange(minValue, maxValue, step);
        slider.setValue(initial, juce::dontSendNotification);
    }

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

    const char* kRoleNames[] = { "anchor", "motif", "counterpoint" };
    const char* kArcDimensionNames[] = { "energy", "tension", "density", "complexity", "coherence" };
}

PresetLibraryContent::PresetLibraryContent(ComposerMastermindAudioProcessor& processor,
                                            std::function<void(const juce::String&)> status)
    : processorRef(processor), setStatus(std::move(status))
{
    addAndMakeVisible(builderHeaderLabel);
    builderHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(presetIdInput);
    presetIdInput.setTextToShowWhenEmpty("preset id/name, e.g. anchor_steady", juce::Colours::grey);

    addAndMakeVisible(targetRoleCombo);
    targetRoleCombo.addItem("Anchor", 1);
    targetRoleCombo.addItem("Motif", 2);
    targetRoleCombo.addItem("Counterpoint", 3);
    targetRoleCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(tagsInput);
    tagsInput.setTextToShowWhenEmpty("tags, comma-separated (optional)", juce::Colours::grey);

    addAndMakeVisible(gridModeCombo);
    gridModeCombo.addItem("No Override", 1);
    gridModeCombo.addItem("Binary", 2);
    gridModeCombo.addItem("Ternary", 3);
    gridModeCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(activePatternCombo);
    activePatternCombo.addItem("No Override", 1);
    activePatternCombo.addItem("Stop", 2);
    activePatternCombo.addItem("Pattern 1", 3);
    activePatternCombo.addItem("Pattern 2", 4);
    activePatternCombo.addItem("Pattern 3", 5);
    activePatternCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(overrideSwingToggle);

    addAndMakeVisible(swingCombo);
    swingCombo.addItem("Off", 1);
    swingCombo.addItem("Triplet", 2);
    swingCombo.addItem("Shuffle", 3);
    swingCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(savePresetButton);
    savePresetButton.onClick = [this] { savePresetClicked(); };

    addAndMakeVisible(libraryHeaderLabel);
    libraryHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(savedPresetsCombo);

    addAndMakeVisible(loadPresetButton);
    loadPresetButton.onClick = [this] { loadPresetClicked(); };

    addAndMakeVisible(removePresetButton);
    removePresetButton.onClick = [this] { removePresetClicked(); };

    addAndMakeVisible(presetsDisplay);
    presetsDisplay.setMultiLine(true);
    presetsDisplay.setReadOnly(true);
    presetsDisplay.setScrollbarsShown(true);
    presetsDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));

    addAndMakeVisible(applyHeaderLabel);
    applyHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(applyPresetCombo);
    applyPresetCombo.setTextWhenNothingSelected("(pick preset)");

    addAndMakeVisible(applySceneCombo);
    applySceneCombo.setTextWhenNothingSelected("(pick scene)");

    addAndMakeVisible(applyToSceneButton);
    applyToSceneButton.onClick = [this] { applyToSceneClicked(); };

    // -- Rhythmic-relationship preset builder --

    addAndMakeVisible(rhythmicHeaderLabel);
    rhythmicHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(rhythmicIdInput);
    rhythmicIdInput.setTextToShowWhenEmpty("preset id/name, e.g. polyrhythm_AB", juce::Colours::grey);

    addAndMakeVisible(rhythmicTagsInput);
    rhythmicTagsInput.setTextToShowWhenEmpty("tags, comma-separated (optional)", juce::Colours::grey);

    addAndMakeVisible(slotRoleCombo);
    slotRoleCombo.addItem("Anchor", 1);
    slotRoleCombo.addItem("Motif", 2);
    slotRoleCombo.addItem("Counterpoint", 3);
    slotRoleCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(slotGridModeCombo);
    slotGridModeCombo.addItem("No Override", 1);
    slotGridModeCombo.addItem("Binary", 2);
    slotGridModeCombo.addItem("Ternary", 3);
    slotGridModeCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(slotActivePatternCombo);
    slotActivePatternCombo.addItem("No Override", 1);
    slotActivePatternCombo.addItem("Stop", 2);
    slotActivePatternCombo.addItem("Pattern 1", 3);
    slotActivePatternCombo.addItem("Pattern 2", 4);
    slotActivePatternCombo.addItem("Pattern 3", 5);
    slotActivePatternCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(addRoleSlotButton);
    addRoleSlotButton.onClick = [this] { addRoleSlotClicked(); };

    addAndMakeVisible(slotOverrideSwingToggle);

    addAndMakeVisible(slotSwingCombo);
    slotSwingCombo.addItem("Off", 1);
    slotSwingCombo.addItem("Triplet", 2);
    slotSwingCombo.addItem("Shuffle", 3);
    slotSwingCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(pendingRoleSlotsLabel);
    pendingRoleSlotsLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    pendingRoleSlotsLabel.setMinimumHorizontalScale(1.0f);

    addAndMakeVisible(saveRhythmicPresetButton);
    saveRhythmicPresetButton.onClick = [this] { saveRhythmicPresetClicked(); };

    addAndMakeVisible(clearPendingSlotsButton);
    clearPendingSlotsButton.onClick = [this] { clearPendingRoleSlotsClicked(); };

    addAndMakeVisible(rhythmicLibraryHeaderLabel);
    rhythmicLibraryHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(savedRhythmicPresetsCombo);

    addAndMakeVisible(loadRhythmicPresetButton);
    loadRhythmicPresetButton.onClick = [this] { loadRhythmicPresetClicked(); };

    addAndMakeVisible(removeRhythmicPresetButton);
    removeRhythmicPresetButton.onClick = [this] { removeRhythmicPresetClicked(); };

    addAndMakeVisible(rhythmicPresetsDisplay);
    rhythmicPresetsDisplay.setMultiLine(true);
    rhythmicPresetsDisplay.setReadOnly(true);
    rhythmicPresetsDisplay.setScrollbarsShown(true);
    rhythmicPresetsDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));

    addAndMakeVisible(rhythmicApplyHeaderLabel);
    rhythmicApplyHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(applyRhythmicPresetCombo);
    applyRhythmicPresetCombo.setTextWhenNothingSelected("(pick preset)");

    addAndMakeVisible(applyRhythmicSceneCombo);
    applyRhythmicSceneCombo.setTextWhenNothingSelected("(pick scene)");

    addAndMakeVisible(applyRhythmicToSceneButton);
    applyRhythmicToSceneButton.onClick = [this] { applyRhythmicToSceneClicked(); };

    // -- Arc preset builder --

    addAndMakeVisible(arcHeaderLabel);
    arcHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(arcIdInput);
    arcIdInput.setTextToShowWhenEmpty("preset id/name, e.g. swell", juce::Colours::grey);

    addAndMakeVisible(arcTagsInput);
    arcTagsInput.setTextToShowWhenEmpty("tags, comma-separated (optional)", juce::Colours::grey);

    addAndMakeVisible(loadFromArcDimensionCombo);
    loadFromArcDimensionCombo.addItem("Energy", 1);
    loadFromArcDimensionCombo.addItem("Tension", 2);
    loadFromArcDimensionCombo.addItem("Density", 3);
    loadFromArcDimensionCombo.addItem("Complexity", 4);
    loadFromArcDimensionCombo.addItem("Coherence", 5);
    loadFromArcDimensionCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(loadFromArcStartBarSlider);
    styleSlider(loadFromArcStartBarSlider, 1.0, 64.0, 1.0, 1.0);

    addAndMakeVisible(loadFromArcEndBarSlider);
    styleSlider(loadFromArcEndBarSlider, 1.0, 64.0, 1.0, 16.0);

    addAndMakeVisible(loadFromArcButton);
    loadFromArcButton.onClick = [this] { loadFromLiveArcClicked(); };

    addAndMakeVisible(breakpointPositionSlider);
    styleSlider(breakpointPositionSlider, 0.0, 1.0, 0.05, 0.0);

    addAndMakeVisible(breakpointValueSlider);
    styleSlider(breakpointValueSlider, 0.0, 1.0, 0.05, 0.0);

    addAndMakeVisible(addBreakpointButton);
    addBreakpointButton.onClick = [this] { addArcBreakpointClicked(); };

    addAndMakeVisible(pendingArcBreakpointsLabel);
    pendingArcBreakpointsLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    pendingArcBreakpointsLabel.setMinimumHorizontalScale(1.0f);

    addAndMakeVisible(saveArcPresetButton);
    saveArcPresetButton.onClick = [this] { saveArcPresetClicked(); };

    addAndMakeVisible(clearPendingBreakpointsButton);
    clearPendingBreakpointsButton.onClick = [this] { clearPendingArcBreakpointsClicked(); };

    addAndMakeVisible(arcLibraryHeaderLabel);
    arcLibraryHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(savedArcPresetsCombo);

    addAndMakeVisible(loadArcPresetButton);
    loadArcPresetButton.onClick = [this] { loadArcPresetClicked(); };

    addAndMakeVisible(removeArcPresetButton);
    removeArcPresetButton.onClick = [this] { removeArcPresetClicked(); };

    addAndMakeVisible(arcPresetsDisplay);
    arcPresetsDisplay.setMultiLine(true);
    arcPresetsDisplay.setReadOnly(true);
    arcPresetsDisplay.setScrollbarsShown(true);
    arcPresetsDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));

    addAndMakeVisible(arcApplyHeaderLabel);
    arcApplyHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(applyArcPresetCombo);
    applyArcPresetCombo.setTextWhenNothingSelected("(pick preset)");

    addAndMakeVisible(applyTargetArcCombo);
    applyTargetArcCombo.addItem("Energy", 1);
    applyTargetArcCombo.addItem("Tension", 2);
    applyTargetArcCombo.addItem("Density", 3);
    applyTargetArcCombo.addItem("Complexity", 4);
    applyTargetArcCombo.addItem("Coherence", 5);
    applyTargetArcCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(applyStartBarSlider);
    styleSlider(applyStartBarSlider, 1.0, 64.0, 1.0, 1.0);

    addAndMakeVisible(applyEndBarSlider);
    styleSlider(applyEndBarSlider, 1.0, 64.0, 1.0, 16.0);

    addAndMakeVisible(applyArcPresetButton);
    applyArcPresetButton.onClick = [this] { applyArcPresetClicked(); };

    // -- Motif preset builder --

    addAndMakeVisible(motifHeaderLabel);
    motifHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(motifApplicationModeLabel);
    addAndMakeVisible(motifApplicationModeCombo);
    motifApplicationModeCombo.addItem("Nudge", 1);
    motifApplicationModeCombo.addItem("Phrase", 2);
    motifApplicationModeCombo.setSelectedId(
        processorRef.getComposerCore().getMotifApplicationMode() == MotifEngine::ApplicationMode::Phrase ? 2 : 1,
        juce::dontSendNotification);
    motifApplicationModeCombo.onChange = [this] { motifApplicationModeChanged(); };

    addAndMakeVisible(motifIdInput);
    motifIdInput.setTextToShowWhenEmpty("preset id/name, e.g. rising_third", juce::Colours::grey);

    addAndMakeVisible(motifTagsInput);
    motifTagsInput.setTextToShowWhenEmpty("tags, comma-separated (optional)", juce::Colours::grey);

    addAndMakeVisible(motifSemitoneOffsetLabel);
    motifSemitoneOffsetLabel.setFont(juce::Font(11.0f, juce::Font::plain));
    addAndMakeVisible(motifSemitoneOffsetSlider);
    styleSlider(motifSemitoneOffsetSlider, -24.0, 24.0, 1.0, 0.0);

    addAndMakeVisible(motifRelativeDurationLabel);
    motifRelativeDurationLabel.setFont(juce::Font(11.0f, juce::Font::plain));
    addAndMakeVisible(motifRelativeDurationSlider);
    styleSlider(motifRelativeDurationSlider, 0.25, 4.0, 0.25, 1.0);

    addAndMakeVisible(motifRelativeVelocityLabel);
    motifRelativeVelocityLabel.setFont(juce::Font(11.0f, juce::Font::plain));
    addAndMakeVisible(motifRelativeVelocitySlider);
    styleSlider(motifRelativeVelocitySlider, 0.25, 2.0, 0.05, 1.0);

    addAndMakeVisible(addMotifNoteButton);
    addMotifNoteButton.onClick = [this] { addMotifNoteClicked(); };

    addAndMakeVisible(addMotifRestButton);
    addMotifRestButton.onClick = [this] { addMotifRestClicked(); };

    addAndMakeVisible(pendingMotifNotesLabel);
    pendingMotifNotesLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    pendingMotifNotesLabel.setMinimumHorizontalScale(1.0f);

    addAndMakeVisible(saveMotifPresetButton);
    saveMotifPresetButton.onClick = [this] { saveMotifPresetClicked(); };

    addAndMakeVisible(clearPendingMotifNotesButton);
    clearPendingMotifNotesButton.onClick = [this] { clearPendingMotifNotesClicked(); };

    addAndMakeVisible(captureHeaderLabel);
    captureHeaderLabel.setFont(juce::Font(13.0f, juce::Font::bold));

    addAndMakeVisible(captureInstanceCombo);
    captureInstanceCombo.setTextWhenNothingSelected("(pick instance)");

    addAndMakeVisible(capturePatternCombo);
    capturePatternCombo.addItem("Pattern 1", 1);
    capturePatternCombo.addItem("Pattern 2", 2);
    capturePatternCombo.addItem("Pattern 3", 3);
    capturePatternCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(captureMotifPresetButton);
    captureMotifPresetButton.onClick = [this] { captureMotifPresetClicked(); };

    addAndMakeVisible(motifLibraryHeaderLabel);
    motifLibraryHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(savedMotifPresetsCombo);

    addAndMakeVisible(loadMotifPresetButton);
    loadMotifPresetButton.onClick = [this] { loadMotifPresetClicked(); };

    addAndMakeVisible(removeMotifPresetButton);
    removeMotifPresetButton.onClick = [this] { removeMotifPresetClicked(); };

    addAndMakeVisible(motifPresetsDisplay);
    motifPresetsDisplay.setMultiLine(true);
    motifPresetsDisplay.setReadOnly(true);
    motifPresetsDisplay.setScrollbarsShown(true);
    motifPresetsDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));

    refreshPresetsDisplay();
    refreshPendingRoleSlotsPreview();
    refreshRhythmicPresetsDisplay();
    refreshPendingArcBreakpointsPreview();
    refreshArcPresetsDisplay();
    refreshPendingMotifNotesPreview();
    refreshMotifPresetsDisplay();
    refreshAll();
}

void PresetLibraryContent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

int PresetLibraryContent::getPreferredHeight()
{
    return kPreferredHeight;
}

void PresetLibraryContent::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto nextRow = [&](int height) -> juce::Rectangle<int>
    {
        auto row = area.removeFromTop(height);
        area.removeFromTop(kRowGap);
        return row;
    };

    builderHeaderLabel.setBounds(nextRow(20));

    auto idRow = nextRow(kRowHeight);
    presetIdInput.setBounds(idRow.removeFromLeft(220));
    idRow.removeFromLeft(kMargin);
    targetRoleCombo.setBounds(idRow.removeFromLeft(150));

    tagsInput.setBounds(nextRow(kRowHeight));

    auto overrideRow = nextRow(kRowHeight);
    gridModeCombo.setBounds(overrideRow.removeFromLeft(150));
    overrideRow.removeFromLeft(kMargin);
    activePatternCombo.setBounds(overrideRow.removeFromLeft(150));

    auto swingRow = nextRow(kRowHeight);
    overrideSwingToggle.setBounds(swingRow.removeFromLeft(150));
    swingRow.removeFromLeft(kMargin);
    swingCombo.setBounds(swingRow.removeFromLeft(150));

    savePresetButton.setBounds(nextRow(kRowHeight).removeFromLeft(150));

    area.removeFromTop(kMargin - kRowGap);
    libraryHeaderLabel.setBounds(nextRow(20));

    auto libraryRow = nextRow(kRowHeight);
    savedPresetsCombo.setBounds(libraryRow.removeFromLeft(200));
    libraryRow.removeFromLeft(kMargin);
    loadPresetButton.setBounds(libraryRow.removeFromLeft(90));
    libraryRow.removeFromLeft(kMargin);
    removePresetButton.setBounds(libraryRow.removeFromLeft(90));

    presetsDisplay.setBounds(area.removeFromTop(70));
    area.removeFromTop(kRowGap);

    area.removeFromTop(kMargin - kRowGap);
    applyHeaderLabel.setBounds(nextRow(20));

    auto applyRow = nextRow(kRowHeight);
    applyPresetCombo.setBounds(applyRow.removeFromLeft(200));
    applyRow.removeFromLeft(kMargin);
    applySceneCombo.setBounds(applyRow.removeFromLeft(200));

    applyToSceneButton.setBounds(nextRow(kRowHeight).removeFromLeft(150));

    // -- Rhythmic-relationship section --

    area.removeFromTop(kMargin - kRowGap);
    rhythmicHeaderLabel.setBounds(nextRow(20));

    rhythmicIdInput.setBounds(nextRow(kRowHeight).removeFromLeft(220));
    rhythmicTagsInput.setBounds(nextRow(kRowHeight));

    auto slotRow = nextRow(kRowHeight);
    slotRoleCombo.setBounds(slotRow.removeFromLeft(110));
    slotRow.removeFromLeft(kSmallGap);
    slotGridModeCombo.setBounds(slotRow.removeFromLeft(110));
    slotRow.removeFromLeft(kSmallGap);
    slotActivePatternCombo.setBounds(slotRow.removeFromLeft(110));
    slotRow.removeFromLeft(kSmallGap);
    addRoleSlotButton.setBounds(slotRow.removeFromLeft(110));

    auto slotSwingRow = nextRow(kRowHeight);
    slotOverrideSwingToggle.setBounds(slotSwingRow.removeFromLeft(140));
    slotSwingRow.removeFromLeft(kMargin);
    slotSwingCombo.setBounds(slotSwingRow.removeFromLeft(150));

    pendingRoleSlotsLabel.setBounds(nextRow(18));

    auto saveRhythmicRow = nextRow(kRowHeight);
    saveRhythmicPresetButton.setBounds(saveRhythmicRow.removeFromLeft(150));
    saveRhythmicRow.removeFromLeft(kMargin);
    clearPendingSlotsButton.setBounds(saveRhythmicRow.removeFromLeft(120));

    area.removeFromTop(kMargin - kRowGap);
    rhythmicLibraryHeaderLabel.setBounds(nextRow(20));

    auto rhythmicLibraryRow = nextRow(kRowHeight);
    savedRhythmicPresetsCombo.setBounds(rhythmicLibraryRow.removeFromLeft(200));
    rhythmicLibraryRow.removeFromLeft(kMargin);
    loadRhythmicPresetButton.setBounds(rhythmicLibraryRow.removeFromLeft(90));
    rhythmicLibraryRow.removeFromLeft(kMargin);
    removeRhythmicPresetButton.setBounds(rhythmicLibraryRow.removeFromLeft(90));

    rhythmicPresetsDisplay.setBounds(area.removeFromTop(60));
    area.removeFromTop(kRowGap);

    area.removeFromTop(kMargin - kRowGap);
    rhythmicApplyHeaderLabel.setBounds(nextRow(20));

    auto rhythmicApplyRow = nextRow(kRowHeight);
    applyRhythmicPresetCombo.setBounds(rhythmicApplyRow.removeFromLeft(200));
    rhythmicApplyRow.removeFromLeft(kMargin);
    applyRhythmicSceneCombo.setBounds(rhythmicApplyRow.removeFromLeft(200));

    applyRhythmicToSceneButton.setBounds(nextRow(kRowHeight).removeFromLeft(150));

    // -- Arc preset section --

    area.removeFromTop(kMargin - kRowGap);
    arcHeaderLabel.setBounds(nextRow(20));

    arcIdInput.setBounds(nextRow(kRowHeight).removeFromLeft(220));
    arcTagsInput.setBounds(nextRow(kRowHeight));

    auto loadFromArcRow = nextRow(kRowHeight);
    loadFromArcDimensionCombo.setBounds(loadFromArcRow.removeFromLeft(150));
    loadFromArcRow.removeFromLeft(kMargin);
    loadFromArcButton.setBounds(loadFromArcRow.removeFromLeft(150));

    auto loadFromArcBarsRow = nextRow(kRowHeight);
    loadFromArcStartBarSlider.setBounds(loadFromArcBarsRow.removeFromLeft(220));
    loadFromArcBarsRow.removeFromLeft(kMargin);
    loadFromArcEndBarSlider.setBounds(loadFromArcBarsRow.removeFromLeft(220));

    auto breakpointRow = nextRow(kRowHeight);
    breakpointPositionSlider.setBounds(breakpointRow.removeFromLeft(220));
    breakpointRow.removeFromLeft(kMargin);
    breakpointValueSlider.setBounds(breakpointRow.removeFromLeft(220));

    addBreakpointButton.setBounds(nextRow(kRowHeight).removeFromLeft(150));

    pendingArcBreakpointsLabel.setBounds(nextRow(18));

    auto saveArcRow = nextRow(kRowHeight);
    saveArcPresetButton.setBounds(saveArcRow.removeFromLeft(150));
    saveArcRow.removeFromLeft(kMargin);
    clearPendingBreakpointsButton.setBounds(saveArcRow.removeFromLeft(120));

    area.removeFromTop(kMargin - kRowGap);
    arcLibraryHeaderLabel.setBounds(nextRow(20));

    auto arcLibraryRow = nextRow(kRowHeight);
    savedArcPresetsCombo.setBounds(arcLibraryRow.removeFromLeft(200));
    arcLibraryRow.removeFromLeft(kMargin);
    loadArcPresetButton.setBounds(arcLibraryRow.removeFromLeft(90));
    arcLibraryRow.removeFromLeft(kMargin);
    removeArcPresetButton.setBounds(arcLibraryRow.removeFromLeft(90));

    arcPresetsDisplay.setBounds(area.removeFromTop(60));
    area.removeFromTop(kRowGap);

    area.removeFromTop(kMargin - kRowGap);
    arcApplyHeaderLabel.setBounds(nextRow(20));

    auto arcApplyRow = nextRow(kRowHeight);
    applyArcPresetCombo.setBounds(arcApplyRow.removeFromLeft(200));
    arcApplyRow.removeFromLeft(kMargin);
    applyTargetArcCombo.setBounds(arcApplyRow.removeFromLeft(150));

    auto arcApplyBarsRow = nextRow(kRowHeight);
    applyStartBarSlider.setBounds(arcApplyBarsRow.removeFromLeft(220));
    arcApplyBarsRow.removeFromLeft(kMargin);
    applyEndBarSlider.setBounds(arcApplyBarsRow.removeFromLeft(220));

    applyArcPresetButton.setBounds(nextRow(kRowHeight).removeFromLeft(150));

    // -- Motif preset section --

    area.removeFromTop(kMargin - kRowGap);
    motifHeaderLabel.setBounds(nextRow(20));

    auto motifModeRow = nextRow(kRowHeight);
    motifApplicationModeLabel.setBounds(motifModeRow.removeFromLeft(160));
    motifApplicationModeCombo.setBounds(motifModeRow.removeFromLeft(120));

    motifIdInput.setBounds(nextRow(kRowHeight).removeFromLeft(220));
    motifTagsInput.setBounds(nextRow(kRowHeight));

    auto motifNoteLabelRow = nextRow(16);
    motifSemitoneOffsetLabel.setBounds(motifNoteLabelRow.removeFromLeft(150));
    motifNoteLabelRow.removeFromLeft(kMargin);
    motifRelativeDurationLabel.setBounds(motifNoteLabelRow.removeFromLeft(150));
    motifNoteLabelRow.removeFromLeft(kMargin);
    motifRelativeVelocityLabel.setBounds(motifNoteLabelRow.removeFromLeft(150));

    auto motifNoteRow = nextRow(kRowHeight);
    motifSemitoneOffsetSlider.setBounds(motifNoteRow.removeFromLeft(150));
    motifNoteRow.removeFromLeft(kMargin);
    motifRelativeDurationSlider.setBounds(motifNoteRow.removeFromLeft(150));
    motifNoteRow.removeFromLeft(kMargin);
    motifRelativeVelocitySlider.setBounds(motifNoteRow.removeFromLeft(150));

    auto addMotifRow = nextRow(kRowHeight);
    addMotifNoteButton.setBounds(addMotifRow.removeFromLeft(150));
    addMotifRow.removeFromLeft(kMargin);
    addMotifRestButton.setBounds(addMotifRow.removeFromLeft(150));

    pendingMotifNotesLabel.setBounds(nextRow(18));

    auto saveMotifRow = nextRow(kRowHeight);
    saveMotifPresetButton.setBounds(saveMotifRow.removeFromLeft(150));
    saveMotifRow.removeFromLeft(kMargin);
    clearPendingMotifNotesButton.setBounds(saveMotifRow.removeFromLeft(120));

    area.removeFromTop(kMargin - kRowGap);
    captureHeaderLabel.setBounds(nextRow(18));

    auto captureRow = nextRow(kRowHeight);
    captureInstanceCombo.setBounds(captureRow.removeFromLeft(150));
    captureRow.removeFromLeft(kMargin);
    capturePatternCombo.setBounds(captureRow.removeFromLeft(110));
    captureRow.removeFromLeft(kMargin);
    captureMotifPresetButton.setBounds(captureRow.removeFromLeft(140));

    area.removeFromTop(kMargin - kRowGap);
    motifLibraryHeaderLabel.setBounds(nextRow(20));

    auto motifLibraryRow = nextRow(kRowHeight);
    savedMotifPresetsCombo.setBounds(motifLibraryRow.removeFromLeft(200));
    motifLibraryRow.removeFromLeft(kMargin);
    loadMotifPresetButton.setBounds(motifLibraryRow.removeFromLeft(90));
    motifLibraryRow.removeFromLeft(kMargin);
    removeMotifPresetButton.setBounds(motifLibraryRow.removeFromLeft(90));

    motifPresetsDisplay.setBounds(area.removeFromTop(60));
}

RolePreset PresetLibraryContent::buildPresetFromPanel(const std::string& id) const
{
    RolePreset preset;
    preset.id = id;
    preset.name = id;
    preset.targetRole = kRoleNames[juce::jlimit(0, 2, targetRoleCombo.getSelectedItemIndex())];

    for (const auto& tag : juce::StringArray::fromTokens(tagsInput.getText(), ",", ""))
    {
        const auto trimmed = tag.trim();
        if (trimmed.isNotEmpty())
            preset.tags.push_back(trimmed.toStdString());
    }

    if (gridModeCombo.getSelectedId() > 1)
        preset.gridMode = gridModeCombo.getSelectedId() - 2;

    if (activePatternCombo.getSelectedId() > 1)
        preset.activePattern = activePatternCombo.getSelectedId() - 2;

    if (overrideSwingToggle.getToggleState())
        preset.swing = CCMapping::swingPercentForState(swingCombo.getSelectedId() - 1);

    return preset;
}

void PresetLibraryContent::loadPresetIntoPanel(const RolePreset& preset)
{
    presetIdInput.setText(preset.id, juce::dontSendNotification);

    int roleId = 1;
    for (int i = 0; i < 3; ++i)
    {
        if (preset.targetRole == kRoleNames[i])
        {
            roleId = i + 1;
            break;
        }
    }
    targetRoleCombo.setSelectedId(roleId, juce::dontSendNotification);

    juce::StringArray tagArray;
    for (const auto& tag : preset.tags)
        tagArray.add(tag);
    tagsInput.setText(tagArray.joinIntoString(", "), juce::dontSendNotification);

    gridModeCombo.setSelectedId(preset.gridMode >= 0 ? preset.gridMode + 2 : 1, juce::dontSendNotification);
    activePatternCombo.setSelectedId(preset.activePattern >= 0 ? preset.activePattern + 2 : 1, juce::dontSendNotification);

    overrideSwingToggle.setToggleState(preset.swing >= 0.0f, juce::dontSendNotification);
    swingCombo.setSelectedId(CCMapping::swingStateForPercent(preset.swing >= 0.0f ? preset.swing : 0.0f) + 1,
                              juce::dontSendNotification);
}

void PresetLibraryContent::savePresetClicked()
{
    const auto id = presetIdInput.getText().trim().toStdString();
    if (id.empty())
    {
        setStatus("Save preset failed: enter a preset id/name first");
        return;
    }

    const auto preset = buildPresetFromPanel(id);

    std::string errorMessage;
    if (!Validation::isValidRolePreset(preset, errorMessage))
    {
        setStatus("Save preset failed: " + errorMessage);
        return;
    }

    processorRef.getComposerCore().getPresetLibrary().addOrReplaceRolePreset(preset);
    setStatus("Saved preset '" + juce::String(id) + "' (role: " + juce::String(preset.targetRole) + ")");
    refreshAll();
}

void PresetLibraryContent::loadPresetClicked()
{
    if (savedPresetsCombo.getSelectedId() <= 0)
    {
        setStatus("Load preset skipped: no saved preset selected");
        return;
    }

    const auto presetId = savedPresetsCombo.getText().toStdString();

    RolePreset preset;
    if (!processorRef.getComposerCore().getPresetLibrary().getRolePresetById(presetId, preset))
    {
        setStatus("Load preset failed: '" + juce::String(presetId) + "' not found");
        return;
    }

    loadPresetIntoPanel(preset);
    setStatus("Loaded preset '" + juce::String(presetId) + "'");
}

void PresetLibraryContent::removePresetClicked()
{
    if (savedPresetsCombo.getSelectedId() <= 0)
    {
        setStatus("Remove preset skipped: no saved preset selected");
        return;
    }

    const auto presetId = savedPresetsCombo.getText().toStdString();
    processorRef.getComposerCore().getPresetLibrary().removeRolePreset(presetId);
    setStatus("Removed preset '" + juce::String(presetId) + "' from library");
    refreshAll();
}

void PresetLibraryContent::applyToSceneClicked()
{
    if (applyPresetCombo.getSelectedId() <= 0)
    {
        setStatus("Apply preset skipped: no preset selected");
        return;
    }

    if (applySceneCombo.getSelectedId() <= 0)
    {
        setStatus("Apply preset skipped: no scene selected");
        return;
    }

    const auto presetId = applyPresetCombo.getText().toStdString();
    const auto sceneId = applySceneCombo.getText().toStdString();

    auto& composerCore = processorRef.getComposerCore();

    RolePreset preset;
    if (!composerCore.getPresetLibrary().getRolePresetById(presetId, preset))
    {
        setStatus("Apply preset failed: preset '" + juce::String(presetId) + "' not found");
        return;
    }

    Scene scene;
    if (!composerCore.getSceneLibrary().getSceneById(sceneId, scene))
    {
        setStatus("Apply preset failed: scene '" + juce::String(sceneId) + "' not found");
        return;
    }

    const auto instances = composerCore.getInstanceRegistry().getAllInstances();
    const int affectedCount = PresetResolver::applyRolePreset(preset, instances, scene);

    if (affectedCount == 0)
    {
        setStatus("Apply preset '" + juce::String(presetId) + "' to '" + juce::String(sceneId)
                      + "' affected 0 instances (none registered with role '" + juce::String(preset.targetRole) + "')");
        return;
    }

    composerCore.getSceneLibrary().addOrReplaceScene(scene);
    setStatus("Applied preset '" + juce::String(presetId) + "' to scene '" + juce::String(sceneId) + "' ("
                  + juce::String(affectedCount) + " instance(s) with role '" + juce::String(preset.targetRole) + "')");
    refreshAll();
}

void PresetLibraryContent::refreshPresetsDisplay()
{
    juce::String text;
    for (const auto& preset : processorRef.getComposerCore().getPresetLibrary().getAllRolePresets())
    {
        text << preset.id << "  role=" << preset.targetRole;

        if (preset.activePattern >= 0)
            text << "  activePattern=" << preset.activePattern;
        if (preset.gridMode >= 0)
            text << "  gridMode=" << preset.gridMode;
        if (preset.swing >= 0.0f)
            text << "  swing=" << preset.swing;

        if (!preset.tags.empty())
        {
            text << "  tags=";
            for (size_t i = 0; i < preset.tags.size(); ++i)
            {
                if (i > 0)
                    text << ",";
                text << preset.tags[i];
            }
        }

        text << "\n";
    }

    if (text.isEmpty())
        text = "(no saved presets)";

    presetsDisplay.setText(text, juce::dontSendNotification);
}

void PresetLibraryContent::addRoleSlotClicked()
{
    RhythmicRelationshipRoleSlot slot;
    slot.targetRole = kRoleNames[juce::jlimit(0, 2, slotRoleCombo.getSelectedItemIndex())];

    if (slotGridModeCombo.getSelectedId() > 1)
        slot.gridMode = slotGridModeCombo.getSelectedId() - 2;

    if (slotActivePatternCombo.getSelectedId() > 1)
        slot.activePattern = slotActivePatternCombo.getSelectedId() - 2;

    if (slotOverrideSwingToggle.getToggleState())
        slot.swing = CCMapping::swingPercentForState(slotSwingCombo.getSelectedId() - 1);

    for (auto& existing : pendingRoleSlots)
    {
        if (existing.targetRole == slot.targetRole)
        {
            existing = slot;
            refreshPendingRoleSlotsPreview();
            return;
        }
    }

    pendingRoleSlots.push_back(slot);
    refreshPendingRoleSlotsPreview();
}

void PresetLibraryContent::clearPendingRoleSlotsClicked()
{
    pendingRoleSlots.clear();
    refreshPendingRoleSlotsPreview();
    setStatus("Cleared pending role slots");
}

void PresetLibraryContent::saveRhythmicPresetClicked()
{
    const auto id = rhythmicIdInput.getText().trim().toStdString();
    if (id.empty())
    {
        setStatus("Save preset failed: enter a preset id/name first");
        return;
    }

    RhythmicRelationshipPreset preset;
    preset.id = id;
    preset.name = id;
    preset.roleSlots = pendingRoleSlots;

    for (const auto& tag : juce::StringArray::fromTokens(rhythmicTagsInput.getText(), ",", ""))
    {
        const auto trimmed = tag.trim();
        if (trimmed.isNotEmpty())
            preset.tags.push_back(trimmed.toStdString());
    }

    std::string errorMessage;
    if (!Validation::isValidRhythmicRelationshipPreset(preset, errorMessage))
    {
        setStatus("Save preset failed: " + errorMessage);
        return;
    }

    processorRef.getComposerCore().getPresetLibrary().addOrReplaceRhythmicRelationshipPreset(preset);
    pendingRoleSlots.clear();
    rhythmicIdInput.clear();
    refreshPendingRoleSlotsPreview();
    setStatus("Saved preset '" + juce::String(id) + "' (" + juce::String((int) preset.roleSlots.size()) + " role slot(s))");
    refreshAll();
}

void PresetLibraryContent::loadRhythmicPresetClicked()
{
    if (savedRhythmicPresetsCombo.getSelectedId() <= 0)
    {
        setStatus("Load preset skipped: no saved preset selected");
        return;
    }

    const auto presetId = savedRhythmicPresetsCombo.getText().toStdString();

    RhythmicRelationshipPreset preset;
    if (!processorRef.getComposerCore().getPresetLibrary().getRhythmicRelationshipPresetById(presetId, preset))
    {
        setStatus("Load preset failed: '" + juce::String(presetId) + "' not found");
        return;
    }

    rhythmicIdInput.setText(preset.id, juce::dontSendNotification);

    juce::StringArray tagArray;
    for (const auto& tag : preset.tags)
        tagArray.add(tag);
    rhythmicTagsInput.setText(tagArray.joinIntoString(", "), juce::dontSendNotification);

    pendingRoleSlots = preset.roleSlots;
    refreshPendingRoleSlotsPreview();
    setStatus("Loaded preset '" + juce::String(presetId) + "' (" + juce::String((int) preset.roleSlots.size())
                  + " role slot(s)) - edit slots above and Save Preset to update it");
}

void PresetLibraryContent::removeRhythmicPresetClicked()
{
    if (savedRhythmicPresetsCombo.getSelectedId() <= 0)
    {
        setStatus("Remove preset skipped: no saved preset selected");
        return;
    }

    const auto presetId = savedRhythmicPresetsCombo.getText().toStdString();
    processorRef.getComposerCore().getPresetLibrary().removeRhythmicRelationshipPreset(presetId);
    setStatus("Removed preset '" + juce::String(presetId) + "' from library");
    refreshAll();
}

void PresetLibraryContent::applyRhythmicToSceneClicked()
{
    if (applyRhythmicPresetCombo.getSelectedId() <= 0)
    {
        setStatus("Apply preset skipped: no preset selected");
        return;
    }

    if (applyRhythmicSceneCombo.getSelectedId() <= 0)
    {
        setStatus("Apply preset skipped: no scene selected");
        return;
    }

    const auto presetId = applyRhythmicPresetCombo.getText().toStdString();
    const auto sceneId = applyRhythmicSceneCombo.getText().toStdString();

    auto& composerCore = processorRef.getComposerCore();

    RhythmicRelationshipPreset preset;
    if (!composerCore.getPresetLibrary().getRhythmicRelationshipPresetById(presetId, preset))
    {
        setStatus("Apply preset failed: preset '" + juce::String(presetId) + "' not found");
        return;
    }

    Scene scene;
    if (!composerCore.getSceneLibrary().getSceneById(sceneId, scene))
    {
        setStatus("Apply preset failed: scene '" + juce::String(sceneId) + "' not found");
        return;
    }

    const auto instances = composerCore.getInstanceRegistry().getAllInstances();
    const int affectedCount = PresetResolver::applyRhythmicRelationshipPreset(preset, instances, scene);

    if (affectedCount == 0)
    {
        setStatus("Apply preset '" + juce::String(presetId) + "' to '" + juce::String(sceneId)
                      + "' affected 0 instances (none registered with any of this preset's roles)");
        return;
    }

    composerCore.getSceneLibrary().addOrReplaceScene(scene);
    setStatus("Applied preset '" + juce::String(presetId) + "' to scene '" + juce::String(sceneId) + "' ("
                  + juce::String(affectedCount) + " instance(s) across " + juce::String((int) preset.roleSlots.size())
                  + " role slot(s))");
    refreshAll();
}

void PresetLibraryContent::refreshPendingRoleSlotsPreview()
{
    juce::String text = "Pending role slots: ";
    if (pendingRoleSlots.empty())
    {
        text << "(none)";
    }
    else
    {
        for (size_t i = 0; i < pendingRoleSlots.size(); ++i)
        {
            if (i > 0)
                text << ", ";
            const auto& slot = pendingRoleSlots[i];
            text << slot.targetRole << "(";
            bool any = false;
            if (slot.activePattern >= 0) { text << "pattern=" << slot.activePattern; any = true; }
            if (slot.gridMode >= 0) { text << (any ? "," : "") << "grid=" << slot.gridMode; any = true; }
            if (slot.swing >= 0.0f) { text << (any ? "," : "") << "swing=" << slot.swing; any = true; }
            if (!any) text << "no override";
            text << ")";
        }
    }
    pendingRoleSlotsLabel.setText(text, juce::dontSendNotification);
}

void PresetLibraryContent::refreshRhythmicPresetsDisplay()
{
    juce::String text;
    for (const auto& preset : processorRef.getComposerCore().getPresetLibrary().getAllRhythmicRelationshipPresets())
    {
        text << preset.id << "  roleSlots=" << (int) preset.roleSlots.size() << " (";
        for (size_t i = 0; i < preset.roleSlots.size(); ++i)
        {
            if (i > 0)
                text << ",";
            text << preset.roleSlots[i].targetRole;
        }
        text << ")";

        if (!preset.tags.empty())
        {
            text << "  tags=";
            for (size_t i = 0; i < preset.tags.size(); ++i)
            {
                if (i > 0)
                    text << ",";
                text << preset.tags[i];
            }
        }

        text << "\n";
    }

    if (text.isEmpty())
        text = "(no saved presets)";

    rhythmicPresetsDisplay.setText(text, juce::dontSendNotification);
}

void PresetLibraryContent::loadFromLiveArcClicked()
{
    const auto dimensionName = kArcDimensionNames[juce::jlimit(0, 4, loadFromArcDimensionCombo.getSelectedItemIndex())];
    const int startBar = static_cast<int>(loadFromArcStartBarSlider.getValue());
    const int endBar = static_cast<int>(loadFromArcEndBarSlider.getValue());

    if (endBar <= startBar)
    {
        setStatus("Load from live arc failed: end bar must be after start bar");
        return;
    }

    const auto arc = processorRef.getComposerCore().getArcSet().getArc(dimensionName);

    pendingArcBreakpoints.clear();
    for (const auto& point : arc.getBreakpoints())
    {
        if (point.bar < startBar || point.bar > endBar)
            continue;

        ArcPresetBreakpoint breakpoint;
        breakpoint.position = static_cast<float>(point.bar - startBar) / static_cast<float>(endBar - startBar);
        breakpoint.value = point.value;
        pendingArcBreakpoints.push_back(breakpoint);
    }

    std::sort(pendingArcBreakpoints.begin(), pendingArcBreakpoints.end(),
               [](const ArcPresetBreakpoint& a, const ArcPresetBreakpoint& b) { return a.position < b.position; });
    refreshPendingArcBreakpointsPreview();

    if (pendingArcBreakpoints.empty())
        setStatus("Loaded 0 breakpoints from live '" + juce::String(dimensionName) + "' arc in bars "
                       + juce::String(startBar) + "-" + juce::String(endBar) + " - nothing authored in that range yet");
    else
        setStatus("Loaded " + juce::String((int) pendingArcBreakpoints.size()) + " breakpoint(s) from live '"
                       + juce::String(dimensionName) + "' arc - review below, set id/tags, then Save Preset");
}

void PresetLibraryContent::addArcBreakpointClicked()
{
    ArcPresetBreakpoint breakpoint;
    breakpoint.position = static_cast<float>(breakpointPositionSlider.getValue());
    breakpoint.value = static_cast<float>(breakpointValueSlider.getValue());

    for (auto& existing : pendingArcBreakpoints)
    {
        if (std::abs(existing.position - breakpoint.position) < 0.001f)
        {
            existing = breakpoint;
            refreshPendingArcBreakpointsPreview();
            return;
        }
    }

    pendingArcBreakpoints.push_back(breakpoint);
    std::sort(pendingArcBreakpoints.begin(), pendingArcBreakpoints.end(),
               [](const ArcPresetBreakpoint& a, const ArcPresetBreakpoint& b) { return a.position < b.position; });
    refreshPendingArcBreakpointsPreview();
}

void PresetLibraryContent::clearPendingArcBreakpointsClicked()
{
    pendingArcBreakpoints.clear();
    refreshPendingArcBreakpointsPreview();
    setStatus("Cleared pending breakpoints");
}

void PresetLibraryContent::saveArcPresetClicked()
{
    const auto id = arcIdInput.getText().trim().toStdString();
    if (id.empty())
    {
        setStatus("Save preset failed: enter a preset id/name first");
        return;
    }

    ArcPreset preset;
    preset.id = id;
    preset.name = id;
    preset.breakpoints = pendingArcBreakpoints;

    for (const auto& tag : juce::StringArray::fromTokens(arcTagsInput.getText(), ",", ""))
    {
        const auto trimmed = tag.trim();
        if (trimmed.isNotEmpty())
            preset.tags.push_back(trimmed.toStdString());
    }

    std::string errorMessage;
    if (!Validation::isValidArcPreset(preset, errorMessage))
    {
        setStatus("Save preset failed: " + errorMessage);
        return;
    }

    processorRef.getComposerCore().getPresetLibrary().addOrReplaceArcPreset(preset);
    pendingArcBreakpoints.clear();
    arcIdInput.clear();
    refreshPendingArcBreakpointsPreview();
    setStatus("Saved preset '" + juce::String(id) + "' (" + juce::String((int) preset.breakpoints.size()) + " breakpoint(s))");
    refreshAll();
}

void PresetLibraryContent::loadArcPresetClicked()
{
    if (savedArcPresetsCombo.getSelectedId() <= 0)
    {
        setStatus("Load preset skipped: no saved preset selected");
        return;
    }

    const auto presetId = savedArcPresetsCombo.getText().toStdString();

    ArcPreset preset;
    if (!processorRef.getComposerCore().getPresetLibrary().getArcPresetById(presetId, preset))
    {
        setStatus("Load preset failed: '" + juce::String(presetId) + "' not found");
        return;
    }

    arcIdInput.setText(preset.id, juce::dontSendNotification);

    juce::StringArray tagArray;
    for (const auto& tag : preset.tags)
        tagArray.add(tag);
    arcTagsInput.setText(tagArray.joinIntoString(", "), juce::dontSendNotification);

    pendingArcBreakpoints = preset.breakpoints;
    refreshPendingArcBreakpointsPreview();
    setStatus("Loaded preset '" + juce::String(presetId) + "' (" + juce::String((int) preset.breakpoints.size())
                  + " breakpoint(s)) - edit above and Save Preset to update it");
}

void PresetLibraryContent::removeArcPresetClicked()
{
    if (savedArcPresetsCombo.getSelectedId() <= 0)
    {
        setStatus("Remove preset skipped: no saved preset selected");
        return;
    }

    const auto presetId = savedArcPresetsCombo.getText().toStdString();
    processorRef.getComposerCore().getPresetLibrary().removeArcPreset(presetId);
    setStatus("Removed preset '" + juce::String(presetId) + "' from library");
    refreshAll();
}

void PresetLibraryContent::applyArcPresetClicked()
{
    if (applyArcPresetCombo.getSelectedId() <= 0)
    {
        setStatus("Apply preset skipped: no preset selected");
        return;
    }

    const auto presetId = applyArcPresetCombo.getText().toStdString();
    const auto targetArcName = kArcDimensionNames[juce::jlimit(0, 4, applyTargetArcCombo.getSelectedItemIndex())];
    const int startBar = static_cast<int>(applyStartBarSlider.getValue());
    const int endBar = static_cast<int>(applyEndBarSlider.getValue());

    auto& composerCore = processorRef.getComposerCore();

    ArcPreset preset;
    if (!composerCore.getPresetLibrary().getArcPresetById(presetId, preset))
    {
        setStatus("Apply preset failed: preset '" + juce::String(presetId) + "' not found");
        return;
    }

    const int stampedCount = PresetResolver::applyArcPreset(preset, targetArcName, startBar, endBar,
                                                              composerCore.getArcSet());

    if (stampedCount == 0)
    {
        setStatus("Apply preset '" + juce::String(presetId) + "' failed: check the preset has breakpoints and "
                      "Start Bar < End Bar");
        return;
    }

    setStatus("Applied preset '" + juce::String(presetId) + "' to '" + juce::String(targetArcName) + "' bars "
                  + juce::String(startBar) + "-" + juce::String(endBar) + " (" + juce::String(stampedCount)
                  + " breakpoint(s)) - see the Arcs tab");
}

void PresetLibraryContent::refreshPendingArcBreakpointsPreview()
{
    juce::String text = "Pending breakpoints: ";
    if (pendingArcBreakpoints.empty())
    {
        text << "(none)";
    }
    else
    {
        for (size_t i = 0; i < pendingArcBreakpoints.size(); ++i)
        {
            if (i > 0)
                text << ", ";
            text << "(" << pendingArcBreakpoints[i].position << "," << pendingArcBreakpoints[i].value << ")";
        }
    }
    pendingArcBreakpointsLabel.setText(text, juce::dontSendNotification);
}

void PresetLibraryContent::refreshArcPresetsDisplay()
{
    juce::String text;
    for (const auto& preset : processorRef.getComposerCore().getPresetLibrary().getAllArcPresets())
    {
        text << preset.id << "  breakpoints=" << (int) preset.breakpoints.size();

        if (!preset.tags.empty())
        {
            text << "  tags=";
            for (size_t i = 0; i < preset.tags.size(); ++i)
            {
                if (i > 0)
                    text << ",";
                text << preset.tags[i];
            }
        }

        text << "\n";
    }

    if (text.isEmpty())
        text = "(no saved presets)";

    arcPresetsDisplay.setText(text, juce::dontSendNotification);
}

void PresetLibraryContent::motifApplicationModeChanged()
{
    const auto mode = motifApplicationModeCombo.getSelectedId() == 2
                           ? MotifEngine::ApplicationMode::Phrase
                           : MotifEngine::ApplicationMode::Nudge;

    processorRef.getComposerCore().setMotifApplicationMode(mode);
    setStatus(juce::String("Motif application mode set to ")
                   + (mode == MotifEngine::ApplicationMode::Phrase ? "Phrase" : "Nudge"));
}

void PresetLibraryContent::addMotifNoteClicked()
{
    MotifNote note;
    note.semitoneOffset = static_cast<int>(motifSemitoneOffsetSlider.getValue());
    note.relativeDuration = static_cast<float>(motifRelativeDurationSlider.getValue());
    note.relativeVelocity = static_cast<float>(motifRelativeVelocitySlider.getValue());

    // Appended, not sorted/deduped like arc breakpoints - a motif cell is a
    // melodic sequence, order is the point.
    pendingMotifNotes.push_back(note);
    refreshPendingMotifNotesPreview();
}

void PresetLibraryContent::addMotifRestClicked()
{
    MotifNote note;
    note.isRest = true;
    note.relativeDuration = static_cast<float>(motifRelativeDurationSlider.getValue());

    pendingMotifNotes.push_back(note);
    refreshPendingMotifNotesPreview();
}

void PresetLibraryContent::clearPendingMotifNotesClicked()
{
    pendingMotifNotes.clear();
    refreshPendingMotifNotesPreview();
    setStatus("Cleared pending motif notes");
}

void PresetLibraryContent::saveMotifPresetClicked()
{
    const auto id = motifIdInput.getText().trim().toStdString();
    if (id.empty())
    {
        setStatus("Save preset failed: enter a preset id/name first");
        return;
    }

    MotifPreset preset;
    preset.id = id;
    preset.name = id;
    preset.notes = pendingMotifNotes;

    for (const auto& tag : juce::StringArray::fromTokens(motifTagsInput.getText(), ",", ""))
    {
        const auto trimmed = tag.trim();
        if (trimmed.isNotEmpty())
            preset.tags.push_back(trimmed.toStdString());
    }

    std::string errorMessage;
    if (!Validation::isValidMotifPreset(preset, errorMessage))
    {
        setStatus("Save preset failed: " + errorMessage);
        return;
    }

    processorRef.getComposerCore().getPresetLibrary().addOrReplaceMotifPreset(preset);
    pendingMotifNotes.clear();
    motifIdInput.clear();
    refreshPendingMotifNotesPreview();
    setStatus("Saved preset '" + juce::String(id) + "' (" + juce::String((int) preset.notes.size()) + " note(s))");
    refreshAll();
}

void PresetLibraryContent::loadMotifPresetClicked()
{
    if (savedMotifPresetsCombo.getSelectedId() <= 0)
    {
        setStatus("Load preset skipped: no saved preset selected");
        return;
    }

    const auto presetId = savedMotifPresetsCombo.getText().toStdString();

    MotifPreset preset;
    if (!processorRef.getComposerCore().getPresetLibrary().getMotifPresetById(presetId, preset))
    {
        setStatus("Load preset failed: '" + juce::String(presetId) + "' not found");
        return;
    }

    motifIdInput.setText(preset.id, juce::dontSendNotification);

    juce::StringArray tagArray;
    for (const auto& tag : preset.tags)
        tagArray.add(tag);
    motifTagsInput.setText(tagArray.joinIntoString(", "), juce::dontSendNotification);

    pendingMotifNotes = preset.notes;
    refreshPendingMotifNotesPreview();
    setStatus("Loaded preset '" + juce::String(presetId) + "' (" + juce::String((int) preset.notes.size())
                  + " note(s)) - edit above and Save Preset to update it");
}

void PresetLibraryContent::removeMotifPresetClicked()
{
    if (savedMotifPresetsCombo.getSelectedId() <= 0)
    {
        setStatus("Remove preset skipped: no saved preset selected");
        return;
    }

    const auto presetId = savedMotifPresetsCombo.getText().toStdString();
    processorRef.getComposerCore().getPresetLibrary().removeMotifPreset(presetId);
    setStatus("Removed preset '" + juce::String(presetId) + "' from library");
    refreshAll();
}

void PresetLibraryContent::captureMotifPresetClicked()
{
    if (captureInstanceCombo.getSelectedId() <= 0)
    {
        setStatus("Capture skipped: pick an instance first");
        return;
    }

    const auto instanceId = captureInstanceCombo.getText().toStdString();
    const int patternIndex = capturePatternCombo.getSelectedId() - 1;

    CachedPattern cached;
    if (!processorRef.getComposerCore().getPatternSyncServer().getCache().get(instanceId, patternIndex, cached))
    {
        setStatus("Capture failed: no confirmed content for '" + juce::String(instanceId) + "' P"
                      + juce::String(patternIndex + 1) + " yet - resync it first (Awareness tab)");
        return;
    }

    const auto derived = MotifEngine::deriveMotifPresetFromPattern("", {}, cached.snapshot.steps);
    if (derived.notes.empty())
    {
        setStatus("Capture failed: '" + juce::String(instanceId) + "' P" + juce::String(patternIndex + 1)
                      + " has no enabled steps - nothing to capture");
        return;
    }

    pendingMotifNotes = derived.notes;
    refreshPendingMotifNotesPreview();
    setStatus("Captured '" + juce::String(instanceId) + "' P" + juce::String(patternIndex + 1) + " ("
                  + juce::String((int) derived.notes.size()) + " entries) - name it above and Save Preset");
}

void PresetLibraryContent::refreshPendingMotifNotesPreview()
{
    juce::String text = "Pending notes: ";
    if (pendingMotifNotes.empty())
    {
        text << "(none)";
    }
    else
    {
        for (size_t i = 0; i < pendingMotifNotes.size(); ++i)
        {
            if (i > 0)
                text << ", ";

            if (pendingMotifNotes[i].isRest)
                text << "REST/x" << pendingMotifNotes[i].relativeDuration << "dur";
            else
                text << pendingMotifNotes[i].semitoneOffset << "st/x" << pendingMotifNotes[i].relativeDuration
                     << "dur/x" << pendingMotifNotes[i].relativeVelocity << "vel";
        }
    }
    pendingMotifNotesLabel.setText(text, juce::dontSendNotification);
}

void PresetLibraryContent::refreshMotifPresetsDisplay()
{
    juce::String text;
    for (const auto& preset : processorRef.getComposerCore().getPresetLibrary().getAllMotifPresets())
    {
        text << preset.id << "  notes=" << (int) preset.notes.size();

        if (!preset.tags.empty())
        {
            text << "  tags=";
            for (size_t i = 0; i < preset.tags.size(); ++i)
            {
                if (i > 0)
                    text << ",";
                text << preset.tags[i];
            }
        }

        text << "\n";
    }

    if (text.isEmpty())
        text = "(no saved presets)";

    motifPresetsDisplay.setText(text, juce::dontSendNotification);
}

void PresetLibraryContent::refreshAll()
{
    std::vector<std::string> presetIds;
    for (const auto& preset : processorRef.getComposerCore().getPresetLibrary().getAllRolePresets())
        presetIds.push_back(preset.id);
    repopulate(savedPresetsCombo, presetIds, false);
    repopulate(applyPresetCombo, presetIds, false);

    std::vector<std::string> sceneIds;
    for (const auto& scene : processorRef.getComposerCore().getSceneLibrary().getAllScenes())
        sceneIds.push_back(scene.id);
    repopulate(applySceneCombo, sceneIds, false);
    repopulate(applyRhythmicSceneCombo, sceneIds, false);

    std::vector<std::string> rhythmicPresetIds;
    for (const auto& preset : processorRef.getComposerCore().getPresetLibrary().getAllRhythmicRelationshipPresets())
        rhythmicPresetIds.push_back(preset.id);
    repopulate(savedRhythmicPresetsCombo, rhythmicPresetIds, false);
    repopulate(applyRhythmicPresetCombo, rhythmicPresetIds, false);

    std::vector<std::string> arcPresetIds;
    for (const auto& preset : processorRef.getComposerCore().getPresetLibrary().getAllArcPresets())
        arcPresetIds.push_back(preset.id);
    repopulate(savedArcPresetsCombo, arcPresetIds, false);
    repopulate(applyArcPresetCombo, arcPresetIds, false);

    std::vector<std::string> motifPresetIds;
    for (const auto& preset : processorRef.getComposerCore().getPresetLibrary().getAllMotifPresets())
        motifPresetIds.push_back(preset.id);
    repopulate(savedMotifPresetsCombo, motifPresetIds, false);

    refreshCaptureInstanceCombo();

    refreshPresetsDisplay();
    refreshRhythmicPresetsDisplay();
    refreshArcPresetsDisplay();
    refreshMotifPresetsDisplay();
}

void PresetLibraryContent::refreshCaptureInstanceCombo()
{
    std::vector<std::string> instanceIds;
    for (const auto& instance : processorRef.getComposerCore().getInstanceRegistry().getAllInstances())
        instanceIds.push_back(instance.id);
    repopulate(captureInstanceCombo, instanceIds, false);
}
