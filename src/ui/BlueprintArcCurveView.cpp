#include "BlueprintArcCurveView.h"
#include "../plugin/PluginProcessor.h"
#include "../policy/BlueprintGenerator.h"
#include "../util/Validation.h"
#include <algorithm>

namespace
{
    constexpr int kMargin = 10;
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 6;

    void repopulate(juce::ComboBox& combo, const std::vector<std::string>& ids)
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

        if (selectId == 0 && !ids.empty())
            selectId = 1;

        combo.setSelectedId(selectId, juce::dontSendNotification);
    }

    // A real piece isn't always 64 bars (ArcGraphView's old fixed axis) -
    // size the graph to this specific blueprint's own actual span: the
    // furthest a section reaches, or the furthest any already-saved arc
    // point reaches if that's somehow past the sections (shouldn't happen
    // in practice, since the graph itself won't let a drag go past whatever
    // range it was shown - but a cloned/hand-authored blueprint could still
    // carry one). Falls back to ArcGraphView's own 64-bar default only for
    // a genuinely empty/new blueprint with no sections and no saved curves
    // yet, since there's nothing real to size against.
    int naturalMaxBar(const Blueprint& blueprint)
    {
        int result = 0;

        for (const auto& section : blueprint.sections)
            result = std::max(result, section.startBar + section.durationBars);

        for (const auto& curve : blueprint.arcCurves)
            for (const auto& point : curve.points)
                result = std::max(result, point.bar);

        constexpr int kFallbackMaxBar = 64;
        return result > 0 ? result : kFallbackMaxBar;
    }
}

BlueprintArcCurveView::BlueprintArcCurveView(ComposerMastermindAudioProcessor& processor,
                                              std::function<void(const juce::String&)> status)
    : processorRef(processor), setStatus(std::move(status)),
      curveGraph(scratchArcSet, "Drag/add/delete like normal - editing turns a derived curve into a saved one.")
{
    addAndMakeVisible(headerLabel);
    headerLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(blueprintCombo);
    blueprintCombo.setTextWhenNothingSelected("(no blueprints saved yet)");
    blueprintCombo.onChange = [this] { blueprintComboChanged(); };

    addAndMakeVisible(removeButton);
    removeButton.onClick = [this] { removeClicked(); };

    addAndMakeVisible(newButton);
    newButton.onClick = [this] { newBlueprintClicked(); };

    addAndMakeVisible(barLengthLabel);
    barLengthSlider.setSliderStyle(juce::Slider::IncDecButtons);
    barLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
    barLengthSlider.setRange(4.0, 256.0, 1.0);
    barLengthSlider.setValue(32.0, juce::dontSendNotification);
    barLengthSlider.onValueChange = [this]
    {
        if (isNewBlueprintMode)
            curveGraph.setMaxBar((int) barLengthSlider.getValue());
    };
    addAndMakeVisible(barLengthSlider);
    barLengthLabel.setVisible(false);
    barLengthSlider.setVisible(false);

    addAndMakeVisible(badgeLabel);
    badgeLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    badgeLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(curveGraph);
    curveGraph.onSelectionChanged = [this] { updateBadge(); };
    curveGraph.onEdited = [this]
    {
        // An edit turns the currently-viewed dimension from derived into
        // custom right now, in this session's working state - don't wait
        // for the next reload to notice. Without this, the badge stayed
        // "Derived from sections" through an entire edit, which looked
        // like the edit hadn't registered even when it had.
        auto& dims = derivedDimensions;
        dims.erase(std::remove(dims.begin(), dims.end(), curveGraph.getSelectedArcName()), dims.end());
        updateBadge();
    };

    addAndMakeVisible(saveNameInput);
    saveNameInput.setTextToShowWhenEmpty("name this version, e.g. song_a_v2", juce::Colours::grey);

    addAndMakeVisible(saveButton);
    saveButton.onClick = [this] { primaryActionClicked(); };

    addAndMakeVisible(hintLabel);
    hintLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    hintLabel.setJustificationType(juce::Justification::topLeft);

    refreshAll();
}

void BlueprintArcCurveView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void BlueprintArcCurveView::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto nextRow = [&](int height) -> juce::Rectangle<int>
    {
        auto row = area.removeFromTop(height);
        area.removeFromTop(kRowGap);
        return row;
    };

    auto pickerRow = nextRow(kRowHeight);
    headerLabel.setBounds(pickerRow.removeFromLeft(80));
    blueprintCombo.setBounds(pickerRow.removeFromLeft(190));
    pickerRow.removeFromLeft(kMargin / 2);
    removeButton.setBounds(pickerRow.removeFromLeft(65));
    pickerRow.removeFromLeft(kMargin);
    newButton.setBounds(pickerRow.removeFromLeft(105));
    pickerRow.removeFromLeft(kMargin);
    barLengthLabel.setBounds(pickerRow.removeFromLeft(85));
    barLengthSlider.setBounds(pickerRow.removeFromLeft(100));
    pickerRow.removeFromLeft(kMargin);
    badgeLabel.setBounds(pickerRow.removeFromLeft(140));

    auto bottomRow = area.removeFromBottom(kRowHeight);
    area.removeFromBottom(kRowGap);
    hintLabel.setBounds(area.removeFromBottom(18));
    area.removeFromBottom(kRowGap);

    curveGraph.setBounds(area);

    saveNameInput.setBounds(bottomRow.removeFromLeft(240));
    bottomRow.removeFromLeft(kMargin);
    saveButton.setBounds(bottomRow.removeFromLeft(170));
}

void BlueprintArcCurveView::refreshAll()
{
    std::vector<std::string> blueprintIds;
    for (const auto& blueprint : processorRef.getComposerCore().getBlueprintLibrary().getAllBlueprints())
        blueprintIds.push_back(blueprint.id);

    repopulate(blueprintCombo, blueprintIds);
    blueprintComboChanged();
}

void BlueprintArcCurveView::removeClicked()
{
    if (loadedBlueprintId.empty())
    {
        setStatus("Remove failed: no blueprint selected");
        return;
    }

    const auto removedId = loadedBlueprintId;
    processorRef.getComposerCore().getBlueprintLibrary().removeBlueprint(removedId);

    // Force the next refreshAll() to actually reload rather than treating
    // "still selected in the combo" as unchanged - the id we just removed
    // no longer exists in the library, so repopulate()'s "preserve by text"
    // can't find it and falls back to whatever's now first (or nothing).
    loadedBlueprintId.clear();

    setStatus("Removed blueprint '" + juce::String(removedId) + "' from library");
    refreshAll();
}

void BlueprintArcCurveView::blueprintComboChanged()
{
    const auto selectedId = blueprintCombo.getText().toStdString();
    if (selectedId.empty() || (selectedId == loadedBlueprintId && !isNewBlueprintMode))
        return;

    isNewBlueprintMode = false;
    barLengthLabel.setVisible(false);
    barLengthSlider.setVisible(false);
    blueprintCombo.setVisible(true);
    removeButton.setVisible(true);

    loadSelectedBlueprint();
}

void BlueprintArcCurveView::newBlueprintClicked()
{
    isNewBlueprintMode = true;
    loadedBlueprintId.clear();
    loadedBlueprint = Blueprint{};
    derivedDimensions.clear();

    blueprintCombo.setSelectedId(0, juce::dontSendNotification);
    blueprintCombo.setVisible(false);
    removeButton.setVisible(false);
    barLengthLabel.setVisible(true);
    barLengthSlider.setVisible(true);

    const int length = (int) barLengthSlider.getValue();

    // A fresh flat curve per dimension, sized to the chosen length rather
    // than ArcSet's own default (1..64) - a blank ArcSet{} would otherwise
    // put its second point off the visible axis for any length other than
    // 64, looking broken the moment "New Blueprint" is clicked.
    ArcSet blankArcSet;
    for (const auto& dimensionName : blankArcSet.getArcNames())
    {
        Arc arc;
        arc.setBreakpoints({ { 1, 0.3f }, { length, 0.3f } });
        blankArcSet.setArc(dimensionName, arc);
    }
    scratchArcSet = blankArcSet;

    curveGraph.setMaxBar(length);
    curveGraph.refreshFromArcSet();

    saveNameInput.setText({}, juce::dontSendNotification);

    updateBadge();
    setStatus("New blueprint: draw one curve (pick it above the graph), then Generate");
}

void BlueprintArcCurveView::loadSelectedBlueprint()
{
    const auto selectedId = blueprintCombo.getText().toStdString();

    if (!processorRef.getComposerCore().getBlueprintLibrary().getBlueprintById(selectedId, loadedBlueprint))
    {
        loadedBlueprintId.clear();
        return;
    }

    loadedBlueprintId = selectedId;

    const auto instances = processorRef.getComposerCore().getInstanceRegistry().getAllInstances();
    scratchArcSet = BlueprintGenerator::resolveBlueprintArcSet(loadedBlueprint, instances, derivedDimensions);
    curveGraph.setMaxBar(naturalMaxBar(loadedBlueprint));
    curveGraph.refreshFromArcSet();

    saveNameInput.setText(loadedBlueprint.id + "_v2", juce::dontSendNotification);

    updateBadge();
}

void BlueprintArcCurveView::updateBadge()
{
    if (isNewBlueprintMode)
    {
        badgeLabel.setText("New blueprint", juce::dontSendNotification);
        badgeLabel.setColour(juce::Label::textColourId, juce::Colours::limegreen);
        hintLabel.setText(
            "'" + curveGraph.getSelectedArcName() + "' becomes the driving arc - the other 4 dimensions get "
            "derived from the sections it produces. Name it below, then Generate.",
            juce::dontSendNotification);
        saveButton.setButtonText("Generate Blueprint");
        return;
    }

    const auto& selectedDimension = curveGraph.getSelectedArcName();
    const bool isDerived = std::find(derivedDimensions.begin(), derivedDimensions.end(), selectedDimension)
                            != derivedDimensions.end();

    saveButton.setButtonText("Save as new blueprint");

    if (isDerived)
    {
        badgeLabel.setText("Derived from sections", juce::dontSendNotification);
        badgeLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        hintLabel.setText(
            "This curve isn't saved yet - edit a point to turn it into this blueprint's real arc before saving.",
            juce::dontSendNotification);
    }
    else
    {
        badgeLabel.setText("Custom arc", juce::dontSendNotification);
        badgeLabel.setColour(juce::Label::textColourId, juce::Colours::limegreen);
        hintLabel.setText("Saves every dimension shown here onto a new blueprint.", juce::dontSendNotification);
    }
}

void BlueprintArcCurveView::primaryActionClicked()
{
    if (isNewBlueprintMode)
        generateClicked();
    else
        saveClicked();
}

void BlueprintArcCurveView::saveClicked()
{
    if (loadedBlueprintId.empty())
    {
        setStatus("Save failed: no blueprint selected");
        return;
    }

    const auto newId = saveNameInput.getText().trim().toStdString();
    if (newId.empty())
    {
        setStatus("Save failed: name this version first");
        return;
    }

    // A clone of the selected blueprint - same sections (and therefore the
    // same scene references), new id/name, and every dimension currently
    // held in the scratch ArcSet written as an explicit saved curve (not
    // just the ones the user touched this session) so the new blueprint
    // never depends on re-deriving from sections that might themselves get
    // edited later.
    Blueprint newBlueprint = loadedBlueprint;
    newBlueprint.id = newId;
    newBlueprint.name = newId;
    newBlueprint.arcCurves.clear();

    const auto editedDimension = curveGraph.getSelectedArcName();
    int editedDimensionPointCount = 0;

    for (const auto& dimensionName : scratchArcSet.getArcNames())
    {
        // ArcSet::getArc returns an Arc by value - binding straight to
        // ".getBreakpoints()" via "const auto&" is a dangling reference
        // (the reference points into a temporary Arc that's destroyed at
        // the end of this statement, not into scratchArcSet itself).
        // Naming the Arc as a real local keeps it alive for the rest of
        // this scope, so the reference into it stays valid.
        const Arc arc = scratchArcSet.getArc(dimensionName);
        const auto& breakpoints = arc.getBreakpoints();
        if (dimensionName == editedDimension)
            editedDimensionPointCount = (int) breakpoints.size();

        if (breakpoints.empty())
            continue; // never save a curve with no real data - see resolveBlueprintArcSet's matching guard

        BlueprintArcCurve curve;
        curve.dimension = dimensionName;
        for (const auto& breakpoint : breakpoints)
            curve.points.push_back({ breakpoint.bar, breakpoint.value });
        newBlueprint.arcCurves.push_back(curve);
    }

    std::string errorMessage;
    if (!Validation::isValidBlueprint(newBlueprint, errorMessage))
    {
        setStatus("Save failed: " + juce::String(errorMessage));
        return;
    }

    processorRef.getComposerCore().getBlueprintLibrary().addOrReplaceBlueprint(newBlueprint);

    setStatus("Saved '" + juce::String(newId) + "' as a new blueprint, cloned from '"
                  + juce::String(loadedBlueprintId) + "' (" + juce::String(editedDimension) + ": "
                  + juce::String(editedDimensionPointCount) + " pts, " + juce::String((int) newBlueprint.arcCurves.size())
                  + " curve(s) saved)");

    refreshAll();
}

void BlueprintArcCurveView::generateClicked()
{
    const auto newId = saveNameInput.getText().trim().toStdString();
    if (newId.empty())
    {
        setStatus("Generate failed: name this blueprint first");
        return;
    }

    const auto drivingArcName = curveGraph.getSelectedArcName();

    auto& composerCore = processorRef.getComposerCore();
    const auto instances = composerCore.getInstanceRegistry().getAllInstances();

    // Auto-created default base scene (2026-08-27, user's own call when
    // asked): every registered instance, Pattern 1, Binary, Swing Off - so
    // a blank-slate piece needs nothing pre-authored. BlueprintGenerator::
    // generate clones this per section and layers role-preset-matched
    // overrides on top where they apply.
    Scene baseScene;
    baseScene.id = newId + "_scene";
    baseScene.name = newId + " - Auto Scene";
    baseScene.durationBars = 4;
    baseScene.quantize = "bar";
    for (const auto& instance : instances)
        baseScene.targets.push_back(instance.id);
    baseScene.global.activePattern = 1;
    baseScene.global.gridMode = 0;
    baseScene.global.swing = 0.0f;

    const auto rolePresets = composerCore.getPresetLibrary().getAllRolePresets();
    const auto rhythmicPresets = composerCore.getPresetLibrary().getAllRhythmicRelationshipPresets();
    const auto motifPresets = composerCore.getPresetLibrary().getAllMotifPresets();

    auto proposal = BlueprintGenerator::generate(newId, drivingArcName, scratchArcSet, baseScene, instances,
                                                   rolePresets, rhythmicPresets, motifPresets);

    if (proposal.blueprint.id.empty())
    {
        setStatus("Generate failed: '" + juce::String(drivingArcName)
                      + "' needs at least 2 breakpoints to derive sections from - draw its shape first");
        return;
    }

    // baseScene itself is never referenced by id in the result - each
    // section gets its own clone (section.id + "_scene") in newScenes, per
    // BlueprintGenerator::generate - so only those need adding.
    for (const auto& scene : proposal.newScenes)
        composerCore.getSceneLibrary().addOrReplaceScene(scene);

    for (const auto& preset : proposal.newMotifPresets)
        composerCore.getPresetLibrary().addOrReplaceMotifPreset(preset);

    // Persist the full generated 5-curve ArcSet onto the blueprint itself,
    // same reasoning as ui/GenerateView::commitClicked - so this blueprint's
    // real arc survives being revisited later rather than only living in
    // whatever the live ArcSet happened to hold right after this Generate.
    proposal.blueprint.arcCurves.clear();
    for (const auto& dimensionName : proposal.candidateArcSet.getArcNames())
    {
        const Arc arc = proposal.candidateArcSet.getArc(dimensionName); // see saveClicked's own comment on why
        BlueprintArcCurve curve;
        curve.dimension = dimensionName;
        for (const auto& breakpoint : arc.getBreakpoints())
            curve.points.push_back({ breakpoint.bar, breakpoint.value });
        proposal.blueprint.arcCurves.push_back(curve);
    }

    composerCore.getBlueprintLibrary().addOrReplaceBlueprint(proposal.blueprint);
    composerCore.setCurrentBlueprint(proposal.blueprint);

    setStatus("Generated and committed '" + juce::String(newId) + "' ("
                  + juce::String((int) proposal.blueprint.sections.size()) + " section(s), driven by '"
                  + juce::String(drivingArcName) + "') - now active");

    isNewBlueprintMode = false;
    barLengthLabel.setVisible(false);
    barLengthSlider.setVisible(false);
    blueprintCombo.setVisible(true);
    removeButton.setVisible(true);

    // Reload as a normal existing-blueprint selection, so the view now
    // shows the real generated result (sections, non-driving dimensions
    // baked in) rather than the blank-slate scratch state.
    refreshAll();
    blueprintCombo.setText(newId, juce::dontSendNotification);
    loadSelectedBlueprint();
}
