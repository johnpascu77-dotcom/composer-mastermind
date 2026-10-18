#include "BlueprintView.h"
#include "../plugin/PluginProcessor.h"

BlueprintView::BlueprintView(ComposerMastermindAudioProcessor& processor,
                              std::function<void(const juce::String&)> status)
    : arcGraphView(processor.getComposerCore().getArcSet()), sectionsView(processor, status)
{
    addAndMakeVisible(innerTabs);

    const auto tabColour = juce::Colours::transparentBlack;
    innerTabs.addTab("Arcs", tabColour, &arcGraphView, false);
    innerTabs.addTab("Sections", tabColour, &sectionsView, false);
    innerTabs.applyHighlight();
    innerTabs.setTabTooltips({"Arcs: edit the five arc curves (energy, tension, density, complexity, coherence) over the bars.",
                              "Sections: the stretches of bars that make the piece, each playing a scene, with its own arc shaping."});
}

void BlueprintView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void BlueprintView::resized()
{
    innerTabs.setBounds(getLocalBounds());
}

void BlueprintView::refreshAll()
{
    sectionsView.refreshAll();
}
