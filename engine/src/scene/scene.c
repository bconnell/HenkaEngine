#include "henka_internal.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>

henka_material henka_material_default(void)
{
    henka_material material;

    material.name = "Material";
    material.type = HENKA_MATERIAL_TYPE_LIT;
    material.shader = NULL;
    material.base_color_texture = NULL;
    material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};
    material.use_texture = false;
    material.use_lighting = true;
    material.depth_test = true;
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
        case HENKA_MATERIAL_TYPE_PROCEDURAL_PLACEHOLDER:
            return "Procedural Placeholder";
        case HENKA_MATERIAL_TYPE_LIT:
        default:
            return "Lit";
    }
}

henka_result henka_material_describe(const henka_material* material, char* buffer, size_t buffer_size)
{
    const char* material_name;

    if (material == NULL || buffer == NULL || buffer_size == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    material_name = (material->name != NULL && material->name[0] != '\0') ? material->name : "Material";
    if (snprintf(
        buffer,
        buffer_size,
        "%s | %s | %s%s",
        material_name,
        henka_material_type_get_label(material->type),
        material->use_texture ? "Texture" : "Color",
        material->use_lighting ? " | Lit" : " | Unlit") < 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    return HENKA_SUCCESS;
}

static henka_scene_entity_record* henka_scene_get_entity_record(henka_scene* scene, henka_entity entity)
{
    size_t index;

    if (scene == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return NULL;
    }

    index = (size_t)(entity - 1U);
    if (index >= scene->entity_capacity)
    {
        return NULL;
    }

    if (!scene->entities[index].active)
    {
        return NULL;
    }

    return &scene->entities[index];
}

static char* henka_scene_duplicate_string(const char* value)
{
    char* copy;
    size_t length;

    if (value == NULL)
    {
        return NULL;
    }

    length = strlen(value);
    copy = henka_malloc(length + 1U);
    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(copy, value, length + 1U);
    return copy;
}

static henka_bounds henka_scene_transform_bounds(henka_bounds local_bounds, henka_transform transform)
{
    henka_bounds world_bounds;
    henka_mat4 rotation;
    henka_vec3 scaled_center;
    henka_vec3 scaled_extents;
    henka_vec3 rotated_center;

    scaled_center = (henka_vec3)
    {
        local_bounds.center.x * transform.scale.x,
        local_bounds.center.y * transform.scale.y,
        local_bounds.center.z * transform.scale.z
    };
    scaled_extents = (henka_vec3)
    {
        fabsf(local_bounds.extents.x * transform.scale.x),
        fabsf(local_bounds.extents.y * transform.scale.y),
        fabsf(local_bounds.extents.z * transform.scale.z)
    };
    rotated_center = henka_quat_rotate_vec3(transform.rotation, scaled_center);
    rotation = henka_mat4_rotation(transform.rotation);

    world_bounds.center = henka_vec3_add(transform.position, rotated_center);
    world_bounds.extents.x =
        fabsf(rotation.m[0]) * scaled_extents.x +
        fabsf(rotation.m[4]) * scaled_extents.y +
        fabsf(rotation.m[8]) * scaled_extents.z;
    world_bounds.extents.y =
        fabsf(rotation.m[1]) * scaled_extents.x +
        fabsf(rotation.m[5]) * scaled_extents.y +
        fabsf(rotation.m[9]) * scaled_extents.z;
    world_bounds.extents.z =
        fabsf(rotation.m[2]) * scaled_extents.x +
        fabsf(rotation.m[6]) * scaled_extents.y +
        fabsf(rotation.m[10]) * scaled_extents.z;
    return world_bounds;
}

static bool henka_is_finite_float(float value)
{
    return isfinite(value) != 0;
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
        henka_is_finite_float(transform.scale.x) &&
        henka_is_finite_float(transform.scale.y) &&
        henka_is_finite_float(transform.scale.z);
}

static henka_transform henka_transform_sanitize(henka_transform transform)
{
    const float minimum_scale = 0.01f;

    transform.rotation = henka_quat_normalize(transform.rotation);
    if (transform.scale.x < minimum_scale)
    {
        transform.scale.x = minimum_scale;
    }
    if (transform.scale.y < minimum_scale)
    {
        transform.scale.y = minimum_scale;
    }
    if (transform.scale.z < minimum_scale)
    {
        transform.scale.z = minimum_scale;
    }
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
    henka_scene_entity_record* new_entities;
    size_t new_capacity;
    size_t index;

    new_capacity = scene->entity_capacity == 0U ? 8U : scene->entity_capacity * 2U;
    new_entities = henka_realloc(scene->entities, new_capacity * sizeof(*new_entities));
    if (new_entities == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    for (index = scene->entity_capacity; index < new_capacity; ++index)
    {
        new_entities[index].active = false;
        new_entities[index].visible = true;
        new_entities[index].name = NULL;
        new_entities[index].tag = NULL;
        new_entities[index].transform = henka_transform_identity();
        new_entities[index].mesh = NULL;
        new_entities[index].material = henka_material_default();
        new_entities[index].has_local_bounds = false;
        new_entities[index].local_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
        new_entities[index].interaction = (henka_interaction_desc){false, 2.0f, NULL};
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

    if (scene == NULL)
    {
        return HENKA_INVALID_ENTITY;
    }

    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        if (!scene->entities[index].active)
        {
            scene->entities[index].active = true;
            scene->entities[index].visible = true;
            scene->entities[index].transform = henka_transform_identity();
            scene->entities[index].mesh = NULL;
            scene->entities[index].material = henka_material_default();
            henka_free(scene->entities[index].name);
            henka_free(scene->entities[index].tag);
            scene->entities[index].name = NULL;
            scene->entities[index].tag = NULL;
            scene->entities[index].has_local_bounds = false;
            scene->entities[index].local_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}};
            scene->entities[index].interaction = (henka_interaction_desc){false, 2.0f, NULL};

            if (name != NULL)
            {
                copy = henka_scene_duplicate_string(name);
                if (copy == NULL)
                {
                    scene->entities[index].active = false;
                    return HENKA_INVALID_ENTITY;
                }

                scene->entities[index].name = copy;
            }
            scene->entity_count += 1U;
            return (henka_entity)(index + 1U);
        }
    }

    if (henka_scene_grow(scene) != HENKA_SUCCESS)
    {
        return HENKA_INVALID_ENTITY;
    }

    scene->entities[scene->entity_count].active = true;
    scene->entities[scene->entity_count].visible = true;
    scene->entities[scene->entity_count].transform = henka_transform_identity();
    scene->entities[scene->entity_count].material = henka_material_default();
    scene->entities[scene->entity_count].has_local_bounds = false;
    scene->entities[scene->entity_count].local_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}};
    scene->entities[scene->entity_count].interaction = (henka_interaction_desc){false, 2.0f, NULL};
    if (name != NULL)
    {
        copy = henka_scene_duplicate_string(name);
        if (copy == NULL)
        {
            scene->entities[scene->entity_count].active = false;
            return HENKA_INVALID_ENTITY;
        }

        scene->entities[scene->entity_count].name = copy;
    }
    scene->entity_count += 1U;
    return (henka_entity)scene->entity_count;
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
    record->visible = true;
    record->mesh = NULL;
    record->material = henka_material_default();
    henka_free(record->name);
    henka_free(record->tag);
    record->name = NULL;
    record->tag = NULL;
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
            return (henka_entity)(entity_index + 1U);
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
            *out_entity = (henka_entity)(index + 1U);
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
            *out_entity = (henka_entity)(index + 1U);
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

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL || !record->has_local_bounds)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_bounds = henka_scene_transform_bounds(record->local_bounds, record->transform);
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

henka_result henka_scene_set_entity_transform(henka_scene* scene, henka_entity entity, henka_transform transform)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || !henka_transform_is_valid(transform))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->transform = henka_transform_sanitize(transform);
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
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || material.shader == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->material = material;
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_name(henka_scene* scene, henka_entity entity, const char* name)
{
    char* copy;
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    copy = NULL;
    if (name != NULL)
    {
        copy = henka_scene_duplicate_string(name);
        if (copy == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }

    henka_free(record->name);
    record->name = copy;
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_tag(henka_scene* scene, henka_entity entity, const char* tag)
{
    char* copy;
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    copy = NULL;
    if (tag != NULL)
    {
        copy = henka_scene_duplicate_string(tag);
        if (copy == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
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

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
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

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || interaction == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->interaction = *interaction;
    return HENKA_SUCCESS;
}

henka_interaction_result henka_scene_can_interact(const henka_scene* scene, henka_entity entity, henka_vec3 observer_position)
{
    const henka_scene_entity_record* record;
    float distance;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
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

    if (scene == NULL || out_entity == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_entity = HENKA_INVALID_ENTITY;
    if (out_distance != NULL)
    {
        *out_distance = 0.0f;
    }

    best_distance = 1000000.0f;
    found = false;
    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        henka_bounds world_bounds;
        float distance;
        const henka_scene_entity_record* record = &scene->entities[index];

        if (!record->active || !record->visible || !record->has_local_bounds)
        {
            continue;
        }

        world_bounds = henka_scene_transform_bounds(record->local_bounds, record->transform);
        if (!henka_scene_ray_intersects_bounds(ray, world_bounds, &distance))
        {
            continue;
        }

        if (!found || distance < best_distance)
        {
            best_distance = distance;
            *out_entity = (henka_entity)(index + 1U);
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
    if (scene == NULL || camera == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    scene->camera = *camera;
    scene->has_camera = true;
    return HENKA_SUCCESS;
}

void henka_scene_set_light_direction(henka_scene* scene, henka_vec3 light_direction)
{
    if (scene != NULL)
    {
        scene->light_direction = henka_vec3_normalize(light_direction);
    }
}

void henka_scene_set_ambient_color(henka_scene* scene, henka_vec3 ambient_color)
{
    if (scene != NULL)
    {
        scene->ambient_color = ambient_color;
    }
}
