#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "PianoRollView.h"
#include "PatternSetupView.h"

class ComposerMastermindAudioProcessor;

// One reconstructed note occurrence, accumulated into PrimaryView's recorder
// buffer as playback happens (v1.2 Phase 2's drag-out export,
// docs/score_timeline_ui_concept.md). Not real MIDI capture - see the
// reconstruction comment on launchPpqByInstance below. Onsets/duration are
// in ppq (quarter-note units) relative to the current take's own start
// (takeStartPpq), so a dragged-out MIDI file starts at its own beginning
// rather than wherever it happened to sit on the DAW's absolute timeline.
struct RecordedNote
{
    std::string instanceId;
    int note = 60;
    int velocity = 100;
    double performedOnsetPpq = 0.0; // real timing, including the actual swing value in effect
    double quantizedOnsetPpq = 0.0; // snapped to a clean straight/triplet ratio - for notation export
    double durationPpq = 0.0;
};

// v1.2 (docs/score_timeline_ui_concept.md): the new default top-level view.
// Two top-level modes, both reached from the header row: Live (Phase 1's
// Overlay/Lane piano roll, now with a reconstructed playhead and a
// drag-out MIDI export) and Setup (Phase 2 - editable single-lane
// authoring, see ui/PatternSetupView). Switching to Setup pauses Live
// mode's own refresh timer rather than let it keep repainting hidden
// content for no reason.
class PrimaryView : public juce::Component, private juce::Timer
{
public:
    explicit PrimaryView(ComposerMastermindAudioProcessor& processor);
    ~PrimaryView() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    enum class ExportVariant { AsPerformed, ForNotation };

    void timerCallback() override;
    void refreshFromCore();
    void modeToggleClicked();
    void liveSetupToggleClicked();
    void updateLiveSetupVisibility();
    void exportVariantToggleClicked();
    juce::File requestExportFile();
    void paintLegend(juce::Graphics& g, juce::Rectangle<int> area) const;

    // Cold-start header row (2026-08-23): the user's own framing of the
    // first-run flow - open the plugin, see this view empty, Resync, choose
    // Absolute/Generative, load a Blueprint - so that whole loop is reachable
    // without ever detouring into the Expert tab. Each handler is a direct
    // port of an existing Expert-UI equivalent (PatternAwarenessView::
    // resyncAllClicked, SceneListComponent::contentModeChanged,
    // BlueprintSectionsContent::loadBlueprintClicked/removeBlueprintClicked/
    // primeForPlaybackClicked) - no new backend logic, just surfaced here too.
    void resyncAllClicked();
    void contentModeChanged();
    void loadBlueprintClicked();
    void removeBlueprintClicked();
    void primeForPlaybackClicked();
    void refreshHeaderControls();

    // Score file row (2026-08-27, user's own request): the simple/Score
    // View is meant to be a self-sufficient "score player" - load a
    // composition bundle JSON, hear it, done - without ever needing Expert
    // mode. Deliberately NOT the Scenes tab's "Load/Save Snapshot" buttons
    // (StateSnapshotStore, a whole-session round-trip) - those look similar
    // but are the wrong tool here, which is exactly the confusion that
    // prompted this: CompositionBundleStore is the single-piece format this
    // whole "composing by numbers" thread has been building, already used
    // by BlueprintSectionsContent's own Export/Import, just not reachable
    // from this view before now.
    void loadScoreClicked();
    void saveScoreClicked();

    // Opens score-generator/generate_score.html (2026-09-21) in the
    // system's default browser - the offline, no-server JS port of
    // score-generator/generate_score.py (see that file's own module
    // docstring for exactly which C++ files it mirrors). Generates a
    // composition-bundle JSON entirely client-side and downloads it; the
    // user then Loads it here same as any other score. Dev-machine-local
    // path (kGenerateScoreHtmlPath), same single-machine assumption this
    // whole repo already makes (CLAUDE.md's own hardcoded paths).
    void generateScoreClicked();

    ComposerMastermindAudioProcessor& processorRef;

    juce::TextButton liveSetupToggleButton { "Setup" };
    juce::TextButton modeToggleButton;
    juce::TextButton exportVariantButton;

    juce::TextButton resyncAllButton { "Resync All" };
    juce::Label contentModeLabel { {}, "Content" };
    juce::ComboBox contentModeCombo;
    juce::ComboBox blueprintCombo;
    juce::TextButton loadBlueprintButton { "Load" };
    juce::TextButton removeBlueprintButton { "Remove" };
    juce::TextButton primeButton { "Prime for Playback" };
    juce::TextButton loadScoreButton { "Load Score..." };
    juce::TextButton saveScoreButton { "Save Score..." };
    juce::TextButton generateScoreButton { "Generate Score..." };
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Label headerStatusLabel;

    PianoRollView pianoRoll;
    PatternSetupView patternSetupView;

    bool showingSetup = false;
    ExportVariant exportVariant = ExportVariant::AsPerformed;

    // Live sync's phase-anchor bookkeeping (v1.2, docs/score_timeline_ui_concept.md):
    // reconstruction, not MIDI capture - MPL's audio-thread note output never
    // reaches this plugin, so the playhead is derived from host ppq plus each
    // instance's own tracked grid mode/length and *when Composer Mastermind
    // itself last saw that instance's Active Pattern change*. Exact from the
    // moment this plugin started watching (or itself caused the switch);
    // known limitation for a pattern that was already looping before that,
    // if its tracked Length doesn't evenly divide its grid step count (12 or
    // 16) - MPL exposes no "which bar did you launch this" field to recover
    // the true historical phase from, so this is the closest honest anchor
    // available. Cleared entirely on transport stop, so a fresh play start
    // always re-anchors cleanly rather than trusting stale bar numbers.
    std::map<std::string, int> lastSeenActivePattern;
    std::map<std::string, double> launchPpqByInstance;

    // The recorder buffer (v1.2 Phase 2): accumulates every step actually
    // reconstructed as sounding, timestamped relative to the current take's
    // own start - the "whole piece so far" record drag-out export bounces.
    // Resets on every fresh play start (a new take), not on stop, so the
    // export stays available once transport stops.
    std::vector<RecordedNote> recordedNotes;
    std::map<std::string, int> lastRecordedAbsoluteStep;
    bool lastKnownIsPlaying = false;
    double takeStartPpq = 0.0;
    double lastKnownTempoBpm = 120.0;

    juce::Rectangle<int> legendArea; // cached from resized(), read by paint()'s legend

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PrimaryView)
};
