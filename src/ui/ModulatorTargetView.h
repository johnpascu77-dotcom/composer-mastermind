#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class ComposerMastermindAudioProcessor;

// Modulators tab: registers ModulatorTargets - named CC numbers Composer
// Mastermind treats as addressable "control channels" for something outside
// the Instance/Mutation model, typically a Bitwig modulator's own parameter
// paired via Bitwig's own Learn CC (see model/ModulatorTarget.h). "Arc" mode
// targets are driven continuously here (every bar, from the picked ArcSet
// dimension); "Section" mode targets get their value authored per-section
// instead, on the Blueprint tab's Sections page (ui/BlueprintSectionsContent).
class ModulatorTargetView : public juce::Component
{
public:
    ModulatorTargetView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    // Rebuilds the displayed list and the arc-dimension combo (from
    // ArcSet::getArcNames(), which can't change at runtime today but is
    // refreshed defensively the same way every other combo here is).
    void refreshAll();

private:
    void addTargetClicked();
    void removeTargetClicked();
    void sendForPairingClicked();
    void modeChanged();

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;

    juce::TextEditor targetIdInput;
    juce::Slider ccNumberSlider;
    juce::Label ccNumberLabel { {}, "CC" };
    juce::Slider channelSlider;
    juce::Label channelLabel { {}, "Channel" };
    juce::ComboBox modeCombo;
    juce::Label modeLabel { {}, "Mode" };
    juce::ComboBox arcDimensionCombo;
    juce::Label arcDimensionLabel { {}, "Arc Dimension" };
    juce::TextButton addTargetButton { "Add / Update" };
    juce::TextEditor targetListDisplay;

    // Existing-target selection: registered targets picked from this combo
    // (not retyped) for Remove and Send for Pairing, since re-sending the
    // MPL-style "type the id, then click" flow doesn't scale once several
    // targets exist - see ui/SceneListComponent's saved-item combo for the
    // pattern this mirrors.
    juce::ComboBox existingTargetsCombo;
    juce::TextButton removeTargetButton { "Remove" };
    juce::TextButton sendForPairingButton { "Send for Pairing" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulatorTargetView)
};
