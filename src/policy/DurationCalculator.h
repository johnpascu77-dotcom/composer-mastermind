#pragma once

#include <vector>

// Turns a target piece length (minutes) into bars, per the "Propose a
// Piece" generative flow (docs/composer_mastermind_design.md's v2 scope -
// see also policy/ArcShapeLibrary.h, the consumer of suggestedSectionBoundaries).
// Pure/JUCE-free, matching policy/BlueprintGenerator's own style - no side
// effects, deterministic. MC has never had minutes<->bars math anywhere
// before this (scheduling/StepClock.h is ppq-only and deliberately doesn't
// read tempo); this is genuinely new ground, not a rewire.
namespace DurationCalculator
{
    // One control point of a tempo curve: at this many minutes into the
    // piece, the tempo is this many BPM. A single point (atMinute == 0)
    // means a flat tempo for the whole piece; several points describe a
    // piecewise-linear ramp between them - the same shape a Bitwig Tempo
    // lane draws, just with far fewer points to hand-transcribe. MC never
    // drives tempo itself (see project_bitwig_transport_companion in
    // memory / the transport-companion design) - this is advisory output
    // for the user to redraw in Bitwig's own Tempo lane, not a live value.
    struct TempoPoint
    {
        double atMinute = 0.0;
        double bpm = 120.0;
    };

    // The result of one duration calculation.
    struct DurationPlan
    {
        // How many bars the target length actually is at the given tempo.
        int totalBars = 0;

        // The tempo spec, normalized (sorted, deduped, guaranteed to start
        // at minute 0) - ready to hand-transcribe into Bitwig's Tempo lane.
        std::vector<TempoPoint> tempoMap;

        // An even spread of bar boundaries across totalBars for however
        // many sections were asked for. A starting point for
        // ArcShapeLibrary, not a final structural decision -
        // BlueprintGenerator's own archetype classification is what
        // actually determines section shape from the resulting arc curve.
        // Always starts at 0 and ends at totalBars.
        std::vector<int> suggestedSectionBoundaries;
    };

    // Pure: no side effects. beatsPerBar and desiredSectionCount are
    // clamped to >= 1; targetMinutes is clamped to >= 0. tempoSpec may be
    // empty (defaults to a flat 120bpm) or contain any number of points in
    // any order - normalized internally (see DurationPlan::tempoMap).
    DurationPlan compute(double targetMinutes, int beatsPerBar,
                          std::vector<TempoPoint> tempoSpec, int desiredSectionCount);
}
