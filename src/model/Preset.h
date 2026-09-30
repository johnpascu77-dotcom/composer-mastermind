#pragma once

#include <string>
#include <vector>

// A named, reusable, role-addressed bundle of instance-level parameter
// values - the "Preset" unit described in
// docs/composer_mastermind_design.md's "'Send To All' Is One Preset Among
// Many" section. Addressed by Instance::role (e.g. "anchor"), not by
// literal instance id, so the same preset applies across projects/sessions
// regardless of which concrete instance ids happen to be registered -
// resolved at apply-time via policy/PresetResolver, which looks up whoever
// currently holds that role.
//
// v0.1 scope: this is the first of the design doc's three preset
// categories ("role presets, arc presets, and rhythmic-relationship
// presets... different JSON object types"). Only role presets are built so
// far. Fields are instance-level only (activePattern/gridMode/swing/rate,
// matching SceneInstanceOverride's -1/-1.0f "inherit" sentinel exactly) -
// per-pattern fields (transpose/rotation/length/inversion/retrograde/m7)
// aren't included yet, since ScenePattern has no established "leave this field untouched"
// convention to reuse the way SceneInstanceOverride already does.
struct RolePreset
{
    std::string id;
    std::string name;
    std::string targetRole;
    std::vector<std::string> tags;

    int activePattern = -1;
    int gridMode = -1;
    float swing = -1.0f;
    int rate = -1;  // -1 = inherit, same convention as activePattern/gridMode
};

// One role's slot within a RhythmicRelationshipPreset - same field shape as
// RolePreset (minus id/name/tags, which belong to the joint preset as a
// whole, not to one role's slot within it).
struct RhythmicRelationshipRoleSlot
{
    std::string targetRole;

    int activePattern = -1;
    int gridMode = -1;
    float swing = -1.0f;
    int rate = -1;  // -1 = inherit, same convention as activePattern/gridMode
};

// The second of the design doc's three preset categories: a *joint*,
// deliberately-correlated choice across two or more roles at once - e.g.
// "polyrhythm_AB": role "motif" gets Binary while role "counterpoint" gets
// Ternary, chosen together on purpose for a specific groove, not picked
// independently per role the way applying two separate RolePresets would.
// Resolved the same way as RolePreset (policy/PresetResolver), just once
// per role slot instead of once.
struct RhythmicRelationshipPreset
{
    std::string id;
    std::string name;
    std::vector<std::string> tags;
    std::vector<RhythmicRelationshipRoleSlot> roleSlots;
};

// One point in an ArcPreset's normalized shape - position 0..1 (0 = start
// of whatever bar range the preset gets stamped onto, 1 = end), value 0..1
// matching ArcBreakpoint's own convention. Normalized rather than absolute
// bars is the whole point: the same "swell" shape should be stampable onto
// a 4-bar section or a 32-bar one without redrawing it.
struct ArcPresetBreakpoint
{
    float position = 0.0f;
    float value = 0.0f;
};

// The third and last of the design doc's three preset categories: a
// reusable named SHAPE for how a value moves over a stretch of the piece
// ("swell", "plateau", "arch") - distinct from policy/Arc's existing
// per-project curves, which are one specific authored curve over absolute
// bar numbers. Deliberately doesn't record which of ArcSet's 5 dimensions
// it targets - that's chosen at apply-time (policy/PresetResolver::
// applyArcPreset), so the same shape is reusable across energy, tension,
// or any other dimension rather than being tied to one.
struct ArcPreset
{
    std::string id;
    std::string name;
    std::vector<std::string> tags;
    std::vector<ArcPresetBreakpoint> breakpoints;
};

// One note within a MotifPreset's cell - relative, not absolute, so the
// same motif applies wherever a section/instance actually sits. offset is
// semitones from the cell's own first note (0 for that note itself);
// duration/velocity are multipliers applied to whatever the motif/rule
// engine's target step already has, not absolute values, matching the
// "widen what a mutation can touch, don't generate from nothing" idiom
// established for CC 60-64 (see docs/composer_mastermind_design.md's v0.6
// section).
struct MotifNote
{
    int semitoneOffset = 0;
    float relativeDuration = 1.0f;
    float relativeVelocity = 1.0f;

    // A rest (2026-09-21): occupies its own slot in the sequence - same
    // index-based spacing as any other entry (policy/MotifEngine.cpp's
    // stampOnePattern) - but writes no note there, just silence for
    // relativeDuration's worth of steps. semitoneOffset/relativeVelocity
    // are unused when true (kept at their defaults, ignored by the stamp
    // algorithm and skipped by Validation's velocity check). Default false
    // so every preset saved before this field existed round-trips unchanged.
    bool isRest = false;
};

// The fourth preset category, added for v0.6's motif/rule engine: a short
// reusable pitch/rhythm cell (a handful of MotifNotes) plus the
// transpose/invert/retrograde/rotate vocabulary already established for
// MPL's own pattern-level operations, applied one level down to this cell
// instead of a whole pattern. Resolved and written at apply-time by the
// motif/rule engine (policy/MotifEngine, not built yet), through MPL's
// existing CC 60-64 Target Step protocol - matched against a blueprint
// section's archetype by `tags`, the same convention every other preset
// category already uses.
struct MotifPreset
{
    std::string id;
    std::string name;
    std::vector<std::string> tags;
    std::vector<MotifNote> notes;
};
