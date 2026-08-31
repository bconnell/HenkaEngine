# Dedicated Server

> **Status:** Available foundation on the validated Windows path

`henka_dedicated_server` is Henka's renderer-free C17 host for developer- or operator-controlled infrastructure. It links `henka_runtime` and the bounded ENet transport.

## Contents

- [Runtime boundary](#runtime-boundary)
- [Build](#build)
- [Run](#run)
- [Terrain storage](#terrain-storage)
- [Smoke and integration validation](#smoke-and-integration-validation)
- [Packaging](#packaging)
- [Session and recovery behavior](#session-and-recovery-behavior)
- [Bounded-duration runs](#bounded-duration-runs)
- [Current limitations](#current-limitations)

## Runtime boundary

The dedicated server omits graphical-client systems.

| Included | Excluded |
| --- | --- |
| C17 shared runtime | SDL graphical client initialization |
| ENet transport | OpenGL rendering |
| Fixed-tick physics | Display/window ownership |
| Terrain authority and persistence | UI |
| Bounded configuration | KTX transcoding |
| Graceful client shutdown | Graphical assets |

## Build

The normal Windows build produces:

```text
build/examples/dedicated_server/Debug/henka_dedicated_server.exe
```

A server-only configuration is also supported without a C++ compiler or graphical client providers:

```powershell
cmake -S . -B out/server-only `
  -DHENKA_BUILD_CLIENT=OFF `
  -DHENKA_BUILD_DEDICATED_SERVER=ON `
  -DHENKA_BUILD_EXAMPLES=OFF `
  -DHENKA_ENABLE_KTX2_TRANSCODER=OFF
cmake --build out/server-only --config Debug --target henka_dedicated_server
```

## Run

```powershell
.\build\examples\dedicated_server\Debug\henka_dedicated_server.exe `
  --bind 0.0.0.0 --port 7777 --max-clients 32 `
  --tick-rate 60 --save-root save --config server.conf
```

Command-line values override matching values from the optional `key=value` configuration file.

Terrain storage confines paths. Normal deployments should keep storage paths relative to the server working directory or package directory.

## Terrain storage

The shipped `server.conf.example` leaves the optional `world` setting unset. The packaged smoke command therefore needs only its operator-owned `save/` directory.

Supply `--world PATH` or add `world=PATH` to a copied configuration when deploying a validated read-only base Terrain world.

### Base world

When `--world` is configured:

1. the path is treated as a read-only Terrain v1 storage root;
2. startup validates `terrain.manifest`;
3. startup runs recovery on that root;
4. `region_0_0.htr` must be valid before the server binds;
5. the loaded base region is copied into the runtime world.

### Save root

Accepted edits are written to `--save-root`. Its manifest is created or validated transactionally.

Use separate directories for the base root and save root.

## Smoke and integration validation

### Server smoke mode

The bounded `--smoke` mode exercises:

- shared runtime initialization;
- committed Terrain snapshot recovery;
- local ENet binding;
- one in-process client connection;
- ping round trip;
- one deterministic Terrain edit when the save root is empty;
- clean shutdown.

A later smoke run reloads the committed region and reports the same revision.

### Multi-process integration

Run the bounded multi-process authority check with:

```powershell
.\scripts\test_terrain_process_integration_windows.ps1
```

The test launches the dedicated server and two independent runtime-only clients. It verifies:

- one accepted Terrain edit;
- one stale edit rejection;
- late-observer bootstrap;
- resident-region checksum convergence;
- client reconnect after an accepted edit;
- server restart against the same save root;
- exact committed revision and checksum recovery.

### Finite repeated soak

```powershell
.\scripts\soak_terrain_process_integration_windows.ps1 -Iterations 3
```

Each iteration uses fresh server/client processes and an isolated save root. The soak covers repeatability, cleanup, late join, reconnect, and restart recovery for the bounded resident-region contract.

## Packaging

Create the headless deployment package with:

```powershell
.\scripts\package_dedicated_server_windows.ps1 -Configuration Release
```

The package is written to:

```text
out/HenkaDedicatedServer
```

It contains:

- the dedicated executable;
- sample configuration;
- server documentation;
- provenance marker;
- operator-owned `save/` directory.

Run the package validation afterward:

```powershell
.\scripts\check_packaged_dedicated_server_windows.ps1
```

The package can run on a development PC, physical server, or VPS controlled by the operator. Henka does not provide hosted server infrastructure.

Back up the package's `save/` directory according to the deployment policy. Accepted Terrain edits are committed transactionally before acknowledgement.

## Session and recovery behavior

The package check verifies startup, local bind, loopback connection, clean shutdown, and two consecutive save-root runs with revision recovery.

### Connect-time Terrain session info

The server sends a bounded Terrain session-info message when a client connects. The message identifies the world/base and advertises up to 16 resident regions in deterministic row-major coordinate order for snapshot bootstrap.

### Client recovery coverage

The runtime client recovery test covers:

- forced disconnect;
- explicit reconnect;
- replacement of the authoritative server wrapper on the same endpoint;
- a late observer;
- exact resident-sample checksum convergence.

### Terrain delta history

The server retains a fixed 64-entry authoritative Terrain delta history per process.

A connected client may request a bounded regional revision range after detecting a gap. The server sends the complete retained range when available. A transactional regional snapshot is sent when the requested history is unavailable.

This history exists in memory. Durable Terrain recovery remains owned by the Terrain journal and persisted region state.

## Bounded-duration runs

For deterministic local integration checks, `--run-for-ms COUNT` runs the normal server loop for a bounded duration and then follows the normal peer-disconnect and resource-flush shutdown path.

Without this option, the server runs indefinitely until stopped.

## Current limitations

- Application authentication is outside the current server foundation.
- Relevance-driven multi-region orchestration remains future work.
- Relevance-driven reconnect and late-join selection remain future work.
- The current process soak is bounded and does not establish production-scale multiplayer capacity.
- The 64-entry delta history is process-local recovery state.
- Broad multiplayer simulation and large-scale capacity testing remain future work.
