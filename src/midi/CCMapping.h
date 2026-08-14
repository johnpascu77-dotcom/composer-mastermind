#pragma once

#include <string>
#include <cmath>
#include <algorithm>

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
    constexpr int kGridMode = 22;
    constexpr int kSwing = 24;

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

    inline int encodeRotation(int steps)
    {
        return encodeInt(steps, 0, kPatternSteps - 1);
    }

    inline int encodeLength(int steps)
    {
        return encodeInt(steps, kMinPatternLoopLength, kPatternSteps);
    }

    inline int encodeInversion(bool on)
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

    // Encodes a mutation's raw amount into the 0..127 CC value expected for
    // its type. `amount` is interpreted per-type: transpose in semitones,
    // rotation/length in steps, inversion as amount != 0 meaning "on".
    inline int encodeMutationAmount(MutationOffset offset, int amount)
    {
        switch (offset)
        {
            case MutationOffset::Transpose: return encodeTranspose(amount);
            case MutationOffset::Rotation:  return encodeRotation(amount);
            case MutationOffset::Length:    return encodeLength(amount);
            case MutationOffset::Inversion: return encodeInversion(amount != 0);
        }
        return 0;
    }
}
