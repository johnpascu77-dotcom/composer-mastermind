#include "PatternSyncServer.h"
#include <algorithm>

namespace
{
    void sendJson(juce::InterprocessConnection& connection, const juce::var& value)
    {
        const auto json = juce::JSON::toString(value, true);
        connection.sendMessage(juce::MemoryBlock(json.toRawUTF8(), json.getNumBytesAsUTF8()));
    }
}

// One connected MPL instance. Stores the channel it reported in its own
// "hello" handshake so PatternSyncServer::requestSync can address it
// directly - a real duplex connection already disambiguates instances, so
// unlike the (removed) SysEx protocol there's no channel-echo bookkeeping
// needed on responses.
class PatternSyncConnection : public juce::InterprocessConnection
{
public:
    explicit PatternSyncConnection(PatternSyncServer& ownerIn)
        : juce::InterprocessConnection(true), owner(ownerIn)
    {
    }

    ~PatternSyncConnection() override
    {
        disconnect(); // required by InterprocessConnection's own contract
    }

    void connectionMade() override
    {
    }

    void connectionLost() override
    {
        // Can't safely delete `this` synchronously from within our own
        // callback - defer to the next message-loop iteration.
        auto* self = this;
        auto& ownerRef = owner;
        juce::MessageManager::callAsync([&ownerRef, self] { ownerRef.removeConnection(self); });
    }

    void messageReceived(const juce::MemoryBlock& message) override
    {
        const auto json = juce::String::fromUTF8(static_cast<const char*>(message.getData()),
            static_cast<int>(message.getSize()));

        juce::var parsed;
        if (juce::JSON::parse(json, parsed).failed() || !parsed.isObject())
            return;

        const auto type = parsed["type"].toString();

        if (type == "hello")
        {
            channel = static_cast<int>(parsed["channel"]);
            return;
        }

        if (type == "patternDumpResponse")
        {
            if (channel < 0)
                return; // no hello yet - can't attribute this to an instance

            PatternSnapshot snapshot;
            snapshot.patternIndex = static_cast<int>(parsed["patternIndex"]);

            if (auto* stepsArray = parsed["steps"].getArray())
            {
                for (const auto& stepVar : *stepsArray)
                {
                    StepSnapshot step;
                    step.enabled = static_cast<bool>(stepVar["enabled"]);
                    step.note = static_cast<int>(stepVar["note"]);
                    step.velocity = static_cast<int>(stepVar["velocity"]);
                    step.duration = static_cast<int>(stepVar["duration"]);
                    snapshot.steps.push_back(step);
                }
            }

            owner.handlePatternDumpResponse(channel, snapshot);
        }
    }

    void sendPatternDumpRequest(int patternIndex)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("type", "requestPatternDump");
        obj->setProperty("patternIndex", patternIndex);
        sendJson(*this, juce::var(obj));
    }

    void sendWriteStepMessage(int patternIndex, int stepIndex, bool enabled, int note, int velocity, int duration)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("type", "writeStep");
        obj->setProperty("patternIndex", patternIndex);
        obj->setProperty("stepIndex", stepIndex);
        obj->setProperty("enabled", enabled);
        obj->setProperty("note", note);
        obj->setProperty("velocity", velocity);
        obj->setProperty("duration", duration);
        sendJson(*this, juce::var(obj));
    }

    void sendWriteFullPatternMessage(int patternIndex, const std::vector<StepSnapshot>& steps)
    {
        juce::Array<juce::var> stepsArray;

        for (const auto& step : steps)
        {
            auto* stepObj = new juce::DynamicObject();
            stepObj->setProperty("enabled", step.enabled);
            stepObj->setProperty("note", step.note);
            stepObj->setProperty("velocity", step.velocity);
            stepObj->setProperty("duration", step.duration);
            stepsArray.add(juce::var(stepObj));
        }

        auto* obj = new juce::DynamicObject();
        obj->setProperty("type", "writeFullPattern");
        obj->setProperty("patternIndex", patternIndex);
        obj->setProperty("steps", stepsArray);
        sendJson(*this, juce::var(obj));
    }

    int getChannel() const { return channel; }

private:
    PatternSyncServer& owner;
    int channel = -1;
};

PatternSyncServer::PatternSyncServer()
{
    beginWaitingForSocket(kPort);
}

PatternSyncServer::~PatternSyncServer()
{
    stop();

    std::lock_guard<std::mutex> lock(connectionsMutex);
    connections.clear();
}

void PatternSyncServer::setChannelResolver(ChannelResolver resolver)
{
    channelResolver = std::move(resolver);
}

void PatternSyncServer::setCurrentBarProvider(CurrentBarProvider provider)
{
    currentBarProvider = std::move(provider);
}

void PatternSyncServer::requestSync(int channel, int patternIndex)
{
    std::lock_guard<std::mutex> lock(connectionsMutex);

    for (auto& connection : connections)
    {
        if (connection->getChannel() == channel)
        {
            connection->sendPatternDumpRequest(patternIndex);
            return;
        }
    }
}

void PatternSyncServer::sendWriteStep(int channel, int patternIndex, int stepIndex, bool enabled, int note,
                                       int velocity, int duration)
{
    std::lock_guard<std::mutex> lock(connectionsMutex);

    for (auto& connection : connections)
    {
        if (connection->getChannel() == channel)
        {
            connection->sendWriteStepMessage(patternIndex, stepIndex, enabled, note, velocity, duration);
            return;
        }
    }
}

void PatternSyncServer::sendWriteFullPattern(int channel, int patternIndex, const std::vector<StepSnapshot>& steps)
{
    std::lock_guard<std::mutex> lock(connectionsMutex);

    for (auto& connection : connections)
    {
        if (connection->getChannel() == channel)
        {
            connection->sendWriteFullPatternMessage(patternIndex, steps);
            return;
        }
    }
}

bool PatternSyncServer::isChannelConnected(int channel) const
{
    std::lock_guard<std::mutex> lock(connectionsMutex);

    for (const auto& connection : connections)
        if (connection->getChannel() == channel)
            return true;

    return false;
}

InstancePatternCache& PatternSyncServer::getCache()
{
    return cache;
}

juce::InterprocessConnection* PatternSyncServer::createConnectionObject()
{
    auto newConnection = std::make_unique<PatternSyncConnection>(*this);
    auto* rawPointer = newConnection.get();

    std::lock_guard<std::mutex> lock(connectionsMutex);
    connections.push_back(std::move(newConnection));
    return rawPointer;
}

void PatternSyncServer::handlePatternDumpResponse(int channel, const PatternSnapshot& snapshot)
{
    std::string instanceId;
    if (!channelResolver || !channelResolver(channel, instanceId))
        return; // no registered instance on this channel - nothing to attribute it to

    const int currentBar = currentBarProvider ? currentBarProvider() : -1;
    cache.store(instanceId, snapshot, currentBar);
}

void PatternSyncServer::removeConnection(PatternSyncConnection* connection)
{
    std::lock_guard<std::mutex> lock(connectionsMutex);

    connections.erase(std::remove_if(connections.begin(), connections.end(),
                           [connection](const std::unique_ptr<PatternSyncConnection>& entry)
                           { return entry.get() == connection; }),
        connections.end());
}
