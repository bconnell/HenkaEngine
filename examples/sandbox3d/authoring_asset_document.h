#ifndef SANDBOX3D_AUTHORING_ASSET_DOCUMENT_H
#define SANDBOX3D_AUTHORING_ASSET_DOCUMENT_H

#include <stddef.h>

#include <henka/engine.h>
#include <henka/scene.h>

#include "object_authoring_tools.h"

#define SANDBOX3D_AUTHORING_ASSET_PART_CAPACITY 64U

typedef struct sandbox3d_authoring_asset_document sandbox3d_authoring_asset_document;

typedef enum sandbox3d_authoring_provenance
{
    SANDBOX3D_AUTHORING_PROVENANCE_UNKNOWN = 0,
    SANDBOX3D_AUTHORING_PROVENANCE_PRODUCT_NATIVE_AUTHORED
} sandbox3d_authoring_provenance;

#define SANDBOX3D_AUTHORING_PROVENANCE_LABEL "HENKA_PRODUCT_NATIVE_AUTHORED"

typedef enum sandbox3d_authoring_primitive_kind
{
    SANDBOX3D_AUTHORING_PRIMITIVE_BOX = 0,
    SANDBOX3D_AUTHORING_PRIMITIVE_PLANE,
    SANDBOX3D_AUTHORING_PRIMITIVE_CYLINDER,
    SANDBOX3D_AUTHORING_PRIMITIVE_CONE,
    SANDBOX3D_AUTHORING_PRIMITIVE_UV_SPHERE,
    SANDBOX3D_AUTHORING_PRIMITIVE_QUAD_SPHERE
} sandbox3d_authoring_primitive_kind;

typedef struct sandbox3d_authoring_primitive_desc
{
    float width;
    float height;
    float depth;
    float radius;
    size_t segments;
    size_t latitude_segments;
    size_t subdivisions;
} sandbox3d_authoring_primitive_desc;

/* Creates one bounded editor-owned native asset document. The document begins
 * without geometry; generic editable parts are added by later document
 * operations. Creation rejects invalid names before publishing state. */
henka_result sandbox3d_authoring_asset_document_create(
    henka_engine* engine,
    henka_scene* scene,
    const char* name,
    sandbox3d_authoring_asset_document** out_document);
void sandbox3d_authoring_asset_document_destroy(
    sandbox3d_authoring_asset_document* document);

sandbox3d_authoring_provenance
sandbox3d_authoring_asset_document_get_provenance(
    const sandbox3d_authoring_asset_document* document);
const char* sandbox3d_authoring_asset_document_get_name(
    const sandbox3d_authoring_asset_document* document);
/* The document owns the bounded asset-name grammar used by all authoring
 * commands and persistence paths. */
bool sandbox3d_authoring_asset_document_name_is_valid(const char* name);
size_t sandbox3d_authoring_asset_document_get_part_count(
    const sandbox3d_authoring_asset_document* document);

/* Creates one generic editable primitive part through the scene-connected
 * authoring-object bridge. The part is published only after its source mesh,
 * scene entity, and editable bridge all succeed. */
henka_result sandbox3d_authoring_asset_document_add_primitive(
    sandbox3d_authoring_asset_document* document,
    const char* part_name,
    sandbox3d_authoring_primitive_kind kind,
    const sandbox3d_authoring_primitive_desc* desc,
    size_t history_steps,
    size_t* out_part_index);
sandbox3d_authoring_object* sandbox3d_authoring_asset_document_get_part(
    sandbox3d_authoring_asset_document* document,
    size_t part_index);
/* Returns the document index for an already-owned part. The lookup is
 * pointer/entity based and fails closed for objects from another document. */
bool sandbox3d_authoring_asset_document_find_part(
    const sandbox3d_authoring_asset_document* document,
    const sandbox3d_authoring_object* part,
    size_t* out_part_index);
/* Returns the generic source kind recorded for one document part. */
henka_result sandbox3d_authoring_asset_document_get_part_kind(
    const sandbox3d_authoring_asset_document* document,
    const sandbox3d_authoring_object* part,
    sandbox3d_authoring_primitive_kind* out_kind);
/* Adopts an existing scene-connected authoring bridge into this document.
 * The document takes ownership until the caller explicitly transfers it to
 * the editor registry. No scene or ownership state changes on failure. */
henka_result sandbox3d_authoring_asset_document_adopt_part(
    sandbox3d_authoring_asset_document* document,
    sandbox3d_authoring_object* part,
    sandbox3d_authoring_primitive_kind kind,
    size_t* out_part_index);
/* Transfers exactly one part bridge to the editor registry while retaining
 * the document's scene-entity lifetime metadata. The caller becomes
 * responsible for destroying the returned bridge; the document still retires
 * its scene entity unless the metadata is explicitly forgotten. */
henka_result sandbox3d_authoring_asset_document_release_part_ownership(
    sandbox3d_authoring_asset_document* document,
    size_t part_index,
    sandbox3d_authoring_object** out_part);
/* Discards an unpublished document-owned part. This is only valid before
 * ownership transfer and is intended for transactional editor rollback. */
henka_result sandbox3d_authoring_asset_document_discard_part(
    sandbox3d_authoring_asset_document* document,
    size_t part_index);
/* Removes metadata for a part whose ownership was transferred and which is
 * about to be destroyed by the editor registry. It never destroys the bridge
 * or entity itself. */
bool sandbox3d_authoring_asset_document_forget_released_part(
    sandbox3d_authoring_asset_document* document,
    const sandbox3d_authoring_object* part);
/* Saves under project_root using a confined relative manifest path. Every part
 * source and bounded PBR material record is written to the deterministic
 * asset-owned source directory before the manifest is atomically published. */
henka_result sandbox3d_authoring_asset_document_save(
    sandbox3d_authoring_asset_document* document,
    const char* project_root,
    const char* relative_manifest_path);
/* Loads a complete manifest into an independent candidate document. The
 * caller receives no document and the scene retains all pre-existing authored
 * entities when any manifest field or source fails validation. The optional
 * material_template supplies the runtime-owned shader binding. Persisted PBR
 * properties and file-backed texture identities replace its corresponding
 * values; runtime-only or fallback texture identities are rejected on save. */
henka_result sandbox3d_authoring_asset_document_load(
    henka_engine* engine,
    henka_scene* scene,
    const char* project_root,
    const char* relative_manifest_path,
    size_t history_steps,
    const henka_material* material_template,
    sandbox3d_authoring_asset_document** out_document);

#endif
