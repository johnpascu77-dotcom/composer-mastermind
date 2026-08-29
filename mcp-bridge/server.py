"""MCP server exposing Composer Mastermind's own internal state.

This is the second half of the MCP bridge described in
docs/technical_spec_checklist.md's "MCP bridge" section. The first half is
McpBridgeServer (src/composer/McpBridgeServer.h/.cpp), a small JSON
request/response socket embedded in the Composer Mastermind plugin itself,
listening on 127.0.0.1:47824. That C++ side is pure transport plus a small
action table (ComposerCore::handleMcpBridgeRequest) - it doesn't speak MCP at
all, deliberately, since implementing the actual MCP protocol in C++ was out
of scope for a first slice.

This file is the piece that actually speaks MCP: a thin proxy, one tool per
bridge action, each tool just opening a fresh connection to the socket,
sending {"action": "<name>"}, and returning whatever the plugin's own live
state says back. No caching, no interpretation - if the plugin isn't running,
every tool call fails with a clear connection error rather than returning
stale or fabricated data.

Both read and write actions are exposed. Every write tool routes through the
exact same pathway the plugin's own UI uses (Router::routeMutation,
CCDispatcher::sendCC, PatternSyncServer::requestSync, setCurrentBlueprint,
setCurrentScene) - nothing here is a new, separate code path invented for
MCP; it is genuine remote control of the plugin, not a simulation of it.

Run standalone for testing (stdio):
    python server.py
Run for remote/tunnel registration (this environment's Claude app only accepts
remote HTTPS connectors, confirmed while setting up WigAI - see
mcp-bridge/README.md and memory/project_bitwig_mcp_advisor_scope.md):
    python server.py --http
Serves streamable-http on 127.0.0.1:8765/mcp, meant to be reached through a
cloudflared quick tunnel (same mechanism already used for WigAI).
"""

import argparse
import json
import socket
import struct

from mcp.server.mcpserver import MCPServer
from mcp.server.transport_security import TransportSecuritySettings

# Must match McpBridgeServer.h's magic number and kPort exactly - the C++
# side owns this wire format (JUCE's InterprocessConnection framing: an
# 8-byte little-endian header [magic, payload size] followed by the raw
# JSON payload), not something this file gets to redefine independently.
_MAGIC = 0xF2B49E2C
_HOST = "127.0.0.1"
_PORT = 47824
_TIMEOUT_SECONDS = 5.0


def _call_bridge(action: str, **params) -> dict:
    """Sends one request to Composer Mastermind's McpBridgeServer and
    returns its "result" object. Raises a plain, readable exception (which
    the MCP framework surfaces as a tool error) if the plugin isn't
    running, the connection is refused, or the plugin itself reports
    ok: false for this action (e.g. an unknown instance/blueprint/scene id,
    or a mutation blocked by policy budget)."""

    payload = json.dumps({"action": action, **params}).encode("utf-8")
    header = struct.pack("<II", _MAGIC, len(payload))

    try:
        with socket.create_connection((_HOST, _PORT), timeout=_TIMEOUT_SECONDS) as sock:
            sock.sendall(header + payload)

            response_header = _recv_exact(sock, 8)
            magic, size = struct.unpack("<II", response_header)
            if magic != _MAGIC:
                raise RuntimeError(f"unexpected response header from Composer Mastermind (magic={magic:#x})")

            response_bytes = _recv_exact(sock, size)
    except (ConnectionRefusedError, OSError) as error:
        raise RuntimeError(
            "could not reach Composer Mastermind's MCP bridge at "
            f"{_HOST}:{_PORT} - is the plugin (Standalone or hosted in Bitwig) running? ({error})"
        ) from error

    response = json.loads(response_bytes.decode("utf-8"))

    if not response.get("ok", False):
        raise RuntimeError(f"Composer Mastermind rejected '{action}': {response.get('error', 'unknown error')}")

    return response.get("result", {})


def _recv_exact(sock: socket.socket, size: int) -> bytes:
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise RuntimeError("Composer Mastermind closed the connection mid-response")
        data += chunk
    return data


server = MCPServer(
    name="composer-mastermind",
    instructions=(
        "Full read and write access to Composer Mastermind's own live "
        "internal state (a JUCE autonomous MIDI conductor plugin driving "
        "MIDI Pattern Launcher instances in Bitwig). Use the read tools "
        "instead of asking the user for screenshots of the plugin's own "
        "tabs when you need to know current instance roles, cached pattern "
        "content (Awareness), the active blueprint/section, motif presets, "
        "or coherence. Every write tool routes through the same pathway "
        "the plugin's own UI uses (real budget gating for mutations, real "
        "playback effect for commit_blueprint/set_scene) - this is genuine "
        "remote control, so use get_instances/list_blueprints/list_scenes "
        "to confirm valid ids before acting rather than guessing them. "
        "create_scene/create_blueprint/create_motif_preset/set_arc let you "
        "author entirely new library content from a JSON payload (reusing "
        "the same deserializers the plugin's own save/load uses) - this is "
        "how a whole piece gets composed as numbers rather than through "
        "the UI's tabs."
    ),
)


@server.tool()
def get_status() -> dict:
    """Small combined heartbeat: current bar, coherence score, active
    section id, and how many instances are registered. Good first call to
    confirm the plugin is running and playback state before drilling into
    the more detailed tools below."""
    return _call_bridge("getStatus")


@server.tool()
def get_instances() -> dict:
    """Every registered MPL instance: id, MIDI channel, role
    (anchor/motif/counterpoint/unrestricted), enabled state, plus the
    overall coherence score (how much instances currently agree with each
    other - see policy/CoherenceEvaluator)."""
    return _call_bridge("getInstances")


@server.tool()
def get_awareness() -> dict:
    """Each instance's actual cached pattern content, as last confirmed via
    the IPC awareness channel (Resync Now / automatic resync) - real
    ground truth from MPL, not what Composer Mastermind last intended to
    send. Same data ui/PatternAwarenessView shows: per-instance pattern
    index, capture bar, and every enabled step's note/velocity/duration."""
    return _call_bridge("getAwareness")


@server.tool()
def get_blueprint_status() -> dict:
    """The active blueprint (if any): its id/name, which section currently
    covers the playhead, the current bar, and every section's id/scene/bar
    range/archetype. The archetype field is what the motif/rule engine
    keys off (presentation/build/peak/release, or empty)."""
    return _call_bridge("getBlueprintStatus")


@server.tool()
def get_motif_presets() -> dict:
    """Every saved MotifPreset (id, tags, and its relative pitch/rhythm
    cell - semitoneOffset/relativeDuration/relativeVelocity per note),
    plus the current global Nudge/Phrase application mode. A preset only
    actually gets used by a section if its id or a tag matches that
    section's archetype - cross-reference against get_blueprint_status's
    section archetypes to check real coverage."""
    return _call_bridge("getMotifPresets")


@server.tool()
def list_blueprints() -> dict:
    """Every saved blueprint in the library: id, name, section count. Use
    this to find a valid blueprint_id before calling commit_blueprint -
    don't guess an id."""
    return _call_bridge("listBlueprints")


@server.tool()
def list_scenes() -> dict:
    """Every saved scene in the library: id, name, target-instance count.
    Use this to find a valid scene_id before calling set_scene."""
    return _call_bridge("listScenes")


@server.tool()
def resync_instance(instance_id: str, pattern_index: int = 0) -> dict:
    """Requests a real pattern-content resync for one instance/pattern over
    the IPC awareness channel (same as clicking Resync Now in the
    Awareness tab). Fire-and-forget - the response lands asynchronously in
    the cache, so call get_awareness a moment after this to see the
    result. pattern_index is 0-indexed (0 = MPL's Pattern 1)."""
    return _call_bridge("resyncInstance", instanceId=instance_id, patternIndex=pattern_index)


@server.tool()
def write_pattern(instance_id: str, pattern_index: int, steps: list) -> dict:
    """Writes a full 16-step pattern directly into one instance's real
    storage over IPC - the same write Setup mode's piano roll uses when you
    click Commit there (dragging notes by hand produces exactly this shape
    of call). Bypasses MIDI/CC entirely; instant, no parameter polling.
    steps is a list of up to 16 {"enabled": bool, "note": int (0-127),
    "velocity": int (0-127), "duration": int (0-16, in grid steps)} objects,
    index = step position; any positions past the end of the list, or past
    16, stay disabled. Requires the instance to already have a live IPC
    connection (it must be open in MPL) - fails with a clear error
    otherwise, never writes blind. Call get_awareness a moment after this
    to confirm the write actually landed."""
    return _call_bridge("writePattern", instanceId=instance_id, patternIndex=pattern_index, steps=steps)


@server.tool()
def set_motif_application_mode(mode: str) -> dict:
    """Sets the global motif engine write mode: "Nudge" (adjusts existing
    enabled steps relative to their current values) or "Phrase" (writes
    the full motif cell as a contiguous replacement run). Session-level,
    not per-preset or per-section."""
    return _call_bridge("setMotifApplicationMode", mode=mode)


@server.tool()
def set_pitch_field_broadcast(
    enabled: bool | None = None,
    base_cc: int | None = None,
    channel: int | None = None,
    min_bars: int | None = None,
) -> dict:
    """Controls the pitch-field broadcast: every bar, Composer Mastermind emits
    the union of pitch classes its structural voices are currently sounding
    (each registered instance's active-pattern content shifted by its tracked
    transpose) as a 12-bit mask packed into TWO CCs: base_cc (default 110)
    carries pitch classes 0-6 (low 7 bits), base_cc+1 carries 7-11 (high 5
    bits), on the given MIDI channel (default 1). A one-CC-per-pitch-class block
    would run into CC120/CC121 (All Sound Off / Reset All Controllers) and
    hard-mute downstream synths. Only sends when the
    12-bit mask changes, and no more often than min_bars bars apart (default 0 =
    every changing bar; raise it so per-bar transpose nudges don't make the
    wash's field flicker). An OrchNoteFilter with "CC# Mask Base" set to the
    same base (it reads the same two CCs) then constrains its wash to the composition's own harmony instead
    of a hand-authored scale. Session-level, not persisted. Pass only the fields
    you want to change; also settable in the plugin's Modulators tab. Returns
    the current settings plus the last mask (a 12-bit int, bit i = pitch class
    i)."""
    params = {}
    if enabled is not None:
        params["enabled"] = enabled
    if base_cc is not None:
        params["baseCc"] = base_cc
    if channel is not None:
        params["channel"] = channel
    if min_bars is not None:
        params["minBars"] = min_bars
    return _call_bridge("setPitchFieldBroadcast", **params)


@server.tool()
def send_test_cc(instance_id: str, cc: int, value: int) -> dict:
    """Fires one raw CC message immediately to one instance, bypassing
    Mutation/Router/PolicyEngine entirely - not a musical action, a
    diagnostic (same as the Mutations tab's "Send Test CC"). cc and value
    are both 0-127. See CCMapping.h's kNamedCCs for the protocol: 20/21/22/24
    are global (Active Pattern/Target Pattern/Grid Mode/Swing), 30-53 are
    per-pattern mutations (P1/P2/P3 Transpose/Rotation/Length/Inversion,
    base+0/1/2/3), 60-64 are Target Step editing."""
    return _call_bridge("sendTestCC", instanceId=instance_id, cc=cc, value=value)


@server.tool()
def send_mutation(instance_id: str, pattern_index: int, mutation_type: str, amount: int) -> dict:
    """Sends a real, policy-gated mutation (same as the Mutations tab's
    "Send Mutation") - goes through Router/PolicyEngine, so it can be
    legitimately blocked by that instance role's per-bar budget; check the
    "sent" field in the response, don't assume success. mutation_type is
    one of "transpose"/"rotation"/"length"/"inversion"/"retrograde"/"m7"/
    "rate". amount is a DELTA from the instance's current tracked value for
    transpose/rotation/length (e.g. rotation amount=-2 means "2 steps back
    from wherever it currently is", wrapping cyclically) - NOT an absolute
    value. For "inversion"/"retrograde"/"m7", amount != 0 is an absolute
    on/off toggle instead (a boolean has no sensible delta). For "rate"
    (Augmented/Normal/Diminished - a real augmentation/diminution device,
    global per-instance not per-pattern, pattern_index is ignored), amount
    is clamped to [-1, 1] and is an ABSOLUTE target state, not a delta:
    -1=Augmented, 0=Normal, +1=Diminished - e.g. to flag a cadential close,
    send amount=-1 a bar or two before a section boundary. Call
    get_instances first if you need to confirm pattern_index (0-2, matching
    MPL's P1-P3) is the pattern you intend."""
    return _call_bridge(
        "sendMutation",
        instanceId=instance_id,
        patternIndex=pattern_index,
        type=mutation_type,
        amount=amount,
    )


@server.tool()
def commit_blueprint(blueprint_id: str) -> dict:
    """Makes an already-saved blueprint (from list_blueprints) the active
    one - real playback effect, same as clicking Commit in the
    Blueprint/Generate tab. This REPLACES whatever blueprint is currently
    driving playback and resets the tracked active section, so the very
    next bar re-evaluates from scratch against the new blueprint's
    sections."""
    return _call_bridge("commitBlueprint", blueprintId=blueprint_id)


@server.tool()
def set_scene(scene_id: str) -> dict:
    """Makes an already-saved scene (from list_scenes) the current scene
    and routes it immediately - real playback effect, same as
    SceneListComponent's "Send Scene Now". If a blueprint is also active,
    its own section-driven scene routing will override this the next time
    the active section changes."""
    return _call_bridge("setScene", sceneId=scene_id)


@server.tool()
def create_scene(scene: dict) -> dict:
    """Authors (or replaces, if the id already exists) a Scene in the
    library, from a full JSON payload - this is composing with numbers,
    not clicking through the Scenes tab. Shape:
    {
      "id": str, "name": str, "durationBars": int, "quantize": "bar",
      "targets": [instanceId, ...],
      "global": {"activePattern": int (0=stop,1-3=P1-P3), "gridMode": int (0=binary,1=ternary), "swing": float (0-75), "rate": int (0=Augmented,1=Normal,2=Diminished)},
      "instanceOverrides": [{"targetInstance": str, "activePattern": int, "gridMode": int, "swing": float, "rate": int (-1=inherit)}, ...],
      "patterns": [{"targetInstance": str, "patternIndex": int (0-2), "transpose": int, "rotation": int, "length": int (1-16), "inversion": bool}, ...],
      "mutations": [], "nextSceneId": str, "transitionStyle": "hard", "rampBars": int
    }
    instanceOverrides/patterns fields use -1 (or -1.0) to mean "inherit
    global" where applicable. Omit any array you don't need - defaults to
    empty. Does not activate the scene - call set_scene or reference it
    from a blueprint section's sceneId afterward."""
    return _call_bridge("createScene", scene=scene)


@server.tool()
def create_blueprint(blueprint: dict) -> dict:
    """Authors (or replaces) a Blueprint in the library - the whole piece's
    form, as numbers. Shape:
    {
      "id": str, "name": str,
      "sections": [
        {
          "id": str, "name": str, "sceneId": str (must reference a real scene),
          "startBar": int, "durationBars": int,
          "archetype": "presentation" | "build" | "peak" | "release" | "",
          "layerRoles": [{"targetInstance": str, "layerRole": "foreground"|"support"|"background"}, ...],
          "budgetOverrides": [{"role": str, "maxMinorPerBar": int, "maxMediumPerBar": int, "maxMajorPerBar": int}, ...],
          "reservedValues": [{"targetInstance": str, "patternIndex": int, "type": "transpose"|"rotation"|"length"|"inversion", "value": int}, ...],
          "modulatorValues": [{"modulatorTargetId": str, "value": int}, ...]
        }, ...
      ]
    }
    Sections should be authored in the order they play, non-overlapping
    bar ranges (advanceBlueprintIfNeeded matches by list order, first
    match wins). Does not activate the blueprint - call commit_blueprint
    afterward."""
    return _call_bridge("createBlueprint", blueprint=blueprint)


@server.tool()
def create_motif_preset(preset: dict) -> dict:
    """Authors (or replaces) a MotifPreset - a short relative pitch/rhythm
    cell the motif engine applies during build/peak/release passes. Shape:
    {
      "id": str, "name": str, "tags": [archetype-name, ...],
      "notes": [{"semitoneOffset": int, "relativeDuration": float, "relativeVelocity": float}, ...]
    }
    A preset is only actually used by a section if the section's archetype
    matches one of this preset's tags (or equals its id, as a fallback) -
    tag it explicitly rather than relying on the id match. notes order
    matters (it's a melodic cell, not a set) and offsets are relative to
    the cell's own first note, not absolute pitches."""
    return _call_bridge("createMotifPreset", preset=preset)


@server.tool()
def set_arc(dimension: str, breakpoints: list) -> dict:
    """Authors one ArcSet dimension's full curve, replacing whatever shape
    it had. dimension is one of "energy"/"tension"/"density"/"complexity"/
    "coherence". breakpoints is a list of {"bar": int, "value": float
    (0-1)} points, sorted by bar - the curve linearly interpolates between
    them, holds flat before the first and after the last. This is not
    scoped to one blueprint; it's the live ArcSet, so set it before or
    right after commit_blueprint, not per-section."""
    return _call_bridge("setArc", dimension=dimension, breakpoints=breakpoints)


@server.tool()
def create_role_preset(preset: dict) -> dict:
    """Authors (or replaces) a RolePreset - a default behavior for a
    musical function (anchor/motif/counterpoint/...), applied to every
    currently-registered instance holding that role. Shape:
    {
      "id": str, "name": str, "targetRole": str, "tags": [str, ...],
      "activePattern": int (-1 = inherit), "gridMode": int (-1 = inherit, 0=binary, 1=ternary),
      "swing": float (-1.0 = inherit, 0-75), "rate": int (-1 = inherit, 0=Augmented,1=Normal,2=Diminished)
    }
    Doesn't apply itself to anything - call apply_role_preset against a
    saved scene afterward."""
    return _call_bridge("createRolePreset", preset=preset)


@server.tool()
def create_rhythmic_relationship_preset(preset: dict) -> dict:
    """Authors (or replaces) a RhythmicRelationshipPreset - a joint,
    deliberately-correlated choice across 2+ roles applied together in one
    call (e.g. role "motif" Binary while role "counterpoint" Ternary,
    chosen together on purpose for a specific groove). Shape:
    {
      "id": str, "name": str, "tags": [str, ...],
      "roleSlots": [{"targetRole": str, "activePattern": int, "gridMode": int, "swing": float, "rate": int (-1=inherit)}, ...]
    }
    (same -1/-1.0 inherit sentinel as create_role_preset). Needs at least 2
    role slots. Doesn't apply itself - call
    apply_rhythmic_relationship_preset against a saved scene afterward."""
    return _call_bridge("createRhythmicRelationshipPreset", preset=preset)


@server.tool()
def create_arc_preset(preset: dict) -> dict:
    """Authors (or replaces) an ArcPreset - a reusable *normalized* shape
    ("swell"/"plateau"/"arch") stampable onto any bar range, decoupled from
    which ArcSet dimension it targets (chosen at apply-time). Shape:
    {
      "id": str, "name": str, "tags": [str, ...],
      "breakpoints": [{"position": float (0-1), "value": float (0-1)}, ...]
    }
    Doesn't apply itself - call apply_arc_preset afterward."""
    return _call_bridge("createArcPreset", preset=preset)


@server.tool()
def get_presets() -> dict:
    """Every saved RolePreset/RhythmicRelationshipPreset/ArcPreset (id,
    name, and category-specific summary fields - not the full body, use
    this to find a valid preset_id before calling one of the apply_*
    tools). Motif presets aren't included here - see get_motif_presets."""
    return _call_bridge("getPresets")


@server.tool()
def apply_role_preset(preset_id: str, scene_id: str) -> dict:
    """Applies a saved RolePreset onto a saved Scene, in place - merges the
    preset's activePattern/gridMode/swing into every currently-registered
    instance holding that preset's targetRole, then saves the scene back to
    the library (same effect as the Presets tab's "apply to scene" button).
    A one-shot authoring-time stamp, not a live reference - re-apply if the
    preset or the scene's instances change later."""
    return _call_bridge("applyRolePreset", presetId=preset_id, sceneId=scene_id)


@server.tool()
def apply_rhythmic_relationship_preset(preset_id: str, scene_id: str) -> dict:
    """Applies a saved RhythmicRelationshipPreset onto a saved Scene, in
    place - resolves every role slot together in one call, then saves the
    scene back to the library. Same one-shot-stamp semantics as
    apply_role_preset."""
    return _call_bridge("applyRhythmicRelationshipPreset", presetId=preset_id, sceneId=scene_id)


@server.tool()
def apply_arc_preset(preset_id: str, target_arc_name: str, start_bar: int, end_bar: int) -> dict:
    """Stamps a saved ArcPreset's normalized shape onto the LIVE ArcSet's
    target_arc_name curve within [start_bar, end_bar] - any existing
    breakpoints strictly inside that range are replaced. target_arc_name is
    one of energy/tension/density/complexity/coherence. This is the same
    live ArcSet set_arc writes to, not a blueprint-scoped copy."""
    return _call_bridge(
        "applyArcPreset", presetId=preset_id, targetArcName=target_arc_name, startBar=start_bar, endBar=end_bar
    )


@server.tool()
def create_modulator_target(target: dict) -> dict:
    """Registers (or replaces) a ModulatorTarget - a CC number/channel
    Composer Mastermind will drive on its own, meant to be paired to a
    Bitwig modulator's parameter. Shape:
    {
      "id": str, "ccNumber": int (default 70, deliberately outside MPL's 20-64 range),
      "midiChannel": int, "mode": "arc" | "section", "arcDimension": str (required if mode="arc")
    }
    "arc" mode continuously sends that ArcSet dimension's current value
    every bar; "section" mode sends a fixed value once per section boundary,
    authored per-section via a blueprint section's modulatorValues. This
    only registers Composer Mastermind's own side - actually pairing the
    CC/channel to a real Bitwig modulator parameter still needs a one-time
    manual "Learn CC" gesture in Bitwig itself (Learn CC, not Bitwig's
    generic "Map to Controller or Key", which ignores plugin-generated CC)."""
    return _call_bridge("createModulatorTarget", target=target)


@server.tool()
def get_modulator_targets() -> dict:
    """Every registered ModulatorTarget: id, ccNumber, midiChannel, mode,
    arcDimension. Doesn't reveal anything about whether it's actually
    paired to a Bitwig modulator - that pairing lives entirely inside the
    Bitwig project, invisible to Composer Mastermind."""
    return _call_bridge("getModulatorTargets")


@server.tool()
def create_modulation_route(route: dict) -> dict:
    """Registers (or replaces) a ModulationRoute - the modulation matrix
    (2026-08-27): wires an ArcSet dimension directly to a real MPL
    parameter on a specific instance/pattern, or broadcast to every
    registered instance. Distinct from create_modulator_target: this drives
    MPL's own parameters through the same CC encoding a Mutation uses, no
    Bitwig pairing needed at all. Shape:
    {
      "id": str, "arcDimension": str (energy/tension/density/complexity/coherence),
      "targetInstance": str (an instance id, or "*" for every registered instance),
      "patternIndex": int (0-2; ignored for swing/rate/activePattern/gridMode),
      "parameter": "transpose" | "rotation" | "length" | "swing" | "rate" |
                   "inversion" | "retrograde" | "m7" | "activePattern" | "gridMode",
      "outputMin": float, "outputMax": float,
      "threshold": float (0-1, default 0.5),
      "invert": bool, "enabled": bool,
      "dispatchMode": "bar" | "sequence" | "barCycle" (default "bar"),
      "sequenceValues": [float, ...]  (required if dispatchMode="sequence" or "barCycle"),
      "phraseLengthBars": int (default 4, used only if dispatchMode="barCycle")
    }
    transpose/rotation/length/swing/rate are continuous - by default re-sent
    every bar ("bar" dispatch mode), the arc's 0-1 sample linearly mapped into
    [outputMin, outputMax] (leave both at 0 to use that parameter's own full
    natural range, e.g. -48..48 for transpose; rate's natural range is 0-2,
    banded to the nearest of Augmented/Normal/Diminished). inversion/
    retrograde/m7/gridMode are threshold-crossing booleans (fire only when
    the sample crosses threshold); activePattern instead bands the sample
    into 4 states (0=stop, 1-3=pattern) - threshold/outputMin/outputMax are
    unused for it.

    "sequence" dispatch mode (2026-08-27, "fragment sequencer" - only valid
    for the 5 continuous parameters, ignores arcDimension/outputMin/
    outputMax/threshold entirely): steps through sequenceValues[] once per
    PATTERN LOOP CYCLE instead of once per bar - real melodic sequencing,
    the device a Bach invention/fugue uses constantly (a short fragment
    restated at a new pitch level every repetition). Shrink the target
    pattern's Length first (see send_mutation/create_scene's patterns[]) so
    a "loop cycle" is short enough to actually cycle through the sequence at
    a musical rate - a 16-step Length gives one sequence step every 16
    16th-notes, a Length of 3-4 gives one every bar-fraction, much closer to
    how a real sequence sounds. Note the loop cycle can never exceed one bar
    (MPL's own pattern Length caps at 16 steps) - for anything longer, use
    "barCycle" instead.

    "barCycle" dispatch mode (2026-08-27, phrase-cadence breathing - same
    parameter/sequenceValues restrictions as "sequence", also ignores
    arcDimension/outputMin/outputMax/threshold): steps through
    sequenceValues[] once every phraseLengthBars BARS instead of pattern
    steps - genuine multi-bar phrasing, decoupled from the one-bar ceiling
    above. A short phraseLengthBars (2-4) gives phrase-internal breathing
    within a section; set it equal to a whole section's durationBars for a
    once-per-section cadence instead (e.g. rate dipping to Augmented near a
    section's end, no live-fired mutation needed). Give different instances
    different phraseLengthBars on purpose - that's what keeps an ensemble's
    breathing from landing in mechanical lockstep every phrase, the same way
    real independent voices don't all cadence in exact unison.

    If a route targets the exact same instance/pattern/parameter a built-in
    curve already drives (Energy/Tension -> transpose, Density -> swing),
    the route wins for that instance and the built-in curve skips it there;
    every other instance keeps following the built-in curve untouched -
    true for both dispatch modes."""
    return _call_bridge("createModulationRoute", route=route)


@server.tool()
def get_modulation_routes() -> dict:
    """Every registered ModulationRoute: id, arcDimension, targetInstance,
    patternIndex, parameter, outputMin/outputMax, threshold, invert,
    enabled, dispatchMode, sequenceValues. Use this to confirm what's
    actually wired before assuming a route is (or isn't) affecting a given
    instance's sound."""
    return _call_bridge("getModulationRoutes")


@server.tool()
def generate_blueprint(blueprint_id: str, driving_arc_name: str, base_scene_id: str, commit: bool = False) -> dict:
    """The Generate tab's auto-assembly, as a tool: one arc's consecutive
    breakpoints become section boundaries, each section is classified into
    presentation/build/peak/release purely from its breakpoint values, and
    role/rhythmic-relationship presets tagged with that archetype get
    applied automatically. driving_arc_name must already have >=2
    breakpoints (call set_arc first). With commit=False (default), this is
    pure preview - nothing is saved, same as the UI's Generate button
    before you click Commit; the response's "sections"/"newSceneCount"
    tells you what *would* be created. With commit=True, it also saves the
    generated scenes and blueprint to their libraries and makes the
    blueprint active (same as clicking Commit) - real playback effect."""
    return _call_bridge(
        "generateBlueprint",
        blueprintId=blueprint_id,
        drivingArcName=driving_arc_name,
        baseSceneId=base_scene_id,
        commit=commit,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--http",
        action="store_true",
        help="serve streamable-http on 127.0.0.1:8765/mcp instead of stdio (for tunneling, e.g. via cloudflared)",
    )
    args = parser.parse_args()

    if args.http:
        # The MCP SDK's DNS-rebinding protection rejects any request whose
        # Host header isn't in an allowlist, which by default means only
        # 127.0.0.1/localhost - exactly what a cloudflared quick tunnel's
        # random *.trycloudflare.com hostname is not. allowed_hosts only
        # supports exact matches or a ":*" port-wildcard suffix (confirmed
        # by reading transport_security.py directly - there's no true "*"
        # glob), and a free quick tunnel gets a new random hostname every
        # restart anyway (same as WigAI's setup), so allowlisting one
        # hostname isn't practical. Disabled outright instead - acceptable
        # for a local single-user dev tool; see README.md's "no auth"
        # caveat, which this makes explicit rather than silently true.
        server.run(
            transport="streamable-http",
            host="127.0.0.1",
            port=8765,
            transport_security=TransportSecuritySettings(enable_dns_rebinding_protection=False),
        )
    else:
        server.run(transport="stdio")
