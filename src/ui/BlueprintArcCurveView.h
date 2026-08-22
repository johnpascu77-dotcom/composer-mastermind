#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <string>
#include <vector>
#include "ArcGraphView.h"
#include "../policy/ArcSet.h"
#include "../model/Blueprint.h"

class ComposerMastermindAudioProcessor;

// The "curve-based blueprint authoring" screen (docs/score_timeline_ui_concept.md's
// item 1 - the user's own framing was "the cockpit"): pick a saved
// blueprint, see each of its 5 arc dimensions as an editable curve laid
// over its own section/archetype sequence, and save an edited curve as a
// new blueprint. Reuses ui/ArcGraphView unchanged for the actual graph
// interaction (drag/add/delete), the same way ui/GenerateView does for its
// draft preview - only the surrounding picker/badge/save chrome is new
// here.
//
// Data model (see model/Blueprint.h's BlueprintArcCurve): a dimension the
// selected blueprint hasn't explicitly saved a curve for is shown "derived"
// - computed live from its section/archetype sequence via
// BlueprintGenerator::deriveArcSetFromSections, never written anywhere
// until touched. The moment the user edits it, that dimension is treated
// as "custom" for this session; Save writes every dimension currently held
// in the scratch ArcSet (derived and custom alike) onto a NEW blueprint -
// "basically a new musical piece" cloned from the selected one, per the
// user's own framing - never overwriting the original in place.
class BlueprintArcCurveView : public juce::Component
{
public:
    BlueprintArcCurveView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    // Repopulates the blueprint picker from BlueprintLibrary, preserving
    // the current selection by text (same pattern as every other tab's
    // periodically-refreshed combo) - only reloads the scratch ArcSet if
    // the selection actually changed, so this survives the shared 300ms
    // refresh timer without discarding an in-progress edit.
    void refreshAll();

private:
    void blueprintComboChanged();
    void loadSelectedBlueprint();
    void updateBadge();
    void saveClicked();
    void removeClicked();

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;

    juce::Label headerLabel { {}, "Blueprint" };
    juce::ComboBox blueprintCombo;
    juce::TextButton removeButton { "Remove" };
    juce::Label badgeLabel;

    // Declared before curveGraph deliberately - curveGraph binds a
    // reference to this in its constructor, so it must already exist.
    // Mutated in place via ArcSet::operator= when the selected blueprint
    // changes (never replaced as an object), same reason
    // ui/GenerateView's candidateArcSet works this way - curveGraph has no
    // other way to notice a swap besides refreshFromArcSet().
    ArcSet scratchArcSet;
    ArcGraphView curveGraph;

    juce::TextEditor saveNameInput;
    juce::TextButton saveButton { "Save as new blueprint" };
    juce::Label hintLabel;

    std::string loadedBlueprintId;
    Blueprint loadedBlueprint;
    std::vector<std::string> derivedDimensions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlueprintArcCurveView)
};
