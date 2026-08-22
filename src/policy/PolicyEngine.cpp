#include "PolicyEngine.h"

bool PolicyEngine::authorize(const Mutation& mutation, const std::string& targetRole, int currentBar,
                              const RoleBudget* budgetOverride)
{
    const auto weight = MutationPolicy::classifyWeight(mutation);
    return mutationPolicy.tryConsume(mutation.targetInstance, targetRole, weight, currentBar, budgetOverride);
}

MutationPolicy& PolicyEngine::getMutationPolicy()
{
    return mutationPolicy;
}
