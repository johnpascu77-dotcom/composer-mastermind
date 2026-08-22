#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class ComposerMastermindAudioProcessor;

// Activity Log tab (2026-08-21, user's own request): a human-readable,
// bar-stamped log of the structural decisions ComposerCore actually made -
// section entries, Presentation baseline resets, motif stamps, phrase-chain
// seeds/advances, rhythm mutations (applied or blocked by policy) - real
// visibility into "what changed and when" without cross-referencing
// Awareness or just trusting the audio. Deliberately coarse: individual
// step/note edits stay in Awareness's territory, this is the higher-level
// narrative of what the engine decided. See ComposerCore::logActivity for
// exactly what gets recorded. Newest entries at the top.
class ActivityLogView : public juce::Component
{
public:
    explicit ActivityLogView(ComposerMastermindAudioProcessor& processor);

    void paint(juce::Graphics&) override;
    void resized() override;

    // Redraws from ComposerCore::getRecentActivityLog() - refreshed
    // periodically by EditorView's shared timer, same as every other tab.
    void refreshAll();

private:
    ComposerMastermindAudioProcessor& processorRef;

    juce::Label headerLabel { {}, "Newest first" };
    juce::TextEditor logDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ActivityLogView)
};
