#include "CCDispatcher.h"
#include <iostream>

void CCDispatcher::sendCC(int channel, int controller, int value)
{
    if (debugEnabled)
    {
        std::cout << "[CC] channel=" << channel
                  << " controller=" << controller
                  << " value=" << value << std::endl;
    }

    std::lock_guard<std::mutex> lock(pendingMutex);
    pending.push_back({ channel, controller, value });
}

void CCDispatcher::sendBatch(const std::vector<CCMessage>& messages)
{
    for (const auto& message : messages)
    {
        sendCC(message.channel, message.controller, message.value);
    }
}

void CCDispatcher::setDebugEnabled(bool shouldLog)
{
    debugEnabled = shouldLog;
}

std::vector<CCMessage> CCDispatcher::takePending()
{
    std::lock_guard<std::mutex> lock(pendingMutex);
    std::vector<CCMessage> result;
    result.swap(pending);
    return result;
}
