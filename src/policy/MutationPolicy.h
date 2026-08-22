#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include "../model/Mutation.h"

// How much perceptual weight a single Mutation carries, per
// docs/mutation_policy_v0_1.md's classification table.
enum class MutationWeight
{
    Minor,
    Medium,
    Major
};

// Per-role change allowance, consumed once per bar. -1 means unlimited.
// Presets below are a v0.4 approximation of the source roadmap's §7 role
// descriptions; multi-bar cooldowns (a real "rare" rather than "zero per
// single bar") are deferred to v0.5+ once arcs can modulate budgets over
// a section instead of resetting flatly every bar.
struct RoleBudget
{
    int maxMinorPerBar = -1;
    int maxMediumPerBar = -1;
    int maxMajorPerBar = -1;
};

// Classifies mutations and enforces per-instance/role and system-wide
// per-bar budgets. Mutex-guarded and JUCE-free (called from both the audio
// thread via scheduled/chain-driven routing and the UI thread via manual
// test sends) so it stays unit testable without a plugin host.
class MutationPolicy
{
public:
    static MutationWeight classifyWeight(const Mutation& mutation);

    // Unrecognized/empty roles get an unrestricted budget - governance is
    // opt-in per instance, not a silent behavior change for existing setups.
    static RoleBudget budgetForRole(const std::string& role);

    void setGlobalMaxMajorPerBar(int maxMajor);

    // Returns true and records the change against this bar's ledger if the
    // instance's role budget (and the system-wide major cap) allow it;
    // returns false, and nothing is recorded, otherwise. If budgetOverride is
    // non-null, it's used in place of budgetForRole(role) - the caller
    // (Router, via a blueprint-section resolver) has already decided this
    // bar falls inside a section that overrides this role's budget.
    bool tryConsume(const std::string& instanceId, const std::string& role, MutationWeight weight, int currentBar,
                     const RoleBudget* budgetOverride = nullptr);

private:
    struct InstanceLedger
    {
        int bar = -1;
        int minorCount = 0;
        int mediumCount = 0;
        int majorCount = 0;
    };

    std::mutex ledgerMutex;
    std::unordered_map<std::string, InstanceLedger> ledgerByInstance;

    int globalMaxMajorPerBar = 1;
    int globalMajorBar = -1;
    int globalMajorCount = 0;
};
