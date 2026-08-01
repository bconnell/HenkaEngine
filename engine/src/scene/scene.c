#include "henka_internal.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>

#include "../core/checked.h"

static bool henka_is_finite_float(float value);

henka_material henka_material_default(void)
{
    henka_material material;

    material.name = "Material";
    material.type = HENKA_MATERIAL_TYPE_LIT;
    material.shader = NULL;
    material.base_color_texture = NULL;
    material.normal_texture = NULL;
    material.metallic_roughness_texture = NULL;
    material.occlusion_texture = NULL;
    material.emissive_texture = NULL;
    material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};
    material.emissive_color = (henka_vec3){0.0f, 0.0f, 0.0f};
    material.metallic = 0.0f;
    material.roughness = 0.5f;
    material.normal_scale = 1.0f;
    material.occlusion_strength = 1.0f;
    material.emissive_strength = 0.0f;
    material.alpha_cutoff = 0.5f;
    material.use_texture = false;
    material.use_lighting = true;
    material.depth_test = true;
    material.alpha_mode = HENKA_MATERIAL_ALPHA_OPAQUE;
    material.double_sided = false;
    material.cast_shadows = true;
    material.receive_shadows = true;
    return material;
}

const char* henka_material_type_get_label(henka_material_type type)
{
    switch (type)
    {
        case HENKA_MATERIAL_TYPE_UNLIT:
            return "Unlit";
        case HENKA_MATERIAL_TYPE_VERTEX_COLOR:
            return "Vertex Color";
        case HENKA_MATERIAL_TYPE_PROCEDURAL_RESERVED:
            return "Reserved Procedural";
        case HENKA_MATERIAL_TYPE_LIT:
        default:
            return "Lit";
    }
}

static bool henka_material_texture_matches(
    const henka_texture* texture,
    henka_texture_usage usage,
    henka_texture_color_space color_space)
{
    henka_texture_info info;

    if (texture == NULL)
    {
        return true;
    }
    if (henka_texture_get_info(texture, &info) != HENKA_SUCCESS ||
        !info.backend_ready || info.usage != usage || info.color_space != color_space)
    {
        return false;
    }
    return true;
}

henka_result henka_material_validate(const henka_material* material)
{
    if (material == NULL || material->shader == NULL ||
        material->type < HENKA_MATERIAL_TYPE_LIT ||
        material->type > HENKA_MATERIAL_TYPE_VERTEX_COLOR ||
        !henka_is_finite_float(material->base_color.x) ||
        !henka_is_finite_float(material->base_color.y) ||
        !henka_is_finite_float(material->base_color.z) ||
        !henka_is_finite_float(material->base_color.w) ||
        !henka_is_finite_float(material->emissive_color.x) ||
        !henka_is_finite_float(material->emissive_color.y) ||
        !henka_is_finite_float(material->emissive_color.z) ||
        !henka_is_finite_float(material->metallic) ||
        !henka_is_finite_float(material->roughness) ||
        !henka_is_finite_float(material->normal_scale) ||
        !henka_is_finite_float(material->occlusion_strength) ||
        !henka_is_finite_float(material->emissive_strength) ||
        !henka_is_finite_float(material->alpha_cutoff) ||
        material->base_color.x < 0.0f || material->base_color.x > 1.0f ||
        material->base_color.y < 0.0f || material->base_color.y > 1.0f ||
        material->base_color.z < 0.0f || material->base_color.z > 1.0f ||
        material->base_color.w < 0.0f || material->base_color.w > 1.0f ||
        material->emissive_color.x < 0.0f || material->emissive_color.y < 0.0f ||
        material->emissive_color.z < 0.0f || material->emissive_color.x > 1.0f ||
        material->emissive_color.y > 1.0f || material->emissive_color.z > 1.0f ||
        material->metallic < 0.0f ||
        material->metallic > 1.0f || material->roughness < 0.045f ||
        material->roughness > 1.0f || material->normal_scale < 0.0f ||
        material->normal_scale > 4.0f || material->occlusion_strength < 0.0f ||
        material->occlusion_strength > 1.0f || material->emissive_strength < 0.0f ||
        material->emissive_strength > 100.0f || material->alpha_cutoff < 0.0f ||
        material->alpha_cutoff > 1.0f ||
        material->alpha_mode > HENKA_MATERIAL_ALPHA_BLENDED ||
        (!material->use_texture && material->base_color_texture != NULL) ||
        (material->use_texture && material->base_color_texture == NULL) ||
        !henka_material_texture_matches(material->base_color_texture, HENKA_TEXTURE_USAGE_COLOR, HENKA_TEXTURE_COLOR_SPACE_SRGB) ||
        !henka_material_texture_matches(material->normal_texture, HENKA_TEXTURE_USAGE_NORMAL, HENKA_TEXTURE_COLOR_SPACE_LINEAR) ||
        !henka_material_texture_matches(material->metallic_roughness_texture, HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS, HENKA_TEXTURE_COLOR_SPACE_LINEAR) ||
        !henka_material_texture_matches(material->occlusion_texture, HENKA_TEXTURE_USAGE_OCCLUSION, HENKA_TEXTURE_COLOR_SPACE_LINEAR) ||
        !henka_material_texture_matches(material->emissive_texture, HENKA_TEXTURE_USAGE_EMISSIVE, HENKA_TEXTURE_COLOR_SPACE_SRGB))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_SUCCESS;
}

henka_result henka_material_describe(const henka_material* material, char* buffer, size_t buffer_size)
{
    const char* material_name;
    int written;

    if (material == NULL || buffer == NULL || buffer_size == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    material_name = (material->name != NULL && material->name[0] != '\0') ? material->name : "Material";
    written = snprintf(
        buffer,
        buffer_size,
        "%s | %s | %s%s | M %.2f R %.2f",
        material_name,
        henka_material_type_get_label(material->type),
        material->use_texture ? "Texture" : "Color",
        material->use_lighting ? " | Lit" : " | Unlit",
        material->metallic,
        material->roughness);
    if (written < 0 || (size_t)written >= buffer_size)
    {
        buffer[buffer_size - 1U] = '\0';
        return HENKA_ERROR_UNKNOWN;
    }

    return HENKA_SUCCESS;
}

#define HENKA_ENTITY_SLOT_BITS 21U
#define HENKA_ENTITY_SLOT_MASK \
    ((((henka_entity)1U) << HENKA_ENTITY_SLOT_BITS) - (henka_entity)1U)
#define HENKA_ENTITY_GENERATION_SHIFT HENKA_ENTITY_SLOT_BITS
#define HENKA_ENTITY_GENERATION_MASK \
    (UINT64_MAX >> HENKA_ENTITY_GENERATION_SHIFT)

static henka_entity henka_scene_make_entity(
    size_t index,
    uint64_t generation)
{
    const henka_entity slot = (henka_entity)index + (henka_entity)1U;

    if (index >= HENKA_MAX_SCENE_ENTITIES ||
        slot == HENKA_INVALID_ENTITY ||
        slot > HENKA_ENTITY_SLOT_MASK ||
        generation == 0U ||
        generation > HENKA_ENTITY_GENERATION_MASK)
    {
        return HENKA_INVALID_ENTITY;
    }

    return ((henka_entity)generation <<
            HENKA_ENTITY_GENERATION_SHIFT) |
        slot;
}

static bool henka_scene_decode_entity(
    henka_entity entity,
    size_t* out_index,
    uint64_t* out_generation)
{
    const henka_entity slot = entity & HENKA_ENTITY_SLOT_MASK;
    const uint64_t generation =
        (uint64_t)(entity >> HENKA_ENTITY_GENERATION_SHIFT);

    if (out_index != NULL)
    {
        *out_index = 0U;
    }
    if (out_generation != NULL)
    {
        *out_generation = 0U;
    }

    if (entity == HENKA_INVALID_ENTITY ||
        slot == HENKA_INVALID_ENTITY ||
        generation == 0U ||
        out_index == NULL ||
        out_generation == NULL)
    {
        return false;
    }

    *out_index = (size_t)(slot - (henka_entity)1U);
    *out_generation = generation;
    return true;
}

static void henka_scene_advance_entity_generation(
    henka_scene_entity_record* record)
{
    if (record == NULL)
    {
        return;
    }

    record->generation =
        (record->generation + 1U) &
        HENKA_ENTITY_GENERATION_MASK;
    if (record->generation == 0U)
    {
        record->generation = 1U;
    }
}

static henka_scene_entity_record* henka_scene_get_entity_record(
    henka_scene* scene,
    henka_entity entity)
{
    uint64_t generation;
    size_t index;

    if (scene == NULL ||
        !henka_scene_decode_entity(
            entity,
            &index,
            &generation) ||
        index >= scene->entity_capacity)
    {
        return NULL;
    }

    if (!scene->entities[index].active ||
        scene->entities[index].generation != generation)
    {
        return NULL;
    }

    return &scene->entities[index];
}

static henka_result henka_scene_duplicate_text(const char* value, char** out_copy)
{
    size_t allocation_size;
    char* copy;
    size_t length;

    if (out_copy == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_copy = NULL;
    if (value == NULL)
    {
        return HENKA_SUCCESS;
    }

    if (!henka_checked_c_string_length(value, HENKA_MAX_SCENE_TEXT_BYTES, &length) ||
        !henka_checked_size_add(length, 1U, &allocation_size))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    copy = henka_malloc(allocation_size);
    if (copy == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    memcpy(copy, value, allocation_size);
    *out_copy = copy;
    return HENKA_SUCCESS;
}

static bool henka_scene_double_fits_float(double value)
{
    return isfinite(value) && value >= -(double)FLT_MAX && value <= (double)FLT_MAX;
}

static bool henka_scene_try_transform_bounds(
    henka_bounds local_bounds,
    henka_transform transform,
    henka_bounds* out_world_bounds)
{
    double center_x;
    double center_y;
    double center_z;
    double extent_x;
    double extent_y;
    double extent_z;
    double scaled_center_x;
    double scaled_center_y;
    double scaled_center_z;
    double scaled_extent_x;
    double scaled_extent_y;
    double scaled_extent_z;
    henka_mat4 rotation;

    if (out_world_bounds == NULL)
    {
        return false;
    }

    *out_world_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    rotation = henka_mat4_rotation(transform.rotation);
    scaled_center_x = (double)local_bounds.center.x * (double)transform.scale.x;
    scaled_center_y = (double)local_bounds.center.y * (double)transform.scale.y;
    scaled_center_z = (double)local_bounds.center.z * (double)transform.scale.z;
    scaled_extent_x = fabs((double)local_bounds.extents.x * (double)transform.scale.x);
    scaled_extent_y = fabs((double)local_bounds.extents.y * (double)transform.scale.y);
    scaled_extent_z = fabs((double)local_bounds.extents.z * (double)transform.scale.z);

    center_x = (double)transform.position.x +
        (double)rotation.m[0] * scaled_center_x +
        (double)rotation.m[4] * scaled_center_y +
        (double)rotation.m[8] * scaled_center_z;
    center_y = (double)transform.position.y +
        (double)rotation.m[1] * scaled_center_x +
        (double)rotation.m[5] * scaled_center_y +
        (double)rotation.m[9] * scaled_center_z;
    center_z = (double)transform.position.z +
        (double)rotation.m[2] * scaled_center_x +
        (double)rotation.m[6] * scaled_center_y +
        (double)rotation.m[10] * scaled_center_z;
    extent_x =
        fabs((double)rotation.m[0]) * scaled_extent_x +
        fabs((double)rotation.m[4]) * scaled_extent_y +
        fabs((double)rotation.m[8]) * scaled_extent_z;
    extent_y =
        fabs((double)rotation.m[1]) * scaled_extent_x +
        fabs((double)rotation.m[5]) * scaled_extent_y +
        fabs((double)rotation.m[9]) * scaled_extent_z;
    extent_z =
        fabs((double)rotation.m[2]) * scaled_extent_x +
        fabs((double)rotation.m[6]) * scaled_extent_y +
        fabs((double)rotation.m[10]) * scaled_extent_z;

    if (!henka_scene_double_fits_float(center_x) ||
        !henka_scene_double_fits_float(center_y) ||
        !henka_scene_double_fits_float(center_z) ||
        !henka_scene_double_fits_float(extent_x) ||
        !henka_scene_double_fits_float(extent_y) ||
        !henka_scene_double_fits_float(extent_z) ||
        !henka_scene_double_fits_float(center_x - extent_x) ||
        !henka_scene_double_fits_float(center_x + extent_x) ||
        !henka_scene_double_fits_float(center_y - extent_y) ||
        !henka_scene_double_fits_float(center_y + extent_y) ||
        !henka_scene_double_fits_float(center_z - extent_z) ||
        !henka_scene_double_fits_float(center_z + extent_z))
    {
        return false;
    }

    out_world_bounds->center = (henka_vec3){
        (float)center_x,
        (float)center_y,
        (float)center_z};
    out_world_bounds->extents = (henka_vec3){
        (float)extent_x,
        (float)extent_y,
        (float)extent_z};
    return true;
}

static const float g_henka_scene_minimum_scale_magnitude = 0.01f;

static bool henka_is_finite_float(float value)
{
    return isfinite(value) != 0;
}

static bool henka_scale_component_is_valid(float value)
{
    return henka_is_finite_float(value) &&
        fabsf(value) >= g_henka_scene_minimum_scale_magnitude;
}

static bool henka_scale_vector_is_valid(henka_vec3 value)
{
    return henka_scale_component_is_valid(value.x) &&
        henka_scale_component_is_valid(value.y) &&
        henka_scale_component_is_valid(value.z);
}

static bool henka_scene_vec3_is_finite(henka_vec3 value)
{
    return henka_is_finite_float(value.x) &&
        henka_is_finite_float(value.y) &&
        henka_is_finite_float(value.z);
}

static bool henka_scene_bounds_are_valid(henka_bounds bounds)
{
    return henka_scene_vec3_is_finite(bounds.center) &&
        henka_scene_vec3_is_finite(bounds.extents) &&
        bounds.extents.x >= 0.0f &&
        bounds.extents.y >= 0.0f &&
        bounds.extents.z >= 0.0f;
}

static bool henka_scene_material_is_valid(henka_material material)
{
    return henka_material_validate(&material) == HENKA_SUCCESS;
}

static bool henka_transform_is_valid(henka_transform transform)
{
    return henka_is_finite_float(transform.position.x) &&
        henka_is_finite_float(transform.position.y) &&
        henka_is_finite_float(transform.position.z) &&
        henka_is_finite_float(transform.rotation.x) &&
        henka_is_finite_float(transform.rotation.y) &&
        henka_is_finite_float(transform.rotation.z) &&
        henka_is_finite_float(transform.rotation.w) &&
        henka_scale_vector_is_valid(transform.scale);
}

static henka_transform henka_transform_sanitize(henka_transform transform)
{
    transform.rotation = henka_quat_normalize(transform.rotation);
    return transform;
}

static bool henka_scene_ray_intersects_bounds(henka_ray ray, henka_bounds bounds, float* out_distance)
{
    const float epsilon = 0.00001f;
    henka_vec3 minimum;
    henka_vec3 maximum;
    float tmin;
    float tmax;
    int axis;

    minimum = henka_vec3_subtract(bounds.center, bounds.extents);
    maximum = henka_vec3_add(bounds.center, bounds.extents);
    tmin = 0.0f;
    tmax = 1000000.0f;

    for (axis = 0; axis < 3; ++axis)
    {
        const float origin = axis == 0 ? ray.origin.x : (axis == 1 ? ray.origin.y : ray.origin.z);
        const float direction = axis == 0 ? ray.direction.x : (axis == 1 ? ray.direction.y : ray.direction.z);
        const float min_value = axis == 0 ? minimum.x : (axis == 1 ? minimum.y : minimum.z);
        const float max_value = axis == 0 ? maximum.x : (axis == 1 ? maximum.y : maximum.z);

        if (fabsf(direction) < epsilon)
        {
            if (origin < min_value || origin > max_value)
            {
                return false;
            }

            continue;
        }

        {
            float t0 = (min_value - origin) / direction;
            float t1 = (max_value - origin) / direction;
            float near_value = t0 < t1 ? t0 : t1;
            float far_value = t0 > t1 ? t0 : t1;

            if (near_value > tmin)
            {
                tmin = near_value;
            }

            if (far_value < tmax)
            {
                tmax = far_value;
            }

            if (tmax < tmin)
            {
                return false;
            }
        }
    }

    if (out_distance != NULL)
    {
        *out_distance = tmin >= 0.0f ? tmin : tmax;
    }

    return tmax >= 0.0f;
}

static const henka_scene_entity_record* henka_scene_get_entity_record_const(const henka_scene* scene, henka_entity entity)
{
    return henka_scene_get_entity_record((henka_scene*)scene, entity);
}

static henka_result henka_scene_grow(henka_scene* scene)
{
    size_t allocation_size;
    size_t index;
    size_t new_capacity;
    henka_scene_entity_record* new_entities;
    size_t required;

    if (scene == NULL ||
        !henka_checked_size_add(scene->entity_capacity, 1U, &required) ||
        !henka_checked_capacity(
            scene->entity_capacity,
            required,
            8U,
            HENKA_MAX_SCENE_ENTITIES,
            &new_capacity) ||
        !henka_checked_size_multiply(new_capacity, sizeof(*new_entities), &allocation_size))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    new_entities = henka_realloc(scene->entities, allocation_size);
    if (new_entities == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    for (index = scene->entity_capacity; index < new_capacity; ++index)
    {
        new_entities[index].active = false;
        new_entities[index].generation = 1U;
        new_entities[index].visible = true;
        new_entities[index].flags = HENKA_SCENE_ENTITY_FLAG_NONE;
        new_entities[index].name = NULL;
        new_entities[index].tag = NULL;
        new_entities[index].transform = henka_transform_identity();
        new_entities[index].mesh = NULL;
        new_entities[index].material = henka_material_default();
        new_entities[index].material_name = NULL;
        new_entities[index].has_local_bounds = false;
        new_entities[index].local_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
        new_entities[index].interaction = (henka_interaction_desc){false, 2.0f, NULL};
        new_entities[index].interaction_prompt = NULL;
    }

    scene->entities = new_entities;
    scene->entity_capacity = new_capacity;
    return HENKA_SUCCESS;
}

henka_result henka_scene_create(henka_scene** out_scene)
{
    henka_scene* scene;

    if (out_scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_scene = NULL;

    scene = henka_calloc(1U, sizeof(*scene));
    if (scene == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    scene->light_direction.x = -0.4f;
    scene->light_direction.y = -1.0f;
    scene->light_direction.z = -0.2f;
    scene->light_color = (henka_vec3){1.0f, 0.96f, 0.90f};
    scene->light_intensity = 3.0f;
    scene->ambient_color.x = 0.16f;
    scene->ambient_color.y = 0.18f;
    scene->ambient_color.z = 0.22f;

    *out_scene = scene;
    return HENKA_SUCCESS;
}

void henka_scene_destroy(henka_scene* scene)
{
    size_t index;

    if (scene == NULL)
    {
        return;
    }

    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        henka_free(scene->entities[index].name);
        henka_free(scene->entities[index].tag);
        henka_free(scene->entities[index].material_name);
        henka_free(scene->entities[index].interaction_prompt);
    }

    henka_free(scene->entities);
    henka_free(scene);
}

henka_entity henka_scene_create_entity(henka_scene* scene)
{
    return henka_scene_create_entity_named(scene, NULL);
}

henka_entity henka_scene_create_entity_named(henka_scene* scene, const char* name)
{
    size_t index;
    char* copy;
    henka_result copy_result;

    if (scene == NULL || scene->entity_count >= HENKA_MAX_SCENE_ENTITIES)
    {
        return HENKA_INVALID_ENTITY;
    }

    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        if (!scene->entities[index].active)
        {
            scene->entities[index].active = true;
            scene->entities[index].visible = true;
            scene->entities[index].flags = HENKA_SCENE_ENTITY_FLAG_NONE;
            scene->entities[index].transform = henka_transform_identity();
            scene->entities[index].mesh = NULL;
            scene->entities[index].material = henka_material_default();
            henka_free(scene->entities[index].name);
            henka_free(scene->entities[index].tag);
            henka_free(scene->entities[index].material_name);
            henka_free(scene->entities[index].interaction_prompt);
            scene->entities[index].name = NULL;
            scene->entities[index].tag = NULL;
            scene->entities[index].material_name = NULL;
            scene->entities[index].interaction_prompt = NULL;
            scene->entities[index].has_local_bounds = false;
            scene->entities[index].local_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}};
            scene->entities[index].interaction = (henka_interaction_desc){false, 2.0f, NULL};

            copy_result = henka_scene_duplicate_text(name, &copy);
            if (copy_result != HENKA_SUCCESS)
            {
                scene->entities[index].active = false;
                return HENKA_INVALID_ENTITY;
            }

            scene->entities[index].name = copy;
            scene->entity_count += 1U;
            return henka_scene_make_entity(
                index,
                scene->entities[index].generation);
        }
    }

    if (henka_scene_grow(scene) != HENKA_SUCCESS)
    {
        return HENKA_INVALID_ENTITY;
    }

    scene->entities[scene->entity_count].active = true;
    scene->entities[scene->entity_count].visible = true;
    scene->entities[scene->entity_count].flags = HENKA_SCENE_ENTITY_FLAG_NONE;
    scene->entities[scene->entity_count].transform = henka_transform_identity();
    scene->entities[scene->entity_count].material = henka_material_default();
    scene->entities[scene->entity_count].has_local_bounds = false;
    scene->entities[scene->entity_count].local_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}};
    scene->entities[scene->entity_count].interaction = (henka_interaction_desc){false, 2.0f, NULL};
    copy_result = henka_scene_duplicate_text(name, &copy);
    if (copy_result != HENKA_SUCCESS)
    {
        scene->entities[scene->entity_count].active = false;
        return HENKA_INVALID_ENTITY;
    }

    scene->entities[scene->entity_count].name = copy;
    scene->entity_count += 1U;
    return henka_scene_make_entity(
        scene->entity_count - 1U,
        scene->entities[scene->entity_count - 1U].generation);
}

void henka_scene_destroy_entity(henka_scene* scene, henka_entity entity)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return;
    }

    record->active = false;
    henka_scene_advance_entity_generation(record);
    record->visible = true;
    record->flags = HENKA_SCENE_ENTITY_FLAG_NONE;
    record->mesh = NULL;
    record->material = henka_material_default();
    henka_free(record->name);
    henka_free(record->tag);
    henka_free(record->material_name);
    henka_free(record->interaction_prompt);
    record->name = NULL;
    record->tag = NULL;
    record->material_name = NULL;
    record->interaction_prompt = NULL;
    record->has_local_bounds = false;
    record->interaction = (henka_interaction_desc){false, 2.0f, NULL};
    if (scene->entity_count > 0U)
    {
        scene->entity_count -= 1U;
    }
}

bool henka_scene_is_entity_valid(const henka_scene* scene, henka_entity entity)
{
    return henka_scene_get_entity_record_const(scene, entity) != NULL;
}

bool henka_scene_is_entity_visible(const henka_scene* scene, henka_entity entity)
{
    const henka_scene_entity_record* record;

    record = henka_scene_get_entity_record_const(scene, entity);
    return record != NULL ? record->visible : false;
}

size_t henka_scene_get_entity_count(const henka_scene* scene)
{
    if (scene == NULL)
    {
        return 0U;
    }

    return scene->entity_count;
}

henka_entity henka_scene_get_entity_at_index(const henka_scene* scene, size_t index)
{
    size_t active_index;
    size_t entity_index;

    if (scene == NULL)
    {
        return HENKA_INVALID_ENTITY;
    }

    active_index = 0U;
    for (entity_index = 0U; entity_index < scene->entity_capacity; ++entity_index)
    {
        if (!scene->entities[entity_index].active)
        {
            continue;
        }

        if (active_index == index)
        {
            return henka_scene_make_entity(
                entity_index,
                scene->entities[entity_index].generation);
        }

        active_index += 1U;
    }

    return HENKA_INVALID_ENTITY;
}

const char* henka_scene_get_entity_name(const henka_scene* scene, henka_entity entity)
{
    const henka_scene_entity_record* record;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return NULL;
    }

    return record->name;
}

const char* henka_scene_get_entity_tag(const henka_scene* scene, henka_entity entity)
{
    const henka_scene_entity_record* record;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return NULL;
    }

    return record->tag;
}

henka_result henka_scene_find_entity_by_name(const henka_scene* scene, const char* name, henka_entity* out_entity)
{
    size_t index;

    if (scene == NULL || name == NULL || name[0] == '\0' || out_entity == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_entity = HENKA_INVALID_ENTITY;

    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        if (!scene->entities[index].active || scene->entities[index].name == NULL)
        {
            continue;
        }

        if (strcmp(scene->entities[index].name, name) == 0)
        {
            *out_entity = henka_scene_make_entity(
                index,
                scene->entities[index].generation);
            return HENKA_SUCCESS;
        }
    }

    return HENKA_ERROR_UNKNOWN;
}

henka_result henka_scene_find_entity_by_tag(const henka_scene* scene, const char* tag, henka_entity* out_entity)
{
    size_t index;

    if (scene == NULL || tag == NULL || tag[0] == '\0' || out_entity == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_entity = HENKA_INVALID_ENTITY;
    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        if (!scene->entities[index].active || scene->entities[index].tag == NULL)
        {
            continue;
        }

        if (strcmp(scene->entities[index].tag, tag) == 0)
        {
            *out_entity = henka_scene_make_entity(
                index,
                scene->entities[index].generation);
            return HENKA_SUCCESS;
        }
    }

    return HENKA_ERROR_UNKNOWN;
}

henka_result henka_scene_get_entity_info(const henka_scene* scene, henka_entity entity, henka_scene_object_info* out_info)
{
    const henka_scene_entity_record* record;

    if (out_info == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_info->entity = entity;
    out_info->name = record->name;
    out_info->tag = record->tag;
    out_info->visible = record->visible;
    out_info->has_bounds = record->has_local_bounds;
    out_info->local_bounds = record->local_bounds;
    out_info->transform = record->transform;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_transform(const henka_scene* scene, henka_entity entity, henka_transform* out_transform)
{
    const henka_scene_entity_record* record;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL || out_transform == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_transform = record->transform;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_mesh(const henka_scene* scene, henka_entity entity, henka_mesh** out_mesh)
{
    const henka_scene_entity_record* record;

    if (out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_mesh = record->mesh;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_material(const henka_scene* scene, henka_entity entity, henka_material* out_material)
{
    const henka_scene_entity_record* record;

    if (out_material == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_material = record->material;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_local_bounds(const henka_scene* scene, henka_entity entity, henka_bounds* out_bounds)
{
    const henka_scene_entity_record* record;

    if (out_bounds == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL || !record->has_local_bounds)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_bounds = record->local_bounds;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_world_bounds(const henka_scene* scene, henka_entity entity, henka_bounds* out_bounds)
{
    const henka_scene_entity_record* record;

    if (out_bounds == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL || !record->has_local_bounds ||
        !henka_scene_try_transform_bounds(record->local_bounds, record->transform, out_bounds))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_interaction(const henka_scene* scene, henka_entity entity, henka_interaction_desc* out_interaction)
{
    const henka_scene_entity_record* record;

    if (out_interaction == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_interaction = record->interaction;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_flags(const henka_scene* scene, henka_entity entity, uint32_t* out_flags)
{
    const henka_scene_entity_record* record;

    if (out_flags == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_flags = record->flags;
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_transform(henka_scene* scene, henka_entity entity, henka_transform transform)
{
    henka_scene_entity_record* record;
    henka_transform sanitized_transform;
    henka_bounds world_bounds;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || !henka_transform_is_valid(transform))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    sanitized_transform = henka_transform_sanitize(transform);
    if (record->has_local_bounds &&
        !henka_scene_try_transform_bounds(record->local_bounds, sanitized_transform, &world_bounds))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->transform = sanitized_transform;
    return HENKA_SUCCESS;
}

henka_result henka_scene_translate_entity(henka_scene* scene, henka_entity entity, henka_vec3 delta)
{
    henka_transform transform;

    if (!henka_is_finite_float(delta.x) || !henka_is_finite_float(delta.y) || !henka_is_finite_float(delta.z))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_scene_get_entity_transform(scene, entity, &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    transform.position = henka_vec3_add(transform.position, delta);
    return henka_scene_set_entity_transform(scene, entity, transform);
}

henka_result henka_scene_rotate_entity(henka_scene* scene, henka_entity entity, henka_quat delta_rotation)
{
    henka_transform transform;

    if (!henka_is_finite_float(delta_rotation.x) ||
        !henka_is_finite_float(delta_rotation.y) ||
        !henka_is_finite_float(delta_rotation.z) ||
        !henka_is_finite_float(delta_rotation.w))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_scene_get_entity_transform(scene, entity, &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    transform.rotation = henka_quat_multiply(delta_rotation, transform.rotation);
    return henka_scene_set_entity_transform(scene, entity, transform);
}

henka_result henka_scene_scale_entity(henka_scene* scene, henka_entity entity, henka_vec3 scale_multiplier)
{
    henka_transform transform;

    if (!henka_is_finite_float(scale_multiplier.x) ||
        !henka_is_finite_float(scale_multiplier.y) ||
        !henka_is_finite_float(scale_multiplier.z))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_scene_get_entity_transform(scene, entity, &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    transform.scale.x *= scale_multiplier.x;
    transform.scale.y *= scale_multiplier.y;
    transform.scale.z *= scale_multiplier.z;
    return henka_scene_set_entity_transform(scene, entity, transform);
}

henka_result henka_scene_set_entity_mesh(henka_scene* scene, henka_entity entity, henka_mesh* mesh)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->mesh = mesh;
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_material(henka_scene* scene, henka_entity entity, henka_material material)
{
    char* material_name;
    henka_scene_entity_record* record;
    henka_result result;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || !henka_scene_material_is_valid(material))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    material_name = NULL;
    if (material.name != NULL && material.name[0] != '\0')
    {
        result = henka_scene_duplicate_text(material.name, &material_name);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }

    henka_free(record->material_name);
    record->material_name = material_name;
    record->material = material;
    record->material.name = material_name != NULL ? material_name : "Material";
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_name(henka_scene* scene, henka_entity entity, const char* name)
{
    char* copy;
    henka_scene_entity_record* record;
    henka_result result;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_scene_duplicate_text(name, &copy);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    henka_free(record->name);
    record->name = copy;
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_tag(henka_scene* scene, henka_entity entity, const char* tag)
{
    char* copy;
    henka_scene_entity_record* record;
    henka_result result;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_scene_duplicate_text(tag, &copy);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    henka_free(record->tag);
    record->tag = copy;
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_visible(henka_scene* scene, henka_entity entity, bool visible)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->visible = visible;
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_local_bounds(henka_scene* scene, henka_entity entity, henka_bounds bounds)
{
    henka_scene_entity_record* record;
    henka_bounds world_bounds;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || !henka_scene_bounds_are_valid(bounds) ||
        !henka_scene_try_transform_bounds(bounds, record->transform, &world_bounds))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->local_bounds = bounds;
    record->has_local_bounds = true;
    return HENKA_SUCCESS;
}

henka_result henka_scene_clear_entity_local_bounds(henka_scene* scene, henka_entity entity)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->has_local_bounds = false;
    record->local_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_interaction(henka_scene* scene, henka_entity entity, const henka_interaction_desc* interaction)
{
    henka_scene_entity_record* record;
    char* prompt_copy;
    henka_result result;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || interaction == NULL ||
        !henka_is_finite_float(interaction->max_distance) ||
        interaction->max_distance < 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_scene_duplicate_text(interaction->prompt, &prompt_copy);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    henka_free(record->interaction_prompt);
    record->interaction_prompt = prompt_copy;
    record->interaction = *interaction;
    record->interaction.prompt = prompt_copy;
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_flags(henka_scene* scene, henka_entity entity, uint32_t flags)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->flags = flags;
    return HENKA_SUCCESS;
}

bool henka_scene_is_entity_helper(const henka_scene* scene, henka_entity entity)
{
    const henka_scene_entity_record* record;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return false;
    }

    return (record->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U;
}

bool henka_scene_is_entity_transform_locked(const henka_scene* scene, henka_entity entity)
{
    const henka_scene_entity_record* record;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return false;
    }

    return (record->flags & HENKA_SCENE_ENTITY_FLAG_TRANSFORM_LOCKED) != 0U;
}

henka_interaction_result henka_scene_can_interact(const henka_scene* scene, henka_entity entity, henka_vec3 observer_position)
{
    const henka_scene_entity_record* record;
    float distance;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL || !henka_scene_vec3_is_finite(observer_position))
    {
        return HENKA_INTERACTION_RESULT_UNAVAILABLE;
    }

    if (!record->interaction.enabled)
    {
        return HENKA_INTERACTION_RESULT_DISABLED;
    }

    distance = henka_vec3_length(henka_vec3_subtract(record->transform.position, observer_position));
    if (distance > record->interaction.max_distance)
    {
        return HENKA_INTERACTION_RESULT_OUT_OF_RANGE;
    }

    return HENKA_INTERACTION_RESULT_AVAILABLE;
}

henka_result henka_scene_pick_entity(const henka_scene* scene, henka_ray ray, henka_entity* out_entity, float* out_distance)
{
    float best_distance;
    size_t index;
    bool found;

    if (out_entity == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_entity = HENKA_INVALID_ENTITY;
    if (out_distance != NULL)
    {
        *out_distance = 0.0f;
    }

    if (scene == NULL ||
        !henka_scene_vec3_is_finite(ray.origin) ||
        !henka_scene_vec3_is_finite(ray.direction))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    ray.direction = henka_vec3_normalize(ray.direction);
    if (henka_vec3_length(ray.direction) <= 0.000001f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    best_distance = 1000000.0f;
    found = false;
    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        henka_bounds world_bounds;
        float distance;
        const henka_scene_entity_record* record = &scene->entities[index];

        if (!record->active || !record->visible || !record->has_local_bounds || (record->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U)
        {
            continue;
        }

        if (!henka_scene_try_transform_bounds(record->local_bounds, record->transform, &world_bounds) ||
            !henka_scene_ray_intersects_bounds(ray, world_bounds, &distance))
        {
            continue;
        }

        if (!found || distance < best_distance)
        {
            best_distance = distance;
            *out_entity = henka_scene_make_entity(
                index,
                scene->entities[index].generation);
            found = true;
        }
    }

    if (!found)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    if (out_distance != NULL)
    {
        *out_distance = best_distance;
    }

    return HENKA_SUCCESS;
}

henka_result henka_scene_set_camera(henka_scene* scene, const henka_camera* camera)
{
    if (scene == NULL || !henka_camera_is_valid(camera))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    scene->camera = *camera;
    scene->has_camera = true;
    return HENKA_SUCCESS;
}

void henka_scene_set_light_direction(henka_scene* scene, henka_vec3 light_direction)
{
    henka_vec3 normalized_direction;

    if (scene == NULL ||
        !henka_is_finite_float(light_direction.x) ||
        !henka_is_finite_float(light_direction.y) ||
        !henka_is_finite_float(light_direction.z))
    {
        return;
    }

    normalized_direction = henka_vec3_normalize(light_direction);
    if (henka_vec3_length(normalized_direction) <= 0.000001f)
    {
        return;
    }

    scene->light_direction = normalized_direction;
}

void henka_scene_set_light_color(henka_scene* scene, henka_vec3 light_color)
{
    if (scene != NULL && henka_is_finite_float(light_color.x) &&
        henka_is_finite_float(light_color.y) && henka_is_finite_float(light_color.z) &&
        light_color.x >= 0.0f && light_color.y >= 0.0f && light_color.z >= 0.0f &&
        light_color.x <= 1.0f && light_color.y <= 1.0f && light_color.z <= 1.0f)
    {
        scene->light_color = light_color;
    }
}

void henka_scene_set_light_intensity(henka_scene* scene, float light_intensity)
{
    if (scene != NULL && henka_is_finite_float(light_intensity) && light_intensity >= 0.0f && light_intensity <= 10000.0f)
    {
        scene->light_intensity = light_intensity;
    }
}

void henka_scene_set_ambient_color(henka_scene* scene, henka_vec3 ambient_color)
{
    if (scene != NULL &&
        henka_is_finite_float(ambient_color.x) &&
        henka_is_finite_float(ambient_color.y) &&
        henka_is_finite_float(ambient_color.z) &&
        ambient_color.x >= 0.0f && ambient_color.y >= 0.0f && ambient_color.z >= 0.0f &&
        ambient_color.x <= 16.0f && ambient_color.y <= 16.0f && ambient_color.z <= 16.0f)
    {
        scene->ambient_color = ambient_color;
    }
}
