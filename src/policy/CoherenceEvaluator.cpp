#include "CoherenceEvaluator.h"
#include <unordered_map>
#include <algorithm>

namespace CoherenceEvaluator
{
    float evaluate(const std::vector<InstanceParameterState>& states)
    {
        if (states.size() < 2)
            return 1.0f;

        // Grid mode agreement: fraction sharing the majority value (0 or 1).
        int gridZeroCount = 0;
        for (const auto& state : states)
        {
            if (state.gridMode == 0)
                ++gridZeroCount;
        }
        const int gridOneCount = static_cast<int>(states.size()) - gridZeroCount;
        const float gridAgreement = static_cast<float>(std::max(gridZeroCount, gridOneCount)) / states.size();

        // Active pattern agreement: fraction sharing the majority pattern.
        std::unordered_map<int, int> patternCounts;
        for (const auto& state : states)
            ++patternCounts[state.activePattern];

        int maxPatternCount = 0;
        for (const auto& entry : patternCounts)
            maxPatternCount = std::max(maxPatternCount, entry.second);

        const float patternAgreement = static_cast<float>(maxPatternCount) / states.size();

        // Swing closeness: inverse of the normalized spread across the 0-75% range.
        float minSwing = states.front().swing;
        float maxSwing = states.front().swing;
        for (const auto& state : states)
        {
            minSwing = std::min(minSwing, state.swing);
            maxSwing = std::max(maxSwing, state.swing);
        }

        constexpr float kMaxSwingSpread = 75.0f;
        const float swingSpread = std::clamp((maxSwing - minSwing) / kMaxSwingSpread, 0.0f, 1.0f);
        const float swingAgreement = 1.0f - swingSpread;

        return (gridAgreement + patternAgreement + swingAgreement) / 3.0f;
    }
}
