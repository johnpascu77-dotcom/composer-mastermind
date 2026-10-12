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

These avoid MPL's CC 20-64, OrchConductor's 20-54 / 102-105 and the field mask's 110-121. Put the targets on their
own MIDI channel (AME accepts any) and route that channel to AME's track, with an empty second Note-FX layer so it
merges with the notes (the same Bitwig lesson as OrchConductor).

## Recipes (Modulators tab, or `modulatorTargets[]` in a bundle)

- **More transformation as the piece grows more complex:** CC 87, mode `arc`, dimension `complexity` (or `tension`).
- **A different chain per section:** CC 85, mode `section`; give each section a `modulatorValues` entry with the
  preset number.
- **Drift through the preset list over the whole piece:** CC 86, mode `arc`, dimension `narrative-position`.
- **Stage pattern per section:** CC 88, mode `section`, value = 8 x mask (stages 1+3 = mask 5 = value 40).

```json
"modulatorTargets": [
  { "id": "ame-stages", "ccNumber": 87, "midiChannel": 15, "mode": "arc", "arcDimension": "complexity" },
  { "id": "ame-preset", "ccNumber": 85, "midiChannel": 15, "mode": "section", "arcDimension": "" }
]
```

Arc targets re-send every bar; AME acts only when the decoded value changes, so repeats never undo hand edits.
Changes are Future-Only and land within a callback or two, which suits bar-level control.
