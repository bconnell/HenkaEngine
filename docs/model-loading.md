# Model Loading

Henka Engine currently supports a small, early OBJ loading path aimed at proving content-driven rendering in the sandbox.

## What works now

- OBJ files loaded from local assets
- comments and blank lines
- Windows and Unix line endings
- extra whitespace around tokens
- vertex positions
- texture coordinates when present
- normals when present
- computed face normals when normals are missing
- triangle faces
- quad and n-gon fan triangulation
- positive and negative position, texture-coordinate, and normal OBJ indices
- degenerate face rejection before mesh emission
- ignored non-render statements for `o`, `g`, `s`, `mtllib`, and `usemtl`
- cached OBJ mesh loading through the asset manager
- explicit retry of failed cached OBJ mesh fallbacks after a source asset is fixed

## What does not work yet

- MTL material import
- concave polygon correction beyond basic fan triangulation
- model hierarchies
- skeletal animation
- glTF
- editor import workflows
- full live replacement of already-loaded mesh assets that scenes may still reference

## Failure behavior

If an OBJ file is missing or cannot be parsed:

- the engine logs a clear error
- the sandbox keeps running
- the asset manager returns a fallback mesh so the scene stays visible
- repeated normal loads of the same path reuse the cached result instead of rebuilding ownership each time
- failed cached OBJ mesh fallbacks can be retried explicitly after the source asset is fixed
- already-loaded real meshes are not destroyed by the retry helper, so scenes do not lose a mesh they may still reference
- malformed faces, out-of-range indices, and degenerate triangles are rejected instead of being emitted as unstable geometry

The sandbox includes both a valid sample OBJ asset and a missing-model example so this behavior is easy to inspect during manual QA.

## Sample asset

`assets/models/henka_marker.obj` is a small self-authored sample asset included for testing and demonstration. It is intentionally simple and readable.
