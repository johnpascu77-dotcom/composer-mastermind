#include "ProposalLibrary.h"
#include <algorithm>

namespace
{
    // Filesystem-safe: keep alphanumerics/space/dash, replace everything
    // else (including any "__" the user typed, which is reserved below as
    // the name/timestamp separator) with a single underscore.
    juce::String sanitizeForFilename(const std::string& name)
    {
        juce::String result;
        for (const auto ch : juce::String(name))
        {
            if (juce::CharacterFunctions::isLetterOrDigit(ch) || ch == ' ' || ch == '-')
                result += ch;
            else
                result += '_';
        }

        result = result.trim();
        return result.isEmpty() ? juce::String("Untitled Proposal") : result;
    }
}

juce::File ProposalLibrary::getProposalsFolder() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("ComposerMastermind")
        .getChildFile("Proposals");
}

std::vector<ProposalLibrary::ProposalSummary> ProposalLibrary::listProposals() const
{
    std::vector<ProposalSummary> summaries;

    const auto folder = getProposalsFolder();
    if (!folder.isDirectory())
        return summaries;

    for (const auto& file : folder.findChildFiles(juce::File::findFiles, false, "*.json"))
    {
        ProposalSummary summary;
        summary.id = file.getFileNameWithoutExtension().toStdString();
        summary.createdAt = file.getLastModificationTime();

        const auto stem = file.getFileNameWithoutExtension();
        const auto separatorIndex = stem.lastIndexOf("__");
        summary.name = (separatorIndex >= 0 ? stem.substring(0, separatorIndex) : stem).toStdString();

        summaries.push_back(summary);
    }

    std::sort(summaries.begin(), summaries.end(),
               [](const ProposalSummary& a, const ProposalSummary& b) { return a.createdAt > b.createdAt; });

    return summaries;
}

bool ProposalLibrary::saveProposal(const std::string& name, const juce::String& bundleJson)
{
    const auto folder = getProposalsFolder();
    if (!folder.isDirectory() && !folder.createDirectory().wasOk())
        return false;

    const auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    const auto filename = sanitizeForFilename(name) + "__" + timestamp + ".json";

    return folder.getChildFile(filename).replaceWithText(bundleJson);
}

bool ProposalLibrary::loadProposal(const std::string& id, juce::String& outBundleJson) const
{
    const auto file = getProposalsFolder().getChildFile(juce::String(id) + ".json");
    if (!file.existsAsFile())
        return false;

    outBundleJson = file.loadFileAsString();
    return true;
}

bool ProposalLibrary::deleteProposal(const std::string& id)
{
    const auto file = getProposalsFolder().getChildFile(juce::String(id) + ".json");
    if (!file.existsAsFile())
        return false;

    return file.deleteFile();
}
