# Composition Bundle Format (v3) — "Composing by Numbers"

This is the file format behind the user's own framing: one JSON file, a blank score with every parameter Composer
Mastermind can control in MPL, downloadable, hand-editable, and re-importable to reconfigure the plugin into that
exact piece. It's real and working today, reachable two ways:

- **Score View** (the plugin's simple default screen) — **Load Score...**/**Save Score...** buttons right in the
  header, no detour into Expert mode needed at all. This is the intended everyday path for "just load a score and
  play it."
- **Expert UI's Sections tab** → **Export**/**Import** (same underlying format, same files - interchangeable),
  or the MCP bridge's `create_blueprint`/`create_scene`/`create_motif_preset` actions authoring the same shape
  piece by piece for anyone driving Composer Mastermind programmatically.

A ready-to-import starting point lives alongside this doc: **[composition_bundle_template.json](composition_bundle_template.json)**
— a minimal two-section piece (Presentation → Peak) with every top-level field populated, most values at sensible
defaults. Load it via either path above to hear it play, then start changing numbers.

**To see a full, complex, real piece in this exact format**: any of the 5 blueprints already in the library
(`prelude_in_stillness`, `three_against_four`, `perpetuum_mobile`, `invention_no1`, `twelve_tone_invention`) can be
exported right now (Score View's Save Score, or the Sections tab) — that's the fastest way to see every field
actually used in a finished 12-section piece, rather than reading this doc in the abstract.

**Not the same as the Scenes tab's Save/Load Snapshot buttons** — those round-trip the entire session (every
instance/scene/blueprint/preset library at once, via `StateSnapshotStore`), a different, older, coarser mechanism.
Loading a composition-bundle file through the snapshot loader will not do what you expect (it parses a different
top-level shape) - always use Load Score/Import, not Load Snapshot, for a single piece.

## Top-level shape

```json
{
  "schema": "ComposerMastermindComposition.v3",
  "blueprint": { ... },
  "scenes": [ ... ],
  "instances": [ ... ],
  "motifPresets": [ ... ],
  "modulatorTargets": [ ... ],
  "modulationRoutes": [ ... ]
}
```

`instances`, `motifPresets`, `modulatorTargets`, and `modulationRoutes` are optional — an older export (v1/v2) or
a hand-written file that omits them imports fine, just relying on whatever's already registered in the session.
For a genuinely self-contained score (import into an empty session, hear the whole piece), include all six.

## `instances[]` — who's playing

One entry per MPL instance the piece needs.

| Field | Meaning |
|---|---|
| `id` | Must match the id used everywhere else in this file (`scenes[].targets`, `.patterns[].targetInstance`, etc.) |
| `name` | Display name only |
| `midiChannel` | 1-16, which MPL instance this routes to (each MPL instance listens on its own External Control channel) |
| `role` | `"anchor"` / `"motif"` / `"counterpoint"`, or a custom string. Matters a lot — see "Roles decide who plays" below |
| `enabled` | `false` disables routing to this instance entirely |
| `lastKnownScene` | Bookkeeping only, safe to leave `""` |

**Roles decide who plays, not just what.** A `build` section only stamps/develops instances whose role is
`motif` or `counterpoint`; `release` only touches `counterpoint`; `presentation`/`peak` touch everyone. This
happens regardless of what's explicitly named in a scene — an instance can go silent or untouched in a section
purely because its role doesn't match that archetype's eligibility, which is a common surprise the first time you
write a piece by hand.

## `motifPresets[]` — the actual musical material

This is the piece's most easily-missed requirement, and the direct cause of a real bug found while testing: a
blueprint section with no matching motif preset stamps nothing and plays silence, with no error. A preset is
"matching" a section when one of its `tags` equals that section's `archetype` string exactly.

| Field | Meaning |
|---|---|
| `id`, `name` | Identity |
| `tags` | Archetype strings this preset can be chosen for — `"presentation"`, `"build"`, `"peak"`, `"release"` |
| `notes[]` | The actual cell: `semitoneOffset` (relative to the cell's own first note), `relativeDuration`/`relativeVelocity` (multipliers on whatever the target step already has, not absolute values) |

If more than one preset shares a tag, the engine picks between them (novelty-aware, avoiding whichever the
previous section just used) — write several presets with the same tag for real variety across a piece, not just
one per archetype.

## `blueprint.sections[]` — the piece's timeline

| Field | Meaning |
|---|---|
| `id`, `name` | Identity |
| `sceneId` | Which scene from `scenes[]` this section routes when it becomes active |
| `startBar`, `durationBars` | This section covers `[startBar, startBar + durationBars)`. Sections should be non-overlapping and in order |
| `archetype` | `"presentation"` / `"build"` / `"peak"` / `"release"`, or `""` for "untouched by the motif/rhythm engine" |
| `layerRoles[]` | `{targetInstance, layerRole}` — `layerRole` `"background"` silences that instance for the section (unless the scene already says otherwise); `"foreground"`/`"support"` are recorded but don't currently force anything without a volume dimension |
| `budgetOverrides[]` | `{role, maxMinorPerBar, maxMediumPerBar, maxMajorPerBar}` — per-role override of how many discrete `Mutation`s of each weight that role may receive this section; `-1` = unlimited, omit a role to use the static default |
| `reservedValues[]` | `{targetInstance, patternIndex, type, value}` — "apex exclusivity": no earlier section may also reach this exact value for this instance/pattern/parameter, so a later climax still lands as a genuine peak. `type` is one of `transpose`/`rotation`/`length`/`inversion`/`retrograde`/`m7`/`rate`; for the three booleans, `value` is `0`=off, nonzero=on; for `rate`, `value` is the resolved state (`0`=Augmented/`1`=Normal/`2`=Diminished), `patternIndex` is ignored (rate is global per-instance) |
| `modulatorValues[]` | `{modulatorTargetId, value}` — a fixed 0-127 CC value sent once at this section's boundary to an external Bitwig modulator (see `modulatorTargets[]` below); skipped if the id isn't registered |
| `capturedContent[]` | Literal, verbatim step content (bypasses the motif engine entirely for that instance/pattern) — normally produced by the editor's "Capture Current" button, not hand-authored |

## `blueprint.arcCurves[]` — the piece's shape over time

Five possible dimensions, each an independent piecewise-linear curve: `{dimension, points: [{bar, value}]}`,
`value` conventionally 0..1. Omit a dimension entirely to let it fall back to a flat per-archetype baseline
instead of an authored curve. What each one currently drives, every bar a section is active:

| Dimension | Drives |
|---|---|
| `energy` | Melodic curve amplitude (how wide the register drift swings) |
| `tension` | Melodic curve register center (pulls the shared tonal center upward as it rises) |
| `density` | Swing state — banded into Off/Triplet/Shuffle (even thirds across 0..1), not a continuous glide |
| `complexity` | Mutation budget scaling (1x-2x) **and** phrase-chain target banding for build/peak/release sections |
| `coherence` | Retrograde/M7 divergence — below 0.35, Retrograde turns on; below 0.15, M7 joins it. Both thresholds sit below every archetype's baseline, so this only fires on a deliberately authored dip, not just by picking an archetype |

Full detail and exact formulas: [arc_dimension_mapping_concept.md](arc_dimension_mapping_concept.md).

## `scenes[]` — what each section actually sends

| Field | Meaning |
|---|---|
| `id`, `name` | Identity |
| `durationBars`, `quantize`, `nextSceneId`, `transitionStyle`, `rampBars` | The older scene-chain mechanism (manual "Send Scene Now" / chained scenes) — inert when a blueprint is driving playback via section `startBar`/`durationBars`. Safe to leave at defaults for a blueprint-only piece |
| `targets[]` | Instance ids that receive `global` |
| `global.activePattern` | `0` = stopped, `1`-`3` = which pattern plays |
| `global.gridMode` | `0` = Binary (16 steps), `1` = Ternary (12 steps) |
| `global.swing` | `0.0` (Off), `66.67` (Triplet), or `100.0` (Shuffle) — any other value gets snapped to whichever of the three is nearest, both on import and at CC-send time |
| `global.rate` | `0` (Augmented), `1` (Normal, default), or `2` (Diminished) — a real classical augmentation/diminution device: the whole active pattern restated in longer or shorter note values, onsets and durations both scaling together. Global per-instance like `swing`, CC 23 |
| `instanceOverrides[]` | `{targetInstance, activePattern, gridMode, swing, rate}` — per-instance divergence from `global`; `-1`/`-1.0` means "inherit global" for that one field, so an instance-override entry only needs to set the fields it's actually overriding |
| `patterns[]` | `{targetInstance, patternIndex, transpose, rotation, length, inversion, retrograde, m7}` — direct per-pattern parameter values. `patternIndex` is 0-2 (MPL's P1/P2/P3). `transpose` -48..48 semitones; `rotation` 0-15 steps; `length` 1-16 steps; `inversion`/`retrograde`/`m7` booleans |
| `mutations[]` | `{type, targetInstance, patternIndex, amount, applyAtBar, strength}` — authored one-shot changes; `type` is the same 7-value vocabulary as `reservedValues[].type` above. `amount` is a *delta* from whatever that parameter is currently tracked at (not an absolute value) for `transpose`/`rotation`/`length`; for the 3 booleans, `amount != 0` is an absolute on/off toggle, matching `reservedValues`' convention. For `rate`, `amount` is clamped to `[-1, 1]` and is instead an *absolute target state* (`-1`=Augmented, `0`=Normal, `+1`=Diminished, centered on `0` so an omitted `amount` stays inert) — e.g. `{"type": "rate", "targetInstance": "MPL1", "amount": -1, "applyAtBar": 24}` flags a cadential close a bar or two before a section ends, the whole reason this parameter exists |

## `modulatorTargets[]` — optional, for driving external Bitwig modulators

Only relevant if a section's `modulatorValues[]` references one. `{id, ccNumber, midiChannel, mode, arcDimension}` —
`ccNumber` is deliberately outside MPL's own 20-64 protocol range (a virtual-MIDI-cable-routed CC paired to a
Bitwig modulator's own "Learn CC", not anything MPL itself decodes); `mode` is `"arc"` (continuously tracks an
`arcDimension` value every bar) or `"section"` (a fixed value per section, authored via `modulatorValues[]`
instead). Most pieces won't need this at all.

## `modulationRoutes[]` — the modulation matrix, for driving a real MPL parameter

`{id, arcDimension, targetInstance, patternIndex, parameter, outputMin, outputMax, threshold, invert, enabled,
dispatchMode, sequenceValues, phraseLengthBars}` — distinct from `modulatorTargets[]` above: this drives an actual Composer
Mastermind `Instance`'s own parameter directly (through the same CC encoding `patterns[]`/`mutations[]` use), not
an external Bitwig modulator. `targetInstance` is an instance id from `instances[]`, or `"*"` to broadcast the
same route to every registered instance. `patternIndex` (0-2) only matters for the 6 pattern-scoped parameters;
ignored for `swing`, `rate`, `activePattern`, and `gridMode`. `parameter` is one of `transpose`/`rotation`/`length`/
`swing`/`rate` (continuous) or `inversion`/`retrograde`/`m7`/`gridMode` (boolean, crosses `threshold` 0..1) or
`activePattern` (bands into 4 states: 0 = stop, 1-3 = pattern - `threshold`/`outputMin`/`outputMax` unused).
`rate`'s natural range is `0..2`, banded to the nearest of Augmented/Normal/Diminished - leave `outputMin`/
`outputMax` at `0`/`0` to use it. `invert` flips the arc sample (`1 - value`) before mapping.

`dispatchMode` (default `"bar"`) only matters for the 5 continuous parameters:
- `"bar"` — the arc's value is linearly mapped into `[outputMin, outputMax]` and re-sent every bar; `0`/`0` means
  "use that parameter's own full range."
- `"sequence"` (2026-08-27, "fragment sequencer") — ignores `arcDimension`/`outputMin`/`outputMax`/`threshold`
  entirely, and instead steps through `sequenceValues` (a plain array of numbers) once per pattern LOOP CYCLE
  instead of once per bar - real melodic sequencing (a short fragment restated at a new pitch level every
  repetition, the device a Bach invention/fugue uses constantly). Shrink the target pattern's `length` first (in
  its scene's own `patterns[]` entry) so a loop cycle is short enough to actually cycle through the sequence at a
  musical rate. A loop cycle can never exceed one bar (MPL's own pattern `length` caps at 16 steps) - for
  anything longer, use `"barCycle"` instead.
- `"barCycle"` (2026-08-27, phrase-cadence breathing) — same `sequenceValues` shape and ignored-field set as
  `"sequence"`, but steps once every `phraseLengthBars` BARS instead of pattern steps, decoupled entirely from
  the one-bar ceiling above - genuine multi-bar phrasing. A short `phraseLengthBars` (2-4) gives phrase-internal
  breathing inside a section; set it equal to a whole section's `durationBars` for a once-per-section cadence
  instead (e.g. `rate` dipping to Augmented near a section's end, with no manually-fired mutation needed - the
  actual reason this mode was built). **Give different instances different `phraseLengthBars` on purpose** - an
  ensemble where every voice breathes on the same period sounds mechanically synchronized; independent periods
  (2 bars on one instance, 3 on another, 4 on a third) drift in and out of alignment across a section the way
  real independent phrasing does, achieved deterministically (a different fixed number per route), not via any
  randomness.

If a route targets the same instance/parameter a built-in consumer already drives (Energy/Tension → Transpose,
Density → Swing, Coherence → Retrograde/M7, Complexity → Active Pattern), the route wins for that instance and the
built-in consumer skips it there - true regardless of dispatch mode.

## What's still hand-wavy

- `mutations[]` inside a scene aren't yet dispatched by anything (`Router::routeMutation` takes one `Mutation` at
  a time, not a scene's list) — send individual mutations via the MCP bridge's `send_mutation` or the Debug panel
  instead, for now.
- No validation catches a `targets`/`patterns` entry referencing an instance id that isn't in `instances[]`, or a
  `patternIndex` outside 0-2 in most places — a typo there silently no-ops rather than erroring on import.
