#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "PresetLibraryContent.h"

class ComposerMastermindAudioProcessor;

// Presets tab: a thin juce::Viewport host around ui/PresetLibraryContent,
// which owns the actual controls. Split out once role presets +
// rhythmic-relationship presets together outgrew the tab's available
// height - same fix as ui/BlueprintSectionsView.
class PresetLibraryView : public juce::Component
{
public:
    PresetLibraryView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    void refreshAll();

private:
    juce::Viewport viewport;
    PresetLibraryContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetLibraryView)
};
