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

    // Called whenever this becomes visible again - both from PrimaryView's
    // own Live/Setup toggle AND from PrimaryView::visibilityChanged() (the
    // outer Expert<->Score-View shell switch, added 2026-08-23 to close a
    // real staleness gap the first path alone didn't cover) - so the
    // instance list reflects anything registered/removed while Setup mode
    // was hidden either way.
    void refreshInstanceList();

private:
    void selectionChanged();
    void loadCurrentSelection();
    void gridModeChanged();
    void commitClicked();
    void discardClicked();
    void setDirty(bool isDirty);
    void lockToggleRequested(int stepIndex);

    ComposerMastermindAudioProcessor& processorRef;

    juce::ComboBox instanceCombo;
    juce::ComboBox patternCombo;

    // The grid this pattern is being AUTHORED against (2026-08-23, user's own
    // design: "the binary or ternary decision of what is to be committed
    // happens at this point" - Setup mode chooses the grid, MPL obeys it on
    // Commit, rather than the piano roll only ever mirroring whatever MPL
    // already happens to be in). Initialized from the instance's currently
    // tracked grid mode when a selection loads (a sensible starting point,
    // not a forced one) but fully user-editable from here on - changing it
    // re-renders the grid immediately (12 vs 16 columns) without discarding
    // any notes already drawn, and Commit pushes it to MPL as a real CC
    // alongside the step content, so what plays back matches what was drawn.
    juce::ComboBox gridModeCombo;
    juce::Label gridModeLabel { {}, "Grid" };

    juce::TextButton commitButton { "Commit to MPL" };
    juce::TextButton discardButton { "Discard Changes" };
    juce::Label statusLabel;

    PianoRollView pianoRoll;

    bool dirty = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternSetupView)
};
