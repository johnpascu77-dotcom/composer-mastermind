#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SceneListComponent.h"

class ComposerMastermindAudioProcessor;

// "Scenes" tab: a thin juce::Viewport host around ui/SceneListComponent,
// which owns the actual controls. Split out once the panel grew past the
// tab's available height (target/override became real pending lists
// instead of "always everyone" / "at most one override") - same reason
// ui/BlueprintSectionsView needed one for the Sections tab.
class SceneListView : public juce::Component
{
public:
    SceneListView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    void refreshAll();

private:
    juce::Viewport viewport;
    SceneListComponent content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SceneListView)
};
