# Mutation Policy v0.1

Defines how much and how often Composer Mastermind should be allowed to change MPL instances' parameters, following
the source roadmap's cognitive-load principle (§8-9, §13.3, §16): *musical richness must be intentionally tamed.*
**Not yet implemented** — `Router::routeMutation` currently applies every mutation unconditionally. This document
specifies what `policy/PolicyEngine` and `policy/MutationPolicy` should enforce once built (v0.4 in
[composer_mastermind_design.md](composer_mastermind_design.md)).

## Change Weight Classification

Every `Mutation` should be classified by perceptual weight before being allowed through:

| Weight | Examples (from `Mutation::type` + context) |
|---|---|
| Minor | small `rotation` change, subtle swing adjustment |
| Medium | `transpose` by a small interval, `length` change, active-pattern switch |
| Major | `inversion` toggle, octave transposition, multiple layers changing pattern simultaneously |

This roughly maps onto `Mutation::strength` (already a field on the struct: "light"/"medium"/"strong",
[`model/Mutation.h`](../src/model/Mutation.h)) but that field is currently author-set and unvalidated — the policy
engine should derive/cross-check weight from `type` + `amount`, not just trust whatever `strength` string was
supplied.

## Budget Rules (from source roadmap §8.3)

- No more than 1 major change per bar, system-wide.
- No more than 2 major changes at a section boundary.
- An anchor-role instance (see design doc's role presets idea) should not receive major mutations during
  high-density sections.
- If one instance receives a pitch-affecting mutation (transpose) in a given pass, other instances should stay
  stable that same pass — avoid simultaneous transpose+rotation+length+inversion across multiple instances in one
  bar.

## Where This Plugs In

`policy/PolicyEngine` should sit between scene/mutation *intent* and `Router`:

```
Scheduler / manual UI trigger
        |
        v
policy/PolicyEngine  --(reject / downgrade / pass)-->  Router::routeMutation / routeScene
        |
        v
policy/MutationPolicy  (per-instance role + budget bookkeeping, ledger of changes this bar/section)
```

Per the design doc's "change-budget ledger" idea: this gate should apply to *every* mutation, including ones
triggered manually from the editor's test panel, not just scheduled ones — otherwise the governor is trivially
bypassed and gives a false sense of enforcement.

## Explicitly Deferred

Step-level mutation (§13, §16 of the source roadmap: target note/velocity/duration/enabled) depends on MPL
implementing a safe external step-edit queue first (its own v1.20.0 roadmap item) — see the CC map discrepancy note
in [routing_policy_v0_1.md](routing_policy_v0_1.md). This policy currently only governs the mutations already
wired end-to-end: transpose/rotation/length/inversion.
