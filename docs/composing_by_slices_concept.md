# Composing by Slices — MPL as trigger "shooter", MidiSampler as the instrument

Status: **concept; 1-bar-section timing risk MEASURED LIVE 2026-10-08 (see "Measured" below); MidiSampler stop-all key + Start-only built; GENERATOR BUILT (v1, `score-generator/slice_score.py`, not yet heard live)** (written 2026-10-08). Decided with the user: Option B — MidiSampler
(`C:\AudioDev\Repos\MidiSampler`, GitHub `johnpascu77-dotcom/MIDISampler`) sits *downstream of MPL*. MPL stops playing
notes in the musical sense and only **fires triggers**; each trigger note is a keyboard key that MidiSampler maps to a
slice/zone of a loaded MIDI source. The score generator then composes *which slices sound, when, and for how long* — a new
kind of piece in the same JSON bundle.

Chain: `MC -> (CC + captured patterns) -> MPL "shooter" instances -> MidiSampler zones -> per-zone output channel -> instruments`.
MPL's output channel = MidiSampler's input channel; MidiSampler's per-zone Out channel fans the result to the orchestra.

## What was checked in the code (facts, not assumptions)

| Fact | Where | Consequence |
|---|---|---|
| A section with `capturedContent` in Absolute mode writes its steps verbatim and is **exempt from every per-bar modification** (melodic curve, swing, coherence, modulation routes, cadences) | `ComposerCore.cpp` `enterSection` / Absolute branch | Trigger pitches (= slice keys) can never be transposed or mutated by arcs. Safe by construction, as long as the whole piece is literal |
| MPL pattern = 16 steps per bar (binary), fixed `4 ppq / gridStepCount` per step; Rate Augmented stretches a cycle to 2 bars | `PluginProcessor.cpp` ~2449-2462 | **One pattern cycle = 1 bar (2 with Augmented).** There is no step size larger than 1/16 bar, so "longer pauses" cannot live inside one pattern |
| MPL **loops** its pattern: the same steps fire every cycle | `PluginProcessor.cpp` ~2566 | A latch trigger fired once per cycle **toggles every cycle** (ON, OFF, ON...). This is the central design problem below |
| A pattern switch/stop is applied only at a bar start, and MPL force-releases its own sounding notes then | `PluginProcessor.cpp` ~2533-2552 | Harmless for latch zones (they ignore note-offs). Matters for Gate zones |
| Writing content to the *active* pattern races with the bar-gated switch and caused real stuck notes | memory: write_pattern bug | Slice sections must alternate P1/P2/P3 so content is always written to an inactive pattern (the existing generator already cycles `patternIndex`) |
| Instance `role` is not validated | `Validation.cpp` | A new role `"trigger"` is allowed without C++ changes |
| MidiSampler Trigger (latch): press = start, press again = stop; a voice that finished by itself is simply inactive, so the next press **starts** it again | MidiSampler engine | The generator must know when each slice ends, or it will press "stop" on something already finished and restart it |
| MidiSampler scales source velocity by trigger velocity | MidiSampler engine | Trigger velocity is a free dynamics lane driven by the arc |

## The core problem: a looping pattern vs a toggle

Because MPL repeats its pattern, one trigger step is not "press once", it is "press every cycle".
- **Gate zones** (slice plays while the key is held, MPL's note duration = hold time): works directly. One bar per fire at most
  (max note length is one cycle). Simple, robust, no state.
- **Latch zones** (what the user wants for long slices): a held-ON state across several bars cannot come from one looping
  pattern. It has to be a **bar-by-bar timeline**.

### Recommended compile: bar-resolution timeline -> sections
1. Plan a timeline `S(bar)` = the set of slices that should be sounding. Events at a bar boundary = the *symmetric difference*
   between consecutive sets (a slice entering = press, a slice leaving = press again).
2. Every bar with at least one event becomes a **1-bar section** whose `capturedContent` fires exactly those keys, one per step
   (MPL is monophonic: simultaneous toggles are spread over adjacent 16ths; extra simultaneity = more shooter instances, one
   key each). Pattern length is one cycle, so each key fires exactly once.
3. Runs of bars with no events become **one multi-bar silent section** (all steps disabled, or `activePattern = 0`). That is the
   "longer pause between MPL's fired notes", and the slices keep sounding through it because latch holds them.
4. Alternate `patternIndex` 0/1/2 across consecutive sections (inactive-pattern write rule above).

Cost: up to ~1 section + 1 scene per event bar. A 100-bar piece with ~40 event bars is ~80 sections: fine for JSON, the
generator already writes per-section scenes.

### The generator must simulate voice state
For each slice it needs the real duration, so it knows when a non-looping slice has ended on its own (no stop press needed,
and a stop press would restart it). So the generator mirrors MidiSampler's slicing in Python (same convention this repo
already uses for `ArcShapeLibrary`/`DurationCalculator`): read the source `.mid`, apply `Beat/Equal/Transient/Manual`
boundaries exactly as `computeSliceBoundaries` does, then convert slice length in source beats to bars using the zone's
speed ratio and the global Speed: `bars = slice_beats / (ratio * speed) / beats_per_bar`.
Looping slices (Loop Forward/Bidirectional) never end by themselves, so only an explicit second press ends them.

## What "composing by slices" means musically (generator strategies)
All driven by the existing arc machinery, so the piece still has a narrative shape:
- **Density arc -> how many slices are latched ON at once** (layers building and thinning). Energy -> trigger velocity.
- **Slice choice**: in source order (the source's own form), retrograde, arch along the energy curve, or **a 12-slice row with
  P/I/R/RI forms** as the slice order (Equal x12 slicing) — the atonal use: the *order of slices* is the twelve-tone row.
- **Pauses are structure**: a section's silence length can follow the tension curve (long air before the peak).
- **Zone design** (set once in MidiSampler): e.g. zone A = Loop slices (sustaining beds), zone B = one-shot slices (events), zone
  C = same source inverted with ratio 2/3 (mensuration). The generator only needs each zone's key range and slice map.

## Drift-proofing (small MidiSampler additions to do alongside)
Toggle semantics fail silently if the plugin's state and the generator's model disagree (e.g. a manual key press, a restart
mid-piece). Planned MidiSampler features:
1. **Stop-all key**: any note-on on a chosen key = release everything. The generator can fire it at section starts / the end of
   the piece so a score always resynchronises.
2. **Playback mode "Start only"** next to Gate and Latch: a press always (re)starts, never stops; stopping is done by the
   stop-all key or by the slice ending. Fully stateless for the generator, at the cost of no per-slice stop.
Latch stays available for the explicit ON/OFF writing the user wants.

## Measured: the 1-bar-section risk (2026-10-08)
Tested live in Bitwig by recording MPL's own output (details and note table in `docs/slice_test/README.md`). Result: **1-bar sections work**
(no skipped or doubled changes, exact positions inside the bar, no write race) but **every change arrives exactly 2 bars late**: Mastermind's bar counter is
0-based relative to playback start (+1 bar) and MPL applies a queued pattern only at its own next bar start (+1 bar). It is constant, so the generator
**shifts every trigger 2 bars earlier** and treats the first 2 played bars as pre-roll. MPL also keeps its active pattern between runs, so a score should
open by selecting an empty pattern (or firing the stop-all key early).

## Open points / risks
- (RESOLVED, see Measured above) **Latency of 1-bar sections was untested.** MPL applies a queued pattern only at a bar start (`isAtBarStart && currentBar !=
  lastLaunchBar`). If MC's CC for a section ever lands just after MPL's bar-start step, MPL waits a whole bar and every trigger
  lands one bar late. Existing multi-bar sections would hide this; 1-bar sections will expose it. **Must be verified live with a
  tiny test score before building the full generator.**
- Trigger notes are real notes: a Gate zone downstream of an MPL whose `Transpose` mutation is active would change slice keys.
  Absolute mode removes that risk; do not mix generative sections into a slice piece.
- MidiSampler setup (source file, slice mode/grid, zone key ranges, latch) is **not in the MC bundle**. Either document it as
  a companion setup, or give MidiSampler Save/Load Setup (a small JSON) and have the generator emit that file next to the
  bundle so the two always agree.

## Proposed build order
1. MidiSampler: Stop-all key + "Start only" playback mode (small, engine tests).
2. A tiny hand-written 6-bar test bundle (one shooter, one latch slice ON, silent bars, OFF) to **verify 1-bar section timing live**.
3. `score-generator/slice_score.py`: Python mirror of the slicing, voice-state simulator, timeline -> sections compiler,
   strategies above, `trigger` role instances.
4. MidiSampler Save/Load Setup + companion file emission.
5. Only then: cascade stages inside MidiSampler (see `MidiSampler/Docs/note_sampler_research.md`) — a second, independent way
   to get the same effect without MPL.

## Generator v1 (built 2026-10-08): `score-generator/slice_score.py`

**Easiest way to run it: a window.** Double-click `score-generator/run_slice_score.bat` (or open `slice_score_gui.py` in IDLE and press F5). Pick the
`.mid` you load into MidiSampler, copy MidiSampler's slicing/playback settings into sections 3 and 4, choose the shape and strategy, press *Generate
score*. A live preview shows the slices (keys and lengths in bars) before you generate; afterwards the window shows the result, the setup sheet and the
timeline. It remembers your settings in `slice_score_gui_settings.json` (not committed). IDLE users can also press F5 on `slice_score.py` itself: with no
options it uses the `IDLE_SETTINGS` list at the top of the file. The window and the command line share one code path (`slice_score.generate`).

Command line (same thing, for scripting):

```
python slice_score.py --title "Bach Clouds" --source ..\docs\slice_test\slice_test_source.mid --minutes 2 --bpm 110 \
    --shape arch_rise_fall --strategy arch --seed 7 --output ..\docs\slice_score_demo.json
```
Writes the bundle plus `<name>.setup.md` (exact MidiSampler / MPL / Mastermind settings the score assumes) and `<name>.timeline.txt` (bar by
bar: what is pressed, which slices sound, and the Bitwig bar where you will hear it). A ready example is `docs/slice_score_demo.*`.

**How it works**
- Mirrors MidiSampler exactly (slice boundaries, slice lengths in bars from speed x ratio, Latch / Start only / stop-all) and reads the source `.mid`.
- Plans a bar-by-bar timeline from the energy arc (`--shape`): energy sets how many slices are latched at once (`--max-layers`), how long they are
  held before changing (`--min-hold` / `--max-hold`), and the trigger velocity. Quiet stretches can drop to silence (`--no-breaths` to disable);
  the opening never does. The last section is silent and a stop-all key (default 23) closes the piece.
- Slice choice (`--strategy`): `order`, `retro`, `arch` (index follows the energy curve), `random`, `row` (a seeded twelve-tone row over 12 slices
  with P / I / R / RI forms; needs `--slice-by equal --count 12` or finer).
- Modes: `--sustain loop|forward`, `--playback latch|start_only`. Start-only + loop cannot stop a slice individually, so shrinking the layers is
  a stop-all followed by restarting the new set in the same bar.
- Compiles to **1-bar sections for every bar with trigger presses and one multi-bar silent section for every run without**, rotating the pattern index
  P1 -> P2 -> P3 per section so a pattern is never written while MPL plays it. Plans in bundle bars; **everything is heard 2 bars later** (first sound
  in Bitwig bar 3). `--shooters N` splits keys over N MPL instances; a key always uses the same one (latch matching is per key *and* channel).
- Honest scope of the "P1/P2/P3 approach": the three patterns are used as a rotation (the pattern that is written is always a free one), not as three
  bars written once, because the content changes every bar of a real piece. A periodic piece (the same 3-bar cycle repeating) could write once and only
  switch the active pattern; not built.

**How it is checked (no Bitwig needed)**
1. Built-in replay: after generating, the finished bundle is re-read and replayed through a fresh voice model; it must reproduce the planned slice
   states bar by bar, with non-empty sections exactly 1 bar, the pattern rotation intact, nothing sounding after the end, and the voice limit respected.
2. `slice_score_selftest.py`: 540 combinations (strategy x playback/sustain x slicing x shooters x seeds) must all pass that replay.
3. `slice_score_crosscheck.py`: replays the presses through the REAL C++ `SamplerEngine` (`MidiSampler/Tools/SamplerReplay.cpp`) and compares the sounding
   slice keys at 576 checkpoints per scenario against the Python model: 9 scenarios, all agree (odd slice lengths, Thru, ratio 2/3 + speed 90, transient,
   stop-all, start-only). A deliberate 30 % slice-length error is detected (86 mismatches); a 5 % error is not, because it falls inside the half-step resolution.

**Not verified:** how it sounds, and that the 2-bar lag holds for a long piece with many 1-bar sections (it was measured on 8 sections). The first live run
of `docs/slice_score_demo.json` is the real test.

## Plain-clip route: `slice_midi_score.py` (no MPL)

MPL turned out not to be a good match; hand-drawn MIDI clips played straight into MidiSampler sound better. `score-generator/slice_midi_score.py`
writes such a clip: octave = zone (= slice), key inside the octave = transposition, note length = how long a Gate zone loops, velocity = dynamics.
It reads the plugin's own **Save Setup** JSON (zones, voice limit, stop key, source), plans layers/accents against `midisampler_model.py`
(validated against the real C++ engine), writes a `.mid` plus a `.report.txt`, and re-verifies the finished file through the model and
`MidiSamplerReplay.exe`. Options: `--allowed-transpositions 0,3,7`, `--avoid-clashes`, `--max-layers`, `--strategy`, `--shape`, `--accent-rate`.
Start-only loop zones switch phrase-wise with the Stop key; Trigger (latch) zones are refused. Self-test: `python slice_midi_selftest.py`.
Not verified by ear yet.
