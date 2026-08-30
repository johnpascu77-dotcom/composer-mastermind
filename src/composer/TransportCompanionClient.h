#pragma once

#include <juce_events/juce_events.h>
#include <atomic>

// Fires a one-shot UDP OSC packet ("/mc/blueprint-end") to a Bitwig controller
// script when the blueprint reaches its end, so the script can call
// transport.stop() - a plugin can't touch the host transport itself, and this
// rig can't loop a loopMIDI port back to a controller input, so OSC/UDP is the
// path (Bitwig's OSC module is the documented way for an external app to poke
// the host).
//
// signalBlueprintEnd() is audio-thread safe (just raises an atomic flag). The
// actual send runs on a short throwaway thread launched from the timer
// (message thread), so nothing blocks audio or the UI.
class TransportCompanionClient : private juce::Timer
{
public:
    static constexpr int kDefaultPort = 47825; // next after McpBridgeServer's 47824

    TransportCompanionClient();
    ~TransportCompanionClient() override;

    void setEnabled(bool shouldBeEnabled) { enabled.store(shouldBeEnabled); }
    void setPort(int newPort);

    // Call from anywhere, including the audio thread.
    void signalBlueprintEnd() { blueprintEndPending.store(true); }

private:
    void timerCallback() override;

    std::atomic<bool> enabled { true };
    std::atomic<int> port;
    std::atomic<bool> blueprintEndPending { false };
};
