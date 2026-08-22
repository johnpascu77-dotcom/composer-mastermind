#pragma once

#include <juce_events/juce_events.h>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class McpBridgeConnection; // defined in McpBridgeServer.cpp - an implementation detail

// Local-socket JSON request/response server exposing Composer Mastermind's
// own internal state (Awareness, Instances, active blueprint/section, motif
// presets, coherence) for external inspection - the read-only first slice of
// the MCP bridge, built after a session's worth of live debugging that could
// only proceed via screenshot round-trips of the plugin's own tabs.
//
// Deliberately NOT an MCP server itself - implementing the actual MCP
// JSON-RPC/tool-listing protocol in C++ is out of scope for this slice. A
// small standalone MCP server (elsewhere, likely Node.js) is meant to sit in
// front of this socket and translate MCP tool calls into requests here -
// mirrors PatternSyncServer's own shape exactly: this class is pure
// transport, ComposerCore owns the actual logic via an injected handler.
//
// Protocol (JSON over InterprocessConnection's own message framing, one
// request per message, connection stays open for repeated requests):
//   client -> CM: {"action":"<name>"}
//   CM -> client: {"ok":true,"result":{...}} or {"ok":false,"error":"..."}
// See ComposerCore::handleMcpBridgeRequest for the actual action table.
class McpBridgeServer : private juce::InterprocessConnectionServer
{
public:
    McpBridgeServer();
    ~McpBridgeServer() override;

    // Answers a parsed request - set once by ComposerCore's constructor,
    // mirrors PatternSyncServer's ChannelResolver/CurrentBarProvider
    // injection pattern so this class stays pure transport, no knowledge of
    // ComposerCore's internals.
    using RequestHandler = std::function<juce::var(const juce::var& request)>;
    void setRequestHandler(RequestHandler handler);

private:
    friend class McpBridgeConnection;

    juce::InterprocessConnection* createConnectionObject() override;
    juce::var handleRequest(const juce::var& request);
    void removeConnection(McpBridgeConnection* connection);

    static constexpr int kPort = 47824; // one above PatternSyncServer's 47823 - keep in sync with any external MCP client

    mutable std::mutex connectionsMutex;
    std::vector<std::unique_ptr<McpBridgeConnection>> connections;

    RequestHandler requestHandler;
};
