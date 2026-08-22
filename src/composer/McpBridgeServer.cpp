#include "McpBridgeServer.h"
#include <algorithm>

namespace
{
    void sendJson(juce::InterprocessConnection& connection, const juce::var& value)
    {
        const auto json = juce::JSON::toString(value, true);
        connection.sendMessage(juce::MemoryBlock(json.toRawUTF8(), json.getNumBytesAsUTF8()));
    }

    juce::var errorResponse(const juce::String& message)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("ok", false);
        obj->setProperty("error", message);
        return juce::var(obj);
    }
}

// One connected client (the future MCP proxy process, or any raw test
// client during development). Stateless beyond the socket itself - every
// message is a complete, independent request, unlike PatternSyncServer's
// per-instance "hello" handshake.
class McpBridgeConnection : public juce::InterprocessConnection
{
public:
    explicit McpBridgeConnection(McpBridgeServer& ownerIn)
        : juce::InterprocessConnection(true), owner(ownerIn)
    {
    }

    ~McpBridgeConnection() override
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
        {
            sendJson(*this, errorResponse("malformed request - expected a JSON object"));
            return;
        }

        sendJson(*this, owner.handleRequest(parsed));
    }

private:
    McpBridgeServer& owner;
};

McpBridgeServer::McpBridgeServer()
{
    beginWaitingForSocket(kPort);
}

McpBridgeServer::~McpBridgeServer()
{
    stop();

    std::lock_guard<std::mutex> lock(connectionsMutex);
    connections.clear();
}

void McpBridgeServer::setRequestHandler(RequestHandler handler)
{
    requestHandler = std::move(handler);
}

juce::var McpBridgeServer::handleRequest(const juce::var& request)
{
    if (!requestHandler)
        return errorResponse("no request handler wired up");

    return requestHandler(request);
}

juce::InterprocessConnection* McpBridgeServer::createConnectionObject()
{
    auto newConnection = std::make_unique<McpBridgeConnection>(*this);
    auto* rawPointer = newConnection.get();

    std::lock_guard<std::mutex> lock(connectionsMutex);
    connections.push_back(std::move(newConnection));
    return rawPointer;
}

void McpBridgeServer::removeConnection(McpBridgeConnection* connection)
{
    std::lock_guard<std::mutex> lock(connectionsMutex);

    connections.erase(std::remove_if(connections.begin(), connections.end(),
                           [connection](const std::unique_ptr<McpBridgeConnection>& entry)
                           { return entry.get() == connection; }),
        connections.end());
}
