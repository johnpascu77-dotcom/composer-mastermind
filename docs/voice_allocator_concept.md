# Voice allocator — a MIDI-only counterpart to PolySWAM's voice allocation

Status: **v0.2 built 2026-10-19** (v0.1 + Polyphony Lock, voice affinity, Composer Mastermind CC hooks 112-116, HTML guide; humanize dropped by the user: the goal is notation in Dorico) (`C:\AudioDev\Repos\VoiceAllocator`: engine, checks, offline tool, VST3/CLAP/Standalone plug-in; see its README). Decisions from the user: processing recorded MIDI is the cleaner input but live input with MorphQuantizer as a guard downstream is the vision; first look at the MIDI output, distribute to instruments afterwards with OrchNoteMapper presets; channel = player is fine; legato by overlap is expected by SWAM and every library with a legato option, and generators like Instacomposer write clean cuts on the grid, so the allocator creates the overlap itself. Origin: the user is trying Audio Modeling's PolySWAM and finds its
"intelligent voice allocation" the best part, but it is consumed inside PolySWAM's engine and cannot be captured as MIDI.
Goal: a private MIDI tool (VST3/CLAP MIDI effect + offline tool, like the other Orch-family tools) that does the same job and
**emits the allocation as ordinary per-player MIDI channels**, so it can be recorded, edited and notated.

## 1. What PolySWAM does (public sources only)

Audio Modeling publishes the concepts, not the algorithm. Everything below is from their pages and press coverage;
what is *not* public is marked.

| Feature | What the sources say |
|---|---|
| Capture buffer | A micro-latency window that "compensates for human execution jitter"; the engine analyses the notes as a **block** before assigning voices. Length not published. |
| Three strategies | **Multi Poly**: analyses the playing and splits parts across instruments by melody vs chord. **Complete Voices**: manages which instruments are free and fills gaps with "intelligent thickening" (a Tower-of-Power style horn section: one note → all instruments play it; a full chord → each instrument takes its own note, without doubling or tripling counts). **Multi Mono**: layers across instruments in real time, "zero latency". (Product page names them Complete / Multi / Mono.) |
| Assignment | "Each note is assigned to a distinct virtual player"; considers instrument range, available polyphony, melodic continuity, **voice affinity** and legato relationships; an "optimized assignment system reportedly minimizes total pitch movement and prevents voice crossing". |
| Memory affinity | Recurring notes return to the same virtual player across phrases. |
| Smart Legato | Sample-accurate MIDI scheduling and per-voice overlap logic: overlapping or closely timed notes trigger the SWAM engines' own legato on the **same** instrument; no keyswitches. Single lines stay monophonic with legato. |
| Ranges | A RANGES screen: key range and **overlap interval** between instruments/sections, editable; instruments can be layered or split across the keyboard. The screenshot shows sections (Violins, Violas, Cellos, Double-Basses) with Low/High key, Low/High overlap and Max polyphony per section. |
| Intelligent Transpose | Keeps each part inside the playable range of its modelled instrument. |
| Polyphony Lock | A pedal freezes the current voicing in real time. |
| Humanization | Controlled variation of timing, intonation and dynamics so the ensemble is not mechanically synchronous. |
| Not mentioned | MPE (channel aftertouch only), polyphonic aftertouch, voice-stealing rules, the capture-buffer length, any numbers. |

Audio Modeling's own comparison with Divisimate says an external MIDI tool cannot reach the engine-level features (Dynamic
Spread Compensation, Voice Affinity, specialised allocation) because it treats instruments as black boxes. That is true
for the *sound* features; the **allocation logic itself is MIDI-in / channel-out and can be reproduced**, which is the point of this doc.

Prior art outside PolySWAM: **Divisimate** (Nextmidi): up to 8 voices to 32 virtual ports; divisi modes **Bottom Up**
(fill voices from the lowest), **Top Down** (from the top, melody first) and **Fill Voices** (spread notes evenly across all active
voices when fewer are played); a melody range and a low-bass range can bypass the divisi engine; it "constantly looks for
the best option to keep the internal voice leading intact". Reviewers note it suits one-take performance more than independent counterpoint.

## 2. What we already have in the ecosystem

| Piece | What it gives a voice allocator |
|---|---|
| **ChordHarmonizer** `Voice to Channel` | Each voice of a chord on its own channel (top-first or bottom-first). *Only by position in the chord*: no memory, no ranges, no legato, no crossing logic. The seed of the idea. |
| **ChannelFan** | Key split into 2-4 zones with a channel each; **Fan to target channels** (= PolySWAM's Multi Mono); exact note-off bookkeeping per output channel (held-note masks). |
| **PolyGate** | The capture window done right: lookahead, "event window" that groups notes starting together into one event, event pick (outer + loudest / spread / low- or top-heavy), debris filter, register bands. Its engine is the natural front end. |
| **MpeBend** | Group window and automatic anchors (a ~1/16-beat hold-back), per-note channel bookkeeping (round robin over member channels). |
| **OrchNoteMapper / OrchGate** | Per-channel instrument ranges, octave folding into range ("Intelligent Transpose"), participation gating per instrument. |
| **OrchHarp / OrchPiano** | `selectVoices` (register-aware voicing), `kdeHandSplit` (melody/accompaniment split by the largest real gap between played pitches), `travelNote`. |
| **SeedRandomizer, MpeBend drift** | Replayable humanization of velocity, length, timing, pitch. |
| **Orch rig / Composer Mastermind** | One instrument = one MIDI channel convention; MC can drive modes and locks by CC (Modulator Targets). |

So the *new* parts are: an assignment engine with memory, range/overlap model, legato handling, the fill/thickening modes and the lock pedal.

## 3. Proposed design

**Name (working):** VoiceAllocator. MIDI effect in → N player channels out. Pure std-only engine (`VoiceAllocatorEngine.h`) with an
offline tool and checks, like PolyGate/ChannelFan.

### 3.1 Model
- **Players** (1..16): each has an output channel, a name, a playable range `[lo, hi]`, a *section* it belongs to, a
  home register (for affinity), and a state: free, or sounding (pitch, note-on time, input note id), plus `lastPitch` with the time it ended (memory).
- **Sections** (optional grouping like Violins / Violas / Cellos / Basses): a key range `[low, high]` of the *input* keyboard with a
  low and a high **overlap** (PolySWAM's Ranges screen) and a max polyphony = number of its players.
- **Presets** for the real rigs: SWAM quartet (4 players), Opus 5tett (5), string orchestra (violins 1/2, violas, cellos, basses), brass, woodwinds. Ranges from the instruments' real ranges.

### 3.2 Pipeline
1. **Capture window** (PolyGate style, 0 to ~1/16 beat, user setting; **0 = incremental, zero latency** like Multi Mono): notes whose starts fall in the window form one *event*. Latency is a constant lateness of everything after the plug-in (a MIDI plug-in cannot report latency to the host).
2. **Section routing** by key range and overlap (a note in an overlap zone is a candidate for both sections).
3. **Allocation** per event (below).
4. **Output**: note-on/off on the player's channel, with range folding (octave transposition into the player's range) and legato overlap handling; note-offs always follow the channel the note-on used (ChannelFan's held-mask pattern).

### 3.3 Allocation (the core)
For an event with notes `n1 < n2 < ... < nk` and candidate players sorted by register `p1 .. pm` (free players plus those whose note is ending):
- **Monotone (non-crossing) assignment by dynamic programming**: choose an increasing map note → player minimising
  `sum cost(note, player)`, O(k·m). With sorted notes and sorted players this *is* the optimal non-crossing assignment, so voice crossing is excluded by construction.
- `cost(note, player)` = pitch movement from the player's last pitch (full weight while its note is held/just ended, decaying with silence) + range penalty (outside `[lo, hi]` costs a fold, far outside is forbidden) + affinity bonus (same pitch class or same pitch as the player's recent notes, "memory affinity") − a continuation bonus if assigning the note to a player whose previous note is still sounding *and* the new note overlaps it (legato).
- **Held notes constrain new ones**: a sounding player is not a candidate unless it is a legato continuation; a note arriving between two held pitches can only go to a player whose register lies between them (keeps the non-crossing property over time).
- **Voice stealing**: if all players are busy, steal the player with the smallest cost penalty (oldest note, or the one furthest from the new pitch); setting: steal / drop the new note.
- **Single note** (monophonic line): stays on the *lead player of its section* (the player that played the previous note, i.e. continuity/affinity) with legato.

### 3.4 Modes
| Mode | Behaviour | PolySWAM / Divisimate equivalent |
|---|---|---|
| **Multi Poly** | Melody vs chord analysis: a single line stays on one player with legato; chords are distributed one note per player by the DP, melody (top, or by `kdeHandSplit`-style gap) keeps the lead player. | Multi Poly; Divisimate Top Down |
| **Complete Voices** | Always use all players of the section: *k* notes over *m* players, *k < m* → thicken: extra players double notes at unison or octave (weighted to the top and bottom), never leaving a player idle and never tripling a note while another is idle; *k ≥ m* → plain assignment. | Complete Voices; Divisimate Fill Voices |
| **Multi Mono** | Every note goes to every player in range (layering), no analysis, zero latency. | Multi Mono (= ChannelFan *Fan to target channels*) |
| **Bottom Up / Top Down** | Fixed chord-order assignment (ChordHarmonizer's current Voice to Channel). | Divisimate modes |

### 3.5 Legato (needs a quick live test first)
For mono-modelled strings (SWAM) a new note-on that overlaps the previous note on the same channel is played as legato. The allocator
makes that explicit: when a new note continues a line (same player chosen, previous note still held), it emits the new note-on *before* the old note-off;
when the previous note was released just before (within a "legato window"), it can optionally stretch the old note to overlap.
**Verify live on SWAM and Opus before relying on it** (our earlier SWAM work did not isolate this).

### 3.6 Extras (later phases)
- **Polyphony Lock** (pedal or CC): freeze the current player↔note assignment; new notes replace only unlocked players.
- **Voice/Memory affinity** across phrases (the DP's affinity term + a per-pitch-class "last player" table).
- **Humanize**: per-player replayable timing/velocity spread (reuse SeedRandomizer's hash approach).
- **Per-player CC routing**: dynamics/expression CC to all players, or per player (ChannelFan fan-out already does the all-players case).
- **Offline tool**: render a whole MIDI file into per-player channels with the *full phrase* known (better than real time: it can look ahead, so crossings and affinity are optimal), and export per-player MIDI/Dorico staves — this is where it beats PolySWAM for notation.
- **Composer Mastermind hook**: mode, lock, max polyphony as CC targets, so an arc can thicken/thin the section.

## 4. Fit and conflicts
- MIDI-only means no spread compensation, no engine-aware legato; instead we get **capturable output**, per-section control and notation.
- **Conflict with MpeBend**: both want to own the channel. A voice allocator output channel = player; MpeBend rewrites channels to MPE members. Use one per track (MpeBend per player track *after* the allocator splits by channel with Bitwig channel filters, or MPE only in single-player tracks).
- Needs the **CLAP** build if pitch bend must pass in Bitwig (same finding as ChannelFan/MpeBend).
- Capture of the output: the existing OrchCapture / OrchMerge path already records per-channel MIDI.

## 5. Build plan (proposed)
1. **v0.1 engine + checks + offline tool**: players/sections/ranges, DP assignment, the four modes, held-note bookkeeping, range folding; std-only checks (no crossings, monotone, held notes never stolen wrongly, note-offs on the right channel, determinism); Python/offline renderer for a MIDI file. A small test corpus: scale line, 4-note chords, a chorale with moving inner voices, a line that jumps register, a thick chord cluster.
2. **v0.2 plugin** (VST3 + CLAP): capture window, editor with the sections/ranges view (a piano strip like PolySWAM's), presets for SWAM quartet and Opus 5tett, counters.
3. **v0.3 legato + lock + affinity**, live-tested on SWAM and Opus.
4. **v0.4 humanize, MC CC hooks, notation export.**

## 6. Open questions for the user
1. Primary use: **live playing** (zero or tiny latency, incremental) or **processing recorded MIDI** (offline, whole-phrase optimum)? Both share the engine, but the order of work differs.
2. First target rig: SWAM quartet (4 mono players), Opus sections (the single-instance MPE problem already showed one Opus instance per section is safer), or the VEPro string/woodwind/brass instances?
3. How should a **thick chord** behave by default: thicken (Complete Voices) or leave players silent?
4. Is **channel = player** acceptable (so MpeBend cannot sit after it on the same track)?

## Sources
- PolySWAM product page: https://audiomodeling.com/products/polyswam
- What can PolySWAM do (KB): https://kb.audiomodeling.com/support/solutions/articles/206000082778-what-can-polyswam-do-
- PolySWAM vs Divisimate (KB): https://kb.audiomodeling.com/support/solutions/articles/206000082891-understanding-the-differences-polyswam-vs-divisimate
- MIDI Innovation Award entry: https://midi.org/innovation-award/polyswam
- Piano & Synth Magazine: https://pianoandsynth.com/polyswam-from-audio-modeling-coming-6-october/
- Divisimate 2 review (Sound On Sound): https://www.soundonsound.com/reviews/nextmidi-divisimate-2
