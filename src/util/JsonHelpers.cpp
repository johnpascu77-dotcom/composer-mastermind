#include "JsonHelpers.h"

namespace JsonHelpers
{
    int getInt(const juce::var& obj, const char* key, int defaultValue)
    {
        if (!obj.isObject())
            return defaultValue;

        const auto& value = obj[key];
        return value.isVoid() ? defaultValue : static_cast<int>(value);
    }

    float getFloat(const juce::var& obj, const char* key, float defaultValue)
    {
        if (!obj.isObject())
            return defaultValue;

        const auto& value = obj[key];
        return value.isVoid() ? defaultValue : static_cast<float>(value);
    }

    bool getBool(const juce::var& obj, const char* key, bool defaultValue)
    {
        if (!obj.isObject())
            return defaultValue;

        const auto& value = obj[key];
        return value.isVoid() ? defaultValue : static_cast<bool>(value);
    }

    std::string getString(const juce::var& obj, const char* key, const std::string& defaultValue)
    {
        if (!obj.isObject())
            return defaultValue;

        const auto& value = obj[key];
        return value.isVoid() ? defaultValue : value.toString().toStdString();
    }

    juce::Array<juce::var> getArray(const juce::var& obj, const char* key)
    {
        if (!obj.isObject())
            return {};

        const auto& value = obj[key];
        if (auto* array = value.getArray())
            return *array;

        return {};
    }

    std::vector<std::string> getStringArray(const juce::var& obj, const char* key)
    {
        std::vector<std::string> result;
        for (const auto& item : getArray(obj, key))
            result.push_back(item.toString().toStdString());
        return result;
    }
}
