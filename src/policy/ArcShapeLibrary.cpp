#include "ArcShapeLibrary.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace ArcShapeLibrary
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;

        float clamp01(float x)
        {
            return std::max(0.0f, std::min(1.0f, x));
        }

        // One value per shape at normalized position t (0..1 across the
        // whole piece) - ported from OrchConductor's own sampleArc energy
        // component (Source/OrchConductorProcessor.cpp), same shapes, same
        // constants, adapted to MC's single-curve need.
        float sampleShape(ShapeName shape, float t)
        {
            switch (shape)
            {
                case ShapeName::OrganicBuild:
                    return clamp01(std::pow(t, 1.6f));

                case ShapeName::ArchRiseFall:
                    return clamp01(std::sin(t * kPi));

                case ShapeName::LongFade:
                    return clamp01(std::pow(1.0f - t, 1.3f));

                case ShapeName::TerracedBlocks:
                {
                    const int step = std::clamp((int) (t * 3.999f), 0, 3);
                    const float levels[] = { 0.2f, 0.5f, 0.35f, 0.9f };
                    return levels[step];
                }

                case ShapeName::SurgingWaves:
                {
                    const float base = 0.12f + 0.5f * t;
                    const float swell = 0.32f * (0.5f - 0.5f * std::cos(t * 6.0f * kPi));
                    return clamp01(base + swell);
                }

                case ShapeName::HeroicJourney:
                {
                    if (t < 0.28f)      return clamp01(0.42f + 0.06f * std::sin(t * 12.0f));
                    if (t < 0.52f)      { const float u = (t - 0.28f) / 0.24f; return clamp01(0.4f - 0.24f * u); }
                    const float u = (t - 0.52f) / 0.48f;
                    return clamp01(0.18f + 0.82f * std::pow(u, 1.2f));
                }

                case ShapeName::SuspenseRelease:
                {
                    if (t < 0.72f)      return clamp01(0.16f + 0.14f * t);
                    if (t < 0.84f)      { const float u = (t - 0.72f) / 0.12f; return clamp01(0.24f + 0.7f * u); }
                    const float u = (t - 0.84f) / 0.16f;
                    return clamp01(0.94f - 0.5f * u);
                }

                case ShapeName::MosaicEpisodic:
                    return clamp01(0.5f + 0.42f * std::sin(t * 7.3f + 1.1f));

                case ShapeName::CatastropheCollapse:
                {
                    if (t < 0.56f)      return clamp01(0.82f * std::pow(t / 0.56f, 1.3f));
                    if (t < 0.63f)      { const float u = (t - 0.56f) / 0.07f; return clamp01(0.82f - 0.74f * u); }
                    const float u = (t - 0.63f) / 0.37f;
                    return clamp01(0.08f + 0.4f * u);
                }

                case ShapeName::PastoralPlateau:
                {
                    if (t < 0.22f)      return clamp01((t / 0.22f) * 0.5f);
                    if (t < 0.85f)      return clamp01(0.5f + 0.06f * std::sin(t * 8.0f * kPi));
                    return clamp01(0.5f + ((t - 0.85f) / 0.15f) * 0.22f);
                }
            }

            return clamp01(t);
        }
    }

    std::string shapeName(ShapeName shape)
    {
        switch (shape)
        {
            case ShapeName::OrganicBuild:        return "Organic Build";
            case ShapeName::ArchRiseFall:         return "Arch (Rise & Fall)";
            case ShapeName::LongFade:             return "Long Fade / Dissolution";
            case ShapeName::TerracedBlocks:        return "Terraced Blocks";
            case ShapeName::SurgingWaves:          return "Surging Waves";
            case ShapeName::HeroicJourney:         return "Heroic Journey";
            case ShapeName::SuspenseRelease:       return "Suspense -> Release";
            case ShapeName::MosaicEpisodic:        return "Mosaic / Episodic";
            case ShapeName::CatastropheCollapse:   return "Catastrophe / Collapse";
            case ShapeName::PastoralPlateau:       return "Pastoral Plateau";
        }
        return "Organic Build";
    }

    std::vector<std::string> shapeNames()
    {
        return {
            shapeName(ShapeName::OrganicBuild),
            shapeName(ShapeName::ArchRiseFall),
            shapeName(ShapeName::LongFade),
            shapeName(ShapeName::TerracedBlocks),
            shapeName(ShapeName::SurgingWaves),
            shapeName(ShapeName::HeroicJourney),
            shapeName(ShapeName::SuspenseRelease),
            shapeName(ShapeName::MosaicEpisodic),
            shapeName(ShapeName::CatastropheCollapse),
            shapeName(ShapeName::PastoralPlateau)
        };
    }

    std::vector<ArcBreakpoint> generateBreakpoints(ShapeName shape, const std::vector<int>& sectionBoundaries,
                                                     float restlessness, int seed)
    {
        if (sectionBoundaries.size() < 2)
            return {};

        restlessness = clamp01(restlessness);

        const int totalBars = sectionBoundaries.back();
        std::mt19937 rng(static_cast<unsigned int>(seed));
        std::uniform_real_distribution<float> jitter(-0.08f, 0.08f);

        std::vector<ArcBreakpoint> breakpoints;
        breakpoints.reserve(sectionBoundaries.size());

        for (int bar : sectionBoundaries)
        {
            const float t = totalBars > 0 ? static_cast<float>(bar) / static_cast<float>(totalBars) : 0.0f;
            float value = sampleShape(shape, t);

            if (restlessness > 0.0f)
                value = clamp01(value + jitter(rng) * restlessness);

            breakpoints.push_back({ bar, value });
        }

        return breakpoints;
    }
}
