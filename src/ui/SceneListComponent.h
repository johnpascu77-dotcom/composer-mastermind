#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include "../model/Scene.h"

class ComposerMastermindAudioProcessor;

// Scenes tab: build/send a test scene and save/load/remove named scenes in
// the library, plus export/import the whole snapshot to a JSON file.
// Combined onto one tab since both halves are "scene" concerns and share
// the scene-building controls.
//
// Targets/overrides are genuine pending lists (2026-08-22), not a single
// implicit "all instances, one override" shape - the earlier version always
// targeted every registered instance and could hold at most one
// SceneInstanceOverride, so loading a real multi-override scene (e.g.
// anything authored via the MCP bridge) and resaving would have silently
// dropped every override but one. pendingTargets is seeded once at
// construction with every currently-registered instance (matching the old
// default behaviour for anyone who doesn't customize anything); "Target
// All" re-seeds it from the live registry on demand (e.g. after adding a
// new instance), "Add Target" adds one at a time, "Clear Pending" empties
// both lists. "Load" now populates the whole panel (including both pending
// lists) from a saved scene for editing, rather than immediately sending it
// - "Send Scene Now" already covers "play it right now," separately.
class SceneListComponent : public juce::Component
{
public:
    SceneListComponent(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    // Repopulates the instance/override/target combos (from InstanceRegistry)
    // and the saved-scene/next-scene combos + list text (from SceneLibrary).
    // Never touches pendingTargets/pendingOverrides - those only change via
    // explicit user action (Add/Target All/Clear/Load), the same care
    // BlueprintSectionsContent's workingSections already takes.
    void refreshAll();

    // Total height this component needs - see ui/SceneListView, its
    // juce::Viewport host (this form outgrew the tab's available height
    // once target/override became real pending lists, same reason
    // BlueprintSectionsContent needed one).
    static int getPreferredHeight();

private:
    void sendGlobalSceneClicked();
    void saveSceneClicked();
    void loadSceneClicked();
    void removeSceneClicked();
    void saveSnapshotToFileClicked();
    void loadSnapshotFromFileClicked();
    void contentModeChanged();
    void addTargetClicked();
    void targetAllClicked();
    void addOverrideClicked();
    void clearPendingClicked();
    void refreshPendingDisplay();

    // Builds a Scene from the current panel controls, including the pending
    // target/override lists (shared by "Send Scene Now" and "Save Current
    // Scene" so they can't drift).
    Scene buildSceneFromPanel(const std::string& id) const;

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Label sceneSectionLabel { {}, "Global Scene" };
    juce::Slider activePatternSlider;
    juce::Label activePatternLabel { {}, "Active Pattern (0=stop, 1-3)" };
    juce::ComboBox gridModeCombo;
    juce::Label gridModeLabel { {}, "Grid Mode" };
    juce::ComboBox swingCombo;
    juce::Label swingLabel { {}, "Swing" };
    juce::Slider durationBarsSlider;
    juce::Label durationBarsLabel { {}, "Duration Bars" };
    juce::ComboBox nextSceneCombo;
    juce::Label nextSceneLabel { {}, "Next Scene (chain)" };

    juce::ComboBox targetInstanceCombo;
    juce::Label targetInstanceLabel { {}, "Targets" };
    juce::TextButton addTargetButton { "Add Target" };
    juce::TextButton targetAllButton { "Target All" };
    juce::Label pendingTargetsLabel;

    juce::ComboBox overrideInstanceCombo;
    juce::Label overrideInstanceLabel { {}, "Override" };
    juce::ComboBox overrideGridModeCombo;
    juce::ComboBox overrideActivePatternCombo;
    juce::TextButton addOverrideButton { "Add Override" };
    juce::Label pendingOverridesLabel;

    juce::TextButton sendGlobalSceneButton { "Send Scene Now" };
    juce::TextButton clearPendingButton { "Clear Pending" };

    juce::Label librarySectionLabel { {}, "Scene Library" };
    juce::TextEditor sceneNameInput;
    juce::TextButton saveSceneButton { "Save Current Scene" };
    juce::ComboBox savedScenesCombo;
    juce::TextButton loadSceneButton { "Load" };
    juce::TextButton removeSceneButton { "Remove" };
    juce::TextEditor sceneListDisplay;
    juce::TextButton saveSnapshotButton { "Save Snapshot To File..." };
    juce::TextButton loadSnapshotButton { "Load Snapshot From File..." };

    // Global content-mode switch (2026-08-23, user's own request - see
    // composer/ComposerCore.h's ContentMode doc comment): Generative (the
    // long-standing default) vs Absolute, which lets a section with captured
    // content (ui/BlueprintSectionsContent's "Capture Current") play back
    // verbatim and frozen instead. Placed here, next to Load Snapshot, since
    // "load a file and get exactly what's in it" is the whole point of
    // Absolute mode.
    juce::Label contentModeLabel { {}, "Content Mode" };
    juce::ComboBox contentModeCombo;

    std::vector<std::string> pendingTargets;
    std::vector<SceneInstanceOverride> pendingOverrides;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SceneListComponent)
};
