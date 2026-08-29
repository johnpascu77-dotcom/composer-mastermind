#include "MutationPolicy.h"
#include <cstdlib>

MutationWeight MutationPolicy::classifyWeight(const Mutation& mutation)
{
    if (mutation.type == "inversion" || mutation.type == "retrograde" || mutation.type == "m7")
        return MutationWeight::Major; // full-pattern identity change, same weight as Inversion

    if (mutation.type == "transpose")
        return (std::abs(mutation.amount) >= 12) ? MutationWeight::Major : MutationWeight::Medium;

    if (mutation.type == "rotation")
        return (std::abs(mutation.amount) <= 2) ? MutationWeight::Minor : MutationWeight::Medium;

    if (mutation.type == "length")
        return MutationWeight::Medium;

    if (mutation.type == "rate")
        return MutationWeight::Medium; // full pattern-wide rhythmic-character change, but reversible/single-CC -
                                        // not identity-flipping like Inversion/Retrograde/M7's Major

    // Unknown mutation type: default to the cautious middle ground rather
    // than assuming it's harmless.
    return MutationWeight::Medium;
}

RoleBudget MutationPolicy::budgetForRole(const std::string& role)
{
    if (role == "anchor")
        return { 1, 0, 0 };

    if (role == "motif")
        return { 2, 1, 1 };

    if (role == "counterpoint")
        return { 3, 2, 1 };

    return { -1, -1, -1 };
}

void MutationPolicy::setGlobalMaxMajorPerBar(int maxMajor)
{
    std::lock_guard<std::mutex> lock(ledgerMutex);
    globalMaxMajorPerBar = maxMajor;
}

bool MutationPolicy::tryConsume(const std::string& instanceId, const std::string& role,
                                 MutationWeight weight, int currentBar, const RoleBudget* budgetOverride)
{
    std::lock_guard<std::mutex> lock(ledgerMutex);

    if (weight == MutationWeight::Major)
    {
        if (globalMajorBar != currentBar)
        {
            globalMajorBar = currentBar;
            globalMajorCount = 0;
        }

        if (globalMaxMajorPerBar >= 0 && globalMajorCount >= globalMaxMajorPerBar)
            return false;
    }

    const auto budget = budgetOverride ? *budgetOverride : budgetForRole(role);
    auto& ledger = ledgerByInstance[instanceId];

    if (ledger.bar != currentBar)
    {
        ledger.bar = currentBar;
        ledger.minorCount = 0;
        ledger.mediumCount = 0;
        ledger.majorCount = 0;
    }

    int* count = nullptr;
    int limit = -1;

    switch (weight)
    {
        case MutationWeight::Minor:  count = &ledger.minorCount;  limit = budget.maxMinorPerBar;  break;
        case MutationWeight::Medium: count = &ledger.mediumCount; limit = budget.maxMediumPerBar; break;
        case MutationWeight::Major:  count = &ledger.majorCount;  limit = budget.maxMajorPerBar;  break;
    }

    if (limit >= 0 && *count >= limit)
        return false;

    ++(*count);

    if (weight == MutationWeight::Major)
        ++globalMajorCount;

    return true;
}
