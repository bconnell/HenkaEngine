# Model Loading

Henka Engine supports bounded OBJ and glTF 2.0/GLB loading paths for local project assets.

The glTF path is the interchange geometry authority. It produces the same
`henka_model_data` used by OBJ and mesh upload, so imported geometry does not
create a parallel renderer representation. The current bounded geometry subset
supports triangle primitives, embedded data-URI buffers, GLB JSON/BIN chunks,
confined external `.bin` buffers for file loads, positions, normals, UV0/UV1,
vertex colors, tangents, indexed or non-indexed accessors, and generated
triangle normals when normals are absent. UV1 is preserved through imported
model data and the renderer vertex stream for later lightmap/detail-material
use; current material shaders continue to sample UV0. Material mapping is resolved through
the shared scene material and asset-manager contracts rather than through a
separate Henka-only material schema. `henka_assets_load_gltf_mesh_with_material`
returns a material instance using imported glTF scalar controls and resolves
source-relative images through the descriptor-aware texture cache. Texture
semantic validation remains centralized in `henka_material_validate`.

`henka_assets_load_gltf_material_asset` caches the material by canonical glTF
source identity and returns a stable manager-owned asset object. Scenes may copy
its current `henka_material` instance through
`henka_assets_get_material_asset_material`. Reload parses and resolves a new
candidate first; only a fully valid candidate replaces the asset value, so a
parse, path, dependency, or semantic-validation failure leaves the previous
material and its stable identity intact.

The bounded scene importer is exposed separately through
`henka_model_scene_data_load_gltf`. It preserves per-primitive material
bindings, selected scene roots, node parentage, local/world matrices, cameras,
and `KHR_lights_punctual` point, spot, and directional records. Scene data is
CPU-owned until a manager/renderer instantiation path publishes its dependent
meshes and material instances. Instantiation also applies the first active
glTF camera and publishes active punctual lights into the runtime scene; the
runtime's four-local-light limit remains an explicit bounded fallback. A
failed scene parse or dependency build cannot publish partial renderer state.

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
- glTF PBR material factors, alpha mode, double-sided state, and supported
  `KHR_materials_ior`, `specular`, `clearcoat`, `sheen`, and emissive-strength
  scalar extensions
- source-relative external image URIs resolved as manager-owned texture
  dependencies with color-space/semantic descriptors
- embedded base-color, normal, metallic-roughness, occlusion, and emissive
  images supplied as bounded base64 data URIs or glTF image bufferViews;
  decoded textures use the same manager-owned semantic cache and fallback path
- `KHR_texture_basisu` source selection for external KTX2 images; the texture
  loader validates KTX2 bounds, dimensions, layers/faces, and mip ownership
  before the pinned KTX-Software boundary decodes uncompressed RGBA8 or
  transcodes Basis payloads to RGBA8. The container transfer function is checked
  against the requested semantic color space; a mismatch is rejected rather than
  silently applying an sRGB/linear interpretation. Other GPU-native compressed
  formats are rejected until a capability-selected upload path exists. The same checked
  KTX2 boundary is used for external images and embedded URI/bufferView bytes,
  so GLB and data-URI ownership does not create a second decoder or bypass the
  asset manager.
- multiple triangle primitives, node hierarchies with cycle and parent checks,
  selected top-level scene roots (child nodes are rejected as malformed roots),
  TRS or matrix node transforms, perspective and
  orthographic cameras, and bounded punctual-light records

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
The importer accepts only the material and punctual-light extensions it maps
to the shared renderer model. Unsupported entries in `extensionsUsed` or
`extensionsRequired` fail the load rather than being silently treated as
equivalent content. A node must use either a matrix or TRS transform, as
required by glTF, and conflicting representations are rejected.

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
- skeletal animation, skinning, morph targets, and editor hierarchy authoring
- skeletal animation
- editor import workflows
- live replacement of an already-loaded mesh that scenes may still reference
- glTF skins, animations, morph targets, and compressed buffer extensions in
  the existing manager renderer path
- a second Henka-only material JSON authority; an editor material format will
  only be introduced if it adds instance/editor behavior beyond glTF and will
  reuse this same material and dependency path

## Sample asset

`assets/models/henka_marker.obj` remains the OBJ sample used by loader tests;
`assets/models/henka_marker.gltf` is the embedded-buffer glTF sample used by
the sandbox.
