#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "InstanceView.h"
#include "SceneListView.h"
#include "DebugPanel.h"
#include "BlueprintView.h"
#include "PresetLibraryView.h"
#include "GenerateView.h"
#include "ModulatorTargetView.h"
#include "PatternAwarenessView.h"
#include "ActivityLogView.h"
#include "MilestoneView.h"

class ComposerMastermindAudioProcessor;

// The editor's tabbed shell: Instances / Scenes / Mutations / Blueprint /
// Presets / Generate / Modulators / Awareness / Activity Log / Milestones
// tabs, plus a status bar shared by all of them (each page reports through
// an injected callback rather than owning its own label). A periodic timer
// refreshes every page's displayed lists regardless of which tab is active,
// so state changed from one tab (e.g. a scene sent from Scenes) shows up on
// another (e.g. coherence on Instances) without wiring explicit cross-page
// callbacks between page components.
class EditorView : public juce::Component, private juce::Timer
{
public:
    explicit EditorView(ComposerMastermindAudioProcessor& processor);
    ~EditorView() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Called by PluginProcessor after the DAW restores plugin state, since
    // setStateInformation has no other way to tell an already-open editor
    // its displayed lists are now stale. The periodic timer would catch it
    // within its own interval regardless; this makes it immediate.
    void refreshAll();

private:
    void timerCallback() override;
    void setStatusText(const juce::String& text);

    ComposerMastermindAudioProcessor& processorRef;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    InstanceView instanceView;
    SceneListView sceneListView;
    DebugPanel debugPanel;
    BlueprintView blueprintView;
    PresetLibraryView presetLibraryView;
    GenerateView generateView;
    ModulatorTargetView modulatorTargetView;
    PatternAwarenessView patternAwarenessView;
    ActivityLogView activityLogView;
    MilestoneView milestoneView;

    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorView)
};
