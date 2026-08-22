#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class ComposerMastermindAudioProcessor;

// Instances tab: register/remove the MPL instances this conductor drives,
// assign each a role, and see the live coherence score alongside them.
class InstanceView : public juce::Component
{
public:
    InstanceView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    // Rebuilds the displayed list/coherence text from the current registry
    // state. Called after any action that might have changed it, and
    // periodically by EditorView's timer to catch changes made from other
    // tabs (e.g. a scene sent from the Scenes tab).
    void refreshInstanceList();

private:
    void addInstanceClicked();
    void removeInstanceClicked();
    void loadSelectedClicked();

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;

    juce::TextEditor instanceIdInput;
    juce::Slider instanceChannelSlider;
    juce::Label instanceChannelLabel { {}, "Channel" };
    juce::ComboBox instanceRoleCombo;
    juce::TextButton addInstanceButton { "Add Instance" };
    juce::TextEditor instanceListDisplay;

    // Existing-instance selection: registered instances picked from this
    // combo (not retyped) for Remove and Load - mirrors ui/
    // ModulatorTargetView's identical fix for the same friction ("has to
    // be rewritten in the empty box in order to remove it").
    juce::ComboBox existingInstancesCombo;
    juce::TextButton removeInstanceButton { "Remove" };
    juce::TextButton loadSelectedButton { "Load" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstanceView)
};
