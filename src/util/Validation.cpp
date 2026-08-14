#include "Validation.h"

namespace Validation
{
    bool isValidMidiChannel(int channel)
    {
        return channel >= 1 && channel <= 16;
    }

    bool isValidPatternIndex(int index)
    {
        return index >= 0 && index <= 3;
    }

    bool isValidGridMode(int mode)
    {
        return mode == 0 || mode == 1;
    }

    bool isValidSwing(float swing)
    {
        return swing >= 0.0f && swing <= 75.0f;
    }

    bool isValidScene(const Scene& scene, std::string& errorMessage)
    {
        if (scene.id.empty())
        {
            errorMessage = "Scene id is empty";
            return false;
        }

        if (scene.name.empty())
        {
            errorMessage = "Scene name is empty";
            return false;
        }

        if (scene.durationBars <= 0)
        {
            errorMessage = "Scene durationBars must be greater than 0";
            return false;
        }

        if (!isValidGridMode(scene.global.gridMode))
        {
            errorMessage = "Invalid grid mode";
            return false;
        }

        if (!isValidSwing(scene.global.swing))
        {
            errorMessage = "Invalid swing value";
            return false;
        }

        for (const auto& override : scene.instanceOverrides)
        {
            if (override.targetInstance.empty())
            {
                errorMessage = "Scene instance override has an empty target instance";
                return false;
            }

            if (override.gridMode != -1 && !isValidGridMode(override.gridMode))
            {
                errorMessage = "Invalid grid mode override for '" + override.targetInstance + "'";
                return false;
            }

            if (override.swing != -1.0f && !isValidSwing(override.swing))
            {
                errorMessage = "Invalid swing override for '" + override.targetInstance + "'";
                return false;
            }
        }

        return true;
    }

    bool isValidMutation(const Mutation& mutation, std::string& errorMessage)
    {
        if (mutation.type.empty())
        {
            errorMessage = "Mutation type is empty";
            return false;
        }

        if (mutation.targetInstance.empty())
        {
            errorMessage = "Mutation target instance is empty";
            return false;
        }

        return true;
    }

    bool isValidInstance(const Instance& instance, std::string& errorMessage)
    {
        if (instance.id.empty())
        {
            errorMessage = "Instance id is empty";
            return false;
        }

        if (!isValidMidiChannel(instance.midiChannel))
        {
            errorMessage = "Invalid MIDI channel";
            return false;
        }

        return true;
    }
}
