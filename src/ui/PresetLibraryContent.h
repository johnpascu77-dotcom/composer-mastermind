#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include "../model/Preset.h"

class ComposerMastermindAudioProcessor;

// The actual controls for the Presets tab - authors RolePresets (role-
// addressed, instance-level activePattern/gridMode/swing bundles) and
// RhythmicRelationshipPresets (the same fields, but as a joint bundle
// across 2+ roles applied together), saves/loads/removes them from
// PresetLibrary, and applies a saved preset directly onto a saved Scene in
// SceneLibrary via policy/PresetResolver. First two of the design doc's
// three preset categories ("'Send To All' Is One Preset Among Many");
// arc presets remain unbuilt. Lives inside a fixed, taller-than-the-
// viewport size set by ui/PresetLibraryView (its juce::Viewport host).
class PresetLibraryContent : public juce::Component
{
public:
    PresetLibraryContent(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    void refreshAll();

    static int getPreferredHeight();

private:
    void savePresetClicked();
    void loadPresetClicked();
    void removePresetClicked();
    void applyToSceneClicked();
    void refreshPresetsDisplay();
    RolePreset buildPresetFromPanel(const std::string& id) const;
    void loadPresetIntoPanel(const RolePreset& preset);

    void addRoleSlotClicked();
    void clearPendingRoleSlotsClicked();
    void saveRhythmicPresetClicked();
    void loadRhythmicPresetClicked();
    void removeRhythmicPresetClicked();
    void applyRhythmicToSceneClicked();
    void refreshPendingRoleSlotsPreview();
    void refreshRhythmicPresetsDisplay();

    void loadFromLiveArcClicked();
    void addArcBreakpointClicked();
    void clearPendingArcBreakpointsClicked();
    void saveArcPresetClicked();
    void loadArcPresetClicked();
    void removeArcPresetClicked();
    void applyArcPresetClicked();
    void refreshPendingArcBreakpointsPreview();
    void refreshArcPresetsDisplay();

    void addMotifNoteClicked();
    void addMotifRestClicked();
    void clearPendingMotifNotesClicked();
    void saveMotifPresetClicked();
    void loadMotifPresetClicked();
    void removeMotifPresetClicked();
    void captureMotifPresetClicked();
    void refreshPendingMotifNotesPreview();
    void refreshMotifPresetsDisplay();
    void motifApplicationModeChanged();
    void refreshCaptureInstanceCombo();

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;

    // -- Role preset builder --
    juce::Label builderHeaderLabel { {}, "Role Preset Builder" };
    juce::TextEditor presetIdInput;
    juce::ComboBox targetRoleCombo;
    juce::TextEditor tagsInput;

    juce::ComboBox gridModeCombo;
    juce::ComboBox activePatternCombo;
    juce::ToggleButton overrideSwingToggle { "Override Swing" };
    juce::ComboBox swingCombo;

    juce::TextButton savePresetButton { "Save Preset" };

    juce::Label libraryHeaderLabel { {}, "Role Preset Library" };
    juce::ComboBox savedPresetsCombo;
    juce::TextButton loadPresetButton { "Load" };
    juce::TextButton removePresetButton { "Remove" };
    juce::TextEditor presetsDisplay;

    juce::Label applyHeaderLabel { {}, "Apply Role Preset to Scene" };
    juce::ComboBox applyPresetCombo;
    juce::ComboBox applySceneCombo;
    juce::TextButton applyToSceneButton { "Apply to Scene" };

    // -- Rhythmic-relationship preset builder --
    juce::Label rhythmicHeaderLabel { {}, "Rhythmic Relationship Preset Builder" };
    juce::TextEditor rhythmicIdInput;
    juce::TextEditor rhythmicTagsInput;

    juce::ComboBox slotRoleCombo;
    juce::ComboBox slotGridModeCombo;
    juce::ComboBox slotActivePatternCombo;
    juce::TextButton addRoleSlotButton { "Add Role Slot" };

    juce::ToggleButton slotOverrideSwingToggle { "Override Swing" };
    juce::ComboBox slotSwingCombo;

    juce::Label pendingRoleSlotsLabel;
    juce::TextButton saveRhythmicPresetButton { "Save Preset" };
    juce::TextButton clearPendingSlotsButton { "Clear Pending" };

    juce::Label rhythmicLibraryHeaderLabel { {}, "Rhythmic Relationship Library" };
    juce::ComboBox savedRhythmicPresetsCombo;
    juce::TextButton loadRhythmicPresetButton { "Load" };
    juce::TextButton removeRhythmicPresetButton { "Remove" };
    juce::TextEditor rhythmicPresetsDisplay;

    juce::Label rhythmicApplyHeaderLabel { {}, "Apply Rhythmic Preset to Scene" };
    juce::ComboBox applyRhythmicPresetCombo;
    juce::ComboBox applyRhythmicSceneCombo;
    juce::TextButton applyRhythmicToSceneButton { "Apply to Scene" };

    // -- Arc preset builder --
    juce::Label arcHeaderLabel { {}, "Arc Preset Builder" };
    juce::TextEditor arcIdInput;
    juce::TextEditor arcTagsInput;

    // Round-trip fix (v1.2 Phase 0, docs/arc_dimension_mapping_concept.md):
    // populate pendingArcBreakpoints from whatever is currently drawn on the
    // live ArcSet (the same one ui/ArcGraphView edits on the Blueprint tab)
    // for a chosen dimension/bar-range, normalized to position 0..1 - an
    // alternative to entering every breakpoint by hand below.
    juce::ComboBox loadFromArcDimensionCombo;
    juce::Slider loadFromArcStartBarSlider;
    juce::Slider loadFromArcEndBarSlider;
    juce::TextButton loadFromArcButton { "Load From Live Arc" };

    juce::Slider breakpointPositionSlider;
    juce::Slider breakpointValueSlider;
    juce::TextButton addBreakpointButton { "Add Breakpoint" };

    juce::Label pendingArcBreakpointsLabel;
    juce::TextButton saveArcPresetButton { "Save Preset" };
    juce::TextButton clearPendingBreakpointsButton { "Clear Pending" };

    juce::Label arcLibraryHeaderLabel { {}, "Arc Preset Library" };
    juce::ComboBox savedArcPresetsCombo;
    juce::TextButton loadArcPresetButton { "Load" };
    juce::TextButton removeArcPresetButton { "Remove" };
    juce::TextEditor arcPresetsDisplay;

    juce::Label arcApplyHeaderLabel { {}, "Apply Arc Preset" };
    juce::ComboBox applyArcPresetCombo;
    juce::ComboBox applyTargetArcCombo;
    juce::Slider applyStartBarSlider;
    juce::Slider applyEndBarSlider;
    juce::TextButton applyArcPresetButton { "Apply to Arc" };

    // -- Motif preset builder --
    // No "apply" section yet, unlike the other three categories - there's
    // no consumer until the motif/rule engine (policy/MotifEngine) exists,
    // see docs/technical_spec_checklist.md's v0.6 section. This slice is
    // authoring + storage only.
    juce::Label motifHeaderLabel { {}, "Motif Preset Builder" };

    // Global, session-level (see ComposerCore::getMotifApplicationMode) -
    // governs how policy/MotifEngine writes *any* motif it applies, not a
    // property of the preset being built below.
    juce::Label motifApplicationModeLabel { {}, "Application Mode (global)" };
    juce::ComboBox motifApplicationModeCombo;

    juce::TextEditor motifIdInput;
    juce::TextEditor motifTagsInput;

    juce::Label motifSemitoneOffsetLabel { {}, "Semitone Offset" };
    juce::Slider motifSemitoneOffsetSlider;
    juce::Label motifRelativeDurationLabel { {}, "Relative Duration" };
    juce::Slider motifRelativeDurationSlider;
    juce::Label motifRelativeVelocityLabel { {}, "Relative Velocity" };
    juce::Slider motifRelativeVelocitySlider;
    juce::TextButton addMotifNoteButton { "Add Note" };
    juce::TextButton addMotifRestButton { "Add Rest" };

    juce::Label pendingMotifNotesLabel;
    juce::TextButton saveMotifPresetButton { "Save Preset" };
    juce::TextButton clearPendingMotifNotesButton { "Clear Pending" };

    // Capture from MPL (2026-09-21, user's own workflow): draw/play the
    // motive for real in MPL, resync it, pull its confirmed content
    // straight into pendingMotifNotes instead of re-entering it note by
    // note above - same instance/pattern-picker shape as
    // BlueprintSectionsContent's "Capture Current", including its "resync
    // it first" refusal.
    juce::Label captureHeaderLabel { {}, "Capture from MPL" };
    juce::ComboBox captureInstanceCombo;
    juce::ComboBox capturePatternCombo;
    juce::TextButton captureMotifPresetButton { "Capture Pattern" };

    juce::Label motifLibraryHeaderLabel { {}, "Motif Preset Library" };
    juce::ComboBox savedMotifPresetsCombo;
    juce::TextButton loadMotifPresetButton { "Load" };
    juce::TextButton removeMotifPresetButton { "Remove" };
    juce::TextEditor motifPresetsDisplay;

    std::vector<RhythmicRelationshipRoleSlot> pendingRoleSlots;
    std::vector<ArcPresetBreakpoint> pendingArcBreakpoints;
    std::vector<MotifNote> pendingMotifNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetLibraryContent)
};
