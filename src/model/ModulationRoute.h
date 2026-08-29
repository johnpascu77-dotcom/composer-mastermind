#pragma once

#include <string>
#include <vector>

// One MPL parameter a ModulationRoute can drive. Deliberately covers only
// the "whole pattern/instance" parameters MPL's External Control CC protocol
// exposes (CCMapping.h) - not the Target Step Editing family (CC 21/60-64),
// which writes specific note content rather than something a continuous or
// threshold-crossing curve sensibly modulates.
//
// Transpose/Rotation/Length are pattern-scoped (ModulationRoute::patternIndex
// selects which of the instance's 3 patterns) and continuous - re-evaluated
// and re-sent every bar, matching applyContinuousMelodicCurve's own cadence.
// Swing/Rate/ActivePattern/GridMode are instance-global (patternIndex is
// ignored for these) - Swing and Rate are continuous same as the pattern-
// scoped three; ActivePattern and GridMode are discrete/threshold, only
// re-sent on a state change, same restraint as applyCoherenceDivergenceIfDue
// and firePhraseChainIfDue.
// Inversion/Retrograde/M7 are pattern-scoped booleans, threshold-crossing.
enum class ModulationParameter
{
    Transpose,
    Rotation,
    Length,
    Swing,
    Inversion,
    Retrograde,
    M7,
    ActivePattern,
    GridMode,
    Rate
};

// How a route's continuous parameter (Transpose/Rotation/Length/Swing/Rate) gets its value
// and how often it's re-sent - orthogonal to which parameter is chosen, since the same
// parameter can be driven either way:
//   Bar (default): the existing arc-sampled behavior - outputMin/outputMax-mapped,
//     re-evaluated once per bar, matching applyContinuousMelodicCurve's own cadence.
//   Sequence (2026-08-27, "fragment sequencer" - docs/composer_mastermind_design.md):
//     steps through sequenceValues[] once per PATTERN LOOP CYCLE instead of once per
//     bar - real melodic sequencing (a short, Length-shrunk fragment restated at a new
//     pitch level every repetition), the device a Bach invention/fugue uses constantly.
//     Ignores arcDimension/outputMin/outputMax/threshold entirely. Meaningless for the
//     5 threshold-crossing parameters - see isContinuousModulationParameter. Bounded by
//     CCMapping::kPatternSteps - a loop cycle can never exceed one bar.
//   BarCycle (2026-08-27, phrase-cadence breathing): steps through sequenceValues[]
//     once per phraseLengthBars BARS instead of pattern steps - deliberately decoupled
//     from the one-bar ceiling above, for genuine multi-bar phrasing. A short
//     phraseLengthBars (2-4) gives phrase-internal breathing; set to a whole section's
//     durationBars, it gives a once-per-section cadence instead. Different routes are
//     meant to carry different phraseLengthBars per instance on purpose - that's what
//     keeps an ensemble's breathing from landing in mechanical lockstep every phrase.
//     Also ignores arcDimension/outputMin/outputMax/threshold; also meaningless for the
//     5 threshold-crossing parameters.
enum class ModulationDispatchMode { Bar, Sequence, BarCycle };

// A user-authored "arc dimension -> MPL parameter" wire, the general-purpose
// counterpart to the two hardcoded consumers (Energy/Tension -> Transpose,
// Density -> Swing, see ComposerCore::applyContinuousMelodicCurve/
// applyContinuousSwing) - letting any of the 5 arc dimensions drive any of
// MPL's parameters, on any instance/pattern, continuously.
//
// Distinct from ModulatorTarget (model/ModulatorTarget.h), which addresses a
// raw (midiChannel, ccNumber) pair meant for something OUTSIDE the Instance/
// MPL model entirely (a Bitwig modulator paired via Learn CC) - this struct
// targets a real Composer Mastermind Instance by id and a named MPL
// parameter, resolved and encoded through CCMapping's own per-parameter
// rules at dispatch time (see Router::routeContinuousParameter/
// routeThresholdParameter), the same targeting shape Mutation/ReservedValue
// already use.
struct ModulationRoute
{
    std::string id;
    std::string arcDimension;   // one of ArcSet::getArcNames()

    // Instance id, or "*" to broadcast the same route to every currently
    // registered instance - directly for the "too many instance/pattern/
    // parameter combinations to author by hand" scaling problem this
    // feature exists to solve.
    std::string targetInstance;

    int patternIndex = 0;       // 0-2; ignored for Swing/Rate/ActivePattern/GridMode
    ModulationParameter parameter = ModulationParameter::Transpose;

    // Continuous parameters: the arc's 0..1 sample is linearly mapped into
    // [outputMin, outputMax] (each parameter's own real domain, e.g.
    // Transpose +-CCMapping::kMaxTranspose) before being sent. Left at 0/0
    // means "use the parameter's full default domain" - resolved at
    // dispatch time, not authoring time, so a route saved before a domain
    // constant changes doesn't silently mean something else.
    float outputMin = 0.0f;
    float outputMax = 0.0f;

    // Threshold parameters: fires only when the (possibly inverted) arc
    // sample crosses this 0..1 point. ActivePattern instead bands 0..1 into
    // 3 states (see CCMapping::bandNormalized) - threshold is unused there.
    float threshold = 0.5f;

    bool invert = false;
    bool enabled = true;

    ModulationDispatchMode dispatchMode = ModulationDispatchMode::Bar;
    std::vector<float> sequenceValues;   // used when dispatchMode == Sequence or BarCycle
    int phraseLengthBars = 4;            // used only when dispatchMode == BarCycle
};

// String<->enum mapping for serialization/UI, same shape as CCMapping's own
// mutationOffsetForType - kept here (not in StateSerializer) so the UI combo
// (ui/ModulatorTargetView) and the serializer both read from one source of
// truth instead of duplicating the string table.
inline const char* modulationParameterToString(ModulationParameter parameter)
{
    switch (parameter)
    {
        case ModulationParameter::Transpose:     return "transpose";
        case ModulationParameter::Rotation:      return "rotation";
        case ModulationParameter::Length:        return "length";
        case ModulationParameter::Swing:         return "swing";
        case ModulationParameter::Inversion:     return "inversion";
        case ModulationParameter::Retrograde:    return "retrograde";
        case ModulationParameter::M7:            return "m7";
        case ModulationParameter::ActivePattern: return "activePattern";
        case ModulationParameter::GridMode:      return "gridMode";
        case ModulationParameter::Rate:          return "rate";
    }
    return "transpose";
}

inline bool modulationParameterFromString(const std::string& text, ModulationParameter& outParameter)
{
    if (text == "transpose")     { outParameter = ModulationParameter::Transpose;     return true; }
    if (text == "rotation")      { outParameter = ModulationParameter::Rotation;      return true; }
    if (text == "length")        { outParameter = ModulationParameter::Length;        return true; }
    if (text == "swing")         { outParameter = ModulationParameter::Swing;         return true; }
    if (text == "inversion")     { outParameter = ModulationParameter::Inversion;     return true; }
    if (text == "retrograde")    { outParameter = ModulationParameter::Retrograde;    return true; }
    if (text == "m7")            { outParameter = ModulationParameter::M7;            return true; }
    if (text == "activePattern") { outParameter = ModulationParameter::ActivePattern; return true; }
    if (text == "gridMode")      { outParameter = ModulationParameter::GridMode;      return true; }
    if (text == "rate")          { outParameter = ModulationParameter::Rate;          return true; }
    return false;
}

inline const char* modulationDispatchModeToString(ModulationDispatchMode mode)
{
    switch (mode)
    {
        case ModulationDispatchMode::Bar:      return "bar";
        case ModulationDispatchMode::Sequence: return "sequence";
        case ModulationDispatchMode::BarCycle: return "barCycle";
    }
    return "bar";
}

inline bool modulationDispatchModeFromString(const std::string& text, ModulationDispatchMode& outMode)
{
    if (text == "bar")      { outMode = ModulationDispatchMode::Bar;      return true; }
    if (text == "sequence") { outMode = ModulationDispatchMode::Sequence; return true; }
    if (text == "barCycle") { outMode = ModulationDispatchMode::BarCycle; return true; }
    return false;
}

// True for parameters that address the whole instance (patternIndex is
// ignored) rather than one of its 3 patterns.
inline bool isGlobalModulationParameter(ModulationParameter parameter)
{
    return parameter == ModulationParameter::Swing || parameter == ModulationParameter::ActivePattern
        || parameter == ModulationParameter::GridMode || parameter == ModulationParameter::Rate;
}

// True for the 5 continuously-dispatched parameters (re-evaluated and
// re-sent every bar); false for the 5 threshold-crossing ones (only sent on
// a state change). See ModulationParameter's own doc comment.
inline bool isContinuousModulationParameter(ModulationParameter parameter)
{
    return parameter == ModulationParameter::Transpose || parameter == ModulationParameter::Rotation
        || parameter == ModulationParameter::Length || parameter == ModulationParameter::Swing
        || parameter == ModulationParameter::Rate;
}
