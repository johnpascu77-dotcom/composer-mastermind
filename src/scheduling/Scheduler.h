#pragma once

#include <vector>
#include "ScheduledEvent.h"

class Scheduler
{
public:
    void scheduleEvent(const ScheduledEvent& event);
    std::vector<ScheduledEvent> popDueEvents(int currentBar);

private:
    std::vector<ScheduledEvent> eventQueue;
};
