#include "PresetLibraryView.h"

PresetLibraryView::PresetLibraryView(ComposerMastermindAudioProcessor& processor,
                                      std::function<void(const juce::String&)> status)
    : content(processor, std::move(status))
{
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
}

void PresetLibraryView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PresetLibraryView::resized()
{
    viewport.setBounds(getLocalBounds());
    content.setSize(viewport.getWidth() - viewport.getScrollBarThickness(), PresetLibraryContent::getPreferredHeight());
}

void PresetLibraryView::refreshAll()
{
    content.refreshAll();
}
