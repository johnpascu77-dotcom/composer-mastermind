#pragma once

#include <vector>

// A single point in an Arc's shape: at this bar, the arc has this value.
// Values are conventionally normalized 0..1 (per docs/composer_mastermind_design.md's
// v0.5 section), but nothing here enforces that range.
struct ArcBreakpoint
{
    int bar = 0;
    float value = 0.0f;
};

// The result of querying an Arc at a bar: not just the interpolated value,
// but its direction/magnitude of recent change and how far until the next
// authored point. A bar-position value alone can't distinguish "energy 0.7
// rising toward a climax" from "energy 0.7 falling after one" - callers
// that need to react differently to those two cases need delta and
// distanceToNextBreakpoint, not just value.
struct ArcSample
{
    float value = 0.0f;
    float delta = 0.0f;
    int distanceToNextBreakpoint = 0;
};

// Generic piecewise-linear breakpoint curve over bar position. Reused for
// every named arc dimension (energy/tension/density/complexity/coherence)
// rather than bespoke logic per type - see composer_mastermind_design.md's
// v0.5 section. Not yet consumed by anything (no Blueprint JSON exists to
// author breakpoints through yet) - this is infrastructure, the same way
// Scheduler sat unused for two milestones before SceneAdvancePolicy used it.
class Arc
{
public:
    // Breakpoints are sorted by bar; duplicate bars keep the last one set.
    void setBreakpoints(std::vector<ArcBreakpoint> points);

    // value: linear interpolation between the surrounding breakpoints (flat
    // before the first / after the last).
    // delta: value minus the most recent breakpoint at or before this bar
    // (zero before the first breakpoint, since nothing has started moving yet).
    // distanceToNextBreakpoint: bars until the next breakpoint strictly after
    // this one (zero once at or past the last breakpoint - nothing more ahead).
    ArcSample evaluate(int bar) const;

    // Sorted by bar, deduped - the same shape setBreakpoints() would produce
    // for whatever was last set. Needed by anything that displays/edits the
    // curve directly (the arc graph editor), not just samples it.
    const std::vector<ArcBreakpoint>& getBreakpoints() const;

private:
    std::vector<ArcBreakpoint> breakpoints;
};
