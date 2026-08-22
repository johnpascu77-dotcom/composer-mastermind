#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "ArcGraphView.h"
#include "../policy/ArcSet.h"
#include "../policy/BlueprintGenerator.h"

class ComposerMastermindAudioProcessor;

// Generate tab: the v1.1 "Generative Blueprint Proposal" front end (see
// docs/composer_mastermind_design.md's v1.1 section and
// policy/BlueprintGenerator, which does the actual work). Pick a base
// scene and a driving arc, click Generate - the proposal (a full Blueprint
// + its per-section scenes + a re-baked 5-curve ArcSet) is held entirely
// in memory and previewed here, never touching live state until Commit.
// The preview reuses ui/ArcGraphView unchanged, just pointed at this
// component's own candidateArcSet member instead of the live one - the
// same drag/add/delete interactions work on a draft exactly like they do
// on the real thing, since "always inspectable and editable before
// playback" is the whole point.
class GenerateView : public juce::Component
{
public:
    GenerateView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    // Repopulates the base-scene combo.
    void refreshAll();

private:
    void generateClicked();
    void commitClicked();
    void discardClicked();
    void refreshSummaryDisplay();

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;

    juce::Label builderHeaderLabel { {}, "Generate Blueprint Proposal" };
    juce::TextEditor blueprintIdInput;
    juce::ComboBox baseSceneCombo;
    juce::Label drivingArcLabel { {}, "Driving Arc" };
    juce::ComboBox drivingArcCombo;
    juce::TextButton generateButton { "Generate" };

    juce::Label summaryHeaderLabel { {}, "Proposal Summary" };
    juce::TextEditor summaryDisplay;

    juce::Label previewHeaderLabel { {}, "Preview (not committed)" };

    // Declared before previewGraph deliberately - previewGraph binds a
    // reference to this in its constructor, so it must already exist.
    // Replaced wholesale (not edited field-by-field) each time Generate
    // runs; previewGraph.refreshFromArcSet() is called afterward since it
    // otherwise has no way to notice the swap.
    ArcSet candidateArcSet;
    ArcGraphView previewGraph;

    juce::TextButton commitButton { "Commit" };
    juce::TextButton discardButton { "Discard" };

    BlueprintGenerator::GeneratedBlueprintProposal currentProposal;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenerateView)
};
