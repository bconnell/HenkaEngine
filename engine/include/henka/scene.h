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
typedef struct henka_material_asset henka_material_asset;

/*
 * Opaque generation-checked scene identity. Numeric values are not stable
 * array indexes and must not be decoded by callers.
 */
typedef uint64_t henka_entity;

#define HENKA_INVALID_ENTITY ((henka_entity)0)

typedef enum henka_scene_parenting_mode
{
    HENKA_SCENE_PARENT_KEEP_LOCAL = 0,
    HENKA_SCENE_PARENT_KEEP_WORLD
} henka_scene_parenting_mode;

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
    int transmission_uv_set;
    int thickness_uv_set;
    henka_vec4 base_color;
    henka_vec3 emissive_color;
    float metallic;
    float roughness;
    float specular_factor;
    henka_vec3 specular_color;
    float ior;
    /* Bounded environment-based transmission factor; refraction/layered volume remain renderer work. */
    float transmission;
    /* Optional glTF KHR_materials_transmission scalar texture; manager-owned and
     * sampled as a bounded linear factor in the renderer. */
    henka_texture* transmission_texture;
    /* Bounded glTF volume attenuation controls; refraction and layered volume remain renderer work. */
    float thickness;
    /* Optional glTF KHR_materials_volume thickness data; manager-owned and
     * sampled as a bounded linear scalar in the renderer. */
    henka_texture* thickness_texture;
    float attenuation_distance;
    henka_vec3 attenuation_color;
    /* Runtime-authored bounded direct-light diffusion approximation. This is
     * not a full multi-scatter subsurface profile or diffusion texture. */
    float subsurface;
    henka_vec3 subsurface_color;
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

/* A complete presentation update for one live scene entity. All pointer
 * values are borrowed for the duration of the call. The scene validates and
 * prepares every fallible allocation before publishing any field or revision,
 * so callers do not need an allocating rollback path. Name changes are
 * included in the same atomic update but do not consume render revisions. */
typedef struct henka_scene_entity_presentation_update
{
    const char* name;
    henka_transform transform;
    bool visible;
    henka_interaction_desc interaction;
    bool apply_material;
    henka_material material;
} henka_scene_entity_presentation_update;

typedef enum henka_scene_environment_mode
{
    HENKA_SCENE_ENVIRONMENT_GRADIENT = 0,
    HENKA_SCENE_ENVIRONMENT_HDRI,
    HENKA_SCENE_ENVIRONMENT_PROCEDURAL
} henka_scene_environment_mode;

typedef enum henka_scene_environment_preset
{
    HENKA_SCENE_ENVIRONMENT_PRESET_CLEAR_MIDDAY = 0,
    HENKA_SCENE_ENVIRONMENT_PRESET_GOLDEN_HOUR,
    HENKA_SCENE_ENVIRONMENT_PRESET_MOONLIT_NIGHT,
    HENKA_SCENE_ENVIRONMENT_PRESET_ALIEN_HAZE,
    HENKA_SCENE_ENVIRONMENT_PRESET_COUNT
} henka_scene_environment_preset;

typedef struct henka_scene_atmosphere_desc
{
    float rayleigh_scattering;
    float mie_scattering;
    float mie_anisotropy;
    float density;
    float turbidity;
    float ozone_absorption;
    float atmosphere_height;
    float planet_radius;
    henka_vec3 ground_albedo;
    float horizon_intensity;
} henka_scene_atmosphere_desc;

typedef struct henka_scene_sun_desc
{
    bool enabled;
    bool manual_direction;
    henka_vec3 direction;
    henka_vec3 color;
    float intensity;
    float angular_radius;
} henka_scene_sun_desc;

typedef struct henka_scene_moon_desc
{
    bool enabled;
    bool manual_direction;
    henka_vec3 direction;
    henka_vec3 color;
    float intensity;
    float angular_radius;
} henka_scene_moon_desc;

typedef struct henka_scene_stars_desc
{
    bool enabled;
    float intensity;
    float rotation;
} henka_scene_stars_desc;

typedef struct henka_scene_environment_desc
{
    henka_vec3 ground_color;
    henka_vec3 horizon_color;
    henka_vec3 zenith_color;
    float intensity;
    /* Borrowed linear HDR equirectangular texture; the scene does not own it. */
    henka_texture* hdr_texture;
    float hdr_rotation;
    henka_scene_environment_mode mode;
    henka_scene_atmosphere_desc atmosphere;
    henka_scene_sun_desc sun;
    henka_scene_moon_desc moon;
    henka_scene_stars_desc stars;
    float time_of_day_hours;
    float day_length_seconds;
    float time_scale;
    bool time_of_day_enabled;
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
/* Validates material type, scalar, UV, alpha, and terrain-layer values without renderer-owned resources. */
henka_result henka_material_validate_values(const henka_material* material);
henka_result henka_material_validate(const henka_material* material);
henka_result henka_material_describe(const henka_material* material, char* buffer, size_t buffer_size);
henka_material henka_material_default(void);
henka_material henka_material_terrain_default(void);
henka_scene_environment_desc henka_scene_environment_default(void);
const char* henka_scene_environment_preset_get_label(
    henka_scene_environment_preset preset);
henka_result henka_scene_create(henka_scene** out_scene);
/* Creates an independent scene state while borrowing renderer-owned mesh,
 * texture, shader, and material-asset resources from the source scene. Entity
 * slot generations and public scene state are preserved, so generation-checked
 * handles remain meaningful in the clone. */
henka_result henka_scene_clone(
    const henka_scene* source,
    henka_scene** out_clone);
void henka_scene_destroy(henka_scene* scene);
/* Returns the monotonically changing scene render revision.  Any public scene
 * mutation that can change visible geometry, transforms, materials, bounds,
 * visibility, or render settings advances this revision.  Revision
 * exhaustion is a fail-closed limit: a mutation that would exceed the
 * revision watermark is rejected or ignored according to the mutator's
 * return contract, and the existing scene state remains live. */
uint64_t henka_scene_get_render_revision(const henka_scene* scene);
/* Reports whether a bounded caller-side transaction can perform the requested
 * number of visible scene mutations without exhausting either monotonic
 * revision watermark.  This is a preflight check; it does not reserve the
 * capacity or mutate the scene. */
bool henka_scene_has_render_revision_capacity(
    const henka_scene* scene,
    uint64_t mutation_count);
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
henka_result henka_scene_get_entity_local_transform(
    const henka_scene* scene,
    henka_entity entity,
    henka_transform* out_transform);
henka_result henka_scene_get_entity_world_transform(
    const henka_scene* scene,
    henka_entity entity,
    henka_transform* out_transform);
henka_result henka_scene_get_entity_parent(
    const henka_scene* scene,
    henka_entity entity,
    henka_entity* out_parent);
/* Enumerates direct children in deterministic storage order. Passing
 * HENKA_INVALID_ENTITY selects root entities. */
henka_result henka_scene_get_entity_child_count(
    const henka_scene* scene,
    henka_entity parent,
    size_t* out_count);
henka_result henka_scene_get_entity_child_at_index(
    const henka_scene* scene,
    henka_entity parent,
    size_t index,
    henka_entity* out_child);
/* Reparents a subtree transactionally. KEEP_LOCAL preserves the child's local
 * transform; KEEP_WORLD preserves its current world transform. The bounded
 * v1 TRS model rejects parent scales that would require shear representation. */
henka_result henka_scene_set_entity_parent(
    henka_scene* scene,
    henka_entity entity,
    henka_entity parent,
    henka_scene_parenting_mode mode);
henka_result henka_scene_set_entity_local_transform(
    henka_scene* scene,
    henka_entity entity,
    henka_transform transform);
henka_result henka_scene_get_entity_mesh(const henka_scene* scene, henka_entity entity, henka_mesh** out_mesh);
henka_result henka_scene_get_entity_material(const henka_scene* scene, henka_entity entity, henka_material* out_material);
/* Borrowed manager-owned definition identity, when the effective material
 * originated from an asset-manager material definition. */
henka_result henka_scene_get_entity_material_asset(
    const henka_scene* scene,
    henka_entity entity,
    const henka_material_asset** out_asset);
/* Returns the borrowed material definition revision and whether the entity's
 * effective material contains explicit overrides over that definition. */
henka_result henka_scene_get_entity_material_asset_state(
    const henka_scene* scene,
    henka_entity entity,
    uint64_t* out_revision,
    bool* out_overridden);
henka_result henka_scene_get_entity_local_bounds(const henka_scene* scene, henka_entity entity, henka_bounds* out_bounds);
henka_result henka_scene_get_entity_world_bounds(const henka_scene* scene, henka_entity entity, henka_bounds* out_bounds);
henka_result henka_scene_get_entity_interaction(const henka_scene* scene, henka_entity entity, henka_interaction_desc* out_interaction);
henka_result henka_scene_get_entity_flags(const henka_scene* scene, henka_entity entity, uint32_t* out_flags);
/* Returns the user-facing logical owner for a render child. New entities own
 * themselves until an importer or editor assigns a valid owner. */
henka_result henka_scene_get_entity_selection_owner(
    const henka_scene* scene,
    henka_entity entity,
    henka_entity* out_owner);
/* Assigns a valid scene entity as the logical selection owner for one render
 * entity. Ownership does not remove child identity or alter rendering. */
henka_result henka_scene_set_entity_selection_owner(
    henka_scene* scene,
    henka_entity entity,
    henka_entity owner);
henka_result henka_scene_set_entity_transform(henka_scene* scene, henka_entity entity, henka_transform transform);
/* Applies name, transform, visibility, interaction, and optionally inline
 * material state as one preflighted scene transaction. */
henka_result henka_scene_apply_entity_presentation(
    henka_scene* scene,
    henka_entity entity,
    const henka_scene_entity_presentation_update* update);
henka_result henka_scene_translate_entity(henka_scene* scene, henka_entity entity, henka_vec3 delta);
henka_result henka_scene_rotate_entity(henka_scene* scene, henka_entity entity, henka_quat delta_rotation);
henka_result henka_scene_scale_entity(henka_scene* scene, henka_entity entity, henka_vec3 scale_multiplier);
henka_result henka_scene_set_entity_mesh(henka_scene* scene, henka_entity entity, henka_mesh* mesh);
henka_result henka_scene_clear_entity_mesh(henka_scene* scene, henka_entity entity);
henka_result henka_scene_set_entity_material(henka_scene* scene, henka_entity entity, henka_material material);
/* Associates a borrowed manager-owned definition with the entity. Passing
 * NULL clears the association without changing the effective material. */
henka_result henka_scene_set_entity_material_asset(
    henka_scene* scene,
    henka_entity entity,
    const henka_material_asset* asset);
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
/* Identical camera values are a successful no-op and do not advance the scene revision. */
henka_result henka_scene_set_camera(henka_scene* scene, const henka_camera* camera);
henka_result henka_scene_get_camera(const henka_scene* scene, henka_camera* out_camera);
void henka_scene_set_light_direction(henka_scene* scene, henka_vec3 light_direction);
void henka_scene_set_light_color(henka_scene* scene, henka_vec3 light_color);
void henka_scene_set_light_intensity(henka_scene* scene, float light_intensity);
void henka_scene_set_ambient_color(henka_scene* scene, henka_vec3 ambient_color);
henka_result henka_scene_set_environment(
    henka_scene* scene,
    henka_scene_environment_desc environment);
/* Applies a bounded built-in environment starting point through the same
 * transactional environment setter. The preset is not retained as hidden
 * state; the resulting descriptor remains ordinarily editable and serializable. */
henka_result henka_scene_set_environment_preset(
    henka_scene* scene,
    henka_scene_environment_preset preset);
henka_result henka_scene_get_environment(
    const henka_scene* scene,
    henka_scene_environment_desc* out_environment);
/* Advances deterministic environment time without requiring a renderer. The
 * scene sun and direct-light state are updated transactionally. */
henka_result henka_scene_advance_environment_time(
    henka_scene* scene,
    float delta_seconds);
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
