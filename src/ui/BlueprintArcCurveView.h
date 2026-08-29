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
// item 1 - the user's own framing was "the cockpit"). Two modes:
//
// - **Reshape an existing blueprint**: pick a saved blueprint, see each of
//   its 5 arc dimensions as an editable curve laid over its own section/
//   archetype sequence, save an edited curve as a new blueprint (same
//   sections/scenes, just a different arc). The original behaviour.
// - **New Blueprint** (2026-08-27, user's own request): start completely
//   blank, choose a length in bars, draw one curve, and Generate - this
//   calls the same policy/BlueprintGenerator::generate ui/GenerateView
//   already uses to turn a driving arc's shape into real sections (one Peak
//   at the curve's global maximum, Presentation/Build before it, Release
//   after), just triggered from this screen instead of that separate tab,
//   and committing in one motion rather than a separate preview/commit
//   step. Closes the gap between this being the intended primary curve-
//   authoring surface and the actual curve-to-sections engine living
//   somewhere else entirely.
//
// Reuses ui/ArcGraphView unchanged for the actual graph interaction (drag/
// add/delete/set axis length), the same way ui/GenerateView does for its
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

    // "New Blueprint" (2026-08-27, user's own request - start and draw a
    // whole new blueprint right here, choosing its length up front, rather
    // than only ever reshaping an existing one's curves). Resets to a blank
    // scratch ArcSet sized to barLengthSlider's current value and flips the
    // view into isNewBlueprintMode - see primaryActionClicked for what the
    // main button does differently while this is on.
    void newBlueprintClicked();

    // Dispatches to saveClicked() (reshape an existing blueprint, unchanged
    // behaviour) or generateClicked() (blank-slate mode) depending on
    // isNewBlueprintMode - one button, two jobs, same "Inv Off/On"-style
    // dynamic-label convention used elsewhere in this codebase rather than
    // two separate buttons competing for the same row.
    void primaryActionClicked();

    // Blank-slate path: runs BlueprintGenerator::generate on whichever
    // dimension curveGraph currently has selected (the "driving" arc),
    // auto-creating a minimal default scene (every registered instance,
    // Pattern 1/Binary/Swing Off) if the user hasn't picked one - the
    // user's own call when asked, over requiring an existing scene first.
    // Commits immediately (matching "draw here, then commit" as one motion,
    // not a separate preview/discard step like ui/GenerateView's) - the
    // curve just drawn already served as the preview.
    void generateClicked();

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;

    juce::Label headerLabel { {}, "Blueprint" };
    juce::ComboBox blueprintCombo;
    juce::TextButton removeButton { "Remove" };
    juce::TextButton newButton { "New Blueprint" };
    juce::Label barLengthLabel { {}, "Length (bars)" };
    juce::Slider barLengthSlider;
    juce::Label badgeLabel;

    bool isNewBlueprintMode = false;

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
