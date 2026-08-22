#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <string>
#include "PianoRollView.h"

class ComposerMastermindAudioProcessor;

// v1.2 Phase 2, Track A (docs/score_timeline_ui_concept.md): Setup mode -
// authoring one instance's one pattern (P1/P2/P3) directly, replacing the
// need to open MPL's own floating editor window just to seed content.
// Always single-lane (an instance + pattern picker, not all 3 at once - see
// PianoRollView::canEditNow, which only allows editing with exactly one
// lane loaded). Edits stay local until Commit writes them through
// PatternSyncServer::sendWriteFullPattern (the same IPC MotifEngine's own
// stamping already uses, v0.6) - Discard reloads from the cache instead of
// keeping unsaved local edits around silently.
class PatternSetupView : public juce::Component
{
public:
    explicit PatternSetupView(ComposerMastermindAudioProcessor& processor);

    void resized() override;

    // Called whenever this becomes visible again (PrimaryView's Live/Setup
    // toggle) so the instance list reflects anything registered/removed
    // while Setup mode was hidden.
    void refreshInstanceList();

private:
    void selectionChanged();
    void loadCurrentSelection();
    void commitClicked();
    void discardClicked();
    void setDirty(bool isDirty);
    void lockToggleRequested(int stepIndex);

    ComposerMastermindAudioProcessor& processorRef;

    juce::ComboBox instanceCombo;
    juce::ComboBox patternCombo;
    juce::TextButton commitButton { "Commit to MPL" };
    juce::TextButton discardButton { "Discard Changes" };
    juce::Label statusLabel;

    PianoRollView pianoRoll;

    bool dirty = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternSetupView)
};
