#include "PianoRollView.h"
#include "../policy/MonophonicOverlap.h"
#include <algorithm>

PianoRollView::PianoRollView()
{
}

void PianoRollView::setMode(Mode newMode)
{
    if (mode == newMode)
        return;

    mode = newMode;
    repaint();
}

void PianoRollView::setLanes(std::vector<PianoRollLane> newLanes)
{
    lanes = std::move(newLanes);
    gestureMode = GestureMode::None;
    gestureStep = -1;
    repaint();
}

void PianoRollView::setEditable(bool shouldBeEditable)
{
    editable = shouldBeEditable;
}

bool PianoRollView::canEditNow() const
{
    return editable && mode == Mode::Lane && lanes.size() == 1;
}

bool PianoRollView::isStepLocked(int stepIndex) const
{
    if (lanes.size() != 1)
        return false;

    const auto& locked = lanes[0].lockedStepIndices;
    return std::find(locked.begin(), locked.end(), stepIndex) != locked.end();
}

bool PianoRollView::hitTest(juce::Point<float> position, const PitchRange& range, int& outStepIndex,
                             int& outNote) const
{
    auto gridArea = getLocalBounds().toFloat();
    gridArea.removeFromLeft(kPitchLabelWidth);

    if (!gridArea.contains(position))
        return false;

    const int rowCount = juce::jmax(1, range.maxNote - range.minNote + 1);
    const float rowHeight = gridArea.getHeight() / static_cast<float>(rowCount);
    const float colWidth = gridArea.getWidth() / static_cast<float>(kSteps);

    outStepIndex = juce::jlimit(0, kSteps - 1,
                                 static_cast<int>((position.x - gridArea.getX()) / colWidth));
    const int row = juce::jlimit(0, rowCount - 1,
                                  static_cast<int>((position.y - gridArea.getY()) / rowHeight));
    outNote = range.maxNote - row;
    return true;
}

int PianoRollView::findNoteStartAtCell(const std::vector<StepSnapshot>& steps, int stepIndex, int note)
{
    for (int i = 0; i < (int) steps.size(); ++i)
    {
        if (!steps[(size_t) i].enabled || steps[(size_t) i].note != note)
            continue;

        const int duration = juce::jmax(1, steps[(size_t) i].duration);
        if (stepIndex >= i && stepIndex < i + duration)
            return i;
    }
    return -1;
}

bool PianoRollView::stepRangeOverlapsExisting(const std::vector<StepSnapshot>& steps, int startStep, int duration,
                                               int ignoredStep, int totalSteps)
{
    return MonophonicOverlap::rangeOverlapsExisting(steps, startStep, duration, ignoredStep, totalSteps);
}

int PianoRollView::maxNonOverlappingDuration(const std::vector<StepSnapshot>& steps, int startStep,
                                              int requestedDuration, int ignoredStep, int totalSteps)
{
    return MonophonicOverlap::maxNonOverlappingDuration(steps, startStep, requestedDuration, ignoredStep, totalSteps);
}

void PianoRollView::applyNoteEditSafely(int stepIndex, int note, int velocity, int duration, int ignoredStep)
{
    if (lanes.size() != 1 || stepIndex < 0 || stepIndex >= kSteps)
        return;

    auto& steps = lanes[0].steps;
    if (steps.size() < (size_t) kSteps)
        steps.resize((size_t) kSteps);

    const int clampedNote = juce::jlimit(0, 127, note);
    const int clampedVelocity = juce::jlimit(1, 127, velocity);
    const int requestedDuration = juce::jlimit(1, kSteps - stepIndex, duration);
    const int safeDuration = maxNonOverlappingDuration(steps, stepIndex, requestedDuration, ignoredStep, kSteps);

    if (stepRangeOverlapsExisting(steps, stepIndex, safeDuration, ignoredStep, kSteps))
        return; // still conflicts even at the smallest duration - refuse rather than overwrite

    steps[(size_t) stepIndex] = { true, clampedNote, clampedVelocity, safeDuration };

    repaint();
    if (onStepEdited)
        onStepEdited();
}

void PianoRollView::mouseDown(const juce::MouseEvent& event)
{
    if (!canEditNow())
    {
        if (!editable && !event.mods.isRightButtonDown())
        {
            exportDragStartPosition = event.position;
            exportDragStarted = false;
        }
        return;
    }

    gestureMode = GestureMode::None;
    gestureStep = -1;

    const auto range = computePitchRange();
    int stepIndex = -1, note = -1;
    if (!hitTest(event.position, range, stepIndex, note))
        return;

    auto& steps = lanes[0].steps;
    if (steps.size() < (size_t) kSteps)
        steps.resize((size_t) kSteps);

    if (event.mods.isRightButtonDown())
    {
        const int owningStep = findNoteStartAtCell(steps, stepIndex, note);
        if (owningStep < 0)
            return; // nothing exactly under the cursor to erase

        if (isStepLocked(owningStep))
            return; // protected - Ctrl+click to unlock before erasing

        steps[(size_t) owningStep].enabled = false;
        repaint();
        if (onStepEdited)
            onStepEdited();
        return;
    }

    if (event.mods.isCtrlDown())
    {
        const int lockTarget = findNoteStartAtCell(steps, stepIndex, note);
        if (lockTarget >= 0 && onLockToggleRequested)
            onLockToggleRequested(lockTarget);
        return; // never a create/resize/move gesture too - one or the other per click
    }

    const bool shiftDown = event.mods.isShiftDown();
    const bool altDown = event.mods.isAltDown();
    const int existingNoteStart = findNoteStartAtCell(steps, stepIndex, note);

    if ((shiftDown || altDown) && existingNoteStart >= 0)
    {
        if (isStepLocked(existingNoteStart))
            return; // protected - Ctrl+click to unlock before moving/repitching

        gestureStep = existingNoteStart;
        gestureNote = steps[(size_t) existingNoteStart].note;
        gestureDuration = juce::jmax(1, steps[(size_t) existingNoteStart].duration);
        gestureVelocity = steps[(size_t) existingNoteStart].velocity > 0 ? steps[(size_t) existingNoteStart].velocity : 100;

        if (altDown)
        {
            gestureMode = shiftDown ? GestureMode::MovePitchAndDuration : GestureMode::MovePitch;
        }
        else
        {
            gestureMode = GestureMode::MoveTime;
            gestureClickOffsetSteps = juce::jlimit(0, juce::jmax(0, gestureDuration - 1), stepIndex - existingNoteStart);
        }
        return;
    }

    // Plain click, or a modifier held over empty space - create/select/resize,
    // matching MPL's startMelodyDurationDrag fallback exactly.
    if (existingNoteStart >= 0)
    {
        if (isStepLocked(existingNoteStart))
            return; // protected - Ctrl+click to unlock before resizing

        gestureStep = existingNoteStart;
        gestureNote = steps[(size_t) existingNoteStart].note;
        gestureDuration = juce::jmax(1, steps[(size_t) existingNoteStart].duration);
        gestureVelocity = steps[(size_t) existingNoteStart].velocity > 0 ? steps[(size_t) existingNoteStart].velocity : 100;
    }
    else
    {
        if (stepRangeOverlapsExisting(steps, stepIndex, 1, -1, kSteps))
            return; // another note (any pitch) already sustains through this cell - refuse

        gestureStep = stepIndex;
        gestureNote = note;
        gestureDuration = 1;
        gestureVelocity = 100;
    }

    gestureMode = GestureMode::CreateOrResize;
    applyNoteEditSafely(gestureStep, gestureNote, gestureVelocity, gestureDuration, gestureStep);
}

void PianoRollView::mouseDrag(const juce::MouseEvent& event)
{
    if (!canEditNow())
    {
        if (!editable && !exportDragStarted && onRequestExportFile
            && event.position.getDistanceFrom(exportDragStartPosition) >= kDragThresholdPixels)
        {
            exportDragStarted = true; // one attempt per mouse-down, regardless of outcome below

            const auto file = onRequestExportFile();
            if (file != juce::File())
            {
                if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
                    container->performExternalDragDropOfFiles({ file.getFullPathName() }, false, this);
            }
        }
        return;
    }

    if (gestureMode == GestureMode::None || gestureStep < 0)
        return;

    const auto range = computePitchRange();
    int stepIndex = -1, note = -1;
    if (!hitTest(event.position, range, stepIndex, note))
        return;

    switch (gestureMode)
    {
        case GestureMode::CreateOrResize:
        case GestureMode::MovePitchAndDuration:
        {
            const int endStep = juce::jlimit(gestureStep, kSteps - 1, stepIndex);
            const int requestedDuration = juce::jlimit(1, kSteps - gestureStep, endStep - gestureStep + 1);
            const int pitchToUse = gestureMode == GestureMode::MovePitchAndDuration ? note : gestureNote;
            applyNoteEditSafely(gestureStep, pitchToUse, gestureVelocity, requestedDuration, gestureStep);
            break;
        }

        case GestureMode::MovePitch:
        {
            applyNoteEditSafely(gestureStep, note, gestureVelocity, gestureDuration, gestureStep);
            break;
        }

        case GestureMode::MoveTime:
        {
            const int maxStartStep = juce::jmax(0, kSteps - gestureDuration);
            const int requestedStartStep = juce::jlimit(0, maxStartStep, stepIndex - gestureClickOffsetSteps);
            if (requestedStartStep == gestureStep)
                break;

            auto& steps = lanes[0].steps;
            if (stepRangeOverlapsExisting(steps, requestedStartStep, gestureDuration, gestureStep, kSteps))
                break; // would collide with another note - refuse, note stays put

            steps[(size_t) gestureStep].enabled = false;
            gestureStep = requestedStartStep;
            steps[(size_t) gestureStep] = { true, gestureNote, gestureVelocity, gestureDuration };

            repaint();
            if (onStepEdited)
                onStepEdited();
            break;
        }

        case GestureMode::None:
            break;
    }
}

void PianoRollView::mouseUp(const juce::MouseEvent&)
{
    gestureMode = GestureMode::None;
    gestureStep = -1;
    exportDragStarted = false;
}

PianoRollView::PitchRange PianoRollView::computePitchRange() const
{
    int minNote = 128;
    int maxNote = -1;

    for (const auto& lane : lanes)
    {
        if (!lane.hasData)
            continue;

        for (const auto& step : lane.steps)
        {
            if (!step.enabled)
                continue;

            minNote = juce::jmin(minNote, step.note);
            maxNote = juce::jmax(maxNote, step.note);
        }
    }

    if (maxNote < minNote)
        return { 54, 78 }; // nothing to show yet - a sensible default 2-octave window around middle C

    minNote = juce::jmax(0, minNote - 2);
    maxNote = juce::jmin(127, maxNote + 2);

    if (maxNote - minNote < 11) // keep a minimum span so one held note doesn't render as one giant row
    {
        const int centre = (minNote + maxNote) / 2;
        minNote = juce::jmax(0, centre - 6);
        maxNote = juce::jmin(127, centre + 6);
    }

    return { minNote, maxNote };
}

juce::String PianoRollView::noteName(int midiNote)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int index = ((midiNote % 12) + 12) % 12;
    const int octave = (midiNote / 12) - 2; // MIDI note 60 = C3, Bitwig's convention (resolved 2026-08-22)
    return juce::String(names[index]) + juce::String(octave);
}

void PianoRollView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    auto area = getLocalBounds().toFloat();
    if (area.isEmpty())
        return;

    const auto range = computePitchRange();

    if (mode == Mode::Overlay)
    {
        std::vector<const PianoRollLane*> lanePointers;
        for (const auto& lane : lanes)
            lanePointers.push_back(&lane);

        paintLane(g, area, range, lanePointers, true);
        return;
    }

    if (lanes.empty())
    {
        g.setColour(juce::Colours::grey);
        g.drawText("No instances registered", getLocalBounds(), juce::Justification::centred);
        return;
    }

    const float bandHeight = area.getHeight() / static_cast<float>(lanes.size());
    for (const auto& lane : lanes)
    {
        auto bandArea = area.removeFromTop(bandHeight).reduced(0.0f, 1.5f);
        std::vector<const PianoRollLane*> single { &lane };
        paintLane(g, bandArea, range, single, true);
    }
}

void PianoRollView::paintLane(juce::Graphics& g, juce::Rectangle<float> laneArea, const PitchRange& range,
                               const std::vector<const PianoRollLane*>& lanesToDraw, bool drawPitchLabels)
{
    if (laneArea.getWidth() <= 0.0f || laneArea.getHeight() <= 0.0f)
        return;

    const auto backgroundColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    g.setColour(backgroundColour.brighter(0.05f));
    g.fillRect(laneArea);

    auto gridArea = laneArea;
    if (drawPitchLabels)
        gridArea.removeFromLeft(kPitchLabelWidth);

    const int rowCount = juce::jmax(1, range.maxNote - range.minNote + 1);
    const float rowHeight = gridArea.getHeight() / static_cast<float>(rowCount);
    const float colWidth = gridArea.getWidth() / static_cast<float>(kSteps);

    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    for (int row = 0; row <= rowCount; ++row)
    {
        const float y = gridArea.getY() + rowHeight * static_cast<float>(row);
        const int noteAtRow = range.maxNote - row;
        const bool isC = ((noteAtRow % 12) + 12) % 12 == 0;

        g.setColour(backgroundColour.contrasting(isC ? 0.35f : 0.15f));
        g.drawHorizontalLine(juce::roundToInt(y), gridArea.getX(), gridArea.getRight());

        if (drawPitchLabels && row < rowCount && isC)
        {
            g.setColour(juce::Colours::grey);
            g.drawText(noteName(noteAtRow),
                       juce::Rectangle<float>(laneArea.getX(), y, kPitchLabelWidth - 4.0f, rowHeight),
                       juce::Justification::centredRight);
        }
    }

    for (int step = 0; step <= kSteps; ++step)
    {
        const float x = gridArea.getX() + colWidth * static_cast<float>(step);
        g.setColour(backgroundColour.contrasting(step % 4 == 0 ? 0.35f : 0.12f));
        g.drawVerticalLine(juce::roundToInt(x), gridArea.getY(), gridArea.getBottom());
    }

    for (const auto* lane : lanesToDraw)
    {
        if (lane->activePatternNumber <= 0 || !lane->hasData)
            continue;

        for (int stepIndex = 0; stepIndex < (int) lane->steps.size() && stepIndex < kSteps; ++stepIndex)
        {
            const auto& step = lane->steps[(size_t) stepIndex];
            if (!step.enabled)
                continue;

            const int row = range.maxNote - step.note;
            if (row < 0 || row >= rowCount)
                continue; // outside the current display range - shouldn't happen, computePitchRange spans all notes

            const int durationSteps = juce::jmax(1, step.duration);
            const float x = gridArea.getX() + colWidth * static_cast<float>(stepIndex);
            const float y = gridArea.getY() + rowHeight * static_cast<float>(row);
            const float w = juce::jmax(1.0f, colWidth * static_cast<float>(durationSteps) - 1.0f);
            const float h = juce::jmax(2.0f, rowHeight - 2.0f);

            g.setColour(lane->colour.withAlpha(0.85f));
            g.fillRoundedRectangle(x + 0.5f, y + 1.0f, w, h, 2.0f);

            const bool locked = std::find(lane->lockedStepIndices.begin(), lane->lockedStepIndices.end(), stepIndex)
                                 != lane->lockedStepIndices.end();
            if (locked)
            {
                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.drawRoundedRectangle(x + 0.5f, y + 1.0f, w, h, 2.0f, 1.5f);
            }
        }
    }

    for (const auto* lane : lanesToDraw)
    {
        if (!lane->hasLivePlayhead)
            continue;

        const float x = gridArea.getX() + juce::jlimit(0.0f, 1.0f, lane->livePlayheadFraction) * gridArea.getWidth();
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawLine(x, gridArea.getY(), x, gridArea.getBottom(), 1.5f);
    }

    // Per-lane identity/status - only meaningful when this call is drawing
    // exactly one lane (Lane mode). Overlay mode's shared colour key lives
    // in PrimaryView's legend instead, since one label per lane wouldn't be
    // legible overlaid on a shared grid.
    if (lanesToDraw.size() == 1)
    {
        const auto* lane = lanesToDraw.front();
        g.setColour(lane->colour);
        g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Bold")));
        g.drawText(lane->displayName, laneArea.reduced(4.0f, 2.0f), juce::Justification::topLeft);

        if (lane->activePatternNumber <= 0)
        {
            g.setColour(juce::Colours::grey);
            g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Italic")));
            g.drawText("stopped", laneArea, juce::Justification::centred);
        }
        else if (!lane->hasData)
        {
            g.setColour(juce::Colours::grey);
            g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Italic")));
            g.drawText("no data yet - Resync Now in the Awareness tab", laneArea, juce::Justification::centred);
        }
    }
}
