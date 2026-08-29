#pragma once

#include <string>
#include <cmath>
#include <algorithm>
#include <array>

// Maps Composer Mastermind's scene/mutation model onto the External Control
// CC protocol implemented by MIDI Pattern Launcher (see
// ..\NewProject\Source\PluginProcessor.cpp, handleExternalControlCC).
//
// MPL decodes each CC 0..127 back into a plain value with:
//     plain = min + round((ccValue / 127) * (max - min))
// so encoding here must be the exact inverse of that, per target parameter.
namespace CCMapping
{
    constexpr int kActivePattern = 20;
    constexpr int kTargetPattern = 21; // live in MPL since 2026-08-18 ("Option A") - see docs/routing_policy_v0_1.md
    constexpr int kGridMode = 22;
    constexpr int kRate = 23; // v1.29.0 - Augmented/Normal/Diminished, global per-instance, mirrors kSwing below
    constexpr int kSwing = 24;
    constexpr int kTargetStep = 60;
    constexpr int kTargetNote = 61;
    constexpr int kTargetVelocity = 62;
    constexpr int kTargetDuration = 63;
    constexpr int kTargetEnabled = 64;

    constexpr int kMaxPatterns = 3;          // MPL numPatterns
    constexpr int kPatternSteps = 16;        // MPL patternLength
    constexpr int kMinPatternLoopLength = 1; // MPL minPatternLoopLength
    constexpr int kTernaryGridSteps = 12;    // MPL getGridStepCount()'s ternary branch - see PluginProcessor.cpp
    constexpr int kMaxTranspose = 48;

    // v1.28.0: MPL's own Swing knob narrowed from a continuous slider to a
    // 3-state choice (CC 24 now decodes 0/1/2 via scaleInt, not a percent) -
    // a continuous value almost always landed in the muddy middle between
    // MPL's only two notation-clean ratios (0% straight, 66.67% true
    // triplet), reading as sloppy rather than musical.
    //
    // v1.28.1: Shuffle's own percent was originally 75% (the old
    // continuous slider's historical maximum), but live testing found it
    // indistinguishable from Triplet by ear - 75% only reaches a 2.2:1
    // long:short onset ratio, barely different from Triplet's 2:1. Raised
    // to 100% instead: the swing delay formula's true mathematical ceiling
    // (the swung note lands exactly at the midpoint between its
    // neighbours), which works out to a genuinely different 3:1 ratio - the
    // real dotted-eighth-plus-sixteenth shuffle feel MPL couldn't reach
    // before. 100% cannot be exceeded without inverting step order, so this
    // is the actual boundary, not an arbitrary round number. kMaxSwing is
    // now just an alias for Shuffle's percent - the two were only ever
    // different by historical accident.
    //
    // Composer Mastermind's own model still stores swing as a plain percent
    // (unchanged field type everywhere - Scene/Preset/InstanceParameterState
    // etc.), just constrained to these two named values plus 0 at the point
    // it's actually turned into a CC (encodeSwing) or authored from a
    // continuous arc (see ComposerCore::applyContinuousSwing) - any other
    // percent an old scene/preset might still carry gets snapped to
    // whichever of the three is nearest, not rejected.
    constexpr float kTripletSwingPercent = 200.0f / 3.0f; // 66.67%, exact 2:1 ratio
    constexpr float kShuffleSwingPercent = 100.0f;
    constexpr float kMaxSwing = kShuffleSwingPercent;

    enum class MutationOffset
    {
        Transpose = 0,
        Rotation = 1,
        Length = 2,
        Inversion = 3,
        Retrograde = 4, // live in MPL since 2026-08-25 - see docs/composer_mastermind_design.md
        M7 = 5          // interval multiplication by 7 mod 12, live in MPL since 2026-08-25
    };

    // Returns -1 for out-of-range pattern indices (MPL only exposes 3 patterns).
    inline int patternBaseCC(int patternIndex)
    {
        switch (patternIndex)
        {
            case 0: return 30;
            case 1: return 40;
            case 2: return 50;
            default: return -1;
        }
    }

    inline int encodeInt(int value, int minValue, int maxValue)
    {
        if (maxValue <= minValue)
            return 0;

        const double normalised = static_cast<double>(std::clamp(value, minValue, maxValue) - minValue)
                                   / static_cast<double>(maxValue - minValue);
        return std::clamp(static_cast<int>(std::lround(normalised * 127.0)), 0, 127);
    }

    inline int encodeFloat(float value, float minValue, float maxValue)
    {
        if (maxValue <= minValue)
            return 0;

        const float normalised = (std::clamp(value, minValue, maxValue) - minValue) / (maxValue - minValue);
        return std::clamp(static_cast<int>(std::lround(normalised * 127.0f)), 0, 127);
    }

    inline int encodeActivePattern(int pattern) // 0 = stopped, 1..3 = pattern
    {
        return encodeInt(pattern, 0, kMaxPatterns);
    }

    inline int encodeGridMode(int mode) // 0 = binary, 1 = ternary
    {
        return encodeInt(mode, 0, 1);
    }

    // Mirrors MPL's own getGridStepCount() (PluginProcessor.cpp): in Ternary
    // grid mode, playback only ever reads the first 12 raw steps regardless
    // of Length/pattern-loop settings - content authored or rendered past
    // that boundary is either silently dropped (writes) or genuinely
    // unplayable (UI). Every write path AND every UI rendering/edit-boundary
    // path must bound itself to this, not to kPatternSteps blindly - shared
    // here (rather than duplicated per call site) so policy/MotifEngine.cpp
    // and ui/PianoRollView.cpp can't drift out of agreement with each other.
    inline int effectiveStepCount(int gridMode)
    {
        return gridMode == 1 ? kTernaryGridSteps : kPatternSteps;
    }

    // Nearest of the 3 legal swing states (0/1=Triplet/2=Shuffle) for an
    // arbitrary percent - used both to encode a CC (encodeSwing below) and
    // to let ComposerCore's continuous Density consumer band a 0..1 arc
    // value onto one of the three (see swingStateForNormalized).
    inline int swingStateForPercent(float percent)
    {
        const float distanceToOff = std::abs(percent - 0.0f);
        const float distanceToTriplet = std::abs(percent - kTripletSwingPercent);
        const float distanceToShuffle = std::abs(percent - kShuffleSwingPercent);

        if (distanceToOff <= distanceToTriplet && distanceToOff <= distanceToShuffle)
            return 0;

        return distanceToTriplet <= distanceToShuffle ? 1 : 2;
    }

    // Bands a 0..1 value evenly into one of numBands states (0..numBands-1) -
    // the general form of the even-thirds banding technique Density's swing
    // mapping and Complexity's phrase-chain target both already used as
    // separate, duplicated logic. Shared here so ModulationRoute's
    // ActivePattern dispatch (Router::routeThresholdParameter) can reuse it
    // too instead of a third copy. numBands <= 1 always returns 0.
    inline int bandNormalized(float normalised0to1, int numBands)
    {
        if (numBands <= 1)
            return 0;

        const float clamped = std::clamp(normalised0to1, 0.0f, 1.0f);
        const int band = static_cast<int>(clamped * static_cast<float>(numBands));
        return std::clamp(band, 0, numBands - 1);
    }

    // Bands a 0..1 arc value into one of the 3 swing states, evenly - the
    // "impulse target" side of Density's mapping (see
    // ComposerCore::applyContinuousSwing).
    inline int swingStateForNormalized(float normalised0to1)
    {
        return bandNormalized(normalised0to1, 3);
    }

    inline float swingPercentForState(int state)
    {
        switch (state)
        {
            case 1:  return kTripletSwingPercent;
            case 2:  return kShuffleSwingPercent;
            default: return 0.0f;
        }
    }

    inline int encodeSwing(float swingPercent)
    {
        return encodeInt(swingStateForPercent(swingPercent), 0, 2);
    }

    // v1.29.0: Rate - a real classical augmentation/diminution device (the
    // whole active pattern restated in longer or shorter note values, onsets
    // AND durations both scaling together - see MPL's own
    // getGlobalRateMultiplier() in PluginProcessor.cpp). Global per-instance
    // like Swing (CC 23, fixed - not per-pattern), but unlike Swing the
    // model already stores it as a clean 3-state int (InstanceParameterState
    // ::rate: 0=Augmented/1=Normal/2=Diminished), so there's no percent
    // domain to snap - encodeRate is a direct pass-through to encodeInt.
    inline int encodeRate(int state)
    {
        return encodeInt(state, 0, 2);
    }

    // Bands a 0..1 arc value into one of the 3 rate states, evenly - the
    // continuous-modulation-route counterpart to encodeRate, mirroring
    // swingStateForNormalized above.
    inline int rateStateForNormalized(float normalised0to1)
    {
        return bandNormalized(normalised0to1, 3);
    }

    // 0.5x/1.0x/2.0x - mirrors MPL's own getGlobalRateMultiplier() exactly.
    // Used only by StepClock/PrimaryView's playback-time reconstructions to
    // stay in sync with MPL's real (rate-scaled) step clock - not needed for
    // the CC encode path itself, which only ever deals in the 0/1/2 state.
    inline float rateMultiplierForState(int state)
    {
        switch (state)
        {
            case 0:  return 0.5f;  // Augmented - restated in longer values, slower
            case 2:  return 2.0f;  // Diminished - restated in shorter values, faster
            default: return 1.0f;  // Normal
        }
    }

    // Maps a "rate" Mutation's amount ([-1, 1], clamped) to an absolute
    // target state (0=Augmented/1=Normal/2=Diminished) - see Mutation.h's
    // own comment for why "rate" is an absolute-set exception rather than a
    // delta like Rotation/Transpose.
    inline int rateStateFromMutationAmount(int amount)
    {
        return std::clamp(amount, -1, 1) + 1;
    }

    inline int encodeTranspose(int semitones)
    {
        return encodeInt(semitones, -kMaxTranspose, kMaxTranspose);
    }

    // Rotation is cyclic (a 16-step pattern rotated by -2 is equivalent to
    // rotating it by +14), unlike transpose which is genuinely signed - so
    // out-of-range values wrap modulo kPatternSteps rather than clamping to
    // the boundary. Clamping would silently floor any negative amount to 0,
    // discarding the caller's intent instead of expressing it correctly.
    // Exposed separately (not just inlined into encodeRotation) so callers
    // that need the resolved *plain* value - e.g. Router updating tracked
    // state to match what was actually sent - can share this logic instead
    // of duplicating the wrap.
    inline int wrapRotation(int steps)
    {
        int wrapped = steps % kPatternSteps;
        if (wrapped < 0)
            wrapped += kPatternSteps;

        return wrapped;
    }

    inline int encodeRotation(int steps)
    {
        return encodeInt(wrapRotation(steps), 0, kPatternSteps - 1);
    }

    inline int encodeLength(int steps)
    {
        return encodeInt(steps, kMinPatternLoopLength, kPatternSteps);
    }

    inline int encodeInversion(bool on)
    {
        return on ? 127 : 0;
    }

    // Retrograde: reverses the loop-relative playback order (time-domain,
    // reflected before Rotation - see MPL's getRotatedSourceStepIndex).
    inline int encodeRetrograde(bool on)
    {
        return on ? 127 : 0;
    }

    // M7: interval multiplication by 7 mod 12, applied to each stored note's
    // pitch class (pitch-domain, ahead of Inversion - see MPL's
    // applyPatternTransformsToNote). Its own inverse, same as Inversion.
    inline int encodeM7(bool on)
    {
        return on ? 127 : 0;
    }

    // Target Step Editing (CC 21/60-64) - live in MPL since 2026-08-18 (see
    // docs/routing_policy_v0_1.md), consumed for the first time on this side
    // by policy/MotifEngine. These write directly into a specific step of a
    // specific pattern (matching MPL's own Target-parameter step editor
    // rather than a whole-pattern transform like transpose/rotation/length/
    // inversion above), so they take a step/pattern index rather than
    // producing a delta - there's no "current value" to nudge relative to
    // the way InstanceStateTracker tracks for the pattern-level parameters.
    inline int encodeTargetPattern(int patternIndex) // 0..kMaxPatterns-1
    {
        return encodeInt(patternIndex, 0, kMaxPatterns - 1);
    }

    inline int encodeTargetStep(int stepIndex) // 1..kPatternSteps, matches MPL's own 1-indexed targetStepParam
    {
        return encodeInt(stepIndex, 1, kPatternSteps);
    }

    inline int encodeTargetNote(int note) // 0..127
    {
        return encodeInt(note, 0, 127);
    }

    inline int encodeTargetVelocity(int velocity) // 1..127
    {
        return encodeInt(velocity, 1, 127);
    }

    inline int encodeTargetDuration(int durationSteps) // 1..kPatternSteps
    {
        return encodeInt(durationSteps, 1, kPatternSteps);
    }

    inline int encodeTargetEnabled(bool on)
    {
        return on ? 127 : 0;
    }

    // Maps a Mutation::type string to its CC offset within a pattern's block
    // (30-35, 40-45, 50-55). Returns false if unrecognised.
    inline bool mutationOffsetForType(const std::string& type, MutationOffset& outOffset)
    {
        if (type == "transpose") { outOffset = MutationOffset::Transpose; return true; }
        if (type == "rotation")  { outOffset = MutationOffset::Rotation;  return true; }
        if (type == "length")    { outOffset = MutationOffset::Length;    return true; }
        if (type == "inversion")  { outOffset = MutationOffset::Inversion;  return true; }
        if (type == "retrograde") { outOffset = MutationOffset::Retrograde; return true; }
        if (type == "m7")         { outOffset = MutationOffset::M7;         return true; }
        return false;
    }

    struct NamedCC
    {
        int cc;
        const char* label;
    };

    // The full CC protocol MPL implements (docs/routing_policy_v0_1.md), for
    // UI that needs to offer a raw CC number by name - e.g. a manual "send
    // one CC now" diagnostic - without inventing its own copy of the map.
    // Includes CC 21/60-64 (live in MPL since 2026-08-18) even though those
    // aren't yet wired into Mutation/Router - this table is deliberately a
    // separate, simpler concern from the encode*/routing machinery above.
    inline constexpr std::array<NamedCC, 28> kNamedCCs { {
        { 20, "20 - Active Pattern" },
        { 21, "21 - Target Pattern" },
        { 22, "22 - Grid Mode" },
        { 23, "23 - Rate" },
        { 24, "24 - Swing" },
        { 30, "30 - P1 Transpose" },
        { 31, "31 - P1 Rotation" },
        { 32, "32 - P1 Length" },
        { 33, "33 - P1 Inversion" },
        { 34, "34 - P1 Retrograde" },
        { 35, "35 - P1 M7" },
        { 40, "40 - P2 Transpose" },
        { 41, "41 - P2 Rotation" },
        { 42, "42 - P2 Length" },
        { 43, "43 - P2 Inversion" },
        { 44, "44 - P2 Retrograde" },
        { 45, "45 - P2 M7" },
        { 50, "50 - P3 Transpose" },
        { 51, "51 - P3 Rotation" },
        { 52, "52 - P3 Length" },
        { 53, "53 - P3 Inversion" },
        { 54, "54 - P3 Retrograde" },
        { 55, "55 - P3 M7" },
        { 60, "60 - Target Step" },
        { 61, "61 - Target Note" },
        { 62, "62 - Target Velocity" },
        { 63, "63 - Target Duration" },
        { 64, "64 - Target Enabled" },
    } };
}
