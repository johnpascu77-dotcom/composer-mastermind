#pragma once

#include "PluginProcessor.h"
#include "../ui/MainShellView.h"

// Thin JUCE-required AudioProcessorEditor wrapper. All actual content lives
// in MainShellView (v1.2 Phase 0: the new PrimaryView by default, with an
// Expert button revealing the existing, unchanged EditorView tabbed shell) -
// this class just owns and sizes it, and forwards the state-restore refresh
// callback from PluginProcessor.
//
// juce::DragAndDropContainer (v1.2 Phase 2, drag-out MIDI export): the
// standard JUCE place for this - ui/PianoRollView calls
// DragAndDropContainer::findParentDragContainerFor(this) to find this
// ancestor and start an OS-level file drag once its own deliberate-drag
// gesture fires.
class ComposerMastermindAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                public juce::DragAndDropContainer
{
public:
    explicit ComposerMastermindAudioProcessorEditor(ComposerMastermindAudioProcessor&);
    ~ComposerMastermindAudioProcessorEditor() override;

    void resized() override;

    // Called by PluginProcessor after the DAW restores plugin state.
    void refreshAll();

private:
    MainShellView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ComposerMastermindAudioProcessorEditor)
};
