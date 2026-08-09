#ifndef HENKA_SCENE_H
#define HENKA_SCENE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/camera.h>
#include <henka/math.h>
#include <henka/mesh.h>
#include <henka/result.h>
#include <henka/shader.h>
#include <henka/texture.h>

typedef struct henka_scene henka_scene;

/*
 * Opaque generation-checked scene identity. Numeric values are not stable
 * array indexes and must not be decoded by callers.
 */
typedef uint64_t henka_entity;

#define HENKA_INVALID_ENTITY ((henka_entity)0)

typedef enum henka_material_type
{
    HENKA_MATERIAL_TYPE_LIT = 0,
    HENKA_MATERIAL_TYPE_UNLIT,
    HENKA_MATERIAL_TYPE_VERTEX_COLOR,
    HENKA_MATERIAL_TYPE_PROCEDURAL_RESERVED
} henka_material_type;

typedef enum henka_material_alpha_mode
{
    HENKA_MATERIAL_ALPHA_OPAQUE = 0,
    HENKA_MATERIAL_ALPHA_MASKED,
    HENKA_MATERIAL_ALPHA_BLENDED
} henka_material_alpha_mode;

#define HENKA_MATERIAL_TERRAIN_LAYER_COUNT 4U

typedef struct henka_material_layer
{
    henka_texture* base_color_texture;
    henka_texture* normal_texture;
    henka_texture* metallic_roughness_texture;
    henka_vec4 base_color;
    float metallic;
    float roughness;
    float texture_scale_meters;
    float normal_scale;
} henka_material_layer;

typedef struct henka_material
{
    const char* name;
    henka_material_type type;
    henka_shader* shader;
    henka_texture* base_color_texture;
    henka_texture* normal_texture;
    henka_texture* metallic_roughness_texture;
    henka_texture* occlusion_texture;
    henka_texture* emissive_texture;
    /* glTF texture coordinate set selection; the bounded runtime supports UV0 and UV1. */
    int base_color_uv_set;
    int normal_uv_set;
    int metallic_roughness_uv_set;
    int occlusion_uv_set;
    int emissive_uv_set;
    henka_vec4 base_color;
    henka_vec3 emissive_color;
    float metallic;
    float roughness;
    float specular_factor;
    henka_vec3 specular_color;
    float ior;
    /* Bounded environment-based transmission factor; refraction/layered volume remain renderer work. */
    float transmission;
    /* Bounded glTF volume attenuation controls; refraction and layered volume remain renderer work. */
    float thickness;
    float attenuation_distance;
    henka_vec3 attenuation_color;
    float normal_scale;
    float occlusion_strength;
    float emissive_strength;
    float clearcoat;
    float clearcoat_roughness;
    float alpha_cutoff;
    bool use_texture;
    bool use_lighting;
    bool depth_test;
    henka_material_alpha_mode alpha_mode;
    bool double_sided;
    bool cast_shadows;
    bool receive_shadows;
    henka_vec3 sheen_color;
    float sheen_roughness;
    bool terrain_layers_enabled;
    henka_material_layer terrain_layers[HENKA_MATERIAL_TERRAIN_LAYER_COUNT];
} henka_material;

typedef struct henka_scene_object_info
{
    henka_entity entity;
    const char* name;
    const char* tag;
    bool visible;
    bool has_bounds;
    henka_bounds local_bounds;
    henka_transform transform;
} henka_scene_object_info;

typedef struct henka_interaction_desc
{
    bool enabled;
    float max_distance;
    const char* prompt;
} henka_interaction_desc;

typedef struct henka_scene_environment_desc
{
    henka_vec3 ground_color;
    henka_vec3 horizon_color;
    henka_vec3 zenith_color;
    float intensity;
    /* Borrowed linear HDR equirectangular texture; the scene does not own it. */
    henka_texture* hdr_texture;
    float hdr_rotation;
} henka_scene_environment_desc;

#define HENKA_SCENE_MAX_REFLECTION_PROBES 8U

typedef struct henka_scene_reflection_probe_desc
{
    henka_vec3 position;
    henka_vec3 extents;
    float influence;
    bool enabled;
    bool box_projection;
} henka_scene_reflection_probe_desc;

#define HENKA_SCENE_MAX_LOD_LEVELS 3U

typedef struct henka_scene_lod_desc
{
    uint32_t level_count;
    henka_mesh* meshes[HENKA_SCENE_MAX_LOD_LEVELS];
    float max_distances[HENKA_SCENE_MAX_LOD_LEVELS];
} henka_scene_lod_desc;

#define HENKA_SCENE_MAX_LOCAL_LIGHTS 4U

typedef enum henka_scene_light_type
{
    HENKA_SCENE_LIGHT_POINT = 0,
    HENKA_SCENE_LIGHT_SPOT
} henka_scene_light_type;

typedef struct henka_scene_light_desc
{
    henka_scene_light_type type;
    henka_vec3 position;
    henka_vec3 direction;
    henka_vec3 color;
    float intensity;
    float range;
    float inner_cone_cosine;
    float outer_cone_cosine;
    bool enabled;
} henka_scene_light_desc;

typedef enum henka_scene_fog_mode
{
    HENKA_SCENE_FOG_LINEAR = 0,
    HENKA_SCENE_FOG_EXPONENTIAL,
    HENKA_SCENE_FOG_EXPONENTIAL_SQUARED
} henka_scene_fog_mode;

typedef struct henka_scene_fog_desc
{
    bool enabled;
    henka_scene_fog_mode mode;
    henka_vec3 color;
    float start_distance;
    float end_distance;
    float density;
} henka_scene_fog_desc;

typedef enum henka_scene_entity_flags
{
    HENKA_SCENE_ENTITY_FLAG_NONE = 0,
    HENKA_SCENE_ENTITY_FLAG_HELPER = 1 << 0,
    HENKA_SCENE_ENTITY_FLAG_TRANSFORM_LOCKED = 1 << 1
} henka_scene_entity_flags;

typedef enum henka_interaction_result
{
    HENKA_INTERACTION_RESULT_UNAVAILABLE = 0,
    HENKA_INTERACTION_RESULT_DISABLED,
    HENKA_INTERACTION_RESULT_OUT_OF_RANGE,
    HENKA_INTERACTION_RESULT_AVAILABLE
} henka_interaction_result;

const char* henka_material_type_get_label(henka_material_type type);
henka_result henka_material_validate(const henka_material* material);
henka_result henka_material_describe(const henka_material* material, char* buffer, size_t buffer_size);
henka_material henka_material_default(void);
henka_material henka_material_terrain_default(void);
henka_result henka_scene_create(henka_scene** out_scene);
void henka_scene_destroy(henka_scene* scene);
henka_entity henka_scene_create_entity(henka_scene* scene);
henka_entity henka_scene_create_entity_named(henka_scene* scene, const char* name);
void henka_scene_destroy_entity(henka_scene* scene, henka_entity entity);
bool henka_scene_is_entity_valid(const henka_scene* scene, henka_entity entity);
bool henka_scene_is_entity_visible(const henka_scene* scene, henka_entity entity);
size_t henka_scene_get_entity_count(const henka_scene* scene);
henka_entity henka_scene_get_entity_at_index(const henka_scene* scene, size_t index);
const char* henka_scene_get_entity_name(const henka_scene* scene, henka_entity entity);
const char* henka_scene_get_entity_tag(const henka_scene* scene, henka_entity entity);
henka_result henka_scene_find_entity_by_name(const henka_scene* scene, const char* name, henka_entity* out_entity);
henka_result henka_scene_find_entity_by_tag(const henka_scene* scene, const char* tag, henka_entity* out_entity);
henka_result henka_scene_get_entity_info(const henka_scene* scene, henka_entity entity, henka_scene_object_info* out_info);
henka_result henka_scene_get_entity_transform(const henka_scene* scene, henka_entity entity, henka_transform* out_transform);
henka_result henka_scene_get_entity_mesh(const henka_scene* scene, henka_entity entity, henka_mesh** out_mesh);
henka_result henka_scene_get_entity_material(const henka_scene* scene, henka_entity entity, henka_material* out_material);
henka_result henka_scene_get_entity_local_bounds(const henka_scene* scene, henka_entity entity, henka_bounds* out_bounds);
henka_result henka_scene_get_entity_world_bounds(const henka_scene* scene, henka_entity entity, henka_bounds* out_bounds);
henka_result henka_scene_get_entity_interaction(const henka_scene* scene, henka_entity entity, henka_interaction_desc* out_interaction);
henka_result henka_scene_get_entity_flags(const henka_scene* scene, henka_entity entity, uint32_t* out_flags);
henka_result henka_scene_set_entity_transform(henka_scene* scene, henka_entity entity, henka_transform transform);
henka_result henka_scene_translate_entity(henka_scene* scene, henka_entity entity, henka_vec3 delta);
henka_result henka_scene_rotate_entity(henka_scene* scene, henka_entity entity, henka_quat delta_rotation);
henka_result henka_scene_scale_entity(henka_scene* scene, henka_entity entity, henka_vec3 scale_multiplier);
henka_result henka_scene_set_entity_mesh(henka_scene* scene, henka_entity entity, henka_mesh* mesh);
henka_result henka_scene_set_entity_material(henka_scene* scene, henka_entity entity, henka_material material);
henka_result henka_scene_set_entity_name(henka_scene* scene, henka_entity entity, const char* name);
henka_result henka_scene_set_entity_tag(henka_scene* scene, henka_entity entity, const char* tag);
henka_result henka_scene_set_entity_visible(henka_scene* scene, henka_entity entity, bool visible);
henka_result henka_scene_set_entity_local_bounds(henka_scene* scene, henka_entity entity, henka_bounds bounds);
henka_result henka_scene_clear_entity_local_bounds(henka_scene* scene, henka_entity entity);
henka_result henka_scene_set_entity_interaction(henka_scene* scene, henka_entity entity, const henka_interaction_desc* interaction);
henka_result henka_scene_set_entity_flags(henka_scene* scene, henka_entity entity, uint32_t flags);
bool henka_scene_is_entity_helper(const henka_scene* scene, henka_entity entity);
bool henka_scene_is_entity_transform_locked(const henka_scene* scene, henka_entity entity);
henka_interaction_result henka_scene_can_interact(const henka_scene* scene, henka_entity entity, henka_vec3 observer_position);
henka_result henka_scene_pick_entity(const henka_scene* scene, henka_ray ray, henka_entity* out_entity, float* out_distance);
henka_result henka_scene_set_camera(henka_scene* scene, const henka_camera* camera);
void henka_scene_set_light_direction(henka_scene* scene, henka_vec3 light_direction);
void henka_scene_set_light_color(henka_scene* scene, henka_vec3 light_color);
void henka_scene_set_light_intensity(henka_scene* scene, float light_intensity);
void henka_scene_set_ambient_color(henka_scene* scene, henka_vec3 ambient_color);
henka_result henka_scene_set_environment(
    henka_scene* scene,
    henka_scene_environment_desc environment);
henka_result henka_scene_get_environment(
    const henka_scene* scene,
    henka_scene_environment_desc* out_environment);
henka_result henka_scene_add_reflection_probe(
    henka_scene* scene,
    henka_scene_reflection_probe_desc probe,
    uint32_t* out_probe_index);
henka_result henka_scene_update_reflection_probe(
    henka_scene* scene,
    uint32_t probe_index,
    henka_scene_reflection_probe_desc probe);
henka_result henka_scene_remove_reflection_probe(
    henka_scene* scene,
    uint32_t probe_index);
henka_result henka_scene_get_reflection_probe(
    const henka_scene* scene,
    uint32_t probe_index,
    henka_scene_reflection_probe_desc* out_probe);
henka_result henka_scene_set_entity_lod(
    henka_scene* scene,
    henka_entity entity,
    henka_scene_lod_desc lod);
henka_result henka_scene_get_entity_lod(
    const henka_scene* scene,
    henka_entity entity,
    henka_scene_lod_desc* out_lod);
henka_result henka_scene_add_light(
    henka_scene* scene,
    henka_scene_light_desc light,
    uint32_t* out_light_index);
henka_result henka_scene_update_light(
    henka_scene* scene,
    uint32_t light_index,
    henka_scene_light_desc light);
henka_result henka_scene_remove_light(
    henka_scene* scene,
    uint32_t light_index);
henka_result henka_scene_get_light(
    const henka_scene* scene,
    uint32_t light_index,
    henka_scene_light_desc* out_light);
henka_result henka_scene_set_fog(henka_scene* scene, henka_scene_fog_desc fog);
henka_result henka_scene_get_fog(
    const henka_scene* scene,
    henka_scene_fog_desc* out_fog);

#endif
