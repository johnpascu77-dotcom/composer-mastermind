#include "ActivityLogView.h"
#include "../plugin/PluginProcessor.h"

namespace
{
    constexpr int kMargin = 10;
    constexpr int kHeaderHeight = 18;
}

ActivityLogView::ActivityLogView(ComposerMastermindAudioProcessor& processor)
    : processorRef(processor)
{
    addAndMakeVisible(headerLabel);
    headerLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Italic")));

    addAndMakeVisible(logDisplay);
    logDisplay.setMultiLine(true);
    logDisplay.setReadOnly(true);
    logDisplay.setScrollbarsShown(true);
    logDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    refreshAll();
}

void ActivityLogView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void ActivityLogView::resized()
{
    auto area = getLocalBounds().reduced(kMargin);
    headerLabel.setBounds(area.removeFromTop(kHeaderHeight));
    area.removeFromTop(4);
    logDisplay.setBounds(area);
}

void ActivityLogView::refreshAll()
{
    const auto entries = processorRef.getComposerCore().getRecentActivityLog();

    if (entries.empty())
    {
        logDisplay.setText("(no activity yet - enters here once a section starts, or Prime for Playback is pressed)",
            juce::dontSendNotification);
        return;
    }

    juce::String text;
    for (const auto& entry : entries)
        text << "bar " << entry.bar << "  " << entry.message << "\n";

    logDisplay.setText(text, juce::dontSendNotification);
}
