#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "ArcGraphView.h"
#include "BlueprintSectionsView.h"

class ComposerMastermindAudioProcessor;

// Blueprint tab shell: hosts two inner tabs, "Arcs" (ui/ArcGraphView, the
// draggable breakpoint graph) and "Sections" (ui/BlueprintSectionsView, the
// Blueprint JSON sections/layer-role/budget-override editor). Split into two
// inner tabs once the Blueprint tab grew a second concern beyond arcs -
// mirrors ui/EditorView's outer TabbedComponent shape at a smaller scale.
class BlueprintView : public juce::Component
{
public:
    BlueprintView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    // Delegates to BlueprintSectionsView::refreshAll(); ArcGraphView doesn't
    // need external refresh, it reads ArcSet directly in paint().
    void refreshAll();

private:
    juce::TabbedComponent innerTabs { juce::TabbedButtonBar::TabsAtTop };
    ArcGraphView arcGraphView;
    BlueprintSectionsView sectionsView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlueprintView)
};
