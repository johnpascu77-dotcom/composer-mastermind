#include "MilestoneView.h"
#include "../plugin/PluginProcessor.h"

namespace
{
    constexpr int kMargin = 10;
    constexpr int kHeaderHeight = 18;
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 4;
}

MilestoneView::MilestoneView(ComposerMastermindAudioProcessor& processor,
                              std::function<void(const juce::String&)> status)
    : processorRef(processor), setStatus(std::move(status))
{
    addAndMakeVisible(headerLabel);
    headerLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Italic")));

    addAndMakeVisible(listDisplay);
    listDisplay.setMultiLine(true);
    listDisplay.setReadOnly(true);
    listDisplay.setScrollbarsShown(true);
    listDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    addAndMakeVisible(restoreCombo);
    restoreCombo.setTextWhenNothingSelected("(select a milestone)");

    addAndMakeVisible(restoreButton);
    restoreButton.onClick = [this] { restoreClicked(); };

    refreshAll();
}

void MilestoneView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MilestoneView::resized()
{
    auto area = getLocalBounds().reduced(kMargin);
    headerLabel.setBounds(area.removeFromTop(kHeaderHeight));
    area.removeFromTop(4);

    auto restoreRow = area.removeFromBottom(kRowHeight);
    restoreButton.setBounds(restoreRow.removeFromRight(100));
    restoreRow.removeFromRight(kMargin);
    restoreCombo.setBounds(restoreRow);
    area.removeFromBottom(kRowGap);

    listDisplay.setBounds(area);
}

void MilestoneView::refreshAll()
{
    const auto previousSelection = restoreCombo.getText();

    const auto milestones = processorRef.getComposerCore().getMilestoneLibrary().getAll();

    if (milestones.empty())
    {
        listDisplay.setText("(no milestones yet - captured automatically each time a section starts)",
            juce::dontSendNotification);
        restoreCombo.clear(juce::dontSendNotification);
        return;
    }

    juce::String text;
    restoreCombo.clear(juce::dontSendNotification);

    int itemId = 1;
    int selectId = 0;
    for (const auto& milestone : milestones)
    {
        const juce::String rowText = "bar " + juce::String(milestone.bar) + "  " + juce::String(milestone.label)
                                      + "  (" + juce::String((int) milestone.instances.size()) + " instance(s))";
        text << rowText << "\n";

        restoreCombo.addItem(rowText, itemId);
        if (rowText == previousSelection)
            selectId = itemId;
        ++itemId;
    }

    restoreCombo.setSelectedId(selectId, juce::dontSendNotification);
    listDisplay.setText(text, juce::dontSendNotification);
}

void MilestoneView::restoreClicked()
{
    const int selectedId = restoreCombo.getSelectedId();
    if (selectedId <= 0)
    {
        setStatus("Restore milestone skipped: no milestone selected");
        return;
    }

    const auto index = static_cast<size_t>(selectedId - 1);
    if (processorRef.getComposerCore().restoreMilestone(index))
    {
        setStatus("Restored milestone: " + restoreCombo.getText());
        processorRef.getComposerCore().fullRefresh();
    }
    else
    {
        setStatus("Restore milestone failed: not found");
    }
}
