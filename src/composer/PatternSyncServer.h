#pragma once

#include <juce_events/juce_events.h>
#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <mutex>
#include "../state/InstancePatternCache.h"

class PatternSyncConnection; // defined in PatternSyncServer.cpp - an implementation detail

// Local-socket server MPL instances connect to for real pattern-content
// awareness - built because Bitwig does not deliver incoming SysEx to a
// hosted VST instrument's plugin code (community-reported, confirmed
// empirically 2026-08-19: CC round-trips fine over identical routing, SysEx
// never does), which made the originally-planned MPL Composer Bridge SysEx
// read channel unusable while both plugins run inside Bitwig. This replaces
// that approach entirely on Composer Mastermind's side (the old
// midi/ComposerBridgeCodec + SysEx-based PatternSyncCoordinator were
// removed, not kept alongside - they had no other purpose). MPL's own SysEx
// Composer Bridge protocol is untouched and still used by MPL's matching
// ComposerBridgeIpcClient's counterpart - the client, not this server -
// only for its own step-editing use elsewhere; here it's purely transport.
//
// One MPL instance = one persistent connection, each self-identifying with
// its externalControlChannelParam value via a "hello" message on connect -
// unlike SysEx, a real duplex connection already disambiguates which
// instance a message came from, so there's no channel-echo bookkeeping
// needed the way the (removed) SysEx protocol required.
//
// Protocol (JSON over InterprocessConnection's own message framing):
//   MPL -> CM, on connect:      {"type":"hello","channel":<int>}
//   CM -> MPL:                  {"type":"requestPatternDump","patternIndex":<int>}
//   MPL -> CM:                  {"type":"patternDumpResponse","patternIndex":<int>,
//                                 "steps":[{"enabled":bool,"note":int,"velocity":int,
//                                 "duration":int}, ...]}
//   CM -> MPL:                  {"type":"writeStep","patternIndex":<int>,"stepIndex":<int>,
//                                 "enabled":bool,"note":int,"velocity":int,"duration":int}
//   CM -> MPL:                  {"type":"writeFullPattern","patternIndex":<int>,
//                                 "steps":[{"enabled":bool,"note":int,"velocity":int,
//                                 "duration":int}, ...16 entries]}
// The two write message types (added 2026-08-21, see sendWriteStep/
// sendWriteFullPattern) exist because policy/MotifEngine's step-content
// writes went through MPL's CC 60-64 Target Step Editing protocol until
// this point - ordinary APVTS parameters, committed via MPL's own
// syncEngineFromParameters() polling once per audio block. That's fine for
// one step at a time, but MotifEngine's new section-entry "stamp" (writing
// several steps at once) exposed a real bug: several step-writes landing
// within the same block silently collapse to whatever the last one left the
// parameters at, since the poll only ever observes state at the moment it
// runs, not every value it passed through (confirmed live, 2026-08-21).
// This same duplex connection already delivers pattern reads reliably
// (Bitwig won't route SysEx to a hosted plugin - see this file's own
// opening comment - so MPL's matching SysEx write commands 0x01/0x04 aren't
// reachable here either), so extending it with two write message types was
// more direct than pacing CC delivery one write per block: each write
// message is a single complete record, applied synchronously and atomically
// on MPL's side (setStepValues/clearStep directly, no APVTS/polling
// involved) the instant it's received - no pacing needed by construction.
// Must be kept in sync by hand with MPL's Source/ComposerBridgeIpcClient -
// the two projects don't share a header.
class PatternSyncServer : private juce::InterprocessConnectionServer
{
public:
    PatternSyncServer();
    ~PatternSyncServer() override;

    // Resolves a connected instance's reported channel to a registered
    // Instance's id (matching Instance::midiChannel) - set once by
    // ComposerCore's constructor, mirrors routing/Router.h's
    // BudgetOverrideResolver/ReservedValueChecker injection pattern.
    using ChannelResolver = std::function<bool(int channel, std::string& outInstanceId)>;
    void setChannelResolver(ChannelResolver resolver);

    // Supplies "what bar is it right now" for cache timestamping - same
    // injection pattern as the channel resolver.
    using CurrentBarProvider = std::function<int()>;
    void setCurrentBarProvider(CurrentBarProvider provider);

    // Requests a pattern dump from whichever connected MPL instance
    // reported this channel in its hello handshake. Silently does nothing
    // if no connection has identified itself with this channel yet.
    void requestSync(int channel, int patternIndex);

    // Writes one step directly into MPL's real pattern storage - atomic,
    // no CC/parameter-polling involved (see this file's own class comment
    // for why this exists instead of going through CC 60-64). Silently does
    // nothing if no connection has identified itself with this channel yet.
    void sendWriteStep(int channel, int patternIndex, int stepIndex, bool enabled, int note, int velocity,
                        int duration);

    // Writes an entire pattern (all steps) directly into MPL's real storage
    // in one atomic message - what policy/MotifEngine's section-entry
    // "stamp" uses instead of writing steps one at a time. `steps` should
    // have one entry per step (MPL's own fixed step count); a short vector
    // just leaves MPL's remaining steps untouched, not implicitly cleared.
    void sendWriteFullPattern(int channel, int patternIndex, const std::vector<StepSnapshot>& steps);

    // True if some connection has identified itself with this channel -
    // for UI display, so "nothing happened" is distinguishable from "not
    // even connected."
    bool isChannelConnected(int channel) const;

    InstancePatternCache& getCache();

private:
    friend class PatternSyncConnection;

    juce::InterprocessConnection* createConnectionObject() override;

    void handlePatternDumpResponse(int channel, const PatternSnapshot& snapshot);
    void removeConnection(PatternSyncConnection* connection);

    static constexpr int kPort = 47823; // must match MPL's ComposerBridgeIpcClient.cpp

    mutable std::mutex connectionsMutex;
    std::vector<std::unique_ptr<PatternSyncConnection>> connections;

    ChannelResolver channelResolver;
    CurrentBarProvider currentBarProvider;
    InstancePatternCache cache;
};
