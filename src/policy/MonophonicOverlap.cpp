#include "MonophonicOverlap.h"
#include <algorithm>

namespace MonophonicOverlap
{
    bool rangeOverlapsExisting(const std::vector<StepSnapshot>& steps, int startStep, int duration, int ignoredStep,
                                int totalSteps)
    {
        const int newEnd = std::min(totalSteps - 1, startStep + duration - 1);

        for (int i = 0; i < (int) steps.size(); ++i)
        {
            if (i == ignoredStep || !steps[(size_t) i].enabled)
                continue;

            const int otherDuration = std::max(1, steps[(size_t) i].duration);
            const int otherEnd = std::min(totalSteps - 1, i + otherDuration - 1);

            if (startStep <= otherEnd && i <= newEnd)
                return true;
        }
        return false;
    }

    int maxNonOverlappingDuration(const std::vector<StepSnapshot>& steps, int startStep, int requestedDuration,
                                   int ignoredStep, int totalSteps)
    {
        requestedDuration = std::clamp(requestedDuration, 1, std::max(1, totalSteps - startStep));

        int maxDuration = requestedDuration;
        for (int duration = 1; duration <= requestedDuration; ++duration)
        {
            if (rangeOverlapsExisting(steps, startStep, duration, ignoredStep, totalSteps))
                break;
            maxDuration = duration;
        }
        return std::clamp(maxDuration, 1, std::max(1, totalSteps - startStep));
    }

    std::vector<StepSnapshot> normalizePattern(std::vector<StepSnapshot> steps)
    {
        const int totalSteps = (int) steps.size();

        for (int i = 0; i < totalSteps; ++i)
        {
            auto& step = steps[(size_t) i];
            if (!step.enabled)
                continue;

            int nextEnabled = totalSteps; // no later note this pass - runs to the array's own end
            for (int j = i + 1; j < totalSteps; ++j)
            {
                if (steps[(size_t) j].enabled)
                {
                    nextEnabled = j;
                    break;
                }
            }

            const int maxDuration = std::max(1, nextEnabled - i);
            const int currentDuration = step.duration > 0 ? step.duration : 1;
            step.duration = std::min(currentDuration, maxDuration);
        }

        return steps;
    }
}
