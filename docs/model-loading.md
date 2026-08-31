# Model Loading

Henka Engine supports bounded OBJ and glTF 2.0/GLB loading paths for local project assets.

> **Primary interchange path:** glTF/GLB
> **Legacy/simple geometry path:** OBJ

## Contents

- [Shared import model](#shared-import-model)
- [OBJ support](#obj-support)
- [glTF and GLB support](#gltf-and-glb-support)
- [Materials and textures](#materials-and-textures)
- [Scene import](#scene-import)
- [OBJ limits](#obj-limits)
- [glTF input limits](#gltf-input-limits)
- [Failure behavior](#failure-behavior)
- [Current limitations](#current-limitations)
- [Sample assets](#sample-assets)

## Shared import model

All model and scene loader destinations must be zero-initialized before their
first load. A later successful load replaces the destination's owned data only
after the complete input has been validated. Invalid input, unreadable files,
and allocation failures leave an existing destination unchanged; callers can
therefore retry a failed reload without losing the previously loaded model or
scene.

## Supported input
The glTF path is the interchange geometry authority. OBJ and glTF both produce `henka_model_data`, which feeds the normal renderer mesh-upload boundary.

Imported geometry can carry:

- positions;
- normals;
- UV0 and UV1;
- normalized RGBA vertex colors;
- tangent frames with handedness;
- indexed or non-indexed triangle data.

Generated triangle normals are used when supported geometry omits normals. UV1 is preserved through model data and the renderer vertex stream. The built-in material shader selects UV0 or UV1 per mapped texture semantic.

## OBJ support

The OBJ loader accepts:

- comments and blank lines;
- Windows and Unix line endings;
- extra whitespace around tokens;
- vertex positions;
- texture coordinates;
- normals;
- computed face normals when normals are absent;
- triangle, quad, and bounded n-gon faces through fan triangulation;
- positive and negative position, texture-coordinate, and normal indices;
- `o`, `g`, `s`, `mtllib`, and `usemtl` records as non-render statements;
- cached mesh loading through the asset manager;
- explicit retry after a failed cached source load is corrected.

### OBJ limits

OBJ input is treated as untrusted file content.

| Limit | Current boundary |
| --- | --- |
| Source file or in-memory source | 16 MiB |
| Individual line | 4,096 bytes |
| Vertices in one face | 128 |
| Position/UV/normal arrays | Bounded |
| Emitted vertex/index arrays | Bounded by checked renderer limits |

Numeric values must parse completely and remain finite. Allocation growth uses checked addition and multiplication. File reads must seek successfully and return the complete expected byte count.

Inputs outside these limits fail without returning a partial model.

## glTF and GLB support

The bounded glTF path supports:

### Geometry and buffers

- triangle primitives;
- glTF JSON with embedded data-URI buffers;
- GLB version 2 JSON/BIN containers;
- confined external `.bin` buffers for file loads;
- indexed and non-indexed accessors;
- positions, normals, tangents, UV0, UV1, and vertex colors;
- checked component conversion;
- generated triangle normals when normals are absent;
- multiple triangle primitives.

### Nodes and scenes

- node hierarchies with parent and cycle checks;
- selected top-level scene roots;
- TRS transforms;
- finite affine non-sheared matrix transforms;
- perspective cameras;
- orthographic cameras;
- bounded `KHR_lights_punctual` point, spot, and directional records.

Child nodes used as top-level scene roots are rejected as malformed input.

### Accessor validation

Optional vertex-attribute and index references must be nonnegative accessor indexes. Position, normal, and tangent accessors require their supported float shapes. UV and color integer accessors require normalized storage. Index accessors accept scalar unsigned byte, unsigned short, or unsigned int values.

Buffer views must remain inside their declared buffers. Accessor alignment, stride, component type, and element size are validated before publication.

### Matrix-authored nodes

Matrix-authored nodes are validated and decomposed into the runtime TRS contract before scene instantiation.

The importer rejects matrices containing:

- perspective terms;
- singular axes;
- shear;
- non-finite values.

The original local and computed world matrices remain available in CPU scene data for valid matrix-authored nodes.

### glTF input limits

| Limit | Current boundary |
| --- | --- |
| Aggregate source/container | 64 MiB |
| Supported JSON table | 256 entries |
| JSON nesting | Bounded |
| Renderer output | Bounded by renderer mesh-element limits |

GLB headers must declare the exact input length. The first chunk must be one aligned JSON chunk. Duplicate JSON or BIN chunks are rejected.

External buffer URIs are resolved beneath the model directory. Rooted paths, traversal paths, and URI-like paths are rejected.

The importer accepts material and punctual-light extensions that map to the shared renderer model. Unsupported entries in `extensionsUsed` or `extensionsRequired` fail the load. Node mesh, camera, and punctual-light references must be valid in-range indexes. Camera fields require finite positive ranges and far planes beyond near planes. A node may use matrix or TRS transform representation according to the glTF contract.

## Materials and textures

Material mapping uses the shared scene material and asset-manager contracts. Henka currently has one material authority for these imported values: the shared `henka_material` model.

`henka_assets_load_gltf_mesh_with_material` returns a material instance using imported glTF scalar controls and source-relative image dependencies through the descriptor-aware texture cache.

Supported material data includes:

- core glTF PBR metallic-roughness factors;
- alpha mode;
- double-sided state;
- `KHR_materials_ior`;
- transmission factor and transmission texture;
- bounded `KHR_materials_volume` attenuation and thickness controls;
- specular controls;
- clearcoat;
- sheen;
- emissive strength.

Texture semantic validation remains centralized in `henka_material_validate`.

### Manager-owned material assets

`henka_assets_load_gltf_material_asset` caches a material by canonical glTF source identity and returns a stable manager-owned asset.

Scenes can copy the current material value through `henka_assets_get_material_asset_material`.

Reload follows a candidate-first transaction:

1. parse the source;
2. resolve dependencies;
3. validate material semantics;
4. publish the complete candidate.

A failed parse, path, dependency, or semantic validation leaves the previous manager-owned material and stable identity intact.

### Material instances

A manager-owned definition can create a stack-owned `henka_material_instance`. Instances retain definition shader and semantic texture dependencies while supporting validated local overrides for scalar, color, alpha, and render-state values.

`henka_assets_refresh_material_instance` pulls the latest definition revision transactionally and preserves explicit overrides.

`henka_assets_get_material_asset_dependencies` exposes borrowed semantic texture edges. Callers can inspect each texture's source metadata through the asset manager.

## KTX2 and Basis texture dependencies

`KHR_texture_basisu` can select external or embedded KTX2 images.

The texture boundary validates:

- KTX2 container bounds;
- dimensions;
- layers and faces;
- complete bounded mip chains;
- semantic color-space compatibility;
- selected GPU-format capability.

Supported OpenGL upload targets include, when advertised by the active context:

- BC1 RGB/RGBA;
- BC3;
- BC5;
- BC7;
- ETC2;
- ASTC 4x4;
- RGBA8 fallback for supported uncompressed/Basis cases.

Basis normal maps prefer BC5 or ETC2-RG. BC7 and ASTC RGBA are valid compressed alternatives. BC1 and BC3 are excluded from normal-map transcode selection.

Native compressed payloads require matching GPU capability. Exact selected mip bytes participate in renderer memory accounting.

The same checked KTX2 boundary handles external images, data URIs, and bufferView-backed images. These sources remain manager-owned dependencies.

### Residency support

Manager-owned KTX2 textures support:

- synchronous bounded top-mip residency requests;
- deterministic distance and semantic-slot priority;
- active-frame pinning;
- revision-checked stale-request cancellation;
- deterministic trim-to-budget;
- an opt-in Windows worker for bounded source reads.

Validation and GPU upload remain render-thread work. Automatic policy eviction, broader background decode, cross-backend capability coverage, and the final visual stress matrix remain future work.

## Scene import

`henka_model_scene_data_load_gltf` loads the bounded CPU-side scene representation.

Scene data preserves:

- per-primitive material bindings;
- selected scene roots;
- node parentage;
- local and world matrices;
- cameras;
- punctual lights.

Scene data stays CPU-owned until manager/renderer instantiation publishes dependent meshes and material instances.

Instantiation applies the first active glTF camera and publishes active punctual lights into the runtime scene. The runtime currently supports a bounded four-local-light list.

A valid scene may contain cameras, lights, and nodes without mesh buffers. Mesh-bearing scenes require valid bounded buffers, accessors, and triangle primitives.

`henka_model_scene_data_set_active_scene` selects another validated scene before instantiation. `henka_assets_set_gltf_scene_active_scene` provides the manager-owned equivalent. Invalid indexes leave the current selection unchanged.

## Failure behavior

Missing, malformed, truncated, oversized, and unsupported OBJ/glTF source content follows the bounded asset-source failure path.

For source failures:

- the engine logs the failure;
- no partial model is returned;
- the Sandbox remains operational where fallback use is valid;
- the asset manager can provide the visible fallback mesh;
- failed cached source loads can be retried after correction;
- `henka_assets_load_gltf_mesh` uses the same canonical cache identity and transactional retry contract as OBJ meshes;
- existing successful meshes remain owned by their established owner.

Allocation, renderer, and other runtime failures are returned as runtime errors. They are not cached as successful source loads.

Malformed faces, empty meshes, invalid indexes, non-finite values, degenerate triangles, and unsafe allocation requests are rejected before renderer upload.

## Current limitations

Current model/import gaps include:

- MTL material import;
- concave OBJ polygon correction beyond fan triangulation;
- skeletal animation;
- skinning;
- morph targets;
- editor hierarchy authoring;
- complete editor import workflows;
- live replacement of a loaded mesh while active scenes still reference it;
- compressed glTF buffer extensions outside the supported boundary;
- production refraction and layered-volume rendering for imported materials;
- broader cross-backend compressed-texture validation.

A future editor material format may add editor and instance behavior while continuing to use the shared material and dependency model.

## Sample assets

- `assets/models/henka_marker.obj` is the OBJ loader sample used by tests.
- `assets/models/henka_marker.gltf` is the embedded-buffer glTF sample used by the Sandbox.
