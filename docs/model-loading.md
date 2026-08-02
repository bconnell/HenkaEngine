# Model Loading

Henka Engine supports bounded OBJ and glTF 2.0/GLB loading paths for local project assets.

The glTF path is the interchange geometry authority. It produces the same
`henka_model_data` used by OBJ and mesh upload, so imported geometry does not
create a parallel renderer representation. The current bounded geometry subset
supports triangle primitives, embedded data-URI buffers, GLB JSON/BIN chunks,
confined external `.bin` buffers for file loads, positions, normals, UV0,
vertex colors, tangents, indexed or non-indexed accessors, and generated
triangle normals when normals are absent. Material mapping is intentionally
being built on the shared scene material and asset-manager contracts rather
than through a separate Henka-only material schema.

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
- glTF triangle primitives through `henka_model_data_load_gltf` and
  `henka_model_data_load_gltf_from_memory`
- GLB version 2 JSON/BIN containers and glTF data-URI buffers
- indexed and non-indexed glTF accessors with checked component conversion

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

glTF/GLB input is also bounded: the aggregate source or container is limited
to 64 MiB, JSON arrays are limited to 256 entries per supported table, JSON
nesting is limited, buffer views must remain inside their declared buffers,
accessor strides and component types are checked, and output remains bounded
by the renderer mesh-element limit. External buffer URIs are resolved beneath
the model's directory; rooted, traversal, and URI-like paths are rejected.

## Failure behavior

When an OBJ or glTF file is missing, malformed, truncated, oversized, or outside the supported format:

- the engine logs the failure
- no partial model is returned
- the sandbox remains operational
- the asset manager uses its visible fallback mesh
- a failed cached load may be retried after the source file is corrected
- `henka_assets_load_gltf_mesh` uses the same canonical identity, fallback,
  ownership, and transactional retry contract as OBJ mesh assets
- a successfully loaded mesh is not destroyed by the retry path

Malformed faces, invalid indices, non-finite values, degenerate triangles, and unsafe allocation requests are rejected before renderer upload.

## Not supported

The current loader does not provide:

- MTL material import
- concave polygon correction beyond fan triangulation
- model hierarchies
- skeletal animation
- editor import workflows
- live replacement of an already-loaded mesh that scenes may still reference
- glTF scene hierarchies, skins, animations, morph targets, multiple named
  material bindings, and compressed buffer extensions

## Sample asset

`assets/models/henka_marker.obj` is a small self-authored sample used by the sandbox and tests.
