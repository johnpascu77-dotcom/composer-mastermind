#include "DurationCalculator.h"
#include <algorithm>
#include <cmath>

namespace DurationCalculator
{
    namespace
    {
        std::vector<TempoPoint> normalizeTempoSpec(std::vector<TempoPoint> tempoSpec)
        {
            if (tempoSpec.empty())
                tempoSpec.push_back({ 0.0, 120.0 });

            std::sort(tempoSpec.begin(), tempoSpec.end(),
                      [](const TempoPoint& a, const TempoPoint& b) { return a.atMinute < b.atMinute; });

            std::vector<TempoPoint> deduped;
            for (const auto& point : tempoSpec)
            {
                if (!deduped.empty() && deduped.back().atMinute == point.atMinute)
                    deduped.back() = point; // last wins for a repeated minute
                else
                    deduped.push_back(point);
            }

            if (deduped.front().atMinute > 0.0)
                deduped.insert(deduped.begin(), { 0.0, deduped.front().bpm });

            return deduped;
        }

        // Total beats elapsed across [0, targetMinutes], integrating the
        // piecewise-linear tempo curve segment by segment (trapezoidal -
        // exact for a linear bpm ramp, which is exactly what a Bitwig
        // Tempo lane draws between two points).
        double totalBeats(const std::vector<TempoPoint>& tempoMap, double targetMinutes)
        {
            double beats = 0.0;

            for (size_t i = 0; i + 1 < tempoMap.size(); ++i)
            {
                const auto& start = tempoMap[i];
                const auto& end = tempoMap[i + 1];

                if (start.atMinute >= targetMinutes)
                    return beats;

                const double segmentEndMinute = std::min(end.atMinute, targetMinutes);
                const double segmentMinutes = segmentEndMinute - start.atMinute;
                if (segmentMinutes <= 0.0)
                    continue;

                const double fraction = (end.atMinute > start.atMinute)
                    ? (segmentEndMinute - start.atMinute) / (end.atMinute - start.atMinute)
                    : 1.0;
                const double bpmAtSegmentEnd = start.bpm + (end.bpm - start.bpm) * fraction;
                const double avgBpm = (start.bpm + bpmAtSegmentEnd) * 0.5;

                beats += avgBpm * segmentMinutes;
            }

            // Trailing flat segment from the last tempo point to
            // targetMinutes, at that point's own bpm - a tempo map with one
            // point is exactly this: a flat tempo for the whole piece.
            const auto& last = tempoMap.back();
            if (last.atMinute < targetMinutes)
                beats += last.bpm * (targetMinutes - last.atMinute);

            return beats;
        }
    }

    DurationPlan compute(double targetMinutes, int beatsPerBar,
                          std::vector<TempoPoint> tempoSpec, int desiredSectionCount)
    {
        DurationPlan plan;

        beatsPerBar = std::max(1, beatsPerBar);
        desiredSectionCount = std::max(1, desiredSectionCount);
        targetMinutes = std::max(0.0, targetMinutes);

        plan.tempoMap = normalizeTempoSpec(std::move(tempoSpec));

        const double beats = totalBeats(plan.tempoMap, targetMinutes);
        plan.totalBars = std::max(1, static_cast<int>(std::round(beats / beatsPerBar)));

        plan.suggestedSectionBoundaries.push_back(0);
        for (int i = 1; i < desiredSectionCount; ++i)
        {
            const int boundary = static_cast<int>(std::round(
                (static_cast<double>(plan.totalBars) * i) / desiredSectionCount));

            if (boundary > plan.suggestedSectionBoundaries.back())
                plan.suggestedSectionBoundaries.push_back(boundary);
        }

        if (plan.suggestedSectionBoundaries.back() != plan.totalBars)
            plan.suggestedSectionBoundaries.push_back(plan.totalBars);

        return plan;
    }
}
