#pragma once

#include <vector>
#include <string>
#include "Arc.h"

// Turns a named whole-piece curve shape into breakpoints for ONE driving
// Arc dimension directly - the MC-side analogue of the sibling plugin
// OrchConductor's own sampleArc(shape, t) (its Narrative Lane Maker's
// Propose button), so a user names a shape instead of hand-drawing one.
// Feeds straight into the existing, unmodified
// BlueprintGenerator::generate()'s drivingArcName/liveArcSet inputs -
// BlueprintGenerator already turns one driving curve into classified
// Presentation/Build/Peak/Release sections and derives the other 4 arc
// dimensions itself (see its own comment: "energy is normally the driving
// dimension"); this only needs to produce that one input curve. The
// intended caller sets the result onto ArcSet's "energy" dimension via
// setArc before calling BlueprintGenerator::generate, though nothing here
// hardcodes that name - dimension choice stays the caller's decision.
// Pure/JUCE-free, matching policy/BlueprintGenerator's own style.
namespace ArcShapeLibrary
{
    // Naming deliberately mirrors OrchConductor's own 10 narrative-lane
    // shapes - same vocabulary across the two plugins, cheap and worth it
    // given the user's own "Narrative Lane" analogy for this feature.
    enum class ShapeName
    {
        OrganicBuild,
        ArchRiseFall,
        LongFade,
        TerracedBlocks,
        SurgingWaves,
        HeroicJourney,
        SuspenseRelease,
        MosaicEpisodic,
        CatastropheCollapse,
        PastoralPlateau
    };

    // Display name, e.g. "Arch (Rise & Fall)" - for UI population.
    std::string shapeName(ShapeName shape);

    // Every shape's display name, in enum declaration order.
    std::vector<std::string> shapeNames();

    // sectionBoundaries: bar numbers from 0 to the piece's total bar count
    // (DurationCalculator::DurationPlan::suggestedSectionBoundaries is the
    // intended source) - one breakpoint is produced per boundary.
    // restlessness (0..1): 0 = the shape's own smooth curve, unmodified;
    // higher values add a bounded, seeded jitter on top of each
    // breakpoint's value, so two runs of the same shape aren't identical -
    // "unexpected" riding on top of the shape's own "coherent" curve, not
    // instead of it. Values stay clamped to 0..1 (Arc's own convention).
    // Deterministic for a given seed - no raw/unseeded randomness anywhere
    // in this codebase's generative code, and this doesn't start that.
    // sectionBoundaries with fewer than 2 entries returns an empty vector
    // (matches BlueprintGenerator::generate's own "fewer than 2
    // breakpoints -> could not generate" convention).
    std::vector<ArcBreakpoint> generateBreakpoints(ShapeName shape, const std::vector<int>& sectionBoundaries,
                                                     float restlessness, int seed);
}
