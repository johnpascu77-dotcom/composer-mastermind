# Routing Policy v0.1

Defines how Composer Mastermind targets and communicates with MIDI Pattern Launcher (MPL) instances. Implemented in
[`midi/CCMapping.h`](../src/midi/CCMapping.h), [`midi/CCDispatcher`](../src/midi/CCDispatcher.h), and
[`routing/Router`](../src/routing/Router.h). See [composer_mastermind_design.md](composer_mastermind_design.md) for
how this fits the larger roadmap.

## Transport

MIDI CC only, per the source roadmap §14.1: portable, DAW-independent, recordable, inspectable. No SysEx (MPL's
separate "Composer Bridge" SysEx protocol, `..\Docs\ComposerBridgeProtocol.md`, is a different higher-bandwidth
channel for full step-data read/write and is out of scope here).

## Instance Targeting

One MIDI channel per MPL instance (§14.2). `Instance.midiChannel` (1-16) must match that instance's own
"External Control Channel" parameter. Composer Mastermind does not currently support "All" broadcast (channel 0)
targeting from its own side — every registered `Instance` names one specific channel, so distinguishing instances is
always possible even if the underlying MIDI routing merges their streams.

Bitwig-side routing model: Composer Mastermind runs on one instrument track (MIDI out, silent audio); each MPL
instance runs on its own track with its MIDI input source set to the Composer Mastermind track.

## CC Map (currently live in MPL)

7-bit values (0-127), decoded by MPL as `plain = min + round((cc/127) * (max-min))`; `CCMapping.h` encodes the exact
inverse.

| CC | Target | Plain range | Encoder |
|---|---|---|---|
| 20 | Active Pattern | 0=stop, 1-3 | `encodeActivePattern` |
| 22 | Grid Mode | 0=binary, 1=ternary | `encodeGridMode` |
| 24 | Swing | 0-75% | `encodeSwing` |
| 30/31/32/33 | P1 Transpose/Rotation/Length/Inversion | -48..48 / 0..15 / 1..16 / off\<64,on\>=64 | `encodeTranspose`/`encodeRotation`/`encodeLength`/`encodeInversion` |
| 40/41/42/43 | P2 same | | |
| 50/51/52/53 | P3 same | | |

`patternBaseCC(index)` maps pattern index 0/1/2 → base CC 30/40/50; `mutationOffsetForType` maps a `Mutation::type`
string to the +0/+1/+2/+3 offset within that block.

## CC Map (reserved by the roadmap, not yet live in MPL)

CC 21 (Target Pattern), CC 23 (Editor View Mode), CC 60-64 (Target Step: step/note/velocity/duration/enabled) are
documented in the source roadmap's §15.1 default map but are **not implemented** in MPL's current
`handleExternalControlCC`. Do not add these to `CCMapping.h` until MPL actually implements them (its own roadmap
items v1.19.0/v1.20.0) and the protocol is verified — otherwise Composer Mastermind would silently send CC that MPL
ignores, which is worse than not sending it, because it would look like a routing bug on this side.

## Two Routing Paths

- **Scene routing** (`Router::routeScene`): global fields (`Scene::global`, CC 20/22/24) go to every instance in
  `Scene::targets`; per-pattern fields (`Scene::patterns`, a `ScenePattern` per target) go to CC 30-53 blocks on
  whichever instance each `ScenePattern::targetInstance` names. A scene can therefore set one instance's global
  pattern/swing while separately setting another instance's per-pattern transpose, in one call.
- **Mutation routing** (`Router::routeMutation`): a single targeted change to one instance's one pattern's one
  parameter. No budget/governor gate yet — every mutation passed in is sent immediately. That gate is planned for
  v0.4 of the design doc's roadmap (`policy/PolicyEngine`); until then, `routeMutation` is intentionally "dumb pipe."

## Known Limitation: Fixed Pattern Count

`CCMapping::kMaxPatterns = 3`, matching MPL's current `numPatterns`. If MPL's pattern count ever changes, every CC
encoding here becomes wrong silently (no error, just musically incorrect values reaching MPL). See
"Instance capability negotiation" in the design doc for the proposed fix — not yet implemented.
