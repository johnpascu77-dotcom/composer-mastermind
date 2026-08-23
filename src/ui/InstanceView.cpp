#include "InstanceView.h"
#include "../plugin/PluginProcessor.h"
#include "../model/Instance.h"
#include "../util/Validation.h"

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

InstanceView::InstanceView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> status)
    : processorRef(processor), setStatus(std::move(status))
{
    addAndMakeVisible(instanceIdInput);
    instanceIdInput.setTextToShowWhenEmpty("instance id, e.g. mpl_1", juce::Colours::grey);

    addAndMakeVisible(instanceChannelSlider);
    styleSlider(instanceChannelSlider, 1.0, 16.0, 1.0, 1.0);
    addAndMakeVisible(instanceChannelLabel);

    addAndMakeVisible(instanceRoleCombo);
    instanceRoleCombo.addItem("Unrestricted", 1);
    instanceRoleCombo.addItem("Anchor", 2);
    instanceRoleCombo.addItem("Motif", 3);
    instanceRoleCombo.addItem("Counterpoint", 4);
    instanceRoleCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(addInstanceButton);
    addInstanceButton.onClick = [this] { addInstanceClicked(); };

    addAndMakeVisible(existingInstancesCombo);
    existingInstancesCombo.setTextWhenNothingSelected("(pick existing instance)");

    addAndMakeVisible(removeInstanceButton);
    removeInstanceButton.onClick = [this] { removeInstanceClicked(); };

    addAndMakeVisible(loadSelectedButton);
    loadSelectedButton.onClick = [this] { loadSelectedClicked(); };

    addAndMakeVisible(resetButton);
    resetButton.onClick = [this] { resetClicked(); };

    addAndMakeVisible(instanceListDisplay);
    instanceListDisplay.setMultiLine(true);
    instanceListDisplay.setReadOnly(true);
    instanceListDisplay.setScrollbarsShown(true);
    instanceListDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    refreshInstanceList();
}

void InstanceView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void InstanceView::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto controlsRow = area.removeFromTop(kRowHeight);
    instanceIdInput.setBounds(controlsRow.removeFromLeft(160));
    controlsRow.removeFromLeft(kMargin);
    instanceChannelLabel.setBounds(controlsRow.removeFromLeft(60));
    instanceChannelSlider.setBounds(controlsRow.removeFromLeft(110));
    controlsRow.removeFromLeft(kMargin);
    instanceRoleCombo.setBounds(controlsRow.removeFromLeft(90));
    controlsRow.removeFromLeft(kMargin);
    addInstanceButton.setBounds(controlsRow.removeFromLeft(90));

    area.removeFromTop(kRowGap);
    auto existingRow = area.removeFromTop(kRowHeight);
    existingInstancesCombo.setBounds(existingRow.removeFromLeft(200));
    existingRow.removeFromLeft(kMargin);
    loadSelectedButton.setBounds(existingRow.removeFromLeft(70));
    existingRow.removeFromLeft(kMargin);
    removeInstanceButton.setBounds(existingRow.removeFromLeft(70));
    existingRow.removeFromLeft(kMargin);
    resetButton.setBounds(existingRow.removeFromLeft(160));

    area.removeFromTop(kRowGap);
    instanceListDisplay.setBounds(area);
}

void InstanceView::addInstanceClicked()
{
    const auto id = instanceIdInput.getText().trim().toStdString();
    const int channel = static_cast<int>(instanceChannelSlider.getValue());

    static const char* roleNames[] = { "", "anchor", "motif", "counterpoint" };
    const int roleIndex = juce::jlimit(0, 3, instanceRoleCombo.getSelectedItemIndex());
    const std::string role = roleNames[roleIndex];

    Instance instance(id, id, channel, role);

    std::string errorMessage;
    if (!Validation::isValidInstance(instance, errorMessage))
    {
        setStatus("Add instance failed: " + errorMessage);
        return;
    }

    Instance existing;
    const bool alreadyExisted = processorRef.getComposerCore().getInstanceRegistry().getInstanceById(id, existing);

    processorRef.getComposerCore().getInstanceRegistry().addInstance(instance); // upserts by id

    setStatus((alreadyExisted ? "Updated instance '" : "Added instance '") + juce::String(id) + "' on channel "
                   + juce::String(channel) + (role.empty() ? juce::String() : " (role: " + juce::String(role) + ")"));
    instanceIdInput.clear();
    refreshInstanceList();
}

void InstanceView::removeInstanceClicked()
{
    if (existingInstancesCombo.getSelectedId() <= 0)
    {
        setStatus("Remove skipped: no instance selected");
        return;
    }

    const auto id = existingInstancesCombo.getText().toStdString();

    if (processorRef.getComposerCore().getInstanceRegistry().removeInstance(id))
        setStatus("Removed instance '" + juce::String(id) + "'");
    else
        setStatus("Remove failed: no instance '" + juce::String(id) + "'");

    refreshInstanceList();
}

void InstanceView::loadSelectedClicked()
{
    if (existingInstancesCombo.getSelectedId() <= 0)
    {
        setStatus("Load skipped: no instance selected");
        return;
    }

    const auto id = existingInstancesCombo.getText().toStdString();

    Instance instance;
    if (!processorRef.getComposerCore().getInstanceRegistry().getInstanceById(id, instance))
    {
        setStatus("Load failed: '" + juce::String(id) + "' not found");
        return;
    }

    instanceIdInput.setText(instance.id, juce::dontSendNotification);
    instanceChannelSlider.setValue(instance.midiChannel, juce::dontSendNotification);

    static const char* roleNames[] = { "", "anchor", "motif", "counterpoint" };
    int roleItemId = 1;
    for (int i = 0; i < 4; ++i)
    {
        if (instance.role == roleNames[i])
        {
            roleItemId = i + 1;
            break;
        }
    }
    instanceRoleCombo.setSelectedId(roleItemId, juce::dontSendNotification);

    setStatus("Loaded instance '" + juce::String(id) + "' - edit above and Add Instance to update it");
}

void InstanceView::resetClicked()
{
    auto options = juce::MessageBoxOptions::makeOptionsOkCancel(
        juce::MessageBoxIconType::WarningIcon,
        "New Project",
        "This clears every instance, scene, blueprint, preset, and modulator target in this project. "
        "It cannot be undone within this session. Continue?",
        "Reset",
        "Cancel");

    juce::NativeMessageBox::showAsync(options, [this](int result)
    {
        if (result != 0) // button index, 0-based in the order passed to makeOptionsOkCancel - 0 = "Reset"
            return;

        processorRef.getComposerCore().resetToFactoryDefaults();
        setStatus("New Project - every library cleared");
        refreshInstanceList();
    });
}

void InstanceView::refreshInstanceList()
{
    const auto instances = processorRef.getComposerCore().getInstanceRegistry().getAllInstances();

    const auto previousSelection = existingInstancesCombo.getText();
    existingInstancesCombo.clear(juce::dontSendNotification);
    int itemId = 1;
    int selectId = 0;
    for (const auto& instance : instances)
    {
        existingInstancesCombo.addItem(instance.id, itemId);
        if (instance.id == previousSelection.toStdString())
            selectId = itemId;
        ++itemId;
    }
    existingInstancesCombo.setSelectedId(selectId, juce::dontSendNotification);

    juce::String text;
    text << "Coherence: " << juce::String(processorRef.getComposerCore().getCurrentCoherence(), 2)
         << "  (how much registered instances currently agree with each other)\n";

    for (const auto& instance : instances)
    {
        text << instance.id << "  ch=" << instance.midiChannel
             << "  " << (instance.enabled ? "enabled" : "disabled")
             << "  role=" << (instance.role.empty() ? "unrestricted" : instance.role) << "\n";
    }

    if (instances.empty())
        text << "(no instances registered)";

    instanceListDisplay.setText(text, juce::dontSendNotification);
}
