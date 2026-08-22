# Composer Mastermind MCP Bridge

Read-only MCP access to Composer Mastermind's own live internal state, so Claude can inspect
Instances/roles, Awareness (cached pattern content), the active blueprint/section, and motif
presets directly — instead of relying on screenshots of the plugin's own tabs.

## How it fits together

Two halves, matching the same "transport vs. logic" split used everywhere else in this project
(e.g. `PatternSyncServer`):

1. **`src/composer/McpBridgeServer.h`/`.cpp`** (inside the plugin itself) — a small local JSON
   request/response socket on `127.0.0.1:47824`. Pure transport; `ComposerCore::handleMcpBridgeRequest`
   owns the actual action table. Does **not** speak MCP.
2. **`mcp-bridge/server.py`** (this folder) — the piece that actually speaks MCP. One tool per
   bridge action, each just opening a fresh socket connection, asking, and returning whatever the
   plugin's live state says. No caching.

The plugin must be running (Standalone, or hosted in Bitwig) for any tool call to succeed — if it
isn't, tools fail with a clear connection error, not stale or fabricated data.

Read-only by design, first slice. Write actions (Resync Now, commit a blueprint, send a test CC,
change motif application mode) are a deliberate second slice, not built yet.

## Setup

```bash
pip install -r requirements.txt
```

## Running standalone (for testing, outside any MCP client)

```bash
python server.py
```

This starts the stdio MCP server and blocks, waiting for a client on stdin/stdout — useful for
local testing with a scripted MCP client, but **not** how this gets registered day to day (see
below) — this desktop app's connector UI doesn't support local stdio servers.

## Registering as a connector

Confirmed empirically (2026-08-20): this environment's Claude app is not the open-source Claude
Code CLI — there's no `claude` binary on `PATH` and `claude_desktop_config.json` has no
`mcpServers` key. Its connector UI only accepts **remote HTTPS** connectors, same limitation
already documented for WigAI. So this needs the same tunnel treatment:

1. Start the plugin (Standalone or hosted in Bitwig) so there's real state to query.
2. Start the bridge in HTTP mode:
   ```bash
   python server.py --http
   ```
   Serves `http://127.0.0.1:8765/mcp`.
3. Tunnel it:
   ```bash
   cloudflared tunnel --url http://127.0.0.1:8765
   ```
   Prints a random `https://<words>.trycloudflare.com` URL — **ephemeral, changes every time
   cloudflared restarts**, same as WigAI's tunnel.
4. In the Claude app, add a custom connector pointing at `https://<that-url>/mcp` (don't forget
   the `/mcp` path). Name it `composer-mastermind` or similar.

Both `server.py --http` and `cloudflared` need to keep running in the background for the connector
to work - they aren't spawned automatically the way a local stdio server would be.

Verified end-to-end 2026-08-20 with a scripted MCP client through a real tunnel URL: `list_tools`
returned all 5 tools, `get_status` returned real data through the full chain (MCP client → tunnel
→ local HTTP server → local socket → plugin).

## Tools exposed

| Tool | Returns |
|---|---|
| `get_status` | Current bar, coherence, active section id, registered instance count |
| `get_instances` | Every instance's id/channel/role/enabled + coherence |
| `get_awareness` | Each instance's real cached pattern content (Awareness tab data) |
| `get_blueprint_status` | Active blueprint id/name, current bar, active section, every section's bar range + archetype |
| `get_motif_presets` | Every MotifPreset's id/tags/notes + current Nudge/Phrase mode |

## Known limitations (first slice, not oversights)

- Read-only — no way to trigger a resync, commit a blueprint, or send a test CC yet.
- One fixed port (47824) — if two Composer Mastermind instances ever ran simultaneously (e.g. a
  Standalone test alongside a Bitwig-hosted instance), only the one that bound the port first
  would be reachable.
- No auth — anything that can reach `127.0.0.1:47824` can read this state. In `--http` mode, DNS-
  rebinding protection is disabled outright (a cloudflared quick tunnel's random hostname can't be
  allowlisted in advance, and the SDK's `allowed_hosts` has no true wildcard, only exact-match or a
  `:*` port suffix). Fine for a local single-user dev tool exposed through an ephemeral tunnel URL
  only you know; would need real thought before this goes anywhere more exposed than that.
