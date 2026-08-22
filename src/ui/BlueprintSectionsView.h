#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "BlueprintSectionsContent.h"

class ComposerMastermindAudioProcessor;

// "Sections" inner tab of the Blueprint tab: a thin juce::Viewport host
// around ui/BlueprintSectionsContent, which owns the actual controls. Split
// out once the Sections form grew past the tab's available height (adding
// reserved-value authoring on top of layer roles + budget overrides) -
// scrolling here is the correct fix rather than cramming rows tighter, since
// this form is likely to keep growing as more section-level concepts get
// added.
class BlueprintSectionsView : public juce::Component
{
public:
    BlueprintSectionsView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    void refreshAll();

private:
    juce::Viewport viewport;
    BlueprintSectionsContent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlueprintSectionsView)
};
