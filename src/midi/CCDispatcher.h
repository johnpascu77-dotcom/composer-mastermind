#pragma once

#include <vector>
#include <mutex>

struct CCMessage
{
    int channel = 1;
    int controller = 0;
    int value = 0;
};

// Messages are queued from whichever thread builds scenes/mutations (the UI
// message thread, and eventually the audio thread via scheduled events) and
// drained from the audio thread each block, so the queue is mutex-guarded.
class CCDispatcher
{
public:
    void sendCC(int channel, int controller, int value);
    void sendBatch(const std::vector<CCMessage>& messages);

    void setDebugEnabled(bool shouldLog);

    // Returns and clears all messages queued since the last call.
    std::vector<CCMessage> takePending();

private:
    bool debugEnabled = false;
    std::vector<CCMessage> pending;
    std::mutex pendingMutex;
};
