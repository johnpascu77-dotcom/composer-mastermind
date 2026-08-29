#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class ComposerMastermindAudioProcessor;

// Modulators tab: two independent panels, both wrapped in an internal
// Viewport (content is taller than most tab real estate allows) since it
// hosts two full forms rather than one.
//
// Panel 1 - registers ModulatorTargets: named CC numbers Composer Mastermind
// treats as addressable "control channels" for something outside the
// Instance/Mutation model, typically a Bitwig modulator's own parameter
// paired via Bitwig's own Learn CC (see model/ModulatorTarget.h). "Arc" mode
// targets are driven continuously here (every bar, from the picked ArcSet
// dimension); "Section" mode targets get their value authored per-section
// instead, on the Blueprint tab's Sections page (ui/BlueprintSectionsContent).
//
// Panel 2 - registers ModulationRoutes (the modulation matrix, 2026-08-27,
// model/ModulationRoute.h): wires an arc dimension directly to a real MPL
// parameter (Transpose/Rotation/Length/Swing/Rate/Inversion/Retrograde/M7/
// ActivePattern/GridMode) on a specific instance/pattern or broadcast to
// every instance ("*"). A continuous parameter (Transpose/Rotation/Length/
// Swing/Rate) can dispatch on the Bar clock (arc-sampled, the original
// behavior), the Sequence clock (2026-08-27 "fragment sequencer" - steps
// through a hand-authored value list once per pattern LOOP CYCLE instead of
// once per bar, capped at one bar by MPL's own pattern Length), or the
// BarCycle clock (2026-08-27 phrase-cadence breathing - steps through the
// same kind of value list once per phraseLengthBars BARS instead, genuine
// multi-bar phrasing with no one-bar ceiling - see model/ModulationRoute.h's
// ModulationDispatchMode). Deliberately a separate panel, not a merge into
// Panel 1's form - a ModulatorTarget addresses a raw (channel, CC) pair with
// no idea what's on the other end; a ModulationRoute addresses a real
// Composer Mastermind Instance and is encoded through CCMapping's own
// per-parameter rules, a genuinely different concept.
class ModulatorTargetView : public juce::Component,
                            private juce::Timer
{
public:
    ModulatorTargetView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);
    ~ModulatorTargetView() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Rebuilds both panels' displayed lists and combos (arc dimensions,
    // registered instances) - can't change at runtime today for arc names,
    // but instances can, so this is called whenever the tab becomes visible.
    void refreshAll();

private:
    void addTargetClicked();
    void removeTargetClicked();
    void sendForPairingClicked();
    void modeChanged();

    void addRouteClicked();
    void removeRouteClicked();
    void routeParameterChanged();

    void timerCallback() override;
    void pushPitchFieldSettings();
    void updatePitchFieldStatus();

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;

    // Viewport host - both panels below live inside `content`, which grows
    // taller than the tab and scrolls, rather than everything being crammed
    // into whatever space the tab happens to have.
    juce::Viewport viewport;
    juce::Component content;

    // --- Panel 0: pitch-field broadcast (MC -> OrchNoteFilter, Orch ecosystem) ---
    juce::Label pitchFieldHeader { {}, "Pitch Field Broadcast  (MC -> OrchNoteFilter, 2 CCs from Base)" };
    juce::ToggleButton pitchFieldEnableToggle { "Broadcast the pitch classes the voices are sounding" };
    juce::Slider pitchFieldBaseCcSlider;
    juce::Label pitchFieldBaseCcLabel { {}, "Base CC" };
    juce::Slider pitchFieldChannelSlider;
    juce::Label pitchFieldChannelLabel { {}, "Channel" };
    juce::Slider pitchFieldMinBarsSlider;
    juce::Label pitchFieldMinBarsLabel { {}, "Min Bars" };
    juce::TextButton pitchFieldResyncButton { "Re-broadcast now" };
    juce::Label pitchFieldStatusLabel;

    // --- Panel 1: external Modulator Targets (unchanged from before) ---
    juce::Label externalTargetsHeader { {}, "External Modulator Targets (Bitwig / other CC destinations)" };
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

    juce::ComboBox existingTargetsCombo;
    juce::TextButton removeTargetButton { "Remove" };
    juce::TextButton sendForPairingButton { "Send for Pairing" };

    // --- Panel 2: Instance Modulation Routes (the modulation matrix) ---
    juce::Label routesHeader { {}, "Instance Modulation Routes (drive a real MPL parameter)" };
    juce::TextEditor routeIdInput;
    juce::ComboBox routeArcDimensionCombo;
    juce::Label routeArcDimensionLabel { {}, "Arc Dimension" };
    juce::ComboBox routeTargetInstanceCombo;
    juce::Label routeTargetInstanceLabel { {}, "Instance" };
    juce::ComboBox routePatternIndexCombo;
    juce::Label routePatternIndexLabel { {}, "Pattern" };
    juce::ComboBox routeParameterCombo;
    juce::Label routeParameterLabel { {}, "Parameter" };
    juce::ComboBox routeDispatchModeCombo;
    juce::Label routeDispatchModeLabel { {}, "Dispatch" };
    juce::TextEditor routeSequenceValuesInput;
    juce::Label routeSequenceValuesLabel { {}, "Sequence (comma-separated)" };
    juce::Slider routePhraseLengthBarsSlider;
    juce::Label routePhraseLengthBarsLabel { {}, "Phrase Bars" };
    juce::Slider routeOutputMinSlider;
    juce::Label routeOutputMinLabel { {}, "Min" };
    juce::Slider routeOutputMaxSlider;
    juce::Label routeOutputMaxLabel { {}, "Max" };
    juce::Slider routeThresholdSlider;
    juce::Label routeThresholdLabel { {}, "Threshold" };
    juce::ToggleButton routeInvertToggle { "Invert" };
    juce::ToggleButton routeEnabledToggle { "Enabled" };
    juce::TextButton addRouteButton { "Add / Update" };
    juce::TextEditor routeListDisplay;

    juce::ComboBox existingRoutesCombo;
    juce::TextButton removeRouteButton { "Remove" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulatorTargetView)
};
