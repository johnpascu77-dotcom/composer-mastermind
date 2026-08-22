#pragma once

#include "MutationPolicy.h"
#include "../model/Mutation.h"

// The single gate all mutation dispatch passes through - classifies the
// mutation's weight and asks MutationPolicy's per-instance/role and
// system-wide budgets whether it's allowed this bar. Router calls this
// before ever encoding/sending CC, for both scheduled/chain-driven and
// manual UI-triggered mutations, so the governor can't be silently bypassed
// by either path.
class PolicyEngine
{
public:
    // If budgetOverride is non-null, it's used in place of MutationPolicy's
    // static per-role table for this one call - see Router::routeMutation
    // for where that override comes from (a blueprint section covering the
    // current bar).
    bool authorize(const Mutation& mutation, const std::string& targetRole, int currentBar,
                    const RoleBudget* budgetOverride = nullptr);

    MutationPolicy& getMutationPolicy();

private:
    MutationPolicy mutationPolicy;
};
