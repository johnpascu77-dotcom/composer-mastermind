#include "Scheduler.h"
#include <algorithm>

void Scheduler::scheduleEvent(const ScheduledEvent& event)
{
    eventQueue.push_back(event);
}

std::vector<ScheduledEvent> Scheduler::popDueEvents(int currentBar)
{
    std::vector<ScheduledEvent> due;
    std::vector<ScheduledEvent> remaining;

    for (const auto& event : eventQueue)
    {
        if (event.targetBar <= currentBar)
            due.push_back(event);
        else
            remaining.push_back(event);
    }

    eventQueue = remaining;

    std::sort(due.begin(), due.end(), [](const ScheduledEvent& a, const ScheduledEvent& b)
    {
        return a.priority > b.priority;
    });

    return due;
}
