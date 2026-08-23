#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <string>
#include <functional>
#include "../model/PatternSnapshot.h"
#include "../midi/CCMapping.h"

// One instance's data as the piano roll should draw it - not fetched here,
// just handed in via PianoRollView::setLanes. Kept deliberately dumb (no
// ComposerCore/processor reference) so this component stays reusable across
// v1.2's later phases (Setup mode's editable rendering, Live mode's
// reconstructed/scrolling rendering) per docs/score_timeline_ui_concept.md -
// only who populates it and how often differs, not the rendering itself.
struct PianoRollLane
{
    std::string instanceId;
    juce::String displayName;
    juce::Colour colour;

    // False when nothing has ever been dumped for this instance/pattern yet
    // (InstancePatternCache has no entry) - distinct from "stopped", so the
    // UI can tell "silent by choice" from "no data available."
    bool hasData = false;

    int activePatternNumber = 0; // 0 = stopped (CC 20 convention), else 1..3
    std::vector<StepSnapshot> steps;

    // 0 = binary, 1 = ternary - this instance's actual tracked grid mode
    // (routing/InstanceStateTracker), NOT read by this component itself (it
    // stays deliberately dumb, no ComposerCore reference - see the class
    // comment above). Populated by whoever builds the lane
    // (ui/PatternSetupView, ui/PrimaryView). Determines how many of this
    // lane's 16 raw steps are actually reachable/playable right now
    // (CCMapping::effectiveStepCount) and how the grid renders them - a
    // ternary lane's real 12 steps span the same physical bar-width a binary
    // lane's 16 do, they're just wider individually. See docs/
    // composer_mastermind_design.md's "Piano roll grid-mode mismatch fix".
    int gridMode = 0;

    // Step indices protected from automated engine writes (Setup mode's
    // Locked Notes, v1.2 Phase 2 - see state/LockedStepLibrary.h). Purely
    // for rendering here; PianoRollView never mutates lock state itself,
    // see onLockToggleRequested below.
    std::vector<int> lockedStepIndices;

    // Live sync (v1.2, docs/score_timeline_ui_concept.md): a moving playhead
    // reconstructed from host ppq + this instance's tracked grid mode/length/
    // pattern-launch phase (see ui/PrimaryView.cpp's reconstruction math) -
    // not real MIDI capture, MPL's audio-thread output never reaches this
    // plugin. 0..1, position across the rendered grid width regardless of
    // whether this lane's grid mode is binary(16) or ternary(12) steps, so
    // the line stays correctly placed either way. Never set by Setup mode.
    bool hasLivePlayhead = false;
    float livePlayheadFraction = 0.0f;
};

// v1.2 Phase 1+2 (docs/score_timeline_ui_concept.md): Overlay (all lanes on
// one grid, condensed-score style) or Lane (one grid per instance) mode,
// read-only by default. No playhead/live sync yet (Phase 2's Live mode) -
// steps are laid out by step index across MPL's fixed 16-step grid, not
// real bar/beat time. Real note names, MIDI note 60 = C3 (Bitwig's own
// convention, resolved 2026-08-22).
//
// Phase 2 addition: setEditable(true), combined with Mode::Lane and exactly
// one lane, turns this into Setup mode's authoring surface. Gesture set
// deliberately ported from MPL's own Melody view (Source/PluginEditor.cpp,
// user's own request 2026-08-22, so muscle memory transfers between the two
// plugins) rather than invented fresh:
//   - Left click empty cell: create a note (duration 1); drag right: resize.
//   - Left click an existing note, no modifiers: select it (no-op) and drag
//     right still resizes it from its own start.
//   - Right click: erase whatever note is exactly under the cursor.
//   - Shift + drag on an existing note: move it in time (pitch/duration fixed).
//   - Alt + drag on an existing note: change its pitch (position/duration fixed).
//   - Alt+Shift + drag on an existing note: change pitch and duration together
//     (start position fixed) - matches MPL's MoveExistingPitchAndDuration.
//   - Ctrl + click on an existing note: request a lock toggle (Locked Notes,
//     Composer-Mastermind-only, MPL has no equivalent, so Ctrl was free to
//     use here without colliding with the ported gesture set above).
// Every write goes through a refuse-on-conflict model (ported from MPL's
// melodyStepRangeOverlapsExistingNote/getMaxNonOverlappingMelodyDuration),
// never a destructive overwrite: a move/resize that would collide with
// another note clamps or is rejected outright, the same as MPL's own editor.
// Edits mutate the held lane data in place (read back via getLanes()) and
// fire onStepEdited so the owner (ui/PatternSetupView) can track dirty state
// and drive Commit - this component has no opinion on saving.
class PianoRollView : public juce::Component
{
public:
    enum class Mode { Overlay, Lane };

    PianoRollView();

    void setMode(Mode newMode);
    Mode getMode() const { return mode; }

    void setLanes(std::vector<PianoRollLane> newLanes);
    const std::vector<PianoRollLane>& getLanes() const { return lanes; }

    // Only takes effect for Mode::Lane with exactly one lane - Overlay mode
    // and multi-lane Lane mode stay read-only regardless (ambiguous which
    // lane a click would target).
    void setEditable(bool shouldBeEditable);
    bool isEditable() const { return editable; }

    std::function<void()> onStepEdited;

    // Fired on Ctrl+click on an existing note (stepIndex = that note's own
    // start). PianoRollView doesn't know about LockedStepLibrary or which
    // instance/pattern this lane even is on the Composer Mastermind side -
    // the owner (ui/PatternSetupView) resolves the toggle and hands back an
    // updated lane via setLanes.
    std::function<void(int stepIndex)> onLockToggleRequested;

    // Drag-out MIDI export (v1.2, docs/score_timeline_ui_concept.md).
    // Fired once a deliberate drag (past kDragThresholdPixels, not a click or
    // small wobble) starts in read-only (Live) mode - guards against exactly
    // the failure mode a commercial JUCE plugin's changelog documented
    // fixing (Stellarizer's own drag-guard bug, cited when this was
    // designed): a click/small pointer wobble must never trigger an export.
    // Only active when !editable, so it never fights Setup mode's own
    // gestures. Returns the temp .mid file to drag, or an invalid File{} if
    // export isn't allowed right now (still playing, nothing recorded yet) -
    // the owner (ui/PrimaryView) decides that and builds the file; this
    // component only detects the gesture and performs the OS-level drag.
    std::function<juce::File()> onRequestExportFile;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    struct PitchRange
    {
        int minNote = 60;
        int maxNote = 71;
    };

    PitchRange computePitchRange() const;
    void paintLane(juce::Graphics& g, juce::Rectangle<float> laneArea, const PitchRange& range,
                    const std::vector<const PianoRollLane*>& lanesToDraw, bool drawPitchLabels);
    static juce::String noteName(int midiNote);

    bool canEditNow() const;
    bool hitTest(juce::Point<float> position, const PitchRange& range, int& outStepIndex, int& outNote) const;

    // CCMapping::effectiveStepCount for the sole editable lane (canEditNow()
    // already requires exactly one) - the shared bound every edit gesture's
    // hit-testing/duration-clamping/overlap-refusal uses, so a note can
    // never be placed, dragged, or resized past what the instrument will
    // actually play in its current grid mode.
    int editableLaneSteps() const;

    // True if stepIndex is in the (sole, editable) lane's lockedStepIndices -
    // Locked Notes must be unlocked (Ctrl+click) before any gesture can
    // move/resize/erase them, matching the Stellarizer feature this was
    // inspired by ("unlock them whenever you want to edit them again").
    bool isStepLocked(int stepIndex) const;

    // Pitch-exact lookup - matches MPL's findMelodyNoteStartAtCell. Used for
    // gesture selection and erase, so only the note actually rendered under
    // the cursor is affected, never some other note merely sharing the same
    // time columns at a different pitch.
    static int findNoteStartAtCell(const std::vector<StepSnapshot>& steps, int stepIndex, int note);

    // Time-only (pitch-agnostic) overlap check/clamp - matches MPL's
    // melodyStepRangeOverlapsExistingNote/getMaxNonOverlappingMelodyDuration.
    // Used to refuse or clamp create/resize/move edits so two notes can never
    // occupy overlapping time ranges, matching MPL instances being monophonic.
    static bool stepRangeOverlapsExisting(const std::vector<StepSnapshot>& steps, int startStep, int duration,
                                           int ignoredStep, int totalSteps);
    static int maxNonOverlappingDuration(const std::vector<StepSnapshot>& steps, int startStep,
                                          int requestedDuration, int ignoredStep, int totalSteps);

    // Matches MPL's applyMelodyNoteEditSafely - clamps duration via
    // maxNonOverlappingDuration, then only writes if the result still
    // doesn't overlap (belt and suspenders, mirroring MPL's own structure).
    void applyNoteEditSafely(int stepIndex, int note, int velocity, int duration, int ignoredStep);

    enum class GestureMode { None, CreateOrResize, MoveTime, MovePitch, MovePitchAndDuration };

    Mode mode = Mode::Overlay;
    std::vector<PianoRollLane> lanes;
    bool editable = false;

    GestureMode gestureMode = GestureMode::None;
    int gestureStep = -1;
    int gestureNote = -1;
    int gestureDuration = 1;
    int gestureVelocity = 100;
    int gestureClickOffsetSteps = 0; // MoveTime only - preserves where within the note you grabbed it

    juce::Point<float> exportDragStartPosition;
    bool exportDragStarted = false;
    static constexpr float kDragThresholdPixels = 8.0f;

    static constexpr int kSteps = CCMapping::kPatternSteps;
    static constexpr float kPitchLabelWidth = 34.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollView)
};
