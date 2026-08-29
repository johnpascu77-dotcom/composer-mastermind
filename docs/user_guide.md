# User Guide — Your First Piece

Everything else in `docs/` is written for whoever's building this plugin next session. This one is different: it's
written for whoever's *using* it — sitting in front of the Expert UI for the first time, MPL instances loaded,
wondering what to click. It builds one small real piece from nothing to playing audio, with the actual values
typed in at every step, not an abstract field reference. Skim past what you already know; come back to the exact
step where you got stuck.

The example piece: two instances, two sections, 24 bars, built entirely from the Expert UI. Once this works, every
other tab (Presets, Generate, Compose, Modulators) is optional depth on top of the same core loop, not a
prerequisite to it.

## Before you open the plugin

Composer Mastermind doesn't make sound itself — it sends MIDI CC to instances of MPL (MIDI Pattern Launcher), and
MPL does the actual playback. Two things need to be true before anything below will do anything audible:

1. **Each MPL instance you want to drive must have "External Control Enabled" turned on**, and set to a specific
   MIDI channel that no other MPL instance is also listening on. This is the single most common reason "nothing
   happens" — check this first, always, before suspecting a bug.
2. **Composer Mastermind's MIDI output needs to actually reach those MPL instances** in your DAW's routing (a
   shared MIDI bus, or track-to-track routing — however your session is set up; this part is ordinary DAW routing,
   not something this plugin's UI controls).

If you're not sure either of these is true yet, sort that out first — everything below assumes it already is.

## Step 1 — Register your instances (Instances tab)

Tell Composer Mastermind which MPL instances exist and which MIDI channel each one is listening on.

For our example, two instances:

| Field | Instance 1 | Instance 2 |
|---|---|---|
| Instance id | `mpl1` | `mpl2` |
| Channel | `1` | `2` |
| Role | Anchor | Motif |

For each one: type the id, drag Channel to match that instance's MPL channel, pick a Role, click **Add Instance**.
Role isn't cosmetic — it gates how much the mutation system is allowed to change that instance later (Anchor
barely moves; Motif and Counterpoint move more freely). If you're not sure yet, leave it Unrestricted and assign
roles later — you can always reload an instance (pick it from the combo below the form, click **Load**) and
resave with a different role.

You should now see both instances listed at the bottom, each showing `ch=1`/`ch=2` and its role.

## Step 2 — Build your scenes (Scenes tab)

A **scene** is one snapshot of settings — which pattern each instance plays, grid mode, swing — sent to your
targeted instances all at once. A **section** (next step) is a *stretch of bars* that plays one scene. You always
build scenes first, then reference them from sections.

Build two scenes for our example: a calm intro and a busier build.

**Scene 1 — `intro_scene`:**
- Active Pattern: `1`
- Grid Mode: `Binary`
- Swing: `0`
- Duration Bars: `8` (informational for this workflow — the *section* you attach this to is what actually
  controls how long it plays; leave the default)
- Targets: by default both `mpl1` and `mpl2` are already in "Pending targets" (every registered instance is
  seeded in automatically) — leave it, or click **Target All** if you've added instances since opening the tab
- Type `intro_scene` in the scene name field, click **Save Current Scene**

**Scene 2 — `build_scene`:**
- Active Pattern: `1`
- Grid Mode: `Binary`
- Swing: `30`
- Targets: leave both (Target All if needed)
- Optional: pick `mpl2` in the Override row, set its Grid Mode to `Ternary`, click **Add Override** — now `mpl2`
  runs a different feel than `mpl1` even though they share this scene's other settings. Check "Pending overrides"
  shows it before saving.
- Type `build_scene`, click **Save Current Scene**

You should now see both listed in the Scene Library display, each showing `targets=2`.

**If you need to fix a scene later**: pick it from the library combo, click **Load** (this fills in every field,
including the target and override lists — it does *not* send anything). Change what you want, click **Save
Current Scene** again with the same name to update it in place. **Send Scene Now** is a separate button — it
sends whatever's currently in the panel immediately, for previewing/testing without touching the saved library.

## Step 3 — Build your blueprint's sections (Blueprint tab → Sections)

This is where most first-time confusion happens (it's genuinely the least self-explanatory tab), so slow down
here.

A **section** ties one of your saved scenes to a bar range and gives it a name and an archetype. The blueprint is
just an ordered list of these. Build two:

**Section 1 — the opening:**
- Section id/name: `intro` (this is just a label for *this section*, not a scene name — pick anything short and
  memorable; it needs to be unique within this blueprint)
- Scene: `intro_scene` (pick it from the dropdown — this is where you connect the scene you built above)
- Start Bar: `0`
- Duration: `8`
- Archetype: `Presentation` (the calm opening statement — see the note below on what archetype actually does)
- Click **Add Section**

**Section 2 — the build:**
- Section id/name: `build`
- Scene: `build_scene`
- Start Bar: `8`
- Duration: `16`
- Archetype: `Build`
- Click **Add Section**

You should now see both in "Sections in this Blueprint": `intro bars=0-8 archetype=presentation ...` and
`build bars=8-24 archetype=build ...`.

**What Archetype actually does**: it's not decoration. Setting a section to `Peak`, `Build`, `Release`, or
`Presentation` is what lets the motif engine (if you use presets later) and the arc-derived curve view (the
Compose tab) reason about *where this section sits in the piece's shape*. `(none)` is valid — nothing breaks — it
just means this section stays untouched by anything archetype-driven. For a first piece, setting it correctly
costs nothing and unlocks those features later without having to come back and fix it.

**The part that trips people up — editing vs. creating:**

The id field always does *exactly* one of two things: type an id that doesn't exist yet and click Add Section,
and you get a **new** section. Type an id that already exists and click Add Section, and you **overwrite** that
section with whatever's currently in the form — including a blank Duration slider back at its default if you
didn't reset it, silently discarding what was actually saved there.

That second case is almost never what you want by accident. If you're trying to **change something about a
section you already built** (like `intro`'s duration), do **not** retype its name into the id field by hand.
Instead:

1. Scroll to the **"(pick section)"** dropdown near the bottom.
2. Select the section (e.g. `intro`).
3. Click **Load Section**.

This fills in every field — id, scene, start bar, duration, archetype, and any layer roles/budget overrides/
reserved values/modulator values it already has — from what's actually saved. Now change just the one thing you
came to change (e.g. drag Duration to `12`), and click **Add Section**. The status bar will confirm with
"Updated section 'intro'" — if it instead says "Added section," something didn't match and you just created a
duplicate; check the id field against the list above it.

The four "pending" rows above Add Section (layer roles, budget overrides, reserved values, modulator values) are
optional per-section fine-tuning — none of them are required for a first working piece. Leave them empty and move
on; they're worth learning once the basic loop is solid, not before.

## Step 4 — Save the blueprint

Scroll to "Blueprint Library" at the bottom of the Sections page:
- Type a name, e.g. `my_first_piece`
- Click **Save Blueprint**

This also makes it the *active* blueprint — playback will follow it as soon as the transport moves past bar 0.

**One setting you didn't have to touch, but should know exists**: the Scenes tab (and, as of this piece, the Score
View's own header too) has a **Content Mode** combo, Generative or Absolute. Everything you just built used the
default, Generative — every section stamps its notes from motif presets and whatever's live on the instrument,
same as always. Absolute is a different, optional thing: a section can instead carry literal, hand-captured note
content that plays back exactly the same way every time, frozen for as long as that section lasts. Nothing above
needed it, and leaving Content Mode on Generative is the right choice for a first piece — it's covered properly in
`docs/advanced_workflow_example.md` once you're ready for it.

## Step 5 — Make it play

Two ways to hear it:

- **Transport already stopped, want to check content lands correctly first**: click **Prime for Playback** (top
  right of the Sections page). This routes and stamps the first section's content immediately, without waiting
  for the transport — lets you confirm MPL is actually receiving something before you press play for real.
- **Just press play** in your DAW (or the Standalone app's own transport). As the host's bar counter crosses each
  section's Start Bar, Composer Mastermind automatically routes that section's scene — no separate "activate"
  step once a blueprint is saved as current.

## Step 6 — Confirm it's actually working

- The status line at the top of the Sections page should read something like `Blueprint 'my_first_piece' - bar
  N - now playing: intro` (or `build`) once the transport is past bar 0.
- Check the **Activity Log** tab — it logs every structural decision (section entries, mutations, etc.) with the
  bar number it happened at. If it's empty, nothing has actually fired yet.
- Check MPL's own instance windows (or the **Awareness** tab, which mirrors the same cached content) to confirm
  the Active Pattern actually changed on each instance.
- If you hear nothing but the log shows activity: it's almost certainly the "External Control Enabled" /
  channel-mismatch prerequisite from the top of this guide, not a bug in the blueprint you just built.

That's a complete, working piece. Everything past this point is optional.

## Reloading it later — the Score View's own shortcut

Everything above went through the Expert UI. Next time you open the plugin fresh (a new session, or after
switching projects), you don't need to go back into Expert just to get this piece playing again — the **Score
View** (click **Score View**, top right, from Expert; it's also the plugin's default screen) has its own small
header row with the same essentials: **Resync All**, the **Content Mode** combo, and a **Blueprint** dropdown with
**Load**, **Remove**, and **Prime for Playback** right next to it.

The everyday loop from a cold start: click **Resync All** first (so Composer Mastermind actually knows what's on
each instance right now), pick your saved blueprint (`my_first_piece`) from the dropdown, click **Load**. That one
click both makes it the active blueprint *and* primes the first section immediately — no separate Prime step
needed, and no detour through the Sections tab at all. Press play in your DAW same as before.

## Loading a score from a file, not the library

The Blueprint dropdown above only lists pieces already saved in *this session's* library. If someone sent you a
piece as a single JSON file (or you saved one yourself — see below), the same row has **Load Score...** and
**Save Score...** buttons for exactly that: **Load Score...** opens a file picker, imports the piece (blueprint,
scenes, instances, and motif presets, all in one file — see
[composition_bundle_format.md](composition_bundle_format.md)), makes it current, and primes it — one click, same
as picking from the dropdown and clicking Load. **Save Score...** does the reverse: writes whatever piece is
currently loaded out to a `.json` file you can hand to someone else, or re-import later, or eventually edit by
hand once you're comfortable with the format.

Don't confuse this with the Scenes tab's **Save/Load Snapshot To/From File** buttons, which look similar but do
something different — a snapshot round-trips your *entire session* (every instance, scene, blueprint, and preset
at once), while Load/Save Score here is scoped to one piece.

## What's next (all optional, in roughly the order you'd want them)

- **Presets tab** — once you're tired of hand-typing the same scene settings for every section, save reusable
  role/rhythmic-relationship/arc/motif bundles here and apply them instead. All four categories support Load for
  editing exactly like Sections now does.
- **Generate tab** — instead of hand-building every section, draw one arc curve (say, Energy rising then falling)
  and let it auto-assemble a full section-by-section blueprint proposal from your saved presets. Preview before
  committing.
- **Compose view** (the "Compose" button, top right, outside the Expert UI entirely) — pick a saved blueprint and
  see/edit its 5 arc dimensions as actual curves instead of numbers, sized to that blueprint's real length. Save
  edits as a new blueprint version.
- **Modulators tab** — two separate things live here, both optional, neither required for the piece you just
  built:
  - **External Modulator Targets** (the top panel) — if you want an arc to also drive one of your DAW's own
    modulators (a Bitwig Random/Curve modulator's Rate or Depth, say), not anything MPL itself plays, register a
    target here. Needs a one-time pairing step in Bitwig itself — see `docs/advanced_workflow_example.md`'s Step 8
    for the exact procedure.
  - **Instance Modulation Routes** (the panel below it) — this one drives a real MPL parameter directly, no
    Bitwig pairing needed at all. Try it on the piece you just built: Arc Dimension `complexity`, Instance `*` (All
    Instances), Pattern `1`, Parameter `Rotation`, leave Min/Max at `0`/`0`, click **Add / Update**. Now every
    instance's Pattern 1 rotation continuously drifts as `build`'s Complexity rises, instead of sitting still for
    all 16 bars — audible, with zero Mutations authored by hand. Broadcasting to `*` sends the same wiring to
    every registered instance at once, each computing and sending its own CC independently. If you point a route
    at a parameter a built-in curve already drives (Energy/Tension already drive Transpose; Density already
    drives Swing), your route simply takes over for whichever instance/pattern it targets — the built-in curve
    keeps driving every other instance untouched by it, no fighting between the two. `docs/advanced_workflow_example.md`'s
    Step 9 covers all 9 drivable parameters and the continuous-vs-threshold distinction in full.
- **Awareness / Activity Log / Milestones tabs** — all read-only diagnostics: what MPL actually has right now,
  what's happened and when, and session checkpoints you can jump back to. Nothing to author here, just to watch.

## Troubleshooting quick hits

- **Nothing sounds at all** → MPL's "External Control Enabled" + matching MIDI channel. Check this before
  anything else.
- **Edited a section but the piece sounds unchanged** → you probably typed a fresh Add Section instead of Load
  Section + Add Section. Check the status bar said "Updated," not "Added."
- **A saved scene only affects one instance instead of all of them** → check "Pending targets" before you saved
  it — it only includes whichever instances were in that list at save time, not automatically everyone.
- **Notes sound cut off or garbled on one instrument** → MPL instances are monophonic; overlapping note durations
  get automatically trimmed to the next note's start as of 2026-08-22, so this shouldn't happen anymore. If it
  still does, that's a bug — flag it.
- **Want to wipe everything and start a genuinely new piece** → the Instances tab has a **New Project (Reset
  All)** button. It clears every instance, scene, blueprint, preset, and modulator target you've saved — asks for
  confirmation first, since it can't be undone. It deliberately leaves session-local things alone (the Activity
  Log, Milestones), since those aren't authored content to lose.
