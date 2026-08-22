#include "PluginEditor.h"

ComposerMastermindAudioProcessorEditor::ComposerMastermindAudioProcessorEditor(ComposerMastermindAudioProcessor& p)
    : AudioProcessorEditor(&p), view(p)
{
    addAndMakeVisible(view);

    // Resizable (2026-08-22, user's own request once the Phase 1 piano roll
    // made the fixed 640x560 size feel cramped) - every view here already
    // lays out from getLocalBounds() (Viewport-hosted tabs included), so
    // growing/shrinking needs no further changes. Minimum matches the old
    // fixed size, so nothing that fit before stops fitting.
    setResizable(true, true);
    setResizeLimits(640, 560, 2000, 1400);
    setSize(900, 650);
}

ComposerMastermindAudioProcessorEditor::~ComposerMastermindAudioProcessorEditor()
{
}

void ComposerMastermindAudioProcessorEditor::resized()
{
    view.setBounds(getLocalBounds());
}

void ComposerMastermindAudioProcessorEditor::refreshAll()
{
    view.refreshAll();
}
