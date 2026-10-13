# Driving the Atonal Motive Transformer (AME) from Mastermind

AME is a sibling CLAP plug-in (`C:\atonal-motive-engine\`, github.com/johnpascu77-dotcom/AtonalMotiveEngine) that
transforms a captured motive through a chain of up to four operations. It is standalone-first and listens for a
small CC block on its input, exactly like OrchConductor's narrative bridge (see
`OrchConductor_MC_Integration_And_Narrative_Scan_Design.md`). **Mastermind needs no code for this**: the existing
Modulator Targets already send any CC on any channel. The full contract is AME's `docs/MASTERMIND_BRIDGE.md`.

| CC | AME control | Value |
|----|-------------|-------|
| 85 | chain preset | preset number, 0-based (shown in AME's preset dropdown) |
| 86 | chain scan | 0-127 scans the preset list |
| 87 | active stages | 0-4: first N stages on |
| 88 | stage mask | value / 8 = bit mask of stages on |
| 89 | output tap | 0-4: final, or after stage k |
| 90 | retrigger | 127 = one Retrigger on the next bar line (every message counts; send it a bar early) |

These avoid MPL's CC 20-64, OrchConductor's 20-54 / 102-105 and the field mask's 110-121. Put the targets on their
own MIDI channel (AME accepts any) and route that channel to AME's track, with an empty second Note-FX layer so it
merges with the notes (the same Bitwig lesson as OrchConductor).

## Recipes (Modulators tab, or `modulatorTargets[]` in a bundle)

**Shortcut:** on the Modulators tab, set the MIDI channel slider to the one the AME bridge listens on, press
**AME bridge...** and pick one of the six ready-made targets (`ame-stages`, `ame-preset`, `ame-scan`, `ame-mask`,
`ame-tap`, `ame-retrigger`). It adds or replaces the target with the recipe below; for the `section` ones you then enter the value
per section.

- **More transformation as the piece grows more complex:** CC 87, mode `arc`, dimension `complexity` (or `tension`).
- **A different chain per section:** CC 85, mode `section`; give each section a `modulatorValues` entry with the
  preset number.
- **Drift through the preset list over the whole piece:** CC 86, mode `arc`, dimension `narrative-position`. Turn on
  **Send 1 bar ahead** (the menu does) so each bar sends the position of the *next* bar: AME prepares that chain during
  the bar, and a CC 90 retrigger at the next bar line enters exactly the preset the position calls for. Without it the
  entrance plays the chain chosen one bar earlier. Scan changes only the chain; add CC 90 (section values) to hear
  each new chain, and do not combine CC 86 with CC 85 on the same instance.
- **Stage pattern per section:** CC 88, mode `section`, value = 8 x mask (stages 1+3 = mask 5 = value 40).
- **Enter the new chain exactly on each section's downbeat:** CC 90 = 127, mode `section`, and turn on **Send 1 bar
  ahead** for it and for the preset target (the "AME bridge..." menu does this for you). MC then sends the next
  section's preset and retrigger during the last bar of the previous section; AME prepares the new chain in that
  bar and fires the retrigger on the bar line itself, sample-accurately. Without "send ahead" the CC arrives on the
  bar line and the entrance trails it by AME's preparation time (a short delay-like smear). The first section of a
  piece has no bar before it, so its values go out at its start as usual.

```json
"modulatorTargets": [
  { "id": "ame-stages", "ccNumber": 87, "midiChannel": 15, "mode": "arc", "arcDimension": "complexity" },
  { "id": "ame-preset", "ccNumber": 85, "midiChannel": 15, "mode": "section", "arcDimension": "" }
]
```

Arc targets re-send every bar; AME acts only when the decoded value changes, so repeats never undo hand edits.
Changes are Future-Only and land within a callback or two, which suits bar-level control.
