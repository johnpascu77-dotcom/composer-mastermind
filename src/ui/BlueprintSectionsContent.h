#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>
#include "../model/Blueprint.h"

class ComposerMastermindAudioProcessor;

// The actual controls for the "Sections" inner tab - authors a Blueprint's
// ordered list of BlueprintSections: each a bar range referencing a Scene,
// plus per-instance foreground/support/background layering, per-role
// mutation budget overrides, and per-parameter reserved values (apex
// exclusivity) for that stretch. Mirrors SceneListComponent's build-panel +
// save/load/remove-to-library shape. Lives inside a fixed, taller-than-the-
// viewport size set by ui/BlueprintSectionsView (its juce::Viewport host) -
// this component doesn't manage its own scrolling.
//
// Consumption status: budgetOverrides feed PolicyEngine (via
// ComposerCore::resolveSectionBudgetOverride), reservedValues feed Router's
// apex-exclusivity check (via ComposerCore::isValueReservedByLaterSection) -
// see docs/mutation_policy_v0_1.md - and layerRoles' "background" entries
// feed ComposerCore::applyLayerRoleOverrides (forces Active Pattern = 0 for
// that instance unless the section's scene already says otherwise).
// "foreground"/"support" don't get a forced behavior - MPL's CC protocol has
// no volume dimension to differentiate them at, only engaged-vs-stopped.
class BlueprintSectionsContent : public juce::Component
{
public:
    BlueprintSectionsContent(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    // Repopulates the scene/instance combos (from SceneLibrary/InstanceRegistry)
    // and the saved-blueprint combo (from BlueprintLibrary).
    void refreshAll();

    // Total height this component needs to lay out all its rows - the
    // Viewport host sizes this component to at least this height.
    static int getPreferredHeight();

private:
    void addLayerRoleClicked();
    void addBudgetOverrideClicked();
    void addReservedValueClicked();
    void addModulatorValueClicked();
    void captureContentClicked();
    void addSectionClicked();
    void clearPendingClicked();
    void loadSectionClicked();
    void removeSectionClicked();
    void saveBlueprintClicked();
    void loadBlueprintClicked();
    void removeBlueprintClicked();
    void exportBlueprintClicked();
    void importBlueprintClicked();
    void primeForPlaybackClicked();

    void refreshPendingPreview();
    void refreshSectionsDisplay();
    void refreshNowPlaying();

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;

    // Reflects ComposerCore's blueprint-driven section playback (see
    // ComposerCore::advanceBlueprintIfNeeded) - which section, if any, is
    // currently playing at the current bar. Refreshed by refreshAll() on
    // the shared 300ms timer, same as everything else in this tab.
    juce::Label nowPlayingLabel;

    // "Prepare while stopped" (user's own idea, 2026-08-21) - runs
    // ComposerCore::primeForPlayback(), routing/resetting/stamping the
    // current blueprint's first section right now regardless of transport
    // state, so playback starts from already-prepared content instead of
    // racing the first bar's own setup against MPL's playback engine.
    juce::TextButton primeButton { "Prime for Playback" };

    // Section-under-construction controls.
    juce::TextEditor sectionIdInput;
    juce::ComboBox sceneCombo;
    juce::Slider startBarSlider;
    juce::Label startBarLabel { {}, "Start Bar" };
    juce::Slider durationBarsSlider;
    juce::Label durationBarsLabel { {}, "Duration" };
    juce::ComboBox archetypeCombo;
    juce::Label archetypeLabel { {}, "Archetype" };

    juce::ComboBox layerRoleInstanceCombo;
    juce::ComboBox layerRoleValueCombo;
    juce::TextButton addLayerRoleButton { "Add Layer Role" };
    juce::Label pendingLayerRolesLabel;

    juce::ComboBox budgetRoleCombo;
    juce::Label minorLabel { {}, "Min" };
    juce::Slider minorSlider;
    juce::Label mediumLabel { {}, "Med" };
    juce::Slider mediumSlider;
    juce::Label majorLabel { {}, "Maj" };
    juce::Slider majorSlider;
    juce::TextButton addBudgetOverrideButton { "Add Budget" };
    juce::Label pendingBudgetOverridesLabel;

    juce::ComboBox reservedValueInstanceCombo;
    juce::ComboBox reservedValuePatternCombo;
    juce::ComboBox reservedValueTypeCombo;
    juce::Slider reservedValueSlider;
    juce::TextButton addReservedValueButton { "Add Reserved" };
    juce::Label pendingReservedValuesLabel;

    // Section-scoped half of ModulatorTarget's "section" mode (model/ModulatorTarget.h) -
    // a fixed CC value sent once when this section becomes active, resolved by
    // target id against ModulatorTargetLibrary at playback time
    // (ComposerCore::sendSectionModulatorValues).
    juce::ComboBox modulatorValueTargetCombo;
    juce::Slider modulatorValueSlider;
    juce::TextButton addModulatorValueButton { "Add Modulator Value" };
    juce::Label pendingModulatorValuesLabel;

    // "Absolute" content mode's authoring side (model/Blueprint.h's
    // SectionCapturedContent, composer/ComposerCore.h's ContentMode) - reads
    // whatever's actually confirmed-real on the chosen instance/pattern right
    // now (PatternSyncServer's InstancePatternCache, the same ground truth
    // ui/PatternAwarenessView shows) and captures it verbatim into this
    // section. Not a value to type in, hence a button instead of a slider.
    juce::ComboBox capturedContentInstanceCombo;
    juce::ComboBox capturedContentPatternCombo;
    juce::TextButton captureContentButton { "Capture Current" };
    juce::Label pendingCapturedContentLabel;

    juce::TextButton addSectionButton { "Add Section" };
    juce::TextButton clearPendingButton { "Clear Pending" };

    juce::Label sectionsHeaderLabel { {}, "Sections in this Blueprint" };
    juce::TextEditor sectionsDisplay;
    juce::ComboBox removeSectionCombo;
    juce::TextButton loadSectionButton { "Load Section" };
    juce::TextButton removeSectionButton { "Remove Section" };

    juce::Label libraryHeaderLabel { {}, "Blueprint Library" };
    juce::TextEditor blueprintNameInput;
    juce::TextButton saveBlueprintButton { "Save Blueprint" };
    juce::ComboBox savedBlueprintsCombo;
    juce::TextButton loadBlueprintButton { "Load" };
    juce::TextButton removeBlueprintButton { "Remove" };

    // Single-composition JSON round-trip (2026-08-25, state/CompositionBundleStore) -
    // one blueprint plus only the scenes its sections reference, self-
    // contained on disk, distinct from SceneListComponent's whole-session
    // Save/Load Snapshot. Export operates on savedBlueprintsCombo's
    // selection (same as Remove); Import adds to both libraries and
    // activates the result, same as Load.
    juce::TextButton exportBlueprintButton { "Export to File..." };
    juce::TextButton importBlueprintButton { "Import from File..." };
    std::unique_ptr<juce::FileChooser> fileChooser;

    std::vector<BlueprintSection> workingSections;
    std::vector<SectionLayerRole> pendingLayerRoles;
    std::vector<SectionBudgetOverride> pendingBudgetOverrides;
    std::vector<ReservedValue> pendingReservedValues;
    std::vector<SectionModulatorValue> pendingModulatorValues;
    std::vector<SectionCapturedContent> pendingCapturedContent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlueprintSectionsContent)
};
