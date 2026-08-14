#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <vector>

// Safe, forward-compatible accessors over juce::var JSON objects: a missing
// or wrong-typed field falls back to the given default rather than crashing
// or silently producing a garbage value. Used by StateSerializer so a scene
// file from an older/partial schema still loads instead of failing outright.
namespace JsonHelpers
{
    int getInt(const juce::var& obj, const char* key, int defaultValue);
    float getFloat(const juce::var& obj, const char* key, float defaultValue);
    bool getBool(const juce::var& obj, const char* key, bool defaultValue);
    std::string getString(const juce::var& obj, const char* key, const std::string& defaultValue = {});

    // Empty if the property is missing or not an array.
    juce::Array<juce::var> getArray(const juce::var& obj, const char* key);
    std::vector<std::string> getStringArray(const juce::var& obj, const char* key);
}
