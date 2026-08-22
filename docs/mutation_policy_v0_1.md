# Mutation Policy v0.1

Defines how much and how often Composer Mastermind is allowed to change MPL instances' parameters, following the
source roadmap's cognitive-load principle (§8-9, §13.3, §16): *musical richness must be intentionally tamed.*
**Implemented (v0.4, confirmed working live in Bitwig 2026-08-16).** `Router::routeMutation` now consults
`policy/PolicyEngine` before dispatching any CC.

## Change Weight Classification (implemented in `MutationPolicy::classifyWeight`)

| `Mutation::type` | Rule | Weight |
|---|---|---|
| `inversion` | always | Major |
| `transpose` | `\|amount\| >= 12` (an octave or more) | Major |
| `transpose` | `\|amount\| < 12` | Medium |
| `rotation` | `\|amount\| <= 2` steps | Minor |
| `rotation` | `\|amount\| > 2` steps | Medium |
| `length` | always | Medium |
| unrecognized type | always | Medium (cautious default, not "harmless") |

Grounded directly in the source roadmap's own classification (§9): inversion and octave-plus transposition are
explicitly called out as major; smaller transposition and length changes as medium; subtle rotation as minor. No
"minor transpose" tier exists in the source material, so transpose is Medium-or-Major only, never Minor, matching
that.

`Mutation::strength` (the free "light"/"medium"/"strong" string field on the struct) is still not consulted —
weight is derived from `type` + `amount` as originally planned, not trusted from whatever string an author
supplied. `strength` remains an author-facing hint field only; nothing currently reads it.

**`amount` is a delta, not an absolute target (fixed 2026-08-16, see [routing_policy_v0_1.md](routing_policy_v0_1.md)
for the full story):** this table's thresholds read *more* correctly now than when first written, not less — an
`|amount| >= 12` transpose genuinely means "changing by an octave or more from wherever it was," which is what
"major" should mean. Before the fix, `amount` was (bugged) treated as an absolute target, which this same
threshold would have measured incorrectly (e.g. a transpose *to* 40 read as "major" regardless of whether the
actual audible jump from the previous value was 1 semitone or 40).

## Budget Rules (implemented in `MutationPolicy`)

Per-instance, per-bar, keyed by `Instance::role` (`MutationPolicy::budgetForRole`):

| Role | Max Minor/bar | Max Medium/bar | Max Major/bar |
|---|---|---|---|
| `anchor` | 1 | 0 | 0 |
| `motif` | 2 | 1 | 1 |
| `counterpoint` | 3 | 2 | 1 |
| unrecognized/empty role | unlimited | unlimited | unlimited |

Plus one system-wide rule, independent of role: **no more than 1 major mutation per bar, across all instances**
(`MutationPolicy::globalMaxMajorPerBar`, default 1, matching source roadmap §8.3).

**Deliberate v0.4 approximation, not the final model:** the source roadmap's §7.1 describes anchor mutation as
"rare," not strictly zero — a true "rare" behavior needs a multi-bar cooldown window, which needs the v0.5 Arc
evaluator to exist first (a budget that resets every single bar can't express "at most once every 4 bars," only
"at most N times per bar"). `anchor`'s medium/major budget is set to 0 as the closest expressible approximation
today: an anchor instance gets no medium/major mutations through the automatic gate at all, only occasional minor
ones. Revisit once arcs can modulate budgets over a section instead of resetting flatly every bar.

**Not implemented, explicitly deferred to v0.5:** "no more than 2 major changes at a section boundary" and
"an anchor instance should not receive major mutations during high-density sections" both require section/arc
awareness that doesn't exist until blueprints (v0.5) are built. Only the per-bar and system-wide rules above are
live today. Also deferred: "if one instance receives a transpose in a pass, others should stay stable that pass"
— a cross-instance-in-the-same-bar correlation rule, not yet implemented (each instance's budget is currently
independent of what other instances did this same bar, aside from the shared system-wide major cap).

**Update (2026-08-16):** section awareness now exists as data — `model/Blueprint.h`'s `BlueprintSection::
budgetOverrides` (a `SectionBudgetOverride` per role) is exactly the per-role override this table's "deliberate
v0.4 approximation" note above was waiting on.

**Wired (2026-08-17):** `PolicyEngine::authorize` and `MutationPolicy::tryConsume` now accept an optional
`const RoleBudget* budgetOverride` — when non-null, it's used in place of `budgetForRole(role)` for that one
call. `Router` doesn't know the `Blueprint` model exists (kept decoupled deliberately): it holds a
`BudgetOverrideResolver` (`std::function<bool(role, currentBar, RoleBudget&)>`), injected via
`Router::setBudgetOverrideResolver`. `ComposerCore`'s constructor wires this to a new private method,
`ComposerCore::resolveSectionBudgetOverride`, which locks the current blueprint, finds the section whose
`[startBar, startBar+durationBars)` covers `currentBar`, and returns that section's `SectionBudgetOverride` for
the given role if one exists (false — fall back to the static table — if no section covers the bar, or the
covering section doesn't override that role). `Router::routeMutation` calls the resolver right before
`policyEngine.authorize`, so **both** dispatch paths get this for free from the same choke point that already
existed: the scene chain's `Scene::mutations` (via `Router::routeScene`'s internal `routeMutation` calls) and
`DebugPanel`'s manual "Send Mutation" button both flow through `Router::routeMutation`, neither needed its own
call site touched. A role/bar combination with no covering section, or covered by a blueprint with no override
for that role, behaves exactly as before — this is additive, not a change to existing budget behavior when no
blueprint is active.

**Confirmed working live in Bitwig (2026-08-17):** an Anchor-role instance (normally 0 medium/major budget) with
a "Climax" section (bars 0-16) overriding Anchor to Med=1/Maj=1 — a transpose mutation to that instance
succeeded during playback ("Sent transpose mutation to '1'"), where it would have been blocked without the
section's override in effect.

**Still not done (as of 2026-08-17):** the "no more than 2 major changes at a section boundary" and
cross-instance-in-the-same-bar correlation rules from the deferred list above still don't exist — this pass
only wired the per-role override value itself, not new rule *types*.

## Apex Exclusivity (the clamping half, built 2026-08-17)

The other long-deferred v0.5 item — "don't let an earlier scene spend the climax's reserved values" — is now
built. `model/Blueprint.h`'s `ReservedValue` (`targetInstance`, `patternIndex`, `type`, `value`) marks a
specific *resulting* per-pattern parameter value as belonging to a later section; `BlueprintSection::
reservedValues` holds a list of them. Enforcement mirrors the budget-override wiring exactly: `Router` holds an
injected `ReservedValueChecker` (`std::function<bool(targetInstance, patternIndex, type, value, currentBar)>`,
set via `Router::setReservedValueChecker`), and `ComposerCore::isValueReservedByLaterSection` implements it —
locks the current blueprint, and returns true only if some section whose `startBar` is *strictly after*
`currentBar` reserves that exact value. `Router::routeMutation` computes the mutation's resulting absolute
value (transpose/rotation/length's new absolute value, or 0/1 for inversion — the same `updatedPattern` fields
already computed for CC encoding, not the delta) right after the existing budget check, and blocks the send
entirely (no CC, no state recorded) if the checker says yes.

**Scope, matching the budget-override wiring's precedent exactly:** only `Router::routeMutation` is gated —
`Router::routeScene`'s direct `ScenePattern`/`SceneGlobal` sends (`sendScenePattern`/`sendSceneToInstance`) are
**not** checked against reserved values, same as they're not checked against mutation budgets. A scene's own
absolute pattern values are treated as a deliberate whole-state snapshot rather than the granular "is this
specific nudge allowed" question apex exclusivity is about — if an author explicitly builds an early scene that
matches the climax's exact ScenePattern values, that's on them; this only stops a *mutation* (an incremental
nudge) from drifting into reserved territory.

The section that owns a reserved value is always free to reach it — the check only fires for a bar *before*
that section starts, never during or after it, so the climax section itself is never blocked from playing its
own reserved values.

Confirmed working live in Bitwig (2026-08-17): an inversion mutation reserved for the "climax" section was
blocked while sent from "intro" (bar < 8), succeeded once the transport crossed into "climax" (bar >= 8), and
was unaffected for other instance/pattern/value combinations not covered by that reservation.

## Where This Plugs In (as built)

```
Scheduler / scene chain / manual UI "Send Mutation"
        |
        v
Router::routeMutation(mutation, currentBar)
        |
        v
PolicyEngine::authorize(mutation, targetInstance.role, currentBar)
        |
        v
MutationPolicy::classifyWeight + tryConsume  --(true: consumed, false: rejected)-->  CC dispatch or silent skip
```

`Router::routeMutation` is the single choke-point every mutation path already flowed through before v0.4 (scene
chain, `Scene::mutations`, and the manual UI button all call it) — the gate was added *inside* that function
rather than requiring each caller to remember to check first, so it genuinely can't be bypassed by any current
caller, matching the "single gate" goal without needing to touch every call site. `Router::routeScene`'s own
direct CC sends (`sendSceneToInstance`/`sendScenePattern`, i.e. `Scene::global`/`Scene::patterns`) are **not**
gated — scoped deliberately to `Mutation` dispatch only, since scenes are already governed by the v0.3 chain/
duration mechanism and represent a deliberate whole-state snapshot rather than the granular "how often does this
one thing change" question the budget model is about.

`ComposerCore::getCurrentBar()` (new, atomic, updated each `processBar` tick) gives the manual UI path a bar
number to authorize against even though message-thread button clicks aren't otherwise bar-aware — without this,
"gate manual sends too" would have been unenforceable outside live playback.

## Explicitly Deferred

Step-level mutation (§13, §16 of the source roadmap: target note/velocity/duration/enabled) depends on MPL
implementing a safe external step-edit queue first (its own v1.20.0 roadmap item) — see the CC map discrepancy
note in [routing_policy_v0_1.md](routing_policy_v0_1.md). This policy currently only governs the mutations
already wired end-to-end: transpose/rotation/length/inversion.
