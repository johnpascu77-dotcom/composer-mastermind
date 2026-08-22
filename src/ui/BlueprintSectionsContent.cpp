#include "BlueprintSectionsContent.h"
#include "../plugin/PluginProcessor.h"
#include "../util/Validation.h"
#include <algorithm>

namespace
{
    constexpr int kRowHeight = 24;
    constexpr int kRowGap = 4;
    constexpr int kMargin = 10;
    constexpr int kSmallGap = 5;

    // Sum of every row/gap laid out in resized(), kept in sync with it by
    // hand - see BlueprintSectionsContent::getPreferredHeight().
    constexpr int kPreferredHeight = 660;

    void styleSlider(juce::Slider& slider, double minValue, double maxValue, double step, double initial)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, kRowHeight);
        slider.setRange(minValue, maxValue, step);
        slider.setValue(initial, juce::dontSendNotification);
    }

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

    const char* kLayerRoleNames[] = { "foreground", "support", "background" };
    const char* kBudgetRoleNames[] = { "anchor", "motif", "counterpoint" };
    const char* kMutationTypeNames[] = { "transpose", "rotation", "length", "inversion" };
    const char* kArchetypeNames[] = { "", "presentation", "build", "peak", "release" };

    // Selects the item whose displayed text matches - used to restore a
    // combo's selection from a model value (e.g. sceneCombo, whose item
    // text IS the scene id) rather than an item id, which isn't stable
    // across repopulate() calls.
    void selectComboByText(juce::ComboBox& combo, const juce::String& text)
    {
        for (int i = 0; i < combo.getNumItems(); ++i)
        {
            if (combo.getItemText(i) == text)
            {
                combo.setSelectedItemIndex(i, juce::dontSendNotification);
                return;
            }
        }
    }

    // archetypeCombo's item text ("Presentation", "Build", ...) is a display
    // label, not the raw model string kArchetypeNames holds - map by
    // position instead of by text.
    int archetypeComboIdForValue(const std::string& archetype)
    {
        for (int i = 0; i < (int) (sizeof(kArchetypeNames) / sizeof(kArchetypeNames[0])); ++i)
            if (kArchetypeNames[i] == archetype)
                return i + 1;
        return 1; // "(none)"
    }
}

BlueprintSectionsContent::BlueprintSectionsContent(ComposerMastermindAudioProcessor& processor,
                                                    std::function<void(const juce::String&)> status)
    : processorRef(processor), setStatus(std::move(status))
{
    addAndMakeVisible(nowPlayingLabel);
    nowPlayingLabel.setFont(juce::Font(13.0f, juce::Font::bold));

    addAndMakeVisible(primeButton);
    primeButton.onClick = [this] { primeForPlaybackClicked(); };

    addAndMakeVisible(sectionIdInput);
    sectionIdInput.setTextToShowWhenEmpty("section id/name, e.g. buildup", juce::Colours::grey);

    addAndMakeVisible(sceneCombo);
    sceneCombo.setTextWhenNothingSelected("(no scene)");

    addAndMakeVisible(startBarSlider);
    styleSlider(startBarSlider, 0.0, 256.0, 1.0, 0.0);
    addAndMakeVisible(startBarLabel);

    addAndMakeVisible(durationBarsSlider);
    styleSlider(durationBarsSlider, 1.0, 64.0, 1.0, 4.0);
    addAndMakeVisible(durationBarsLabel);

    addAndMakeVisible(archetypeCombo);
    archetypeCombo.addItem("(none)", 1);
    archetypeCombo.addItem("Presentation", 2);
    archetypeCombo.addItem("Build", 3);
    archetypeCombo.addItem("Peak", 4);
    archetypeCombo.addItem("Release", 5);
    archetypeCombo.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(archetypeLabel);

    addAndMakeVisible(layerRoleInstanceCombo);
    layerRoleInstanceCombo.setTextWhenNothingSelected("(pick instance)");

    addAndMakeVisible(layerRoleValueCombo);
    layerRoleValueCombo.addItem("Foreground", 1);
    layerRoleValueCombo.addItem("Support", 2);
    layerRoleValueCombo.addItem("Background", 3);
    layerRoleValueCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(addLayerRoleButton);
    addLayerRoleButton.onClick = [this] { addLayerRoleClicked(); };

    addAndMakeVisible(pendingLayerRolesLabel);
    pendingLayerRolesLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    pendingLayerRolesLabel.setMinimumHorizontalScale(1.0f);

    addAndMakeVisible(budgetRoleCombo);
    budgetRoleCombo.addItem("Anchor", 1);
    budgetRoleCombo.addItem("Motif", 2);
    budgetRoleCombo.addItem("Counterpoint", 3);
    budgetRoleCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(minorLabel);
    addAndMakeVisible(minorSlider);
    styleSlider(minorSlider, -1.0, 8.0, 1.0, -1.0);

    addAndMakeVisible(mediumLabel);
    addAndMakeVisible(mediumSlider);
    styleSlider(mediumSlider, -1.0, 8.0, 1.0, -1.0);

    addAndMakeVisible(majorLabel);
    addAndMakeVisible(majorSlider);
    styleSlider(majorSlider, -1.0, 8.0, 1.0, -1.0);

    addAndMakeVisible(addBudgetOverrideButton);
    addBudgetOverrideButton.onClick = [this] { addBudgetOverrideClicked(); };

    addAndMakeVisible(pendingBudgetOverridesLabel);
    pendingBudgetOverridesLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    pendingBudgetOverridesLabel.setMinimumHorizontalScale(1.0f);

    addAndMakeVisible(reservedValueInstanceCombo);
    reservedValueInstanceCombo.setTextWhenNothingSelected("(pick instance)");

    addAndMakeVisible(reservedValuePatternCombo);
    reservedValuePatternCombo.addItem("Pattern 1", 1);
    reservedValuePatternCombo.addItem("Pattern 2", 2);
    reservedValuePatternCombo.addItem("Pattern 3", 3);
    reservedValuePatternCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(reservedValueTypeCombo);
    reservedValueTypeCombo.addItem("Transpose", 1);
    reservedValueTypeCombo.addItem("Rotation", 2);
    reservedValueTypeCombo.addItem("Length", 3);
    reservedValueTypeCombo.addItem("Inversion", 4);
    reservedValueTypeCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(reservedValueSlider);
    styleSlider(reservedValueSlider, -48.0, 48.0, 1.0, 0.0);

    addAndMakeVisible(addReservedValueButton);
    addReservedValueButton.onClick = [this] { addReservedValueClicked(); };

    addAndMakeVisible(pendingReservedValuesLabel);
    pendingReservedValuesLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    pendingReservedValuesLabel.setMinimumHorizontalScale(1.0f);

    addAndMakeVisible(modulatorValueTargetCombo);
    modulatorValueTargetCombo.setTextWhenNothingSelected("(pick modulator target)");

    addAndMakeVisible(modulatorValueSlider);
    styleSlider(modulatorValueSlider, 0.0, 127.0, 1.0, 0.0);

    addAndMakeVisible(addModulatorValueButton);
    addModulatorValueButton.onClick = [this] { addModulatorValueClicked(); };

    addAndMakeVisible(pendingModulatorValuesLabel);
    pendingModulatorValuesLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    pendingModulatorValuesLabel.setMinimumHorizontalScale(1.0f);

    addAndMakeVisible(addSectionButton);
    addSectionButton.onClick = [this] { addSectionClicked(); };

    addAndMakeVisible(clearPendingButton);
    clearPendingButton.onClick = [this] { clearPendingClicked(); };

    addAndMakeVisible(sectionsHeaderLabel);
    sectionsHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(sectionsDisplay);
    sectionsDisplay.setMultiLine(true);
    sectionsDisplay.setReadOnly(true);
    sectionsDisplay.setScrollbarsShown(true);
    sectionsDisplay.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));

    addAndMakeVisible(removeSectionCombo);
    removeSectionCombo.setTextWhenNothingSelected("(pick section)");

    addAndMakeVisible(loadSectionButton);
    loadSectionButton.onClick = [this] { loadSectionClicked(); };

    addAndMakeVisible(removeSectionButton);
    removeSectionButton.onClick = [this] { removeSectionClicked(); };

    addAndMakeVisible(libraryHeaderLabel);
    libraryHeaderLabel.setFont(juce::Font(15.0f, juce::Font::bold));

    addAndMakeVisible(blueprintNameInput);
    blueprintNameInput.setTextToShowWhenEmpty("blueprint id/name, e.g. song_a", juce::Colours::grey);

    addAndMakeVisible(saveBlueprintButton);
    saveBlueprintButton.onClick = [this] { saveBlueprintClicked(); };

    addAndMakeVisible(savedBlueprintsCombo);

    addAndMakeVisible(loadBlueprintButton);
    loadBlueprintButton.onClick = [this] { loadBlueprintClicked(); };

    addAndMakeVisible(removeBlueprintButton);
    removeBlueprintButton.onClick = [this] { removeBlueprintClicked(); };

    refreshPendingPreview();
    refreshSectionsDisplay();
    refreshNowPlaying();
    refreshAll();
}

void BlueprintSectionsContent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

int BlueprintSectionsContent::getPreferredHeight()
{
    return kPreferredHeight;
}

void BlueprintSectionsContent::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto nextRow = [&](int height) -> juce::Rectangle<int>
    {
        auto row = area.removeFromTop(height);
        area.removeFromTop(kRowGap);
        return row;
    };

    auto nowPlayingRow = nextRow(kRowHeight);
    primeButton.setBounds(nowPlayingRow.removeFromRight(150));
    nowPlayingRow.removeFromRight(kMargin);
    nowPlayingLabel.setBounds(nowPlayingRow);

    auto sectionRow = nextRow(kRowHeight);
    sectionIdInput.setBounds(sectionRow.removeFromLeft(200));
    sectionRow.removeFromLeft(kMargin);
    sceneCombo.setBounds(sectionRow.removeFromLeft(150));

    auto barsRow = nextRow(kRowHeight);
    startBarLabel.setBounds(barsRow.removeFromLeft(55));
    startBarSlider.setBounds(barsRow.removeFromLeft(130));
    barsRow.removeFromLeft(kMargin);
    durationBarsLabel.setBounds(barsRow.removeFromLeft(55));
    durationBarsSlider.setBounds(barsRow.removeFromLeft(130));

    auto archetypeRow = nextRow(kRowHeight);
    archetypeLabel.setBounds(archetypeRow.removeFromLeft(70));
    archetypeCombo.setBounds(archetypeRow.removeFromLeft(150));

    auto layerRoleRow = nextRow(kRowHeight);
    layerRoleInstanceCombo.setBounds(layerRoleRow.removeFromLeft(150));
    layerRoleRow.removeFromLeft(kSmallGap);
    layerRoleValueCombo.setBounds(layerRoleRow.removeFromLeft(110));
    layerRoleRow.removeFromLeft(kSmallGap);
    addLayerRoleButton.setBounds(layerRoleRow.removeFromLeft(120));

    pendingLayerRolesLabel.setBounds(nextRow(18));

    auto budgetRow = nextRow(kRowHeight);
    budgetRoleCombo.setBounds(budgetRow.removeFromLeft(110));
    budgetRow.removeFromLeft(kSmallGap);
    minorLabel.setBounds(budgetRow.removeFromLeft(28));
    minorSlider.setBounds(budgetRow.removeFromLeft(75));
    budgetRow.removeFromLeft(kSmallGap);
    mediumLabel.setBounds(budgetRow.removeFromLeft(30));
    mediumSlider.setBounds(budgetRow.removeFromLeft(75));
    budgetRow.removeFromLeft(kSmallGap);
    majorLabel.setBounds(budgetRow.removeFromLeft(28));
    majorSlider.setBounds(budgetRow.removeFromLeft(75));

    auto addBudgetRow = nextRow(kRowHeight);
    addBudgetOverrideButton.setBounds(addBudgetRow.removeFromLeft(120));

    pendingBudgetOverridesLabel.setBounds(nextRow(18));

    auto reservedValueRow = nextRow(kRowHeight);
    reservedValueInstanceCombo.setBounds(reservedValueRow.removeFromLeft(150));
    reservedValueRow.removeFromLeft(kSmallGap);
    reservedValuePatternCombo.setBounds(reservedValueRow.removeFromLeft(100));
    reservedValueRow.removeFromLeft(kSmallGap);
    reservedValueTypeCombo.setBounds(reservedValueRow.removeFromLeft(110));

    auto addReservedValueRow = nextRow(kRowHeight);
    reservedValueSlider.setBounds(addReservedValueRow.removeFromLeft(150));
    addReservedValueRow.removeFromLeft(kSmallGap);
    addReservedValueButton.setBounds(addReservedValueRow.removeFromLeft(120));

    pendingReservedValuesLabel.setBounds(nextRow(18));

    auto modulatorValueRow = nextRow(kRowHeight);
    modulatorValueTargetCombo.setBounds(modulatorValueRow.removeFromLeft(180));
    modulatorValueRow.removeFromLeft(kSmallGap);
    modulatorValueSlider.setBounds(modulatorValueRow.removeFromLeft(150));
    modulatorValueRow.removeFromLeft(kSmallGap);
    addModulatorValueButton.setBounds(modulatorValueRow.removeFromLeft(140));

    pendingModulatorValuesLabel.setBounds(nextRow(18));

    auto addSectionRow = nextRow(kRowHeight);
    addSectionButton.setBounds(addSectionRow.removeFromLeft(120));
    addSectionRow.removeFromLeft(kMargin);
    clearPendingButton.setBounds(addSectionRow.removeFromLeft(120));

    area.removeFromTop(kMargin - kRowGap);
    sectionsHeaderLabel.setBounds(nextRow(20));

    sectionsDisplay.setBounds(area.removeFromTop(70));
    area.removeFromTop(kRowGap);

    auto removeSectionRow = nextRow(kRowHeight);
    removeSectionCombo.setBounds(removeSectionRow.removeFromLeft(200));
    removeSectionRow.removeFromLeft(kMargin);
    loadSectionButton.setBounds(removeSectionRow.removeFromLeft(100));
    removeSectionRow.removeFromLeft(kMargin);
    removeSectionButton.setBounds(removeSectionRow.removeFromLeft(120));

    area.removeFromTop(kMargin - kRowGap);
    libraryHeaderLabel.setBounds(nextRow(20));

    auto saveBlueprintRow = nextRow(kRowHeight);
    blueprintNameInput.setBounds(saveBlueprintRow.removeFromLeft(220));
    saveBlueprintRow.removeFromLeft(kMargin);
    saveBlueprintButton.setBounds(saveBlueprintRow.removeFromLeft(150));

    auto loadBlueprintRow = nextRow(kRowHeight);
    savedBlueprintsCombo.setBounds(loadBlueprintRow.removeFromLeft(220));
    loadBlueprintRow.removeFromLeft(kMargin);
    loadBlueprintButton.setBounds(loadBlueprintRow.removeFromLeft(90));
    loadBlueprintRow.removeFromLeft(kMargin);
    removeBlueprintButton.setBounds(loadBlueprintRow.removeFromLeft(90));
}

void BlueprintSectionsContent::addLayerRoleClicked()
{
    if (layerRoleInstanceCombo.getSelectedId() <= 0)
    {
        setStatus("Add layer role skipped: pick an instance first");
        return;
    }

    SectionLayerRole layerRole;
    layerRole.targetInstance = layerRoleInstanceCombo.getText().toStdString();
    layerRole.layerRole = kLayerRoleNames[juce::jlimit(0, 2, layerRoleValueCombo.getSelectedItemIndex())];

    for (auto& existing : pendingLayerRoles)
    {
        if (existing.targetInstance == layerRole.targetInstance)
        {
            existing = layerRole;
            refreshPendingPreview();
            return;
        }
    }

    pendingLayerRoles.push_back(layerRole);
    refreshPendingPreview();
}

void BlueprintSectionsContent::addBudgetOverrideClicked()
{
    SectionBudgetOverride budgetOverride;
    budgetOverride.role = kBudgetRoleNames[juce::jlimit(0, 2, budgetRoleCombo.getSelectedItemIndex())];
    budgetOverride.maxMinorPerBar = static_cast<int>(minorSlider.getValue());
    budgetOverride.maxMediumPerBar = static_cast<int>(mediumSlider.getValue());
    budgetOverride.maxMajorPerBar = static_cast<int>(majorSlider.getValue());

    for (auto& existing : pendingBudgetOverrides)
    {
        if (existing.role == budgetOverride.role)
        {
            existing = budgetOverride;
            refreshPendingPreview();
            return;
        }
    }

    pendingBudgetOverrides.push_back(budgetOverride);
    refreshPendingPreview();
}

void BlueprintSectionsContent::addReservedValueClicked()
{
    if (reservedValueInstanceCombo.getSelectedId() <= 0)
    {
        setStatus("Add reserved value skipped: pick an instance first");
        return;
    }

    ReservedValue reservedValue;
    reservedValue.targetInstance = reservedValueInstanceCombo.getText().toStdString();
    reservedValue.patternIndex = reservedValuePatternCombo.getSelectedId() - 1;
    reservedValue.type = kMutationTypeNames[juce::jlimit(0, 3, reservedValueTypeCombo.getSelectedItemIndex())];
    reservedValue.value = (reservedValue.type == "inversion")
                               ? (reservedValueSlider.getValue() != 0.0 ? 1 : 0)
                               : static_cast<int>(reservedValueSlider.getValue());

    for (auto& existing : pendingReservedValues)
    {
        if (existing.targetInstance == reservedValue.targetInstance
            && existing.patternIndex == reservedValue.patternIndex && existing.type == reservedValue.type)
        {
            existing = reservedValue;
            refreshPendingPreview();
            return;
        }
    }

    pendingReservedValues.push_back(reservedValue);
    refreshPendingPreview();
}

void BlueprintSectionsContent::addModulatorValueClicked()
{
    if (modulatorValueTargetCombo.getSelectedId() <= 0)
    {
        setStatus("Add modulator value skipped: pick a modulator target first");
        return;
    }

    SectionModulatorValue modulatorValue;
    modulatorValue.modulatorTargetId = modulatorValueTargetCombo.getText().toStdString();
    modulatorValue.value = static_cast<int>(modulatorValueSlider.getValue());

    for (auto& existing : pendingModulatorValues)
    {
        if (existing.modulatorTargetId == modulatorValue.modulatorTargetId)
        {
            existing = modulatorValue;
            refreshPendingPreview();
            return;
        }
    }

    pendingModulatorValues.push_back(modulatorValue);
    refreshPendingPreview();
}

void BlueprintSectionsContent::addSectionClicked()
{
    const auto id = sectionIdInput.getText().trim().toStdString();
    if (id.empty())
    {
        setStatus("Add section failed: enter a section id/name first");
        return;
    }

    BlueprintSection section;
    section.id = id;
    section.name = id;
    section.sceneId = sceneCombo.getText().toStdString();
    section.startBar = static_cast<int>(startBarSlider.getValue());
    section.durationBars = static_cast<int>(durationBarsSlider.getValue());
    section.archetype = kArchetypeNames[juce::jlimit(0, 4, archetypeCombo.getSelectedItemIndex())];
    section.layerRoles = pendingLayerRoles;
    section.budgetOverrides = pendingBudgetOverrides;
    section.reservedValues = pendingReservedValues;
    section.modulatorValues = pendingModulatorValues;

    bool replaced = false;
    for (auto& existing : workingSections)
    {
        if (existing.id == id)
        {
            existing = section;
            replaced = true;
            break;
        }
    }

    if (!replaced)
        workingSections.push_back(section);

    std::sort(workingSections.begin(), workingSections.end(),
               [](const BlueprintSection& a, const BlueprintSection& b) { return a.startBar < b.startBar; });

    pendingLayerRoles.clear();
    pendingBudgetOverrides.clear();
    pendingReservedValues.clear();
    pendingModulatorValues.clear();
    sectionIdInput.clear();

    refreshPendingPreview();
    refreshSectionsDisplay();

    std::vector<std::string> sectionIds;
    for (const auto& existing : workingSections)
        sectionIds.push_back(existing.id);
    repopulate(removeSectionCombo, sectionIds, false);

    setStatus((replaced ? "Updated section '" : "Added section '") + juce::String(id) + "'");
}

void BlueprintSectionsContent::clearPendingClicked()
{
    pendingLayerRoles.clear();
    pendingBudgetOverrides.clear();
    pendingReservedValues.clear();
    pendingModulatorValues.clear();
    refreshPendingPreview();
    setStatus("Cleared pending layer roles, budget overrides, reserved values, and modulator values");
}

void BlueprintSectionsContent::loadSectionClicked()
{
    if (removeSectionCombo.getSelectedId() <= 0)
    {
        setStatus("Load section skipped: no section selected");
        return;
    }

    const auto id = removeSectionCombo.getText().toStdString();

    for (const auto& existing : workingSections)
    {
        if (existing.id != id)
            continue;

        // Populates the builder controls (including the pending layer-role/
        // budget/reserved/modulator lists, which otherwise only ever grow
        // by hand one "Add" at a time) from an already-saved section, so
        // editing one field doesn't mean re-typing everything else from
        // scratch. addSectionClicked() already updates in place when the
        // id matches an existing section - this is what was missing to
        // actually reach that path with the rest of the section's data
        // intact.
        sectionIdInput.setText(existing.id, juce::dontSendNotification);
        selectComboByText(sceneCombo, juce::String(existing.sceneId));
        startBarSlider.setValue(existing.startBar, juce::dontSendNotification);
        durationBarsSlider.setValue(existing.durationBars, juce::dontSendNotification);
        archetypeCombo.setSelectedId(archetypeComboIdForValue(existing.archetype), juce::dontSendNotification);

        pendingLayerRoles = existing.layerRoles;
        pendingBudgetOverrides = existing.budgetOverrides;
        pendingReservedValues = existing.reservedValues;
        pendingModulatorValues = existing.modulatorValues;

        refreshPendingPreview();
        setStatus("Loaded section '" + juce::String(id) + "' for editing - change fields, then Add Section to update it");
        return;
    }

    setStatus("Load section failed: '" + juce::String(id) + "' not found");
}

void BlueprintSectionsContent::removeSectionClicked()
{
    if (removeSectionCombo.getSelectedId() <= 0)
    {
        setStatus("Remove section skipped: no section selected");
        return;
    }

    const auto id = removeSectionCombo.getText().toStdString();

    for (auto it = workingSections.begin(); it != workingSections.end(); ++it)
    {
        if (it->id == id)
        {
            workingSections.erase(it);
            break;
        }
    }

    refreshSectionsDisplay();

    std::vector<std::string> sectionIds;
    for (const auto& existing : workingSections)
        sectionIds.push_back(existing.id);
    repopulate(removeSectionCombo, sectionIds, false);

    setStatus("Removed section '" + juce::String(id) + "'");
}

void BlueprintSectionsContent::saveBlueprintClicked()
{
    const auto blueprintId = blueprintNameInput.getText().trim().toStdString();
    if (blueprintId.empty())
    {
        setStatus("Save blueprint failed: enter a blueprint name/id first");
        return;
    }

    Blueprint blueprint;
    blueprint.id = blueprintId;
    blueprint.name = blueprintId;
    blueprint.sections = workingSections;

    // Preserve any arc curves already saved on this blueprint id (see
    // ui/BlueprintArcCurveView) - this tab only edits sections, so
    // resaving from here shouldn't silently discard curve data authored
    // elsewhere.
    Blueprint existingBlueprint;
    if (processorRef.getComposerCore().getBlueprintLibrary().getBlueprintById(blueprintId, existingBlueprint))
        blueprint.arcCurves = existingBlueprint.arcCurves;

    std::string errorMessage;
    if (!Validation::isValidBlueprint(blueprint, errorMessage))
    {
        setStatus("Save blueprint failed: " + errorMessage);
        return;
    }

    processorRef.getComposerCore().getBlueprintLibrary().addOrReplaceBlueprint(blueprint);
    processorRef.getComposerCore().setCurrentBlueprint(blueprint);

    setStatus("Saved blueprint '" + juce::String(blueprintId) + "' with "
                  + juce::String((int) blueprint.sections.size()) + " section(s)");
    refreshAll();
}

void BlueprintSectionsContent::loadBlueprintClicked()
{
    if (savedBlueprintsCombo.getSelectedId() <= 0)
    {
        setStatus("Load blueprint skipped: no saved blueprint selected");
        return;
    }

    const auto blueprintId = savedBlueprintsCombo.getText().toStdString();

    Blueprint blueprint;
    if (!processorRef.getComposerCore().getBlueprintLibrary().getBlueprintById(blueprintId, blueprint))
    {
        setStatus("Load blueprint failed: '" + juce::String(blueprintId) + "' not found");
        return;
    }

    workingSections = blueprint.sections;
    pendingLayerRoles.clear();
    pendingBudgetOverrides.clear();
    pendingReservedValues.clear();
    pendingModulatorValues.clear();
    blueprintNameInput.setText(blueprint.name, juce::dontSendNotification);

    processorRef.getComposerCore().setCurrentBlueprint(blueprint);

    refreshPendingPreview();
    refreshSectionsDisplay();

    std::vector<std::string> sectionIds;
    for (const auto& existing : workingSections)
        sectionIds.push_back(existing.id);
    repopulate(removeSectionCombo, sectionIds, false);

    setStatus("Loaded blueprint '" + juce::String(blueprintId) + "' ("
                  + juce::String((int) blueprint.sections.size()) + " section(s))");
}

void BlueprintSectionsContent::removeBlueprintClicked()
{
    if (savedBlueprintsCombo.getSelectedId() <= 0)
    {
        setStatus("Remove blueprint skipped: no saved blueprint selected");
        return;
    }

    const auto blueprintId = savedBlueprintsCombo.getText().toStdString();
    processorRef.getComposerCore().getBlueprintLibrary().removeBlueprint(blueprintId);
    setStatus("Removed blueprint '" + juce::String(blueprintId) + "' from library");
    refreshAll();
}

void BlueprintSectionsContent::primeForPlaybackClicked()
{
    processorRef.getComposerCore().primeForPlayback();
    setStatus("Primed first section for playback - patterns should already reflect it in MPL now");
}

void BlueprintSectionsContent::refreshPendingPreview()
{
    juce::String layerText = "Pending layer roles: ";
    if (pendingLayerRoles.empty())
    {
        layerText << "(none)";
    }
    else
    {
        for (size_t i = 0; i < pendingLayerRoles.size(); ++i)
        {
            if (i > 0)
                layerText << ", ";
            layerText << pendingLayerRoles[i].targetInstance << "=" << pendingLayerRoles[i].layerRole;
        }
    }
    pendingLayerRolesLabel.setText(layerText, juce::dontSendNotification);

    juce::String budgetText = "Pending budget overrides: ";
    if (pendingBudgetOverrides.empty())
    {
        budgetText << "(none)";
    }
    else
    {
        for (size_t i = 0; i < pendingBudgetOverrides.size(); ++i)
        {
            if (i > 0)
                budgetText << ", ";
            const auto& budgetOverride = pendingBudgetOverrides[i];
            budgetText << budgetOverride.role << "(min=" << budgetOverride.maxMinorPerBar
                       << " med=" << budgetOverride.maxMediumPerBar
                       << " maj=" << budgetOverride.maxMajorPerBar << ")";
        }
    }
    pendingBudgetOverridesLabel.setText(budgetText, juce::dontSendNotification);

    juce::String reservedText = "Pending reserved values: ";
    if (pendingReservedValues.empty())
    {
        reservedText << "(none)";
    }
    else
    {
        for (size_t i = 0; i < pendingReservedValues.size(); ++i)
        {
            if (i > 0)
                reservedText << ", ";
            const auto& reservedValue = pendingReservedValues[i];
            reservedText << reservedValue.targetInstance << "/p" << (reservedValue.patternIndex + 1)
                        << "/" << reservedValue.type << "=" << reservedValue.value;
        }
    }
    pendingReservedValuesLabel.setText(reservedText, juce::dontSendNotification);

    juce::String modulatorText = "Pending modulator values: ";
    if (pendingModulatorValues.empty())
    {
        modulatorText << "(none)";
    }
    else
    {
        for (size_t i = 0; i < pendingModulatorValues.size(); ++i)
        {
            if (i > 0)
                modulatorText << ", ";
            modulatorText << pendingModulatorValues[i].modulatorTargetId << "=" << pendingModulatorValues[i].value;
        }
    }
    pendingModulatorValuesLabel.setText(modulatorText, juce::dontSendNotification);
}

void BlueprintSectionsContent::refreshSectionsDisplay()
{
    juce::String text;
    for (const auto& section : workingSections)
    {
        text << section.id << "  scene=" << (section.sceneId.empty() ? "(none)" : section.sceneId)
             << "  bars=" << section.startBar << "-" << (section.startBar + section.durationBars)
             << "  archetype=" << (section.archetype.empty() ? "(none)" : section.archetype)
             << "  layerRoles=" << (int) section.layerRoles.size()
             << "  budgetOverrides=" << (int) section.budgetOverrides.size()
             << "  reservedValues=" << (int) section.reservedValues.size()
             << "  modulatorValues=" << (int) section.modulatorValues.size() << "\n";
    }

    if (text.isEmpty())
        text = "(no sections yet)";

    sectionsDisplay.setText(text, juce::dontSendNotification);
}

void BlueprintSectionsContent::refreshNowPlaying()
{
    Blueprint currentBlueprint;
    if (!processorRef.getComposerCore().getCurrentBlueprint(currentBlueprint))
    {
        nowPlayingLabel.setText("No active blueprint", juce::dontSendNotification);
        return;
    }

    const auto currentBar = processorRef.getComposerCore().getCurrentBar();
    const auto activeSectionId = processorRef.getComposerCore().getActiveSectionId();

    juce::String text = "Blueprint '" + juce::String(currentBlueprint.name) + "' - bar " + juce::String(currentBar) + " - ";
    text << (activeSectionId.empty() ? juce::String("no section covers this bar")
                                      : "now playing section '" + juce::String(activeSectionId) + "'");

    nowPlayingLabel.setText(text, juce::dontSendNotification);
}

void BlueprintSectionsContent::refreshAll()
{
    refreshNowPlaying();

    std::vector<std::string> sceneIds;
    for (const auto& scene : processorRef.getComposerCore().getSceneLibrary().getAllScenes())
        sceneIds.push_back(scene.id);
    repopulate(sceneCombo, sceneIds, false);

    std::vector<std::string> instanceIds;
    for (const auto& instance : processorRef.getComposerCore().getInstanceRegistry().getAllInstances())
        instanceIds.push_back(instance.id);
    repopulate(layerRoleInstanceCombo, instanceIds, false);
    repopulate(reservedValueInstanceCombo, instanceIds, false);

    std::vector<std::string> modulatorTargetIds;
    for (const auto& target : processorRef.getComposerCore().getModulatorTargetLibrary().getAllTargets())
        modulatorTargetIds.push_back(target.id);
    repopulate(modulatorValueTargetCombo, modulatorTargetIds, false);

    std::vector<std::string> blueprintIds;
    for (const auto& blueprint : processorRef.getComposerCore().getBlueprintLibrary().getAllBlueprints())
        blueprintIds.push_back(blueprint.id);
    repopulate(savedBlueprintsCombo, blueprintIds, false);
}
