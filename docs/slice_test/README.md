# Slice trigger timing test

Goal: check **live** that a bundle built from 1-bar sections fires its MPL triggers on the right bar, and that MidiSampler's
latch ON/OFF, silent holds and stop-all key behave. Concept and risk: [../composing_by_slices_concept.md](../composing_by_slices_concept.md).

Files: `slice_test_source.mid` (8 bars, monophonic, one arpeggio per bar on a rising root, so you can tell by ear which source bar
is sounding) and `../example_score_slice_trigger_test.json` (the bundle). Regenerate both with
`python score-generator/slice_trigger_test.py`.

## Setup

**MidiSampler** (install the latest build first; close Bitwig, copy the `.vst3`):
1. Load `slice_test_source.mid` into zone 1's source (slot 1).
2. Zone 1 **enabled, and every other zone switched off** (a key outside every enabled zone just passes through to the piano):
   Mode **Slice**, Slice by **Beat grid**, Grid **4 beats** (so each slice is exactly one bar), Slice playback **Slice**,
   First slice key **24 (shown as C0)**, Sustain mode **Loop Forward**, Playback **Trigger (press again to stop)**, key range 0-127.
3. Global bar: **Stop key = 23 (shown as B-1)**, **Speed 100 %** (a leftover 25 % would make a one-bar slice last four bars). Speed 100 %, Sync on. Voices 4 or more.
4. You should see 8 green slices on the display, labelled C0 ... G0 (keys 24-31).

**Bitwig:** put MPL (MIDI channel 1) on a track, route its note output into a track carrying MidiSampler, and MidiSampler's output into any
instrument. Tempo **100 BPM** (the source file's own tempo; with Sync on the slice length follows the host anyway, but bar 1 of
the source then equals one host bar only if the host is also 4/4).

**Mastermind:** load the bundle (Load Score), set Content mode to **Absolute** (these sections are literal captured content; in
Generative mode they would be ignored), make sure MPL's External Control is enabled and on channel 1, MPL pattern length 16, Binary grid.
Then play the blueprint from bar 1.

## What you should hear

**Measured offset (2026-10-08, verified live): everything arrives exactly 2 bars after the bundle's bar number, counting Bitwig's
bars from 1 with playback started at bar 1.** Add 2 to every bar number in the table below. Why and what it means:
see the "Measured" section at the end. The table lists the bundle's own bars; "Bitwig bar" is where you will actually hear it.

| Bundle bars | What fires | You should hear (at Bitwig bar = bundle bar + 2) |
|---|---|---|
| 1 | slice 1 (key 24 = C0) at beat 1 | source bar 1 (low C arpeggio) starts and **loops every bar** |
| 2-3 | nothing (silent section) | the slice 1 loop **keeps going** |
| 4 | C0 at beat 1, then D0 (26) at beat 2 | slice 1 **stops** on beat 1; slice 3 (source bar 3, E arpeggio) starts on beat 2 and loops, offset one beat from the bar line |
| 5-6 | nothing | slice 3 keeps looping |
| 7 | D0 at beat 1, D#0 (27) at beat 3 | slice 3 **stops** on beat 1; slice 4 (source bar 4, G arpeggio) starts on beat 3 |
| 8 | nothing | slice 4 keeps looping |
| 9 | B-1 (23, stop-all) at beat 1 | **everything stops** |
| 10-11 | nothing | silence |

## What to report back
- **Timing (the main question):** does each event land on the bar it should? If everything is exactly **one bar late** it is MPL's
  bar-start gating (see the concept doc); say whether it is a constant one-bar offset or varies. If some bars are skipped entirely,
  that is the pattern-write race instead.
- Does slice 1 really stop at bar 4 (latch OFF), or restart?
- Does the stop-all key at bar 9 silence everything? Anything hanging at bars 10-11?
- Whether the single trigger at the start of bar 4 beat 2 / bar 7 beat 3 is on the right 16th.

## Variant: one-shot slices that end by themselves
Set Sustain mode **Forward** and Playback **Start only**. Then each trigger plays its slice once (one bar) and the slice stops
on its own, with no need for an OFF press. Bar 4's first press (C1) simply restarts slice 1 for one bar instead of stopping it;
the stop-all key at bar 9 is then a no-op. This is the "stateless" mode a generated score can rely on.

## Measured (2026-10-08): the 2-bar offset is real, constant and explained

Run by Claude through Bitwig: MidiSampler was bypassed, a spare track recorded the MIDI Pattern Launcher device's own output
(Bitwig track input = "Grand Piano > MIDI Pattern Launcher" device), and the clip's 8 notes were read off the piano roll:

| Note | Landed at (Bitwig) | Intended bundle bar |
|---|---|---|
| C0 | bars 1, 2, 3 (leftover pattern 1 from the previous run, still looping) | - |
| C0 | bar 6 beat 1 | 4 |
| D0 | bar 6 beat 2 | 4 |
| D0 | bar 9 beat 1 | 7 |
| D#0 | bar 9 beat 3 | 7 |
| B-1 | bar 11 beat 1 | 9 |

- **Every section change arrives exactly 2 bars late**, none skipped, none doubled, no stuck or dropped trigger. Positions *inside* a bar are exact
  (a beat apart in bar 6, two beats in bar 9). The pattern-write race did not appear (P1/P2/P3 alternation works).
- **Cause, bar 1 of 2:** Mastermind's bar counter is **0-based and relative to playback start** (`PluginProcessor.cpp`: `processBar(currentBarIndex)` then
  `++currentBarIndex`, first value 0). Bundle `startBar: N` therefore becomes active in the **(N+1)-th bar played**. (`get_status` at Bitwig bar 4 reported `currentBar 3`.)
- **Cause, bar 2 of 2:** MPL applies a queued pattern only at its *own next* bar start (`PluginProcessor.cpp` ~2533). Mastermind's message is sent at the bar boundary and
  reaches MPL just after it, so MPL waits one more bar. This is deterministic (block ordering), which is why all seven changes had the same lag.
- **Consequence for a generator:** author every trigger **2 bars earlier** than the bar where it should sound, and leave the first 2 played bars as pre-roll
  (nothing can sound there from a bundle). Alternatively teach Mastermind to send the pattern change one bar early (not done).
- **Gotcha seen:** MPL keeps its active pattern between runs. Anything left looping from a previous play-through sounds again at the start of the next one.
  A generator should open with a section that selects an empty pattern, or fire the stop-all key early.
- Not covered by this measurement: MidiSampler's reaction (latch ON/OFF, loops, stop-all) - that was verified only by the engine checks, not live.
