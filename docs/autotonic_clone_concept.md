# Automatable modal transposer — a private counterpart to AutoTonic

Status: **scoped 2026-10-20; v0.1 built the same day** in `C:\AudioDev\Repos\ModalMap` (engine + checks + VST3/CLAP/Standalone; local git, no GitHub remote yet; CLAP installed to `%LOCALAPPDATA%\Programs\Common\CLAP`, not live-tested in Bitwig). User priority: live playing with the linear white-key mapping and black-key headers, automatable. Decisions taken in v0.1: Degree mode folded into Linear (an N-tone scale wraps every N degrees); modes = Linear / Snap / Thru; CC 106 tonic (value = tonic x 11), 107 preset, 108 mapping, 109 header, and CC 110/111 as scale mask only when "Follow field" is on; **v0.2 (same day): chord stacks** (13 diatonic stack presets, stored per header with a third lock C, CC 80, velocity %) and **program-change header switching** (PC 1-15, option, consumed) built; **v0.3: keyboard zones** (4 zones: key range, output channel, mapping/stack/anchor overrides, outside-zone policy) built; **v0.4: per-zone tonic and scale overrides** built; **Offline web UI** (`Launch ModalMap Offline UI.bat`): drop a .mid, controls + changes along the timeline, before/after piano roll, audition, download/save. **v0.8: duplicate-note policy (retrigger / merge / separate channel).** **v0.7 (2026-10-20): the Custom pattern became a visual chord-shape editor (grid, per-note velocity, Learn from the keyboard, per-scale-degree shapes, chord-name fills, inversions, voicings) plus a chord strum (ms, up/down/random) - beyond anything AutoTonic offers.** v0.6 (2026-10-20): MirrorKeys (AutoTonic v1.12 #MIRROR), the v1.14 pattern language as a Custom pattern stack + per-header entries (tonic/scale commands, [n] recalls, chord on the header key), custom colours skin file; repo now on GitHub (johnpascu77-dotcom/ModalMap, private). v0.5 (2026-10-20): Nuke black-key mode** (modelled on AutoTonic v1.13's Nuke Mode: every black key plays; our version = sharp/flat bias, never mutes) and the **offline re-keying tool** (`Tools/mm_apply.py` + `ModalMapOffline`: tonic/scale/stack curves over time, source key, JSON plan, per-track, CC/tempo passthrough) built; grace notes, feedback loops and a user library file are still open. Origin: the user uses AutoTonic (standalone, Windows) a lot and misses
being able to automate the **tonic** and the **scale** from the DAW. Same recipe as VoiceAllocator/PolySWAM
([voice_allocator_concept.md](voice_allocator_concept.md)): rebuild the *idea* as a private MIDI-FX plug-in whose state is
a set of real parameters, so Bitwig automation, Composer Mastermind CCs and the Orch rig can drive it.

Source: the AutoTonic User Manual (v1.10 rev222.1, 2019), read in full. The engine is patented (Austrian patent AT 516.297),
so this is a **personal-use rebuild of the musical idea from the published behaviour, not a copy of the product**: no
code, UI artwork, scale database or name is taken. Keep it private (not distributed).

## 1. What AutoTonic actually does (from the manual)

| Aspect | Manual says |
|---|---|
| Core idea | A **sandwich** note processor between a MIDI keyboard and a sound source. Every **white key** of the input keyboard plays the *next degree of the chosen scale* ("true linear fingerboard"); the output is the scale's real pitches. Not random: every output note is a deterministic function of the input note (so it is a pure mapping, ideal for automation). |
| State = **Tonic** + **Scale** | Tonic: one of the 12 pitch classes (enharmonics ignored). Scale: a **12-step interval mask** ("x/o" string, e.g. Lydian `xoxoxoxxoxox`). Up to 24,576 scales are claimed (12 tonics × 2^11 masks). |
| **Headers** | Memory slots, each holding a Tonic + Scale pair (each with its own Lock). Placed on **black keys** of the input keyboard; pressing the black key switches the harmonic context in real time ("key switching technology"). One active at a time. An *unlocked* modifier is overwritten by the previously used header's value, so tonic and scale can be switched independently by mixing locked/unlocked headers. |
| Power key | Default input note 61 toggles processing; when off everything passes unchanged. |
| Processing range | Only notes **21–108**. Channel 1 by default. |
| Pitch compensation | Draggable INPUT anchors offset the input pitch per header (e.g. to keep tonic C and tonic B in the same register). |
| Library | Scale templates (manual says "over 50" in one place and "400+" added in v1.6 and "over 500" in another), categories, user scales, 2-line header abbreviations, export/import of the whole database. |
| Learn | Hold ← then play a key = set the tonic; hold → then press keys = toggle scale intervals (as if tonic C). |
| **x/o modifier + stacking** (v1.7/1.8) | Chord/interval **stacking** driven by variables: one input key outputs several notes (e.g. diatonic triads/7ths), "duplication". Chord Memorizer (real-time chord switching), keyboard **zones** (left hand chords, right hand solo), MIDI-channel zones, **grace notes** triggered independently, **feedback loops** to automate harmony switching, overtone scales that vary per octave, scales that differ ascending vs descending. |
| Routing | Input/output **channel scrollwheels**, modes Omni / Filter / Thru (input) and CH.# / ALL / AUTO (output), MULTI (headers listen on all channels), **multiple instances**, MONO/POLY input, **DELAY 0–999 ms** (lets a "control instance" switch headers *before* the audible note), PANIC, Mute keys. CC, program change, channel pressure, pitch wheel and **poly aftertouch** are passed along on the mapped channel. |
| Why standalone | The author argues a DAW plug-in adds buffering/latency and ties it to one plug-in format. It needs a virtual MIDI cable on Windows (loopMIDI etc.) and an online licence check. |

**Where the automation gap comes from:** the *only* control surface besides the mouse is "play a black key / tap a MIDI note".
There are no CCs, no host parameters, no OSC, and the app lives outside the DAW, so a clip lane cannot say "tonic = D, scale =
Dorian at bar 17". The workaround is a second "control instance" that triggers header keys from recorded notes (that is what
DELAY and the feedback loops are for), but headers hold only what was set by hand beforehand.

## 2. What we already own that overlaps

| Piece | Overlap |
|---|---|
| **OrchNoteFilter** with *CC Control* | Already **pulls notes toward a pitch-class mask** sent by Mastermind on **CC 110 / 111** (12-bit mask, low 7 bits = pitch classes 0–6, high 5 = 7–11). That is a *snap/quantise to a scale* (non-linear, nearest-note) — the field half of the job exists. |
| **Composer Mastermind pitch-field broadcast** (`set_pitch_field_broadcast`), AME style profiles | Mastermind already derives a per-section pitch-class set and sends it as the mask. The scale *as an arc/section value* is therefore already a concept in the system. |
| **SlonimskyPatterns** (`C:\AudioDev\Repos\SlonimskyPatterns`) | Scale/pattern thesaurus data usable as a starting library. |
| **ChordHarmonizer**, **PolyGate**, **MorphQuantizer** | Chord stacking from a function pool; event grouping; grid quantise. Reusable ideas for the stacking mode. |
| **ChannelFan / VoiceAllocator** | Template for zones/channels, held-note bookkeeping (note-offs must follow the mapping used at note-on), CC remote-control hooks, CLAP build for pitch bend. |

The distinct thing AutoTonic does that we do **not** have: the **linear degree mapping** (white key *n* → scale degree *n*, so
you play in any mode with the same finger pattern; octave = one scale cycle even for 5- or 8-note scales). Snap-to-mask
(OrchNoteFilter) is different: it moves wrong notes to nearby right ones and keeps the input's rhythm and shape.

## 3. Proposed design

**Working name:** ModalMap (any name except AutoTonic). MIDI effect (VST3 + CLAP, + Standalone for live use), engine as a pure
std-only header (`ModalMapEngine.h`) with checks, the same layout as PolyGate/ChannelFan/VoiceAllocator.

### 3.1 State (all of it host parameters, so everything automates)
- **Tonic** (0–11) and **Scale** as a 12-bit interval mask, plus a **Scale preset** choice list (library index) and a "Custom" mask editor.
- **Mapping mode** (the core choice, AutoTonic is only the first):
  1. **Linear** (AutoTonic's way): input white keys → consecutive scale degrees. Defined input: white keys of 7 per octave →
     degree index; black keys either pass, are muted, or act as header keys (below). Cycle length N = number of scale tones, so an octave of
     input is N degrees: a 5-note scale shrinks the range, an 8-note scale widens it; offset by tonic and a transposable **anchor**
     (pitch compensation, automatable).
  2. **Snap**: nearest scale tone (up / down / nearest), the OrchNoteFilter behaviour but with a real scale and tonic.
  3. **Degree-preserving transpose** ("modal interchange"): treat the input as C-major degrees and map to the same degree in the target scale (7 → N-note scales by clamp/wrap rule).
- **Headers**: 12–24 stored Tonic+Scale(+anchor) slots, switchable by black key, by **program change**, by CC, or by host automation of "Header #". Per-modifier lock as in the manual (so tonic and scale can be switched independently).
- **Stacking**: a stack template per header (intervals in scale degrees, e.g. `0,2,4` triads, `0,2,4,6` sevenths), velocity ratio per layer, optional per-layer output channel (feeds VoiceAllocator).
- **Routing**: input channel filter (omni/filter/thru), output channel (same/fixed/per-layer), processing range 21–108 default, pass-through of CC, pitch bend, aftertouch.
- **Lookahead (the DELAY idea, done properly):** a MIDI plug-in cannot see the future, but the host *does* know it. Because tonic and scale are ordinary parameters, a change at bar 17 lands exactly at the audio-block boundary — no delay hack. Added bonus: **sample-accurate header changes inside the block** keyed to the event timestamp.

### 3.2 The part AutoTonic cannot do: control from Mastermind
- **New CC set** (consumed by the plug-in, optional channel filter), proposed **CC 106 tonic (0–11 scaled ×10 or `v%12`), CC 107 scale preset, CC 108 mapping mode, CC 109 header index** — or better, a single reuse of the **existing CC 110/111 12-bit pitch-class mask** as the *scale mask* (tonic then comes from a separate CC), so Mastermind's existing field broadcast and the Romanian-shadow style profile drive it unchanged. Check: CC 102-105 and 112-121 are already taken (OrchConductor, VoiceAllocator, field mask), 20-64 is MPL, 85-90 AME. 106-109 are free as far as the docs show; verify against `CCMapping.h` and each repo before fixing numbers.
- A Mastermind **section → tonic/scale** mapping through ordinary Modulator Targets in *Section* mode (stepped), or a `ModulationRoute`-style continuous drift for a tonic that moves around the cycle of fifths with the arc.
- One transposer per voice, each on its own channel, so different instruments can sit in different modes at the same time (poly-modal) — the "multiple instances / MULTI" idea, but driven by one score.

### 3.3 Library
Own data file (JSON), seeded from public-domain theory: the standard modes, harmonic/melodic minor families and their modes,
pentatonic/hexatonic, whole-tone, octatonic (both), augmented, double-harmonic, Hungarian/Romanian, Messiaen modes of limited transposition, plus the
Slonimsky thesaurus data we already hold. Names stored per scale; **no import of AutoTonic's database** (its content and
licence belong to the vendor). Dedupe by mask equality, show matching scale name live (same function the manual describes).

### 3.4 UI
Small: the two keyboards (input/output, as in the manual — lit keys show the mapping, which is the whole point of the original), tonic selector, interval strip (12 toggles), scale list, header strip, stacking editor, anchor. A text-only "mapping table" view is the cheap first version.

## 4. Feasibility and risks

- **Feasibility: high.** Linear mapping is a few lines of index arithmetic; held-note bookkeeping (note-off follows the mapped pitch, also after a header change while keys are held — a stuck-note class bug AutoTonic itself fixed in v1.5/1.9) is a solved pattern in ChannelFan/VoiceAllocator. Realistic size: smaller than VoiceAllocator.
- **Patent / legal:** AT 516.297 covers their method of mapping scales onto white keys with black-key key switching. We are not distributing or selling; for private use this is low risk, but do not publish or ship it, and do not copy their scale library or artwork. Worth one more note in the repo README.
- **Latency model:** ModalMap is zero-latency in the plug-in; the author's "plug-in = latency" argument does not apply to a MIDI-only block-synchronous FX.
- **Pitch-bend note:** if bend must pass, build the CLAP (same finding as ChannelFan/MpeBend in Bitwig).
- **Semantics to decide, not hide:** how the linear mode treats black keys and scale lengths other than 7 (the manual shows a fixed 12-key layout; output keyboard simply lights target pitches).
- **Interaction with OrchNoteFilter:** do not run both on one track unless intended (one moves notes onto a scale, the other re-maps degrees). With Linear mode the filter becomes redundant on that track.

## 5. Build plan (proposed)

1. **v0.1 engine + checks:** Linear/Snap/Degree modes, tonic+mask, anchor, held-note bookkeeping (note-offs follow note-on mapping across tonic/scale/header changes), range 21–108, checks: round trips, 5/6/7/8-tone scales, octave cycle, stuck notes on mid-note change, determinism. Offline MIDI-file tool (re-key a recorded clip to a tonic/scale curve) — usable at once for Dorico export.
2. **v0.2 plug-in:** parameters (all automatable), preset list, stacking, headers by black key / program change, CLAP + VST3 + Standalone, minimal UI with keyboards.
3. **v0.3 Mastermind hooks:** CC set above (or the existing 110/111 mask), section-stepped tonic/scale via Modulator Targets, one-score poly-modal demo.
4. **v0.4 library tool + HTML guide** (generator in `tools/plugin_guides`), tooltips.

## 6. Open questions for the user

1. **Which job matters most?** (a) live playing with a linear keyboard exactly like AutoTonic but automatable, (b) re-keying recorded/generated MIDI to a tonic/scale curve (offline + Bitwig automation), (c) feeding Mastermind's score a per-section mode and letting every voice follow. (b)+(c) are nearly free; (a) needs the black-key headers and a UI.
2. **Linear vs snap:** do you want the white-key-degree mapping at all, or is the OrchNoteFilter-style snap with a proper tonic/scale enough? Linear is what makes AutoTonic unique and is the new capability.
3. **Black keys:** header switching as in the original, or let them play chromatic passing tones (a mapping that interleaves)?
4. **Stacking/chords** in v0.2 or later?
5. **CC numbers:** reuse the 110/111 mask as the scale (recommended) or a separate set 106–109?
6. **Name** — something other than "AutoTonic".

## Sources
- AutoTonic User Manual (PDF): https://www.autotonic.net/-/manual/AutoTonic_Manual.pdf
- Reviews/news on the product: https://www.gearnews.com/autotonic-midi-scale-transposition-tool , https://ask.audio/articles/autotonic-combines-real-time-scale-mapping-keyswitching-for-creative-songwriters-producers , https://www.kvraudio.com/product/12148/review
- Internal: [voice_allocator_concept.md](voice_allocator_concept.md), [style_profile_romanian_shadow.md](style_profile_romanian_shadow.md) (CC 110/111 pitch-field mask), [ame_bridge.md](ame_bridge.md) (CC allocation notes)
