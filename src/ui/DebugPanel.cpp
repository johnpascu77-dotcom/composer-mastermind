#include "DebugPanel.h"
#include "../plugin/PluginProcessor.h"
#include "../model/Mutation.h"
#include "../model/Instance.h"
#include "../model/ComposerState.h"
#include "../util/Validation.h"
#include "../midi/CCMapping.h"

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

DebugPanel::DebugPanel(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> status)
    : processorRef(processor), setStatus(std::move(status))
{
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

    addAndMakeVisible(mutationPreviewLabel);
    mutationPreviewLabel.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle("Italic")));

    mutationTargetCombo.onChange = [this] { refreshMutationPreview(); };
    mutationPatternCombo.onChange = [this] { refreshMutationPreview(); };
    mutationTypeCombo.onChange = [this] { refreshMutationPreview(); };
    mutationAmountSlider.onValueChange = [this] { refreshMutationPreview(); };

    addAndMakeVisible(sendMutationButton);
    sendMutationButton.onClick = [this] { sendMutationClicked(); };

    addAndMakeVisible(testCCSectionLabel);
    testCCSectionLabel.setFont(juce::Font(juce::FontOptions().withHeight(16.0f).withStyle("Bold")));

    addAndMakeVisible(testCCTargetCombo);
    addAndMakeVisible(testCCTargetLabel);

    addAndMakeVisible(testCCNumberCombo);
    for (const auto& named : CCMapping::kNamedCCs)
        testCCNumberCombo.addItem(named.label, named.cc);
    testCCNumberCombo.setSelectedId(CCMapping::kNamedCCs.front().cc, juce::dontSendNotification);
    addAndMakeVisible(testCCNumberLabel);

    addAndMakeVisible(testCCValueSlider);
    styleSlider(testCCValueSlider, 0.0, 127.0, 1.0, 64.0);
    addAndMakeVisible(testCCValueLabel);

    addAndMakeVisible(sendTestCCButton);
    sendTestCCButton.onClick = [this] { sendTestCCClicked(); };

    refreshTargets();
}

juce::String DebugPanel::describeMutationPreview() const
{
    if (mutationTargetCombo.getSelectedId() <= 0)
        return "(no target instance selected)";

    const auto targetId = mutationTargetCombo.getText().toStdString();
    const int patternIndex = juce::jlimit(0, 2, mutationPatternCombo.getSelectedId() - 1);
    const int typeIndex = juce::jlimit(0, 3, mutationTypeCombo.getSelectedItemIndex());
    const int amount = static_cast<int>(mutationAmountSlider.getValue());

    InstanceParameterState state; // defaults if this instance has never been tracked yet
    processorRef.getComposerCore().getInstanceStateTracker().getState(targetId, state);
    const auto& pattern = state.patterns[static_cast<size_t>(patternIndex)];

    juce::String text;
    switch (typeIndex)
    {
        case 0: // Transpose
        {
            const int newValue = juce::jlimit(-CCMapping::kMaxTranspose, CCMapping::kMaxTranspose,
                                               pattern.transpose + amount);
            text << "current: " << pattern.transpose << " semitones  ->  new: " << newValue << " semitones";
            break;
        }
        case 1: // Rotation
        {
            const int newValue = CCMapping::wrapRotation(pattern.rotation + amount);
            text << "current: step " << pattern.rotation << "  ->  new: step " << newValue
                 << "  (of " << CCMapping::kPatternSteps << ", wraps)";
            break;
        }
        case 2: // Length
        {
            const int newValue = juce::jlimit(CCMapping::kMinPatternLoopLength, CCMapping::kPatternSteps,
                                               pattern.length + amount);
            text << "current: " << pattern.length << " steps  ->  new: " << newValue << " steps";
            break;
        }
        default: // Inversion - absolute toggle, not a delta (a boolean has no sensible "nudge")
        {
            const bool newValue = amount != 0;
            text << "current: " << (pattern.inversion ? "on" : "off")
                 << "  ->  new: " << (newValue ? "on" : "off") << "  (any non-zero amount = on)";
            break;
        }
    }

    return text;
}

void DebugPanel::refreshMutationPreview()
{
    mutationPreviewLabel.setText(describeMutationPreview(), juce::dontSendNotification);
}

void DebugPanel::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void DebugPanel::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto nextRow = [&](int height) -> juce::Rectangle<int>
    {
        auto row = area.removeFromTop(height);
        area.removeFromTop(kRowGap);
        return row;
    };

    auto targetRow = nextRow(kRowHeight);
    mutationTargetLabel.setBounds(targetRow.removeFromLeft(180));
    mutationTargetCombo.setBounds(targetRow);

    auto patternRow = nextRow(kRowHeight);
    mutationPatternLabel.setBounds(patternRow.removeFromLeft(180));
    mutationPatternCombo.setBounds(patternRow.removeFromLeft(150));

    auto typeRow = nextRow(kRowHeight);
    mutationTypeLabel.setBounds(typeRow.removeFromLeft(180));
    mutationTypeCombo.setBounds(typeRow.removeFromLeft(150));

    auto amountRow = nextRow(kRowHeight);
    mutationAmountLabel.setBounds(amountRow.removeFromLeft(220));
    mutationAmountSlider.setBounds(amountRow);

    mutationPreviewLabel.setBounds(nextRow(kRowHeight));

    sendMutationButton.setBounds(nextRow(kRowHeight).removeFromLeft(150));

    area.removeFromTop(kRowGap * 2);
    testCCSectionLabel.setBounds(nextRow(kRowHeight + 4));

    auto ccTargetRow = nextRow(kRowHeight);
    testCCTargetLabel.setBounds(ccTargetRow.removeFromLeft(180));
    testCCTargetCombo.setBounds(ccTargetRow);

    auto ccNumberRow = nextRow(kRowHeight);
    testCCNumberLabel.setBounds(ccNumberRow.removeFromLeft(180));
    testCCNumberCombo.setBounds(ccNumberRow.removeFromLeft(220));

    auto ccValueRow = nextRow(kRowHeight);
    testCCValueLabel.setBounds(ccValueRow.removeFromLeft(180));
    testCCValueSlider.setBounds(ccValueRow);

    sendTestCCButton.setBounds(nextRow(kRowHeight).removeFromLeft(150));
}

void DebugPanel::sendMutationClicked()
{
    if (mutationTargetCombo.getSelectedId() <= 0)
    {
        setStatus("Send mutation skipped: no target instance");
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
        setStatus("Send mutation failed: " + errorMessage);
        return;
    }

    const auto previewBeforeSend = describeMutationPreview();

    auto& composerCore = processorRef.getComposerCore();
    const bool sent = composerCore.getRouter().routeMutation(mutation, composerCore.getCurrentBar());

    if (sent)
        setStatus("Sent " + juce::String(mutation.type) + " mutation to '" + juce::String(mutation.targetInstance)
                       + "': " + previewBeforeSend);
    else
        setStatus("Mutation to '" + juce::String(mutation.targetInstance)
                      + "' blocked (policy budget exceeded for this bar, or unknown target)");

    refreshMutationPreview();
}

void DebugPanel::sendTestCCClicked()
{
    if (testCCTargetCombo.getSelectedId() <= 0)
    {
        setStatus("Send test CC skipped: no target instance");
        return;
    }

    Instance instance;
    const auto targetId = testCCTargetCombo.getText().toStdString();
    if (!processorRef.getComposerCore().getInstanceRegistry().getInstanceById(targetId, instance))
    {
        setStatus("Send test CC failed: unknown target instance");
        return;
    }

    const int cc = testCCNumberCombo.getSelectedId();
    const int value = static_cast<int>(testCCValueSlider.getValue());
    processorRef.getComposerCore().getCCDispatcher().sendCC(instance.midiChannel, cc, value);

    setStatus("Sent CC " + juce::String(cc) + "=" + juce::String(value)
                   + " to '" + juce::String(targetId) + "' (channel " + juce::String(instance.midiChannel) + ")");
}

void DebugPanel::refreshComboFromInstances(juce::ComboBox& combo)
{
    const auto instances = processorRef.getComposerCore().getInstanceRegistry().getAllInstances();

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

    if (selectId == 0 && !instances.empty())
        selectId = 1;

    combo.setSelectedId(selectId, juce::dontSendNotification);
}

void DebugPanel::refreshTargets()
{
    refreshComboFromInstances(mutationTargetCombo);
    refreshComboFromInstances(testCCTargetCombo);
    refreshMutationPreview();
}
