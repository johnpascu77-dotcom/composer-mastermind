#pragma once

#include <vector>
#include <string>
#include <mutex>
#include "../model/ModulationRoute.h"

// A named set of ModulationRoutes. Mirrors ModulatorTargetLibrary's
// copy-returning, mutex-guarded style for the same reason - authored/edited
// from the UI thread, read every bar from ComposerCore's audio-thread tick.
class ModulationRouteLibrary
{
public:
    bool addOrReplaceRoute(const ModulationRoute& route);
    bool removeRoute(const std::string& routeId);

    bool getRouteById(const std::string& routeId, ModulationRoute& outRoute) const;
    std::vector<ModulationRoute> getAllRoutes() const;

    void clear();

private:
    std::vector<ModulationRoute> routes;
    mutable std::mutex routesMutex;
};
