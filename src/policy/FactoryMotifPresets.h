#pragma once

#include <string>
#include <vector>
#include "../model/Preset.h"

// One simple, hand-composed MotifPreset per archetype - the "starter books
// on the shelf" (user's own framing, 2026-09-21): the generative engine
// (policy/MotifEngine.cpp's stampMotifForSection/applyForSection) can only
// ever develop a section if a MotifPreset tagged for that section's
// archetype already exists in the library; nothing previously guaranteed
// one did, for either a hand-authored blueprint or a Propose-generated one.
// Deliberately simple, narrow-interval cells (4-5 notes) - the point isn't
// to author a finished melody here, it's to give the existing
// transformation machinery (rotation/transpose/inversion/retrograde/m7,
// register drift, rhythm passes) real, coherent material to work from.
// "Simple starting motives are the best to obtain some coherent music."
namespace FactoryMotifPresets
{
    // All 4 factory presets (presentation/build/peak/release), in that
    // order. Used both to seed a fresh/reset PresetLibrary
    // (ComposerCore::resetToFactoryDefaults and its constructor) and, one
    // at a time via findForArchetype, to auto-mint whatever a generated
    // proposal needs that the live library doesn't already have tagged.
    std::vector<MotifPreset> getAll();

    // The single factory preset for one archetype tag ("presentation",
    // "build", "peak", "release"), or nullptr for anything else. Returns a
    // pointer into a static table - never dangling, safe to copy from
    // immediately.
    const MotifPreset* findForArchetype(const std::string& archetypeTag);
}
