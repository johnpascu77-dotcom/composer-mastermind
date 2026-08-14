#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include "../composer/ComposerCore.h"

// Builds/restores a full plugin snapshot (registered instances + the scene
// library + which scene was last active) as a single JSON string. Used both
// by PluginProcessor's DAW-driven getStateInformation/setStateInformation
// and by the editor's manual file export/import.
namespace StateSnapshotStore
{
    juce::String createSnapshot(ComposerCore& core);

    // Invalid entries (fails Validation::isValid*) are skipped rather than
    // aborting the whole restore, so one bad scene doesn't take down the
    // rest of a snapshot. Returns false only on a JSON parse failure.
    bool restoreSnapshot(const juce::String& json, ComposerCore& core, std::string& errorMessage);
}
