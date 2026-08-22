#include "ArcSet.h"

namespace
{
    Arc makeDefaultArc()
    {
        Arc arc;
        arc.setBreakpoints({ { 1, 0.3f }, { 64, 0.3f } });
        return arc;
    }
}

ArcSet::ArcSet()
{
    for (const auto* name : { "energy", "tension", "density", "complexity", "coherence" })
        arcs.push_back({ name, makeDefaultArc() });
}

ArcSet::ArcSet(const ArcSet& other)
{
    std::lock_guard<std::mutex> lock(other.arcsMutex);
    arcs = other.arcs;
}

ArcSet& ArcSet::operator=(const ArcSet& other)
{
    if (this == &other)
        return *this;

    std::scoped_lock lock(arcsMutex, other.arcsMutex);
    arcs = other.arcs;
    return *this;
}

std::vector<std::string> ArcSet::getArcNames() const
{
    std::lock_guard<std::mutex> lock(arcsMutex);

    std::vector<std::string> names;
    names.reserve(arcs.size());
    for (const auto& entry : arcs)
        names.push_back(entry.first);

    return names;
}

Arc ArcSet::getArc(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(arcsMutex);

    for (const auto& entry : arcs)
    {
        if (entry.first == name)
            return entry.second;
    }

    return {};
}

void ArcSet::setArc(const std::string& name, const Arc& arc)
{
    std::lock_guard<std::mutex> lock(arcsMutex);

    for (auto& entry : arcs)
    {
        if (entry.first == name)
        {
            entry.second = arc;
            return;
        }
    }
}
