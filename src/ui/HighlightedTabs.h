#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// A TabbedComponent whose selected tab cannot be missed. The default look gives
// every tab the same (transparent) background, so with the Expert editor's ten
// tabs it was hard to tell which page was showing. Here the selected tab is a
// filled accent colour with bright text and every other tab is dark with dim
// text; the colours follow the selection, however it changes.
class HighlightedTabs : public juce::TabbedComponent
{
public:
    explicit HighlightedTabs(juce::TabbedButtonBar::Orientation orientation)
        : juce::TabbedComponent(orientation)
    {
    }

    // Hover help for each tab, in tab order (call after the tabs have been added).
    void setTabTooltips(const juce::StringArray& tips)
    {
        for (int i = 0; i < getNumTabs() && i < tips.size(); ++i)
            if (auto* button = getTabbedButtonBar().getTabButton(i))
                button->setTooltip(tips[i]);
    }

    // Call once after the tabs have been added.
    void applyHighlight()
    {
        auto& bar = getTabbedButtonBar();
        bar.setColour(juce::TabbedButtonBar::tabTextColourId, dimText);
        bar.setColour(juce::TabbedButtonBar::frontTextColourId, brightText);
        bar.setColour(juce::TabbedButtonBar::tabOutlineColourId, outline);
        bar.setColour(juce::TabbedButtonBar::frontOutlineColourId, accent.brighter(0.4f));

        for (int i = 0; i < getNumTabs(); ++i)
            setTabBackgroundColour(i, i == getCurrentTabIndex() ? accent : idle);
        bar.repaint();
    }

    // TabbedComponent fills the whole content area with the selected tab's
    // colour. Only the tab itself should be coloured, so keep the pages on the
    // normal window background.
    void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    }

private:
    void currentTabChanged(int, const juce::String&) override { applyHighlight(); }

    const juce::Colour accent { 0xff2f7fd0 };
    const juce::Colour idle { 0xff262d33 };
    const juce::Colour brightText { 0xffffffff };
    const juce::Colour dimText { 0xffa7b0b8 };
    const juce::Colour outline { 0xff3a444c };
};
