# Scene Format v0.1

Defines the JSON shape for a persisted `Scene`, once `state/SceneLibrary` / `state/StateSerializer` exist (v0.2 in
[composer_mastermind_design.md](composer_mastermind_design.md)). **Not implemented yet** — today a `Scene` only
ever exists transiently, built in memory by the editor's test panel
([`plugin/PluginEditor.cpp`](../src/plugin/PluginEditor.cpp)) and never saved or loaded.

## v0.1 Scope: Flat Scene Only

The source roadmap describes both a flat scene model (§10.1) and a hierarchical layer-based model (§10.2). This
project's current `Scene` struct ([`model/Scene.h`](../src/model/Scene.h)) is the flat model — global fields plus a
list of per-instance `ScenePattern` overrides. v0.1 of the JSON format should serialize exactly that struct, field
for field, rather than jumping straight to the hierarchical `layers: {...}` shape from §10.2's example. The
hierarchical model is a v0.3+ concern (blueprint sections assign roles/foreground/background *across* scenes; a
single scene doesn't need that structure yet).

## JSON Shape (mirrors `Scene` 1:1)

```json
{
  "schema": "ComposerMastermindScene.v1",
  "id": "scene_intro",
  "name": "Intro",
  "durationBars": 8,
  "quantize": "bar",
  "targets": ["mpl_bass", "mpl_motif"],
  "global": {
    "activePattern": 1,
    "gridMode": 0,
    "swing": 12.0
  },
  "instanceOverrides": [
    {
      "targetInstance": "mpl_motif",
      "gridMode": 1,
      "activePattern": -1,
      "swing": -1.0
    }
  ],
  "patterns": [
    {
      "targetInstance": "mpl_bass",
      "patternIndex": 0,
      "transpose": -12,
      "rotation": 0,
      "length": 16,
      "inversion": false
    }
  ],
  "mutations": [],
  "nextSceneId": "scene_statement",
  "transitionStyle": "hard",
  "rampBars": 0
}
```

Field-for-field mapping to [`model/Scene.h`](../src/model/Scene.h):

| JSON key | C++ field | Notes |
|---|---|---|
| `id`, `name` | `Scene::id`, `Scene::name` | required, validated non-empty by `Validation::isValidScene` |
| `durationBars` | `Scene::durationBars` | must be > 0; drives `SceneAdvancePolicy` (v0.3) |
| `quantize` | `Scene::quantize` | currently unused by any code — reserve for v0.3's timing options |
| `targets` | `Scene::targets` | instance ids receiving the `global` block via CC 20/22/24 |
| `global.*` | `Scene::global` (`SceneGlobal`) | `gridMode` 0/1, `swing` 0-75, validated; the *default* sent to every target unless overridden |
| `instanceOverrides[]` | `Scene::instanceOverrides` (`vector<SceneInstanceOverride>`) | per-instance divergence from `global` — `-1`/`-1.0` means "inherit global" for that field. This is what makes differentiated/poly-rhythmic scenes possible (see [composer_mastermind_design.md](composer_mastermind_design.md), "'Send To All' Is One Preset Among Many") |
| `patterns[]` | `Scene::patterns` (`vector<ScenePattern>`) | each entry targets one instance's one pattern index (0-2) |
| `mutations[]` | `Scene::mutations` (`vector<Mutation>`) | scene-bundled mutations, not yet consumed by any routing code — `Router::routeMutation` takes a single `Mutation` today, not a scene's list; wiring this up is part of v0.2 |
| `nextSceneId`, `transitionStyle`, `rampBars` | same | scene-chain fields, inert until `SceneAdvancePolicy` (v0.3) exists |

## Validation

`Validation::isValidScene` ([`util/Validation.h`](../src/util/Validation.h)) already checks id/name non-empty,
`durationBars > 0`, grid mode, and swing range. It does **not** currently validate `targets`/`patterns` reference
real registered instance ids, or that `patternIndex` is 0-2 — `Router` silently no-ops on an unknown instance id or
out-of-range pattern index rather than erroring. Scene loading (v0.2) should decide whether to surface that as a
load-time warning (recommended — a scene file authored against instance ids that don't match the current registry
is a common authoring mistake and should be visible, not silently inert).

## Blueprint / Scene-Set JSON (later, v0.3+)

Out of scope for v0.1. When built, it wraps a list of scenes plus the roadmap's §11/§18 section/arc structure —
see [composer_mastermind_design.md](composer_mastermind_design.md) roadmap and the source roadmap's §18.2 example
JSON, which this project should follow closely rather than re-deriving independently.

## Preset Library (later, v0.2+, not this doc's scope yet)

The bigger vision — a JSON library of named, reusable, role-addressed presets (role presets, arc presets,
rhythmic-relationship presets) that sections reference by id rather than inlining raw `Scene` field values — is
described in [composer_mastermind_design.md](composer_mastermind_design.md)'s "'Send To All' Is One Preset Among
Many" section. This doc only defines the *scene* shape those presets ultimately resolve to; the preset-library
schema itself needs its own doc once v0.2 persistence work actually starts.
