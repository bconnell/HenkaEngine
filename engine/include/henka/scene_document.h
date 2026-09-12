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
#include <henka/scene.h>
#include <henka/script.h>

#define HENKA_SCENE_DOCUMENT_FORMAT_VERSION UINT32_C(13)
#define HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V12 UINT32_C(12)
#define HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V11 UINT32_C(11)
#define HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V10 UINT32_C(10)
#define HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V9 UINT32_C(9)
#define HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V8 UINT32_C(8)
#define HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V7 UINT32_C(7)
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

/* Supported non-terrain texture dependency identities may be persisted by
 * confined source path when a manager-owned material instance explicitly
 * overrides them. Texture pointers, shaders, manager-owned material
 * definitions, and terrain-layer resources remain outside Scene Document
 * authority. */
#define HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_BASE_COLOR (UINT32_C(1) << 0U)
#define HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_NORMAL (UINT32_C(1) << 1U)
#define HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_METALLIC_ROUGHNESS (UINT32_C(1) << 2U)
#define HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_OCCLUSION (UINT32_C(1) << 3U)
#define HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_EMISSIVE (UINT32_C(1) << 4U)
#define HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_TRANSMISSION (UINT32_C(1) << 5U)
#define HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_THICKNESS (UINT32_C(1) << 6U)
#define HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_KNOWN_MASK UINT32_C(0x7F)

typedef struct henka_scene_document_renderer
{
    bool enabled;
    char material_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    bool material_override;
    henka_material_type material_type;
    int base_color_uv_set;
    int normal_uv_set;
    int metallic_roughness_uv_set;
    int occlusion_uv_set;
    int emissive_uv_set;
    int transmission_uv_set;
    int thickness_uv_set;
    henka_vec4 base_color;
    float metallic;
    float roughness;
    henka_vec3 emissive;
    float emissive_strength;
    float specular_factor;
    henka_vec3 specular_color;
    float ior;
    float transmission;
    float thickness;
    float attenuation_distance;
    henka_vec3 attenuation_color;
    float subsurface;
    henka_vec3 subsurface_color;
    float normal_scale;
    float occlusion_strength;
    float clearcoat;
    float clearcoat_roughness;
    float alpha_cutoff;
    henka_material_alpha_mode alpha_mode;
    bool use_texture;
    bool use_lighting;
    bool depth_test;
    bool double_sided;
    bool cast_shadows;
    bool receive_shadows;
    uint32_t texture_override_mask;
    char base_color_texture_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    char normal_texture_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    char metallic_roughness_texture_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    char occlusion_texture_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    char emissive_texture_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    char transmission_texture_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    char thickness_texture_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    henka_vec3 sheen_color;
    float sheen_roughness;
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

/* Value-owned controller authoring. The runtime controller creates and owns
 * its physics body during Play; the document stores only validated settings.
 * The component is mutually exclusive with an authored physics body. */
typedef struct henka_scene_document_character_controller
{
    bool enabled;
    float radius;
    float half_height;
    float max_speed;
    float jump_speed;
    float acceleration;
    float deceleration;
    float air_control;
    float slope_limit_degrees;
    uint32_t layer;
    uint32_t mask;
} henka_scene_document_character_controller;

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
    henka_scene_document_character_controller character_controller;
    henka_audio_emitter_config audio;
    size_t behavior_count;
    henka_scene_document_behavior behaviors[HENKA_SCENE_DOCUMENT_MAX_BEHAVIORS_PER_OBJECT];
    henka_scene_document_id parent_id;
} henka_scene_document_object;

henka_scene_document_object henka_scene_document_object_default(void);
henka_scene_document_behavior henka_scene_document_behavior_default(void);
henka_scene_document_character_controller
henka_scene_document_character_controller_default(void);

henka_result henka_scene_document_create(henka_scene_document** out_document);
void henka_scene_document_destroy(henka_scene_document* document);
henka_result henka_scene_document_clear(henka_scene_document* document);
/* Replaces destination with a validated authored snapshot transactionally.
 * Persistent object/behavior IDs and the allocation watermark are copied
 * exactly; runtime pointers and handles are not part of the document. */
henka_result henka_scene_document_copy(
    henka_scene_document* destination,
    const henka_scene_document* source);
/* Swaps validated fixed-storage contents without allocating. The document
 * objects remain at their original addresses so borrowed bridge pointers stay
 * valid; the prepared source owns the previous destination contents after
 * the swap. */
henka_result henka_scene_document_swap_contents(
    henka_scene_document* destination,
    henka_scene_document* prepared_source);
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
/* The authored environment stores only value-owned scene settings. HDR
 * textures remain borrowed runtime resources and are rejected by the setter
 * until a supported asset-path authority can reconstruct them. The getter
 * returns a runtime-compatible descriptor with hdr_texture set to NULL. Legacy
 * documents load with the default environment. */
henka_result henka_scene_document_set_environment(
    henka_scene_document* document,
    henka_scene_environment_desc environment);
henka_result henka_scene_document_get_environment(
    const henka_scene_document* document,
    henka_scene_environment_desc* out_environment);
/* The authored render settings contain direct lighting and fog values.
 * Value-owned local lights and probe volumes are stored separately; renderer
 * owned textures, shaders, and captured probe resources are not serialized.
 * Legacy documents load default local-light and probe configuration. */
henka_result henka_scene_document_set_render_settings(
    henka_scene_document* document,
    henka_scene_render_settings settings);
henka_result henka_scene_document_get_render_settings(
    const henka_scene_document* document,
    henka_scene_render_settings* out_settings);
/* Local lights and reflection-probe volumes are value-owned authored scene
 * configuration. Renderer-owned captured textures are derived after load and
 * are not serialized as document state. */
henka_result henka_scene_document_set_render_resources(
    henka_scene_document* document,
    henka_scene_render_resources resources);
henka_result henka_scene_document_get_render_resources(
    const henka_scene_document* document,
    henka_scene_render_resources* out_resources);
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
