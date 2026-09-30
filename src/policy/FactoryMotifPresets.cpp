#include "FactoryMotifPresets.h"
#include <algorithm>

namespace
{
    // Presentation: a calm, stepwise "call" - establishes an idea without
    // spending much range. Root / step up / third up / step up, settling on
    // a slightly held last note.
    MotifPreset makePresentation()
    {
        MotifPreset preset;
        preset.id = "factory_presentation";
        preset.name = "Factory - Presentation";
        preset.tags = { "presentation" };
        preset.notes = {
            { 0, 1.0f, 1.0f },
            { 2, 1.0f, 0.9f },
            { 4, 1.0f, 0.95f },
            { 2, 1.5f, 0.85f },
        };
        return preset;
    }

    // Build: a rising, arpeggiated-feel line with a slight pull-back before
    // the last note - more notes than Presentation, so there's real motion
    // to gather energy from as later passes develop it.
    MotifPreset makeBuild()
    {
        MotifPreset preset;
        preset.id = "factory_build";
        preset.name = "Factory - Build";
        preset.tags = { "build" };
        preset.notes = {
            { 0, 0.5f, 0.85f },
            { 3, 0.5f, 0.9f },
            { 5, 0.5f, 0.95f },
            { 7, 0.5f, 1.0f },
            { 5, 1.0f, 0.95f },
        };
        return preset;
    }

    // Peak: the widest leaps and strongest dynamics of the four - a bold
    // climax gesture (up a fifth, down to a third, up an octave).
    MotifPreset makePeak()
    {
        MotifPreset preset;
        preset.id = "factory_peak";
        preset.name = "Factory - Peak";
        preset.tags = { "peak" };
        preset.notes = {
            { 0, 0.5f, 1.1f },
            { 7, 0.5f, 1.05f },
            { 4, 0.5f, 1.1f },
            { 12, 1.0f, 1.2f },
            { 7, 0.5f, 1.0f },
        };
        return preset;
    }

    // Release: a falling gesture back toward rest - stepwise descent, final
    // note held long and quiet, a genuine cadence rather than just a fade.
    MotifPreset makeRelease()
    {
        MotifPreset preset;
        preset.id = "factory_release";
        preset.name = "Factory - Release";
        preset.tags = { "release" };
        preset.notes = {
            { 0, 1.0f, 0.9f },
            { -2, 1.0f, 0.8f },
            { -4, 1.0f, 0.7f },
            { -7, 2.0f, 0.6f },
        };
        return preset;
    }
}

namespace FactoryMotifPresets
{
    std::vector<MotifPreset> getAll()
    {
        return { makePresentation(), makeBuild(), makePeak(), makeRelease() };
    }

    const MotifPreset* findForArchetype(const std::string& archetypeTag)
    {
        static const std::vector<MotifPreset> all = getAll();

        for (const auto& preset : all)
            if (std::find(preset.tags.begin(), preset.tags.end(), archetypeTag) != preset.tags.end())
                return &preset;

        return nullptr;
    }
}
