#pragma once

#include "PluginProcessor.h"
#include "../model/Scene.h"
#include <memory>

// Minimal functional UI for driving MIDI Pattern Launcher instances over
// External Control CC while the policy/arc engine is still being built out.
// Panels:
//   1. Instance manager - register the MPL instances this conductor drives.
//   2. Global scene test - push Active Pattern / Grid Mode / Swing, with an
//      optional single-instance override to test differentiated scenes.
//   3. Mutation test - push a single per-pattern transpose/rotation/length/
//      inversion change to one instance.
//   4. Scene library - save/load/remove named scenes (built from panel 2's
//      current state) in-memory, plus export/import the whole snapshot
//      (instances + library) to a JSON file.
class ComposerMastermindAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                private juce::Button::Listener
{
public:
    explicit ComposerMastermindAudioProcessorEditor(ComposerMastermindAudioProcessor&);
    ~ComposerMastermindAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void buttonClicked(juce::Button* button) override;

    void addInstanceClicked();
    void removeInstanceClicked();
    void sendGlobalSceneClicked();
    void sendMutationClicked();
    void saveSceneClicked();
    void loadSceneClicked();
    void removeSceneClicked();
    void saveSnapshotToFileClicked();
    void loadSnapshotFromFileClicked();

    void refreshInstanceList();
    void refreshSceneList();

    // Builds a Scene from the current Global Scene panel controls (shared
    // by "Send Scene Now" and "Save Current Scene" so they can't drift).
    Scene buildSceneFromPanel(const std::string& id) const;

    ComposerMastermindAudioProcessor& processorRef;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Instance manager
    juce::Label instanceSectionLabel { {}, "Instances" };
    juce::TextEditor instanceIdInput;
    juce::Slider instanceChannelSlider;
    juce::Label instanceChannelLabel { {}, "Channel" };
    juce::TextButton addInstanceButton { "Add Instance" };
    juce::TextButton removeInstanceButton { "Remove" };
    juce::TextEditor instanceListDisplay;

    // Global scene test
    juce::Label sceneSectionLabel { {}, "Global Scene (sent to all enabled instances)" };
    juce::Slider activePatternSlider;
    juce::Label activePatternLabel { {}, "Active Pattern (0=stop, 1-3)" };
    juce::ComboBox gridModeCombo;
    juce::Label gridModeLabel { {}, "Grid Mode" };
    juce::Slider swingSlider;
    juce::Label swingLabel { {}, "Swing %" };
    juce::Slider durationBarsSlider;
    juce::Label durationBarsLabel { {}, "Duration Bars" };
    juce::ComboBox nextSceneCombo;
    juce::Label nextSceneLabel { {}, "Next Scene (chain)" };
    juce::ComboBox overrideInstanceCombo;
    juce::Label overrideInstanceLabel { {}, "Instance Override" };
    juce::ComboBox overrideGridModeCombo;
    juce::ComboBox overrideActivePatternCombo;
    juce::TextButton sendGlobalSceneButton { "Send Scene Now" };

    // Mutation test
    juce::Label mutationSectionLabel { {}, "Mutation (sent to one instance)" };
    juce::ComboBox mutationTargetCombo;
    juce::Label mutationTargetLabel { {}, "Target Instance" };
    juce::ComboBox mutationPatternCombo;
    juce::Label mutationPatternLabel { {}, "Pattern" };
    juce::ComboBox mutationTypeCombo;
    juce::Label mutationTypeLabel { {}, "Type" };
    juce::Slider mutationAmountSlider;
    juce::Label mutationAmountLabel { {}, "Amount" };
    juce::TextButton sendMutationButton { "Send Mutation" };

    // Scene library
    juce::Label librarySectionLabel { {}, "Scene Library" };
    juce::TextEditor sceneNameInput;
    juce::TextButton saveSceneButton { "Save Current Scene" };
    juce::ComboBox savedScenesCombo;
    juce::TextButton loadSceneButton { "Load and Send" };
    juce::TextButton removeSceneButton { "Remove" };
    juce::TextEditor sceneListDisplay;
    juce::TextButton saveSnapshotButton { "Save Snapshot To File..." };
    juce::TextButton loadSnapshotButton { "Load Snapshot From File..." };

    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ComposerMastermindAudioProcessorEditor)
};
