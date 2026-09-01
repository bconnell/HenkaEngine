#include <henka/scene_document.h>

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <henka/memory.h>
#include <henka/persistence.h>

#define HENKA_SCENE_DOCUMENT_MAGIC_0 ((uint8_t)'H')
#define HENKA_SCENE_DOCUMENT_MAGIC_1 ((uint8_t)'S')
#define HENKA_SCENE_DOCUMENT_MAGIC_2 ((uint8_t)'C')
#define HENKA_SCENE_DOCUMENT_MAGIC_3 ((uint8_t)'N')
#define HENKA_SCENE_DOCUMENT_HEADER_BYTES 40U
#define HENKA_SCENE_DOCUMENT_FLAG_VISIBLE UINT32_C(1)
#define HENKA_SCENE_DOCUMENT_FLAG_RENDERER_ENABLED UINT32_C(2)
#define HENKA_SCENE_DOCUMENT_FLAG_MATERIAL_OVERRIDE UINT32_C(4)
#define HENKA_SCENE_DOCUMENT_FLAG_INTERACTION_ENABLED UINT32_C(8)
#define HENKA_SCENE_DOCUMENT_FLAG_PHYSICS_ENABLED UINT32_C(16)
#define HENKA_SCENE_DOCUMENT_FLAG_TRIGGER UINT32_C(32)
#define HENKA_SCENE_DOCUMENT_FLAG_AUDIO_ENABLED UINT32_C(64)
#define HENKA_SCENE_DOCUMENT_FLAG_AUDIO_LOOPING UINT32_C(128)
#define HENKA_SCENE_DOCUMENT_FLAG_AUDIO_SPATIAL UINT32_C(256)
#define HENKA_SCENE_DOCUMENT_FLAG_AUDIO_STREAMING UINT32_C(512)
#define HENKA_SCENE_DOCUMENT_KNOWN_FLAGS ( \
    HENKA_SCENE_DOCUMENT_FLAG_VISIBLE | \
    HENKA_SCENE_DOCUMENT_FLAG_RENDERER_ENABLED | \
    HENKA_SCENE_DOCUMENT_FLAG_MATERIAL_OVERRIDE | \
    HENKA_SCENE_DOCUMENT_FLAG_INTERACTION_ENABLED | \
    HENKA_SCENE_DOCUMENT_FLAG_PHYSICS_ENABLED | \
    HENKA_SCENE_DOCUMENT_FLAG_TRIGGER | \
    HENKA_SCENE_DOCUMENT_FLAG_AUDIO_ENABLED | \
    HENKA_SCENE_DOCUMENT_FLAG_AUDIO_LOOPING | \
    HENKA_SCENE_DOCUMENT_FLAG_AUDIO_SPATIAL | \
    HENKA_SCENE_DOCUMENT_FLAG_AUDIO_STREAMING)
#define HENKA_SCENE_DOCUMENT_BEHAVIOR_FLAG_ENABLED UINT32_C(1)
#define HENKA_SCENE_DOCUMENT_BEHAVIOR_KNOWN_FLAGS \
    HENKA_SCENE_DOCUMENT_BEHAVIOR_FLAG_ENABLED

typedef struct henka_scene_document_storage
{
    size_t object_count;
    uint64_t next_id;
    henka_audio_listener audio_listener;
    henka_scene_document_object objects[HENKA_SCENE_DOCUMENT_MAX_OBJECTS];
} henka_scene_document_storage;

struct henka_scene_document
{
    henka_scene_document_storage* storage;
};

typedef struct henka_scene_document_writer
{
    uint8_t* data;
    size_t capacity;
    size_t position;
    bool failed;
} henka_scene_document_writer;

typedef struct henka_scene_document_reader
{
    const uint8_t* data;
    size_t size;
    size_t position;
    bool failed;
} henka_scene_document_reader;

static bool henka_scene_document_size_add(size_t* value, size_t addition)
{
    if (value == NULL || addition > SIZE_MAX - *value)
    {
        return false;
    }
    *value += addition;
    return true;
}

static henka_result henka_scene_document_validate_parent_links(
    const henka_scene_document_storage* storage,
    henka_scene_document_id replacement_id,
    henka_scene_document_id replacement_parent_id,
    bool has_replacement)
{
    for (size_t index = 0U; index < storage->object_count; ++index)
    {
        henka_scene_document_id current_id = storage->objects[index].id;
        for (size_t depth = 0U; depth <= storage->object_count; ++depth)
        {
            henka_scene_document_id parent_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
            size_t current_index = SIZE_MAX;
            if (current_id == HENKA_INVALID_SCENE_DOCUMENT_ID)
            {
                break;
            }
            for (size_t candidate_index = 0U;
                 candidate_index < storage->object_count;
                 ++candidate_index)
            {
                if (storage->objects[candidate_index].id == current_id)
                {
                    current_index = candidate_index;
                    break;
                }
            }
            if (current_index == SIZE_MAX)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            parent_id = has_replacement && current_id == replacement_id
                ? replacement_parent_id
                : storage->objects[current_index].parent_id;
            if (parent_id == HENKA_INVALID_SCENE_DOCUMENT_ID)
            {
                current_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
                break;
            }
            if (parent_id == current_id)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            current_id = parent_id;
        }
        if (current_id != HENKA_INVALID_SCENE_DOCUMENT_ID)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    return HENKA_SUCCESS;
}

static size_t henka_scene_document_bounded_length(const char* value, size_t capacity)
{
    size_t length = 0U;
    if (value == NULL)
    {
        return capacity;
    }
    while (length < capacity && value[length] != '\0')
    {
        ++length;
    }
    return length;
}

static bool henka_scene_document_string_is_valid(
    const char* value,
    size_t capacity,
    bool allow_empty)
{
    size_t index;
    size_t length;

    if (value == NULL)
    {
        return false;
    }
    length = henka_scene_document_bounded_length(value, capacity);
    if (length >= capacity || (!allow_empty && length == 0U))
    {
        return false;
    }
    for (index = 0U; index < length; ++index)
    {
        const unsigned char character = (unsigned char)value[index];
        if (character < 0x20U && character != '\t')
        {
            return false;
        }
    }
    return true;
}

static henka_result henka_scene_document_validate_path(const char* path, bool allow_empty)
{
    char* resolved = NULL;
    henka_result result;

    if (!henka_scene_document_string_is_valid(
            path,
            HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES,
            allow_empty))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (allow_empty && path[0] == '\0')
    {
        return HENKA_SUCCESS;
    }
    result = henka_path_resolve_confined(".", path, &resolved);
    henka_free(resolved);
    return result;
}

static bool henka_scene_document_string_has_suffix(
    const char* value,
    const char* suffix)
{
    const size_t value_length = value == NULL ? 0U : strlen(value);
    const size_t suffix_length = suffix == NULL ? 0U : strlen(suffix);
    return suffix_length > 0U && value_length >= suffix_length &&
        strcmp(value + value_length - suffix_length, suffix) == 0;
}

static henka_result henka_scene_document_validate_behavior(
    const henka_scene_document_behavior* behavior)
{
    const char* expected_suffix;
    if (behavior == NULL ||
        behavior->id == HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID ||
        behavior->language < HENKA_SCRIPT_LANGUAGE_LUA ||
        behavior->language > HENKA_SCRIPT_LANGUAGE_HENKASCRIPT ||
        !henka_scene_document_string_is_valid(
            behavior->asset_path,
            HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES,
            false) ||
        henka_scene_document_validate_path(behavior->asset_path, false) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    expected_suffix = behavior->language == HENKA_SCRIPT_LANGUAGE_LUA
        ? ".lua"
        : ".hks";
    return henka_scene_document_string_has_suffix(behavior->asset_path, expected_suffix)
        ? HENKA_SUCCESS
        : HENKA_ERROR_INVALID_ARGUMENT;
}

static bool henka_scene_document_finite_vec3(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool henka_scene_document_finite_vec4(henka_vec4 value)
{
    return isfinite(value.x) && isfinite(value.y) &&
        isfinite(value.z) && isfinite(value.w);
}

static bool henka_scene_document_finite_quat(henka_quat value)
{
    const float length_squared = value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w;
    return isfinite(length_squared) && length_squared > FLT_EPSILON;
}

static bool henka_scene_document_valid_transform(const henka_transform* transform)
{
    return transform != NULL &&
        henka_scene_document_finite_vec3(transform->position) &&
        henka_scene_document_finite_quat(transform->rotation) &&
        henka_scene_document_finite_vec3(transform->scale) &&
        transform->scale.x != 0.0f &&
        transform->scale.y != 0.0f &&
        transform->scale.z != 0.0f;
}

static bool henka_scene_document_valid_audio_listener(henka_audio_listener listener)
{
    const float forward_length = henka_vec3_length(listener.forward);
    const float up_length = henka_vec3_length(listener.up);
    const float right_length = henka_vec3_length(henka_vec3_cross(
        listener.forward,
        listener.up));
    return henka_scene_document_finite_vec3(listener.position) &&
        henka_scene_document_finite_vec3(listener.forward) &&
        henka_scene_document_finite_vec3(listener.up) &&
        isfinite(forward_length) != 0 && forward_length > 0.0f &&
        isfinite(up_length) != 0 && up_length > 0.0f &&
        isfinite(right_length) != 0 && right_length > 0.0001f;
}

static henka_result henka_scene_document_validate_object(
    const henka_scene_document_object* object)
{
    const henka_scene_document_source* source;
    const henka_scene_document_renderer* renderer;
    const henka_scene_document_interaction* interaction;
    const henka_scene_document_physics* physics;

    if (object == NULL || object->id == HENKA_INVALID_SCENE_DOCUMENT_ID ||
        !henka_scene_document_string_is_valid(
            object->name,
            HENKA_SCENE_DOCUMENT_MAX_NAME_BYTES,
            true) ||
        !henka_scene_document_valid_transform(&object->transform))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    source = &object->source;
    if (source->kind < HENKA_SCENE_DOCUMENT_SOURCE_NONE ||
        source->kind > HENKA_SCENE_DOCUMENT_SOURCE_ASSET ||
        source->primitive < HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX ||
        source->primitive > HENKA_SCENE_DOCUMENT_PRIMITIVE_PLANE ||
        source->asset_kind < HENKA_SCENE_DOCUMENT_ASSET_UNKNOWN ||
        source->asset_kind > HENKA_SCENE_DOCUMENT_ASSET_MATERIAL ||
        !henka_scene_document_finite_vec3(source->primitive_dimensions))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_scene_document_validate_path(
            source->path,
            source->kind == HENKA_SCENE_DOCUMENT_SOURCE_NONE ||
                source->kind == HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (source->kind == HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE &&
        (source->primitive_dimensions.x <= 0.0f ||
            source->primitive_dimensions.y <= 0.0f ||
            source->primitive_dimensions.z <= 0.0f))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if ((source->kind == HENKA_SCENE_DOCUMENT_SOURCE_NONE ||
            source->kind == HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE) &&
        source->path[0] != '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (source->kind == HENKA_SCENE_DOCUMENT_SOURCE_AUTHORING_MESH &&
        source->path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (source->kind == HENKA_SCENE_DOCUMENT_SOURCE_ASSET &&
        (source->path[0] == '\0' || source->asset_kind == HENKA_SCENE_DOCUMENT_ASSET_UNKNOWN))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    renderer = &object->renderer;
    if (henka_scene_document_validate_path(renderer->material_path, true) != HENKA_SUCCESS ||
        !henka_scene_document_finite_vec4(renderer->base_color) ||
        !henka_scene_document_finite_vec3(renderer->emissive) ||
        !isfinite(renderer->metallic) || !isfinite(renderer->roughness) ||
        !isfinite(renderer->emissive_strength) ||
        renderer->metallic < 0.0f || renderer->metallic > 1.0f ||
        renderer->roughness < 0.0f || renderer->roughness > 1.0f ||
        renderer->emissive_strength < 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_audio_emitter_config_validate(&object->audio) != HENKA_SUCCESS ||
        henka_scene_document_validate_path(
            object->audio.clip_path,
            !object->audio.enabled) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    interaction = &object->interaction;
    if (!isfinite(interaction->max_distance) || interaction->max_distance < 0.0f ||
        !henka_scene_document_string_is_valid(
            interaction->prompt,
            HENKA_SCENE_DOCUMENT_MAX_PROMPT_BYTES,
            true))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    physics = &object->physics;
    if (physics->body_type < HENKA_PHYSICS_BODY_STATIC ||
        physics->body_type > HENKA_PHYSICS_BODY_KINEMATIC ||
        physics->shape < HENKA_PHYSICS_SHAPE_SPHERE ||
        physics->shape > HENKA_PHYSICS_SHAPE_HEIGHTFIELD ||
        !henka_scene_document_finite_vec3(physics->collider_offset) ||
        !henka_scene_document_finite_vec3(physics->box_half_extents) ||
        !isfinite(physics->sphere_radius) || !isfinite(physics->mass) ||
        !isfinite(physics->material.restitution) ||
        !isfinite(physics->material.static_friction) ||
        !isfinite(physics->material.dynamic_friction) ||
        !isfinite(physics->material.linear_damping) ||
        !isfinite(physics->material.angular_damping))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (physics->enabled)
    {
        if (physics->shape != HENKA_PHYSICS_SHAPE_SPHERE &&
            physics->shape != HENKA_PHYSICS_SHAPE_BOX)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if ((physics->shape == HENKA_PHYSICS_SHAPE_SPHERE && physics->sphere_radius <= 0.0f) ||
            (physics->shape == HENKA_PHYSICS_SHAPE_BOX &&
                (physics->box_half_extents.x <= 0.0f ||
                    physics->box_half_extents.y <= 0.0f ||
                    physics->box_half_extents.z <= 0.0f)) ||
            physics->mass < 0.0f ||
            (physics->body_type == HENKA_PHYSICS_BODY_DYNAMIC && physics->mass <= 0.0f) ||
            physics->material.restitution < 0.0f || physics->material.restitution > 1.0f ||
            physics->material.static_friction < 0.0f ||
            physics->material.dynamic_friction < 0.0f ||
            physics->material.linear_damping < 0.0f ||
            physics->material.angular_damping < 0.0f ||
            physics->layer == 0U || physics->mask == 0U)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    if (object->behavior_count > HENKA_SCENE_DOCUMENT_MAX_BEHAVIORS_PER_OBJECT)
    {
        return HENKA_ERROR_LIMIT;
    }
    for (size_t index = 0U; index < object->behavior_count; ++index)
    {
        if (henka_scene_document_validate_behavior(&object->behaviors[index]) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (size_t other_index = 0U; other_index < index; ++other_index)
        {
            if (object->behaviors[other_index].id == object->behaviors[index].id)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    return HENKA_SUCCESS;
}

static henka_result henka_scene_document_validate_storage(
    const henka_scene_document_storage* storage)
{
    uint64_t maximum_id = 0U;
    size_t index;

    if (storage == NULL || storage->object_count > HENKA_SCENE_DOCUMENT_MAX_OBJECTS ||
        (storage->next_id == 0U && storage->object_count == 0U) ||
        !henka_scene_document_valid_audio_listener(storage->audio_listener))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < storage->object_count; ++index)
    {
        size_t other_index;
        henka_result result = henka_scene_document_validate_object(&storage->objects[index]);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (storage->objects[index].id > maximum_id)
        {
            maximum_id = storage->objects[index].id;
        }
        for (size_t behavior_index = 0U;
             behavior_index < storage->objects[index].behavior_count;
             ++behavior_index)
        {
            const henka_scene_document_behavior_id behavior_id =
                storage->objects[index].behaviors[behavior_index].id;
            if (behavior_id == storage->objects[index].id)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            if (behavior_id > maximum_id)
            {
                maximum_id = behavior_id;
            }
            for (size_t other_object_index = 0U;
                 other_object_index < index;
                 ++other_object_index)
            {
                if (storage->objects[other_object_index].id == behavior_id)
                {
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
                for (size_t other_behavior_index = 0U;
                     other_behavior_index < storage->objects[other_object_index].behavior_count;
                     ++other_behavior_index)
                {
                    if (storage->objects[other_object_index].behaviors[other_behavior_index].id == behavior_id)
                    {
                        return HENKA_ERROR_INVALID_ARGUMENT;
                    }
                }
            }
        }
        for (other_index = 0U; other_index < index; ++other_index)
        {
            if (storage->objects[other_index].id == storage->objects[index].id)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            for (size_t behavior_index = 0U;
                 behavior_index < storage->objects[other_index].behavior_count;
                 ++behavior_index)
            {
                if (storage->objects[other_index].behaviors[behavior_index].id == storage->objects[index].id)
                {
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
            }
        }
    }
    if (henka_scene_document_validate_parent_links(
            storage,
            HENKA_INVALID_SCENE_DOCUMENT_ID,
            HENKA_INVALID_SCENE_DOCUMENT_ID,
            false) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if ((storage->next_id != 0U && storage->next_id <= maximum_id) ||
        (storage->next_id == 0U && maximum_id != UINT64_MAX))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    return HENKA_SUCCESS;
}

henka_scene_document_object henka_scene_document_object_default(void)
{
    henka_scene_document_object object;
    memset(&object, 0, sizeof(object));
    object.parent_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    object.visible = true;
    object.transform = henka_transform_identity();
    object.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_NONE;
    object.source.primitive = HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX;
    object.source.primitive_dimensions = (henka_vec3){1.0f, 1.0f, 1.0f};
    object.renderer.enabled = true;
    object.renderer.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};
    object.renderer.roughness = 0.5f;
    object.interaction.max_distance = 0.0f;
    object.physics.body_type = HENKA_PHYSICS_BODY_STATIC;
    object.physics.shape = HENKA_PHYSICS_SHAPE_BOX;
    object.physics.sphere_radius = 0.5f;
    object.physics.box_half_extents = (henka_vec3){0.5f, 0.5f, 0.5f};
    object.physics.material = (henka_physics_material){0.0f, 0.5f, 0.5f, 0.0f, 0.0f};
    object.physics.layer = 1U;
    object.physics.mask = HENKA_PHYSICS_ALL_LAYERS;
    object.audio = henka_audio_emitter_config_default();
    return object;
}

henka_scene_document_behavior henka_scene_document_behavior_default(void)
{
    henka_scene_document_behavior behavior;
    memset(&behavior, 0, sizeof(behavior));
    behavior.enabled = true;
    behavior.language = HENKA_SCRIPT_LANGUAGE_NONE;
    return behavior;
}

static size_t henka_scene_document_find_index(
    const henka_scene_document_storage* storage,
    henka_scene_document_id id)
{
    size_t index;
    if (storage == NULL || id == HENKA_INVALID_SCENE_DOCUMENT_ID)
    {
        return SIZE_MAX;
    }
    for (index = 0U; index < storage->object_count; ++index)
    {
        if (storage->objects[index].id == id)
        {
            return index;
        }
    }
    return SIZE_MAX;
}

static size_t henka_scene_document_find_behavior_index(
    const henka_scene_document_object* object,
    henka_scene_document_behavior_id behavior_id)
{
    size_t index;
    if (object == NULL || behavior_id == HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID)
    {
        return SIZE_MAX;
    }
    for (index = 0U; index < object->behavior_count; ++index)
    {
        if (object->behaviors[index].id == behavior_id)
        {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool henka_scene_document_id_is_used(
    const henka_scene_document_storage* storage,
    uint64_t id)
{
    size_t object_index;
    if (storage == NULL || id == 0U)
    {
        return true;
    }
    for (object_index = 0U; object_index < storage->object_count; ++object_index)
    {
        const henka_scene_document_object* object = &storage->objects[object_index];
        if (object->id == id)
        {
            return true;
        }
        for (size_t behavior_index = 0U; behavior_index < object->behavior_count; ++behavior_index)
        {
            if (object->behaviors[behavior_index].id == id)
            {
                return true;
            }
        }
    }
    return false;
}

static henka_result henka_scene_document_allocate_id(
    henka_scene_document_storage* storage,
    henka_scene_document_id* out_id)
{
    if (storage == NULL || out_id == NULL || storage->next_id == 0U)
    {
        return HENKA_ERROR_LIMIT;
    }
    *out_id = storage->next_id;
    storage->next_id = storage->next_id == UINT64_MAX ? 0U : storage->next_id + 1U;
    return HENKA_SUCCESS;
}

henka_result henka_scene_document_create(henka_scene_document** out_document)
{
    henka_scene_document* document;
    if (out_document == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_document = NULL;
    document = (henka_scene_document*)henka_calloc(1U, sizeof(*document));
    if (document == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    document->storage = (henka_scene_document_storage*)henka_calloc(1U, sizeof(*document->storage));
    if (document->storage == NULL)
    {
        henka_free(document);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    document->storage->next_id = 1U;
    document->storage->audio_listener = henka_audio_listener_default();
    *out_document = document;
    return HENKA_SUCCESS;
}

void henka_scene_document_destroy(henka_scene_document* document)
{
    if (document == NULL)
    {
        return;
    }
    henka_free(document->storage);
    henka_free(document);
}

henka_result henka_scene_document_clear(henka_scene_document* document)
{
    if (document == NULL || document->storage == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(document->storage, 0, sizeof(*document->storage));
    document->storage->next_id = 1U;
    document->storage->audio_listener = henka_audio_listener_default();
    return HENKA_SUCCESS;
}

size_t henka_scene_document_get_object_count(const henka_scene_document* document)
{
    return document == NULL || document->storage == NULL ? 0U : document->storage->object_count;
}

henka_result henka_scene_document_get_object_at(
    const henka_scene_document* document,
    size_t index,
    henka_scene_document_object* out_object)
{
    if (document == NULL || document->storage == NULL || out_object == NULL ||
        index >= document->storage->object_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_object = document->storage->objects[index];
    return HENKA_SUCCESS;
}

henka_result henka_scene_document_get_object(
    const henka_scene_document* document,
    henka_scene_document_id id,
    henka_scene_document_object* out_object)
{
    const size_t index = henka_scene_document_find_index(
        document == NULL ? NULL : document->storage,
        id);
    if (index == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_get_object_at(document, index, out_object);
}

henka_result henka_scene_document_add_object(
    henka_scene_document* document,
    const henka_scene_document_object* object,
    henka_scene_document_id* out_id)
{
    henka_scene_document_object candidate;
    henka_result result;
    uint64_t next_id;
    size_t behavior_index;

    if (document == NULL || document->storage == NULL || object == NULL || out_id == NULL ||
        document->storage->object_count >= HENKA_SCENE_DOCUMENT_MAX_OBJECTS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (document->storage->next_id == 0U)
    {
        return HENKA_ERROR_LIMIT;
    }
    candidate = *object;
    next_id = document->storage->next_id;
    if (candidate.id == HENKA_INVALID_SCENE_DOCUMENT_ID)
    {
        candidate.id = next_id;
        if (candidate.id == 0U)
        {
            return HENKA_ERROR_LIMIT;
        }
        next_id = candidate.id == UINT64_MAX ? 0U : candidate.id + 1U;
    }
    else if (henka_scene_document_id_is_used(document->storage, candidate.id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    else if (candidate.id >= next_id)
    {
        next_id = candidate.id == UINT64_MAX ? 0U : candidate.id + 1U;
    }
    if (candidate.parent_id == candidate.id)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (candidate.parent_id != HENKA_INVALID_SCENE_DOCUMENT_ID &&
        henka_scene_document_find_index(document->storage, candidate.parent_id) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (behavior_index = 0U; behavior_index < candidate.behavior_count; ++behavior_index)
    {
        henka_scene_document_behavior* behavior = &candidate.behaviors[behavior_index];
        if (behavior->id == HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID)
        {
            behavior->id = next_id;
            if (behavior->id == 0U)
            {
                return HENKA_ERROR_LIMIT;
            }
            next_id = behavior->id == UINT64_MAX ? 0U : behavior->id + 1U;
        }
        else
        {
            if (behavior->id == candidate.id ||
                henka_scene_document_id_is_used(document->storage, behavior->id))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            for (size_t other_behavior_index = 0U;
                 other_behavior_index < behavior_index;
                 ++other_behavior_index)
            {
                if (candidate.behaviors[other_behavior_index].id == behavior->id)
                {
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
            }
            if (behavior->id >= next_id && next_id != 0U)
            {
                next_id = behavior->id == UINT64_MAX ? 0U : behavior->id + 1U;
            }
        }
    }
    result = henka_scene_document_validate_object(&candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    document->storage->objects[document->storage->object_count++] = candidate;
    document->storage->next_id = next_id;
    *out_id = candidate.id;
    return HENKA_SUCCESS;
}

henka_result henka_scene_document_duplicate_object(
    henka_scene_document* document,
    henka_scene_document_id source_id,
    henka_scene_document_id* out_id)
{
    henka_scene_document_object candidate;
    henka_result result = henka_scene_document_get_object(document, source_id, &candidate);
    size_t behavior_index;
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    candidate.id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    for (behavior_index = 0U; behavior_index < candidate.behavior_count; ++behavior_index)
    {
        candidate.behaviors[behavior_index].id = HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID;
    }
    return henka_scene_document_add_object(document, &candidate, out_id);
}

henka_result henka_scene_document_set_object(
    henka_scene_document* document,
    const henka_scene_document_object* object)
{
    const size_t index = henka_scene_document_find_index(
        document == NULL ? NULL : document->storage,
        object == NULL ? HENKA_INVALID_SCENE_DOCUMENT_ID : object->id);
    henka_result result;
    if (index == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_scene_document_validate_object(object);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    for (size_t other_index = 0U;
         other_index < document->storage->object_count;
         ++other_index)
    {
        const henka_scene_document_object* other = &document->storage->objects[other_index];
        if (other_index == index)
        {
            continue;
        }
        for (size_t behavior_index = 0U;
             behavior_index < object->behavior_count;
             ++behavior_index)
        {
            const henka_scene_document_behavior_id behavior_id = object->behaviors[behavior_index].id;
            if (behavior_id == other->id)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            for (size_t other_behavior_index = 0U;
                 other_behavior_index < other->behavior_count;
                 ++other_behavior_index)
            {
                if (behavior_id == other->behaviors[other_behavior_index].id)
                {
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
            }
        }
        for (size_t other_behavior_index = 0U;
             other_behavior_index < other->behavior_count;
             ++other_behavior_index)
        {
            if (object->id == other->behaviors[other_behavior_index].id)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    if (henka_scene_document_validate_parent_links(
            document->storage,
            object->id,
            object->parent_id,
            true) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    document->storage->objects[index] = *object;
    return HENKA_SUCCESS;
}

henka_result henka_scene_document_remove_object(
    henka_scene_document* document,
    henka_scene_document_id id)
{
    const size_t index = henka_scene_document_find_index(
        document == NULL ? NULL : document->storage,
        id);
    if (index == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (size_t child_index = 0U;
         child_index < document->storage->object_count;
         ++child_index)
    {
        if (document->storage->objects[child_index].parent_id == id)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    if (index + 1U < document->storage->object_count)
    {
        memmove(
            &document->storage->objects[index],
            &document->storage->objects[index + 1U],
            (document->storage->object_count - index - 1U) * sizeof(document->storage->objects[0]));
    }
    --document->storage->object_count;
    memset(
        &document->storage->objects[document->storage->object_count],
        0,
        sizeof(document->storage->objects[0]));
    return HENKA_SUCCESS;
}

size_t henka_scene_document_get_behavior_count(
    const henka_scene_document* document,
    henka_scene_document_id object_id)
{
    const size_t object_index = henka_scene_document_find_index(
        document == NULL ? NULL : document->storage,
        object_id);
    return object_index == SIZE_MAX ? 0U : document->storage->objects[object_index].behavior_count;
}

henka_result henka_scene_document_get_behavior_at(
    const henka_scene_document* document,
    henka_scene_document_id object_id,
    size_t index,
    henka_scene_document_behavior* out_behavior)
{
    const size_t object_index = henka_scene_document_find_index(
        document == NULL ? NULL : document->storage,
        object_id);
    if (object_index == SIZE_MAX || out_behavior == NULL ||
        index >= document->storage->objects[object_index].behavior_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_behavior = document->storage->objects[object_index].behaviors[index];
    return HENKA_SUCCESS;
}

henka_result henka_scene_document_get_behavior(
    const henka_scene_document* document,
    henka_scene_document_id object_id,
    henka_scene_document_behavior_id behavior_id,
    henka_scene_document_behavior* out_behavior)
{
    const size_t object_index = henka_scene_document_find_index(
        document == NULL ? NULL : document->storage,
        object_id);
    const size_t behavior_index = object_index == SIZE_MAX
        ? SIZE_MAX
        : henka_scene_document_find_behavior_index(
            &document->storage->objects[object_index],
            behavior_id);
    if (behavior_index == SIZE_MAX || out_behavior == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_behavior = document->storage->objects[object_index].behaviors[behavior_index];
    return HENKA_SUCCESS;
}

henka_result henka_scene_document_add_behavior(
    henka_scene_document* document,
    henka_scene_document_id object_id,
    const henka_scene_document_behavior* behavior,
    henka_scene_document_behavior_id* out_behavior_id)
{
    const size_t object_index = henka_scene_document_find_index(
        document == NULL ? NULL : document->storage,
        object_id);
    henka_scene_document_behavior candidate;
    uint64_t next_id;

    if (object_index == SIZE_MAX || behavior == NULL || out_behavior_id == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (document->storage->objects[object_index].behavior_count >=
        HENKA_SCENE_DOCUMENT_MAX_BEHAVIORS_PER_OBJECT)
    {
        return HENKA_ERROR_LIMIT;
    }
    candidate = *behavior;
    next_id = document->storage->next_id;
    if (candidate.id == HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID)
    {
        if (next_id == 0U)
        {
            return HENKA_ERROR_LIMIT;
        }
        candidate.id = next_id;
        next_id = candidate.id == UINT64_MAX ? 0U : candidate.id + 1U;
    }
    else
    {
        if (candidate.id == object_id ||
            henka_scene_document_id_is_used(document->storage, candidate.id))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (next_id != 0U && candidate.id >= next_id)
        {
            next_id = candidate.id == UINT64_MAX ? 0U : candidate.id + 1U;
        }
    }
    if (henka_scene_document_validate_behavior(&candidate) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    document->storage->objects[object_index].behaviors[
        document->storage->objects[object_index].behavior_count++] = candidate;
    document->storage->next_id = next_id;
    *out_behavior_id = candidate.id;
    return HENKA_SUCCESS;
}

henka_result henka_scene_document_set_behavior(
    henka_scene_document* document,
    henka_scene_document_id object_id,
    const henka_scene_document_behavior* behavior)
{
    const size_t object_index = henka_scene_document_find_index(
        document == NULL ? NULL : document->storage,
        object_id);
    const size_t behavior_index = object_index == SIZE_MAX || behavior == NULL
        ? SIZE_MAX
        : henka_scene_document_find_behavior_index(
            &document->storage->objects[object_index],
            behavior->id);
    if (behavior_index == SIZE_MAX ||
        henka_scene_document_validate_behavior(behavior) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    document->storage->objects[object_index].behaviors[behavior_index] = *behavior;
    return HENKA_SUCCESS;
}

henka_result henka_scene_document_remove_behavior(
    henka_scene_document* document,
    henka_scene_document_id object_id,
    henka_scene_document_behavior_id behavior_id)
{
    const size_t object_index = henka_scene_document_find_index(
        document == NULL ? NULL : document->storage,
        object_id);
    const size_t behavior_index = object_index == SIZE_MAX
        ? SIZE_MAX
        : henka_scene_document_find_behavior_index(
            &document->storage->objects[object_index],
            behavior_id);
    henka_scene_document_object* object;
    if (behavior_index == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    object = &document->storage->objects[object_index];
    if (behavior_index + 1U < object->behavior_count)
    {
        memmove(
            &object->behaviors[behavior_index],
            &object->behaviors[behavior_index + 1U],
            (object->behavior_count - behavior_index - 1U) * sizeof(object->behaviors[0]));
    }
    --object->behavior_count;
    memset(&object->behaviors[object->behavior_count], 0, sizeof(object->behaviors[0]));
    return HENKA_SUCCESS;
}

henka_result henka_scene_document_set_audio_listener(
    henka_scene_document* document,
    henka_audio_listener listener)
{
    if (document == NULL || document->storage == NULL ||
        !henka_scene_document_valid_audio_listener(listener))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    document->storage->audio_listener = listener;
    return HENKA_SUCCESS;
}

henka_result henka_scene_document_get_audio_listener(
    const henka_scene_document* document,
    henka_audio_listener* out_listener)
{
    if (document == NULL || document->storage == NULL || out_listener == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_listener = document->storage->audio_listener;
    return HENKA_SUCCESS;
}

henka_result henka_scene_document_validate(const henka_scene_document* document)
{
    if (document == NULL || document->storage == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_validate_storage(document->storage);
}

static uint32_t henka_scene_document_checksum(const uint8_t* data, size_t size)
{
    uint32_t checksum = UINT32_C(0xFFFFFFFF);
    size_t index;
    uint32_t bit;
    for (index = 0U; index < size; ++index)
    {
        checksum ^= data[index];
        for (bit = 0U; bit < 8U; ++bit)
        {
            checksum = (checksum & 1U) != 0U
                ? (checksum >> 1U) ^ UINT32_C(0xEDB88320)
                : checksum >> 1U;
        }
    }
    return ~checksum;
}

static void henka_scene_document_write_u16(uint8_t* destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8U) & 0xFFU);
}

static void henka_scene_document_write_u32(uint8_t* destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8U) & 0xFFU);
    destination[2] = (uint8_t)((value >> 16U) & 0xFFU);
    destination[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static void henka_scene_document_write_u64(uint8_t* destination, uint64_t value)
{
    uint32_t index;
    for (index = 0U; index < 8U; ++index)
    {
        destination[index] = (uint8_t)((value >> (index * 8U)) & 0xFFU);
    }
}

static uint16_t henka_scene_document_read_u16(const uint8_t* source)
{
    return (uint16_t)source[0] | (uint16_t)((uint16_t)source[1] << 8U);
}

static uint32_t henka_scene_document_read_u32(const uint8_t* source)
{
    return (uint32_t)source[0] |
        ((uint32_t)source[1] << 8U) |
        ((uint32_t)source[2] << 16U) |
        ((uint32_t)source[3] << 24U);
}

static uint64_t henka_scene_document_read_u64(const uint8_t* source)
{
    uint64_t value = 0U;
    uint32_t index;
    for (index = 0U; index < 8U; ++index)
    {
        value |= (uint64_t)source[index] << (index * 8U);
    }
    return value;
}

static bool henka_scene_document_payload_size(
    const henka_scene_document_storage* storage,
    size_t* out_size)
{
    size_t size = 0U;
    size_t index;
    if (storage == NULL || out_size == NULL)
    {
        return false;
    }
    for (index = 0U; index < storage->object_count; ++index)
    {
        const henka_scene_document_object* object = &storage->objects[index];
        const size_t name_length = strlen(object->name);
        const size_t source_path_length = strlen(object->source.path);
        const size_t material_path_length = strlen(object->renderer.material_path);
        const size_t prompt_length = strlen(object->interaction.prompt);
        const size_t audio_path_length = strlen(object->audio.clip_path);
        if (name_length > UINT16_MAX || source_path_length > UINT16_MAX ||
            material_path_length > UINT16_MAX || prompt_length > UINT16_MAX ||
            audio_path_length > UINT16_MAX ||
            !henka_scene_document_size_add(&size, 8U + 8U + 4U + 2U + name_length + 40U +
                4U + 4U + 12U + 2U + source_path_length + 4U +
                2U + material_path_length + 40U + 4U + 2U + prompt_length +
                4U + 4U + 12U + 4U + 12U + 4U + 20U + 4U + 4U +
                2U + audio_path_length + 4U + 16U + 4U))
        {
            return false;
        }
        for (size_t behavior_index = 0U;
             behavior_index < object->behavior_count;
             ++behavior_index)
        {
            const size_t path_length = strlen(object->behaviors[behavior_index].asset_path);
            if (path_length > UINT16_MAX ||
                !henka_scene_document_size_add(&size, 8U + 4U + 4U + 2U + path_length))
            {
                return false;
            }
        }
    }
    if (!henka_scene_document_size_add(&size, 9U * sizeof(uint32_t)))
    {
        return false;
    }
    *out_size = size;
    return size <= HENKA_SCENE_DOCUMENT_MAX_FILE_BYTES - HENKA_SCENE_DOCUMENT_HEADER_BYTES;
}

static void henka_scene_document_writer_bytes(
    henka_scene_document_writer* writer,
    const void* data,
    size_t size)
{
    if (writer == NULL || writer->failed || data == NULL ||
        size > writer->capacity - writer->position)
    {
        if (writer != NULL)
        {
            writer->failed = true;
        }
        return;
    }
    memcpy(writer->data + writer->position, data, size);
    writer->position += size;
}

static void henka_scene_document_writer_u16(henka_scene_document_writer* writer, uint16_t value)
{
    uint8_t bytes[2];
    henka_scene_document_write_u16(bytes, value);
    henka_scene_document_writer_bytes(writer, bytes, sizeof(bytes));
}

static void henka_scene_document_writer_u32(henka_scene_document_writer* writer, uint32_t value)
{
    uint8_t bytes[4];
    henka_scene_document_write_u32(bytes, value);
    henka_scene_document_writer_bytes(writer, bytes, sizeof(bytes));
}

static void henka_scene_document_writer_u64(henka_scene_document_writer* writer, uint64_t value)
{
    uint8_t bytes[8];
    henka_scene_document_write_u64(bytes, value);
    henka_scene_document_writer_bytes(writer, bytes, sizeof(bytes));
}

static void henka_scene_document_writer_float(henka_scene_document_writer* writer, float value)
{
    uint32_t bits = 0U;
    _Static_assert(sizeof(float) == sizeof(uint32_t), "Scene Document requires 32-bit float");
    memcpy(&bits, &value, sizeof(bits));
    henka_scene_document_writer_u32(writer, bits);
}

static void henka_scene_document_writer_string(
    henka_scene_document_writer* writer,
    const char* value)
{
    const size_t length = strlen(value);
    henka_scene_document_writer_u16(writer, (uint16_t)length);
    henka_scene_document_writer_bytes(writer, value, length);
}

static void henka_scene_document_encode_object(
    henka_scene_document_writer* writer,
    const henka_scene_document_object* object)
{
    uint32_t flags = 0U;
    if (object->visible) flags |= HENKA_SCENE_DOCUMENT_FLAG_VISIBLE;
    if (object->renderer.enabled) flags |= HENKA_SCENE_DOCUMENT_FLAG_RENDERER_ENABLED;
    if (object->renderer.material_override) flags |= HENKA_SCENE_DOCUMENT_FLAG_MATERIAL_OVERRIDE;
    if (object->interaction.enabled) flags |= HENKA_SCENE_DOCUMENT_FLAG_INTERACTION_ENABLED;
    if (object->physics.enabled) flags |= HENKA_SCENE_DOCUMENT_FLAG_PHYSICS_ENABLED;
    if (object->physics.is_trigger) flags |= HENKA_SCENE_DOCUMENT_FLAG_TRIGGER;
    if (object->audio.enabled) flags |= HENKA_SCENE_DOCUMENT_FLAG_AUDIO_ENABLED;
    if (object->audio.looping) flags |= HENKA_SCENE_DOCUMENT_FLAG_AUDIO_LOOPING;
    if (object->audio.spatial) flags |= HENKA_SCENE_DOCUMENT_FLAG_AUDIO_SPATIAL;
    if (object->audio.streaming) flags |= HENKA_SCENE_DOCUMENT_FLAG_AUDIO_STREAMING;

    henka_scene_document_writer_u64(writer, object->id);
    henka_scene_document_writer_u64(writer, object->parent_id);
    henka_scene_document_writer_u32(writer, flags);
    henka_scene_document_writer_string(writer, object->name);
    henka_scene_document_writer_float(writer, object->transform.position.x);
    henka_scene_document_writer_float(writer, object->transform.position.y);
    henka_scene_document_writer_float(writer, object->transform.position.z);
    henka_scene_document_writer_float(writer, object->transform.rotation.x);
    henka_scene_document_writer_float(writer, object->transform.rotation.y);
    henka_scene_document_writer_float(writer, object->transform.rotation.z);
    henka_scene_document_writer_float(writer, object->transform.rotation.w);
    henka_scene_document_writer_float(writer, object->transform.scale.x);
    henka_scene_document_writer_float(writer, object->transform.scale.y);
    henka_scene_document_writer_float(writer, object->transform.scale.z);
    henka_scene_document_writer_u32(writer, (uint32_t)object->source.kind);
    henka_scene_document_writer_u32(writer, (uint32_t)object->source.primitive);
    henka_scene_document_writer_float(writer, object->source.primitive_dimensions.x);
    henka_scene_document_writer_float(writer, object->source.primitive_dimensions.y);
    henka_scene_document_writer_float(writer, object->source.primitive_dimensions.z);
    henka_scene_document_writer_string(writer, object->source.path);
    henka_scene_document_writer_u32(writer, (uint32_t)object->source.asset_kind);
    henka_scene_document_writer_string(writer, object->renderer.material_path);
    henka_scene_document_writer_float(writer, object->renderer.base_color.x);
    henka_scene_document_writer_float(writer, object->renderer.base_color.y);
    henka_scene_document_writer_float(writer, object->renderer.base_color.z);
    henka_scene_document_writer_float(writer, object->renderer.base_color.w);
    henka_scene_document_writer_float(writer, object->renderer.metallic);
    henka_scene_document_writer_float(writer, object->renderer.roughness);
    henka_scene_document_writer_float(writer, object->renderer.emissive.x);
    henka_scene_document_writer_float(writer, object->renderer.emissive.y);
    henka_scene_document_writer_float(writer, object->renderer.emissive.z);
    henka_scene_document_writer_float(writer, object->renderer.emissive_strength);
    henka_scene_document_writer_float(writer, object->interaction.max_distance);
    henka_scene_document_writer_string(writer, object->interaction.prompt);
    henka_scene_document_writer_u32(writer, (uint32_t)object->physics.body_type);
    henka_scene_document_writer_u32(writer, (uint32_t)object->physics.shape);
    henka_scene_document_writer_float(writer, object->physics.collider_offset.x);
    henka_scene_document_writer_float(writer, object->physics.collider_offset.y);
    henka_scene_document_writer_float(writer, object->physics.collider_offset.z);
    henka_scene_document_writer_float(writer, object->physics.sphere_radius);
    henka_scene_document_writer_float(writer, object->physics.box_half_extents.x);
    henka_scene_document_writer_float(writer, object->physics.box_half_extents.y);
    henka_scene_document_writer_float(writer, object->physics.box_half_extents.z);
    henka_scene_document_writer_float(writer, object->physics.mass);
    henka_scene_document_writer_float(writer, object->physics.material.restitution);
    henka_scene_document_writer_float(writer, object->physics.material.static_friction);
    henka_scene_document_writer_float(writer, object->physics.material.dynamic_friction);
    henka_scene_document_writer_float(writer, object->physics.material.linear_damping);
    henka_scene_document_writer_float(writer, object->physics.material.angular_damping);
    henka_scene_document_writer_u32(writer, object->physics.layer);
    henka_scene_document_writer_u32(writer, object->physics.mask);
    henka_scene_document_writer_string(writer, object->audio.clip_path);
    henka_scene_document_writer_u32(writer, (uint32_t)object->audio.bus);
    henka_scene_document_writer_float(writer, object->audio.gain);
    henka_scene_document_writer_float(writer, object->audio.pitch);
    henka_scene_document_writer_float(writer, object->audio.min_distance);
    henka_scene_document_writer_float(writer, object->audio.max_distance);
    henka_scene_document_writer_u32(writer, (uint32_t)object->behavior_count);
    for (size_t behavior_index = 0U;
         behavior_index < object->behavior_count;
         ++behavior_index)
    {
        const henka_scene_document_behavior* behavior = &object->behaviors[behavior_index];
        henka_scene_document_writer_u32(
            writer,
            behavior->enabled ? HENKA_SCENE_DOCUMENT_BEHAVIOR_FLAG_ENABLED : 0U);
        henka_scene_document_writer_u64(writer, behavior->id);
        henka_scene_document_writer_u32(writer, (uint32_t)behavior->language);
        henka_scene_document_writer_string(writer, behavior->asset_path);
    }
}

static bool henka_scene_document_reader_bytes(
    henka_scene_document_reader* reader,
    void* destination,
    size_t size)
{
    if (reader == NULL || reader->failed || destination == NULL ||
        size > reader->size - reader->position)
    {
        if (reader != NULL)
        {
            reader->failed = true;
        }
        return false;
    }
    memcpy(destination, reader->data + reader->position, size);
    reader->position += size;
    return true;
}

static bool henka_scene_document_reader_u16(henka_scene_document_reader* reader, uint16_t* out_value)
{
    uint8_t bytes[2];
    if (out_value == NULL || !henka_scene_document_reader_bytes(reader, bytes, sizeof(bytes))) return false;
    *out_value = henka_scene_document_read_u16(bytes);
    return true;
}

static bool henka_scene_document_reader_u32(henka_scene_document_reader* reader, uint32_t* out_value)
{
    uint8_t bytes[4];
    if (out_value == NULL || !henka_scene_document_reader_bytes(reader, bytes, sizeof(bytes))) return false;
    *out_value = henka_scene_document_read_u32(bytes);
    return true;
}

static bool henka_scene_document_reader_u64(henka_scene_document_reader* reader, uint64_t* out_value)
{
    uint8_t bytes[8];
    if (out_value == NULL || !henka_scene_document_reader_bytes(reader, bytes, sizeof(bytes))) return false;
    *out_value = henka_scene_document_read_u64(bytes);
    return true;
}

static bool henka_scene_document_reader_float(henka_scene_document_reader* reader, float* out_value)
{
    uint32_t bits;
    if (out_value == NULL || !henka_scene_document_reader_u32(reader, &bits)) return false;
    memcpy(out_value, &bits, sizeof(bits));
    return true;
}

static bool henka_scene_document_reader_string(
    henka_scene_document_reader* reader,
    char* destination,
    size_t capacity)
{
    uint16_t length;
    if (destination == NULL || capacity == 0U ||
        !henka_scene_document_reader_u16(reader, &length) ||
        (size_t)length >= capacity ||
        !henka_scene_document_reader_bytes(reader, destination, length))
    {
        if (reader != NULL) reader->failed = true;
        return false;
    }
    destination[length] = '\0';
    return true;
}

static bool henka_scene_document_decode_object(
    henka_scene_document_reader* reader,
    henka_scene_document_object* object,
    uint32_t format_version)
{
    uint32_t flags;
    uint32_t value;
    if (reader == NULL || object == NULL)
    {
        return false;
    }
    *object = henka_scene_document_object_default();
    if (!henka_scene_document_reader_u64(reader, &object->id) ||
        (format_version >= HENKA_SCENE_DOCUMENT_FORMAT_VERSION &&
            !henka_scene_document_reader_u64(reader, &object->parent_id)) ||
        !henka_scene_document_reader_u32(reader, &flags) ||
        (flags & ~HENKA_SCENE_DOCUMENT_KNOWN_FLAGS) != 0U ||
        (format_version < HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V3 &&
            (flags & (HENKA_SCENE_DOCUMENT_FLAG_AUDIO_ENABLED |
                HENKA_SCENE_DOCUMENT_FLAG_AUDIO_LOOPING |
                HENKA_SCENE_DOCUMENT_FLAG_AUDIO_SPATIAL)) != 0U) ||
        (format_version < HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V5 &&
            (flags & HENKA_SCENE_DOCUMENT_FLAG_AUDIO_STREAMING) != 0U) ||
        !henka_scene_document_reader_string(reader, object->name, sizeof(object->name)) ||
        !henka_scene_document_reader_float(reader, &object->transform.position.x) ||
        !henka_scene_document_reader_float(reader, &object->transform.position.y) ||
        !henka_scene_document_reader_float(reader, &object->transform.position.z) ||
        !henka_scene_document_reader_float(reader, &object->transform.rotation.x) ||
        !henka_scene_document_reader_float(reader, &object->transform.rotation.y) ||
        !henka_scene_document_reader_float(reader, &object->transform.rotation.z) ||
        !henka_scene_document_reader_float(reader, &object->transform.rotation.w) ||
        !henka_scene_document_reader_float(reader, &object->transform.scale.x) ||
        !henka_scene_document_reader_float(reader, &object->transform.scale.y) ||
        !henka_scene_document_reader_float(reader, &object->transform.scale.z) ||
        !henka_scene_document_reader_u32(reader, &value))
    {
        return false;
    }
    object->source.kind = (henka_scene_document_source_kind)value;
    if (!henka_scene_document_reader_u32(reader, &value)) return false;
    object->source.primitive = (henka_scene_document_primitive_kind)value;
    if (!henka_scene_document_reader_float(reader, &object->source.primitive_dimensions.x) ||
        !henka_scene_document_reader_float(reader, &object->source.primitive_dimensions.y) ||
        !henka_scene_document_reader_float(reader, &object->source.primitive_dimensions.z) ||
        !henka_scene_document_reader_string(reader, object->source.path, sizeof(object->source.path)) ||
        !henka_scene_document_reader_u32(reader, &value)) return false;
    object->source.asset_kind = (henka_scene_document_asset_kind)value;
    if (!henka_scene_document_reader_string(reader, object->renderer.material_path, sizeof(object->renderer.material_path)) ||
        !henka_scene_document_reader_float(reader, &object->renderer.base_color.x) ||
        !henka_scene_document_reader_float(reader, &object->renderer.base_color.y) ||
        !henka_scene_document_reader_float(reader, &object->renderer.base_color.z) ||
        !henka_scene_document_reader_float(reader, &object->renderer.base_color.w) ||
        !henka_scene_document_reader_float(reader, &object->renderer.metallic) ||
        !henka_scene_document_reader_float(reader, &object->renderer.roughness) ||
        !henka_scene_document_reader_float(reader, &object->renderer.emissive.x) ||
        !henka_scene_document_reader_float(reader, &object->renderer.emissive.y) ||
        !henka_scene_document_reader_float(reader, &object->renderer.emissive.z) ||
        !henka_scene_document_reader_float(reader, &object->renderer.emissive_strength) ||
        !henka_scene_document_reader_float(reader, &object->interaction.max_distance) ||
        !henka_scene_document_reader_string(reader, object->interaction.prompt, sizeof(object->interaction.prompt)) ||
        !henka_scene_document_reader_u32(reader, &value)) return false;
    object->physics.body_type = (henka_physics_body_type)value;
    if (!henka_scene_document_reader_u32(reader, &value)) return false;
    object->physics.shape = (henka_physics_shape_type)value;
    if (!henka_scene_document_reader_float(reader, &object->physics.collider_offset.x) ||
        !henka_scene_document_reader_float(reader, &object->physics.collider_offset.y) ||
        !henka_scene_document_reader_float(reader, &object->physics.collider_offset.z) ||
        !henka_scene_document_reader_float(reader, &object->physics.sphere_radius) ||
        !henka_scene_document_reader_float(reader, &object->physics.box_half_extents.x) ||
        !henka_scene_document_reader_float(reader, &object->physics.box_half_extents.y) ||
        !henka_scene_document_reader_float(reader, &object->physics.box_half_extents.z) ||
        !henka_scene_document_reader_float(reader, &object->physics.mass) ||
        !henka_scene_document_reader_float(reader, &object->physics.material.restitution) ||
        !henka_scene_document_reader_float(reader, &object->physics.material.static_friction) ||
        !henka_scene_document_reader_float(reader, &object->physics.material.dynamic_friction) ||
        !henka_scene_document_reader_float(reader, &object->physics.material.linear_damping) ||
        !henka_scene_document_reader_float(reader, &object->physics.material.angular_damping) ||
        !henka_scene_document_reader_u32(reader, &object->physics.layer) ||
        !henka_scene_document_reader_u32(reader, &object->physics.mask)) return false;
    if (format_version >= HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V3)
    {
        uint32_t audio_bus;
        if (!henka_scene_document_reader_string(
                reader,
                object->audio.clip_path,
                sizeof(object->audio.clip_path)) ||
            !henka_scene_document_reader_u32(reader, &audio_bus) ||
            !henka_scene_document_reader_float(reader, &object->audio.gain) ||
            !henka_scene_document_reader_float(reader, &object->audio.pitch) ||
            !henka_scene_document_reader_float(
                reader,
                &object->audio.min_distance) ||
            !henka_scene_document_reader_float(
                reader,
                &object->audio.max_distance))
        {
            return false;
        }
        object->audio.bus = (henka_audio_bus)audio_bus;
        object->audio.enabled =
            (flags & HENKA_SCENE_DOCUMENT_FLAG_AUDIO_ENABLED) != 0U;
        object->audio.looping =
            (flags & HENKA_SCENE_DOCUMENT_FLAG_AUDIO_LOOPING) != 0U;
        object->audio.spatial =
            (flags & HENKA_SCENE_DOCUMENT_FLAG_AUDIO_SPATIAL) != 0U;
        object->audio.streaming =
            (flags & HENKA_SCENE_DOCUMENT_FLAG_AUDIO_STREAMING) != 0U;
    }
    if (format_version >= HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V2)
    {
        uint32_t behavior_count;
        if (!henka_scene_document_reader_u32(reader, &behavior_count) ||
            behavior_count > HENKA_SCENE_DOCUMENT_MAX_BEHAVIORS_PER_OBJECT)
        {
            return false;
        }
        object->behavior_count = (size_t)behavior_count;
        for (size_t behavior_index = 0U;
             behavior_index < object->behavior_count;
             ++behavior_index)
        {
            uint32_t behavior_flags;
            uint32_t language;
            henka_scene_document_behavior* behavior = &object->behaviors[behavior_index];
            if (!henka_scene_document_reader_u32(reader, &behavior_flags) ||
                (behavior_flags & ~HENKA_SCENE_DOCUMENT_BEHAVIOR_KNOWN_FLAGS) != 0U ||
                !henka_scene_document_reader_u64(reader, &behavior->id) ||
                !henka_scene_document_reader_u32(reader, &language) ||
                !henka_scene_document_reader_string(
                    reader,
                    behavior->asset_path,
                    sizeof(behavior->asset_path)))
            {
                return false;
            }
            behavior->enabled = (behavior_flags & HENKA_SCENE_DOCUMENT_BEHAVIOR_FLAG_ENABLED) != 0U;
            behavior->language = (henka_script_language)language;
        }
    }
    object->visible = (flags & HENKA_SCENE_DOCUMENT_FLAG_VISIBLE) != 0U;
    object->renderer.enabled = (flags & HENKA_SCENE_DOCUMENT_FLAG_RENDERER_ENABLED) != 0U;
    object->renderer.material_override = (flags & HENKA_SCENE_DOCUMENT_FLAG_MATERIAL_OVERRIDE) != 0U;
    object->interaction.enabled = (flags & HENKA_SCENE_DOCUMENT_FLAG_INTERACTION_ENABLED) != 0U;
    object->physics.enabled = (flags & HENKA_SCENE_DOCUMENT_FLAG_PHYSICS_ENABLED) != 0U;
    object->physics.is_trigger = (flags & HENKA_SCENE_DOCUMENT_FLAG_TRIGGER) != 0U;
    return henka_scene_document_validate_object(object) == HENKA_SUCCESS;
}

static bool henka_scene_document_make_payload(
    const henka_scene_document_storage* storage,
    uint8_t** out_payload,
    size_t* out_size)
{
    size_t payload_size;
    size_t index;
    henka_scene_document_writer writer;
    uint8_t* payload;
    if (!henka_scene_document_payload_size(storage, &payload_size) || out_payload == NULL || out_size == NULL)
    {
        return false;
    }
    payload = (uint8_t*)henka_malloc(payload_size == 0U ? 1U : payload_size);
    if (payload == NULL)
    {
        return false;
    }
    writer = (henka_scene_document_writer){payload, payload_size, 0U, false};
    for (index = 0U; index < storage->object_count; ++index)
    {
        henka_scene_document_encode_object(&writer, &storage->objects[index]);
    }
    henka_scene_document_writer_float(&writer, storage->audio_listener.position.x);
    henka_scene_document_writer_float(&writer, storage->audio_listener.position.y);
    henka_scene_document_writer_float(&writer, storage->audio_listener.position.z);
    henka_scene_document_writer_float(&writer, storage->audio_listener.forward.x);
    henka_scene_document_writer_float(&writer, storage->audio_listener.forward.y);
    henka_scene_document_writer_float(&writer, storage->audio_listener.forward.z);
    henka_scene_document_writer_float(&writer, storage->audio_listener.up.x);
    henka_scene_document_writer_float(&writer, storage->audio_listener.up.y);
    henka_scene_document_writer_float(&writer, storage->audio_listener.up.z);
    if (writer.failed || writer.position != payload_size)
    {
        henka_free(payload);
        return false;
    }
    *out_payload = payload;
    *out_size = payload_size;
    return true;
}

static bool henka_scene_document_atomic_replace(const char* temporary_path, const char* path)
{
#if defined(_WIN32)
    return MoveFileExA(temporary_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary_path, path) == 0;
#endif
}

static FILE* henka_scene_document_open_file(const char* path, const char* mode)
{
    FILE* file = NULL;
#if defined(_WIN32)
    if (fopen_s(&file, path, mode) != 0) return NULL;
#else
    file = fopen(path, mode);
#endif
    return file;
}

henka_result henka_scene_document_save_file(
    const henka_scene_document* document,
    const char* project_root,
    const char* relative_path)
{
    char* path = NULL;
    char* temporary_relative_path = NULL;
    char* temporary_path = NULL;
    uint8_t* payload = NULL;
    size_t payload_size = 0U;
    uint8_t header[HENKA_SCENE_DOCUMENT_HEADER_BYTES];
    FILE* file = NULL;
    henka_result result = HENKA_SUCCESS;
    size_t relative_length;

    if (document == NULL || project_root == NULL || relative_path == NULL || relative_path[0] == '\0' ||
        henka_scene_document_validate(document) != HENKA_SUCCESS)
    {
        henka_free(path);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_path_resolve_confined(project_root, relative_path, &path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(path);
        return result;
    }
    if (!henka_scene_document_make_payload(document->storage, &payload, &payload_size))
    {
        henka_free(path);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    relative_length = strlen(relative_path);
    if (relative_length > SIZE_MAX - 5U)
    {
        result = HENKA_ERROR_NUMERIC_RANGE;
        goto save_cleanup;
    }
    temporary_relative_path = (char*)henka_malloc(relative_length + 5U);
    if (temporary_relative_path == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto save_cleanup;
    }
    memcpy(temporary_relative_path, relative_path, relative_length);
    memcpy(temporary_relative_path + relative_length, ".tmp", 5U);
    result = henka_path_resolve_confined(project_root, temporary_relative_path, &temporary_path);
    if (result != HENKA_SUCCESS)
    {
        goto save_cleanup;
    }
    result = henka_path_ensure_parent_directory(path);
    if (result != HENKA_SUCCESS)
    {
        goto save_cleanup;
    }
    memset(header, 0, sizeof(header));
    header[0] = HENKA_SCENE_DOCUMENT_MAGIC_0;
    header[1] = HENKA_SCENE_DOCUMENT_MAGIC_1;
    header[2] = HENKA_SCENE_DOCUMENT_MAGIC_2;
    header[3] = HENKA_SCENE_DOCUMENT_MAGIC_3;
    henka_scene_document_write_u32(header + 4U, HENKA_SCENE_DOCUMENT_FORMAT_VERSION);
    henka_scene_document_write_u32(header + 8U, HENKA_SCENE_DOCUMENT_HEADER_BYTES);
    henka_scene_document_write_u64(header + 12U, (uint64_t)payload_size);
    henka_scene_document_write_u32(header + 20U, (uint32_t)document->storage->object_count);
    henka_scene_document_write_u64(header + 24U, document->storage->next_id);
    henka_scene_document_write_u32(header + 32U, henka_scene_document_checksum(payload, payload_size));
    file = henka_scene_document_open_file(temporary_path, "wb");
    if (file == NULL)
    {
        result = HENKA_ERROR_PLATFORM;
        goto save_cleanup;
    }
    if (fwrite(header, 1U, sizeof(header), file) != sizeof(header) ||
        fwrite(payload, 1U, payload_size, file) != payload_size || fflush(file) != 0)
    {
        fclose(file);
        file = NULL;
        result = HENKA_ERROR_PLATFORM;
        goto save_cleanup;
    }
    if (fclose(file) != 0)
    {
        file = NULL;
        result = HENKA_ERROR_PLATFORM;
        goto save_cleanup;
    }
    file = NULL;
    if (!henka_scene_document_atomic_replace(temporary_path, path))
    {
        result = HENKA_ERROR_PLATFORM;
    }

save_cleanup:
    if (file != NULL) fclose(file);
    if (result != HENKA_SUCCESS && temporary_path != NULL) remove(temporary_path);
    henka_free(payload);
    henka_free(temporary_path);
    henka_free(temporary_relative_path);
    henka_free(path);
    return result;
}

static henka_result henka_scene_document_read_file(
    const char* path,
    uint8_t** out_data,
    size_t* out_size)
{
    FILE* file;
    long length;
    uint8_t* data;
    size_t size;
    if (path == NULL || out_data == NULL || out_size == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_data = NULL;
    *out_size = 0U;
    file = henka_scene_document_open_file(path, "rb");
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0)
    {
        if (file != NULL) fclose(file);
        return HENKA_ERROR_PLATFORM;
    }
    length = ftell(file);
    if (length < (long)HENKA_SCENE_DOCUMENT_HEADER_BYTES ||
        (uint64_t)length > HENKA_SCENE_DOCUMENT_MAX_FILE_BYTES ||
        fseek(file, 0L, SEEK_SET) != 0)
    {
        fclose(file);
        return HENKA_ERROR_LIMIT;
    }
    size = (size_t)length;
    data = (uint8_t*)henka_malloc(size);
    if (data == NULL)
    {
        fclose(file);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (fread(data, 1U, size, file) != size || fclose(file) != 0)
    {
        henka_free(data);
        return HENKA_ERROR_PLATFORM;
    }
    *out_data = data;
    *out_size = size;
    return HENKA_SUCCESS;
}

henka_result henka_scene_document_load_file(
    henka_scene_document* document,
    const char* project_root,
    const char* relative_path)
{
    char* path = NULL;
    uint8_t* data = NULL;
    size_t size = 0U;
    henka_scene_document_storage* candidate = NULL;
    henka_scene_document_reader reader;
    uint64_t payload_size;
    uint64_t next_id;
    uint32_t format_version;
    uint32_t object_count;
    uint32_t checksum;
    size_t index;
    henka_result result;

    if (document == NULL || document->storage == NULL || project_root == NULL || relative_path == NULL ||
        relative_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_path_resolve_confined(project_root, relative_path, &path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(path);
        return result;
    }
    result = henka_scene_document_read_file(path, &data, &size);
    henka_free(path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    format_version = henka_scene_document_read_u32(data + 4U);
    if (data[0] != HENKA_SCENE_DOCUMENT_MAGIC_0 || data[1] != HENKA_SCENE_DOCUMENT_MAGIC_1 ||
        data[2] != HENKA_SCENE_DOCUMENT_MAGIC_2 || data[3] != HENKA_SCENE_DOCUMENT_MAGIC_3 ||
        (format_version != HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION &&
            format_version != HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V2 &&
            format_version != HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V3 &&
            format_version != HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V4 &&
            format_version != HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V5 &&
            format_version != HENKA_SCENE_DOCUMENT_FORMAT_VERSION) ||
        henka_scene_document_read_u32(data + 8U) != HENKA_SCENE_DOCUMENT_HEADER_BYTES ||
        henka_scene_document_read_u32(data + 36U) != 0U)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto load_cleanup;
    }
    payload_size = henka_scene_document_read_u64(data + 12U);
    object_count = henka_scene_document_read_u32(data + 20U);
    next_id = henka_scene_document_read_u64(data + 24U);
    checksum = henka_scene_document_read_u32(data + 32U);
    if (payload_size > HENKA_SCENE_DOCUMENT_MAX_FILE_BYTES - HENKA_SCENE_DOCUMENT_HEADER_BYTES ||
        payload_size != (uint64_t)(size - HENKA_SCENE_DOCUMENT_HEADER_BYTES) ||
        object_count > HENKA_SCENE_DOCUMENT_MAX_OBJECTS ||
        checksum != henka_scene_document_checksum(data + HENKA_SCENE_DOCUMENT_HEADER_BYTES, (size_t)payload_size))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto load_cleanup;
    }
    candidate = (henka_scene_document_storage*)henka_calloc(1U, sizeof(*candidate));
    if (candidate == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto load_cleanup;
    }
    candidate->next_id = next_id;
    candidate->audio_listener = henka_audio_listener_default();
    reader = (henka_scene_document_reader){
        data + HENKA_SCENE_DOCUMENT_HEADER_BYTES,
        (size_t)payload_size,
        0U,
        false};
    for (index = 0U; index < (size_t)object_count; ++index)
    {
        if (!henka_scene_document_decode_object(
                &reader,
                &candidate->objects[index],
                format_version))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto load_cleanup;
        }
        ++candidate->object_count;
    }
    if (format_version >= HENKA_SCENE_DOCUMENT_LEGACY_FORMAT_VERSION_V4 &&
        (!henka_scene_document_reader_float(
            &reader, &candidate->audio_listener.position.x) ||
            !henka_scene_document_reader_float(
                &reader, &candidate->audio_listener.position.y) ||
            !henka_scene_document_reader_float(
                &reader, &candidate->audio_listener.position.z) ||
            !henka_scene_document_reader_float(
                &reader, &candidate->audio_listener.forward.x) ||
            !henka_scene_document_reader_float(
                &reader, &candidate->audio_listener.forward.y) ||
            !henka_scene_document_reader_float(
                &reader, &candidate->audio_listener.forward.z) ||
            !henka_scene_document_reader_float(
                &reader, &candidate->audio_listener.up.x) ||
            !henka_scene_document_reader_float(
                &reader, &candidate->audio_listener.up.y) ||
            !henka_scene_document_reader_float(
                &reader, &candidate->audio_listener.up.z)))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto load_cleanup;
    }
    if (reader.failed || reader.position != reader.size ||
        henka_scene_document_validate_storage(candidate) != HENKA_SUCCESS)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto load_cleanup;
    }
    {
        henka_scene_document_storage* previous = document->storage;
        document->storage = candidate;
        candidate = previous;
    }
    result = HENKA_SUCCESS;

load_cleanup:
    henka_free(candidate);
    henka_free(data);
    return result;
}

static bool henka_scene_document_inspection_append(
    char* buffer,
    size_t capacity,
    size_t* inout_size,
    const char* format,
    ...)
{
    char line[256];
    va_list arguments;
    int written;
    if (buffer == NULL || inout_size == NULL || format == NULL || *inout_size >= capacity)
    {
        return false;
    }
    va_start(arguments, format);
    written = vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= sizeof(line) ||
        (size_t)written > capacity - *inout_size - 1U)
    {
        return false;
    }
    memcpy(buffer + *inout_size, line, (size_t)written);
    *inout_size += (size_t)written;
    buffer[*inout_size] = '\0';
    return true;
}

henka_result henka_scene_document_format_inspection(
    const henka_scene_document* document,
    char* buffer,
    size_t buffer_capacity,
    size_t* out_size)
{
    size_t size = 0U;
    size_t index;
    if (document == NULL || buffer == NULL || buffer_capacity == 0U || out_size == NULL ||
        henka_scene_document_validate(document) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    buffer[0] = '\0';
    if (!henka_scene_document_inspection_append(
            buffer,
            buffer_capacity,
            &size,
            "HSCN version=%u objects=%zu next_id=%llu\n",
            (unsigned int)HENKA_SCENE_DOCUMENT_FORMAT_VERSION,
            document->storage->object_count,
            (unsigned long long)document->storage->next_id))
    {
        return HENKA_ERROR_LIMIT;
    }
    for (index = 0U; index < document->storage->object_count; ++index)
    {
        const henka_scene_document_object* object = &document->storage->objects[index];
        if (!henka_scene_document_inspection_append(
                buffer,
                buffer_capacity,
                &size,
                "object id=%llu source=%u renderer=%u interaction=%u physics=%u behaviors=%zu\n",
                (unsigned long long)object->id,
                (unsigned int)object->source.kind,
                object->renderer.enabled ? 1U : 0U,
                object->interaction.enabled ? 1U : 0U,
                object->physics.enabled ? 1U : 0U,
                object->behavior_count))
        {
            return HENKA_ERROR_LIMIT;
        }
    }
    *out_size = size;
    return HENKA_SUCCESS;
}
