#pragma once

#include <string>

// Suggests which of the sibling plugin OrchConductor's own named
// Narrative Lanes (organic_build/dark_build/bright_build/heroic_build/
// suspense/anticlimax - see that repo's
// Docs/OrchConductor_MC_Integration_And_Narrative_Scan_Design.md) best
// fits one of MC's own generated BlueprintSections, by matching the
// section's archetype-derived energy/tension against a small hardcoded
// lane-character table (same spirit as OrchConductor's own
// factoryCombiCharacter). Purely descriptive - the result is written into
// BlueprintSection::suggestedNarrativeLane as export metadata for the user
// to read and set in OrchConductor's own UI, once, per project. NOT new
// IPC and NOT live coupling - MC<->OrchConductor's existing bridge
// (blueprint playhead position -> CC102 -> OrchConductor's Narrative Scan)
// stays exactly as it is; a richer direct metadata match between the two
// plugins was already designed and deliberately rejected as premature
// (the axes don't line up 1:1) - this stays a one-time authoring
// suggestion, never a live value. Pure/JUCE-free.
namespace NarrativeLaneSuggester
{
    // archetype: BlueprintGenerator::archetypeName's vocabulary
    // ("presentation"/"build"/"peak"/"release", or empty) - used only as a
    // tie-break when two lanes are near-equidistant in energy/tension, not
    // as the primary signal. energy/tension: 0..1, matching ArcSet's own
    // convention - the section's own derived values (see
    // BlueprintGenerator's internal derivedValue) are the intended source.
    std::string suggestLane(const std::string& archetype, float energy, float tension);
}
