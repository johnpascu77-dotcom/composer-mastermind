#include "ProposeView.h"
#include "../plugin/PluginProcessor.h"
#include "../state/CompositionBundleStore.h"
#include <cmath>

namespace
{
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 4;
    constexpr int kMargin = 10;

    juce::String tempoPlanToText(const DurationCalculator::DurationPlan& plan)
    {
        juce::String text;
        text << "Total bars: " << plan.totalBars << "   Tempo map (hand-transcribe into Bitwig's Tempo lane):\n";
        for (const auto& point : plan.tempoMap)
            text << "  at " << juce::String(point.atMinute, 2) << " min -> " << juce::String(point.bpm, 1) << " bpm\n";
        return text;
    }
}

ProposeView::ProposeView(ComposerMastermindAudioProcessor& processor,
                          std::function<void(const juce::String&)> status)
    : processorRef(processor), setStatus(std::move(status)),
      previewGraph(candidateArcSet, "This is a draft - nothing is saved until Commit or Save to Proposal Library.")
{
    addAndMakeVisible(headerLabel);
    headerLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(blueprintIdInput);
    blueprintIdInput.setTextToShowWhenEmpty("blueprint id/name, e.g. tone_poem_a", juce::Colours::grey);

    addAndMakeVisible(lengthLabel);
    addAndMakeVisible(targetMinutesInput);
    targetMinutesInput.setText("20", juce::dontSendNotification);
    targetMinutesInput.setInputRestrictions(0, "0123456789.");

    addAndMakeVisible(beatsPerBarLabel);
    addAndMakeVisible(beatsPerBarInput);
    beatsPerBarInput.setText("4", juce::dontSendNotification);
    beatsPerBarInput.setInputRestrictions(0, "0123456789");

    addAndMakeVisible(sectionCountLabel);
    addAndMakeVisible(sectionCountInput);
    sectionCountInput.setText("6", juce::dontSendNotification);
    sectionCountInput.setInputRestrictions(0, "0123456789");

    addAndMakeVisible(tempoLabel);
    addAndMakeVisible(startBpmInput);
    startBpmInput.setText("100", juce::dontSendNotification);
    startBpmInput.setInputRestrictions(0, "0123456789.");
    addAndMakeVisible(endBpmInput);
    endBpmInput.setText("100", juce::dontSendNotification);
    endBpmInput.setInputRestrictions(0, "0123456789.");

    addAndMakeVisible(shapeLabel);
    addAndMakeVisible(shapeCombo);
    {
        int itemId = 1;
        for (const auto& name : ArcShapeLibrary::shapeNames())
            shapeCombo.addItem(name, itemId++);
        shapeCombo.setSelectedId(1, juce::dontSendNotification);
    }

    addAndMakeVisible(restlessnessLabel);
    addAndMakeVisible(restlessnessSlider);
    restlessnessSlider.setRange(0.0, 1.0);
    restlessnessSlider.setValue(0.3, juce::dontSendNotification);

    addAndMakeVisible(seedLabel);
    addAndMakeVisible(seedInput);
    seedInput.setText("1", juce::dontSendNotification);
    seedInput.setInputRestrictions(0, "0123456789");

    addAndMakeVisible(rerollSeedButton);
    rerollSeedButton.onClick = [this]
    {
        seedInput.setText(juce::String((int) (juce::Time::currentTimeMillis() % 1000000)), juce::dontSendNotification);
    };

    addAndMakeVisible(proposeButton);
    proposeButton.onClick = [this] { proposeClicked(); };

    addAndMakeVisible(summaryHeaderLabel);
    summaryHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(summaryDisplay);
    summaryDisplay.setMultiLine(true);
    summaryDisplay.setReadOnly(true);
    summaryDisplay.setScrollbarsShown(true);
    summaryDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));

    addAndMakeVisible(previewHeaderLabel);
    previewHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(previewGraph);

    addAndMakeVisible(commitButton);
    commitButton.onClick = [this] { commitClicked(); };

    addAndMakeVisible(saveToLibraryButton);
    saveToLibraryButton.onClick = [this] { saveToLibraryClicked(); };

    addAndMakeVisible(libraryHeaderLabel);
    libraryHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(savedProposalsCombo);
    savedProposalsCombo.setTextWhenNothingSelected("(no saved proposals)");

    addAndMakeVisible(loadFromLibraryButton);
    loadFromLibraryButton.onClick = [this] { loadFromLibraryClicked(); };

    addAndMakeVisible(deleteFromLibraryButton);
    deleteFromLibraryButton.onClick = [this] { deleteFromLibraryClicked(); };

    refreshSummaryDisplay();
    refreshAll();
}

void ProposeView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void ProposeView::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto nextRow = [&](int height) -> juce::Rectangle<int>
    {
        auto row = area.removeFromTop(height);
        area.removeFromTop(kRowGap);
        return row;
    };

    headerLabel.setBounds(nextRow(20));
    blueprintIdInput.setBounds(nextRow(kRowHeight).removeFromLeft(300));

    auto lengthRow = nextRow(kRowHeight);
    lengthLabel.setBounds(lengthRow.removeFromLeft(80));
    targetMinutesInput.setBounds(lengthRow.removeFromLeft(60));
    lengthRow.removeFromLeft(kMargin);
    beatsPerBarLabel.setBounds(lengthRow.removeFromLeft(70));
    beatsPerBarInput.setBounds(lengthRow.removeFromLeft(40));
    lengthRow.removeFromLeft(kMargin);
    sectionCountLabel.setBounds(lengthRow.removeFromLeft(60));
    sectionCountInput.setBounds(lengthRow.removeFromLeft(40));

    auto tempoRow = nextRow(kRowHeight);
    tempoLabel.setBounds(tempoRow.removeFromLeft(160));
    startBpmInput.setBounds(tempoRow.removeFromLeft(60));
    tempoRow.removeFromLeft(kMargin);
    endBpmInput.setBounds(tempoRow.removeFromLeft(60));

    auto shapeRow = nextRow(kRowHeight);
    shapeLabel.setBounds(shapeRow.removeFromLeft(50));
    shapeCombo.setBounds(shapeRow.removeFromLeft(180));
    shapeRow.removeFromLeft(kMargin);
    restlessnessLabel.setBounds(shapeRow.removeFromLeft(90));
    restlessnessSlider.setBounds(shapeRow.removeFromLeft(160));

    auto seedRow = nextRow(kRowHeight);
    seedLabel.setBounds(seedRow.removeFromLeft(40));
    seedInput.setBounds(seedRow.removeFromLeft(80));
    seedRow.removeFromLeft(kMargin);
    rerollSeedButton.setBounds(seedRow.removeFromLeft(80));
    seedRow.removeFromLeft(kMargin);
    proposeButton.setBounds(seedRow.removeFromLeft(120));

    area.removeFromTop(kMargin - kRowGap);
    summaryHeaderLabel.setBounds(nextRow(20));
    summaryDisplay.setBounds(area.removeFromTop(90));
    area.removeFromTop(kRowGap);

    area.removeFromTop(kMargin - kRowGap);

    auto libraryRow = area.removeFromBottom(kRowHeight);
    area.removeFromBottom(kRowGap);
    deleteFromLibraryButton.setBounds(libraryRow.removeFromRight(80));
    libraryRow.removeFromRight(kMargin);
    loadFromLibraryButton.setBounds(libraryRow.removeFromRight(80));
    libraryRow.removeFromRight(kMargin);
    savedProposalsCombo.setBounds(libraryRow.removeFromRight(260));
    libraryHeaderLabel.setBounds(libraryRow);

    area.removeFromBottom(kRowGap);
    auto bottomRow = area.removeFromBottom(kRowHeight);
    area.removeFromBottom(kRowGap);
    commitButton.setBounds(bottomRow.removeFromLeft(120));
    bottomRow.removeFromLeft(kMargin);
    saveToLibraryButton.setBounds(bottomRow.removeFromLeft(180));

    previewHeaderLabel.setBounds(nextRow(20));
    previewGraph.setBounds(area);
}

void ProposeView::proposeClicked()
{
    const auto blueprintId = blueprintIdInput.getText().trim().toStdString();
    if (blueprintId.empty())
    {
        setStatus("Propose failed: enter a blueprint id/name first");
        return;
    }

    auto& composerCore = processorRef.getComposerCore();

    const auto instances = composerCore.getInstanceRegistry().getAllInstances();
    if (instances.empty())
    {
        setStatus("Propose failed: register at least one instance first (Instances tab)");
        return;
    }

    const double targetMinutes = targetMinutesInput.getText().getDoubleValue();
    const int beatsPerBar = beatsPerBarInput.getText().getIntValue();
    const int desiredSectionCount = sectionCountInput.getText().getIntValue();
    const double startBpm = startBpmInput.getText().getDoubleValue();
    const double endBpm = endBpmInput.getText().getDoubleValue();
    const int seed = seedInput.getText().getIntValue();
    const auto restlessness = (float) restlessnessSlider.getValue();

    std::vector<DurationCalculator::TempoPoint> tempoSpec { { 0.0, startBpm } };
    if (std::abs(startBpm - endBpm) > 0.0001 && targetMinutes > 0.0)
        tempoSpec.push_back({ targetMinutes, endBpm });

    currentDurationPlan = DurationCalculator::compute(targetMinutes, beatsPerBar, tempoSpec, desiredSectionCount);

    const auto shape = static_cast<ArcShapeLibrary::ShapeName>(
        juce::jlimit(0, (int) ArcShapeLibrary::shapeNames().size() - 1, shapeCombo.getSelectedItemIndex()));
    const auto breakpoints = ArcShapeLibrary::generateBreakpoints(
        shape, currentDurationPlan.suggestedSectionBoundaries, restlessness, seed);

    if (breakpoints.size() < 2)
    {
        setStatus("Propose failed: need at least 2 sections - raise the Sections count or the target length");
        return;
    }

    ArcSet workingArcSet = composerCore.getArcSet();
    Arc drivingArc;
    drivingArc.setBreakpoints(breakpoints);
    workingArcSet.setArc("energy", drivingArc);

    const auto rolePresets = composerCore.getPresetLibrary().getAllRolePresets();
    const auto rhythmicPresets = composerCore.getPresetLibrary().getAllRhythmicRelationshipPresets();

    const auto seedScene = BlueprintGenerator::generateSeedScene(blueprintId + "_seed", instances, rolePresets);

    currentProposal = BlueprintGenerator::generate(blueprintId, "energy", workingArcSet, seedScene, instances,
                                                     rolePresets, rhythmicPresets);

    if (currentProposal.blueprint.id.empty())
    {
        setStatus("Propose failed: could not derive sections from the generated curve");
        return;
    }

    candidateArcSet = currentProposal.candidateArcSet;
    previewGraph.refreshFromArcSet();
    refreshSummaryDisplay();

    setStatus("Proposed '" + juce::String(blueprintId) + "' (" + juce::String((int) currentProposal.blueprint.sections.size())
                  + " section(s), " + juce::String(currentDurationPlan.totalBars)
                  + " bars) - review below, then Commit and/or Save to Proposal Library");
}

void ProposeView::populateLibraries()
{
    auto& composerCore = processorRef.getComposerCore();

    for (const auto& scene : currentProposal.newScenes)
        composerCore.getSceneLibrary().addOrReplaceScene(scene);

    // Same dangling-reference care as GenerateView::commitClicked - name
    // the Arc as a real local before iterating its breakpoints.
    currentProposal.blueprint.arcCurves.clear();
    for (const auto& dimensionName : candidateArcSet.getArcNames())
    {
        const Arc arc = candidateArcSet.getArc(dimensionName);

        BlueprintArcCurve curve;
        curve.dimension = dimensionName;
        for (const auto& breakpoint : arc.getBreakpoints())
            curve.points.push_back({ breakpoint.bar, breakpoint.value });
        currentProposal.blueprint.arcCurves.push_back(curve);
    }

    composerCore.getBlueprintLibrary().addOrReplaceBlueprint(currentProposal.blueprint);
}

void ProposeView::commitClicked()
{
    if (currentProposal.blueprint.id.empty())
    {
        setStatus("Commit skipped: nothing proposed yet");
        return;
    }

    populateLibraries();
    processorRef.getComposerCore().setCurrentBlueprint(currentProposal.blueprint);

    setStatus("Committed proposal '" + juce::String(currentProposal.blueprint.id) + "' - now active");
}

void ProposeView::saveToLibraryClicked()
{
    if (currentProposal.blueprint.id.empty())
    {
        setStatus("Save skipped: nothing proposed yet");
        return;
    }

    populateLibraries();

    std::string errorMessage;
    const auto bundleJson = CompositionBundleStore::createBundle(processorRef.getComposerCore(),
                                                                    currentProposal.blueprint.id, errorMessage);

    if (bundleJson.isEmpty())
    {
        setStatus("Save to Proposal Library failed: " + juce::String(errorMessage));
        return;
    }

    if (!proposalLibrary.saveProposal(currentProposal.blueprint.id, bundleJson))
    {
        setStatus("Save to Proposal Library failed: could not write file");
        return;
    }

    refreshProposalCombo();
    setStatus("Saved '" + juce::String(currentProposal.blueprint.id) + "' to the Proposal Library");
}

void ProposeView::loadFromLibraryClicked()
{
    const auto index = savedProposalsCombo.getSelectedItemIndex();
    if (index < 0 || index >= (int) savedProposals.size())
    {
        setStatus("Load failed: pick a saved proposal first");
        return;
    }

    juce::String bundleJson;
    if (!proposalLibrary.loadProposal(savedProposals[(size_t) index].id, bundleJson))
    {
        setStatus("Load failed: could not read the saved proposal file");
        return;
    }

    std::string errorMessage;
    auto& composerCore = processorRef.getComposerCore();
    if (!CompositionBundleStore::importBundle(bundleJson, composerCore, errorMessage))
    {
        setStatus("Load failed: " + juce::String(errorMessage));
        return;
    }

    composerCore.primeForPlayback();
    setStatus("Loaded and primed proposal '" + juce::String(savedProposals[(size_t) index].name) + "'");
}

void ProposeView::deleteFromLibraryClicked()
{
    const auto index = savedProposalsCombo.getSelectedItemIndex();
    if (index < 0 || index >= (int) savedProposals.size())
    {
        setStatus("Delete failed: pick a saved proposal first");
        return;
    }

    const auto name = savedProposals[(size_t) index].name;
    if (!proposalLibrary.deleteProposal(savedProposals[(size_t) index].id))
    {
        setStatus("Delete failed: could not remove the saved proposal file");
        return;
    }

    refreshProposalCombo();
    setStatus("Deleted '" + juce::String(name) + "' from the Proposal Library");
}

void ProposeView::refreshSummaryDisplay()
{
    juce::String text;

    if (currentProposal.blueprint.id.empty())
    {
        text = "(no proposal generated yet)";
    }
    else
    {
        text << tempoPlanToText(currentDurationPlan) << "\n";

        for (const auto& section : currentProposal.blueprint.sections)
        {
            text << section.name << "  bars=" << section.startBar << "-" << (section.startBar + section.durationBars)
                 << "  archetype=" << section.archetype
                 << "  suggestedNarrativeLane=" << section.suggestedNarrativeLane << "\n";
        }
    }

    summaryDisplay.setText(text, juce::dontSendNotification);
}

void ProposeView::refreshProposalCombo()
{
    savedProposals = proposalLibrary.listProposals();

    savedProposalsCombo.clear(juce::dontSendNotification);
    int itemId = 1;
    for (const auto& proposal : savedProposals)
        savedProposalsCombo.addItem(juce::String(proposal.name) + "  (" + proposal.createdAt.toString(true, true) + ")",
                                      itemId++);
}

void ProposeView::refreshAll()
{
    refreshProposalCombo();
}
