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
typedef uint32_t henka_entity;

#define HENKA_INVALID_ENTITY ((henka_entity)0)

typedef enum henka_material_type
{
    HENKA_MATERIAL_TYPE_LIT = 0,
    HENKA_MATERIAL_TYPE_UNLIT,
    HENKA_MATERIAL_TYPE_VERTEX_COLOR,
    HENKA_MATERIAL_TYPE_PROCEDURAL_PLACEHOLDER
} henka_material_type;

typedef struct henka_material
{
    const char* name;
    henka_material_type type;
    henka_shader* shader;
    henka_texture* base_color_texture;
    henka_vec4 base_color;
    bool use_texture;
    bool use_lighting;
    bool depth_test;
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

typedef enum henka_scene_entity_flags
{
    HENKA_SCENE_ENTITY_FLAG_NONE = 0,
    HENKA_SCENE_ENTITY_FLAG_HELPER = 1 << 0
} henka_scene_entity_flags;

typedef enum henka_interaction_result
{
    HENKA_INTERACTION_RESULT_UNAVAILABLE = 0,
    HENKA_INTERACTION_RESULT_DISABLED,
    HENKA_INTERACTION_RESULT_OUT_OF_RANGE,
    HENKA_INTERACTION_RESULT_AVAILABLE
} henka_interaction_result;

const char* henka_material_type_get_label(henka_material_type type);
henka_result henka_material_describe(const henka_material* material, char* buffer, size_t buffer_size);
henka_material henka_material_default(void);
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
henka_interaction_result henka_scene_can_interact(const henka_scene* scene, henka_entity entity, henka_vec3 observer_position);
henka_result henka_scene_pick_entity(const henka_scene* scene, henka_ray ray, henka_entity* out_entity, float* out_distance);
henka_result henka_scene_set_camera(henka_scene* scene, const henka_camera* camera);
void henka_scene_set_light_direction(henka_scene* scene, henka_vec3 light_direction);
void henka_scene_set_ambient_color(henka_scene* scene, henka_vec3 ambient_color);

#endif
