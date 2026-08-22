#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class ComposerMastermindAudioProcessor;

// Milestones tab (Phase 3, 2026-08-22, see state/MilestoneLibrary.h): browse
// the session-local ring buffer of playback checkpoints and jump the whole
// rig back to one - a quick "what did this sound like a few sections ago"
// safety net while composing, distinct from the permanent Blueprint/Scene/
// Preset libraries. Deliberately minimal, mirroring ActivityLogView's own
// simplicity: a read-only list plus a combo + button to restore.
class MilestoneView : public juce::Component
{
public:
    MilestoneView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    // Redraws from ComposerCore::getMilestoneLibrary() - refreshed
    // periodically by EditorView's shared timer, same as every other tab.
    void refreshAll();

private:
    void restoreClicked();

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;

    juce::Label headerLabel { {}, "Newest first - session-local, not saved with the project" };
    juce::TextEditor listDisplay;
    juce::ComboBox restoreCombo;
    juce::TextButton restoreButton { "Restore" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MilestoneView)
};
