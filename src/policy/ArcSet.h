#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "Arc.h"

// The named collection of arc dimensions from composer_mastermind_design.md's
// v0.5 section: energy/tension/density/complexity/coherence. Owned by
// ComposerCore, edited live from the Blueprint tab's graph editor.
//
// Mutex-guarded even though nothing on the audio thread reads arc values yet
// (Arc itself remains otherwise unconsumed - no Blueprint JSON exists to
// drive scene/mutation routing from an arc's value) - the guard is here from
// the start rather than retrofitted once a consumer exists, same reasoning
// as every other shared-state class in this codebase.
class ArcSet
{
public:
    ArcSet();

    // Explicit rather than defaulted: a std::mutex member makes the
    // implicit copy ctor/assignment deleted, but policy/BlueprintGenerator
    // needs a full-object snapshot to seed a candidate ArcSet a generated
    // proposal can be previewed/edited in before anything touches the live
    // one (see docs/composer_mastermind_design.md's v1.1 section) - each
    // side locks its own mutex rather than copying it.
    ArcSet(const ArcSet& other);
    ArcSet& operator=(const ArcSet& other);

    std::vector<std::string> getArcNames() const;

    // Returns a default-constructed (empty) Arc for an unrecognized name.
    Arc getArc(const std::string& name) const;

    // No-op for an unrecognized name.
    void setArc(const std::string& name, const Arc& arc);

private:
    mutable std::mutex arcsMutex;
    std::vector<std::pair<std::string, Arc>> arcs;
};
