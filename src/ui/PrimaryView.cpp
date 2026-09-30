#include "PrimaryView.h"
#include "../plugin/PluginProcessor.h"
#include "../midi/CCMapping.h"
#include "../state/CompositionBundleStore.h"
#include "InstanceColours.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>
#include <map>

namespace
{
    // Fast enough for a visually smooth playhead sweep without meaningful
    // cost - refreshFromCore only touches in-memory caches, no IO.
    constexpr int kTimerIntervalMs = 60;
    constexpr int kHeaderHeight = 30;
    constexpr int kColdStartRowHeight = 26;
    constexpr int kLegendHeight = 22;
    constexpr int kMargin = 8;

    // Repopulates a combo from a list of ids, preserving the current
    // selection by text if it's still present - same pattern every other
    // list-backed view in this codebase already uses (see e.g.
    // SceneListComponent.cpp's own copy).
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

    // A bar is always 4 quarter-notes here, matching MPL's own hardcoded
    // assumption (Source/PluginProcessor.cpp's gridStepLengthInPpq) - not a
    // general time-signature read, since MPL itself doesn't do one either.
    constexpr double kBarLengthInPpq = 4.0;

    // Mirrors MPL's inversion axis (Source/PluginProcessor.h's
    // inversionAxisNote) - not a coincidence that it matches our own C3=60
    // note-naming convention, both anchor on MIDI note 60.
    constexpr int kInversionAxisNote = 60;

    // Swing/notation banding (docs/arc_dimension_mapping_concept.md's "Swing
    // vs. notation" resolution). Worked out exactly from MPL's own swing
    // formula: all 3 of MPL's swing states now give a clean, notatable
    // long:short onset ratio - 0% (1:1, straight), 66.67% (2:1, true
    // triplet feel), and 100% (3:1, true dotted-eighth-plus-sixteenth
    // shuffle feel - v1.28.1: Shuffle was originally 75%, an in-between
    // 2.2:1 ratio indistinguishable from Triplet by ear or on paper, raised
    // to 100% once live testing confirmed that). MPL's own Swing knob
    // narrowed from a continuous slider to this 3-state choice (v1.28.0,
    // CCMapping::swingStateForPercent/swingPercentForState, shared here
    // instead of duplicating the crossover logic locally). The quantized-
    // for-notation export onset snaps to whichever of the 3 notation-legal
    // states is nearest, so Shuffle now notates as its own genuinely
    // distinct rhythm rather than folding into Triplet's; the as-performed
    // onset keeps whatever's actually tracked (normally one of the 3 legal
    // states already, but not force-validated, so this stays a real snap
    // for legacy content, not a passthrough).

    int gridStepCountFor(bool ternaryGridMode)
    {
        return ternaryGridMode ? 12 : 16;
    }

    // v1.29.0: rateState (0=Augmented/1=Normal/2=Diminished, defaults to
    // Normal) mirrors MPL's own real engine change - Rate divides MPL's
    // actual ppq-per-step value (PluginProcessor.cpp's
    // effectiveGridStepLengthInPpq), so every reconstruction here must apply
    // the same divisor or it silently desyncs the moment an instance's Rate
    // != Normal - see CCMapping::rateMultiplierForState.
    double gridStepLengthInPpqFor(bool ternaryGridMode, int rateState = 1)
    {
        return kBarLengthInPpq / static_cast<double>(gridStepCountFor(ternaryGridMode))
             / CCMapping::rateMultiplierForState(rateState);
    }

    // Mirrors MPL's own step clock (Source/PluginProcessor.cpp:
    // getGridStepCount/gridStepLengthInPpq/playbackStepIndex) so the
    // reconstructed playhead lands on the same grid position MPL is actually
    // playing from, not just a plausible guess. Returns a 0..1 fraction
    // across one grid-step-count's worth of columns (12 or 16), matching how
    // PianoRollView renders the grid regardless of loop length.
    float computeLivePlayheadFraction(bool ternaryGridMode, int trackedLength, double launchPpq, double currentPpq,
                                       int rateState = 1)
    {
        const int gridStepCount = gridStepCountFor(ternaryGridMode);
        const double gridStepLengthInPpq = gridStepLengthInPpqFor(ternaryGridMode, rateState);
        const int activeLoopLength = juce::jlimit(1, gridStepCount, trackedLength);

        const double stepsSinceLaunch = (currentPpq - launchPpq) / gridStepLengthInPpq;
        double loopPosition = std::fmod(stepsSinceLaunch, static_cast<double>(activeLoopLength));
        if (loopPosition < 0.0)
            loopPosition += static_cast<double>(activeLoopLength);

        return static_cast<float>(loopPosition / static_cast<double>(gridStepCount));
    }

    // Mirrors MPL's getRotatedSourceStepIndex exactly (Source/PluginProcessor.cpp) -
    // rotation is a lookup-time offset, never rewrites stored step data.
    // 2026-08-25: retrograde reflects the loop-relative playback position
    // before rotation's offset is subtracted, same as MPL's own function.
    int rotatedSourceStepIndex(int playbackStepIndex, int rotation, int loopLength, bool retrograde)
    {
        const int effectiveRotation = ((rotation % loopLength) + loopLength) % loopLength;
        const int reflectedStepIndex = retrograde ? (loopLength - 1 - playbackStepIndex) : playbackStepIndex;
        return ((reflectedStepIndex - effectiveRotation) % loopLength + loopLength) % loopLength;
    }

    // Mirrors MPL's applyPatternTransformsToNote exactly, including the
    // 2026-08-25 M7 addition: interval multiplication by 7 mod 12 on the
    // stored note's pitch class, applied ahead of Inversion.
    int transformedNote(int storedNote, bool m7, bool inverted, int transpose)
    {
        int result = storedNote;

        if (m7)
        {
            const int pitchClass = result % 12;
            const int m7PitchClass = (pitchClass * 7) % 12;
            result = result - pitchClass + m7PitchClass;
        }

        if (inverted)
            result = (kInversionAxisNote * 2) - result;
        result += transpose;
        return juce::jlimit(0, 127, result);
    }
}

PrimaryView::PrimaryView(ComposerMastermindAudioProcessor& processor)
    : processorRef(processor), patternSetupView(processor)
{
    addAndMakeVisible(liveSetupToggleButton);
    liveSetupToggleButton.onClick = [this] { liveSetupToggleClicked(); };

    addAndMakeVisible(modeToggleButton);
    modeToggleButton.setButtonText("Lane View");
    modeToggleButton.onClick = [this] { modeToggleClicked(); };

    addAndMakeVisible(exportVariantButton);
    exportVariantButton.setButtonText("Export: As Performed");
    exportVariantButton.onClick = [this] { exportVariantToggleClicked(); };

    addAndMakeVisible(resyncAllButton);
    resyncAllButton.onClick = [this] { resyncAllClicked(); };

    addAndMakeVisible(contentModeLabel);
    addAndMakeVisible(contentModeCombo);
    contentModeCombo.addItem("Generative", 1);
    contentModeCombo.addItem("Absolute", 2);
    contentModeCombo.onChange = [this] { contentModeChanged(); };

    addAndMakeVisible(blueprintCombo);
    blueprintCombo.setTextWhenNothingSelected("(pick blueprint)");

    addAndMakeVisible(loadBlueprintButton);
    loadBlueprintButton.onClick = [this] { loadBlueprintClicked(); };

    addAndMakeVisible(removeBlueprintButton);
    removeBlueprintButton.onClick = [this] { removeBlueprintClicked(); };

    addAndMakeVisible(primeButton);
    primeButton.onClick = [this] { primeForPlaybackClicked(); };

    addAndMakeVisible(loadScoreButton);
    loadScoreButton.onClick = [this] { loadScoreClicked(); };

    addAndMakeVisible(saveScoreButton);
    saveScoreButton.onClick = [this] { saveScoreClicked(); };

    addAndMakeVisible(generateScoreButton);
    generateScoreButton.onClick = [this] { generateScoreClicked(); };

    addAndMakeVisible(headerStatusLabel);
    headerStatusLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Italic")));
    headerStatusLabel.setMinimumHorizontalScale(1.0f);

    addAndMakeVisible(pianoRoll);
    pianoRoll.onRequestExportFile = [this] { return requestExportFile(); };
    addChildComponent(patternSetupView); // starts hidden - Live is the default mode

    refreshHeaderControls();
    refreshFromCore();
    startTimer(kTimerIntervalMs);
}

PrimaryView::~PrimaryView()
{
    stopTimer();
}

void PrimaryView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    if (!showingSetup)
        paintLegend(g, legendArea);
}

void PrimaryView::resized()
{
    auto area = getLocalBounds().reduced(kMargin);

    auto headerRow = area.removeFromTop(kHeaderHeight);
    liveSetupToggleButton.setBounds(headerRow.removeFromRight(90));
    headerRow.removeFromRight(6);
    modeToggleButton.setBounds(headerRow.removeFromRight(110));
    headerRow.removeFromRight(6);
    exportVariantButton.setBounds(headerRow.removeFromRight(150));

    area.removeFromTop(4);
    auto coldStartRow = area.removeFromTop(kColdStartRowHeight);
    resyncAllButton.setBounds(coldStartRow.removeFromLeft(90));
    coldStartRow.removeFromLeft(6);
    contentModeLabel.setBounds(coldStartRow.removeFromLeft(50));
    contentModeCombo.setBounds(coldStartRow.removeFromLeft(110));
    coldStartRow.removeFromLeft(6);
    blueprintCombo.setBounds(coldStartRow.removeFromLeft(160));
    coldStartRow.removeFromLeft(6);
    loadBlueprintButton.setBounds(coldStartRow.removeFromLeft(60));
    coldStartRow.removeFromLeft(6);
    removeBlueprintButton.setBounds(coldStartRow.removeFromLeft(70));
    coldStartRow.removeFromLeft(6);
    primeButton.setBounds(coldStartRow.removeFromLeft(140));
    coldStartRow.removeFromLeft(6);
    headerStatusLabel.setBounds(coldStartRow);

    area.removeFromTop(4);
    auto scoreFileRow = area.removeFromTop(kColdStartRowHeight);
    loadScoreButton.setBounds(scoreFileRow.removeFromLeft(110));
    scoreFileRow.removeFromLeft(6);
    saveScoreButton.setBounds(scoreFileRow.removeFromLeft(110));
    scoreFileRow.removeFromLeft(6);
    generateScoreButton.setBounds(scoreFileRow.removeFromLeft(130));

    area.removeFromTop(4);
    legendArea = area.removeFromTop(kLegendHeight);
    area.removeFromTop(4);

    pianoRoll.setBounds(area);
    patternSetupView.setBounds(area);
}

void PrimaryView::visibilityChanged()
{
    // Closes a real staleness gap (found live, 2026-08-23): refreshInstanceList()
    // used to only ever fire from liveSetupToggleClicked()'s own Live<->Setup
    // button, so switching away to the Expert tab (registering/removing
    // instances there) and back to Score View - while Score View happened to
    // already be showing Setup from an earlier click - left Setup mode's
    // instance list silently stale, with no re-trigger short of toggling
    // Live->Setup->Live again. MainShellView's Expert<->Score-View switch
    // calls setVisible() on this component directly, which reliably fires
    // this override - covers the gap without touching the inner toggle path
    // or the paused-while-hidden Live-mode timer at all.
    if (isVisible() && showingSetup)
        patternSetupView.refreshInstanceList();

    if (isVisible())
        refreshHeaderControls();
}

void PrimaryView::timerCallback()
{
    if (!showingSetup)
        refreshFromCore();
}

void PrimaryView::modeToggleClicked()
{
    const bool switchingToLane = pianoRoll.getMode() == PianoRollView::Mode::Overlay;
    pianoRoll.setMode(switchingToLane ? PianoRollView::Mode::Lane : PianoRollView::Mode::Overlay);
    modeToggleButton.setButtonText(switchingToLane ? "Overlay View" : "Lane View");
}

void PrimaryView::liveSetupToggleClicked()
{
    showingSetup = !showingSetup;
    updateLiveSetupVisibility();
}

void PrimaryView::updateLiveSetupVisibility()
{
    pianoRoll.setVisible(!showingSetup);
    modeToggleButton.setVisible(!showingSetup);
    exportVariantButton.setVisible(!showingSetup);
    patternSetupView.setVisible(showingSetup);
    liveSetupToggleButton.setButtonText(showingSetup ? "Live" : "Setup");

    if (showingSetup)
        patternSetupView.refreshInstanceList();
    else
        refreshFromCore();

    refreshHeaderControls();
    repaint();
}

void PrimaryView::refreshFromCore()
{
    auto& composerCore = processorRef.getComposerCore();
    const auto instances = composerCore.getInstanceRegistry().getAllInstances();
    auto& stateTracker = composerCore.getInstanceStateTracker();
    auto& cache = composerCore.getPatternSyncServer().getCache();

    // Playhead read once per tick, on the message thread - AudioProcessor::
    // getPlayHead() is safe to call off the audio thread (it just reads the
    // host's own lock-free published transport snapshot), so no new plumbing
    // through PluginProcessor::processBlock is needed for this.
    bool isPlaying = false;
    double currentPpq = 0.0;
    double lastBarStartPpq = 0.0;
    if (auto* playHead = processorRef.getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            isPlaying = position->getIsPlaying();
            if (const auto ppq = position->getPpqPosition())
                currentPpq = *ppq;
            if (const auto barStart = position->getPpqPositionOfLastBarStart())
                lastBarStartPpq = *barStart;
            if (const auto bpm = position->getBpm())
                if (*bpm > 0.0)
                    lastKnownTempoBpm = *bpm;
        }
    }

    if (!isPlaying)
    {
        // Re-anchor cleanly on the next play start rather than trusting
        // launch bars from a since-ended playthrough (see PrimaryView.h's
        // own comment on this bookkeeping's limitations). Deliberately does
        // NOT clear recordedNotes - that's the take drag-out export reads
        // once stopped; only a fresh *play start* below starts a new take.
        lastSeenActivePattern.clear();
        launchPpqByInstance.clear();
    }
    else if (!lastKnownIsPlaying)
    {
        // Transport just started - a new take begins, discarding whatever
        // was recorded last time (matching the resolved "stopped-transport
        // only" export design: the buffer only ever needs to hold the most
        // recent take, not a history of takes).
        recordedNotes.clear();
        lastRecordedAbsoluteStep.clear();
        takeStartPpq = lastBarStartPpq;
    }
    lastKnownIsPlaying = isPlaying;

    std::vector<PianoRollLane> newLanes;
    for (const auto& instance : instances)
    {
        if (!instance.enabled)
            continue;

        PianoRollLane lane;
        lane.instanceId = instance.id;
        lane.displayName = instance.name.empty() ? juce::String(instance.id) : juce::String(instance.name);
        lane.colour = instanceColourForIndex((int) newLanes.size());

        InstanceParameterState state;
        const bool haveState = stateTracker.getState(instance.id, state);
        if (haveState)
            lane.activePatternNumber = state.activePattern;
        lane.gridMode = state.gridMode; // 0 (binary) default is the right fallback if untracked yet

        CachedPattern cachedPattern;
        bool haveCachedPattern = false;
        if (lane.activePatternNumber >= 1 && lane.activePatternNumber <= CCMapping::kMaxPatterns)
        {
            haveCachedPattern = cache.get(instance.id, lane.activePatternNumber - 1, cachedPattern);
            if (haveCachedPattern)
            {
                lane.hasData = true;
                lane.steps = cachedPattern.snapshot.steps;
            }
        }

        if (isPlaying && haveState && lane.activePatternNumber >= 1
            && lane.activePatternNumber <= CCMapping::kMaxPatterns)
        {
            const auto seenIt = lastSeenActivePattern.find(instance.id);
            if (seenIt == lastSeenActivePattern.end() || seenIt->second != lane.activePatternNumber)
            {
                lastSeenActivePattern[instance.id] = lane.activePatternNumber;
                launchPpqByInstance[instance.id] = lastBarStartPpq;

                // A new launch means a new absolute-step numbering epoch -
                // the old lastRecordedAbsoluteStep value was relative to the
                // *previous* launch and would corrupt the catch-up range
                // below if left in place (either replaying already-recorded
                // steps under bogus new indices, or skipping a huge bogus
                // range). Drop it; the first step recorded under the new
                // launch starts fresh, same "first sight, no backfill"
                // policy as a fresh take.
                lastRecordedAbsoluteStep.erase(instance.id);
            }

            const auto launchIt = launchPpqByInstance.find(instance.id);
            if (launchIt != launchPpqByInstance.end())
            {
                const bool ternary = state.gridMode == 1;
                const int patternIndex = lane.activePatternNumber - 1;
                const auto& patternState = state.patterns[(size_t) patternIndex];

                lane.hasLivePlayhead = true;
                lane.livePlayheadFraction = computeLivePlayheadFraction(
                    ternary, patternState.length, launchIt->second, currentPpq, state.rate);

                // Recorder buffer: catch up on every step boundary crossed
                // since the last tick, not just the current one - a slow
                // poll (or a very fast tempo) must never silently skip a
                // note that already sounded between ticks.
                if (haveCachedPattern)
                {
                    const int gridStepCount = gridStepCountFor(ternary);
                    const double gridStepLengthInPpq = gridStepLengthInPpqFor(ternary, state.rate);
                    const int loopLength = juce::jlimit(1, gridStepCount, patternState.length);

                    const double stepsSinceLaunch = (currentPpq - launchIt->second) / gridStepLengthInPpq;
                    const int currentAbsoluteStep =
                        static_cast<int>(std::floor(stepsSinceLaunch + 1.0e-9));

                    auto recordedIt = lastRecordedAbsoluteStep.find(instance.id);
                    const int firstUnrecorded =
                        recordedIt == lastRecordedAbsoluteStep.end() ? currentAbsoluteStep : recordedIt->second + 1;

                    for (int absStep = firstUnrecorded; absStep <= currentAbsoluteStep; ++absStep)
                    {
                        const int playbackStepIndex = ((absStep % loopLength) + loopLength) % loopLength;
                        const int sourceStepIndex = rotatedSourceStepIndex(playbackStepIndex,
                                                                            patternState.rotation, loopLength,
                                                                            patternState.retrograde);

                        if (sourceStepIndex < 0
                            || (size_t) sourceStepIndex >= cachedPattern.snapshot.steps.size())
                            continue;

                        const auto& sourceStep = cachedPattern.snapshot.steps[(size_t) sourceStepIndex];
                        if (!sourceStep.enabled || sourceStep.velocity <= 0 || sourceStep.duration <= 0)
                            continue;

                        const bool isSwingEligibleStep = (playbackStepIndex % 2) == 1;

                        const float performedSwingPercent = state.swing;
                        const double performedSwingDelayPpq =
                            isSwingEligibleStep && performedSwingPercent > 0.0f
                                ? gridStepLengthInPpq * 0.5 * (static_cast<double>(performedSwingPercent) / 100.0)
                                : 0.0;

                        // Quantized-for-notation: snap to whichever of the 3 legal
                        // states (Off/Triplet/Shuffle) the real swing value is
                        // nearest - a genuinely swung passage should still notate
                        // as swung, not silently flatten to straight. v1.28.1:
                        // Shuffle now gets its own true 3:1 onset position here
                        // too, rather than folding into the triplet bucket -
                        // now that Shuffle is 100% (a genuine dotted-eighth-
                        // plus-sixteenth ratio, not the old in-between 75%),
                        // there's a real third rhythm worth notating distinctly.
                        // Only matters for legacy/arbitrary swing values now
                        // (any current scene/preset already carries an exactly
                        // legal percent, so this is normally a no-op snap).
                        const float quantizedSwingPercent =
                            CCMapping::swingPercentForState(CCMapping::swingStateForPercent(performedSwingPercent));
                        const double quantizedSwingDelayPpq =
                            isSwingEligibleStep && quantizedSwingPercent > 0.0f
                                ? gridStepLengthInPpq * 0.5 * (static_cast<double>(quantizedSwingPercent) / 100.0)
                                : 0.0;

                        const double stepPpq = launchIt->second + static_cast<double>(absStep) * gridStepLengthInPpq;

                        RecordedNote recordedNote;
                        recordedNote.instanceId = instance.id;
                        recordedNote.note = transformedNote(sourceStep.note, patternState.m7,
                                                             patternState.inversion, patternState.transpose);
                        recordedNote.velocity = juce::jlimit(1, 127, sourceStep.velocity);
                        recordedNote.quantizedOnsetPpq = stepPpq + quantizedSwingDelayPpq - takeStartPpq;
                        recordedNote.performedOnsetPpq = stepPpq + performedSwingDelayPpq - takeStartPpq;
                        recordedNote.durationPpq = static_cast<double>(sourceStep.duration) * gridStepLengthInPpq;
                        recordedNotes.push_back(recordedNote);
                    }

                    lastRecordedAbsoluteStep[instance.id] = currentAbsoluteStep;
                }
            }
        }

        newLanes.push_back(std::move(lane));
    }

    pianoRoll.setLanes(std::move(newLanes));
    refreshHeaderControls();
    repaint(legendArea);
}

void PrimaryView::exportVariantToggleClicked()
{
    exportVariant =
        exportVariant == ExportVariant::AsPerformed ? ExportVariant::ForNotation : ExportVariant::AsPerformed;
    exportVariantButton.setButtonText(exportVariant == ExportVariant::AsPerformed ? "Export: As Performed"
                                                                                   : "Export: For Notation");
}

// Drag-out MIDI export (v1.2, docs/score_timeline_ui_concept.md). Stopped-
// transport only (user's explicit call, 2026-08-22 - avoids any issue from
// dragging a buffer still being written to live) - returns an invalid File{}
// while playing or once the take has produced nothing yet, which
// PianoRollView treats as "don't start a drag." Format 1, one track per
// instance (preserves voice separation - MPL instances are monophonic, so
// this needs no polyphonic-merge logic), section/archetype labels as MIDI
// markers, tempo track. quantizedOnsetPpq is already snapped to a clean
// straight/triplet ratio at the point each RecordedNote was captured (see
// CCMapping::swingStateForPercent's use above), so "for notation" needs no
// separate quantization pass here - see PrimaryView.h's RecordedNote comment.
juce::File PrimaryView::requestExportFile()
{
    if (lastKnownIsPlaying || recordedNotes.empty())
        return {};

    auto& composerCore = processorRef.getComposerCore();
    const auto instances = composerCore.getInstanceRegistry().getAllInstances();

    constexpr int ticksPerQuarterNote = 960;
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(ticksPerQuarterNote);

    juce::MidiMessageSequence tempoTrack;
    tempoTrack.addEvent(juce::MidiMessage::textMetaEvent(3, "Composer Mastermind export"), 0.0);
    const int microsecondsPerQuarterNote =
        static_cast<int>(std::round(60000000.0 / juce::jmax(1.0, lastKnownTempoBpm)));
    tempoTrack.addEvent(juce::MidiMessage::tempoMetaEvent(microsecondsPerQuarterNote), 0.0);

    // Section markers - startBar is already relative to this take's own
    // start (ComposerCore's own bar-0-at-play-start numbering, the same
    // origin takeStartPpq anchors to), so no further offset is needed here.
    Blueprint blueprint;
    if (composerCore.getCurrentBlueprint(blueprint))
    {
        for (const auto& section : blueprint.sections)
        {
            const double markerPpq = static_cast<double>(section.startBar) * kBarLengthInPpq;
            if (markerPpq < 0.0)
                continue;

            const juce::String label = section.name.empty() ? juce::String(section.id) : juce::String(section.name);
            tempoTrack.addEvent(juce::MidiMessage::textMetaEvent(6, label), markerPpq * ticksPerQuarterNote);
        }
    }

    tempoTrack.updateMatchedPairs();
    midiFile.addTrack(tempoTrack);

    std::vector<std::string> instanceOrder;
    for (const auto& note : recordedNotes)
        if (std::find(instanceOrder.begin(), instanceOrder.end(), note.instanceId) == instanceOrder.end())
            instanceOrder.push_back(note.instanceId);

    for (const auto& instanceId : instanceOrder)
    {
        juce::String trackName = instanceId;
        for (const auto& instance : instances)
            if (instance.id == instanceId)
                trackName = instance.name.empty() ? juce::String(instance.id) : juce::String(instance.name);

        juce::MidiMessageSequence track;
        track.addEvent(juce::MidiMessage::textMetaEvent(3, trackName), 0.0);

        for (const auto& note : recordedNotes)
        {
            if (note.instanceId != instanceId)
                continue;

            const double onsetPpq =
                exportVariant == ExportVariant::ForNotation ? note.quantizedOnsetPpq : note.performedOnsetPpq;
            const double onsetTicks = juce::jmax(0.0, onsetPpq) * ticksPerQuarterNote;
            const double durationTicks = juce::jmax(1.0, note.durationPpq * ticksPerQuarterNote);

            track.addEvent(juce::MidiMessage::noteOn(1, note.note, static_cast<juce::uint8>(note.velocity)),
                           onsetTicks);
            track.addEvent(juce::MidiMessage::noteOff(1, note.note), onsetTicks + durationTicks);
        }

        track.updateMatchedPairs();
        midiFile.addTrack(track);
    }

    const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto file = tempDir.getChildFile("ComposerMastermind_"
                                            + juce::String(juce::Time::getCurrentTime().toMilliseconds()) + ".mid");

    juce::FileOutputStream stream(file);
    if (!stream.openedOk())
        return {};

    midiFile.writeTo(stream);
    stream.flush();

    return file;
}

// Cold-start header row (2026-08-23) - each handler below is a direct port
// of an existing Expert-UI equivalent (see PrimaryView.h's own comment on
// the row), surfaced here so the whole "open plugin -> Resync -> choose
// Content Mode -> Load a Blueprint" flow never needs to leave the Score View.
void PrimaryView::resyncAllClicked()
{
    auto& composerCore = processorRef.getComposerCore();
    const auto instances = composerCore.getInstanceRegistry().getAllInstances();

    if (instances.empty())
    {
        headerStatusLabel.setText("Resync All skipped: no instances registered", juce::dontSendNotification);
        return;
    }

    auto& server = composerCore.getPatternSyncServer();

    int requested = 0;
    int skippedNotConnected = 0;

    for (const auto& instance : instances)
    {
        const bool connected = server.isChannelConnected(instance.midiChannel);

        for (int patternIndex = 0; patternIndex < CCMapping::kMaxPatterns; ++patternIndex)
        {
            if (!connected)
            {
                ++skippedNotConnected;
                continue;
            }

            server.requestSync(instance.midiChannel, patternIndex);
            ++requested;
        }
    }

    juce::String message = "Resync All: requested " + juce::String(requested) + " pattern(s) across "
                            + juce::String((int) instances.size()) + " instance(s)";
    if (skippedNotConnected > 0)
        message << " (" << skippedNotConnected << " skipped - not connected yet)";

    headerStatusLabel.setText(message, juce::dontSendNotification);
}

void PrimaryView::contentModeChanged()
{
    const auto mode = contentModeCombo.getSelectedId() == 2 ? ContentMode::Absolute : ContentMode::Generative;
    processorRef.getComposerCore().setContentMode(mode);
    headerStatusLabel.setText(mode == ContentMode::Absolute
                                   ? "Content Mode: Absolute - captured sections play back verbatim and frozen"
                                   : "Content Mode: Generative - sections stamp from motif presets as usual",
                               juce::dontSendNotification);
}

void PrimaryView::loadBlueprintClicked()
{
    if (blueprintCombo.getSelectedId() <= 0)
    {
        headerStatusLabel.setText("Load blueprint skipped: no blueprint selected", juce::dontSendNotification);
        return;
    }

    const auto blueprintId = blueprintCombo.getText().toStdString();

    Blueprint blueprint;
    auto& composerCore = processorRef.getComposerCore();
    if (!composerCore.getBlueprintLibrary().getBlueprintById(blueprintId, blueprint))
    {
        headerStatusLabel.setText("Load blueprint failed: '" + juce::String(blueprintId) + "' not found",
                                   juce::dontSendNotification);
        return;
    }

    composerCore.setCurrentBlueprint(blueprint);

    // Populate immediately rather than requiring a second click - matches
    // the user's own description of the flow ("load a Blueprint... the
    // window gets populated"). Safe for the same reason the standalone
    // Prime button already is (ComposerCore.h's own comment: these CC/IPC
    // writes land instantly regardless of transport state); a Generative
    // section with no confirmed cached content yet simply stamps nothing,
    // which is the honest invitation to go draw it in Setup mode.
    composerCore.primeForPlayback();

    headerStatusLabel.setText("Loaded and primed '" + juce::String(blueprintId) + "' ("
                                   + juce::String((int) blueprint.sections.size()) + " section(s))",
                               juce::dontSendNotification);
    refreshFromCore();
}

void PrimaryView::removeBlueprintClicked()
{
    if (blueprintCombo.getSelectedId() <= 0)
    {
        headerStatusLabel.setText("Remove blueprint skipped: no blueprint selected", juce::dontSendNotification);
        return;
    }

    const auto blueprintId = blueprintCombo.getText().toStdString();
    processorRef.getComposerCore().getBlueprintLibrary().removeBlueprint(blueprintId);
    headerStatusLabel.setText("Removed blueprint '" + juce::String(blueprintId) + "' from library",
                               juce::dontSendNotification);
    refreshHeaderControls();
}

void PrimaryView::primeForPlaybackClicked()
{
    processorRef.getComposerCore().primeForPlayback();
    headerStatusLabel.setText("Primed first section for playback - patterns should already reflect it in MPL now",
                               juce::dontSendNotification);
    refreshFromCore();
}

void PrimaryView::loadScoreClicked()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Score",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.json");

    constexpr auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file == juce::File{})
            return;

        auto& composerCore = processorRef.getComposerCore();

        std::string errorMessage;
        if (!CompositionBundleStore::importBundle(file.loadFileAsString(), composerCore, errorMessage))
        {
            headerStatusLabel.setText("Load Score failed: " + juce::String(errorMessage), juce::dontSendNotification);
            return;
        }

        // importBundle already made this the current blueprint - prime
        // immediately too, same "load = ready to hear" framing as
        // loadBlueprintClicked above.
        composerCore.primeForPlayback();

        headerStatusLabel.setText("Loaded and primed score from " + file.getFullPathName(),
                                   juce::dontSendNotification);
        refreshHeaderControls();
        refreshFromCore();
    });
}

void PrimaryView::saveScoreClicked()
{
    auto& composerCore = processorRef.getComposerCore();

    Blueprint blueprint;
    if (!composerCore.getCurrentBlueprint(blueprint))
    {
        headerStatusLabel.setText("Save Score skipped: no score currently loaded", juce::dontSendNotification);
        return;
    }

    std::string errorMessage;
    const auto json = CompositionBundleStore::createBundle(composerCore, blueprint.id, errorMessage);
    if (json.isEmpty())
    {
        headerStatusLabel.setText("Save Score failed: " + juce::String(errorMessage), juce::dontSendNotification);
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Score",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(juce::String(blueprint.id) + ".json"),
        "*.json");

    constexpr auto chooserFlags = juce::FileBrowserComponent::saveMode
                                   | juce::FileBrowserComponent::canSelectFiles
                                   | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync(chooserFlags, [this, json, blueprintId = blueprint.id](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file == juce::File{})
            return;

        if (file.replaceWithText(json))
            headerStatusLabel.setText("Saved score '" + juce::String(blueprintId) + "' to " + file.getFullPathName(),
                                       juce::dontSendNotification);
        else
            headerStatusLabel.setText("Failed to write " + file.getFullPathName(), juce::dontSendNotification);
    });
}

void PrimaryView::generateScoreClicked()
{
    // Dev-machine-local path, same single-machine assumption every other
    // hardcoded path in this repo already makes (CLAUDE.md itself is full
    // of them) - this plugin and its source tree live on the same machine.
    const juce::File htmlFile(
        "C:\\Users\\Asus\\Documents\\JUCE\\Projects\\NewProject\\ComposerMastermind\\score-generator\\generate_score.html");

    if (!htmlFile.existsAsFile())
    {
        headerStatusLabel.setText("Generate Score failed: " + htmlFile.getFullPathName() + " not found",
                                   juce::dontSendNotification);
        return;
    }

    if (htmlFile.startAsProcess())
        headerStatusLabel.setText("Opened the score generator in your browser - Generate, then Load Score the download",
                                   juce::dontSendNotification);
    else
        headerStatusLabel.setText("Generate Score failed: couldn't open " + htmlFile.getFullPathName(),
                                   juce::dontSendNotification);
}

void PrimaryView::refreshHeaderControls()
{
    auto& composerCore = processorRef.getComposerCore();

    contentModeCombo.setSelectedId(
        composerCore.getContentMode() == ContentMode::Absolute ? 2 : 1, juce::dontSendNotification);

    std::vector<std::string> blueprintIds;
    for (const auto& blueprint : composerCore.getBlueprintLibrary().getAllBlueprints())
        blueprintIds.push_back(blueprint.id);
    repopulate(blueprintCombo, blueprintIds, false);
}

void PrimaryView::paintLegend(juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (area.isEmpty())
        return;

    auto remaining = area;
    g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));

    for (const auto& lane : pianoRoll.getLanes())
    {
        if (remaining.getWidth() < 20)
            break;

        constexpr int swatchSize = 10;
        auto swatchArea = remaining.removeFromLeft(swatchSize).withSizeKeepingCentre(swatchSize, swatchSize);
        g.setColour(lane.colour);
        g.fillRoundedRectangle(swatchArea.toFloat(), 2.0f);

        remaining.removeFromLeft(4);
        const int textWidth = juce::jmin(120, remaining.getWidth());
        g.setColour(getLookAndFeel().findColour(juce::Label::textColourId));
        g.drawText(lane.displayName, remaining.removeFromLeft(textWidth), juce::Justification::centredLeft);
        remaining.removeFromLeft(14);
    }
}
