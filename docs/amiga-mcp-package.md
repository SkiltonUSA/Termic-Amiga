# Amiga-MCP multi-client Mac package

The separate `Amiga-MCP-MultiClient-macOS-arm64.zip` download contains the shared
multi-client gateway from [SkiltonUSA/Amiga-MCP](https://github.com/SkiltonUSA/Amiga-MCP),
not the older direct DevBench server. It uses upstream version 0.1.0 Alpha,
commit `994292aa2043edb77ff1079bf09de3519fd40225`, unchanged source and MIT license.
Termic's 0.2.1 Beta version does not rename the gateway's upstream version.

The gateway supports up to 64 HTTP MCP sessions and 32 queued requests by
default. Complete hardware transactions are serialized through one persistent
backend connection, with exclusive reservations for multi-step tests. This
provides multiple client connections, not parallel execution on the Amiga CPU.

## Install and start

1. Extract the ZIP and keep its `Amiga-MCP` folder intact.
2. Have the existing Amiga-side bridge and DevBench backend connected and
   running at `http://127.0.0.1:55010/mcp`. They are prerequisites, not included.
3. Open `Start-Amiga-MCP.command`. No separate Python installation is needed.
   If a gateway already serves this backend, use that running instance.
4. Point Termic AI Proxy and all agents to `http://127.0.0.1:55110/mcp`.
5. Startup is paused. Once direct clients have finished and the target is idle,
   call `amiga_bridge_enable(confirmed_idle=true)` through an MCP client.
   Never bypass a pause caused by an uncertain response without inspecting the
   target. See the included upstream README for reserve/resume/release controls.

The package targets Apple Silicon macOS and has local executable signatures,
not Developer ID notarization. macOS may require approval in Privacy & Security.
The package includes the upstream source, pinned requirements, license and a
source-hash provenance file. No user's reservation, backend address overrides,
credentials or logs are bundled. The default backend/gateway are loopback URLs.

## Build and verification

Export the pinned upstream Git revision to a clean source directory. Create an
isolated Python 3.12 environment, install its `requirements.lock` and
`pyinstaller==6.22.3`, then run `scripts/bridge/build.py --source SOURCE
--revision FULL_COMMIT --output NEW_DIRECTORY`.
Run upstream tests with `PYTHONPATH=SOURCE/src python -m unittest discover -s
SOURCE/tests -v`. Use `scripts/bridge/accept_bundle.py --source SOURCE --binary
PATH/Amiga-MCP` to test the packaged executable through real HTTP MCP clients.

The upstream suite exercises queue bounds, cancellation, timeouts, restart
recovery and multi-session routing. The packaged check uses five clients and
24 concurrent calls against a synthetic backend, checks reply isolation and
one backend transaction at a time, and tests pause/reserve/resume/release.
It never connects to the user's real gateway or Amiga. This is not a new
physical-Amiga installation test or a throughput benchmark.
