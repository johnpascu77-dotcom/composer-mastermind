# The `romanian_shadow` style profile in Composer Mastermind

A style profile (see AME's `docs/STYLE_PROFILES.md`) is a shared vocabulary: AME presets for *how* a motive is
answered, a Mastermind score for *when and in what form*, and optionally a pitch field for the Orch filter. This is
the first one: Romanian-flavoured atonal writing, built from the six sections of the Python quartet "Joc de Umbre".

## Set it up

1. Install the 24 chain presets into AME (once; existing files are kept):

   ```
   python C:\atonal-motive-engine\profiles\romanian_shadow\make_presets.py --install
   ```

   Open an AME editor: the preset dropdown now lists `RO 01 Joc strâmb` … `RO 24 Ecou` after the factory presets
   and your own `01 …` presets, numbered as the bridge numbers them.

2. Build the score. It looks the preset *numbers* up from what is installed, so run it after step 1 and again if
   you add or rename presets:

   ```
   python score-generator\romanian_shadow.py            # -> docs/example_score_ro_joc_de_umbre.json
   python score-generator\romanian_shadow.py --channel 15 --no-field   # other CC channel, no pitch field
   ```

3. In Mastermind's Score View press **Load Score...** and pick `docs/example_score_ro_joc_de_umbre.json`, or
   import it in the Expert UI's Sections tab. Route Mastermind's MIDI output to the track that hosts AME, as for any
   AME bridge (docs/ame_bridge.md), and capture a motive in AME. Rewind to bar 1 and play.

## What the score does

42 bars, 21 two-bar sections in six movements:

| Bars | Movement | Chains (RO …) | Section root | Scene (MPL1–3) |
|---|---|---|---|---|
| 1–6 | Joc strâmb | 01 02 03 | C | MPL1 + MPL2 (a fifth up) |
| 7–12 | Strigături | 05 06 07 | D | MPL1 + MPL3 (octave down), Diminished rate |
| 13–18 | Joc cu pedală | 09 10 12 | Eb | all three; MPL3 low and Augmented (the pedal) |
| 19–28 | Cântec de jale | 13 14 15 16 17 | Ab | MPL1 alone, Augmented — the lament, slow |
| 29–36 | Joc posedat | 18 19 20 21 | F# | all three, ternary grid, MPL2 inverted and Diminished |
| 37–42 | Umbre | 22 23 24 | Bb | MPL3 alone, low and Augmented |

Every two bars each section sends, one bar early, **CC 85** (the chain) and **CC 90** (retrigger on the next bar
line), so the new chain enters exactly on the downbeat; the first two bars have no bar before them and enter a
little late. The six movements are the form; the 24 chains are the vocabulary; change a movement's preset list in
`FORM` (top of the script) to re-orchestrate the piece.

## The optional pitch field

At the first section of each movement the score also sends that movement's **pitch field** — Romanian minor on the
section root (`C D Eb F# G A Bb` shifted to C, D, Eb, Ab, F#, Bb) — as Mastermind's 12-bit pitch-class mask on
**CC 110 / 111** (channel 1 by default: low 7 bits = pitch classes 0–6, high 5 bits = 7–11). An OrchNoteFilter in the
chain, listening on those CCs, will pull notes toward the field. Nothing else uses them: AME and MPL ignore CC 110/111.
Two cautions: Mastermind's own *pitch-field broadcast* (Mutations/Modulators tab) must stay off, or it will overwrite the
profile's masks; and use `--no-field` if no OrchNoteFilter is in the chain.

## Not part of it yet

Rhythm cells (dotted, compound-meter), meter changes, and the quartet's texture modes (homorhythmic blocks, a long
line migrating between instruments) are not profile data yet; the lament's travelling line in particular wants
several AME instances, one per voice. See AME's `docs/STYLE_PROFILES.md` → *Not built yet*.
