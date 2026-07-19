# Model Loading

Henka Engine supports a focused OBJ loading path for local project assets.

## Supported input

The loader currently supports:

- comments and blank lines
- Windows and Unix line endings
- extra whitespace around tokens
- vertex positions
- texture coordinates when present
- normals when present
- computed face normals when normals are absent
- triangle, quad, and n-gon faces through fan triangulation
- positive and negative position, texture-coordinate, and normal indices
- ignored non-render statements for `o`, `g`, `s`, `mtllib`, and `usemtl`
- cached mesh loading through the asset manager
- explicit retry of failed cached mesh loads after the source file is corrected

## Input limits

OBJ input is treated as untrusted file content.

- Source files and in-memory source strings are limited to 16 MiB.
- Individual lines are limited to 4,096 bytes.
- A face may contain at most 128 vertices.
- Position, texture-coordinate, and normal record arrays are bounded.
- Emitted vertex and index arrays are bounded and checked before narrowing to renderer counts.
- Allocation growth uses checked addition and multiplication.
- Numeric values must parse completely and must be finite.
- File reads must seek successfully and return the complete expected byte count.

Inputs outside these limits fail without returning a partial model.

## Failure behavior

When an OBJ file is missing, malformed, truncated, oversized, or outside the supported format:

- the engine logs the failure
- no partial model is returned
- the sandbox remains operational
- the asset manager uses its visible fallback mesh
- a failed cached load may be retried after the source file is corrected
- a successfully loaded mesh is not destroyed by the retry path

Malformed faces, invalid indices, non-finite values, degenerate triangles, and unsafe allocation requests are rejected before renderer upload.

## Not supported

The current loader does not provide:

- MTL material import
- concave polygon correction beyond fan triangulation
- model hierarchies
- skeletal animation
- glTF
- editor import workflows
- live replacement of an already-loaded mesh that scenes may still reference

## Sample asset

`assets/models/henka_marker.obj` is a small self-authored sample used by the sandbox and tests.
