#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include "../composer/ComposerCore.h"

// Builds/imports a single, self-contained composition as one JSON file -
// narrower than StateSnapshotStore, which round-trips the entire session's
// libraries at once. Used by the editor's Export/Import buttons (Sections
// tab), and matches exactly the JSON shape the MCP bridge's create_scene/
// create_blueprint/create_motif_preset actions already accept - "composing
// by numbers" and this file format are the same thing.
//
// v2 (2026-08-27): originally just a Blueprint plus the Scenes its sections
// reference - correct for CC-encodable parameters, but silently incomplete
// for anything else a piece depends on. Confirmed live 2026-08-25 by an
// actual bug: a blueprint whose sections reference a Motif Preset that
// isn't in the *importing* session's library stamps nothing and plays
// silence, no error - MotifEngine::stampMotifForSection's "nothing
// authored for this archetype yet" no-op is correct behaviour in general,
// but wrong for a file that's supposed to be a self-contained score. Same
// risk existed for Instances - build/peak/release archetypes select
// instances by *role*, not by anything explicitly named in the Scene/
// Blueprint JSON, so an importing session with different (or no)
// registered instances would silently touch the wrong ones or nothing at
// all. Now bundles both, so importing into a genuinely empty session
// reconstructs the whole piece, not just its CC-parameter shape.
//
// v3 (2026-08-27): adds ModulationRoute (the modulation-matrix feature,
// model/ModulationRoute.h) - included by instance membership (every route
// targeting "*" or one of the bundle's own gathered instances), since a
// route isn't referenced indirectly through a section's fields the way a
// ModulatorTarget is.
namespace CompositionBundleStore
{
    // Looks up blueprintId in core's BlueprintLibrary, every SceneLibrary
    // scene its sections reference (deduped, including any
    // SectionCapturedContent they carry), every currently-registered
    // Instance (all of them, not just ones explicitly named - see the v2
    // note above on why role-based eligibility makes a referenced-only
    // subset unsafe), every Motif Preset tagged with an archetype this
    // blueprint's sections actually use (the *whole* matching pool per
    // archetype, not just whichever one findPresetForArchetype would
    // currently pick - preserves novelty-aware cycling between them on
    // import), and every Modulator Target referenced by a section's
    // modulatorValues (skipped if no longer registered, same tolerance
    // sendSectionModulatorValues already has at playback time). Returns an
    // empty string and sets errorMessage if the blueprint doesn't exist or
    // references a scene that isn't in the library - never emits a bundle
    // that would fail to import cleanly.
    juce::String createBundle(ComposerCore& core, const std::string& blueprintId, std::string& errorMessage);

    // All-or-nothing: validates the parsed blueprint, every parsed scene,
    // instance, motif preset, and modulator target, and that every
    // section's sceneId is actually present among the scenes included in
    // this file, before touching any library. On any failure returns false
    // with errorMessage set and leaves every library untouched - unlike
    // StateSnapshotStore::restoreSnapshot, which skips bad entries one at a
    // time (right for a whole-session merge, wrong for a single composition
    // that should either fully work or not import at all). Instances/motif
    // presets/modulator targets are optional keys (an older v1 file simply
    // has none), so this stays backward compatible with bundles exported
    // before v2. On success, adds instances, then motif presets and
    // modulator targets, then the scenes, then the blueprint (overwriting
    // by id, same as every other Save/Load path here) and makes it the
    // active blueprint.
    bool importBundle(const juce::String& json, ComposerCore& core, std::string& errorMessage);
}
