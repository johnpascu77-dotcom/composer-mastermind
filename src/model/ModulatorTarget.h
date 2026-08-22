#pragma once

#include <string>

// A CC number Composer Mastermind treats as an addressable "control channel"
// for something outside the Instance/Mutation/MPL model - typically a
// Bitwig modulator's own parameter (a Random/Steps/Curve modulator's own
// Rate/Depth/Shape), paired via Bitwig's own per-modulator "Learn CC" (not
// Bitwig's general "Map to Controller or Key", which ignores plugin-
// generated CC - see docs/routing_policy_v0_1.md) and, in practice, routed
// through a virtual MIDI cable (e.g. loopMIDI) so Bitwig treats the incoming
// CC as hardware-originated. Confirmed working live 2026-08-19.
//
// The CC<->modulator pairing itself lives entirely in the Bitwig project
// (Learn CC), not here - this struct only defines what Composer Mastermind
// sends and when. It has no idea what's on the other end, and doesn't need
// to: Bitwig's own Learn-CC binding auto-scales the raw 0..127 CC value onto
// whatever range the mapped parameter has, so there's no min/max/encode
// concept here either, unlike CCMapping.h's MPL-specific encoders.
struct ModulatorTarget
{
    std::string id;
    int ccNumber = 70;   // deliberately outside MPL's 20-64 protocol range
    int midiChannel = 1;

    // "arc": continuously tracks one ArcSet dimension's current value every
    //   bar (see ComposerCore::sendModulatorTargetUpdates) - arcDimension
    //   names one of ArcSet::getArcNames().
    // "section": takes a fixed value once per BlueprintSection boundary,
    //   authored per-section via SectionModulatorValue (model/Blueprint.h) -
    //   a target with no value listed for the newly-active section simply
    //   isn't sent, and keeps whatever value it last held.
    std::string mode = "arc";
    std::string arcDimension;
};
