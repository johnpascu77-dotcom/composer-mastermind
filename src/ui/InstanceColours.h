#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cstddef>

// One colour per instance, consistent everywhere the UI shows instance
// identity (docs/score_timeline_ui_concept.md) - shared between the Live
// piano roll (ui/PrimaryView) and Setup mode (ui/PatternSetupView) so the
// same instance reads as the same colour in both. Cycles past 3 rather
// than hard-failing if a project ever registers more instances than usual.
inline juce::Colour instanceColourForIndex(int index)
{
    static const juce::Colour palette[] = {
        juce::Colour(0xff378add), // blue
        juce::Colour(0xff1d9e75), // teal
        juce::Colour(0xffd85a30), // coral
        juce::Colour(0xffd4537e), // pink
    };
    return palette[(size_t) index % (sizeof(palette) / sizeof(palette[0]))];
}
