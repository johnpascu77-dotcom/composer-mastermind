#include "ModulationRouteLibrary.h"

bool ModulationRouteLibrary::addOrReplaceRoute(const ModulationRoute& route)
{
    if (route.id.empty())
        return false;

    std::lock_guard<std::mutex> lock(routesMutex);

    for (auto& existing : routes)
    {
        if (existing.id == route.id)
        {
            existing = route;
            return true;
        }
    }

    routes.push_back(route);
    return true;
}

bool ModulationRouteLibrary::removeRoute(const std::string& routeId)
{
    std::lock_guard<std::mutex> lock(routesMutex);

    for (auto it = routes.begin(); it != routes.end(); ++it)
    {
        if (it->id == routeId)
        {
            routes.erase(it);
            return true;
        }
    }

    return false;
}

bool ModulationRouteLibrary::getRouteById(const std::string& routeId, ModulationRoute& outRoute) const
{
    std::lock_guard<std::mutex> lock(routesMutex);

    for (const auto& route : routes)
    {
        if (route.id == routeId)
        {
            outRoute = route;
            return true;
        }
    }

    return false;
}

std::vector<ModulationRoute> ModulationRouteLibrary::getAllRoutes() const
{
    std::lock_guard<std::mutex> lock(routesMutex);
    return routes;
}

void ModulationRouteLibrary::clear()
{
    std::lock_guard<std::mutex> lock(routesMutex);
    routes.clear();
}
