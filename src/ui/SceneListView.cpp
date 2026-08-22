#include "SceneListView.h"

SceneListView::SceneListView(ComposerMastermindAudioProcessor& processor,
                              std::function<void(const juce::String&)> status)
    : content(processor, std::move(status))
{
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
}

void SceneListView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void SceneListView::resized()
{
    viewport.setBounds(getLocalBounds());
    content.setSize(viewport.getWidth() - viewport.getScrollBarThickness(), SceneListComponent::getPreferredHeight());
}

void SceneListView::refreshAll()
{
    content.refreshAll();
}
