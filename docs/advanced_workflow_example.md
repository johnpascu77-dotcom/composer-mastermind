# Advanced Workflow Example — "The Clearing"

`docs/user_guide.md` gets you from nothing to a playing piece using the *core* loop — Instances, a couple of
scenes, two sections, done. This doc picks up from there: one real piece, built through every field and tab the
core guide told you to skip, with the actual values typed in at every step and the *why* behind each decision —
not an abstract reference. Read the core guide first if you haven't; this one assumes you already know what a
scene, a section, and Prime for Playback are.

The example piece: three instances, four sections, 32 bars — first built entirely from the **Expert UI** (Part 1),
then a second time starting from the **Score View** instead (Part 2), a genuinely different way to build the same
kind of piece. Once you're comfortable with both, everything you do in this plugin is a variation on one of them.

## Part 1 — Building from the Expert UI

### Step 1 — Instances: give every instance a real role

Register three instances:

| Field | Instance 1 | Instance 2 | Instance 3 |
|---|---|---|---|
| Instance id | `mpl1` | `mpl2` | `mpl3` |
| Channel | `1` | `2` | `3` |
| Role | Anchor | Counterpoint | *(leave Unrestricted)* |

Leave `mpl3` on Unrestricted on purpose for now — you'll hit a real, useful surprise about what that actually
means in Step 6, and fix it there rather than being told about it in the abstract here.

### Step 2 — Scenes: two override layers, used together

A scene has **two different ways to make instances diverge**, and this piece uses both in the same scene —
`instanceOverrides` (Active Pattern / Grid Mode / Swing, per instance) and `patterns` (per-pattern-index transpose/
rotation/length/inversion). The core guide only used the first. Build four scenes:

**`intro` scene**: Active Pattern `1`, Grid Mode `Binary`, Swing `0`, targets all three. Override row: `mpl2` →
Active Pattern `1`, Grid Mode `Ternary`, Swing `0` — the counterpoint instance starts on a different grid than the
anchor from bar 1.

**`build` scene**: same base. Overrides: `mpl2` → Ternary, Swing `20`; `mpl3` → Active Pattern `2` (a different
pattern than the intro).

**`peak` scene**: overrides: `mpl2` → Ternary, Swing `40`; `mpl3` → Active Pattern `2`, Ternary, Swing `40`. Also
add one **per-pattern override** here (a separate row from the instance overrides above — look for the pattern
row, not the instance row): `mpl1`, Pattern `1`, Rotation `2`, Length `8`. This is the field the core guide never
touched — it changes how *one specific pattern* on one instance behaves, independent of the scene's global feel.

**`release` scene**: overrides: `mpl1` → Active Pattern `0` (stopped); `mpl2` → Binary, Swing `0` (settles back);
`mpl3` → Active Pattern `0` (stopped) — leaves only the counterpoint instance playing.

### Step 3 — Sections: all four pending lists, one real reason each

Build a blueprint with four 8-bar sections, same mechanics as the core guide's Step 3:

| Section | Start Bar | Duration | Scene | Archetype |
|---|---|---|---|---|
| `intro` | `0` | `8` | `intro` | Presentation |
| `build` | `8` | `8` | `build` | Build |
| `peak` | `16` | `8` | `peak` | Peak |
| `release` | `24` | `8` | `release` | Release |

Now use every pending list the core guide told you to leave empty:

- **Layer roles** on `intro`: mark `mpl3` **Background**. The scene already targets `mpl3` at Active Pattern `1`
  — Background overrides that and forces it silent regardless, holding it in reserve for later. (Foreground and
  Support don't force anything — MPL's protocol has no volume/prominence dimension to differentiate "prominent"
  from "supporting" at, only playing-vs-stopped. Only Background has real teeth.)
- **Budget override** on `build`: pick the `counterpoint` role, set Max Minor `4`, Max Medium `2`, Max Major `1` —
  loosens how freely mutations can touch `mpl2` specifically during this section, above whatever the default role
  table gives it everywhere else.
- **Reserved value** on `peak`: `mpl2`, Pattern `1`, type Transpose, value `7`. This is apex exclusivity — no
  earlier section's mutations can drift a transpose value into `+7` on this instance/pattern before `peak` itself
  arrives, so the climax value is guaranteed to still feel like an arrival, not something already heard by
  accident three bars earlier.
- **Layer roles** on `release`: mark `mpl1` and `mpl3` **Background** again — the scene already silences them too;
  this makes it redundant on purpose. If a future scene edit ever forgets to repeat that silence, the section's
  own layer role still enforces it.
- **Modulator values**: nothing to add yet — comes back in Step 7 once you've registered a target.

### Step 3a — Capturing literal content: Absolute vs Generative

Everything so far is **Generative** — every section stamps its notes fresh from motif presets and whatever's live
on the instrument at the moment it starts. There's a second mode, **Absolute**, for the opposite need: a section
that plays back the exact same notes every time, verbatim, frozen for as long as it lasts — a "demo song" or
mechanical-piano-roll piece rather than a self-composing one. The two modes aren't exclusive per piece — a section
with nothing captured always behaves generatively no matter which global mode is active, so you can mix frozen and
generative sections in the same blueprint.

Try it on `intro`. First, get real content onto an instance the normal way — hand-draw a short phrase for `mpl1`
in the Score View's Setup mode (Part 2 below covers this fully) and **Commit** it, or just let the piece play once
so a motif preset stamps something real. Then, back on the **Sections** tab:

1. Pick `mpl1` and the pattern index it's actually playing (Pattern `1`) in the **Captured Content** row.
2. Click **Capture Current**.

If Composer Mastermind hasn't actually confirmed that pattern's content yet (no resync since it was written), you
get an honest refusal instead of capturing something stale or blank — `Capture failed: no confirmed content for
'mpl1' P1 yet - resync it first (Awareness tab)`. Resync (Awareness tab, or the Score View header's **Resync All**
— see Part 2) and try again. Once it succeeds, `intro` shows `capturedContent=1` in the sections list — but this
alone doesn't change anything about playback yet.

**Content Mode** is what actually switches the behavior, and it's global, not per-section: the combo lives on the
Scenes tab (next to Save/Load Snapshot) and, as of this piece, on the Score View's own header too — both control
the exact same setting, so changing it from either place updates the other. Set it to **Absolute**. Now, every
time `intro` is entered — Prime for Playback, or the transport crossing bar 0 — Composer Mastermind writes back
exactly the captured content instead of stamping fresh, and holds it frozen there: no motif passes, no
phrase-chaining, no continuous melodic-curve or swing changes, for the entire 8 bars. Set Content Mode back to
Generative and `intro` returns to stamping normally — the capture itself isn't lost, it's just not being used.

### Step 4 — Save and play

Same as the core guide's Steps 4-6: name the blueprint, save it (this also makes it active), then press play or
click **Prime for Playback**. Check the Activity Log and Awareness tabs to confirm real content is landing on all
three instances before moving on.

### Step 5 — Arc curves: draw four dimensions, leave one blank on purpose

Open the **Arcs** tab (or the **Compose** view, outside the Expert UI — same underlying curve either way) and draw
four shapes across your piece's bar range:

- **Energy**: low at the start, rising through Build, peaking through Peak, falling through Release.
- **Tension**: similar shape, peaking a little sharper.
- **Density**: starts near zero, rises into Build/Peak, falls in Release — drives swing continuously.
- **Complexity**: rises and falls the same way — drives how often phrase-chaining fires and how much the mutation
  budget ceiling scales.
- **Coherence**: leave it blank. This is a deliberate, standing decision in this project, not an oversight —
  coherence as something you *aim for* (rather than just watch as a diagnostic on the Instances tab) is scoped for
  later and isn't wired to anything yet. Drawing a curve here wouldn't currently do anything.

These four run continuously — updating every bar a section is active, not just once at the section boundary — so
you'll hear them shift gradually rather than in steps.

### Step 6 — Presets: all four categories, and the gotcha you set up in Step 1

Open the **Presets** tab.

**Role preset**: name it `anchor_steady`, Target Role `Anchor`, Active Pattern `1`, Grid Mode `Binary`, Swing `0`.
Apply it to your `intro` scene — it should report affecting **1 instance** (`mpl1`, the only registered Anchor).

**Rhythmic-relationship preset**: name it `polyrhythm_pair`, two role slots — `Counterpoint` → Grid Mode `Ternary`,
and `Unrestricted` → Grid Mode `Binary`. Apply it to your `build` scene.

**Here's the surprise from Step 1**: this only affects **1 instance**, not 2. `mpl3` — the one you deliberately
left on Unrestricted — doesn't get touched. **Why**: "Unrestricted" is only a *label* the UI shows for an instance
with no role assigned at all; it was never something you can actually address a preset at. A role-addressed
preset (this category, and plain Role presets) can only ever reach instances that have a real, explicitly-picked
role — never the default. **The fix**: go back to the Instances tab, load `mpl3`, give it an actual role (even a
light one like `Texture`), resave, then re-apply the rhythmic-relationship preset — now it reaches both.

**Arc preset**: name it `swell`, shape `0→0.2, 0.5→1.0, 1→0.2` (a normalized swell, position 0-1 not bar numbers).
Apply it onto any arc dimension, over any bar range — it scales to fit whatever range you give it, which is the
whole point of keeping it normalized rather than tied to absolute bars.

**Motif preset — and a collision, on purpose.** Create two motif presets, both tagged `presentation`:
`theme_a` (a rising 3-note shape: offsets `0, 4, 7`) and `theme_b` (a different rising shape: offsets `0, 2, 5`).
Both are now eligible for any Presentation-archetype section. Play through your `intro` section and check the
Awareness tab against each instance's actual stamped notes — only one of the two ever gets used. **Why**: when
more than one preset shares a tag, the engine just takes the *first one in your library*, in save order — no
recency or "more specific" preference. **The practical takeaway**: don't assume "the preset I just made" is what a
section will actually use once you have more than one candidate for the same archetype — check the Awareness tab
against what's actually playing. If you want to guarantee a specific one wins, give it a tag nothing else already
claims, rather than relying on order.

### Step 7 — Generate: let it assemble a blueprint, and read the result correctly

Open the **Generate** tab. Pick a base scene (`intro` works), pick **Energy** as the driving arc (the curve from
Step 5), and click **Generate**. Nothing is saved yet — review the proposal, then **Commit** for real or
**Discard**.

**If your Energy curve rises immediately from a low starting point, peaks partway through, then falls** — a very
natural shape to draw — you'll likely see something like this:

| Section | Archetype |
|---|---|
| 1 | **Build** |
| 2 | **Peak** |
| 3 | **Release** |
| 4 | **Release** |

Two things worth understanding here, not just accepting:

- **No Presentation section appears** if your curve starts already climbing — Presentation only gets classified
  when there's something *before* a rise to contrast it with. A curve that opens flat or falling produces a real
  Presentation section instead; this is a direct, checkable consequence of the shape you actually drew.
- **Two Release sections in a row** happen if two points on your curve tie for the highest value. Exactly one
  section is ever classified Peak — the one *ending at* the highest point — and everything after that point is
  Release, even before the value has actually started dropping. If you want a clean Build→Peak→Release shape
  instead, give your two highest points slightly different values, or bring the curve down between them.

Committing replaces whatever blueprint was previously active — if you want your hand-built one back, just Commit
it again by name afterward.

### Step 8 — Modulators: registering a target, and pairing it to Bitwig for real

Open the **Modulators** tab. Register a target: CC number `70` (deliberately outside MPL's own 20-64 range, so it
never collides with a real MPL parameter), a MIDI channel distinct from your instances' channels, mode **Arc**,
dimension **Energy**. From now on this sends Energy's current value as a CC, continuously, every bar a section is
active — no further authoring needed.

Registering the target only sets up Composer Mastermind's own side. **Pairing it to an actual Bitwig modulator
needs a real, one-time hands-on step in Bitwig itself** — here's exactly how, since this part genuinely trips
people up:

1. **Install a virtual MIDI cable** — [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) works.
   Create one virtual port.
2. **Route Composer Mastermind's MIDI output through it**, in addition to (or alongside) wherever your MPL
   instances listen. This matters: Bitwig's own CC-learn — both the general "Map to Controller or Key" *and* a
   modulator's own "Learn CC" — only ever accepts **hardware-originated** MIDI. A plugin's own CC output doesn't
   count, even routed track-to-track inside the same project. Without this step, Learn CC will simply never catch
   anything.
3. **Register the loopMIDI port as a Controller** in Bitwig (Settings → Controllers → add a generic controller
   pointed at it), if it isn't already.
4. **Right-click the actual modulator parameter you want to drive** (e.g. a Random modulator's Rate knob) and
   choose **Learn CC** — not the general "Map to Controller or Key" from the same menu, which is the one that
   never sees plugin-generated CC. A modulator's own Learn CC is a separate, narrower mechanism that does work
   once steps 1-3 are in place.
5. **Fire one deliberate CC matching your target** while Bitwig is waiting for the Learn gesture — the Mutations/
   Debug tab's **Send Test CC** control is built for exactly this: pick the instance/channel, CC `70`, any value,
   Send Once.
6. **Confirm it landed** — Bitwig's mapping panel shows a line like `MAPPINGS FROM <your loopMIDI port> / CC70
   (Ch. N)`. From here the knob tracks every CC this target emits live — for an Arc-mode target, that means it now
   visibly follows the curve's shape during playback.

One limitation worth knowing: the pairing itself lives entirely in the Bitwig project file, never in anything
Composer Mastermind saves — a new Bitwig project needs this re-paired by hand, there's no way to export or replay
the pairing itself.

## Part 2 — Building the same kind of piece from the Score View instead

Everything above went through the Expert UI's tabs and forms. The Score View is a genuinely different way in —
piano-roll clicking and curve-dragging instead of typing numbers. Register at least one instance first (Expert →
Instances, same as Step 1) — the Score View still needs a registered instance to have anything to show.

### The header row: everything a cold start needs, without opening Expert

The Score View is also the plugin's *default* screen — the very first thing you see on a fresh open, before you've
touched Expert at all — so it carries its own small header row with the same cold-start essentials the Expert UI
scattered across three tabs: **Resync All** (Awareness tab's action), a **Content Mode** combo (Scenes tab's,
covered in Step 3a above — the two stay in sync no matter which one you change), and a **Blueprint** dropdown with
**Load**, **Remove**, and a standalone **Prime for Playback** next to it.

The real first-run shape this enables: open the plugin, see this view empty, click **Resync All**, pick
Generative or Absolute, pick a saved blueprint from the dropdown, click **Load**. That one click both makes it the
active blueprint *and* primes the first section immediately, so the view populates with real content right away —
or, if you're in Generative mode and nothing's been resynced/stamped yet, stays honestly empty rather than
guessing, which is itself the signal that Setup mode (next) is where to go draw something. No separate Prime click
needed, and nothing above requires Expert at all once a blueprint already exists in the library.

### Setup mode: drawing a pattern directly

From the Score View's main screen, click **Setup**. Pick your instance and a pattern slot. There's also a **Grid**
combo here — Binary or Ternary — and it means what it sounds like: whichever you pick is what you're drawing
*against* (16 evenly-spaced columns for Binary, 12 for Ternary), and it defaults to whatever grid mode that
instance is already tracked as, but you can change it freely before or after placing notes. Notes you've already
drawn aren't discarded if you switch — they just become temporarily hidden/unreachable past the new grid's column
count, and reappear if you switch back.

**Click anywhere on the grid to place a note** — no note-number or id fields, just click. Placing your first note
shows an "Unsaved changes" banner immediately; keep clicking to build out a short phrase.

Click **Commit to MPL** to write it for real — this sends both the pattern content *and* whichever Grid Mode you
picked, so MPL ends up matching what you actually drew rather than whatever grid it happened to already be in.
If nothing happens except an error naming your instance and "not connected" — that's the exact same prerequisite
as the very top of `user_guide.md`: **External Control Enabled**, on the right channel, in MPL itself. Setup mode
can't write anywhere MPL isn't actually listening, the same as every other write path in this plugin.

### Compose view: dragging a curve, and how it actually behaves

Click **Compose** and pick a dimension to edit (Energy, say). **Dragging an existing point** moves it. **Double-
clicking empty space on the line adds a brand new point there** — the curve's label switches to "Custom arc" the
instant you do, meaning you've turned a derived default into something real and editable. Drag that new point to
shape a swell.

**Save as new blueprint** needs an actual blueprint already picked in the **Blueprint** dropdown at the top of the
view first — this screen *versions* an existing blueprint's curve, it doesn't conjure a new blueprint out of a
curve alone. If you haven't built or generated one yet, do that first (Sections tab, or Generate), then come back
here to edit its curve visually.

**One thing worth knowing if you're jumping between tabs while building**: a few list-style panels in this app
(which blueprint shows up in a dropdown, which sections are listed) don't always refresh the instant something
changes elsewhere. If you just created or committed something in one tab and it's not showing up where you expect
in another, switch away and back to that tab once before assuming it didn't save — the underlying data is
reliably there even when a specific list hasn't caught up yet.

### Live mode: the moving playhead, and its one real limit

Switch back to **Live** and press play. The moving line you see isn't captured audio — Composer Mastermind can't
actually hear what MPL plays. It's *reconstructed*, purely from the host's transport position and MPL's own
step-timing math mirrored exactly, so it's normally exact. **The one case where it can drift**: if a pattern was
already looping *before* you opened Live mode to watch it (rather than started fresh after), the reconstructed
phase can be off by up to one loop cycle — unless the pattern's Length happens to be a power of two (1/2/4/8/16 in
Binary, 1/2/3/4/6/12 in Ternary), which is unaffected either way. Every fresh play start re-anchors cleanly, so
this only ever matters if playback was already running before you started watching.

### Drag-out export, in one line

With the transport **stopped**, drag directly out of the grid to export what just played as a real `.mid` file —
one track per instance, tempo-mapped, section names baked in as markers, with a toggle for as-performed vs.
notation-quantized timing.

## Troubleshooting quick hits (beyond the core guide's)

- **Two instances in different Grid Modes still sound rhythmically identical** → check the actual note *count* on
  each pattern, not just the grid mode. Four evenly-spaced notes sound exactly the same real-time pulse whether
  they're stored across 12 steps or 16 — grid mode changes *how finely* a bar is sliced for storage, not how many
  notes you put in it. A genuine polyrhythm needs a different note count on each side (three against four, not
  four against four), not just a different Grid Mode setting.
- **Grid Mode/CC is confirmed correct on both plugins, but it still doesn't sound right** → check the *rest* of
  your signal chain, not just MPL and Composer Mastermind. An instrument's own performance features — arpeggiator,
  chord mode, input quantize — sit between "MPL sent the right MIDI" and "you hear the right audio," and neither
  plugin's own debug readout can see past its own MIDI output. If the numbers look right on both ends and the
  audio still doesn't match, that's the next place to look.
- **A preset applied to a scene affected fewer instances than expected** → check whether every instance you meant
  to reach actually has a real, explicitly-assigned role. "Unrestricted" is a label, not something a preset can
  target (Step 6).
- **A motif preset you just made doesn't seem to be the one playing** → check whether another preset already
  shares its tag; the earliest-saved one with a matching tag always wins (Step 6).
