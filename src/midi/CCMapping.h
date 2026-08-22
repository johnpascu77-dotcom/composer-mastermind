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
    constexpr int kSwing = 24;
    constexpr int kTargetStep = 60;
    constexpr int kTargetNote = 61;
    constexpr int kTargetVelocity = 62;
    constexpr int kTargetDuration = 63;
    constexpr int kTargetEnabled = 64;

    constexpr int kMaxPatterns = 3;          // MPL numPatterns
    constexpr int kPatternSteps = 16;        // MPL patternLength
    constexpr int kMinPatternLoopLength = 1; // MPL minPatternLoopLength
    constexpr int kMaxTranspose = 48;
    constexpr float kMaxSwing = 75.0f;

    enum class MutationOffset
    {
        Transpose = 0,
        Rotation = 1,
        Length = 2,
        Inversion = 3
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

    inline int encodeSwing(float swingPercent)
    {
        return encodeFloat(swingPercent, 0.0f, kMaxSwing);
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
    // (30/31/32/33, 40/41/42/43, 50/51/52/53). Returns false if unrecognised.
    inline bool mutationOffsetForType(const std::string& type, MutationOffset& outOffset)
    {
        if (type == "transpose") { outOffset = MutationOffset::Transpose; return true; }
        if (type == "rotation")  { outOffset = MutationOffset::Rotation;  return true; }
        if (type == "length")    { outOffset = MutationOffset::Length;    return true; }
        if (type == "inversion") { outOffset = MutationOffset::Inversion; return true; }
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
    inline constexpr std::array<NamedCC, 21> kNamedCCs { {
        { 20, "20 - Active Pattern" },
        { 21, "21 - Target Pattern" },
        { 22, "22 - Grid Mode" },
        { 24, "24 - Swing" },
        { 30, "30 - P1 Transpose" },
        { 31, "31 - P1 Rotation" },
        { 32, "32 - P1 Length" },
        { 33, "33 - P1 Inversion" },
        { 40, "40 - P2 Transpose" },
        { 41, "41 - P2 Rotation" },
        { 42, "42 - P2 Length" },
        { 43, "43 - P2 Inversion" },
        { 50, "50 - P3 Transpose" },
        { 51, "51 - P3 Rotation" },
        { 52, "52 - P3 Length" },
        { 53, "53 - P3 Inversion" },
        { 60, "60 - Target Step" },
        { 61, "61 - Target Note" },
        { 62, "62 - Target Velocity" },
        { 63, "63 - Target Duration" },
        { 64, "64 - Target Enabled" },
    } };
}
