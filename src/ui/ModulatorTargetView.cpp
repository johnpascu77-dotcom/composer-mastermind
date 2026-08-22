#include "ModulatorTargetView.h"
#include "../plugin/PluginProcessor.h"
#include "../model/ModulatorTarget.h"
#include "../util/Validation.h"

namespace
{
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 4;
    constexpr int kMargin = 10;

    // Value sent by "Send for Pairing" - arbitrary. Bitwig's Learn CC only
    // needs to see a CC *number* arrive to complete a pairing; the value it
    // carries is irrelevant to establishing the binding.
    constexpr int kPairingTestValue = 100;

    void styleSlider(juce::Slider& slider, double minValue, double maxValue, double step, double initial)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, kRowHeight);
        slider.setRange(minValue, maxValue, step);
        slider.setValue(initial, juce::dontSendNotification);
    }
}

ModulatorTargetView::ModulatorTargetView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> status)
    : processorRef(processor), setStatus(std::move(status))
{
    addAndMakeVisible(targetIdInput);
    targetIdInput.setTextToShowWhenEmpty("target id, e.g. Random1_Rate", juce::Colours::grey);

    addAndMakeVisible(ccNumberSlider);
    styleSlider(ccNumberSlider, 0.0, 127.0, 1.0, 70.0);
    addAndMakeVisible(ccNumberLabel);

    addAndMakeVisible(channelSlider);
    styleSlider(channelSlider, 1.0, 16.0, 1.0, 1.0);
    addAndMakeVisible(channelLabel);

    addAndMakeVisible(modeCombo);
    modeCombo.addItem("Arc (continuous)", 1);
    modeCombo.addItem("Section (stepped)", 2);
    modeCombo.setSelectedId(1, juce::dontSendNotification);
    modeCombo.onChange = [this] { modeChanged(); };
    addAndMakeVisible(modeLabel);

    addAndMakeVisible(arcDimensionCombo);
    addAndMakeVisible(arcDimensionLabel);

    addAndMakeVisible(addTargetButton);
    addTargetButton.onClick = [this] { addTargetClicked(); };

    addAndMakeVisible(existingTargetsCombo);
    existingTargetsCombo.setTextWhenNothingSelected("(pick existing target)");

    addAndMakeVisible(removeTargetButton);
    removeTargetButton.onClick = [this] { removeTargetClicked(); };

    addAndMakeVisible(sendForPairingButton);
    sendForPairingButton.onClick = [this] { sendForPairingClicked(); };

    addAndMakeVisible(targetListDisplay);
    targetListDisplay.setMultiLine(true);
    targetListDisplay.setReadOnly(true);
    targetListDisplay.setScrollbarsShown(true);
    targetListDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    modeChanged();
    refreshAll();
}

void ModulatorTargetView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void ModulatorTargetView::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto nextRow = [&](int height) -> juce::Rectangle<int>
    {
        auto row = area.removeFromTop(height);
        area.removeFromTop(kRowGap);
        return row;
    };

    auto idRow = nextRow(kRowHeight);
    targetIdInput.setBounds(idRow.removeFromLeft(220));

    auto numbersRow = nextRow(kRowHeight);
    ccNumberLabel.setBounds(numbersRow.removeFromLeft(30));
    ccNumberSlider.setBounds(numbersRow.removeFromLeft(150));
    numbersRow.removeFromLeft(kMargin);
    channelLabel.setBounds(numbersRow.removeFromLeft(60));
    channelSlider.setBounds(numbersRow.removeFromLeft(110));

    auto modeRow = nextRow(kRowHeight);
    modeLabel.setBounds(modeRow.removeFromLeft(50));
    modeCombo.setBounds(modeRow.removeFromLeft(160));
    modeRow.removeFromLeft(kMargin);
    arcDimensionLabel.setBounds(modeRow.removeFromLeft(90));
    arcDimensionCombo.setBounds(modeRow.removeFromLeft(140));

    auto addRow = nextRow(kRowHeight);
    addTargetButton.setBounds(addRow.removeFromLeft(110));

    auto existingRow = nextRow(kRowHeight);
    existingTargetsCombo.setBounds(existingRow.removeFromLeft(200));
    existingRow.removeFromLeft(kMargin);
    removeTargetButton.setBounds(existingRow.removeFromLeft(90));
    existingRow.removeFromLeft(kMargin);
    sendForPairingButton.setBounds(existingRow.removeFromLeft(140));

    area.removeFromTop(kRowGap);
    targetListDisplay.setBounds(area);
}

void ModulatorTargetView::modeChanged()
{
    const bool isArcMode = modeCombo.getSelectedId() == 1;
    arcDimensionCombo.setEnabled(isArcMode);
    arcDimensionLabel.setEnabled(isArcMode);
}

void ModulatorTargetView::addTargetClicked()
{
    const auto id = targetIdInput.getText().trim().toStdString();

    ModulatorTarget target;
    target.id = id;
    target.ccNumber = static_cast<int>(ccNumberSlider.getValue());
    target.midiChannel = static_cast<int>(channelSlider.getValue());
    target.mode = modeCombo.getSelectedId() == 1 ? "arc" : "section";
    target.arcDimension = target.mode == "arc" ? arcDimensionCombo.getText().toStdString() : std::string();

    std::string errorMessage;
    if (!Validation::isValidModulatorTarget(target, errorMessage))
    {
        setStatus("Add modulator target failed: " + errorMessage);
        return;
    }

    processorRef.getComposerCore().getModulatorTargetLibrary().addOrReplaceTarget(target);

    setStatus("Set modulator target '" + juce::String(id) + "' -> CC " + juce::String(target.ccNumber)
                   + " ch " + juce::String(target.midiChannel) + " (" + juce::String(target.mode) + ")");
    targetIdInput.clear();
    refreshAll();
}

void ModulatorTargetView::removeTargetClicked()
{
    if (existingTargetsCombo.getSelectedId() <= 0)
    {
        setStatus("Remove modulator target skipped: no target selected");
        return;
    }

    const auto id = existingTargetsCombo.getText().toStdString();

    if (processorRef.getComposerCore().getModulatorTargetLibrary().removeTarget(id))
        setStatus("Removed modulator target '" + juce::String(id) + "'");
    else
        setStatus("Remove failed: no modulator target '" + juce::String(id) + "'");

    refreshAll();
}

void ModulatorTargetView::sendForPairingClicked()
{
    if (existingTargetsCombo.getSelectedId() <= 0)
    {
        setStatus("Send for pairing skipped: no target selected");
        return;
    }

    ModulatorTarget target;
    const auto id = existingTargetsCombo.getText().toStdString();
    if (!processorRef.getComposerCore().getModulatorTargetLibrary().getTargetById(id, target))
    {
        setStatus("Send for pairing failed: unknown target '" + juce::String(id) + "'");
        return;
    }

    processorRef.getComposerCore().getCCDispatcher().sendCC(target.midiChannel, target.ccNumber, kPairingTestValue);

    setStatus("Sent CC " + juce::String(target.ccNumber) + " (ch " + juce::String(target.midiChannel)
                   + ") for '" + juce::String(id) + "' - use Bitwig's Learn CC now");
}

void ModulatorTargetView::refreshAll()
{
    const auto previousArcSelection = arcDimensionCombo.getText();
    arcDimensionCombo.clear(juce::dontSendNotification);
    int arcItemId = 1;
    int arcSelectId = 0;
    for (const auto& name : processorRef.getComposerCore().getArcSet().getArcNames())
    {
        arcDimensionCombo.addItem(name, arcItemId);
        if (name == previousArcSelection.toStdString())
            arcSelectId = arcItemId;
        ++arcItemId;
    }
    if (arcSelectId == 0)
        arcSelectId = 1;
    arcDimensionCombo.setSelectedId(arcSelectId, juce::dontSendNotification);

    const auto targets = processorRef.getComposerCore().getModulatorTargetLibrary().getAllTargets();

    const auto previousTargetSelection = existingTargetsCombo.getText();
    existingTargetsCombo.clear(juce::dontSendNotification);
    int targetItemId = 1;
    int targetSelectId = 0;
    for (const auto& target : targets)
    {
        existingTargetsCombo.addItem(target.id, targetItemId);
        if (target.id == previousTargetSelection.toStdString())
            targetSelectId = targetItemId;
        ++targetItemId;
    }
    existingTargetsCombo.setSelectedId(targetSelectId, juce::dontSendNotification);

    juce::String text;
    for (const auto& target : targets)
    {
        text << target.id << "  CC" << target.ccNumber << "  ch=" << target.midiChannel
             << "  mode=" << target.mode;
        if (target.mode == "arc")
            text << "  arc=" << (target.arcDimension.empty() ? "(none)" : target.arcDimension);
        text << "\n";
    }

    if (targets.empty())
        text = "(no modulator targets registered)\n"
               "Pair each target's CC with a Bitwig modulator's own Learn CC "
               "(via a virtual MIDI cable such as loopMIDI - Bitwig's general "
               "'Map to Controller or Key' ignores plugin-generated CC).";

    targetListDisplay.setText(text, juce::dontSendNotification);
}
