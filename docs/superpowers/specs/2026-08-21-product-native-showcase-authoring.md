# Product-native showcase authoring design

## Product decision

The finished Cheeky Giraffe and Rocket are Henka-authored showcase assets. A
normal developer must be able to begin an asset in the visible Henka editor,
model and refine it through the same component and material workflows, save it,
reload it, continue editing it, place it in a scene, and package it. The final
models must not originate from an imported finished mesh, a generator, a direct
HAMS/native-file write, a hidden fixture constructor, or an asset-specific
engine path.

Repository generators and imported compatibility assets may continue to exist
as separately labeled tests. They are not a fallback or provenance source for
either final showcase model.

## Current evidence and gap

`henka_authoring_mesh` already provides bounded editable topology, validation,
UVs, and mesh history. `sandbox3d_authoring_object` already publishes edits to
the scene, renderer, bounds, physics, and undo/redo transaction. It supports
component selection, transforms, extrusion, bevels, insets, subdivision, vertex
operations, UV projection/packing, and source save/reload.

The current gap is final showcase integration and lifecycle truth:

- A bounded visible workflow now creates generic native assets, adds primitive
  parts, saves, closes, and reopens them; full refinement and final showcase
  authoring are not yet proven.
- The native document now persists bounded PBR material state and texture
  identities in sidecars, while UV-bearing topology is persisted in HAMS
  sources; a first-class document-level material/UV editing API remains open.
- Generated/imported sample paths remain the normal showcase path.
- Current showcase documentation describes the old fixture state, but must not
  remain the final claim.

## Architecture

### One native authoring document per authored asset

Add a bounded `sandbox3d_authoring_asset_document` module. It owns an asset
root, a fixed-capacity collection of scene-connected
`sandbox3d_authoring_object` parts, explicit material bindings, per-part source
identity, and a project-relative persistence destination. It is the only owner
that may create a new native asset, publish topology/material changes, save the
native authoring source, or reload it.

The document does not identify Giraffe or Rocket. It accepts a developer-supplied
asset name, creates generic primitive parts, and exposes ordinary add, select,
duplicate, transform, component-edit, material, UV, save, reload, and close
actions. All capacity, path, and name validation fails before mutating scene or
source state. Every failed operation preserves the prior document and scene.

### Generic primitive source creation

The engine constructors for Box, Plane, Cylinder, Cone, and UV Sphere are the
starting materials. The document converts their returned authoring mesh through
the existing `sandbox3d_authoring_object_create_from_mesh` bridge. Constructors
and document creation use bounded capacity descriptions; no variable-size
allocation or asset-specific geometry is permitted.

The editor exposes one discoverable `New Asset` action and a primitive
chooser. It removes the special `Create Native Rocket` control. Primitive parts
are editable source geometry, not a visual-completion claim.

### Authoring operations and material ownership

Existing component commands remain routed through the authoring-object
transaction. Gap work is driven by the two genuine authoring exercises: add a
generic capability only when the visible workflow cannot create, refine,
material, UV-map, shade, save, or reload an ordinary asset correctly. New
operations must take an authoring object/document and selection; none accepts a
showcase name or uses preset model geometry.

Material changes move from app-local temporary bindings into the document's
authoritative material-binding collection. A binding identifies the part and
material region, validates the material identity and texture path, is undone and
redone with the affected document operation, and serializes with the source.
UV and normal/shading changes use the same selected authoring part and
transaction; render refresh does not become a separate source of truth.

### Persistence, package, and provenance

Native documents save to a project-relative authored-asset directory after a
complete bounded preflight of every part, path, transform, and material. Each
HAMS source, material sidecar, and settings manifest uses the engine's
temporary-write/flush/replace persistence path; a new revision is published
only after preflight succeeds. Reload builds a candidate document, validates
every mesh, material binding, and path, then replaces the live document only
after the candidate is publishable. The package step copies the document, mesh
sources, textures, and material data as a unit.

The persisted document carries explicit provenance:
`HENKA_PRODUCT_NATIVE_AUTHORED`. That label is set only by normal document
creation and editor commands; import and fixture paths cannot write it.

### Visible proof

The existing graphical harness must activate normal visible editor controls and
capture each relevant state. It may move the controlled test cursor and click
the editor but may not create or modify source files directly. Required proof
is: new native asset, primitive source creation, component selection and
topology editing, transform/refinement, material and UV edit, save, process
close/release, reload, second edit, second save/reload, scene placement, and
packaged runtime render.

The final Giraffe and Rocket are constructed through that workflow and inspected
in multiple editor and packaged-runtime views. Automated evidence cannot
override human visual review. The final claim becomes true only after all
evidence is current.

## Safety and quality invariants

- Ordinary imported/user assets are read-only until a user explicitly creates a
  native editable copy or invokes a topology-changing operation.
- Candidate validation, scene/render/physics publication, selection remapping,
  and history updates are one transaction; no partial edit may escape.
- Handles, part arrays, selection lists, mesh capacities, strings, and paths are
  bounded and generation-checked where the existing systems provide handles.
- New C code is C17/MSVC-warning-clean at the project warning level and avoids
  unsafe dynamic growth, implicit narrowing, unchecked arithmetic, or path
  traversal.
- Public documentation does not say the showcase assets were authored in Henka
  until the current editor workflow and package evidence prove it.

## Acceptance evidence

1. Unit and integration tests prove bounded native-document creation, failure
   atomicity, undo/redo, material/UV persistence, source reload, and package
   inclusion.
2. A graphical harness drives only normal visible editor commands and produces
   operation-by-operation screenshots/logs.
3. The final Giraffe and Rocket sources carry
   `HENKA_PRODUCT_NATIVE_AUTHORED`; fixture/import sources cannot acquire that
   label.
4. After save, release, reload, a further topology or material edit succeeds and
   survives a second reload.
5. Packaged runtime renders both authored assets with their persisted materials
   and textures.
6. Human inspection accepts the editor and packaged visual evidence as
   deliberately modeled, production-like assets.
