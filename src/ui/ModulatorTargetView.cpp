#include "ModulatorTargetView.h"
#include "../plugin/PluginProcessor.h"
#include "../model/ModulatorTarget.h"
#include "../model/ModulationRoute.h"
#include "../util/Validation.h"
#include <array>

namespace
{
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 4;
    constexpr int kMargin = 10;
    constexpr int kListHeight = 130;

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

    struct ParameterOption { ModulationParameter parameter; const char* label; };

    // Combo item id N (1-based) maps to kParameterOptions[N - 1].
    constexpr std::array<ParameterOption, 10> kParameterOptions { {
        { ModulationParameter::Transpose,     "Transpose" },
        { ModulationParameter::Rotation,      "Rotation" },
        { ModulationParameter::Length,        "Length" },
        { ModulationParameter::Swing,         "Swing" },
        { ModulationParameter::Rate,          "Rate" },
        { ModulationParameter::Inversion,     "Inversion" },
        { ModulationParameter::Retrograde,    "Retrograde" },
        { ModulationParameter::M7,            "M7" },
        { ModulationParameter::ActivePattern, "Active Pattern" },
        { ModulationParameter::GridMode,      "Grid Mode" },
    } };

    ModulationParameter parameterForComboId(int selectedId)
    {
        const int index = selectedId - 1;
        if (index >= 0 && index < static_cast<int>(kParameterOptions.size()))
            return kParameterOptions[static_cast<size_t>(index)].parameter;
        return ModulationParameter::Transpose;
    }
}

ModulatorTargetView::ModulatorTargetView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> status)
    : processorRef(processor), setStatus(std::move(status))
{
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);

    // --- Panel 0: pitch-field broadcast ---
    auto& core = processorRef.getComposerCore();

    content.addAndMakeVisible(pitchFieldHeader);
    pitchFieldHeader.setFont(juce::Font(15.0f, juce::Font::bold));

    content.addAndMakeVisible(pitchFieldEnableToggle);
    pitchFieldEnableToggle.setToggleState(core.isPitchFieldBroadcastEnabled(), juce::dontSendNotification);
    pitchFieldEnableToggle.onClick = [this] { pushPitchFieldSettings(); };

    content.addAndMakeVisible(pitchFieldBaseCcSlider);
    styleSlider(pitchFieldBaseCcSlider, 0.0, 118.0, 1.0, core.getPitchFieldBroadcastBaseCc());
    pitchFieldBaseCcSlider.onValueChange = [this] { pushPitchFieldSettings(); };
    content.addAndMakeVisible(pitchFieldBaseCcLabel);

    content.addAndMakeVisible(pitchFieldChannelSlider);
    styleSlider(pitchFieldChannelSlider, 1.0, 16.0, 1.0, core.getPitchFieldBroadcastChannel());
    pitchFieldChannelSlider.onValueChange = [this] { pushPitchFieldSettings(); };
    content.addAndMakeVisible(pitchFieldChannelLabel);

    content.addAndMakeVisible(pitchFieldMinBarsSlider);
    styleSlider(pitchFieldMinBarsSlider, 0.0, 32.0, 1.0, core.getPitchFieldBroadcastMinBars());
    pitchFieldMinBarsSlider.onValueChange = [this] { pushPitchFieldSettings(); };
    content.addAndMakeVisible(pitchFieldMinBarsLabel);

    content.addAndMakeVisible(pitchFieldResyncButton);
    pitchFieldResyncButton.onClick = [this]
    {
        pitchFieldEnableToggle.setToggleState(true, juce::dontSendNotification);
        processorRef.getComposerCore().setPitchFieldBroadcastEnabled(true); // clears the last mask -> re-broadcasts next bar
        updatePitchFieldStatus();
        setStatus("Pitch field broadcast: re-broadcasting on the next bar");
    };

    content.addAndMakeVisible(pitchFieldStatusLabel);
    pitchFieldStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    updatePitchFieldStatus();

    startTimerHz(4);

    // --- Panel 1: external Modulator Targets ---
    content.addAndMakeVisible(externalTargetsHeader);
    externalTargetsHeader.setFont(juce::Font(15.0f, juce::Font::bold));

    content.addAndMakeVisible(targetIdInput);
    targetIdInput.setTextToShowWhenEmpty("target id, e.g. Random1_Rate", juce::Colours::grey);

    content.addAndMakeVisible(ccNumberSlider);
    styleSlider(ccNumberSlider, 0.0, 127.0, 1.0, 70.0);
    content.addAndMakeVisible(ccNumberLabel);

    content.addAndMakeVisible(channelSlider);
    styleSlider(channelSlider, 1.0, 16.0, 1.0, 1.0);
    content.addAndMakeVisible(channelLabel);

    content.addAndMakeVisible(modeCombo);
    modeCombo.addItem("Arc (continuous)", 1);
    modeCombo.addItem("Section (stepped)", 2);
    modeCombo.setSelectedId(1, juce::dontSendNotification);
    modeCombo.onChange = [this] { modeChanged(); };
    content.addAndMakeVisible(modeLabel);

    content.addAndMakeVisible(arcDimensionCombo);
    content.addAndMakeVisible(arcDimensionLabel);

    content.addAndMakeVisible(addTargetButton);
    addTargetButton.onClick = [this] { addTargetClicked(); };

    content.addAndMakeVisible(existingTargetsCombo);
    existingTargetsCombo.setTextWhenNothingSelected("(pick existing target)");

    content.addAndMakeVisible(removeTargetButton);
    removeTargetButton.onClick = [this] { removeTargetClicked(); };

    content.addAndMakeVisible(sendForPairingButton);
    sendForPairingButton.onClick = [this] { sendForPairingClicked(); };

    content.addAndMakeVisible(targetListDisplay);
    targetListDisplay.setMultiLine(true);
    targetListDisplay.setReadOnly(true);
    targetListDisplay.setScrollbarsShown(true);
    targetListDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    // --- Panel 2: Instance Modulation Routes ---
    content.addAndMakeVisible(routesHeader);
    routesHeader.setFont(juce::Font(15.0f, juce::Font::bold));

    content.addAndMakeVisible(routeIdInput);
    routeIdInput.setTextToShowWhenEmpty("route id, e.g. Pad_TensionToTranspose", juce::Colours::grey);

    content.addAndMakeVisible(routeArcDimensionCombo);
    content.addAndMakeVisible(routeArcDimensionLabel);

    content.addAndMakeVisible(routeTargetInstanceCombo);
    content.addAndMakeVisible(routeTargetInstanceLabel);

    content.addAndMakeVisible(routePatternIndexCombo);
    routePatternIndexCombo.addItem("Pattern 1", 1);
    routePatternIndexCombo.addItem("Pattern 2", 2);
    routePatternIndexCombo.addItem("Pattern 3", 3);
    routePatternIndexCombo.setSelectedId(1, juce::dontSendNotification);
    content.addAndMakeVisible(routePatternIndexLabel);

    content.addAndMakeVisible(routeParameterCombo);
    for (size_t i = 0; i < kParameterOptions.size(); ++i)
        routeParameterCombo.addItem(kParameterOptions[i].label, static_cast<int>(i) + 1);
    routeParameterCombo.setSelectedId(1, juce::dontSendNotification);
    routeParameterCombo.onChange = [this] { routeParameterChanged(); };
    content.addAndMakeVisible(routeParameterLabel);

    content.addAndMakeVisible(routeDispatchModeCombo);
    routeDispatchModeCombo.addItem("Bar (continuous)", 1);
    routeDispatchModeCombo.addItem("Sequence (loop-cycle)", 2);
    routeDispatchModeCombo.addItem("Bar Cycle (phrase cadence)", 3);
    routeDispatchModeCombo.setSelectedId(1, juce::dontSendNotification);
    routeDispatchModeCombo.onChange = [this] { routeParameterChanged(); };
    content.addAndMakeVisible(routeDispatchModeLabel);

    content.addAndMakeVisible(routeSequenceValuesInput);
    routeSequenceValuesInput.setTextToShowWhenEmpty("e.g. 0, -2, -4, -2", juce::Colours::grey);
    content.addAndMakeVisible(routeSequenceValuesLabel);

    content.addAndMakeVisible(routePhraseLengthBarsSlider);
    styleSlider(routePhraseLengthBarsSlider, 1.0, 32.0, 1.0, 4.0);
    content.addAndMakeVisible(routePhraseLengthBarsLabel);

    content.addAndMakeVisible(routeOutputMinSlider);
    styleSlider(routeOutputMinSlider, -48.0, 48.0, 1.0, 0.0);
    content.addAndMakeVisible(routeOutputMinLabel);

    content.addAndMakeVisible(routeOutputMaxSlider);
    styleSlider(routeOutputMaxSlider, -48.0, 48.0, 1.0, 0.0);
    content.addAndMakeVisible(routeOutputMaxLabel);

    content.addAndMakeVisible(routeThresholdSlider);
    styleSlider(routeThresholdSlider, 0.0, 1.0, 0.01, 0.5);
    content.addAndMakeVisible(routeThresholdLabel);

    content.addAndMakeVisible(routeInvertToggle);
    content.addAndMakeVisible(routeEnabledToggle);
    routeEnabledToggle.setToggleState(true, juce::dontSendNotification);

    content.addAndMakeVisible(addRouteButton);
    addRouteButton.onClick = [this] { addRouteClicked(); };

    content.addAndMakeVisible(existingRoutesCombo);
    existingRoutesCombo.setTextWhenNothingSelected("(pick existing route)");

    content.addAndMakeVisible(removeRouteButton);
    removeRouteButton.onClick = [this] { removeRouteClicked(); };

    content.addAndMakeVisible(routeListDisplay);
    routeListDisplay.setMultiLine(true);
    routeListDisplay.setReadOnly(true);
    routeListDisplay.setScrollbarsShown(true);
    routeListDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    modeChanged();
    routeParameterChanged();
    refreshAll();
}

ModulatorTargetView::~ModulatorTargetView()
{
    stopTimer();
}

void ModulatorTargetView::timerCallback()
{
    updatePitchFieldStatus();
}

void ModulatorTargetView::pushPitchFieldSettings()
{
    auto& core = processorRef.getComposerCore();
    core.setPitchFieldBroadcastBaseCc(juce::roundToInt(pitchFieldBaseCcSlider.getValue()));
    core.setPitchFieldBroadcastChannel(juce::roundToInt(pitchFieldChannelSlider.getValue()));
    core.setPitchFieldBroadcastMinBars(juce::roundToInt(pitchFieldMinBarsSlider.getValue()));
    core.setPitchFieldBroadcastEnabled(pitchFieldEnableToggle.getToggleState());
    updatePitchFieldStatus();
}

void ModulatorTargetView::updatePitchFieldStatus()
{
    static const char* pcNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    auto& core = processorRef.getComposerCore();
    const int mask = core.getLastBroadcastPitchFieldMask();

    juce::String text;
    if (! core.isPitchFieldBroadcastEnabled())
    {
        text = "off";
    }
    else if (mask < 0)
    {
        text = "on - waiting for confirmed pattern content";
    }
    else
    {
        juce::StringArray pcs;
        for (int pc = 0; pc < 12; ++pc)
            if (mask & (1 << pc))
                pcs.add(pcNames[pc]);
        text = "on - last: " + (pcs.isEmpty() ? juce::String("(none)") : pcs.joinIntoString(" "))
             + "  (" + juce::String(pcs.size()) + " pc)";
    }

    pitchFieldStatusLabel.setText("Broadcast: " + text, juce::dontSendNotification);
}

void ModulatorTargetView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void ModulatorTargetView::resized()
{
    viewport.setBounds(getLocalBounds());

    const int contentWidth = juce::jmax(420, getWidth() - 20); // room for the viewport's own scrollbar
    int y = kMargin;

    auto nextRow = [&](int height) -> juce::Rectangle<int>
    {
        juce::Rectangle<int> row(kMargin, y, contentWidth - kMargin * 2, height);
        y += height + kRowGap;
        return row;
    };

    pitchFieldHeader.setBounds(nextRow(20));

    pitchFieldEnableToggle.setBounds(nextRow(kRowHeight).removeFromLeft(420));

    auto pfNumbersRow = nextRow(kRowHeight);
    pitchFieldBaseCcLabel.setBounds(pfNumbersRow.removeFromLeft(60));
    pitchFieldBaseCcSlider.setBounds(pfNumbersRow.removeFromLeft(140));
    pfNumbersRow.removeFromLeft(kMargin);
    pitchFieldChannelLabel.setBounds(pfNumbersRow.removeFromLeft(55));
    pitchFieldChannelSlider.setBounds(pfNumbersRow.removeFromLeft(100));
    pfNumbersRow.removeFromLeft(kMargin);
    pitchFieldMinBarsLabel.setBounds(pfNumbersRow.removeFromLeft(60));
    pitchFieldMinBarsSlider.setBounds(pfNumbersRow.removeFromLeft(100));

    auto pfActionRow = nextRow(kRowHeight);
    pitchFieldResyncButton.setBounds(pfActionRow.removeFromLeft(140));
    pfActionRow.removeFromLeft(kMargin);
    pitchFieldStatusLabel.setBounds(pfActionRow);

    y += 12; // divider gap

    externalTargetsHeader.setBounds(nextRow(20));

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

    targetListDisplay.setBounds(nextRow(kListHeight));

    y += 12; // divider gap between the two panels

    routesHeader.setBounds(nextRow(20));

    auto routeIdRow = nextRow(kRowHeight);
    routeIdInput.setBounds(routeIdRow.removeFromLeft(260));

    auto routeArcInstanceRow = nextRow(kRowHeight);
    routeArcDimensionLabel.setBounds(routeArcInstanceRow.removeFromLeft(90));
    routeArcDimensionCombo.setBounds(routeArcInstanceRow.removeFromLeft(140));
    routeArcInstanceRow.removeFromLeft(kMargin);
    routeTargetInstanceLabel.setBounds(routeArcInstanceRow.removeFromLeft(60));
    routeTargetInstanceCombo.setBounds(routeArcInstanceRow.removeFromLeft(170));

    auto routePatternParamRow = nextRow(kRowHeight);
    routePatternIndexLabel.setBounds(routePatternParamRow.removeFromLeft(60));
    routePatternIndexCombo.setBounds(routePatternParamRow.removeFromLeft(100));
    routePatternParamRow.removeFromLeft(kMargin);
    routeParameterLabel.setBounds(routePatternParamRow.removeFromLeft(70));
    routeParameterCombo.setBounds(routePatternParamRow.removeFromLeft(150));

    auto routeDispatchRow = nextRow(kRowHeight);
    routeDispatchModeLabel.setBounds(routeDispatchRow.removeFromLeft(60));
    routeDispatchModeCombo.setBounds(routeDispatchRow.removeFromLeft(160));

    auto routeRangeRow = nextRow(kRowHeight);
    const auto routeRangeRowFull = routeRangeRow; // preserved for Threshold/Sequence, which lay out differently
    routeOutputMinLabel.setBounds(routeRangeRow.removeFromLeft(30));
    routeOutputMinSlider.setBounds(routeRangeRow.removeFromLeft(150));
    routeRangeRow.removeFromLeft(kMargin);
    routeOutputMaxLabel.setBounds(routeRangeRow.removeFromLeft(30));
    routeOutputMaxSlider.setBounds(routeRangeRow.removeFromLeft(150));
    // Threshold and the Sequence-values field share the same row region -
    // only one of the three (range/threshold/sequence) is ever visible at
    // once (see routeParameterChanged), so overlap is fine.
    routeThresholdLabel.setBounds(routeOutputMinLabel.getBounds());
    routeThresholdSlider.setBounds(routeOutputMinSlider.getBounds().getUnion(routeOutputMaxSlider.getBounds()));

    auto sequenceRow = routeRangeRowFull;
    routeSequenceValuesLabel.setBounds(sequenceRow.removeFromLeft(160));
    sequenceRow.removeFromLeft(kMargin);
    routeSequenceValuesInput.setBounds(sequenceRow);

    // Own row, not sharing routeRangeRowFull like the three above - BarCycle
    // mode needs sequenceValues AND phraseLengthBars visible at once, unlike
    // range/threshold/sequence which are always mutually exclusive.
    auto routePhraseLengthRow = nextRow(kRowHeight);
    routePhraseLengthBarsLabel.setBounds(routePhraseLengthRow.removeFromLeft(90));
    routePhraseLengthBarsSlider.setBounds(routePhraseLengthRow.removeFromLeft(150));

    auto routeTogglesRow = nextRow(kRowHeight);
    routeInvertToggle.setBounds(routeTogglesRow.removeFromLeft(80));
    routeTogglesRow.removeFromLeft(kMargin);
    routeEnabledToggle.setBounds(routeTogglesRow.removeFromLeft(90));

    auto routeAddRow = nextRow(kRowHeight);
    addRouteButton.setBounds(routeAddRow.removeFromLeft(110));

    auto routeExistingRow = nextRow(kRowHeight);
    existingRoutesCombo.setBounds(routeExistingRow.removeFromLeft(200));
    routeExistingRow.removeFromLeft(kMargin);
    removeRouteButton.setBounds(routeExistingRow.removeFromLeft(90));

    routeListDisplay.setBounds(nextRow(kListHeight));

    content.setSize(contentWidth, y + kMargin);
}

void ModulatorTargetView::modeChanged()
{
    const bool isArcMode = modeCombo.getSelectedId() == 1;
    arcDimensionCombo.setEnabled(isArcMode);
    arcDimensionLabel.setEnabled(isArcMode);
}

void ModulatorTargetView::routeParameterChanged()
{
    const ModulationParameter parameter = parameterForComboId(routeParameterCombo.getSelectedId());

    const bool isGlobal = isGlobalModulationParameter(parameter);
    routePatternIndexCombo.setEnabled(!isGlobal);
    routePatternIndexLabel.setEnabled(!isGlobal);

    const bool isContinuous = isContinuousModulationParameter(parameter);
    const bool isActivePattern = parameter == ModulationParameter::ActivePattern;

    // Sequence/BarCycle dispatch only applies to continuous parameters -
    // disable/reset the combo for the 5 threshold-crossing ones rather than
    // let the UI offer a combination Validation would reject.
    routeDispatchModeCombo.setEnabled(isContinuous);
    routeDispatchModeLabel.setEnabled(isContinuous);
    if (!isContinuous)
        routeDispatchModeCombo.setSelectedId(1, juce::dontSendNotification); // back to Bar

    const bool isSequenceDispatch = isContinuous && routeDispatchModeCombo.getSelectedId() == 2;
    const bool isBarCycleDispatch = isContinuous && routeDispatchModeCombo.getSelectedId() == 3;
    const bool usesSequenceValues = isSequenceDispatch || isBarCycleDispatch;

    routeOutputMinSlider.setVisible(isContinuous && !usesSequenceValues);
    routeOutputMinLabel.setVisible(isContinuous && !usesSequenceValues);
    routeOutputMaxSlider.setVisible(isContinuous && !usesSequenceValues);
    routeOutputMaxLabel.setVisible(isContinuous && !usesSequenceValues);
    routeThresholdSlider.setVisible(!isContinuous && !isActivePattern);
    routeThresholdLabel.setVisible(!isContinuous && !isActivePattern);
    routeSequenceValuesInput.setVisible(usesSequenceValues);
    routeSequenceValuesLabel.setVisible(usesSequenceValues);
    routePhraseLengthBarsSlider.setVisible(isBarCycleDispatch);
    routePhraseLengthBarsLabel.setVisible(isBarCycleDispatch);

    if (isContinuous)
    {
        double rangeMin = -48.0;
        double rangeMax = 48.0;
        switch (parameter)
        {
            case ModulationParameter::Rotation: rangeMin = 0.0; rangeMax = 15.0; break;
            case ModulationParameter::Length:   rangeMin = 1.0; rangeMax = 16.0; break;
            case ModulationParameter::Swing:    rangeMin = 0.0; rangeMax = 100.0; break;
            case ModulationParameter::Rate:     rangeMin = 0.0; rangeMax = 2.0;   break;
            default: break; // Transpose keeps -48..48
        }
        routeOutputMinSlider.setRange(rangeMin, rangeMax, 1.0);
        routeOutputMaxSlider.setRange(rangeMin, rangeMax, 1.0);
    }
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

void ModulatorTargetView::addRouteClicked()
{
    const auto id = routeIdInput.getText().trim().toStdString();
    const ModulationParameter parameter = parameterForComboId(routeParameterCombo.getSelectedId());

    ModulationRoute route;
    route.id = id;
    route.arcDimension = routeArcDimensionCombo.getText().toStdString();
    route.targetInstance = routeTargetInstanceCombo.getSelectedId() == 1
        ? "*" : routeTargetInstanceCombo.getText().toStdString();
    route.patternIndex = juce::jmax(0, routePatternIndexCombo.getSelectedId() - 1);
    route.parameter = parameter;
    route.outputMin = static_cast<float>(routeOutputMinSlider.getValue());
    route.outputMax = static_cast<float>(routeOutputMaxSlider.getValue());
    route.threshold = static_cast<float>(routeThresholdSlider.getValue());
    route.invert = routeInvertToggle.getToggleState();
    route.enabled = routeEnabledToggle.getToggleState();
    switch (routeDispatchModeCombo.getSelectedId())
    {
        case 2:  route.dispatchMode = ModulationDispatchMode::Sequence; break;
        case 3:  route.dispatchMode = ModulationDispatchMode::BarCycle; break;
        default: route.dispatchMode = ModulationDispatchMode::Bar;      break;
    }

    if (route.dispatchMode == ModulationDispatchMode::Sequence || route.dispatchMode == ModulationDispatchMode::BarCycle)
    {
        for (const auto& token : juce::StringArray::fromTokens(routeSequenceValuesInput.getText(), ",", ""))
        {
            const auto trimmed = token.trim();
            if (trimmed.isNotEmpty())
                route.sequenceValues.push_back(trimmed.getFloatValue());
        }
    }

    if (route.dispatchMode == ModulationDispatchMode::BarCycle)
        route.phraseLengthBars = static_cast<int>(routePhraseLengthBarsSlider.getValue());

    std::string errorMessage;
    if (!Validation::isValidModulationRoute(route, errorMessage))
    {
        setStatus("Add modulation route failed: " + errorMessage);
        return;
    }

    processorRef.getComposerCore().getModulationRouteLibrary().addOrReplaceRoute(route);

    juce::String sourceDescription;
    if (route.dispatchMode == ModulationDispatchMode::Sequence)
        sourceDescription = juce::String(route.sequenceValues.size()) + "-value sequence";
    else if (route.dispatchMode == ModulationDispatchMode::BarCycle)
        sourceDescription = juce::String(route.sequenceValues.size()) + "-value/"
                           + juce::String(route.phraseLengthBars) + "-bar cycle";
    else
        sourceDescription = juce::String(routeArcDimensionCombo.getText());
    setStatus("Set modulation route '" + juce::String(id) + "': " + sourceDescription
                   + " -> " + juce::String(modulationParameterToString(parameter))
                   + " on " + juce::String(route.targetInstance));
    routeIdInput.clear();
    refreshAll();
}

void ModulatorTargetView::removeRouteClicked()
{
    if (existingRoutesCombo.getSelectedId() <= 0)
    {
        setStatus("Remove modulation route skipped: no route selected");
        return;
    }

    const auto id = existingRoutesCombo.getText().toStdString();

    if (processorRef.getComposerCore().getModulationRouteLibrary().removeRoute(id))
        setStatus("Removed modulation route '" + juce::String(id) + "'");
    else
        setStatus("Remove failed: no modulation route '" + juce::String(id) + "'");

    refreshAll();
}

void ModulatorTargetView::refreshAll()
{
    auto& core = processorRef.getComposerCore();

    // Pitch-field broadcast controls can also be moved via the MCP bridge -
    // pull them back into sync when the tab is shown.
    pitchFieldEnableToggle.setToggleState(core.isPitchFieldBroadcastEnabled(), juce::dontSendNotification);
    pitchFieldBaseCcSlider.setValue(core.getPitchFieldBroadcastBaseCc(), juce::dontSendNotification);
    pitchFieldChannelSlider.setValue(core.getPitchFieldBroadcastChannel(), juce::dontSendNotification);
    pitchFieldMinBarsSlider.setValue(core.getPitchFieldBroadcastMinBars(), juce::dontSendNotification);
    updatePitchFieldStatus();

    const auto& arcSet = core.getArcSet();

    for (auto* combo : { &arcDimensionCombo, &routeArcDimensionCombo })
    {
        const auto previousArcSelection = combo->getText();
        combo->clear(juce::dontSendNotification);
        int arcItemId = 1;
        int arcSelectId = 0;
        for (const auto& name : arcSet.getArcNames())
        {
            combo->addItem(name, arcItemId);
            if (name == previousArcSelection.toStdString())
                arcSelectId = arcItemId;
            ++arcItemId;
        }
        // ModulatorTarget only: the synthetic blueprint-playhead dimension for
        // driving an external Narrative Scan consumer (OrchConductor via CC102).
        // Not offered for ModulationRoutes - it is not an MPL parameter.
        if (combo == &arcDimensionCombo)
        {
            combo->addItem(ComposerCore::kNarrativePositionDimension, arcItemId);
            if (previousArcSelection.toStdString() == ComposerCore::kNarrativePositionDimension)
                arcSelectId = arcItemId;
            ++arcItemId;
        }
        if (arcSelectId == 0)
            arcSelectId = 1;
        combo->setSelectedId(arcSelectId, juce::dontSendNotification);
    }

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

    juce::String targetsText;
    for (const auto& target : targets)
    {
        targetsText << target.id << "  CC" << target.ccNumber << "  ch=" << target.midiChannel
             << "  mode=" << target.mode;
        if (target.mode == "arc")
            targetsText << "  arc=" << (target.arcDimension.empty() ? "(none)" : target.arcDimension);
        targetsText << "\n";
    }

    if (targets.empty())
        targetsText = "(no modulator targets registered)\n"
               "Pair each target's CC with a Bitwig modulator's own Learn CC "
               "(via a virtual MIDI cable such as loopMIDI - Bitwig's general "
               "'Map to Controller or Key' ignores plugin-generated CC).";

    targetListDisplay.setText(targetsText, juce::dontSendNotification);

    // Instance combo: item 1 is always the "*" broadcast option.
    const auto previousInstanceSelection = routeTargetInstanceCombo.getSelectedId() == 1
        ? juce::String("*") : routeTargetInstanceCombo.getText();
    routeTargetInstanceCombo.clear(juce::dontSendNotification);
    routeTargetInstanceCombo.addItem("* (All Instances)", 1);
    int instanceItemId = 2;
    int instanceSelectId = 0;
    const auto instances = processorRef.getComposerCore().getInstanceRegistry().getAllInstances();
    for (const auto& instance : instances)
    {
        routeTargetInstanceCombo.addItem(instance.id, instanceItemId);
        if (juce::String(instance.id) == previousInstanceSelection)
            instanceSelectId = instanceItemId;
        ++instanceItemId;
    }
    if (instanceSelectId == 0)
        instanceSelectId = 1; // no matching instance still registered (or "*" itself) - fall back to broadcast
    routeTargetInstanceCombo.setSelectedId(instanceSelectId, juce::dontSendNotification);

    const auto routes = processorRef.getComposerCore().getModulationRouteLibrary().getAllRoutes();

    const auto previousRouteSelection = existingRoutesCombo.getText();
    existingRoutesCombo.clear(juce::dontSendNotification);
    int routeItemId = 1;
    int routeSelectId = 0;
    for (const auto& route : routes)
    {
        existingRoutesCombo.addItem(route.id, routeItemId);
        if (route.id == previousRouteSelection.toStdString())
            routeSelectId = routeItemId;
        ++routeItemId;
    }
    existingRoutesCombo.setSelectedId(routeSelectId, juce::dontSendNotification);

    juce::String routesText;
    for (const auto& route : routes)
    {
        const bool isSequence = route.dispatchMode == ModulationDispatchMode::Sequence;
        const bool isBarCycle = route.dispatchMode == ModulationDispatchMode::BarCycle;
        const juce::String sourceLabel = isBarCycle ? juce::String("barCycle")
            : isSequence ? juce::String("sequence") : juce::String(route.arcDimension);
        routesText << route.id << "  " << sourceLabel
                   << " -> " << modulationParameterToString(route.parameter) << "  on " << route.targetInstance;
        if (!isGlobalModulationParameter(route.parameter))
            routesText << " p" << (route.patternIndex + 1);
        if (isSequence || isBarCycle)
        {
            routesText << "  values=[";
            for (size_t i = 0; i < route.sequenceValues.size(); ++i)
                routesText << (i > 0 ? "," : "") << route.sequenceValues[i];
            routesText << "]";
            if (isBarCycle)
                routesText << "/" << route.phraseLengthBars << "bars";
        }
        else if (isContinuousModulationParameter(route.parameter))
            routesText << "  range=[" << route.outputMin << "," << route.outputMax << "]";
        else if (route.parameter != ModulationParameter::ActivePattern)
            routesText << "  threshold=" << route.threshold;
        if (route.invert)
            routesText << "  inverted";
        if (!route.enabled)
            routesText << "  DISABLED";
        routesText << "\n";
    }

    if (routes.empty())
        routesText = "(no modulation routes registered)\n"
               "Wire an arc dimension directly to a real MPL parameter on one instance or "
               "broadcast to all - this is the modulation matrix, distinct from the external "
               "targets above.";

    routeListDisplay.setText(routesText, juce::dontSendNotification);
}
