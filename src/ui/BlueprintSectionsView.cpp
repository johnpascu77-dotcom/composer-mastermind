#include "BlueprintSectionsView.h"

BlueprintSectionsView::BlueprintSectionsView(ComposerMastermindAudioProcessor& processor,
                                              std::function<void(const juce::String&)> status)
    : content(processor, std::move(status))
{
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
}

void BlueprintSectionsView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void BlueprintSectionsView::resized()
{
    viewport.setBounds(getLocalBounds());
    // Content is always taller than the viewport (that's the point of this
    // wrapper), so the vertical scrollbar always shows - reserve its width
    // up front rather than fighting a chicken-and-egg layout pass.
    content.setSize(viewport.getWidth() - viewport.getScrollBarThickness(), BlueprintSectionsContent::getPreferredHeight());
}

void BlueprintSectionsView::refreshAll()
{
    content.refreshAll();
}
