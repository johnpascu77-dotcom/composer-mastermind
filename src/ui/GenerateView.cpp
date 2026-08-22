#include "GenerateView.h"
#include "../plugin/PluginProcessor.h"
#include "../model/Scene.h"

namespace
{
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 4;
    constexpr int kMargin = 10;

    void repopulate(juce::ComboBox& combo, const std::vector<std::string>& ids, bool autoSelectFirst)
    {
        const auto previousSelection = combo.getText();
        combo.clear(juce::dontSendNotification);

        int itemId = 1;
        int selectId = 0;
        for (const auto& id : ids)
        {
            combo.addItem(id, itemId);
            if (id == previousSelection.toStdString())
                selectId = itemId;
            ++itemId;
        }

        if (selectId == 0 && autoSelectFirst && !ids.empty())
            selectId = 1;

        combo.setSelectedId(selectId, juce::dontSendNotification);
    }

    const char* kArcDimensionNames[] = { "energy", "tension", "density", "complexity", "coherence" };
}

GenerateView::GenerateView(ComposerMastermindAudioProcessor& processor,
                            std::function<void(const juce::String&)> status)
    : processorRef(processor), setStatus(std::move(status)),
      previewGraph(candidateArcSet, "This is a draft - drag/add/delete like normal, nothing is saved until Commit.")
{
    addAndMakeVisible(builderHeaderLabel);
    builderHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(blueprintIdInput);
    blueprintIdInput.setTextToShowWhenEmpty("blueprint id/name, e.g. song_a", juce::Colours::grey);

    addAndMakeVisible(baseSceneCombo);
    baseSceneCombo.setTextWhenNothingSelected("(pick base scene)");

    addAndMakeVisible(drivingArcLabel);

    addAndMakeVisible(drivingArcCombo);
    drivingArcCombo.addItem("Energy", 1);
    drivingArcCombo.addItem("Tension", 2);
    drivingArcCombo.addItem("Density", 3);
    drivingArcCombo.addItem("Complexity", 4);
    drivingArcCombo.addItem("Coherence", 5);
    drivingArcCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(generateButton);
    generateButton.onClick = [this] { generateClicked(); };

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

    addAndMakeVisible(discardButton);
    discardButton.onClick = [this] { discardClicked(); };

    refreshSummaryDisplay();
    refreshAll();
}

void GenerateView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void GenerateView::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto nextRow = [&](int height) -> juce::Rectangle<int>
    {
        auto row = area.removeFromTop(height);
        area.removeFromTop(kRowGap);
        return row;
    };

    builderHeaderLabel.setBounds(nextRow(20));

    auto idRow = nextRow(kRowHeight);
    blueprintIdInput.setBounds(idRow.removeFromLeft(220));
    idRow.removeFromLeft(kMargin);
    baseSceneCombo.setBounds(idRow.removeFromLeft(180));

    auto arcRow = nextRow(kRowHeight);
    drivingArcLabel.setBounds(arcRow.removeFromLeft(80));
    drivingArcCombo.setBounds(arcRow.removeFromLeft(150));
    arcRow.removeFromLeft(kMargin);
    generateButton.setBounds(arcRow.removeFromLeft(120));

    area.removeFromTop(kMargin - kRowGap);
    summaryHeaderLabel.setBounds(nextRow(20));

    summaryDisplay.setBounds(area.removeFromTop(70));
    area.removeFromTop(kRowGap);

    area.removeFromTop(kMargin - kRowGap);
    previewHeaderLabel.setBounds(nextRow(20));

    auto bottomRow = area.removeFromBottom(kRowHeight);
    area.removeFromBottom(kRowGap);

    previewGraph.setBounds(area);

    commitButton.setBounds(bottomRow.removeFromLeft(120));
    bottomRow.removeFromLeft(kMargin);
    discardButton.setBounds(bottomRow.removeFromLeft(120));
}

void GenerateView::generateClicked()
{
    const auto blueprintId = blueprintIdInput.getText().trim().toStdString();
    if (blueprintId.empty())
    {
        setStatus("Generate failed: enter a blueprint id/name first");
        return;
    }

    if (baseSceneCombo.getSelectedId() <= 0)
    {
        setStatus("Generate failed: pick a base scene first");
        return;
    }

    const auto drivingArcName = kArcDimensionNames[juce::jlimit(0, 4, drivingArcCombo.getSelectedItemIndex())];

    auto& composerCore = processorRef.getComposerCore();

    Scene baseScene;
    if (!composerCore.getSceneLibrary().getSceneById(baseSceneCombo.getText().toStdString(), baseScene))
    {
        setStatus("Generate failed: base scene not found");
        return;
    }

    const auto instances = composerCore.getInstanceRegistry().getAllInstances();
    const auto rolePresets = composerCore.getPresetLibrary().getAllRolePresets();
    const auto rhythmicPresets = composerCore.getPresetLibrary().getAllRhythmicRelationshipPresets();

    currentProposal = BlueprintGenerator::generate(blueprintId, drivingArcName, composerCore.getArcSet(), baseScene,
                                                     instances, rolePresets, rhythmicPresets);

    if (currentProposal.blueprint.id.empty())
    {
        setStatus("Generate failed: the '" + juce::String(drivingArcName)
                      + "' arc needs at least 2 breakpoints to derive sections from - draw its shape on the Arcs "
                        "tab first");
        return;
    }

    candidateArcSet = currentProposal.candidateArcSet;
    previewGraph.refreshFromArcSet();
    refreshSummaryDisplay();

    setStatus("Generated '" + juce::String(blueprintId) + "' ("
                  + juce::String((int) currentProposal.blueprint.sections.size())
                  + " section(s)) - review below, then Commit or Discard");
}

void GenerateView::commitClicked()
{
    if (currentProposal.blueprint.id.empty())
    {
        setStatus("Commit skipped: nothing generated yet");
        return;
    }

    auto& composerCore = processorRef.getComposerCore();

    for (const auto& scene : currentProposal.newScenes)
        composerCore.getSceneLibrary().addOrReplaceScene(scene);

    // Persist the full generated 5-curve ArcSet onto the blueprint itself
    // (not just push it into the live evaluator) so this blueprint's real
    // arc survives being revisited later - e.g. reactivated from the
    // Sections tab's Load button, or opened in the curve-authoring view -
    // rather than only living in whatever the live ArcSet happened to hold
    // right after this Commit. setCurrentBlueprint below reads it back out
    // via the same BlueprintGenerator::resolveBlueprintArcSet path and
    // syncs the live ArcSet from it, so there's no separate manual push
    // needed here anymore.
    currentProposal.blueprint.arcCurves.clear();
    for (const auto& dimensionName : candidateArcSet.getArcNames())
    {
        // ArcSet::getArc returns an Arc by value - iterating straight over
        // ".getBreakpoints()" in a range-based for is a dangling reference
        // (the range binds to what getBreakpoints() returns, a reference
        // into a temporary Arc that's destroyed before the loop body runs
        // once, not to the temporary itself - range-based for's lifetime
        // extension doesn't chase through the intermediate method call).
        // Naming the Arc as a real local keeps it alive for the loop.
        const Arc arc = candidateArcSet.getArc(dimensionName);

        BlueprintArcCurve curve;
        curve.dimension = dimensionName;
        for (const auto& breakpoint : arc.getBreakpoints())
            curve.points.push_back({ breakpoint.bar, breakpoint.value });
        currentProposal.blueprint.arcCurves.push_back(curve);
    }

    composerCore.getBlueprintLibrary().addOrReplaceBlueprint(currentProposal.blueprint);
    composerCore.setCurrentBlueprint(currentProposal.blueprint);

    setStatus("Committed blueprint '" + juce::String(currentProposal.blueprint.id) + "' ("
                  + juce::String((int) currentProposal.blueprint.sections.size()) + " section(s), "
                  + juce::String((int) currentProposal.newScenes.size()) + " scene(s)) - now active");
}

void GenerateView::discardClicked()
{
    currentProposal = BlueprintGenerator::GeneratedBlueprintProposal{};
    candidateArcSet = ArcSet{};
    previewGraph.refreshFromArcSet();
    refreshSummaryDisplay();
    setStatus("Discarded proposal");
}

void GenerateView::refreshSummaryDisplay()
{
    juce::String text;

    if (currentProposal.blueprint.id.empty())
    {
        text = "(no proposal generated yet)";
    }
    else
    {
        for (const auto& section : currentProposal.blueprint.sections)
        {
            text << section.name << "  bars=" << section.startBar << "-" << (section.startBar + section.durationBars)
                 << "  scene=" << section.sceneId << "  layerRoles=" << (int) section.layerRoles.size()
                 << "  budgetOverrides=" << (int) section.budgetOverrides.size()
                 << "  reservedValues=" << (int) section.reservedValues.size() << "\n";
        }
    }

    summaryDisplay.setText(text, juce::dontSendNotification);
}

void GenerateView::refreshAll()
{
    std::vector<std::string> sceneIds;
    for (const auto& scene : processorRef.getComposerCore().getSceneLibrary().getAllScenes())
        sceneIds.push_back(scene.id);
    repopulate(baseSceneCombo, sceneIds, false);
}
