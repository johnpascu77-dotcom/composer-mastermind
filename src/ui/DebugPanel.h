#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class ComposerMastermindAudioProcessor;

// Mutations/Debug tab: push a single per-pattern transpose/rotation/length/
// inversion change to one instance, and see whether the v0.4 policy gate
// let it through. Also hosts Send Test CC, a raw one-shot CC sender used to
// pair Bitwig controls (e.g. a modulator's own "Learn CC") to a known CC
// number on demand.
class DebugPanel : public juce::Component
{
public:
    DebugPanel(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    // Repopulates the target-instance combo from the current registry.
    void refreshTargets();

private:
    void sendMutationClicked();
    void sendTestCCClicked();
    void refreshComboFromInstances(juce::ComboBox& combo);

    // "current -> new" readout for whatever the mutation controls currently
    // describe, computed the same way Router::routeMutation itself resolves
    // amount (delta from InstanceStateTracker's last known state, same
    // clamp/wrap per type) - so what you see here is what will actually
    // happen, without needing to check Awareness or do the arithmetic by
    // hand first. Refreshed on every control change and on the shared
    // 300ms tab timer (via refreshTargets()), since tracked state can also
    // change from elsewhere (e.g. the motif/generative layer).
    juce::String describeMutationPreview() const;
    void refreshMutationPreview();

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;

    juce::ComboBox mutationTargetCombo;
    juce::Label mutationTargetLabel { {}, "Target Instance" };
    juce::ComboBox mutationPatternCombo;
    juce::Label mutationPatternLabel { {}, "Pattern" };
    juce::ComboBox mutationTypeCombo;
    juce::Label mutationTypeLabel { {}, "Type" };
    juce::Slider mutationAmountSlider;
    juce::Label mutationAmountLabel { {}, "Amount (delta from current)" };
    juce::Label mutationPreviewLabel;
    juce::TextButton sendMutationButton { "Send Mutation" };

    // Send Test CC: fires one raw CC message immediately on a target
    // instance's channel, bypassing Mutation/Router/PolicyEngine entirely.
    // Not a musical action - a diagnostic for pairing an unmapped Bitwig
    // control (e.g. a modulator's own knob via its own "Learn CC") to a
    // known CC number, since that pairing needs one deliberate CC to arrive
    // on demand rather than whenever a real mutation happens to fire.
    juce::Label testCCSectionLabel { {}, "Send Test CC" };
    juce::ComboBox testCCTargetCombo;
    juce::Label testCCTargetLabel { {}, "Target Instance" };
    juce::ComboBox testCCNumberCombo;
    juce::Label testCCNumberLabel { {}, "CC" };
    juce::Slider testCCValueSlider;
    juce::Label testCCValueLabel { {}, "Value (0-127)" };
    juce::TextButton sendTestCCButton { "Send Once" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebugPanel)
};
