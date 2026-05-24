#include "henka_internal.h"

#include <henka/memory.h>

henka_material henka_material_default(void)
{
    henka_material material;

    material.shader = NULL;
    material.base_color_texture = NULL;
    material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};
    material.use_texture = false;
    material.use_lighting = true;
    return material;
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
        new_entities[index].transform = henka_transform_identity();
        new_entities[index].mesh = NULL;
        new_entities[index].material = henka_material_default();
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
    if (scene == NULL)
    {
        return;
    }

    henka_free(scene->entities);
    henka_free(scene);
}

henka_entity henka_scene_create_entity(henka_scene* scene)
{
    size_t index;

    if (scene == NULL)
    {
        return HENKA_INVALID_ENTITY;
    }

    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        if (!scene->entities[index].active)
        {
            scene->entities[index].active = true;
            scene->entities[index].transform = henka_transform_identity();
            scene->entity_count += 1U;
            return (henka_entity)(index + 1U);
        }
    }

    if (henka_scene_grow(scene) != HENKA_SUCCESS)
    {
        return HENKA_INVALID_ENTITY;
    }

    scene->entities[scene->entity_count].active = true;
    scene->entities[scene->entity_count].transform = henka_transform_identity();
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
    record->mesh = NULL;
    record->material.shader = NULL;
    if (scene->entity_count > 0U)
    {
        scene->entity_count -= 1U;
    }
}

bool henka_scene_is_entity_valid(const henka_scene* scene, henka_entity entity)
{
    return henka_scene_get_entity_record_const(scene, entity) != NULL;
}

size_t henka_scene_get_entity_count(const henka_scene* scene)
{
    if (scene == NULL)
    {
        return 0U;
    }

    return scene->entity_count;
}

henka_result henka_scene_set_entity_transform(henka_scene* scene, henka_entity entity, henka_transform transform)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->transform = transform;
    return HENKA_SUCCESS;
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
