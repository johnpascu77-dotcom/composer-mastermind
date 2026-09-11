#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "ArcGraphView.h"
#include "../policy/ArcSet.h"
#include "../policy/BlueprintGenerator.h"
#include "../policy/ArcShapeLibrary.h"
#include "../policy/DurationCalculator.h"
#include "../state/ProposalLibrary.h"

class ComposerMastermindAudioProcessor;

// The "Propose a Piece" front end (v2 scope, docs/composer_mastermind_design.md) -
// a 4th MainShellView peer, deliberately NOT a Generate-tab replacement and
// NOT an EditorView tab: a clean playground for MC's own generative
// machinery, matching the user's own framing ("a simple UI for a highly
// intelligent device"). Given a target length/tempo/shape/variety, this
// runs the whole pipeline from intent to a previewable blueprint:
//   DurationCalculator::compute (minutes -> bars + tempo map)
//   -> ArcShapeLibrary::generateBreakpoints (named shape -> one driving arc curve)
//   -> BlueprintGenerator::generateSeedScene (currently-registered instances -> a starting Scene)
//   -> BlueprintGenerator::generate (existing v1.1 machinery - classify sections, dress scenes,
//                                     derive the other 4 arc dimensions, suggest an OrchConductor lane)
// Held entirely in memory and previewed (reusing ui/ArcGraphView exactly as
// GenerateView does) until the user commits and/or saves it - nothing is
// touched live until then. "Save to Proposal Library" and "Commit" are
// independent actions, either or both: a proposal can be filed away without
// making it the active blueprint, or made active without keeping a copy.
class ProposeView : public juce::Component
{
public:
    ProposeView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    // Repopulates the saved-proposals combo.
    void refreshAll();

private:
    void proposeClicked();
    void commitClicked();
    void saveToLibraryClicked();
    void loadFromLibraryClicked();
    void deleteFromLibraryClicked();
    void refreshSummaryDisplay();
    void refreshProposalCombo();

    // Shared by commitClicked/saveToLibraryClicked: adds the generated
    // scenes to SceneLibrary, bakes candidateArcSet into the blueprint's
    // own arcCurves, and adds the blueprint to BlueprintLibrary - everything
    // CompositionBundleStore::createBundle needs to find it, short of
    // making it the live/active blueprint (that's commitClicked's own
    // extra step). Mirrors GenerateView::commitClicked's own first half.
    void populateLibraries();

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;

    juce::Label headerLabel { {}, "Propose a Piece" };
    juce::TextEditor blueprintIdInput;

    juce::Label lengthLabel { {}, "Length (min)" };
    juce::TextEditor targetMinutesInput;
    juce::Label beatsPerBarLabel { {}, "Beats/Bar" };
    juce::TextEditor beatsPerBarInput;
    juce::Label sectionCountLabel { {}, "Sections" };
    juce::TextEditor sectionCountInput;

    juce::Label tempoLabel { {}, "Tempo (start / end BPM)" };
    juce::TextEditor startBpmInput;
    juce::TextEditor endBpmInput;

    juce::Label shapeLabel { {}, "Shape" };
    juce::ComboBox shapeCombo;
    juce::Label restlessnessLabel { {}, "Restlessness" };
    juce::Slider restlessnessSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label seedLabel { {}, "Seed" };
    juce::TextEditor seedInput;
    juce::TextButton rerollSeedButton { "Reroll" };

    juce::TextButton proposeButton { "Propose" };

    juce::Label summaryHeaderLabel { {}, "Proposal Summary (not committed)" };
    juce::TextEditor summaryDisplay;

    juce::Label previewHeaderLabel { {}, "Preview" };
    ArcSet candidateArcSet;
    ArcGraphView previewGraph;

    juce::TextButton commitButton { "Commit" };
    juce::TextButton saveToLibraryButton { "Save to Proposal Library" };

    juce::Label libraryHeaderLabel { {}, "Proposal Library" };
    juce::ComboBox savedProposalsCombo;
    juce::TextButton loadFromLibraryButton { "Load" };
    juce::TextButton deleteFromLibraryButton { "Delete" };

    BlueprintGenerator::GeneratedBlueprintProposal currentProposal;
    DurationCalculator::DurationPlan currentDurationPlan;

    ProposalLibrary proposalLibrary;
    std::vector<ProposalLibrary::ProposalSummary> savedProposals;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProposeView)
};
