#ifndef HENKA_SCENE_DOCUMENT_H
#define HENKA_SCENE_DOCUMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/physics.h>
#include <henka/result.h>

#define HENKA_SCENE_DOCUMENT_FORMAT_VERSION UINT32_C(1)
#define HENKA_SCENE_DOCUMENT_MAX_OBJECTS 1024U
#define HENKA_SCENE_DOCUMENT_MAX_NAME_BYTES 128U
#define HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES 512U
#define HENKA_SCENE_DOCUMENT_MAX_PROMPT_BYTES 128U
#define HENKA_SCENE_DOCUMENT_MAX_FILE_BYTES (4U * 1024U * 1024U)
#define HENKA_SCENE_DOCUMENT_MAX_INSPECTION_BYTES (128U * 1024U)

typedef struct henka_scene_document henka_scene_document;
typedef uint64_t henka_scene_document_id;

#define HENKA_INVALID_SCENE_DOCUMENT_ID ((henka_scene_document_id)0)

typedef enum henka_scene_document_source_kind
{
    HENKA_SCENE_DOCUMENT_SOURCE_NONE = 0,
    HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE,
    HENKA_SCENE_DOCUMENT_SOURCE_AUTHORING_MESH,
    HENKA_SCENE_DOCUMENT_SOURCE_ASSET
} henka_scene_document_source_kind;

typedef enum henka_scene_document_primitive_kind
{
    HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX = 0,
    HENKA_SCENE_DOCUMENT_PRIMITIVE_SPHERE,
    HENKA_SCENE_DOCUMENT_PRIMITIVE_PLANE
} henka_scene_document_primitive_kind;

typedef enum henka_scene_document_asset_kind
{
    HENKA_SCENE_DOCUMENT_ASSET_UNKNOWN = 0,
    HENKA_SCENE_DOCUMENT_ASSET_MESH,
    HENKA_SCENE_DOCUMENT_ASSET_GLTF_SCENE,
    HENKA_SCENE_DOCUMENT_ASSET_MATERIAL
} henka_scene_document_asset_kind;

typedef struct henka_scene_document_source
{
    henka_scene_document_source_kind kind;
    henka_scene_document_primitive_kind primitive;
    henka_vec3 primitive_dimensions;
    henka_scene_document_asset_kind asset_kind;
    char path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
} henka_scene_document_source;

typedef struct henka_scene_document_renderer
{
    bool enabled;
    char material_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    bool material_override;
    henka_vec4 base_color;
    float metallic;
    float roughness;
    henka_vec3 emissive;
    float emissive_strength;
} henka_scene_document_renderer;

typedef struct henka_scene_document_interaction
{
    bool enabled;
    float max_distance;
    char prompt[HENKA_SCENE_DOCUMENT_MAX_PROMPT_BYTES];
} henka_scene_document_interaction;

typedef struct henka_scene_document_physics
{
    bool enabled;
    henka_physics_body_type body_type;
    henka_physics_shape_type shape;
    henka_vec3 collider_offset;
    float sphere_radius;
    henka_vec3 box_half_extents;
    bool is_trigger;
    float mass;
    henka_physics_material material;
    uint32_t layer;
    uint32_t mask;
} henka_scene_document_physics;

/* Pure authoring data. It contains no renderer pointers, asset-manager
 * ownership, physics body IDs, runtime scene handles, or UI state. */
typedef struct henka_scene_document_object
{
    henka_scene_document_id id;
    char name[HENKA_SCENE_DOCUMENT_MAX_NAME_BYTES];
    bool visible;
    henka_transform transform;
    henka_scene_document_source source;
    henka_scene_document_renderer renderer;
    henka_scene_document_interaction interaction;
    henka_scene_document_physics physics;
} henka_scene_document_object;

henka_scene_document_object henka_scene_document_object_default(void);

henka_result henka_scene_document_create(henka_scene_document** out_document);
void henka_scene_document_destroy(henka_scene_document* document);
henka_result henka_scene_document_clear(henka_scene_document* document);
size_t henka_scene_document_get_object_count(const henka_scene_document* document);
henka_result henka_scene_document_get_object_at(
    const henka_scene_document* document,
    size_t index,
    henka_scene_document_object* out_object);
henka_result henka_scene_document_get_object(
    const henka_scene_document* document,
    henka_scene_document_id id,
    henka_scene_document_object* out_object);
henka_result henka_scene_document_add_object(
    henka_scene_document* document,
    const henka_scene_document_object* object,
    henka_scene_document_id* out_id);
henka_result henka_scene_document_duplicate_object(
    henka_scene_document* document,
    henka_scene_document_id source_id,
    henka_scene_document_id* out_id);
henka_result henka_scene_document_set_object(
    henka_scene_document* document,
    const henka_scene_document_object* object);
henka_result henka_scene_document_remove_object(
    henka_scene_document* document,
    henka_scene_document_id id);
henka_result henka_scene_document_validate(const henka_scene_document* document);

/* project_root is trusted by the caller; relative_path is always confined
 * beneath it. Saves are atomic and loads replace the document only after a
 * complete candidate has passed validation. */
henka_result henka_scene_document_save_file(
    const henka_scene_document* document,
    const char* project_root,
    const char* relative_path);
henka_result henka_scene_document_load_file(
    henka_scene_document* document,
    const char* project_root,
    const char* relative_path);

/* Produces a bounded, runtime-independent inspection report containing the
 * format, object IDs, source kinds, and component presence. */
henka_result henka_scene_document_format_inspection(
    const henka_scene_document* document,
    char* buffer,
    size_t buffer_capacity,
    size_t* out_size);

#endif
