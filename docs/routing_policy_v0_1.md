# Routing Policy v0.1

Defines how Composer Mastermind targets and communicates with MIDI Pattern Launcher (MPL) instances. Implemented in
[`midi/CCMapping.h`](../src/midi/CCMapping.h), [`midi/CCDispatcher`](../src/midi/CCDispatcher.h), and
[`routing/Router`](../src/routing/Router.h). See [composer_mastermind_design.md](composer_mastermind_design.md) for
how this fits the larger roadmap.

## Transport

MIDI CC only for the mutation/scene/CC-map machinery described below, per the source roadmap §14.1: portable,
DAW-independent, recordable, inspectable.

**Update (2026-08-19), superseded same day:** MPL's separate "Composer Bridge" SysEx protocol
(`..\Docs\ComposerBridgeProtocol.md`) was briefly planned as v0.6's read/write channel and bumped to a v2 with
per-instance channel filtering — but live testing found **Bitwig does not deliver incoming SysEx to a hosted VST
instrument's plugin code at all**, a host-level limitation (corroborated by community reports), not something
fixable by routing changes. MPL's v2 protocol itself is untouched and still real/tested/useful for non-Bitwig
contexts (other hosts, hardware, MPL's own Standalone test harness) — it's just not usable for this specific
purpose while both plugins run inside Bitwig.

**What's actually live for v0.6 instead: direct local-socket IPC** (`juce::InterprocessConnection`, fixed port
47823) between Composer Mastermind and each MPL instance, entirely bypassing Bitwig's MIDI graph and its SysEx
limitation. See `docs/technical_spec_checklist.md`'s "v0.6 Composer Bridge read channel" section for the full
mechanism (`composer/PatternSyncServer` on this side, `Source/ComposerBridgeIpcClient` in the MPL project).

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

**Bug found and fixed, twice over (2026-08-16).** First fix: `encodeRotation` clamped out-of-range values to
`[0, 15]` like every other encoder here — but rotation is cyclic (a 16-step pattern rotated by -2 is equivalent
to rotating it by +14), unlike transpose which is genuinely signed. Clamping silently floored any negative
rotation amount to 0. `encodeRotation` now wraps modulo `kPatternSteps` (`CCMapping::wrapRotation`) before
clamping.

That fix was necessary but not sufficient — live-testing it immediately surfaced the deeper issue: `Mutation.amount`
was being treated as an **absolute target value** (`encodeRotation(mutation.amount)` directly), so `-2` meant
"set rotation to the literal value -2," which wraps to +14 — mathematically correct, but not remotely what "a
subtle rotation nudge" should mean as a variation device. A mutation is supposed to be a small perturbation
*relative to whatever the instance is currently doing*, not a full re-specification (that's what a `Scene` is
for). Second fix: `Router::routeMutation` now resolves `amount` as a **delta from the instance's last known
state**, read via `routing/InstanceStateTracker` (built in v0.5, originally just for the coherence evaluator —
turned out to be exactly the missing piece here too): `newValue = currentValue + amount`, then
clamp (transpose/length) or wrap (rotation) into range, then encode *that*. `Mutation::amount`'s doc comment in
[`model/Mutation.h`](../src/model/Mutation.h) now states this explicitly. `CCMapping::encodeMutationAmount` (the
old absolute-set helper) was deleted rather than left unused, since keeping it around would invite reintroducing
this exact bug. Inversion is the one exception: `amount != 0` stays an absolute on/off toggle, since a boolean
has no sensible "delta."

Transpose and length don't have the *wrap* half of this issue (transpose's range is genuinely symmetric, and
negative length has no sensible cyclic interpretation — clamping to the minimum is correct there), but they do
share the *relative* half: all three of transpose/rotation/length are now interpreted as deltas from tracked
state, consistently.

**Confirmed working (2026-08-16):** user re-tested the same rotation -2 mutation after the fix — small, correct
nudge from the instance's current rotation, not a jump to the wrapped absolute value. The `-2 → 14` modular
arithmetic itself was correct all along (`-2` in a 16-step space genuinely is `14`); the bug was applying it to
the raw absolute slider value instead of as a delta from tracked state.

## CC Map (live in MPL, not yet consumed by Composer Mastermind)

**CC 21 (Target Pattern) and CC 60-64 (Target Step/Note/Velocity/Duration/Enabled) went live in MPL on 2026-08-18**
("Option A": wired directly into MPL's existing Target-parameter system — `handleExternalControlCC` just calls
`setPlain` on `targetPatternParam`/`targetStepParam`/`targetNoteParam`/`targetVelocityParam`/`targetDurationParam`/
`targetEnabledParam`, the same parameters MPL's own step-editor UI already writes to). Safety comes for free from
MPL's existing `syncEngineFromParameters()` polling/edge-detection (selection change → browse, value change →
commit) — no new queue was needed. Confirmed live in Bitwig the same day: driving CC 21/60/61/62 correctly selected
a step and committed a new note/velocity to it, visible in MPL's own UI (`Last CC: CC 62 ... | accepted`).

**Residual risk above turned out to be a real, deterministic bug, found and fixed 2026-08-20.** It wasn't "a rare
race straddling a block boundary" - it was systematic: `syncEngineFromParameters()`'s polling logic checked
`if (targetSelectionChanged) browse(); else if (targetValuesChanged) commit();` - an exclusive either/or. Composer
Mastermind's `MotifEngine` (v0.6) always sends a step selection *and* new note/velocity/duration/enabled together
as one burst, so by the time the poll next ran, both flags were true simultaneously, and browse always won -
silently reloading the newly-selected step's pre-existing content over the incoming values, discarding the commit
entirely, no error surfaced anywhere. Confirmed live: `MotifEngine` wrote hundreds of steps across a 44-bar test
with zero audible change, while MPL's own "Last CC ... accepted" readout showed the CCs genuinely arriving.
**Fixed** by swapping the priority - `if (targetValuesChanged) commit(); else if (targetSelectionChanged)
browse();` - so a burst that changes both is treated as "select this step and set it to these values" (one edit),
not browse-then-lose-the-values. Human use of MPL's own UI is unaffected: clicking a step calls
`setTargetPatternAndStep()` directly (guarded by `suppressParameterSync`), bypassing this poll entirely.

`CCMapping.h`/`Mutation` on **this** side still do not know about CC 21/60-64 — deliberately deferred until v0.6
("Motivic Variation Engine") is actually scoped and built (see
[composer_mastermind_design.md](composer_mastermind_design.md)'s roadmap). CC 23 (Editor View Mode) remains
unimplemented in MPL and out of scope.

## Two Routing Paths

- **Scene routing** (`Router::routeScene`): global fields (`Scene::global`, CC 20/22/24) go to every instance in
  `Scene::targets`; per-pattern fields (`Scene::patterns`, a `ScenePattern` per target) go to CC 30-53 blocks on
  whichever instance each `ScenePattern::targetInstance` names. A scene can therefore set one instance's global
  pattern/swing while separately setting another instance's per-pattern transpose, in one call.
- **Mutation routing** (`Router::routeMutation`): a single targeted change to one instance's one pattern's one
  parameter. As of v0.4, gated by `policy/PolicyEngine::authorize` before dispatch — see
  [mutation_policy_v0_1.md](mutation_policy_v0_1.md) for the budget model. `routeMutation` returns `bool`
  (dispatched vs. blocked).

## Known Limitation: Fixed Pattern Count

`CCMapping::kMaxPatterns = 3`, matching MPL's current `numPatterns`. If MPL's pattern count ever changes, every CC
encoding here becomes wrong silently (no error, just musically incorrect values reaching MPL). See
"Instance capability negotiation" in the design doc for the proposed fix — not yet implemented.
