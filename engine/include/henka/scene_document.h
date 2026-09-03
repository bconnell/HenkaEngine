#ifndef HENKA_SCENE_DOCUMENT_H
#define HENKA_SCENE_DOCUMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/audio.h>
#include <henka/camera.h>
#include <henka/physics.h>
#include <henka/result.h>
#include <henka/script.h>

#define HENKA_SCENE_DOCUMENT_FORMAT_VERSION UINT32_C(7)
#define HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V6 UINT32_C(6)
#define HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V5 UINT32_C(5)
#define HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V4 UINT32_C(4)
#define HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V3 UINT32_C(3)
#define HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V2 UINT32_C(2)
#define HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION UINT32_C(1)
#define HENKA_SCENE_DOCUMENT_MAX_OBJECTS 1024U
#define HENKA_SCENE_DOCUMENT_MAX_BEHAVIORS_PER_OBJECT 8U
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

typedef uint64_t henka_scene_document_behavior_id;

#define HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID ((henka_scene_document_behavior_id)0)

typedef struct henka_scene_document_behavior
{
    henka_scene_document_behavior_id id;
    bool enabled;
    henka_script_language language;
    char asset_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
} henka_scene_document_behavior;

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
    henka_audio_emitter_config audio;
    size_t behavior_count;
    henka_scene_document_behavior behaviors[HENKA_SCENE_DOCUMENT_MAX_BEHAVIORS_PER_OBJECT];
    henka_scene_document_id parent_id;
} henka_scene_document_object;

henka_scene_document_object henka_scene_document_object_default(void);
henka_scene_document_behavior henka_scene_document_behavior_default(void);

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
/* The authored listener is value-only scene configuration. It is validated
 * and copied transactionally; runtime audio systems receive a separate
 * listener value when the scene is played. Legacy documents load with the
 * default listener. */
henka_result henka_scene_document_set_audio_listener(
    henka_scene_document* document,
    henka_audio_listener listener);
henka_result henka_scene_document_get_audio_listener(
    const henka_scene_document* document,
    henka_audio_listener* out_listener);
/* An authored scene camera is optional. Legacy documents have no authored
 * camera; callers can choose whether to retain or replace their runtime
 * camera when applying a document. */
henka_result henka_scene_document_set_camera(
    henka_scene_document* document,
    const henka_camera* camera);
henka_result henka_scene_document_clear_camera(
    henka_scene_document* document);
bool henka_scene_document_has_camera(
    const henka_scene_document* document);
henka_result henka_scene_document_get_camera(
    const henka_scene_document* document,
    henka_camera* out_camera);
size_t henka_scene_document_get_behavior_count(
    const henka_scene_document* document,
    henka_scene_document_id object_id);
henka_result henka_scene_document_get_behavior_at(
    const henka_scene_document* document,
    henka_scene_document_id object_id,
    size_t index,
    henka_scene_document_behavior* out_behavior);
henka_result henka_scene_document_get_behavior(
    const henka_scene_document* document,
    henka_scene_document_id object_id,
    henka_scene_document_behavior_id behavior_id,
    henka_scene_document_behavior* out_behavior);
henka_result henka_scene_document_add_behavior(
    henka_scene_document* document,
    henka_scene_document_id object_id,
    const henka_scene_document_behavior* behavior,
    henka_scene_document_behavior_id* out_behavior_id);
henka_result henka_scene_document_set_behavior(
    henka_scene_document* document,
    henka_scene_document_id object_id,
    const henka_scene_document_behavior* behavior);
henka_result henka_scene_document_remove_behavior(
    henka_scene_document* document,
    henka_scene_document_id object_id,
    henka_scene_document_behavior_id behavior_id);
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
