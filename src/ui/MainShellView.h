#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PrimaryView.h"
#include "EditorView.h"
#include "BlueprintArcCurveView.h"

class ComposerMastermindAudioProcessor;

// v1.2 Phase 0 (docs/composer_mastermind_design.md): the plugin's top-level
// shell. Owns three peer views, showing exactly one at a time: PrimaryView
// (default - the score/pattern view), BlueprintArcCurveView ("Compose" -
// the curve-based blueprint authoring screen, the user's own "cockpit"
// framing), and the existing, completely unchanged EditorView tabbed shell
// (Instances/Scenes/Mutations/Blueprint/Presets/Generate/Modulators/
// Awareness/Activity Log). Two independent toggle buttons pick between
// them rather than a single 3-way cycle, so either can be reached in one
// click. The "Expert" button is the explicit guarantee this phase exists
// to make good on (user's own requirement, 2026-08-22): nothing already
// working is ever at risk, because new capability is added as its own peer
// view here, never by touching PrimaryView or EditorView internally.
// EditorView keeps running its own refresh timer regardless of which view
// is visible, so its tabs are never stale when switched back to.
class MainShellView : public juce::Component
{
public:
    explicit MainShellView(ComposerMastermindAudioProcessor& processor);

    void resized() override;

    // Forwarded from PluginEditor after the DAW restores plugin state - see
    // EditorView::refreshAll's own comment for why this needs to be
    // explicit rather than waiting for the next timer tick.
    void refreshAll();

private:
    void toggleComposeClicked();
    void toggleExpertClicked();
    void updateVisibilityAndButtonText();
    void setShellStatus(const juce::String& text);

    enum class ShellMode { Primary, Compose, Expert };

    static constexpr int kToggleButtonHeight = 24;
    static constexpr int kToggleButtonWidth = 90;
    static constexpr int kMargin = 6;

    PrimaryView primaryView;
    BlueprintArcCurveView composeView;
    EditorView expertView;
    juce::TextButton composeToggleButton { "Compose" };
    juce::TextButton expertToggleButton;
    juce::Label shellStatusLabel;

    ShellMode mode = ShellMode::Primary;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainShellView)
};
