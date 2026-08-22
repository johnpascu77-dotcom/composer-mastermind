#pragma once

#include <vector>
#include "../model/ComposerState.h"

// Measures how much currently-registered instances agree with each other
// right now - the technical shape of "convergence" from
// composer_mastermind_design.md's coherence/convergence arc concept, mined
// from atonal_phrase_engine's Parameter Concordance Diagnostics. This is a
// diagnostic over InstanceStateTracker's recorded state, not something that
// drives behavior yet (no Blueprint JSON exists to author target coherence
// per section) - see the design doc for what's deferred.
namespace CoherenceEvaluator
{
    // 0 = maximally divergent, 1 = every instance identical on every
    // sampled dimension. Fewer than 2 states has nothing to diverge from,
    // so it returns 1.
    float evaluate(const std::vector<InstanceParameterState>& states);
}
