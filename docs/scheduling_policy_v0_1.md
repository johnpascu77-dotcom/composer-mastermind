# Scheduling Policy v0.1

Defines when Composer Mastermind is allowed to emit scene/mutation changes, following the source roadmap's timing
principle (§17): the Composer should be transport-aware and, in its first version, emit changes **only at bar
boundaries**.

## What Exists Today (v0.3, done 2026-08-15)

[`plugin/PluginProcessor.cpp`](../src/plugin/PluginProcessor.cpp) detects bar boundaries from the host playhead
(`AudioPlayHead::PositionInfo::getPpqPositionOfLastBarStart()`, compared block-to-block) and calls
`ComposerCore::processBar(currentBarIndex)` once per new bar, where `currentBarIndex` is a **monotonic tick
counter** ("how many bar boundaries have been crossed since playback started"), not the host's actual song-position
bar number.

`ComposerCore::processBar` does two things:
- `dispatchSceneIfNeeded` — pops due events from [`scheduling/Scheduler`](../src/scheduling/Scheduler.h)'s
  `ScheduledEvent { type, targetBar, priority }` queue. **Still inert**: nothing calls `Scheduler::scheduleEvent`,
  so this path exists for future one-off scheduled sends but does nothing today.
- `advanceSceneChainIfNeeded` — the actual v0.3 mechanism. **Not** built on top of `Scheduler`: chain advancement
  needs "how many bars has the *current* scene been active," which the generic due-event queue doesn't model
  (it doesn't track *when* something started, only when something is due). Instead, `ComposerCore` tracks
  `currentSceneStartBar` directly, and [`policy/SceneAdvancePolicy::shouldAdvance`](../src/policy/SceneAdvancePolicy.h)
  is a pure, stateless, JUCE-free function: given the active scene, the bar it started on, and the current bar, it
  decides whether `(currentBar - sceneStartBar) >= scene.durationBars` and `scene.nextSceneId` is set. If so,
  `ComposerCore` looks the next scene up in `SceneLibrary`, switches to it, and calls `Router::routeScene`
  directly from the audio thread — satisfying "emit only at bar boundaries" for free, since `processBar` itself
  only ever runs on a boundary.

The editor's manual "Send Scene Now" / "Load and Send" buttons still bypass all of this and route immediately
(see [routing_policy_v0_1.md](routing_policy_v0_1.md)) — but as of v0.3, whatever scene they set *also* becomes
chain-eligible: if that scene has a `nextSceneId`, it will auto-advance after `durationBars` bars of playback,
same as if it had arrived via the (still-inert) scheduler path. A scene with no `nextSceneId` behaves exactly as
it did pre-v0.3: static, no auto-advance.

## v0.1 Rule (implemented, per source roadmap §17.1)

```
Emit scene changes at bar boundaries only.
```

Done — see above. `ComposerCore::processBar` is the only audio-thread entry point that calls into routing for
chain advancement; nothing scene-advancement-related happens off that tick.

## Future Timing Options (source roadmap §17.2, not v0.1)

Immediate / Next Step / Next Beat / Next Bar / Next Section — deliberately deferred. Bar-quantized is the only
mode worth building until the bar-only path is proven stable in real Bitwig playback.

## Transport Loop / Restart Behavior (revisited 2026-08-15)

Two distinct cases, previously conflated as one "known gap":

- **Looping while playing** (Bitwig's loop-brace jumping the playhead back mid-playback, `isPlaying` staying
  true throughout): turns out to be handled correctly *by construction*, not by explicit loop-detection code.
  Because `currentBarIndex` is a monotonic tick counter rather than the host's actual bar-in-song number, and
  the bar-boundary detector compares `abs(newBarStartPpq - lastBarStartPpq)` (symmetric, not just "did it
  increase"), a backward jump still registers as a new bar boundary and still increments the counter. Scene
  durations are measured in elapsed bar-ticks since the scene became current, which stays numerically correct
  across a loop regardless of what the host's own bar-in-song number does.
- **Transport stop then restart mid-scene**: this one was a real gap, now fixed. `PluginProcessor` calls
  `ComposerCore::notifyTransportReset()` on the block where playback transitions into stopped (not every block
  while stopped), which re-arms `sceneStartPending` so the next `processBar` call re-anchors the active scene's
  clock to whatever bar playback resumes on, instead of counting against a stale start-bar from before the stop.

Reasoning from first principles about host playhead callbacks can still miss real quirks a specific host produces
(e.g. whether Bitwig ever briefly reports `isPlaying == false` during a loop-region jump, which would trip the
stop-handling path unexpectedly) — this still deserves an actual Bitwig test (loop a short region with a 2-3 bar
scene chain running, confirm it advances correctly on every pass, then stop/restart mid-chain and confirm the
active scene's remaining duration resets sensibly) before being treated as fully proven, not just reasoned through.
