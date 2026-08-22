#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class ComposerMastermindAudioProcessor;

// Awareness tab: shows each instance's actual current pattern content, as
// last confirmed via a local IPC connection to MPL's ComposerBridgeIpcClient
// (state/InstancePatternCache) - not what Composer Mastermind last intended
// to send. Built over IPC rather than MPL's SysEx Composer Bridge because
// Bitwig doesn't deliver incoming SysEx to a hosted VST instrument at all
// (confirmed 2026-08-19). Manual "Resync Now" is the only way to refresh
// right now (no automatic poll-before-decision yet - there's no rule engine
// consuming freshness on a schedule to justify one); see
// model/PatternSnapshot.h and composer/PatternSyncServer.h for the mechanism
// this displays.
class PatternAwarenessView : public juce::Component
{
public:
    PatternAwarenessView(ComposerMastermindAudioProcessor& processor, std::function<void(const juce::String&)> setStatus);

    void paint(juce::Graphics&) override;
    void resized() override;

    // Repopulates the instance combo and redraws the cache display -
    // refreshed periodically by EditorView's timer, same as every other tab,
    // so a resync triggered elsewhere (or once the rule engine exists,
    // automatically) shows up here without extra wiring.
    void refreshAll();

private:
    void resyncClicked();
    void resyncAllClicked();

    ComposerMastermindAudioProcessor& processorRef;
    std::function<void(const juce::String&)> setStatus;

    juce::ComboBox instanceCombo;
    juce::Label instanceLabel { {}, "Instance" };
    juce::ComboBox patternCombo;
    juce::Label patternLabel { {}, "Pattern" };
    juce::TextButton resyncButton { "Resync Now" };

    // Requests every registered instance's every pattern (0..2, MPL's fixed
    // 3-pattern layout) in one click - added 2026-08-21 after the phrase-
    // chaining feature made "resync every pattern of every instance, not
    // just whichever one the engine already knew about" a real, frequent
    // setup step (previously up to 9 manual instance+pattern+click
    // combinations). Each request still only *asks*; results land
    // asynchronously via the same IPC responses a single Resync Now uses.
    juce::TextButton resyncAllButton { "Resync All" };

    juce::Label connectionStatusLabel;
    juce::TextEditor cacheDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternAwarenessView)
};
