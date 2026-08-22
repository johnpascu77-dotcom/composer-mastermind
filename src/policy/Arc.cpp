#include "Arc.h"
#include <algorithm>

void Arc::setBreakpoints(std::vector<ArcBreakpoint> points)
{
    std::stable_sort(points.begin(), points.end(),
                      [](const ArcBreakpoint& a, const ArcBreakpoint& b) { return a.bar < b.bar; });

    // Duplicate bars: keep the last one written for that bar.
    std::vector<ArcBreakpoint> deduped;
    for (const auto& point : points)
    {
        if (!deduped.empty() && deduped.back().bar == point.bar)
            deduped.back() = point;
        else
            deduped.push_back(point);
    }

    breakpoints = std::move(deduped);
}

ArcSample Arc::evaluate(int bar) const
{
    if (breakpoints.empty())
        return {};

    if (bar < breakpoints.front().bar)
        return { breakpoints.front().value, 0.0f, breakpoints.front().bar - bar };

    if (bar >= breakpoints.back().bar)
        return { breakpoints.back().value, 0.0f, 0 };

    // Find the surrounding pair: lo.bar <= bar < hi.bar.
    size_t hiIndex = 0;
    while (hiIndex < breakpoints.size() && breakpoints[hiIndex].bar <= bar)
        ++hiIndex;

    const auto& lo = breakpoints[hiIndex - 1];
    const auto& hi = breakpoints[hiIndex];

    const float span = static_cast<float>(hi.bar - lo.bar);
    const float t = (span > 0.0f) ? static_cast<float>(bar - lo.bar) / span : 0.0f;
    const float value = lo.value + t * (hi.value - lo.value);

    return { value, value - lo.value, hi.bar - bar };
}

const std::vector<ArcBreakpoint>& Arc::getBreakpoints() const
{
    return breakpoints;
}
