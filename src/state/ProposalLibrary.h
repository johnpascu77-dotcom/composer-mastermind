#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <vector>

// A folder of saved "Propose a Piece" outputs (policy/DurationCalculator.h,
// policy/ArcShapeLibrary.h, policy/BlueprintGenerator::generateSeedScene),
// so a proposal can be listed/browsed/reloaded from the UI rather than
// requiring a file-chooser round trip every time - the same "folder of
// named files, list on demand" idea as the sibling plugin OrchConductor's
// own NarrativeLibrary.json auto-load pattern, just one file per proposal
// here instead of one file holding a whole list. Deliberately reuses
// state/CompositionBundleStore's existing bundle JSON format unmodified as
// the artifact content - a bundle already contains everything a proposal
// needs (Blueprint + every Scene it references + registered Instances +
// tag-matched MotifPresets + referenced ModulatorTargets), so this class
// does no JSON parsing of its own; it only manages files on disk.
// Class, not namespace (matches state/BlueprintLibrary, state/SceneLibrary,
// state/PresetLibrary's own shape) - it owns a listing synced to a real
// folder, not a stateless per-call operation.
class ProposalLibrary
{
public:
    struct ProposalSummary
    {
        std::string id;          // filename stem, unique (name + save timestamp)
        std::string name;        // the name the user gave it at save time
        juce::Time createdAt;
    };

    // Lists every proposal currently saved under
    // <Documents>/ComposerMastermind/Proposals/, newest first. Never
    // throws/asserts on a missing or empty folder - returns an empty list.
    std::vector<ProposalSummary> listProposals() const;

    // Sanitizes name for use in a filename, appends a save timestamp so
    // saving the same name twice never collides, writes bundleJson
    // verbatim (no reformatting/validation - that's
    // CompositionBundleStore::importBundle's job on load), creating the
    // Proposals folder if it doesn't exist yet. Returns false only on a
    // real filesystem failure.
    bool saveProposal(const std::string& name, const juce::String& bundleJson);

    // Reads back exactly what saveProposal wrote for this id. Returns
    // false (outBundleJson left untouched) if no such proposal exists.
    bool loadProposal(const std::string& id, juce::String& outBundleJson) const;

    // Permanently deletes the saved proposal file for this id. Returns
    // false if no such proposal exists, or the file couldn't be removed.
    bool deleteProposal(const std::string& id);

private:
    juce::File getProposalsFolder() const;
};
