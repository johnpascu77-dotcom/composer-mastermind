# The `romanian_shadow` style profile in Composer Mastermind

A style profile (see AME's `docs/STYLE_PROFILES.md`) is a shared vocabulary: AME presets for *how* a motive is
answered, a Mastermind score for *when, by whom, and in what form*, and optionally a pitch field for the Orch
filter. This is the first one: Romanian-flavoured atonal writing, built from the six sections of the Python
quartet "Joc de Umbre".

**One AME per voice.** Mastermind sends control changes; AME makes the notes. So a voice *is* one AME instance on
its own MIDI channel, and the score decides, every two bars, what each voice does.

## Set it up (once)

1. **Install the 24 presets** into AME (existing files are kept):

   ```
   python C:\atonal-motive-engine\profiles\romanian_shadow\make_presets.py --install
   ```

2. **One track per voice in Bitwig.** For each voice add a track with an AME (and an OrchNoteFilter after it, if
   you use the pitch field) and your instrument. Route Mastermind's MIDI output to each track, filtered to that
   voice's channel: V1 = channel 1, V2 = 2, V3 = 3, V4 = 4. Capture the same (or different) motive in each AME.
   Put each voice in its register with a Transpose note effect or your instrument's range (V1 high … V4 low):
   the chains do not move a voice into its register.

3. **Build and load the score** (see *Using the generator* below), then in Mastermind's Score View press
   **Load Score...** and pick `docs/example_score_ro_joc_de_umbre.json`. Its 16 targets (`v1-preset`,
   `v1-retrigger`, `v1-field-lo`, `v1-field-hi`, `v2-…`) appear on the Modulators tab. Remove old targets that send
   on the same CCs and channels (an earlier `ro-preset-scan`, `ame-scan`, `ame-preset`…): a stray arc-mode target
   such as a scan keeps sending its CC 86 every bar and will fight a voice's chain. **Load Score now offers to clear these leftovers** (instances, modulator targets and modulation routes
   already in Mastermind; scenes, presets and blueprints stay): answer *Clear and load* when switching to a different score.

4. Rewind to bar 1 and play. Nothing needs MPL instances: the score has no MPL scenes.

## What the default plan does

42 bars, 21 two-bar steps in six movements; the voices follow the quartet's characters
(V1 high and agile, V2 answers, V3 mediates, V4 low). `python romanian_shadow.py --list` prints it all:

| Bars | Movement | Root | Idea |
|---|---|---|---|
| 1–6 | Joc strâmb | C | V1 alone, then V2 answers (a related motive), V3 enters in canon |
| 7–12 | Strigături | D | calls and cries: V1 high call, V2 and V4 low answers, V3 joins |
| 13–18 | Joc cu pedală | Eb | V4 holds drones (RO 09, 11); V1, V2, V3 dance over them |
| 19–28 | Cântec de jale | Ab | the lament: one long line passed V2 → V3 → V4 → V1 → V2 |
| 29–36 | Joc posedat | F# | all four voices, dense and diminished, graces everywhere |
| 37–42 | Umbre | Bb | thinning out: slow low echoes, a narrowed ghost, then the last ones |

A voice that rests in a step gets no retrigger, so it simply lets its last entrance finish.

Every voice's preset (CC 85) and retrigger (CC 90) go out **one bar ahead**, so each new chain enters exactly on
the bar line; the first step of the piece has no bar before it and enters a little late.

## The pitch field (optional)

On the first step of each movement every voice's channel also receives that movement's **pitch field**: Romanian
minor on the section root (`C D Eb F# G A Bb` shifted to C, D, Eb, Ab, F#, Bb) as Mastermind's 12-bit
pitch-class mask on **CC 110 / 111** (low 7 bits = pitch classes 0–6, high 5 bits = 7–11). An OrchNoteFilter with
**CC Control** enabled pulls notes toward it (AME forwards these CCs to the next device in the chain). Mastermind's
own *pitch-field broadcast* must stay off, or it overwrites the masks. Leave it out with `--no-field`.

## The AME Score page (the easy way)

In Mastermind's **Score View**, the **AME Score...** button (next to *Generate Score...*) opens
`score-generator/ame_score_generator.html` in your browser. It does everything the command-line generator does:

1. **Read AME preset folder...** and pick `%APPDATA%\AtonalMotiveEngine\ChainPresets` (paste it into the folder
   dialog's address bar). The page reads only the preset *names* and computes AME's lane numbers from them; it
   remembers them for next time. (*Assume the standard lane* is only for trying the page out: the numbers are right
   only if your folder holds nothing but `RO 01–24`.)
2. **Voices**: how many (1–4) and each voice's MIDI channel; whether to send the pitch field (CC 110/111).
3. **The plan**: first the **step length** (2, 3, 4, 6 or 8 bars; default 2). Each cell lasts one step, and a motif longer
   than a step overlaps the next entrance, because a later retrigger never cuts an earlier one. Then one card per movement (name, character, pitch-field root, number of two-bar steps); in each card a
   grid with a preset dropdown per voice and step (`— rest —` = the voice is silent in that step). `+ Movement`,
   `Reset`, `Save plan...` / `Load plan...`; and **Generate a plan**: pick a *form* (a sequence of preset families such as
   Joc de Umbre arc, Lament and dance, Possessed rush), a *seed* and a *restlessness* (0–1) and the page rolls a whole new plan
   (same form + seed + restlessness = same plan; the title carries the seed, so each roll saves under its own name). Edit any
   cell afterwards; the plan file is the same (the same plan file the command-line generator reads with `--plan`).
4. **Download score**, then *Score View → Load Score...*.

The page and `romanian_shadow.py` produce byte-identical scores from the same plan and presets.

## Using the command-line generator yourself

The generator is `score-generator\romanian_shadow.py` (needs only Python). Open a PowerShell in the
Mastermind folder (or double-click `score-generator\run_romanian_shadow.bat` for the same thing with arguments).

1. **Look at the plan.** Each row is a voice, each cell is one two-bar step; `--` is a rest:

   ```
   python score-generator\romanian_shadow.py --list
   ```

2. **Make your own copy of the plan** (the starting point for any change):

   ```
   python score-generator\romanian_shadow.py --dump-plan my_plan.json
   ```

   Open `my_plan.json` in any text editor. Per movement you can change the name, `archetype`, `root` (the pitch
   class of the section's pitch field, 0 = C … 11 = B), and `voices`: for each of V1–V4 one entry per step.
   Add `"stepBars": 4` at the top level for 4-bar steps (default 2); `firstBar` is then recomputed for you. An entry is a **number** (preset `RO 01`–`RO 24`), a **name** (`"16 Close"`, or the start of one such as
   `"RO 13"`), or **0** for a rest. All voices of a movement must have the same number of entries. To lengthen a
   movement, add an entry to every voice; to add a movement, copy a movement block and give it the next `firstBar`
   (2 bars per step).

3. **Check, then build.**

   ```
   python score-generator\romanian_shadow.py --plan my_plan.json --list
   python score-generator\romanian_shadow.py --plan my_plan.json --out docs\my_score.json
   ```

   The first line prints your plan back as names (a typo shows up as an error naming the movement, voice and
   step). The second writes the score file.

4. **Load it** in Mastermind (Score View → Load Score...) and play.

Useful switches: `--voices 2` (fewer voices), `--channels 5,6,7,8` (other MIDI channels), `--no-field` (no pitch
field), `--presets-dir` (another preset folder). The score always looks up preset numbers from the presets
actually installed, so after adding or renaming presets simply run it again.

Ideas to try: swap which voice carries the lament line; give V4 `"16 Close"` in the last step; put a movement's
`root` on another pitch class; add a fifth step to *Joc posedat* by adding an entry to each voice; use your own
presets by name (any preset in the folder works, not only `RO …`).

## Not part of it yet

Rhythm cells (dotted, compound-meter), meter changes, and homorhythmic blocks are not profile data yet; the voices'
registers are set in Bitwig, not by the score. See AME's `docs/STYLE_PROFILES.md` → *Not built yet*.
