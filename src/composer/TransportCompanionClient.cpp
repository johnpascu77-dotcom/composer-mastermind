#include "TransportCompanionClient.h"

#include <juce_core/juce_core.h>
#include <cstring>

namespace
{
    // Build a no-argument OSC message: address string (null-terminated, padded
    // to a 4-byte boundary) followed by the type-tag string "," (also padded).
    juce::MemoryBlock buildOscMessage(const char* address)
    {
        juce::MemoryOutputStream stream;

        stream.write(address, std::strlen(address));
        stream.writeByte(0);
        while ((stream.getDataSize() % 4) != 0)
            stream.writeByte(0);

        stream.writeByte(',');
        stream.writeByte(0);
        stream.writeByte(0);
        stream.writeByte(0);

        return stream.getMemoryBlock();
    }
}

TransportCompanionClient::TransportCompanionClient()
    : port(kDefaultPort)
{
    startTimer(150);
}

TransportCompanionClient::~TransportCompanionClient()
{
    stopTimer();
}

void TransportCompanionClient::setPort(int newPort)
{
    port.store(juce::jlimit(1, 65535, newPort));
}

void TransportCompanionClient::timerCallback()
{
    if (!blueprintEndPending.exchange(false))
        return;

    if (!enabled.load())
        return;

    const int targetPort = port.load();

    // Fire-and-forget UDP OSC packet to a Bitwig controller script listening on
    // this port. Off the message thread; if nothing is listening the datagram
    // is simply dropped. IPv4 loopback - Bitwig's OSC server binds "[::]" but
    // is dual-stack, so a 127.0.0.1 packet is delivered.
    juce::Thread::launch([targetPort]
    {
        const juce::MemoryBlock message = buildOscMessage("/mc/blueprint-end");

        juce::DatagramSocket socket(false);
        socket.bindToPort(0); // ephemeral local port
        socket.write("127.0.0.1", targetPort,
                     message.getData(), static_cast<int>(message.getSize()));
    });
}
