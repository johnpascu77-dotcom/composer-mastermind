# Concepts Mined from atonal_phrase_engine

## Source

`C:\Users\Asus\Documents\atonal_phrase_engine\` — a standalone Python atonal phrase/composition generator (not a
plugin, outputs MIDI/MusicXML), built over many sessions (`generate_phrase_v031.py`, 23,337 lines; ~150 incremental
`patch_v0*.py` files from v0.2.0 through v0.5.1d). Unrelated codebase to Composer Mastermind, but the user asked
that it be mined for transferable musical/architectural concepts (2026-08-15) — it's the origin of the "Formal
Field Target Curves" chart shown earlier in the conversation. This doc records what was found and how it maps
(or explicitly doesn't) onto Composer Mastermind. Nothing here is implemented yet; it's reference material for
v0.4/v0.5 and beyond.

**Fundamental scope difference to keep in mind throughout:** that engine generates notes/pitches directly. Composer
Mastermind never does — it only ever sends CC to control an external instrument plugin's coarse exposed parameters
(active pattern, transpose, rotation, length, inversion, grid mode, swing). Every mapping below has to be
reinterpreted down to that coarse level; see "What Does Not Transfer" at the end.

## Pipeline Overview (their engine, for context)

1. Vocabulary load & validation (pitch cells, motives, gestures, section curves, phrase template)
2. Phrase plan construction (section list from template + curves)
3. Foundation & derived-line generation (actual events per section)
4. Internal shaping / apex sculpting (climax bars get internal arrival→intensify→apex→overflow structure)
5. Global governance passes (cross-section corrections: apex exclusivity, arrival reservation)
6. Diagnostics / formal X-ray (concordance scoring, formal report)
7. Expansion to larger form (16-bar model stretched to 64 bars)
8. Event realization & surface constraints (target field → actual note/rest stream)
9. Phrase-aware diagnostics & morph planning (per-phrase formal role + risk/intent)
10. Guarded mutation pipeline (candidate → intent manifest → clone-only mutation → proposal → independent gate)
11. Render & export (MusicXML/MIDI + full JSON decision trace)

## Mechanisms Worth Naming

### Arrival Reservation
`enforce_final_apex_arrival_reservation` (v0.3.5). If an earlier preparatory section's target reaches or exceeds
the final climax's ceiling, it's forcibly pulled back by a reservation margin. **Problem solved:** sections
evaluated independently will happily let "development" accidentally hit the ceiling early, killing the climax's
exclusivity.

### Global Apex Exclusivity
`enforce_final_climax_global_registral_apex` (v0.3.3). The complementary rule: forces the designated climax to
actually *be* the highest point in the piece by promoting it after the fact if something else already exceeded it.

### Apex Sculptor
`sculpt_climax_events`. Reshapes a climax bar *internally* — arrival → intensification → apex → overflow phases
within the bar, weighted by position — rather than treating "intensity: 0.95" as a flat block. **Problem solved:**
a single curve value produces uniform loudness/density, not a shaped gesture with rhetoric.

### Parameter Concordance Diagnostics
`compute_parameter_concordance_diagnostics` (v0.3.4). Per section, normalizes ~10 dimensions (density, intensity,
velocity, register, rhythmic activity, motivic distortion, morph pressure, etc.), counts how many simultaneously
exceed a threshold (0.70), and derives a `concordanceScore` (fraction converged) + `formalEnergy` (weighted
blend). Then explicitly checks: is the *labeled* climax also the energy peak, concordance peak, and register
peak? This is diagnostic, not enforcement — it measures whether intention and outcome actually agree.

### Formal Memory Map
`apply_formal_memory_map_v042a` (v0.4.2a). Per bar-range: `memoryRole`, a `noveltyBudget` (0.25 at presentation →
0.8 at climax → 0.25 at coda), explicit `preserveParameters` vs `transformParameters` lists, and a
`listenerGuidePriority`. Operationalizes "don't change every salient parameter at once" as data, not prose.

### Mutation Intent Manifest → Proposal → Gate chain
v0.5.0d–v0.5.1d. A staged authorization pipeline: candidates generated → checked against protected
bars/apex-functions → written into an intent manifest with explicit capability flags (`mayMutatePitch=false`,
etc.) → mutated only on a cloned layer (diffed against canonical, never touching it) → proposed → independently
gated with rollback-readiness requirements. **Problem solved:** once mutation logic exists, an unguarded generator
can silently corrupt a protected climax bar; this makes "can this touch bar 56?" an explicit, auditable question
at every stage, not an assumption.

### Named groove vocabulary tied to formal role
`groove_id_for_bar_v043g` (v0.4.3g): `A1_STABLE`, `CLIMAX_PRESS`, `APEX_BURST`. Rhythmic identities bound to
section function instead of generic random duration pools — rhythmic memory instead of noise.

### Lookahead / trajectory-aware morphing
`build_lookahead_morpher_plan_v044b`. A bar's meaning depends on value *and* direction: "energy 0.7 rising toward
a climax" ≠ "energy 0.7 falling after one." Plans carry current value, delta from previous, delta toward next,
and distance to phrase goal — not just an interpolated scalar.

## Mapping to Composer Mastermind

### Already basically the same idea, more developed — use these as the blueprint

| Their mechanism | Our concept | What to take from it |
|---|---|---|
| Parameter Concordance Diagnostics | Coherence/convergence arc (v0.5) | This is the closest match in the whole codebase. Concrete formula to adapt: sample each instance's current CC-parameter state, count how many are simultaneously near shared target extremes, derive a scalar coherence score — and separately validate "does the scene actually marked as climax in the chain turn out to be the convergence point?" as a diagnostic pass over the scene chain, not just a live number. |
| Formal Memory Map (`noveltyBudget` + preserve/transform split) | Cognitive-load governor (v0.4) | Argues the governor shouldn't be a flat "max N mutations per bar" counter but a budget split by category, keyed to formal position, cross-referenced against `Instance::role` — their `listenerGuidePriority` is effectively "which role stays the stable guide right now." Ties the governor and role system together from the start instead of bolting role-awareness on later. |
| Section curves + lookahead morpher | Generic `Arc` evaluator (v0.5) | Argues the Arc evaluator's API should expose direction/delta and distance-to-next-breakpoint, not just an interpolated value — so a Mutation can distinguish "energy rising through 0.7" from "energy falling through 0.7," which changes what response is musically appropriate. |

### Genuinely new capabilities not yet planned

- **Cross-scene Arrival Reservation / apex exclusivity.** Nothing today stops an earlier scene's `instanceOverrides`
  or arc targets from prematurely spending the values reserved for a later climax scene in the `nextSceneId`
  chain. Requires the Arc evaluator (or a new governance pass) to know about reserved ceiling values downstream
  and clamp earlier scenes accordingly — a cross-scene constraint, not a per-scene one.
- **Internal ramp shaping.** Extending `Scene::transitionStyle`/`rampBars` from one named style + linear ramp into
  an internal arrival→intensify→apex→overflow shape for the transition itself (a non-linear CC trajectory within
  a single scene transition), rather than one flat lerp.
- **A lightweight mutation-authorization gate.** Not the full clone/diff/rollback ceremony (overkill for realtime
  MIDI) but the core idea: a validation step between a `Mutation` being authored/proposed and it being allowed to
  actually dispatch CC, checking it doesn't touch a protected instance/role/bar combination (e.g. don't mutate the
  anchor instance during the climax bar). A real gap in the current model — `Router::routeMutation` today applies
  anything handed to it, unconditionally.
- **Named presets-per-formal-role, precedent for v1.1.** Their groove vocabulary and legacy-grammar-map (reapplying
  small-form roles onto a larger cyclic form) is precedent for the generative layer's planned preset library: a
  vocabulary of named archetypes (`presentation`, `local_climax_approach`, `global_climax`, `coda_echo_aftermath`)
  bound to arc position, reusable across differently-sized pieces — not composed from scratch per piece.

## What Does Not Transfer

Anything at the note/pitch level: pitch-cell vocabulary, motive interval/duration/contour data, elaboration
gesture vocabulary (ornaments, turns, connectors), instrument range/playability profiles, and all
rendering/notation machinery. "Registral apex" as literal MIDI pitch height has no direct analog — our coarsest
comparable parameter is `transpose`, so any apex-exclusivity logic has to be reinterpreted around transpose/energy
CC ceilings, not real pitch. Fine-grained rhythm generation (rest-splitting, duration quantization) is irrelevant
since we don't generate onsets/rests, only grid mode/swing at the instrument side. Multi-voice contrapuntal
mechanics (imitation, hocketing) don't transfer at the note level, though their multi-instance analog is already
captured more coarsely by the coherence dimension.

## Priorities (adopted into the roadmap)

1. Coherence/convergence evaluator, built on the Concordance Diagnostics model — see v0.5 in
   [composer_mastermind_design.md](composer_mastermind_design.md).
2. Cognitive-load governor built around noveltyBudget + preserve/transform, keyed to role — see v0.4.
3. Cross-scene Arrival Reservation / apex exclusivity as an Arc evaluator constraint — see v0.5.
