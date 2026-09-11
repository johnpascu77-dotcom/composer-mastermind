#include "NarrativeLaneSuggester.h"
#include <array>
#include <cmath>
#include <limits>

namespace NarrativeLaneSuggester
{
    namespace
    {
        struct LaneCharacter { const char* name; float energy; float tension; };

        // Approximate character per OrchConductor factory lane name - not
        // read from OrchConductor's own data (a separate repo/build), just
        // a small hardcoded table in the same spirit as its own
        // factoryCombiCharacter. Deliberately coarse: this only has to
        // pick a reasonable starting point for the user to confirm or
        // change in OrchConductor's own UI, not be exact.
        constexpr std::array<LaneCharacter, 6> kLanes { {
            { "organic_build", 0.50f, 0.35f },
            { "dark_build",    0.55f, 0.75f },
            { "bright_build",  0.60f, 0.25f },
            { "heroic_build",  0.75f, 0.45f },
            { "suspense",      0.25f, 0.80f },
            { "anticlimax",    0.30f, 0.20f },
        } };

        // Tie-break preference per archetype when two lanes are within
        // epsilon of each other - keeps the suggestion deterministic
        // rather than depending on array iteration order alone.
        const char* archetypeTieBreakPreference(const std::string& archetype)
        {
            if (archetype == "peak")    return "heroic_build";
            if (archetype == "build")   return "organic_build";
            if (archetype == "release") return "anticlimax";
            return "organic_build"; // presentation / empty
        }
    }

    std::string suggestLane(const std::string& archetype, float energy, float tension)
    {
        constexpr float kEpsilon = 0.03f;
        const char* preferred = archetypeTieBreakPreference(archetype);

        float bestDistance = std::numeric_limits<float>::max();
        const LaneCharacter* best = &kLanes.front();

        for (const auto& lane : kLanes)
        {
            const float dEnergy = lane.energy - energy;
            const float dTension = lane.tension - tension;
            const float distance = std::sqrt(dEnergy * dEnergy + dTension * dTension);

            if (distance < bestDistance - kEpsilon)
            {
                bestDistance = distance;
                best = &lane;
            }
            else if (distance < bestDistance + kEpsilon && std::string(lane.name) == preferred)
            {
                bestDistance = distance;
                best = &lane;
            }
        }

        return best->name;
    }
}
