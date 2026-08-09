# Dedicated server

`henka_dedicated_server` is a renderer-free C17 host for infrastructure that
the developer or game operator controls. It links `henka_runtime` and the
bounded ENet transport; it does not initialize SDL, OpenGL, a display, UI,
KTX transcoding, or the graphical client.

## Build

The normal Windows build produces:

```text
build/examples/dedicated_server/Debug/henka_dedicated_server.exe
```

The server-only configuration remains valid without a C++ compiler or client
providers:

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

Command-line values override the optional `key=value` configuration file.
Paths are confined by the Terrain storage layer and should normally be kept
relative to the server working directory or package directory.

When `--world` is supplied, it is a read-only base Terrain storage root using
the Terrain v1 manifest and region format. Startup validates
`terrain.manifest`, recovers that root, and requires a valid `region_0_0.htr`
before binding the server. The loaded base region is copied into the runtime
world; accepted edits are written to `--save-root`, whose manifest is created
or validated transactionally. The base root and save root should be separate
directories.

The bounded `--smoke` mode initializes the shared runtime, recovers committed
Terrain snapshots, binds a local ENet endpoint, connects an in-process client,
round-trips a ping, commits one deterministic Terrain edit when the save root
is empty, and exits cleanly. A later smoke run loads the committed region and
reports the same revision. It is a deployment check, not a substitute for a
two-process multiplayer soak.

## Package

Create the headless deployment package with:

```powershell
.\scripts\package_dedicated_server_windows.ps1 -Configuration Release
```

The package is written to `out/HenkaDedicatedServer` and contains only the
dedicated executable, the sample configuration, server documentation, the
provenance marker, and the operator-owned `save/` directory. Run the package
check after packaging:

```powershell
.\scripts\check_packaged_dedicated_server_windows.ps1
```

The package is intended for a development PC, physical server, or VPS under
the operator's control. It does not provide a hosted Henka service. Keep the
package's `save/` directory backed up according to the operator's deployment
policy; accepted Terrain edits are committed transactionally before the
server acknowledges them.

The package check proves startup, local bind, loopback connection, clean
shutdown, and two consecutive save-root runs with revision recovery. It does
not claim reconnect, late-join orchestration, or production multiplayer soak
coverage.
