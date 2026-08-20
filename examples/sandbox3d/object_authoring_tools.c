#include "object_authoring_tools.h"

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/model.h>
#include <henka/persistence.h>

#include "../../engine/src/core/checked.h"

#define SANDBOX3D_AUTHORING_MAX_HISTORY_STEPS 64U
#define SANDBOX3D_AUTHORING_IMPORT_VERTEX_RESERVE 256U
#define SANDBOX3D_AUTHORING_IMPORT_EDGE_RESERVE 512U
#define SANDBOX3D_AUTHORING_IMPORT_FACE_RESERVE 128U
#define SANDBOX3D_AUTHORING_MAX_REGION_TRANSFORMS 16U
#define SANDBOX3D_AUTHORING_IMPORT_WELD_BUCKET_COUNT 8192U
#define SANDBOX3D_AUTHORING_IMPORT_WELD_EPSILON 0.0001f

struct sandbox3d_authoring_object
{
    henka_engine* engine;
    henka_scene* scene;
    henka_entity entity;
    henka_authoring_mesh* mesh;
    henka_authoring_mesh_history* history;
    size_t history_steps;
    henka_mesh* render_mesh;
    henka_mesh* previous_scene_mesh;
    bool had_previous_bounds;
    henka_bounds previous_bounds;
    henka_physics_world* physics_world;
    henka_physics_body_id physics_body;
    henka_authoring_face_id selected_face;
    henka_authoring_face_id* selection_history;
    size_t selection_history_capacity;
    size_t selection_history_count;
    size_t selection_history_cursor;
    sandbox3d_authoring_selection_mode selection_mode;
    uint32_t active_component_id;
    /* Bitsets are authoritative membership; sorted ID caches provide stable
     * ascending iteration for the existing authoring consumers. */
    uint64_t* selected_vertex_bits;
    size_t selected_vertex_word_count;
    size_t selected_vertex_max_id;
    uint32_t* selected_vertices;
    size_t selected_vertex_capacity;
    size_t selected_vertex_count;
    uint64_t* selected_edge_bits;
    size_t selected_edge_word_count;
    size_t selected_edge_max_id;
    uint32_t* selected_edges;
    size_t selected_edge_capacity;
    size_t selected_edge_count;
    uint64_t* selected_face_bits;
    size_t selected_face_word_count;
    size_t selected_face_max_id;
    uint32_t* selected_faces;
    size_t selected_face_capacity;
    size_t selected_face_count;
    char* source_path;
    float merge_distance;
};

static henka_result sandbox3d_authoring_allocate_id_scratch(
    size_t capacity,
    uint32_t** out_ids);

static int64_t sandbox3d_authoring_import_weld_quantize(float value)
{
    return (int64_t)floorf(value / SANDBOX3D_AUTHORING_IMPORT_WELD_EPSILON + 0.5f);
}

static size_t sandbox3d_authoring_import_weld_bucket(henka_vec3 position)
{
    const uint64_t x = (uint64_t)sandbox3d_authoring_import_weld_quantize(position.x);
    const uint64_t y = (uint64_t)sandbox3d_authoring_import_weld_quantize(position.y);
    const uint64_t z = (uint64_t)sandbox3d_authoring_import_weld_quantize(position.z);
    const uint64_t hash =
        x * UINT64_C(73856093) ^
        y * UINT64_C(19349663) ^
        z * UINT64_C(83492791);
    return (size_t)(hash % SANDBOX3D_AUTHORING_IMPORT_WELD_BUCKET_COUNT);
}

static bool sandbox3d_authoring_import_weld_positions_match(
    henka_vec3 left,
    henka_vec3 right)
{
    const henka_vec3 delta = henka_vec3_subtract(left, right);
    const float epsilon = SANDBOX3D_AUTHORING_IMPORT_WELD_EPSILON;
    return fabsf(delta.x) <= epsilon && fabsf(delta.y) <= epsilon && fabsf(delta.z) <= epsilon;
}

static henka_result sandbox3d_authoring_set_source_path(
    sandbox3d_authoring_object* object,
    const char* path)
{
    char* copy;
    size_t length;

    if (object == NULL || path == NULL || path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    length = strlen(path);
    if (length > 767U)
    {
        return HENKA_ERROR_LIMIT;
    }
    copy = henka_calloc(length + 1U, sizeof(*copy));
    if (copy == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    memcpy(copy, path, length + 1U);
    henka_free(object->source_path);
    object->source_path = copy;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_authoring_set_physics_bounds(
    sandbox3d_authoring_object* object,
    const henka_bounds* bounds)
{
    henka_physics_body_state body_state;

    if (object == NULL || bounds == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (object->physics_world == NULL || object->physics_body == HENKA_INVALID_PHYSICS_BODY_ID)
    {
        return HENKA_SUCCESS;
    }
    if (henka_physics_body_get_state(
            object->physics_world, object->physics_body, &body_state) != HENKA_SUCCESS ||
        body_state.collider.shape != HENKA_PHYSICS_SHAPE_BOX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    body_state.collider.offset = bounds->center;
    body_state.collider.data.box.half_extents = bounds->extents;
    return henka_physics_body_set_collider(
        object->physics_world, object->physics_body, body_state.collider);
}

static henka_result sandbox3d_authoring_get_physics_collider(
    const sandbox3d_authoring_object* object,
    henka_physics_collider_desc* out_collider)
{
    henka_physics_body_state body_state;

    if (object == NULL || out_collider == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (object->physics_world == NULL || object->physics_body == HENKA_INVALID_PHYSICS_BODY_ID)
    {
        *out_collider = (henka_physics_collider_desc){0};
        return HENKA_SUCCESS;
    }
    if (henka_physics_body_get_state(
            object->physics_world, object->physics_body, &body_state) != HENKA_SUCCESS ||
        body_state.collider.shape != HENKA_PHYSICS_SHAPE_BOX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_collider = body_state.collider;
    return HENKA_SUCCESS;
}

static void sandbox3d_authoring_restore_physics_collider(
    const sandbox3d_authoring_object* object,
    const henka_physics_collider_desc* collider)
{
    if (object != NULL && collider != NULL && object->physics_world != NULL &&
        object->physics_body != HENKA_INVALID_PHYSICS_BODY_ID)
    {
        (void)henka_physics_body_set_collider(
            object->physics_world, object->physics_body, *collider);
    }
}

static bool sandbox3d_authoring_finite_vec3(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static henka_result sandbox3d_authoring_evaluate_render(
    sandbox3d_authoring_object* object,
    const henka_authoring_mesh* source,
    henka_mesh** out_render_mesh,
    henka_bounds* out_bounds)
{
    henka_authoring_render_vertex* render_vertices = NULL;
    uint32_t* render_indices = NULL;
    henka_model_vertex* model_vertices = NULL;
    uint32_t* model_indices = NULL;
    henka_authoring_render_data render;
    henka_model_data model;
    size_t vertex_count = 0U;
    size_t index_count = 0U;
    size_t face_id;
    size_t vertex_index;
    size_t index;
    henka_vec3 minimum = {0.0f, 0.0f, 0.0f};
    henka_vec3 maximum = {0.0f, 0.0f, 0.0f};
    henka_result result;

    if (object == NULL || source == NULL || out_render_mesh == NULL || out_bounds == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(&model, 0, sizeof(model));
    *out_render_mesh = NULL;
    if (!henka_authoring_mesh_validate(source))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (face_id = 1U; face_id <= HENKA_AUTHORING_MESH_HARD_MAX_FACES; ++face_id)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            source, (henka_authoring_face_id)face_id);
        if (face == NULL)
        {
            continue;
        }
        if (face->corner_count < 3U || face->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        vertex_count += face->corner_count;
        index_count += (face->corner_count - 2U) * 3U;
    }
    if (vertex_count == 0U ||
        vertex_count > (size_t)HENKA_AUTHORING_MESH_HARD_MAX_FACES * HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS ||
        index_count == 0U ||
        index_count > (size_t)HENKA_AUTHORING_MESH_HARD_MAX_FACES *
            (HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS - 2U) * 3U ||
        vertex_count > UINT32_MAX || index_count > UINT32_MAX)
    {
        return HENKA_ERROR_LIMIT;
    }

    render_vertices = henka_calloc(vertex_count, sizeof(*render_vertices));
    render_indices = henka_calloc(index_count, sizeof(*render_indices));
    model_vertices = henka_calloc(vertex_count, sizeof(*model_vertices));
    model_indices = henka_calloc(index_count, sizeof(*model_indices));
    if (render_vertices == NULL || render_indices == NULL ||
        model_vertices == NULL || model_indices == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }

    render = (henka_authoring_render_data){
        render_vertices, vertex_count, 0U, render_indices, index_count, 0U};
    result = henka_authoring_mesh_evaluate(source, &render);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (vertex_index = 0U; vertex_index < render.vertex_count; ++vertex_index)
    {
        const henka_authoring_render_vertex* vertex = &render.vertices[vertex_index];
        if (!sandbox3d_authoring_finite_vec3(vertex->position) ||
            !sandbox3d_authoring_finite_vec3(vertex->normal) ||
            !isfinite(vertex->uv.x) || !isfinite(vertex->uv.y))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        if (vertex_index == 0U)
        {
            minimum = vertex->position;
            maximum = vertex->position;
        }
        else
        {
            minimum.x = fminf(minimum.x, vertex->position.x);
            minimum.y = fminf(minimum.y, vertex->position.y);
            minimum.z = fminf(minimum.z, vertex->position.z);
            maximum.x = fmaxf(maximum.x, vertex->position.x);
            maximum.y = fmaxf(maximum.y, vertex->position.y);
            maximum.z = fmaxf(maximum.z, vertex->position.z);
        }
        model_vertices[vertex_index].position = vertex->position;
        model_vertices[vertex_index].normal = vertex->normal;
        model_vertices[vertex_index].uv = vertex->uv;
        model_vertices[vertex_index].color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};
        model_vertices[vertex_index].tangent = vertex->tangent;
        model_vertices[vertex_index].tangent_valid = false;
        model_vertices[vertex_index].material_region = vertex->material_region;
    }
    for (index = 0U; index < render.index_count; ++index)
    {
        if (render.indices[index] >= render.vertex_count)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        model_indices[index] = render.indices[index];
    }
    model.vertices = model_vertices;
    model.vertex_count = (uint32_t)render.vertex_count;
    model.indices = model_indices;
    model.index_count = (uint32_t)render.index_count;
    result = henka_mesh_create_from_model_data(object->engine, &model, out_render_mesh);
    if (result == HENKA_SUCCESS)
    {
        out_bounds->center = (henka_vec3){
            (minimum.x + maximum.x) * 0.5f,
            (minimum.y + maximum.y) * 0.5f,
            (minimum.z + maximum.z) * 0.5f};
        out_bounds->extents = (henka_vec3){
            (maximum.x - minimum.x) * 0.5f,
            (maximum.y - minimum.y) * 0.5f,
            (maximum.z - minimum.z) * 0.5f};
    }

cleanup:
    henka_free(model_indices);
    henka_free(model_vertices);
    henka_free(render_indices);
    henka_free(render_vertices);
    return result;
}

static bool sandbox3d_authoring_selection_word_count(
    size_t max_slot_id,
    size_t* out_word_count)
{
    size_t bit_count;
    size_t rounded_count;
    if (out_word_count == NULL || max_slot_id == 0U ||
        max_slot_id > HENKA_AUTHORING_MESH_HARD_MAX_EDGES ||
        !henka_checked_size_add(max_slot_id, 1U, &bit_count) ||
        !henka_checked_size_add(bit_count, 63U, &rounded_count))
    {
        return false;
    }
    *out_word_count = rounded_count / 64U;
    return *out_word_count > 0U;
}

static void sandbox3d_authoring_selection_destroy(
    uint64_t** bits,
    uint32_t** ids)
{
    if (bits != NULL)
    {
        henka_free(*bits);
        *bits = NULL;
    }
    if (ids != NULL)
    {
        henka_free(*ids);
        *ids = NULL;
    }
}

static henka_result sandbox3d_authoring_selection_build_resized(
    const uint32_t* old_ids,
    size_t old_count,
    size_t new_max_slot_id,
    uint64_t** out_bits,
    size_t* out_word_count,
    uint32_t** out_ids,
    size_t* out_id_capacity,
    size_t* out_selected_count)
{
    uint64_t* next_bits = NULL;
    uint32_t* next_ids = NULL;
    size_t next_word_count;
    size_t next_count = 0U;
    size_t next_id_capacity;
    size_t bytes;
    size_t index;

    if ((old_count > 0U && old_ids == NULL) || out_bits == NULL ||
        out_word_count == NULL || out_ids == NULL ||
        out_id_capacity == NULL || out_selected_count == NULL ||
        !sandbox3d_authoring_selection_word_count(new_max_slot_id, &next_word_count))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!henka_checked_size_multiply(next_word_count, sizeof(*next_bits), &bytes))
    {
        return HENKA_ERROR_LIMIT;
    }
    next_bits = henka_calloc(next_word_count, sizeof(*next_bits));
    if (next_bits == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    next_id_capacity = old_count < new_max_slot_id ? old_count : new_max_slot_id;
    if (next_id_capacity > 0U)
    {
        if (!henka_checked_size_multiply(next_id_capacity, sizeof(*next_ids), &bytes))
        {
            henka_free(next_bits);
            return HENKA_ERROR_LIMIT;
        }
        next_ids = henka_calloc(next_id_capacity, sizeof(*next_ids));
        if (next_ids == NULL)
        {
            henka_free(next_bits);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }
    for (index = 0U; index < old_count; ++index)
    {
        const uint32_t id = old_ids[index];
        size_t word;
        uint64_t mask;
        if (id == HENKA_AUTHORING_INVALID_ID || (size_t)id > new_max_slot_id)
        {
            continue;
        }
        word = (size_t)id / 64U;
        mask = UINT64_C(1) << ((size_t)id % 64U);
        next_bits[word] |= mask;
        next_ids[next_count++] = id;
    }
    *out_bits = next_bits;
    *out_word_count = next_word_count;
    *out_ids = next_ids;
    *out_id_capacity = next_id_capacity;
    *out_selected_count = next_count;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_authoring_selection_reserve_ids(
    uint32_t** ids,
    size_t* id_capacity,
    size_t required,
    size_t maximum)
{
    size_t next_capacity;
    size_t bytes;
    uint32_t* next_ids;
    if (ids == NULL || id_capacity == NULL || required > maximum)
    {
        return HENKA_ERROR_LIMIT;
    }
    if (required <= *id_capacity)
    {
        return HENKA_SUCCESS;
    }
    next_capacity = *id_capacity == 0U ? 16U : *id_capacity;
    if (next_capacity > maximum) next_capacity = maximum;
    while (next_capacity < required)
    {
        if (next_capacity > maximum / 2U)
        {
            next_capacity = maximum;
        }
        else
        {
            next_capacity *= 2U;
        }
        if (next_capacity < required && next_capacity == maximum)
        {
            return HENKA_ERROR_LIMIT;
        }
    }
    if (!henka_checked_size_multiply(next_capacity, sizeof(*next_ids), &bytes))
    {
        return HENKA_ERROR_LIMIT;
    }
    next_ids = henka_calloc(next_capacity, sizeof(*next_ids));
    if (next_ids == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (*ids != NULL && *id_capacity > 0U)
    {
        if (!henka_checked_size_multiply(*id_capacity, sizeof(*next_ids), &bytes))
        {
            henka_free(next_ids);
            return HENKA_ERROR_LIMIT;
        }
        memcpy(next_ids, *ids, bytes);
    }
    henka_free(*ids);
    *ids = next_ids;
    *id_capacity = next_capacity;
    return HENKA_SUCCESS;
}

static bool sandbox3d_authoring_selection_contains(
    const uint64_t* bits,
    size_t word_count,
    size_t max_slot_id,
    uint32_t id)
{
    size_t word;
    if (bits == NULL || id == HENKA_AUTHORING_INVALID_ID ||
        (size_t)id > max_slot_id)
    {
        return false;
    }
    word = (size_t)id / 64U;
    return word < word_count &&
        (bits[word] & (UINT64_C(1) << ((size_t)id % 64U))) != 0U;
}

static henka_result sandbox3d_authoring_selection_add(
    uint64_t* bits,
    size_t word_count,
    size_t max_slot_id,
    uint32_t** ids,
    size_t* id_capacity,
    size_t* selected_count,
    uint32_t id)
{
    size_t index;
    size_t required;
    size_t word;
    henka_result reserve_result;
    if (bits == NULL || ids == NULL || id_capacity == NULL || selected_count == NULL ||
        id == HENKA_AUTHORING_INVALID_ID || (size_t)id > max_slot_id ||
        !henka_checked_size_add(*selected_count, 1U, &required))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (sandbox3d_authoring_selection_contains(bits, word_count, max_slot_id, id))
    {
        return HENKA_SUCCESS;
    }
    word = (size_t)id / 64U;
    if (word >= word_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    reserve_result = sandbox3d_authoring_selection_reserve_ids(
        ids, id_capacity, required, max_slot_id);
    if (reserve_result != HENKA_SUCCESS)
    {
        return reserve_result;
    }
    index = *selected_count;
    while (index > 0U && (*ids)[index - 1U] > id)
    {
        (*ids)[index] = (*ids)[index - 1U];
        --index;
    }
    (*ids)[index] = id;
    ++(*selected_count);
    bits[word] |= UINT64_C(1) << ((size_t)id % 64U);
    return HENKA_SUCCESS;
}

static void sandbox3d_authoring_selection_clear(
    uint64_t* bits,
    size_t word_count,
    size_t* selected_count)
{
    if (bits != NULL && selected_count != NULL)
    {
        memset(bits, 0, word_count * sizeof(*bits));
        *selected_count = 0U;
    }
}

static void sandbox3d_authoring_repair_selection(sandbox3d_authoring_object* object)
{
    size_t face_id;
    size_t selected_index;
    if (object == NULL)
    {
        return;
    }
    if (object->selected_face == HENKA_AUTHORING_INVALID_ID &&
        object->selected_face_count == 0U &&
        object->selected_vertex_count == 0U &&
        object->selected_edge_count == 0U)
    {
        object->active_component_id = HENKA_AUTHORING_INVALID_ID;
        return;
    }
    if (henka_authoring_mesh_get_face(object->mesh, object->selected_face) == NULL)
    {
        object->selected_face = HENKA_AUTHORING_INVALID_ID;
        for (face_id = 1U; face_id <= HENKA_AUTHORING_MESH_HARD_MAX_FACES; ++face_id)
        {
            if (henka_authoring_mesh_get_face(object->mesh, (henka_authoring_face_id)face_id) != NULL)
            {
                object->selected_face = (henka_authoring_face_id)face_id;
                break;
            }
        }
    }
    if (object->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE ||
        object->selected_face == HENKA_AUTHORING_INVALID_ID)
    {
        return;
    }
    for (selected_index = 0U; selected_index < object->selected_face_count; ++selected_index)
    {
        if (object->selected_faces[selected_index] == object->selected_face)
        {
            return;
        }
    }
    (void)sandbox3d_authoring_selection_add(
        object->selected_face_bits,
        object->selected_face_word_count,
        object->selected_face_max_id,
        &object->selected_faces,
        &object->selected_face_capacity,
        &object->selected_face_count,
        object->selected_face);
}

static henka_result sandbox3d_authoring_resize_selection_for_mesh(
    sandbox3d_authoring_object* object,
    const henka_authoring_mesh* mesh)
{
    const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
    uint64_t* next_vertex_bits = NULL;
    uint64_t* next_edge_bits = NULL;
    uint64_t* next_face_bits = NULL;
    uint32_t* next_vertices = NULL;
    uint32_t* next_edges = NULL;
    uint32_t* next_faces = NULL;
    size_t next_vertex_words = 0U;
    size_t next_edge_words = 0U;
    size_t next_face_words = 0U;
    size_t next_vertex_capacity = 0U;
    size_t next_edge_capacity = 0U;
    size_t next_face_capacity = 0U;
    size_t next_vertex_count = 0U;
    size_t next_edge_count = 0U;
    size_t next_face_count = 0U;
    henka_result result;
    if (object == NULL || mesh == NULL || !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (object->selected_vertex_max_id == desc.max_vertices &&
        object->selected_edge_max_id == desc.max_edges &&
        object->selected_face_max_id == desc.max_faces)
    {
        return HENKA_SUCCESS;
    }
    result = sandbox3d_authoring_selection_build_resized(
        object->selected_vertices,
        object->selected_vertex_count,
        desc.max_vertices,
        &next_vertex_bits,
        &next_vertex_words,
        &next_vertices,
        &next_vertex_capacity,
        &next_vertex_count);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_authoring_selection_build_resized(
        object->selected_edges,
        object->selected_edge_count,
        desc.max_edges,
        &next_edge_bits,
        &next_edge_words,
        &next_edges,
        &next_edge_capacity,
        &next_edge_count);
    if (result != HENKA_SUCCESS)
    {
        henka_free(next_vertex_bits);
        henka_free(next_vertices);
        return result;
    }
    result = sandbox3d_authoring_selection_build_resized(
        object->selected_faces,
        object->selected_face_count,
        desc.max_faces,
        &next_face_bits,
        &next_face_words,
        &next_faces,
        &next_face_capacity,
        &next_face_count);
    if (result != HENKA_SUCCESS)
    {
        henka_free(next_vertex_bits);
        henka_free(next_vertices);
        henka_free(next_edge_bits);
        henka_free(next_edges);
        return result;
    }
    /* The vertex build was intentionally kept separate so a failure in any
     * component type leaves all existing selection storage untouched. */
    sandbox3d_authoring_selection_destroy(
        &object->selected_vertex_bits, &object->selected_vertices);
    sandbox3d_authoring_selection_destroy(
        &object->selected_edge_bits, &object->selected_edges);
    sandbox3d_authoring_selection_destroy(
        &object->selected_face_bits, &object->selected_faces);
    object->selected_vertex_bits = next_vertex_bits;
    object->selected_vertex_word_count = next_vertex_words;
    object->selected_vertex_max_id = desc.max_vertices;
    object->selected_vertices = next_vertices;
    object->selected_vertex_capacity = next_vertex_capacity;
    object->selected_vertex_count = next_vertex_count;
    object->selected_edge_bits = next_edge_bits;
    object->selected_edge_word_count = next_edge_words;
    object->selected_edge_max_id = desc.max_edges;
    object->selected_edges = next_edges;
    object->selected_edge_capacity = next_edge_capacity;
    object->selected_edge_count = next_edge_count;
    object->selected_face_bits = next_face_bits;
    object->selected_face_word_count = next_face_words;
    object->selected_face_max_id = desc.max_faces;
    object->selected_faces = next_faces;
    object->selected_face_capacity = next_face_capacity;
    object->selected_face_count = next_face_count;
    return HENKA_SUCCESS;
}

static uint32_t* sandbox3d_authoring_selected_ids(
    sandbox3d_authoring_object* object,
    size_t* out_count)
{
    if (out_count != NULL) *out_count = 0U;
    if (object == NULL || out_count == NULL)
    {
        return NULL;
    }
    switch (object->selection_mode)
    {
        case SANDBOX3D_AUTHORING_SELECTION_VERTEX:
            *out_count = object->selected_vertex_count;
            return object->selected_vertices;
        case SANDBOX3D_AUTHORING_SELECTION_EDGE:
            *out_count = object->selected_edge_count;
            return object->selected_edges;
        case SANDBOX3D_AUTHORING_SELECTION_FACE:
        default:
            *out_count = object->selected_face_count;
            return object->selected_faces;
    }
}

static const uint32_t* sandbox3d_authoring_selected_ids_const(
    const sandbox3d_authoring_object* object,
    size_t* out_count)
{
    return sandbox3d_authoring_selected_ids((sandbox3d_authoring_object*)object, out_count);
}

static bool sandbox3d_authoring_component_selected(
    const sandbox3d_authoring_object* object,
    uint32_t id)
{
    if (object == NULL)
    {
        return false;
    }
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
    {
        return sandbox3d_authoring_selection_contains(
            object->selected_vertex_bits,
            object->selected_vertex_word_count,
            object->selected_vertex_max_id,
            id);
    }
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        return sandbox3d_authoring_selection_contains(
            object->selected_edge_bits,
            object->selected_edge_word_count,
            object->selected_edge_max_id,
            id);
    }
    return sandbox3d_authoring_selection_contains(
        object->selected_face_bits,
        object->selected_face_word_count,
        object->selected_face_max_id,
        id);
}

static void sandbox3d_authoring_remove_invalid_component_selection(
    sandbox3d_authoring_object* object)
{
    uint32_t* ids;
    uint64_t* bits;
    size_t word_count;
    size_t max_slot_id;
    size_t* count;
    size_t prior_count;
    const uint32_t prior_active_id = object != NULL
        ? object->active_component_id
        : HENKA_AUTHORING_INVALID_ID;
    const henka_authoring_face_id prior_selected_face = object != NULL
        ? object->selected_face
        : HENKA_AUTHORING_INVALID_ID;
    size_t index;
    size_t write_index = 0U;
    if (object == NULL) return;
    ids = sandbox3d_authoring_selected_ids(object, &index);
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
    {
        bits = object->selected_vertex_bits;
        word_count = object->selected_vertex_word_count;
        max_slot_id = object->selected_vertex_max_id;
        count = &object->selected_vertex_count;
    }
    else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        bits = object->selected_edge_bits;
        word_count = object->selected_edge_word_count;
        max_slot_id = object->selected_edge_max_id;
        count = &object->selected_edge_count;
    }
    else
    {
        bits = object->selected_face_bits;
        word_count = object->selected_face_word_count;
        max_slot_id = object->selected_face_max_id;
        count = &object->selected_face_count;
    }
    prior_count = *count;
    sandbox3d_authoring_selection_clear(bits, word_count, count);
    if (ids == NULL) return;
    for (index = 0U; index < prior_count; ++index)
    {
        const bool valid = object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX
            ? henka_authoring_mesh_get_vertex(object->mesh, ids[index]) != NULL
            : object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE
                ? henka_authoring_mesh_get_edge(object->mesh, ids[index]) != NULL
                : henka_authoring_mesh_get_face(object->mesh, ids[index]) != NULL;
        if (valid)
        {
            ids[write_index++] = ids[index];
        }
    }
    for (index = 0U; index < write_index; ++index)
    {
        const uint32_t id = ids[index];
        if ((size_t)id <= max_slot_id)
        {
            bits[(size_t)id / 64U] |= UINT64_C(1) << ((size_t)id % 64U);
        }
    }
    *count = write_index;
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        object->selected_face = prior_selected_face != HENKA_AUTHORING_INVALID_ID &&
                henka_authoring_mesh_get_face(object->mesh, prior_selected_face) != NULL
            ? prior_selected_face
            : write_index > 0U
                ? (henka_authoring_face_id)ids[0U]
                : HENKA_AUTHORING_INVALID_ID;
    }
    if (!sandbox3d_authoring_component_selected(object, prior_active_id))
    {
        object->active_component_id = write_index > 0U
            ? ids[0U]
            : HENKA_AUTHORING_INVALID_ID;
    }
    else
    {
        object->active_component_id = prior_active_id;
    }
}

static size_t sandbox3d_authoring_current_selection_max_id(
    const sandbox3d_authoring_object* object)
{
    if (object == NULL)
    {
        return 0U;
    }
    switch (object->selection_mode)
    {
        case SANDBOX3D_AUTHORING_SELECTION_VERTEX:
            return object->selected_vertex_max_id;
        case SANDBOX3D_AUTHORING_SELECTION_EDGE:
            return object->selected_edge_max_id;
        case SANDBOX3D_AUTHORING_SELECTION_FACE:
        default:
            return object->selected_face_max_id;
    }
}

static bool sandbox3d_authoring_current_component_active(
    const sandbox3d_authoring_object* object,
    uint32_t id)
{
    if (object == NULL || id == HENKA_AUTHORING_INVALID_ID)
    {
        return false;
    }
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
    {
        return henka_authoring_mesh_get_vertex(object->mesh, id) != NULL;
    }
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        return henka_authoring_mesh_get_edge(object->mesh, id) != NULL;
    }
    return henka_authoring_mesh_get_face(object->mesh, id) != NULL;
}

/* Replaces only the active component store after the complete replacement is
 * prepared.  Selection operations are not mesh history entries, but they
 * still need an atomic failure boundary so allocation failure cannot erase a
 * valid user selection. */
static henka_result sandbox3d_authoring_replace_current_selection(
    sandbox3d_authoring_object* object,
    const uint32_t* ids,
    size_t count,
    uint32_t active_hint)
{
    uint64_t* next_bits = NULL;
    uint32_t* next_ids = NULL;
    uint64_t** old_bits;
    uint32_t** old_ids;
    size_t* old_word_count;
    size_t* old_max_id;
    size_t* old_capacity;
    size_t* old_count;
    size_t max_id;
    size_t word_count;
    size_t bytes;
    size_t index;
    uint32_t next_active = HENKA_AUTHORING_INVALID_ID;
    henka_result result;

    if (object == NULL || (count > 0U && ids == NULL) ||
        !sandbox3d_authoring_selection_word_count(
            sandbox3d_authoring_current_selection_max_id(object), &word_count))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    max_id = sandbox3d_authoring_current_selection_max_id(object);
    if (count > max_id || !henka_checked_size_multiply(word_count, sizeof(*next_bits), &bytes))
    {
        return HENKA_ERROR_LIMIT;
    }
    next_bits = henka_calloc(word_count, sizeof(*next_bits));
    if (next_bits == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (count > 0U)
    {
        if (!henka_checked_size_multiply(count, sizeof(*next_ids), &bytes))
        {
            henka_free(next_bits);
            return HENKA_ERROR_LIMIT;
        }
        next_ids = henka_calloc(count, sizeof(*next_ids));
        if (next_ids == NULL)
        {
            henka_free(next_bits);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }
    for (index = 0U; index < count; ++index)
    {
        const uint32_t id = ids[index];
        if (id == HENKA_AUTHORING_INVALID_ID ||
            (index > 0U && ids[index - 1U] >= id) ||
            !sandbox3d_authoring_current_component_active(object, id))
        {
            henka_free(next_bits);
            henka_free(next_ids);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        next_ids[index] = id;
        next_bits[(size_t)id / 64U] |= UINT64_C(1) << ((size_t)id % 64U);
    }
    if (count > 0U)
    {
        next_active = sandbox3d_authoring_selection_contains(
            next_bits, word_count, max_id, active_hint)
            ? active_hint
            : next_ids[0];
    }

    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
    {
        old_bits = &object->selected_vertex_bits;
        old_ids = &object->selected_vertices;
        old_word_count = &object->selected_vertex_word_count;
        old_max_id = &object->selected_vertex_max_id;
        old_capacity = &object->selected_vertex_capacity;
        old_count = &object->selected_vertex_count;
    }
    else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        old_bits = &object->selected_edge_bits;
        old_ids = &object->selected_edges;
        old_word_count = &object->selected_edge_word_count;
        old_max_id = &object->selected_edge_max_id;
        old_capacity = &object->selected_edge_capacity;
        old_count = &object->selected_edge_count;
    }
    else
    {
        old_bits = &object->selected_face_bits;
        old_ids = &object->selected_faces;
        old_word_count = &object->selected_face_word_count;
        old_max_id = &object->selected_face_max_id;
        old_capacity = &object->selected_face_capacity;
        old_count = &object->selected_face_count;
    }
    (void)old_word_count;
    (void)old_max_id;
    sandbox3d_authoring_selection_destroy(old_bits, old_ids);
    *old_bits = next_bits;
    *old_ids = next_ids;
    *old_word_count = word_count;
    *old_max_id = max_id;
    *old_capacity = count;
    *old_count = count;
    object->active_component_id = next_active;
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        object->selected_face = (henka_authoring_face_id)next_active;
    }
    result = HENKA_SUCCESS;
    return result;
}

static bool sandbox3d_authoring_current_selection_contains(
    const sandbox3d_authoring_object* object,
    uint32_t id)
{
    return sandbox3d_authoring_component_selected(object, id);
}

static henka_result sandbox3d_authoring_build_selection_set(
    sandbox3d_authoring_object* object,
    bool invert,
    const uint32_t* explicit_ids,
    size_t explicit_count)
{
    uint32_t* ids = NULL;
    size_t max_id;
    size_t count = 0U;
    size_t id;
    size_t bytes;
    henka_result result;

    if (object == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    max_id = sandbox3d_authoring_current_selection_max_id(object);
    if (max_id == 0U || !henka_checked_size_multiply(max_id, sizeof(*ids), &bytes))
    {
        return HENKA_ERROR_LIMIT;
    }
    if (max_id > 0U)
    {
        ids = henka_calloc(max_id, sizeof(*ids));
        if (ids == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }
    if (explicit_ids != NULL)
    {
        if (explicit_count > max_id)
        {
            henka_free(ids);
            return HENKA_ERROR_LIMIT;
        }
        if (explicit_count > 0U)
        {
            memcpy(ids, explicit_ids, explicit_count * sizeof(*ids));
        }
        count = explicit_count;
    }
    else
    {
        for (id = 1U; id <= max_id; ++id)
        {
            const bool active = sandbox3d_authoring_current_component_active(
                object, (uint32_t)id);
            const bool selected = sandbox3d_authoring_current_selection_contains(
                object, (uint32_t)id);
            if (active && (invert ? !selected : true))
            {
                ids[count++] = (uint32_t)id;
            }
        }
    }
    result = sandbox3d_authoring_replace_current_selection(
        object, ids, count, object->active_component_id);
    henka_free(ids);
    return result;
}

henka_result sandbox3d_authoring_object_select_all_components(
    sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_build_selection_set(object, false, NULL, 0U);
}

henka_result sandbox3d_authoring_object_select_none_components(
    sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_replace_current_selection(
        object, NULL, 0U, HENKA_AUTHORING_INVALID_ID);
}

henka_result sandbox3d_authoring_object_invert_component_selection(
    sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_build_selection_set(object, true, NULL, 0U);
}

henka_result sandbox3d_authoring_object_shrink_component_selection(
    sandbox3d_authoring_object* object)
{
    const uint32_t* selected_ids;
    uint32_t* retained_ids = NULL;
    size_t selected_count = 0U;
    size_t retained_count = 0U;
    size_t index;
    size_t max_id;
    size_t bytes;
    henka_result result;

    if (object == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    selected_ids = sandbox3d_authoring_selected_ids_const(object, &selected_count);
    if (selected_ids == NULL || selected_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    max_id = sandbox3d_authoring_current_selection_max_id(object);
    if (!henka_checked_size_multiply(selected_count, sizeof(*retained_ids), &bytes))
    {
        return HENKA_ERROR_LIMIT;
    }
    retained_ids = henka_calloc(selected_count, sizeof(*retained_ids));
    if (retained_ids == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < selected_count; ++index)
    {
        const uint32_t id = selected_ids[index];
        bool keep = true;
        if ((size_t)id > max_id || !sandbox3d_authoring_current_component_active(object, id))
        {
            henka_free(retained_ids);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        {
            const size_t edge_count = henka_authoring_mesh_get_vertex_edge_count(object->mesh, id);
            size_t edge_index;
            for (edge_index = 0U; edge_index < edge_count; ++edge_index)
            {
                henka_authoring_edge_id edge_id;
                const henka_authoring_edge* edge;
                uint32_t neighbor;
                if (henka_authoring_mesh_get_vertex_edge_at(object->mesh, id, edge_index, &edge_id) != HENKA_SUCCESS ||
                    (edge = henka_authoring_mesh_get_edge(object->mesh, edge_id)) == NULL)
                {
                    henka_free(retained_ids);
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
                neighbor = edge->vertices[0] == id ? edge->vertices[1] : edge->vertices[0];
                if (!sandbox3d_authoring_current_selection_contains(object, neighbor))
                {
                    keep = false;
                    break;
                }
            }
        }
        else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(object->mesh, id);
            size_t endpoint_index;
            if (edge == NULL)
            {
                henka_free(retained_ids);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            for (endpoint_index = 0U; endpoint_index < 2U && keep; ++endpoint_index)
            {
                const size_t edge_count = henka_authoring_mesh_get_vertex_edge_count(
                    object->mesh, edge->vertices[endpoint_index]);
                size_t edge_index;
                for (edge_index = 0U; edge_index < edge_count; ++edge_index)
                {
                    henka_authoring_edge_id neighbor_id;
                    if (henka_authoring_mesh_get_vertex_edge_at(
                            object->mesh, edge->vertices[endpoint_index], edge_index, &neighbor_id) != HENKA_SUCCESS)
                    {
                        henka_free(retained_ids);
                        return HENKA_ERROR_INVALID_ARGUMENT;
                    }
                    if (neighbor_id != id && !sandbox3d_authoring_current_selection_contains(object, neighbor_id))
                    {
                        keep = false;
                        break;
                    }
                }
            }
        }
        else
        {
            const henka_authoring_face* face = henka_authoring_mesh_get_face(object->mesh, id);
            size_t edge_index;
            if (face == NULL)
            {
                henka_free(retained_ids);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            for (edge_index = 0U; edge_index < face->corner_count; ++edge_index)
            {
                const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                    object->mesh, face->edges[edge_index]);
                henka_authoring_face_id other_face = HENKA_AUTHORING_INVALID_ID;
                size_t face_index;
                if (edge == NULL || edge->face_count != 2U)
                {
                    keep = false;
                    break;
                }
                for (face_index = 0U; face_index < edge->face_count; ++face_index)
                {
                    if (edge->faces[face_index] != id)
                    {
                        other_face = edge->faces[face_index];
                        break;
                    }
                }
                if (!sandbox3d_authoring_current_selection_contains(object, other_face))
                {
                    keep = false;
                    break;
                }
            }
        }
        if (keep)
        {
            retained_ids[retained_count++] = id;
        }
    }
    result = sandbox3d_authoring_replace_current_selection(
        object, retained_ids, retained_count, object->active_component_id);
    henka_free(retained_ids);
    return result;
}

static void sandbox3d_authoring_reset_selection_history(sandbox3d_authoring_object* object)
{
    if (object == NULL || object->selection_history == NULL || object->selection_history_capacity == 0U)
    {
        return;
    }
    object->selection_history_count = 1U;
    object->selection_history_cursor = 0U;
    object->selection_history[0] = object->selected_face;
}

static void sandbox3d_authoring_commit_selection_history(
    sandbox3d_authoring_object* object,
    henka_authoring_face_id selected_before,
    henka_authoring_face_id selected_after)
{
    if (object == NULL || object->selection_history == NULL || object->selection_history_capacity == 0U)
    {
        return;
    }
    if (object->selection_history_capacity == 1U)
    {
        object->selection_history[0] = selected_after;
        object->selection_history_count = 1U;
        object->selection_history_cursor = 0U;
        return;
    }
    object->selection_history[object->selection_history_cursor] = selected_before;
    object->selection_history_count = object->selection_history_cursor + 1U;
    if (object->selection_history_count >= object->selection_history_capacity)
    {
        memmove(
            object->selection_history,
            object->selection_history + 1U,
            (object->selection_history_count - 1U) * sizeof(*object->selection_history));
        --object->selection_history_count;
        if (object->selection_history_cursor > 0U)
        {
            --object->selection_history_cursor;
        }
    }
    ++object->selection_history_cursor;
    object->selection_history[object->selection_history_cursor] = selected_after;
    object->selection_history_count = object->selection_history_cursor + 1U;
}

static void sandbox3d_authoring_restore_selection_history(sandbox3d_authoring_object* object)
{
    if (object == NULL || object->selection_history == NULL ||
        object->selection_history_cursor >= object->selection_history_count)
    {
        return;
    }
    object->selected_face = object->selection_history[object->selection_history_cursor];
    sandbox3d_authoring_repair_selection(object);
    object->selection_history[object->selection_history_cursor] = object->selected_face;
}

static henka_result sandbox3d_authoring_publish_candidate(
    sandbox3d_authoring_object* object,
    henka_authoring_mesh* candidate,
    bool checkpoint_history,
    henka_authoring_face_id selected_after)
{
    henka_mesh* candidate_render = NULL;
    henka_mesh* old_scene_mesh = NULL;
    henka_bounds candidate_bounds = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    henka_bounds old_bounds;
    henka_physics_collider_desc old_collider = {0};
    bool had_old_bounds;
    bool had_old_collider;
    const henka_authoring_face_id selected_before = object != NULL
        ? object->selected_face
        : HENKA_AUTHORING_INVALID_ID;
    henka_result result;

    if (object == NULL || candidate == NULL ||
        !henka_scene_is_entity_valid(object->scene, object->entity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_evaluate_render(object, candidate,
        &candidate_render, &candidate_bounds);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (henka_scene_get_entity_mesh(object->scene, object->entity, &old_scene_mesh) != HENKA_SUCCESS)
    {
        henka_mesh_destroy(candidate_render);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    had_old_bounds = henka_scene_get_entity_local_bounds(
        object->scene, object->entity, &old_bounds) == HENKA_SUCCESS;
    had_old_collider = sandbox3d_authoring_get_physics_collider(object, &old_collider) == HENKA_SUCCESS &&
        object->physics_world != NULL && object->physics_body != HENKA_INVALID_PHYSICS_BODY_ID;
    result = henka_scene_set_entity_mesh(object->scene, object->entity, candidate_render);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_local_bounds(object->scene, object->entity, candidate_bounds);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_set_physics_bounds(object, &candidate_bounds);
    }
    if (result != HENKA_SUCCESS)
    {
        (void)henka_scene_set_entity_mesh(object->scene, object->entity, old_scene_mesh);
        if (had_old_bounds) (void)henka_scene_set_entity_local_bounds(object->scene, object->entity, old_bounds);
        else (void)henka_scene_clear_entity_local_bounds(object->scene, object->entity);
        if (had_old_collider) sandbox3d_authoring_restore_physics_collider(object, &old_collider);
        henka_mesh_destroy(candidate_render);
        return result;
    }
    if (checkpoint_history)
    {
        result = henka_authoring_mesh_history_checkpoint(object->history, candidate);
        if (result != HENKA_SUCCESS)
        {
            (void)henka_scene_set_entity_mesh(object->scene, object->entity, old_scene_mesh);
            if (had_old_bounds) (void)henka_scene_set_entity_local_bounds(object->scene, object->entity, old_bounds);
            else (void)henka_scene_clear_entity_local_bounds(object->scene, object->entity);
            if (had_old_collider) sandbox3d_authoring_restore_physics_collider(object, &old_collider);
            henka_mesh_destroy(candidate_render);
            return result;
        }
    }
    henka_authoring_mesh_destroy(object->mesh);
    henka_mesh_destroy(object->render_mesh);
    object->mesh = candidate;
    object->render_mesh = candidate_render;
    object->selected_face = selected_after;
    sandbox3d_authoring_repair_selection(object);
    sandbox3d_authoring_remove_invalid_component_selection(object);
    if (checkpoint_history)
    {
        sandbox3d_authoring_commit_selection_history(
            object, selected_before, object->selected_face);
    }
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_authoring_replace_loaded_source(
    sandbox3d_authoring_object* object,
    henka_authoring_mesh* candidate)
{
    henka_authoring_mesh_history* candidate_history = NULL;
    henka_mesh* candidate_render = NULL;
    henka_mesh* old_scene_mesh = NULL;
    henka_bounds candidate_bounds = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    henka_bounds old_bounds;
    henka_physics_collider_desc old_collider = {0};
    bool had_old_bounds;
    bool had_old_collider;
    henka_result result;

    if (object == NULL || candidate == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_history_create(
        candidate, object->history_steps, &candidate_history);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_evaluate_render(
            object, candidate, &candidate_render, &candidate_bounds);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_get_entity_mesh(object->scene, object->entity, &old_scene_mesh);
    }
    had_old_bounds = henka_scene_get_entity_local_bounds(
        object->scene, object->entity, &old_bounds) == HENKA_SUCCESS;
    had_old_collider = sandbox3d_authoring_get_physics_collider(object, &old_collider) == HENKA_SUCCESS &&
        object->physics_world != NULL && object->physics_body != HENKA_INVALID_PHYSICS_BODY_ID;
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_mesh(object->scene, object->entity, candidate_render);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_local_bounds(object->scene, object->entity, candidate_bounds);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_set_physics_bounds(object, &candidate_bounds);
    }
    if (result == HENKA_SUCCESS)
    {
        /* Resize all component stores only after the external scene/runtime
         * transaction is ready; the helper itself commits all three stores
         * together or leaves them untouched. */
        result = sandbox3d_authoring_resize_selection_for_mesh(object, candidate);
    }
    if (result != HENKA_SUCCESS)
    {
        if (old_scene_mesh != NULL)
        {
            (void)henka_scene_set_entity_mesh(object->scene, object->entity, old_scene_mesh);
        }
        if (had_old_bounds)
        {
            (void)henka_scene_set_entity_local_bounds(object->scene, object->entity, old_bounds);
        }
        else
        {
            (void)henka_scene_clear_entity_local_bounds(object->scene, object->entity);
        }
        if (had_old_collider) sandbox3d_authoring_restore_physics_collider(object, &old_collider);
        henka_mesh_destroy(candidate_render);
        henka_authoring_mesh_history_destroy(candidate_history);
        return result;
    }
    henka_authoring_mesh_destroy(object->mesh);
    henka_authoring_mesh_history_destroy(object->history);
    henka_mesh_destroy(object->render_mesh);
    object->mesh = candidate;
    object->history = candidate_history;
    object->render_mesh = candidate_render;
    sandbox3d_authoring_repair_selection(object);
    sandbox3d_authoring_reset_selection_history(object);
    return HENKA_SUCCESS;
}

size_t sandbox3d_object_authoring_collect_user_entities(
    const henka_scene* scene,
    henka_entity* out_entities,
    size_t capacity)
{
    size_t count;
    size_t index;

    if (scene == NULL)
    {
        return 0U;
    }

    count = 0U;
    for (index = 0U; index < henka_scene_get_entity_count(scene); ++index)
    {
        henka_entity entity = henka_scene_get_entity_at_index(scene, index);
        if (entity == HENKA_INVALID_ENTITY || henka_scene_is_entity_helper(scene, entity))
        {
            continue;
        }

        if (out_entities != NULL && count < capacity)
        {
            out_entities[count] = entity;
        }
        ++count;
    }

    return count;
}

bool sandbox3d_object_authoring_can_edit_entity(
    const henka_scene* scene,
    henka_entity entity)
{
    return scene != NULL &&
        entity != HENKA_INVALID_ENTITY &&
        henka_scene_is_entity_valid(scene, entity) &&
        !henka_scene_is_entity_helper(scene, entity);
}

henka_result sandbox3d_object_authoring_duplicate_entity(
    henka_scene* scene,
    henka_entity source,
    const char* duplicate_name,
    henka_entity* out_duplicate)
{
    henka_entity duplicate;
    henka_transform transform;
    henka_mesh* mesh;
    henka_material material;
    const henka_material_asset* material_asset;
    henka_bounds bounds = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    henka_interaction_desc interaction;
    uint32_t flags;
    const char* tag;
    henka_result result;
    bool has_bounds;

    if (out_duplicate == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_duplicate = HENKA_INVALID_ENTITY;

    if (!sandbox3d_object_authoring_can_edit_entity(scene, source) ||
        duplicate_name == NULL || duplicate_name[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_scene_get_entity_transform(scene, source, &transform) != HENKA_SUCCESS ||
        henka_scene_get_entity_mesh(scene, source, &mesh) != HENKA_SUCCESS ||
        henka_scene_get_entity_material(scene, source, &material) != HENKA_SUCCESS ||
        henka_scene_get_entity_material_asset(scene, source, &material_asset) != HENKA_SUCCESS ||
        henka_scene_get_entity_interaction(scene, source, &interaction) != HENKA_SUCCESS ||
        henka_scene_get_entity_flags(scene, source, &flags) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    tag = henka_scene_get_entity_tag(scene, source);
    has_bounds = henka_scene_get_entity_local_bounds(scene, source, &bounds) == HENKA_SUCCESS;
    duplicate = henka_scene_create_entity_named(scene, duplicate_name);
    if (duplicate == HENKA_INVALID_ENTITY)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    result = henka_scene_set_entity_transform(scene, duplicate, transform);
    if (result == HENKA_SUCCESS && mesh != NULL)
    {
        result = henka_scene_set_entity_mesh(scene, duplicate, mesh);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_material(scene, duplicate, material);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_material_asset(scene, duplicate, material_asset);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_tag(scene, duplicate, tag);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_visible(
            scene,
            duplicate,
            henka_scene_is_entity_visible(scene, source));
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_flags(scene, duplicate, flags);
    }
    if (result == HENKA_SUCCESS)
    {
        result = has_bounds
            ? henka_scene_set_entity_local_bounds(scene, duplicate, bounds)
            : henka_scene_clear_entity_local_bounds(scene, duplicate);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_interaction(scene, duplicate, &interaction);
    }

    if (result != HENKA_SUCCESS)
    {
        henka_scene_destroy_entity(scene, duplicate);
        return result;
    }

    *out_duplicate = duplicate;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_authoring_object_create_from_owned_mesh(
    henka_engine* engine,
    henka_scene* scene,
    henka_entity entity,
    henka_authoring_mesh* mesh,
    size_t history_steps,
    sandbox3d_authoring_object** out_object)
{
    sandbox3d_authoring_object* object;
    henka_mesh* old_mesh = NULL;
    henka_bounds bounds = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    henka_result result;

    if (out_object == NULL || engine == NULL || scene == NULL || mesh == NULL ||
        entity == HENKA_INVALID_ENTITY || !henka_scene_is_entity_valid(scene, entity) ||
        history_steps == 0U || history_steps > SANDBOX3D_AUTHORING_MAX_HISTORY_STEPS)
    {
        henka_authoring_mesh_destroy(mesh);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_object = NULL;
    object = henka_calloc(1U, sizeof(*object));
    if (object == NULL)
    {
        henka_authoring_mesh_destroy(mesh);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    object->engine = engine;
    object->scene = scene;
    object->entity = entity;
    object->mesh = mesh;
    object->history_steps = history_steps;
    /* New authoring objects enter object-selection state. Component overlays
     * are opt-in after the user picks a vertex, edge, or face; seeding face 1
     * here hid the real logical-object silhouette immediately after import. */
    object->selected_face = HENKA_AUTHORING_INVALID_ID;
    object->merge_distance = 0.001f;
    object->selection_mode = SANDBOX3D_AUTHORING_SELECTION_FACE;
    object->active_component_id = HENKA_AUTHORING_INVALID_ID;
    object->selected_face_count = 0U;
    object->physics_body = HENKA_INVALID_PHYSICS_BODY_ID;
    result = sandbox3d_authoring_resize_selection_for_mesh(object, mesh);
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_authoring_object_destroy(object);
        return result;
    }
    object->selection_history_capacity = history_steps;
    object->selection_history = henka_calloc(
        object->selection_history_capacity, sizeof(*object->selection_history));
    if (object->selection_history == NULL)
    {
        sandbox3d_authoring_object_destroy(object);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    sandbox3d_authoring_reset_selection_history(object);
    result = henka_authoring_mesh_history_create(mesh, history_steps, &object->history);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_evaluate_render(object, mesh, &object->render_mesh, &bounds);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_get_entity_mesh(scene, entity, &old_mesh);
    }
    if (result == HENKA_SUCCESS)
    {
        object->previous_scene_mesh = old_mesh;
        object->had_previous_bounds = henka_scene_get_entity_local_bounds(
            scene, entity, &object->previous_bounds) == HENKA_SUCCESS;
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_mesh(scene, entity, object->render_mesh);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_local_bounds(scene, entity, bounds);
    }
    if (result != HENKA_SUCCESS)
    {
        if (object->render_mesh != NULL)
        {
            (void)henka_scene_set_entity_mesh(scene, entity, old_mesh);
        }
        if (object->had_previous_bounds)
        {
            (void)henka_scene_set_entity_local_bounds(scene, entity, object->previous_bounds);
        }
        else
        {
            (void)henka_scene_clear_entity_local_bounds(scene, entity);
        }
        sandbox3d_authoring_object_destroy(object);
        return result;
    }
    *out_object = object;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_object_create_box(
    henka_engine* engine,
    henka_scene* scene,
    henka_entity entity,
    float width,
    float height,
    float depth,
    const henka_authoring_mesh_desc* mesh_desc,
    size_t history_steps,
    sandbox3d_authoring_object** out_object)
{
    henka_authoring_mesh_desc default_desc;
    henka_authoring_mesh* mesh = NULL;
    henka_result result;

    if (out_object == NULL || engine == NULL || scene == NULL ||
        entity == HENKA_INVALID_ENTITY || !henka_scene_is_entity_valid(scene, entity) ||
        history_steps == 0U || history_steps > SANDBOX3D_AUTHORING_MAX_HISTORY_STEPS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_object = NULL;
    default_desc = henka_authoring_mesh_desc_default();
    result = henka_authoring_mesh_create_box(
        mesh_desc == NULL ? &default_desc : mesh_desc, width, height, depth, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    return sandbox3d_authoring_object_create_from_owned_mesh(
        engine, scene, entity, mesh, history_steps, out_object);
}

henka_result sandbox3d_authoring_object_create_from_mesh(
    henka_engine* engine,
    henka_scene* scene,
    henka_entity entity,
    const henka_authoring_mesh* source,
    size_t history_steps,
    sandbox3d_authoring_object** out_object)
{
    henka_authoring_mesh* mesh = NULL;
    henka_result result;

    if (out_object == NULL || source == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_object = NULL;
    result = henka_authoring_mesh_clone(source, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_authoring_object_create_from_owned_mesh(
        engine, scene, entity, mesh, history_steps, out_object);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_object_create_from_model_primitive(
    henka_engine* engine,
    henka_scene* scene,
    henka_entity entity,
    const henka_model_scene_primitive* primitive,
    size_t history_steps,
    sandbox3d_authoring_object** out_object)
{
    henka_authoring_mesh_desc desc;
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id* vertex_ids = NULL;
    size_t* weld_hash_heads = NULL;
    size_t* weld_hash_next = NULL;
    size_t face_count;
    size_t vertex_index;
    size_t index_offset;
    henka_result result;

    if (out_object != NULL) *out_object = NULL;
    if (primitive == NULL || primitive->vertices == NULL || primitive->indices == NULL ||
        primitive->vertex_count < 3U || primitive->index_count < 3U ||
        primitive->index_count % 3U != 0U ||
        primitive->vertex_count > HENKA_AUTHORING_MESH_HARD_MAX_VERTICES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    face_count = primitive->index_count / 3U;
    if (face_count > HENKA_AUTHORING_MESH_HARD_MAX_FACES ||
        face_count > HENKA_AUTHORING_MESH_HARD_MAX_EDGES / 3U)
    {
        return HENKA_ERROR_LIMIT;
    }
    desc = (henka_authoring_mesh_desc){
        primitive->vertex_count <=
                HENKA_AUTHORING_MESH_HARD_MAX_VERTICES - SANDBOX3D_AUTHORING_IMPORT_VERTEX_RESERVE
            ? primitive->vertex_count + SANDBOX3D_AUTHORING_IMPORT_VERTEX_RESERVE
            : HENKA_AUTHORING_MESH_HARD_MAX_VERTICES,
        face_count * 3U <=
                HENKA_AUTHORING_MESH_HARD_MAX_EDGES - SANDBOX3D_AUTHORING_IMPORT_EDGE_RESERVE
            ? face_count * 3U + SANDBOX3D_AUTHORING_IMPORT_EDGE_RESERVE
            : HENKA_AUTHORING_MESH_HARD_MAX_EDGES,
        face_count <= HENKA_AUTHORING_MESH_HARD_MAX_FACES - SANDBOX3D_AUTHORING_IMPORT_FACE_RESERVE
            ? face_count + SANDBOX3D_AUTHORING_IMPORT_FACE_RESERVE
            : HENKA_AUTHORING_MESH_HARD_MAX_FACES,
        HENKA_AUTHORING_MESH_DEFAULT_MAX_FACE_CORNERS};
    result = henka_authoring_mesh_create(&desc, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    vertex_ids = henka_calloc(primitive->vertex_count, sizeof(*vertex_ids));
    weld_hash_heads = henka_calloc(
        SANDBOX3D_AUTHORING_IMPORT_WELD_BUCKET_COUNT,
        sizeof(*weld_hash_heads));
    weld_hash_next = henka_calloc(primitive->vertex_count, sizeof(*weld_hash_next));
    if (vertex_ids == NULL || weld_hash_heads == NULL || weld_hash_next == NULL)
    {
        henka_free(vertex_ids);
        henka_free(weld_hash_heads);
        henka_free(weld_hash_next);
        henka_authoring_mesh_destroy(mesh);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (vertex_index = 0U;
         vertex_index < SANDBOX3D_AUTHORING_IMPORT_WELD_BUCKET_COUNT;
         ++vertex_index)
    {
        weld_hash_heads[vertex_index] = SIZE_MAX;
    }
    for (vertex_index = 0U; vertex_index < primitive->vertex_count; ++vertex_index)
    {
        const henka_model_vertex* source = &primitive->vertices[vertex_index];
        const size_t bucket = sandbox3d_authoring_import_weld_bucket(source->position);
        size_t candidate_index;
        size_t welded_index = SIZE_MAX;

        for (candidate_index = weld_hash_heads[bucket];
             candidate_index != SIZE_MAX;
             candidate_index = weld_hash_next[candidate_index])
        {
            if (sandbox3d_authoring_import_weld_positions_match(
                    source->position,
                    primitive->vertices[candidate_index].position))
            {
                welded_index = candidate_index;
                break;
            }
        }
        if (welded_index != SIZE_MAX)
        {
            vertex_ids[vertex_index] = vertex_ids[welded_index];
        }
        else
        {
            result = henka_authoring_mesh_add_vertex(
                mesh,
                source->position,
                source->uv,
                source->material_region,
                &vertex_ids[vertex_index]);
            if (result != HENKA_SUCCESS)
            {
                goto import_failure;
            }
            weld_hash_next[vertex_index] = weld_hash_heads[bucket];
            weld_hash_heads[bucket] = vertex_index;
        }
    }
    for (index_offset = 0U; index_offset < primitive->index_count; index_offset += 3U)
    {
        const uint32_t first = primitive->indices[index_offset];
        const uint32_t second = primitive->indices[index_offset + 1U];
        const uint32_t third = primitive->indices[index_offset + 2U];
        henka_authoring_vertex_id face_vertices[3];
        henka_authoring_face_id face_id;

        if (first >= primitive->vertex_count || second >= primitive->vertex_count ||
            third >= primitive->vertex_count || first == second || first == third || second == third)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto import_failure;
        }
        face_vertices[0] = vertex_ids[first];
        face_vertices[1] = vertex_ids[second];
        face_vertices[2] = vertex_ids[third];
        result = henka_authoring_mesh_add_face(
            mesh,
            face_vertices,
            3U,
            primitive->vertices[first].material_region,
            true,
            &face_id);
        if (result != HENKA_SUCCESS)
        {
            goto import_failure;
        }
        result = henka_authoring_mesh_set_face_corner_uv(
            mesh, face_id, 0U, primitive->vertices[first].uv);
        if (result == HENKA_SUCCESS)
        {
            result = henka_authoring_mesh_set_face_corner_uv(
                mesh, face_id, 1U, primitive->vertices[second].uv);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_authoring_mesh_set_face_corner_uv(
                mesh, face_id, 2U, primitive->vertices[third].uv);
        }
        if (result != HENKA_SUCCESS)
        {
            goto import_failure;
        }
    }
    henka_free(vertex_ids);
    henka_free(weld_hash_heads);
    henka_free(weld_hash_next);
    if (!henka_authoring_mesh_validate(mesh))
    {
        henka_authoring_mesh_destroy(mesh);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_authoring_object_create_from_owned_mesh(
        engine, scene, entity, mesh, history_steps, out_object);

import_failure:
    henka_free(vertex_ids);
    henka_free(weld_hash_heads);
    henka_free(weld_hash_next);
    henka_authoring_mesh_destroy(mesh);
    return result;
}

void sandbox3d_authoring_object_destroy(sandbox3d_authoring_object* object)
{
    if (object == NULL)
    {
        return;
    }
    if (object->scene != NULL &&
        henka_scene_is_entity_valid(object->scene, object->entity))
    {
        henka_mesh* scene_mesh = NULL;
        if (henka_scene_get_entity_mesh(object->scene, object->entity, &scene_mesh) == HENKA_SUCCESS &&
            scene_mesh == object->render_mesh)
        {
            if (object->previous_scene_mesh != NULL)
            {
                (void)henka_scene_set_entity_mesh(
                    object->scene, object->entity, object->previous_scene_mesh);
            }
            else
            {
                (void)henka_scene_clear_entity_mesh(object->scene, object->entity);
            }
            if (object->had_previous_bounds)
            {
                (void)henka_scene_set_entity_local_bounds(
                    object->scene, object->entity, object->previous_bounds);
            }
            else
            {
                (void)henka_scene_clear_entity_local_bounds(object->scene, object->entity);
            }
        }
    }
    henka_mesh_destroy(object->render_mesh);
    henka_authoring_mesh_history_destroy(object->history);
    henka_authoring_mesh_destroy(object->mesh);
    sandbox3d_authoring_selection_destroy(
        &object->selected_vertex_bits, &object->selected_vertices);
    sandbox3d_authoring_selection_destroy(
        &object->selected_edge_bits, &object->selected_edges);
    sandbox3d_authoring_selection_destroy(
        &object->selected_face_bits, &object->selected_faces);
    henka_free(object->selection_history);
    henka_free(object->source_path);
    henka_free(object);
}

henka_result sandbox3d_authoring_object_bind_physics(
    sandbox3d_authoring_object* object,
    henka_physics_world* world,
    henka_physics_body_id body)
{
    henka_physics_body_state body_state;
    henka_bounds bounds;

    if (object == NULL || world == NULL || body == HENKA_INVALID_PHYSICS_BODY_ID ||
        henka_scene_get_entity_local_bounds(object->scene, object->entity, &bounds) != HENKA_SUCCESS ||
        henka_physics_body_get_state(world, body, &body_state) != HENKA_SUCCESS ||
        body_state.collider.shape != HENKA_PHYSICS_SHAPE_BOX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    body_state.collider.offset = bounds.center;
    body_state.collider.data.box.half_extents = bounds.extents;
    if (henka_physics_body_set_collider(world, body, body_state.collider) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    object->physics_world = world;
    object->physics_body = body;
    return HENKA_SUCCESS;
}

void sandbox3d_authoring_object_unbind_physics(sandbox3d_authoring_object* object)
{
    if (object != NULL)
    {
        object->physics_world = NULL;
        object->physics_body = HENKA_INVALID_PHYSICS_BODY_ID;
    }
}

henka_entity sandbox3d_authoring_object_get_entity(const sandbox3d_authoring_object* object)
{
    return object == NULL ? HENKA_INVALID_ENTITY : object->entity;
}

const henka_authoring_mesh* sandbox3d_authoring_object_get_mesh(const sandbox3d_authoring_object* object)
{
    return object == NULL ? NULL : object->mesh;
}

henka_result sandbox3d_authoring_object_recover_quads(
    sandbox3d_authoring_object* object,
    float minimum_normal_dot,
    float minimum_diagonal_ratio,
    float uv_epsilon,
    size_t* out_recovered_pairs)
{
    henka_authoring_mesh* candidate = NULL;
    size_t recovered_pairs = 0U;
    henka_result result;

    if (out_recovered_pairs != NULL)
    {
        *out_recovered_pairs = 0U;
    }

    if (object == NULL || out_recovered_pairs == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_authoring_mesh_recover_quads(
        candidate,
        minimum_normal_dot,
        minimum_diagonal_ratio,
        uv_epsilon,
        &recovered_pairs);

    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(candidate);
        return result;
    }

    if (recovered_pairs == 0U)
    {
        henka_authoring_mesh_destroy(candidate);
        return HENKA_SUCCESS;
    }

    result = sandbox3d_authoring_publish_candidate(
        object,
        candidate,
        true,
        HENKA_AUTHORING_INVALID_ID);

    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(candidate);
        return result;
    }

    sandbox3d_authoring_object_clear_component_selection(object);

    *out_recovered_pairs = recovered_pairs;
    return HENKA_SUCCESS;
}
henka_result sandbox3d_authoring_object_get_face_ordered_corners(
    const sandbox3d_authoring_object* object,
    henka_authoring_face_id face_id,
    henka_authoring_vertex_id* out_vertices,
    size_t vertex_capacity,
    size_t* out_count)
{
    const henka_authoring_face* face;
    size_t corner;

    if (out_count != NULL)
    {
        *out_count = 0U;
    }
    if (object == NULL || object->mesh == NULL || out_vertices == NULL || out_count == NULL ||
        face_id == HENKA_AUTHORING_INVALID_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    face = henka_authoring_mesh_get_face(object->mesh, face_id);
    if (face == NULL || !face->active || face->vertices == NULL ||
        face->corner_count < 3U ||
        face->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS ||
        face->corner_count > vertex_capacity)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (corner = 0U; corner < face->corner_count; ++corner)
    {
        const henka_authoring_vertex_id vertex_id = face->vertices[corner];
        size_t previous;
        if (vertex_id == HENKA_AUTHORING_INVALID_ID ||
            henka_authoring_mesh_get_vertex(object->mesh, vertex_id) == NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (previous = 0U; previous < corner; ++previous)
        {
            if (out_vertices[previous] == vertex_id)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
        out_vertices[corner] = vertex_id;
    }
    *out_count = face->corner_count;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_object_get_render_material_region_range(
    const sandbox3d_authoring_object* object,
    uint32_t* out_min_region,
    uint32_t* out_max_region)
{
    if (object == NULL || object->render_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_mesh_get_material_region_range(
        object->render_mesh, out_min_region, out_max_region);
}

henka_authoring_face_id sandbox3d_authoring_object_get_selected_face(const sandbox3d_authoring_object* object)
{
    return object == NULL ? HENKA_AUTHORING_INVALID_ID : object->selected_face;
}

uint32_t sandbox3d_authoring_object_get_active_component_id(
    const sandbox3d_authoring_object* object)
{
    return object == NULL ? HENKA_AUTHORING_INVALID_ID : object->active_component_id;
}

void sandbox3d_authoring_object_set_selection_mode(
    sandbox3d_authoring_object* object,
    sandbox3d_authoring_selection_mode mode)
{
    if (object == NULL || mode < SANDBOX3D_AUTHORING_SELECTION_VERTEX ||
        mode > SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        return;
    }
    object->selection_mode = mode;
    {
        size_t count = 0U;
        const uint32_t* ids = sandbox3d_authoring_selected_ids_const(object, &count);
        object->active_component_id = count > 0U && ids != NULL
            ? ids[0U]
            : HENKA_AUTHORING_INVALID_ID;
    }
}

sandbox3d_authoring_selection_mode sandbox3d_authoring_object_get_selection_mode(
    const sandbox3d_authoring_object* object)
{
    return object == NULL ? SANDBOX3D_AUTHORING_SELECTION_FACE : object->selection_mode;
}

void sandbox3d_authoring_object_clear_component_selection(
    sandbox3d_authoring_object* object)
{
    if (object == NULL) return;
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        sandbox3d_authoring_selection_clear(
            object->selected_vertex_bits,
            object->selected_vertex_word_count,
            &object->selected_vertex_count);
    else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        sandbox3d_authoring_selection_clear(
            object->selected_edge_bits,
            object->selected_edge_word_count,
            &object->selected_edge_count);
    else
    {
        sandbox3d_authoring_selection_clear(
            object->selected_face_bits,
            object->selected_face_word_count,
            &object->selected_face_count);
        object->selected_face = HENKA_AUTHORING_INVALID_ID;
    }
    object->active_component_id = HENKA_AUTHORING_INVALID_ID;
}

size_t sandbox3d_authoring_object_get_selected_component_count(
    const sandbox3d_authoring_object* object)
{
    size_t count = 0U;
    (void)sandbox3d_authoring_selected_ids_const(object, &count);
    return count;
}

henka_result sandbox3d_authoring_object_get_selected_component_at(
    const sandbox3d_authoring_object* object,
    size_t ordinal,
    uint32_t* out_id)
{
    size_t count;
    const uint32_t* ids;
    if (out_id != NULL) *out_id = HENKA_AUTHORING_INVALID_ID;
    ids = sandbox3d_authoring_selected_ids_const(object, &count);
    if (ids == NULL || out_id == NULL || ordinal >= count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_id = ids[ordinal];
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_object_select_component(
    sandbox3d_authoring_object* object,
    uint32_t component_id,
    bool additive)
{
    uint64_t* bits;
    size_t word_count;
    size_t max_slot_id;
    uint32_t** ids;
    size_t* id_capacity;
    size_t* count;
    henka_result result;
    if (object == NULL || component_id == HENKA_AUTHORING_INVALID_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
    {
        if (henka_authoring_mesh_get_vertex(object->mesh, component_id) == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
        bits = object->selected_vertex_bits;
        word_count = object->selected_vertex_word_count;
        max_slot_id = object->selected_vertex_max_id;
        ids = &object->selected_vertices;
        id_capacity = &object->selected_vertex_capacity;
        count = &object->selected_vertex_count;
    }
    else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        if (henka_authoring_mesh_get_edge(object->mesh, component_id) == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
        bits = object->selected_edge_bits;
        word_count = object->selected_edge_word_count;
        max_slot_id = object->selected_edge_max_id;
        ids = &object->selected_edges;
        id_capacity = &object->selected_edge_capacity;
        count = &object->selected_edge_count;
    }
    else
    {
        if (henka_authoring_mesh_get_face(object->mesh, component_id) == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
        bits = object->selected_face_bits;
        word_count = object->selected_face_word_count;
        max_slot_id = object->selected_face_max_id;
        ids = &object->selected_faces;
        id_capacity = &object->selected_face_capacity;
        count = &object->selected_face_count;
    }
    if (!additive && !sandbox3d_authoring_selection_contains(
            bits, word_count, max_slot_id, component_id))
    {
        result = sandbox3d_authoring_selection_reserve_ids(
            ids, id_capacity, 1U, max_slot_id);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    if (!additive)
    {
        sandbox3d_authoring_selection_clear(bits, word_count, count);
    }
    result = sandbox3d_authoring_selection_add(
        bits,
        word_count,
        max_slot_id,
        ids,
        id_capacity,
        count,
        component_id);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        object->selected_face = (henka_authoring_face_id)component_id;
    }
    object->active_component_id = component_id;
    return HENKA_SUCCESS;
}

static henka_vec3 sandbox3d_authoring_transform_point(
    henka_transform transform,
    henka_vec3 point)
{
    point.x *= transform.scale.x;
    point.y *= transform.scale.y;
    point.z *= transform.scale.z;
    return henka_vec3_add(transform.position, henka_quat_rotate_vec3(transform.rotation, point));
}

static bool sandbox3d_authoring_ray_triangle(
    henka_ray ray,
    henka_vec3 first,
    henka_vec3 second,
    henka_vec3 third,
    float maximum_distance,
    float* out_distance)
{
    const henka_vec3 edge_one = henka_vec3_subtract(second, first);
    const henka_vec3 edge_two = henka_vec3_subtract(third, first);
    const henka_vec3 cross_direction = henka_vec3_cross(ray.direction, edge_two);
    const float determinant = henka_vec3_dot(edge_one, cross_direction);
    const float inverse_determinant = 1.0f / determinant;
    const henka_vec3 origin_offset = henka_vec3_subtract(ray.origin, first);
    const float barycentric_u = henka_vec3_dot(origin_offset, cross_direction) * inverse_determinant;
    const henka_vec3 cross_offset = henka_vec3_cross(origin_offset, edge_one);
    const float barycentric_v = henka_vec3_dot(ray.direction, cross_offset) * inverse_determinant;
    const float distance = henka_vec3_dot(edge_two, cross_offset) * inverse_determinant;

    if (!isfinite(determinant) || fabsf(determinant) <= 0.000001f ||
        !isfinite(barycentric_u) || !isfinite(barycentric_v) || !isfinite(distance) ||
        barycentric_u < 0.0f || barycentric_u > 1.0f || barycentric_v < 0.0f ||
        barycentric_u + barycentric_v > 1.0f || distance < 0.0f || distance > maximum_distance)
    {
        return false;
    }
    if (out_distance != NULL)
    {
        *out_distance = distance;
    }
    return true;
}

static bool sandbox3d_authoring_ray_point_distance(
    henka_ray ray,
    henka_vec3 point,
    float maximum_distance,
    float* out_distance)
{
    const henka_vec3 offset = henka_vec3_subtract(point, ray.origin);
    const float distance = henka_vec3_dot(offset, ray.direction);
    const henka_vec3 closest = henka_vec3_add(ray.origin, henka_vec3_scale(ray.direction, distance));
    const float error = henka_vec3_length(henka_vec3_subtract(point, closest));
    const float tolerance = 0.18f + distance * 0.018f;
    if (!isfinite(distance) || !isfinite(error) || distance < 0.0f || distance > maximum_distance || error > tolerance)
    {
        return false;
    }
    if (out_distance != NULL) *out_distance = distance;
    return true;
}

static bool sandbox3d_authoring_ray_segment_distance(
    henka_ray ray,
    henka_vec3 first,
    henka_vec3 second,
    float maximum_distance,
    float* out_distance)
{
    const henka_vec3 segment = henka_vec3_subtract(second, first);
    const henka_vec3 origin_to_first = henka_vec3_subtract(ray.origin, first);
    const float segment_length_squared = henka_vec3_dot(segment, segment);
    const float ray_segment_dot = henka_vec3_dot(ray.direction, segment);
    const float ray_first_dot = henka_vec3_dot(ray.direction, origin_to_first);
    const float segment_first_dot = henka_vec3_dot(segment, origin_to_first);
    float ray_distance;
    float segment_factor;
    henka_vec3 ray_point;
    henka_vec3 segment_point;
    float tolerance;
    if (!isfinite(segment_length_squared) || segment_length_squared <= 0.000001f)
    {
        return sandbox3d_authoring_ray_point_distance(ray, first, maximum_distance, out_distance);
    }
    {
        const float denominator = 1.0f * segment_length_squared - ray_segment_dot * ray_segment_dot;
        if (fabsf(denominator) <= 0.000001f)
        {
            segment_factor = fminf(1.0f, fmaxf(0.0f, segment_first_dot / segment_length_squared));
            ray_distance = ray_first_dot + ray_segment_dot * segment_factor;
        }
        else
        {
            ray_distance = (ray_segment_dot * segment_first_dot - segment_length_squared * ray_first_dot) / denominator;
            segment_factor = (ray_segment_dot * ray_distance + segment_first_dot) / segment_length_squared;
            segment_factor = fminf(1.0f, fmaxf(0.0f, segment_factor));
        }
    }
    if (ray_distance < 0.0f || ray_distance > maximum_distance)
    {
        return false;
    }
    ray_point = henka_vec3_add(ray.origin, henka_vec3_scale(ray.direction, ray_distance));
    segment_point = henka_vec3_add(first, henka_vec3_scale(segment, segment_factor));
    tolerance = 0.14f + ray_distance * 0.014f;
    if (henka_vec3_length(henka_vec3_subtract(ray_point, segment_point)) > tolerance)
    {
        return false;
    }
    if (out_distance != NULL) *out_distance = ray_distance;
    return true;
}

henka_result sandbox3d_authoring_object_pick_component(
    sandbox3d_authoring_object* object,
    henka_ray ray,
    float maximum_distance,
    bool additive)
{
    henka_transform transform;
    uint32_t nearest_id = HENKA_AUTHORING_INVALID_ID;
    float nearest_distance = maximum_distance;
    size_t id;
    if (object == NULL || !isfinite(maximum_distance) || maximum_distance <= 0.0f ||
        !sandbox3d_authoring_finite_vec3(ray.origin) || !sandbox3d_authoring_finite_vec3(ray.direction) ||
        henka_vec3_length(ray.direction) <= 0.000001f ||
        henka_scene_get_entity_transform(object->scene, object->entity, &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    ray.direction = henka_vec3_normalize(ray.direction);
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        uint32_t* prior_faces = NULL;
        size_t prior_face_count = 0U;
        size_t prior_face_bytes = 0U;
        henka_authoring_face_id prior_selected_face = object->selected_face;
        henka_result result;
        if (additive)
        {
            prior_face_count = object->selected_face_count;
            if (!henka_checked_size_multiply(
                    prior_face_count, sizeof(prior_faces[0]), &prior_face_bytes))
            {
                return HENKA_ERROR_LIMIT;
            }
            if (prior_face_count > 0U &&
                sandbox3d_authoring_allocate_id_scratch(
                    prior_face_count, &prior_faces) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_OUT_OF_MEMORY;
            }
            if (prior_face_count > 0U)
            {
                memcpy(prior_faces, object->selected_faces, prior_face_bytes);
            }
        }
        result = sandbox3d_authoring_object_pick_face(object, ray, maximum_distance);
        if (result == HENKA_SUCCESS && additive)
        {
            const henka_authoring_face_id picked_face = object->selected_face;
            size_t prior_index;
            sandbox3d_authoring_selection_clear(
                object->selected_face_bits,
                object->selected_face_word_count,
                &object->selected_face_count);
            for (prior_index = 0U; prior_index < prior_face_count; ++prior_index)
            {
                const uint32_t prior_id = prior_faces[prior_index];
                object->selected_faces[prior_index] = prior_id;
                object->selected_face_bits[(size_t)prior_id / 64U] |=
                    UINT64_C(1) << ((size_t)prior_id % 64U);
            }
            object->selected_face_count = prior_face_count;
            object->selected_face = prior_selected_face;
            object->selection_mode = SANDBOX3D_AUTHORING_SELECTION_FACE;
            result = sandbox3d_authoring_object_select_component(
                object, picked_face, true);
        }
        henka_free(prior_faces);
        if (result == HENKA_SUCCESS)
        {
            return result;
        }
        return result;
    }
    for (id = 1U; id <= HENKA_AUTHORING_MESH_HARD_MAX_VERTICES &&
        object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX; ++id)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(object->mesh, (henka_authoring_vertex_id)id);
        float distance;
        if (vertex != NULL && sandbox3d_authoring_ray_point_distance(
                ray, sandbox3d_authoring_transform_point(transform, vertex->position), nearest_distance, &distance))
        {
            nearest_distance = distance;
            nearest_id = (uint32_t)id;
        }
    }
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        for (id = 1U; id <= HENKA_AUTHORING_MESH_HARD_MAX_EDGES; ++id)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(object->mesh, (henka_authoring_edge_id)id);
            const henka_authoring_vertex* first;
            const henka_authoring_vertex* second;
            float distance;
            if (edge == NULL ||
                (first = henka_authoring_mesh_get_vertex(object->mesh, edge->vertices[0])) == NULL ||
                (second = henka_authoring_mesh_get_vertex(object->mesh, edge->vertices[1])) == NULL)
            {
                continue;
            }
            if (sandbox3d_authoring_ray_segment_distance(
                    ray,
                    sandbox3d_authoring_transform_point(transform, first->position),
                    sandbox3d_authoring_transform_point(transform, second->position),
                    nearest_distance,
                    &distance))
            {
                nearest_distance = distance;
                nearest_id = (uint32_t)id;
            }
        }
    }
    return nearest_id == HENKA_AUTHORING_INVALID_ID
        ? HENKA_ERROR_UNKNOWN
        : sandbox3d_authoring_object_select_component(object, nearest_id, additive);
}

henka_result sandbox3d_authoring_object_move_selected_components(
    sandbox3d_authoring_object* object,
    henka_vec3 offset)
{
    henka_authoring_mesh* candidate = NULL;
    uint32_t* vertex_ids = NULL;
    size_t vertex_count = 0U;
    size_t selected_count;
    const uint32_t* selected_ids;
    size_t index;
    henka_result result;
    if (object == NULL || !sandbox3d_authoring_finite_vec3(offset)) return HENKA_ERROR_INVALID_ARGUMENT;
    result = sandbox3d_authoring_allocate_id_scratch(
        object->selected_vertex_max_id, &vertex_ids);
    if (result != HENKA_SUCCESS) return result;
    selected_ids = sandbox3d_authoring_selected_ids_const(object, &selected_count);
    if (selected_ids == NULL || selected_count == 0U)
    {
        henka_free(vertex_ids);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < selected_count; ++index)
    {
        const henka_authoring_vertex_id* candidates = NULL;
        size_t candidate_count = 0U;
        if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        {
            candidates = &selected_ids[index]; candidate_count = 1U;
        }
        else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(object->mesh, selected_ids[index]);
            if (edge != NULL) { candidates = edge->vertices; candidate_count = 2U; }
        }
        else
        {
            const henka_authoring_face* face = henka_authoring_mesh_get_face(object->mesh, selected_ids[index]);
            if (face != NULL) { candidates = face->vertices; candidate_count = face->corner_count; }
        }
        for (size_t candidate_index = 0U; candidate_index < candidate_count; ++candidate_index)
        {
            size_t existing;
            bool duplicate = false;
            for (existing = 0U; existing < vertex_count; ++existing)
                if (vertex_ids[existing] == candidates[candidate_index]) duplicate = true;
            if (!duplicate && vertex_count < object->selected_vertex_max_id)
                vertex_ids[vertex_count++] = candidates[candidate_index];
        }
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        for (index = 0U; index < vertex_count && result == HENKA_SUCCESS; ++index)
        {
            const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(candidate, vertex_ids[index]);
            result = vertex == NULL ? HENKA_ERROR_INVALID_ARGUMENT :
                henka_authoring_mesh_set_vertex_position(candidate, vertex->id, henka_vec3_add(vertex->position, offset));
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(object, candidate, true, object->selected_face);
    }
    if (result != HENKA_SUCCESS) henka_authoring_mesh_destroy(candidate);
    henka_free(vertex_ids);
    return result;
}

static bool sandbox3d_authoring_append_unique_id(
    uint32_t* ids,
    size_t* count,
    size_t capacity,
    uint32_t id)
{
    size_t index;

    if (ids == NULL || count == NULL || id == HENKA_AUTHORING_INVALID_ID)
    {
        return false;
    }
    for (index = 0U; index < *count; ++index)
    {
        if (ids[index] == id)
        {
            return true;
        }
    }
    if (*count >= capacity)
    {
        return false;
    }
    ids[(*count)++] = id;
    return true;
}

static henka_result sandbox3d_authoring_allocate_id_scratch(
    size_t capacity,
    uint32_t** out_ids)
{
    size_t bytes;
    if (out_ids == NULL || capacity == 0U ||
        !henka_checked_size_multiply(capacity, sizeof(uint32_t), &bytes))
    {
        if (out_ids != NULL) *out_ids = NULL;
        return HENKA_ERROR_LIMIT;
    }
    *out_ids = henka_calloc(capacity, sizeof(uint32_t));
    return *out_ids == NULL ? HENKA_ERROR_OUT_OF_MEMORY : HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_object_move_selected_face_normal(
    sandbox3d_authoring_object* object,
    float distance)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_vertex_id ordered_vertices[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id vertex_ids[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    size_t corner_count = 0U;
    size_t vertex_count = 0U;
    size_t index;
    henka_vec3 normal;
    henka_result result;

    if (object == NULL || !isfinite(distance) || fabsf(distance) > 100.0f ||
        object->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE ||
        object->selected_face == HENKA_AUTHORING_INVALID_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_object_get_face_ordered_corners(
        object,
        object->selected_face,
        ordered_vertices,
        sizeof(ordered_vertices) / sizeof(ordered_vertices[0]),
        &corner_count);
    if (result != HENKA_SUCCESS || corner_count < 3U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    {
        const henka_authoring_vertex* first = henka_authoring_mesh_get_vertex(
            object->mesh, ordered_vertices[0]);
        const henka_authoring_vertex* second = henka_authoring_mesh_get_vertex(
            object->mesh, ordered_vertices[1]);
        const henka_authoring_vertex* third = henka_authoring_mesh_get_vertex(
            object->mesh, ordered_vertices[2]);
        henka_vec3 first_edge;
        henka_vec3 second_edge;
        if (first == NULL || second == NULL || third == NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        first_edge = henka_vec3_subtract(second->position, first->position);
        second_edge = henka_vec3_subtract(third->position, first->position);
        if (henka_vec3_length(henka_vec3_cross(first_edge, second_edge)) <= 0.000001f)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        normal = henka_vec3_normalize(henka_vec3_cross(first_edge, second_edge));
    }
    for (index = 0U; index < corner_count; ++index)
    {
        if (!sandbox3d_authoring_append_unique_id(
                vertex_ids,
                &vertex_count,
                sizeof(vertex_ids) / sizeof(vertex_ids[0]),
                ordered_vertices[index]))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        const henka_vec3 offset = henka_vec3_scale(normal, distance);
        for (index = 0U; index < vertex_count && result == HENKA_SUCCESS; ++index)
        {
            const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(
                candidate, vertex_ids[index]);
            result = vertex == NULL ? HENKA_ERROR_INVALID_ARGUMENT :
                henka_authoring_mesh_set_vertex_position(
                    candidate,
                    vertex->id,
                    henka_vec3_add(vertex->position, offset));
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(object, candidate, true, object->selected_face);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_grow_component_selection(
    sandbox3d_authoring_object* object)
{
    uint32_t* additions = NULL;
    size_t addition_capacity;
    size_t addition_count = 0U;
    size_t selected_count = 0U;
    const uint32_t* selected_ids;
    size_t selected_index;

    if (object == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    selected_ids = sandbox3d_authoring_selected_ids_const(object, &selected_count);
    if (selected_ids == NULL || selected_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    addition_capacity = object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX
        ? object->selected_vertex_max_id
        : object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE
            ? object->selected_edge_max_id
            : object->selected_face_max_id;
    if (sandbox3d_authoring_allocate_id_scratch(addition_capacity, &additions) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
    {
        for (selected_index = 0U; selected_index < selected_count; ++selected_index)
        {
            const size_t edge_count = henka_authoring_mesh_get_vertex_edge_count(
                object->mesh, (henka_authoring_vertex_id)selected_ids[selected_index]);
            size_t edge_index;
            for (edge_index = 0U; edge_index < edge_count; ++edge_index)
            {
                henka_authoring_edge_id edge_id;
                const henka_authoring_edge* edge;
                if (henka_authoring_mesh_get_vertex_edge_at(
                        object->mesh,
                        (henka_authoring_vertex_id)selected_ids[selected_index],
                        edge_index,
                        &edge_id) != HENKA_SUCCESS ||
                    (edge = henka_authoring_mesh_get_edge(object->mesh, edge_id)) == NULL)
                {
                    continue;
                }
                (void)sandbox3d_authoring_append_unique_id(
                    additions,
                    &addition_count,
                    addition_capacity,
                    edge->vertices[0] == selected_ids[selected_index]
                        ? edge->vertices[1]
                        : edge->vertices[0]);
            }
        }
    }
    else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
    {
        for (selected_index = 0U; selected_index < selected_count; ++selected_index)
        {
            const henka_authoring_edge* selected_edge = henka_authoring_mesh_get_edge(
                object->mesh, (henka_authoring_edge_id)selected_ids[selected_index]);
            size_t endpoint_index;
            if (selected_edge == NULL)
            {
                continue;
            }
            for (endpoint_index = 0U; endpoint_index < 2U; ++endpoint_index)
            {
                const size_t edge_count = henka_authoring_mesh_get_vertex_edge_count(
                    object->mesh, selected_edge->vertices[endpoint_index]);
                size_t edge_index;
                for (edge_index = 0U; edge_index < edge_count; ++edge_index)
                {
                    henka_authoring_edge_id edge_id;
                    if (henka_authoring_mesh_get_vertex_edge_at(
                            object->mesh,
                            selected_edge->vertices[endpoint_index],
                            edge_index,
                            &edge_id) == HENKA_SUCCESS)
                    {
                        (void)sandbox3d_authoring_append_unique_id(
                            additions,
                            &addition_count,
                            addition_capacity,
                            edge_id);
                    }
                }
            }
        }
    }
    else
    {
        for (selected_index = 0U; selected_index < selected_count; ++selected_index)
        {
            const henka_authoring_face* selected_face = henka_authoring_mesh_get_face(
                object->mesh, (henka_authoring_face_id)selected_ids[selected_index]);
            size_t corner;
            if (selected_face == NULL)
            {
                continue;
            }
            for (corner = 0U; corner < selected_face->corner_count; ++corner)
            {
                const henka_authoring_edge_id edge_id = selected_face->edges[corner];
                const size_t face_count = henka_authoring_mesh_get_edge_face_count(object->mesh, edge_id);
                size_t face_index;
                for (face_index = 0U; face_index < face_count; ++face_index)
                {
                    henka_authoring_face_id face_id;
                    if (henka_authoring_mesh_get_edge_face_at(
                            object->mesh, edge_id, face_index, &face_id) == HENKA_SUCCESS)
                    {
                        (void)sandbox3d_authoring_append_unique_id(
                            additions,
                            &addition_count,
                            addition_capacity,
                            face_id);
                    }
                }
            }
        }
    }

    {
        size_t write_index = 0U;
        for (selected_index = 0U; selected_index < addition_count; ++selected_index)
        {
            if (!sandbox3d_authoring_component_selected(object, additions[selected_index]))
            {
                additions[write_index++] = additions[selected_index];
            }
        }
        addition_count = write_index;
    }
    for (selected_index = 0U; selected_index < addition_count; ++selected_index)
    {
        henka_result result = sandbox3d_authoring_object_select_component(
            object, additions[selected_index], true);
        if (result != HENKA_SUCCESS)
        {
            henka_free(additions);
            return result;
        }
    }
    henka_free(additions);
    return HENKA_SUCCESS;
}

static int sandbox3d_authoring_compare_ids(const void* left, const void* right)
{
    const uint32_t first = *(const uint32_t*)left;
    const uint32_t second = *(const uint32_t*)right;
    return first < second ? -1 : first > second ? 1 : 0;
}

static henka_result sandbox3d_authoring_replace_edge_selection(
    sandbox3d_authoring_object* object,
    const uint32_t* edge_ids,
    size_t edge_count,
    henka_authoring_edge_id active_id)
{
    size_t edge_index;
    size_t bytes;
    bool active_present = false;
    henka_result result;

    if (object == NULL || object->mesh == NULL || edge_ids == NULL || edge_count == 0U ||
        active_id == HENKA_AUTHORING_INVALID_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (edge_count > object->selected_edge_max_id ||
        !henka_checked_size_multiply(edge_count, sizeof(*edge_ids), &bytes))
    {
        return HENKA_ERROR_LIMIT;
    }
    for (edge_index = 0U; edge_index < edge_count; ++edge_index)
    {
        const henka_authoring_edge_id edge_id = edge_ids[edge_index];
        size_t prior_index;
        if (edge_id == HENKA_AUTHORING_INVALID_ID ||
            (size_t)edge_id > object->selected_edge_max_id ||
            henka_authoring_mesh_get_edge(object->mesh, edge_id) == NULL ||
            (size_t)edge_id / 64U >= object->selected_edge_word_count)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (edge_id == active_id)
        {
            active_present = true;
        }
        for (prior_index = 0U; prior_index < edge_index; ++prior_index)
        {
            if (edge_ids[prior_index] == edge_id)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    if (!active_present)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_selection_reserve_ids(
        &object->selected_edges,
        &object->selected_edge_capacity,
        edge_count,
        object->selected_edge_max_id);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    /* Everything above is fallible. The replacement below is deliberately
     * non-fallible so a failed traversal never leaves a partial selection. */
    sandbox3d_authoring_selection_clear(
        object->selected_edge_bits,
        object->selected_edge_word_count,
        &object->selected_edge_count);
    memcpy(object->selected_edges, edge_ids, bytes);
    qsort(object->selected_edges, edge_count, sizeof(*object->selected_edges),
        sandbox3d_authoring_compare_ids);
    for (edge_index = 0U; edge_index < edge_count; ++edge_index)
    {
        const size_t id = (size_t)object->selected_edges[edge_index];
        object->selected_edge_bits[id / 64U] |= UINT64_C(1) << (id % 64U);
    }
    object->selected_edge_count = edge_count;
    object->active_component_id = active_id;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_authoring_collect_edge_loop_branch(
    const sandbox3d_authoring_object* object,
    henka_authoring_edge_id current_edge_id,
    henka_authoring_vertex_id current_vertex_id,
    uint32_t* loop_edges,
    size_t* edge_count,
    size_t edge_capacity)
{
    size_t step;
    if (object == NULL || loop_edges == NULL || edge_count == NULL ||
        edge_capacity == 0U || current_vertex_id == HENKA_AUTHORING_INVALID_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (step = 0U; step < edge_capacity; ++step)
    {
        const henka_authoring_edge* current_edge = henka_authoring_mesh_get_edge(
            object->mesh, current_edge_id);
        const size_t face_count = henka_authoring_mesh_get_edge_face_count(
            object->mesh, current_edge_id);
        henka_authoring_edge_id side_edges[2];
        size_t side_count = 0U;
        size_t face_index;
        size_t incident_count;
        size_t incident_index;
        henka_authoring_edge_id next_edge_id = HENKA_AUTHORING_INVALID_ID;
        size_t next_count = 0U;

        if (current_edge == NULL || face_count == 0U || face_count > 2U ||
            henka_authoring_mesh_get_vertex(object->mesh, current_vertex_id) == NULL)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        for (face_index = 0U; face_index < face_count; ++face_index)
        {
            henka_authoring_face_id face_id;
            const henka_authoring_face* face;
            size_t current_corner = 0U;
            size_t edge_occurrences = 0U;
            if (henka_authoring_mesh_get_edge_face_at(
                    object->mesh, current_edge_id, face_index, &face_id) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_UNKNOWN;
            }
            face = henka_authoring_mesh_get_face(object->mesh, face_id);
            if (face == NULL)
            {
                return HENKA_ERROR_UNKNOWN;
            }
            if (face->corner_count != 4U || face->vertices == NULL || face->edges == NULL)
            {
                return HENKA_SUCCESS;
            }
            for (current_corner = 0U;
                 current_corner < face->corner_count;
                 ++current_corner)
            {
                if (face->edges[current_corner] == current_edge_id)
                {
                    ++edge_occurrences;
                }
            }
            if (edge_occurrences != 1U)
            {
                return HENKA_ERROR_UNKNOWN;
            }
            for (current_corner = 0U;
                 current_corner < face->corner_count;
                 ++current_corner)
            {
                if (face->edges[current_corner] == current_edge_id)
                {
                    break;
                }
            }
            if (face->vertices[current_corner] == current_vertex_id)
            {
                side_edges[side_count++] = face->edges[
                    (current_corner + face->corner_count - 1U) % face->corner_count];
            }
            else if (face->vertices[(current_corner + 1U) % face->corner_count] == current_vertex_id)
            {
                side_edges[side_count++] = face->edges[(current_corner + 1U) % face->corner_count];
            }
            else
            {
                return HENKA_ERROR_UNKNOWN;
            }
        }
        incident_count = henka_authoring_mesh_get_vertex_edge_count(
            object->mesh, current_vertex_id);
        for (incident_index = 0U; incident_index < incident_count; ++incident_index)
        {
            henka_authoring_edge_id candidate_id;
            bool is_side_edge = false;
            size_t side_index;
            if (henka_authoring_mesh_get_vertex_edge_at(
                    object->mesh,
                    current_vertex_id,
                    incident_index,
                    &candidate_id) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_UNKNOWN;
            }
            if (candidate_id == current_edge_id)
            {
                continue;
            }
            for (side_index = 0U; side_index < side_count; ++side_index)
            {
                if (side_edges[side_index] == candidate_id)
                {
                    is_side_edge = true;
                    break;
                }
            }
            if (!is_side_edge)
            {
                next_edge_id = candidate_id;
                ++next_count;
            }
        }
        if (next_count == 0U || next_count > 1U)
        {
            return HENKA_SUCCESS;
        }
        if (henka_authoring_mesh_get_edge(object->mesh, next_edge_id) == NULL)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        for (incident_index = 0U; incident_index < *edge_count; ++incident_index)
        {
            if (loop_edges[incident_index] == next_edge_id)
            {
                return HENKA_SUCCESS;
            }
        }
        if (!sandbox3d_authoring_append_unique_id(
                loop_edges, edge_count, edge_capacity, next_edge_id))
        {
            return HENKA_ERROR_LIMIT;
        }
        {
            const henka_authoring_edge* next_edge = henka_authoring_mesh_get_edge(
                object->mesh, next_edge_id);
            if (next_edge->vertices[0] == current_vertex_id)
            {
                current_vertex_id = next_edge->vertices[1];
            }
            else if (next_edge->vertices[1] == current_vertex_id)
            {
                current_vertex_id = next_edge->vertices[0];
            }
            else
            {
                return HENKA_ERROR_UNKNOWN;
            }
        }
        current_edge_id = next_edge_id;
    }
    return HENKA_ERROR_LIMIT;
}

henka_result sandbox3d_authoring_object_select_edge_loop(
    sandbox3d_authoring_object* object)
{
    uint32_t* loop_edges = NULL;
    const size_t loop_capacity = object != NULL ? object->selected_edge_max_id : 0U;
    const henka_authoring_edge* active_edge;
    const henka_authoring_edge_id active_id = object != NULL
        ? object->active_component_id : HENKA_AUTHORING_INVALID_ID;
    size_t edge_count = 0U;
    henka_result result;

    if (object == NULL || object->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
        !henka_authoring_mesh_validate(object->mesh) || loop_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    active_edge = henka_authoring_mesh_get_edge(object->mesh, active_id);
    if (active_edge == NULL || active_edge->face_count == 0U || active_edge->face_count > 2U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_allocate_id_scratch(loop_capacity, &loop_edges);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (!sandbox3d_authoring_append_unique_id(
            loop_edges, &edge_count, loop_capacity, active_id))
    {
        henka_free(loop_edges);
        return HENKA_ERROR_LIMIT;
    }
    result = sandbox3d_authoring_collect_edge_loop_branch(
        object, active_id, active_edge->vertices[0], loop_edges, &edge_count, loop_capacity);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_collect_edge_loop_branch(
            object, active_id, active_edge->vertices[1], loop_edges, &edge_count, loop_capacity);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_replace_edge_selection(object, loop_edges, edge_count, active_id);
    }
    henka_free(loop_edges);
    return result;
}

static henka_result sandbox3d_authoring_collect_edge_ring_branch(
    const sandbox3d_authoring_object* object,
    henka_authoring_face_id current_face_id,
    henka_authoring_edge_id current_edge_id,
    uint32_t* ring_edges,
    size_t* edge_count,
    size_t edge_capacity)
{
    size_t step;
    if (object == NULL || ring_edges == NULL || edge_count == NULL || edge_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (step = 0U; step < edge_capacity; ++step)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            object->mesh, current_face_id);
        henka_authoring_edge_id opposite_edge_id = HENKA_AUTHORING_INVALID_ID;
        henka_authoring_face_id next_face_id = HENKA_AUTHORING_INVALID_ID;
        size_t current_corner = 0U;
        size_t edge_occurrences = 0U;
        size_t face_index;
        size_t opposite_face_count;

        if (face == NULL)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        if (face->corner_count != 4U || face->edges == NULL)
        {
            return HENKA_SUCCESS;
        }
        for (current_corner = 0U;
             current_corner < face->corner_count;
             ++current_corner)
        {
            if (face->edges[current_corner] == current_edge_id)
            {
                ++edge_occurrences;
            }
        }
        if (edge_occurrences != 1U)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        for (current_corner = 0U;
             current_corner < face->corner_count;
             ++current_corner)
        {
            if (face->edges[current_corner] == current_edge_id)
            {
                opposite_edge_id = face->edges[(current_corner + 2U) % 4U];
                break;
            }
        }
        if (opposite_edge_id == HENKA_AUTHORING_INVALID_ID ||
            henka_authoring_mesh_get_edge(object->mesh, opposite_edge_id) == NULL)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        for (face_index = 0U; face_index < *edge_count; ++face_index)
        {
            if (ring_edges[face_index] == opposite_edge_id)
            {
                return HENKA_SUCCESS;
            }
        }
        if (!sandbox3d_authoring_append_unique_id(
                ring_edges, edge_count, edge_capacity, opposite_edge_id))
        {
            return HENKA_ERROR_LIMIT;
        }
        opposite_face_count = henka_authoring_mesh_get_edge_face_count(
            object->mesh, opposite_edge_id);
        if (opposite_face_count > 2U || opposite_face_count == 0U)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        if (opposite_face_count == 1U)
        {
            return HENKA_SUCCESS;
        }
        for (face_index = 0U; face_index < opposite_face_count; ++face_index)
        {
            henka_authoring_face_id candidate_face_id;
            if (henka_authoring_mesh_get_edge_face_at(
                    object->mesh,
                    opposite_edge_id,
                    face_index,
                    &candidate_face_id) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_UNKNOWN;
            }
            if (candidate_face_id != current_face_id)
            {
                next_face_id = candidate_face_id;
                break;
            }
        }
        if (next_face_id == HENKA_AUTHORING_INVALID_ID)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        current_edge_id = opposite_edge_id;
        current_face_id = next_face_id;
    }
    return HENKA_ERROR_LIMIT;
}

henka_result sandbox3d_authoring_object_select_edge_ring(
    sandbox3d_authoring_object* object)
{
    uint32_t* ring_edges = NULL;
    const size_t ring_capacity = object != NULL ? object->selected_edge_max_id : 0U;
    const henka_authoring_edge* active_edge;
    const henka_authoring_edge_id active_id = object != NULL
        ? object->active_component_id : HENKA_AUTHORING_INVALID_ID;
    size_t adjacent_face_index;
    size_t adjacent_face_count;
    size_t edge_count = 0U;
    henka_result result;

    if (object == NULL || object->selection_mode != SANDBOX3D_AUTHORING_SELECTION_EDGE ||
        !henka_authoring_mesh_validate(object->mesh) || ring_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    active_edge = henka_authoring_mesh_get_edge(object->mesh, active_id);
    if (active_edge == NULL || active_edge->face_count == 0U || active_edge->face_count > 2U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_allocate_id_scratch(ring_capacity, &ring_edges);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (!sandbox3d_authoring_append_unique_id(
            ring_edges, &edge_count, ring_capacity, active_id))
    {
        henka_free(ring_edges);
        return HENKA_ERROR_LIMIT;
    }
    adjacent_face_count = active_edge->face_count;
    for (adjacent_face_index = 0U;
         adjacent_face_index < adjacent_face_count && result == HENKA_SUCCESS;
         ++adjacent_face_index)
    {
        henka_authoring_face_id face_id;
        if (henka_authoring_mesh_get_edge_face_at(
                object->mesh, active_id, adjacent_face_index, &face_id) != HENKA_SUCCESS)
        {
            result = HENKA_ERROR_UNKNOWN;
            break;
        }
        result = sandbox3d_authoring_collect_edge_ring_branch(
            object, face_id, active_id, ring_edges, &edge_count, ring_capacity);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_replace_edge_selection(object, ring_edges, edge_count, active_id);
    }
    henka_free(ring_edges);
    return result;
}

henka_result sandbox3d_authoring_object_proportional_move_selected_components(
    sandbox3d_authoring_object* object,
    henka_vec3 offset,
    size_t ring_count)
{
    henka_authoring_mesh* candidate = NULL;
    uint32_t* selected_vertex_ids = NULL;
    uint16_t* distances = NULL;
    uint32_t* queue = NULL;
    size_t vertex_capacity;
    size_t vertex_array_capacity;
    size_t selected_vertex_count = 0U;
    size_t selected_count = 0U;
    const uint32_t* selected_ids;
    size_t selected_index;
    size_t queue_head = 0U;
    size_t queue_tail = 0U;
    henka_result result;

    if (object == NULL || !sandbox3d_authoring_finite_vec3(offset) || ring_count > 8U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    selected_ids = sandbox3d_authoring_selected_ids_const(object, &selected_count);
    if (selected_ids == NULL || selected_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    vertex_capacity = object->selected_vertex_max_id;
    if (!henka_checked_size_add(vertex_capacity, 1U, &vertex_array_capacity) ||
        sandbox3d_authoring_allocate_id_scratch(vertex_capacity, &selected_vertex_ids) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_LIMIT;
    }
    for (selected_index = 0U; selected_index < selected_count; ++selected_index)
    {
        const henka_authoring_vertex_id* candidates = NULL;
        size_t candidate_count = 0U;
        if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        {
            candidates = &selected_ids[selected_index];
            candidate_count = 1U;
        }
        else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                object->mesh, (henka_authoring_edge_id)selected_ids[selected_index]);
            if (edge != NULL)
            {
                candidates = edge->vertices;
                candidate_count = 2U;
            }
        }
        else
        {
            const henka_authoring_face* face = henka_authoring_mesh_get_face(
                object->mesh, (henka_authoring_face_id)selected_ids[selected_index]);
            if (face != NULL)
            {
                candidates = face->vertices;
                candidate_count = face->corner_count;
            }
        }
        for (size_t candidate_index = 0U; candidate_index < candidate_count; ++candidate_index)
        {
            (void)sandbox3d_authoring_append_unique_id(
                selected_vertex_ids,
                &selected_vertex_count,
                vertex_capacity,
                candidates[candidate_index]);
        }
    }
    if (selected_vertex_count == 0U)
    {
        henka_free(selected_vertex_ids);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    distances = henka_calloc(vertex_array_capacity, sizeof(*distances));
    queue = henka_calloc(vertex_array_capacity, sizeof(*queue));
    if (distances == NULL || queue == NULL)
    {
        henka_free(distances);
        henka_free(queue);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (selected_index = 0U;
         selected_index <= vertex_capacity;
         ++selected_index)
    {
        distances[selected_index] = UINT16_MAX;
    }
    for (selected_index = 0U; selected_index < selected_vertex_count; ++selected_index)
    {
        const uint32_t vertex_id = selected_vertex_ids[selected_index];
        if (vertex_id == HENKA_AUTHORING_INVALID_ID ||
            (size_t)vertex_id > vertex_capacity ||
            henka_authoring_mesh_get_vertex(object->mesh, (henka_authoring_vertex_id)vertex_id) == NULL)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto proportional_move_cleanup;
        }
        if (distances[vertex_id] == UINT16_MAX)
        {
            distances[vertex_id] = 0U;
            if (queue_tail >= vertex_array_capacity)
            {
                result = HENKA_ERROR_LIMIT;
                goto proportional_move_cleanup;
            }
            queue[queue_tail++] = vertex_id;
        }
    }
    while (queue_head < queue_tail)
    {
        const uint32_t vertex_id = queue[queue_head++];
        const uint16_t distance = distances[vertex_id];
        const size_t edge_count = henka_authoring_mesh_get_vertex_edge_count(
            object->mesh, (henka_authoring_vertex_id)vertex_id);
        size_t edge_index;
        if ((size_t)distance >= ring_count)
        {
            continue;
        }
        for (edge_index = 0U; edge_index < edge_count; ++edge_index)
        {
            henka_authoring_edge_id edge_id;
            const henka_authoring_edge* edge;
            uint32_t neighbor_id;
            if (henka_authoring_mesh_get_vertex_edge_at(
                    object->mesh,
                    (henka_authoring_vertex_id)vertex_id,
                    edge_index,
                    &edge_id) != HENKA_SUCCESS ||
                (edge = henka_authoring_mesh_get_edge(object->mesh, edge_id)) == NULL)
            {
                continue;
            }
            neighbor_id = edge->vertices[0] == vertex_id ? edge->vertices[1] : edge->vertices[0];
            if (neighbor_id == HENKA_AUTHORING_INVALID_ID ||
                (size_t)neighbor_id > vertex_capacity ||
                distances[neighbor_id] != UINT16_MAX)
            {
                continue;
            }
            distances[neighbor_id] = (uint16_t)(distance + 1U);
            if (queue_tail >= vertex_array_capacity)
            {
                result = HENKA_ERROR_LIMIT;
                goto proportional_move_cleanup;
            }
            queue[queue_tail++] = neighbor_id;
        }
    }

    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        for (selected_index = 1U;
             selected_index <= vertex_capacity;
             ++selected_index)
        {
            const henka_authoring_vertex* vertex;
            const float falloff_denominator = (float)(ring_count + 1U);
            float falloff;
            henka_vec3 position;
            if (distances[selected_index] == UINT16_MAX ||
                (size_t)distances[selected_index] > ring_count)
            {
                continue;
            }
            vertex = henka_authoring_mesh_get_vertex(candidate, (henka_authoring_vertex_id)selected_index);
            if (vertex == NULL)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                break;
            }
            falloff = ((float)(ring_count + 1U) - (float)distances[selected_index]) /
                falloff_denominator;
            position = henka_vec3_add(vertex->position, henka_vec3_scale(offset, falloff));
            result = henka_authoring_mesh_set_vertex_position(
                candidate, vertex->id, position);
            if (result != HENKA_SUCCESS)
            {
                break;
            }
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(object, candidate, true, object->selected_face);
    }

proportional_move_cleanup:
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(candidate);
    }
    henka_free(distances);
    henka_free(queue);
    henka_free(selected_vertex_ids);
    return result;
}

henka_result sandbox3d_authoring_object_select_connected_components(
    sandbox3d_authoring_object* object)
{
    size_t selected_count;
    size_t iteration;
    size_t iteration_limit;

    if (object == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    selected_count = sandbox3d_authoring_object_get_selected_component_count(object);
    if (selected_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    iteration_limit = object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX
        ? object->selected_vertex_max_id
        : object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE
            ? object->selected_edge_max_id
            : object->selected_face_max_id;

    /* Each successful growth either reaches a fixed point or adds at least
     * one component. The mesh slot capacity bounds this loop even when a
     * malformed topology does not converge. */
    for (iteration = 0U; iteration < iteration_limit; ++iteration)
    {
        const henka_result result =
            sandbox3d_authoring_object_grow_component_selection(object);
        const size_t grown_count =
            sandbox3d_authoring_object_get_selected_component_count(object);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (grown_count == selected_count)
        {
            return HENKA_SUCCESS;
        }
        selected_count = grown_count;
    }

    return HENKA_ERROR_LIMIT;
}

static henka_result sandbox3d_authoring_collect_selected_vertices(
    const sandbox3d_authoring_object* object,
    uint32_t** out_vertex_ids,
    size_t* out_vertex_count)
{
    const uint32_t* selected_ids;
    uint32_t* vertex_ids = NULL;
    size_t selected_count = 0U;
    size_t vertex_count = 0U;
    size_t selected_index;
    henka_result result;

    if (out_vertex_ids != NULL) *out_vertex_ids = NULL;
    if (out_vertex_count != NULL) *out_vertex_count = 0U;
    if (object == NULL || out_vertex_ids == NULL || out_vertex_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    selected_ids = sandbox3d_authoring_selected_ids_const(object, &selected_count);
    if (selected_ids == NULL || selected_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_allocate_id_scratch(
        object->selected_vertex_max_id, &vertex_ids);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    for (selected_index = 0U; selected_index < selected_count; ++selected_index)
    {
        const uint32_t* candidates = NULL;
        size_t candidate_count = 0U;
        if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        {
            candidates = &selected_ids[selected_index];
            candidate_count = 1U;
        }
        else if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                object->mesh, selected_ids[selected_index]);
            if (edge == NULL)
            {
                henka_free(vertex_ids);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            candidates = edge->vertices;
            candidate_count = 2U;
        }
        else
        {
            const henka_authoring_face* face = henka_authoring_mesh_get_face(
                object->mesh, selected_ids[selected_index]);
            if (face == NULL)
            {
                henka_free(vertex_ids);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            candidates = face->vertices;
            candidate_count = face->corner_count;
        }
        for (size_t candidate_index = 0U; candidate_index < candidate_count; ++candidate_index)
        {
            if (!sandbox3d_authoring_append_unique_id(
                    vertex_ids,
                    &vertex_count,
                    object->selected_vertex_max_id,
                    candidates[candidate_index]))
            {
                henka_free(vertex_ids);
                return HENKA_ERROR_LIMIT;
            }
        }
    }
    if (vertex_count == 0U)
    {
        henka_free(vertex_ids);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_vertex_ids = vertex_ids;
    *out_vertex_count = vertex_count;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_authoring_get_face_centroid(
    const henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    henka_vec3* out_centroid)
{
    const henka_authoring_face* face;
    henka_vec3 centroid = {0.0f, 0.0f, 0.0f};
    size_t corner;
    if (mesh == NULL || out_centroid == NULL ||
        (face = henka_authoring_mesh_get_face(mesh, face_id)) == NULL ||
        face->corner_count < 3U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (corner = 0U; corner < face->corner_count; ++corner)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(
            mesh, face->vertices[corner]);
        if (vertex == NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        centroid = henka_vec3_add(centroid, vertex->position);
    }
    *out_centroid = henka_vec3_scale(centroid, 1.0f / (float)face->corner_count);
    return sandbox3d_authoring_finite_vec3(*out_centroid)
        ? HENKA_SUCCESS
        : HENKA_ERROR_INVALID_ARGUMENT;
}

static henka_result sandbox3d_authoring_collect_normal_faces(
    const sandbox3d_authoring_object* object,
    uint32_t** out_face_ids,
    size_t* out_face_count)
{
    uint32_t* selected_ids = NULL;
    size_t selected_count = 0U;
    size_t face_count = 0U;
    size_t selected_index;
    henka_result result;

    if (out_face_ids != NULL) *out_face_ids = NULL;
    if (out_face_count != NULL) *out_face_count = 0U;
    if (object == NULL || out_face_ids == NULL || out_face_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_allocate_id_scratch(
        object->selected_face_max_id, &selected_ids);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        const uint32_t* ids = sandbox3d_authoring_selected_ids_const(object, &selected_count);
        if (ids == NULL || selected_count == 0U)
        {
            henka_free(selected_ids);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (selected_index = 0U; selected_index < selected_count; ++selected_index)
        {
            if (!sandbox3d_authoring_append_unique_id(
                    selected_ids, &face_count, object->selected_face_max_id, ids[selected_index]))
            {
                henka_free(selected_ids);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    else
    {
        const uint32_t* ids = sandbox3d_authoring_selected_ids_const(object, &selected_count);
        if (ids == NULL || selected_count == 0U)
        {
            henka_free(selected_ids);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (selected_index = 0U; selected_index < selected_count; ++selected_index)
        {
            if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
            {
                const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                    object->mesh, ids[selected_index]);
                size_t face_index;
                if (edge == NULL)
                {
                    henka_free(selected_ids);
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
                for (face_index = 0U; face_index < edge->face_count; ++face_index)
                {
                    if (!sandbox3d_authoring_append_unique_id(
                            selected_ids, &face_count, object->selected_face_max_id, edge->faces[face_index]))
                    {
                        henka_free(selected_ids);
                        return HENKA_ERROR_INVALID_ARGUMENT;
                    }
                }
            }
            else
            {
                const size_t edge_count = henka_authoring_mesh_get_vertex_edge_count(
                    object->mesh, ids[selected_index]);
                size_t edge_index;
                for (edge_index = 0U; edge_index < edge_count; ++edge_index)
                {
                    henka_authoring_edge_id edge_id;
                    const henka_authoring_edge* edge;
                    size_t face_index;
                    if (henka_authoring_mesh_get_vertex_edge_at(
                            object->mesh, ids[selected_index], edge_index, &edge_id) != HENKA_SUCCESS ||
                        (edge = henka_authoring_mesh_get_edge(object->mesh, edge_id)) == NULL)
                    {
                        henka_free(selected_ids);
                        return HENKA_ERROR_INVALID_ARGUMENT;
                    }
                    for (face_index = 0U; face_index < edge->face_count; ++face_index)
                    {
                        if (!sandbox3d_authoring_append_unique_id(
                                selected_ids, &face_count, object->selected_face_max_id, edge->faces[face_index]))
                        {
                            henka_free(selected_ids);
                            return HENKA_ERROR_INVALID_ARGUMENT;
                        }
                    }
                }
            }
        }
    }
    if (face_count == 0U)
    {
        henka_free(selected_ids);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_face_ids = selected_ids;
    *out_face_count = face_count;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_authoring_get_component_pivot(
    const sandbox3d_authoring_object* object,
    const uint32_t* vertex_ids,
    size_t vertex_count,
    sandbox3d_authoring_pivot_mode pivot_mode,
    henka_vec3* out_pivot)
{
    henka_vec3 pivot = {0.0f, 0.0f, 0.0f};
    size_t index;
    if (object == NULL || vertex_ids == NULL || vertex_count == 0U || out_pivot == NULL ||
        pivot_mode < SANDBOX3D_AUTHORING_PIVOT_MEDIAN ||
        pivot_mode > SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL ||
        pivot_mode == SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (pivot_mode == SANDBOX3D_AUTHORING_PIVOT_ACTIVE)
    {
        const uint32_t active_id = object->active_component_id;
        if (!sandbox3d_authoring_current_selection_contains(object, active_id))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_VERTEX)
        {
            const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(object->mesh, active_id);
            if (vertex == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
            *out_pivot = vertex->position;
            return HENKA_SUCCESS;
        }
        if (object->selection_mode == SANDBOX3D_AUTHORING_SELECTION_EDGE)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(object->mesh, active_id);
            const henka_authoring_vertex* first;
            const henka_authoring_vertex* second;
            if (edge == NULL ||
                (first = henka_authoring_mesh_get_vertex(object->mesh, edge->vertices[0])) == NULL ||
                (second = henka_authoring_mesh_get_vertex(object->mesh, edge->vertices[1])) == NULL)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            *out_pivot = henka_vec3_scale(henka_vec3_add(first->position, second->position), 0.5f);
            return HENKA_SUCCESS;
        }
        return sandbox3d_authoring_get_face_centroid(
            object->mesh, (henka_authoring_face_id)active_id, out_pivot);
    }
    for (index = 0U; index < vertex_count; ++index)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(
            object->mesh, (henka_authoring_vertex_id)vertex_ids[index]);
        if (vertex == NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        pivot = henka_vec3_add(pivot, vertex->position);
    }
    *out_pivot = henka_vec3_scale(pivot, 1.0f / (float)vertex_count);
    return sandbox3d_authoring_finite_vec3(*out_pivot)
        ? HENKA_SUCCESS
        : HENKA_ERROR_INVALID_ARGUMENT;
}

static henka_result sandbox3d_authoring_get_orientation_axis(
    const sandbox3d_authoring_object* object,
    henka_vec3 requested_axis,
    sandbox3d_authoring_orientation_mode orientation_mode,
    henka_vec3* out_axis)
{
    henka_vec3 axis = requested_axis;
    if (object == NULL || out_axis == NULL ||
        !sandbox3d_authoring_finite_vec3(requested_axis) ||
        orientation_mode < SANDBOX3D_AUTHORING_ORIENTATION_WORLD ||
        orientation_mode > SANDBOX3D_AUTHORING_ORIENTATION_NORMAL ||
        henka_vec3_length(requested_axis) <= 0.000001f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (orientation_mode == SANDBOX3D_AUTHORING_ORIENTATION_WORLD)
    {
        henka_transform transform;
        henka_quat inverse_rotation;
        if (henka_scene_get_entity_transform(object->scene, object->entity, &transform) != HENKA_SUCCESS ||
            !sandbox3d_authoring_finite_vec3(transform.position) ||
            !sandbox3d_authoring_finite_vec3(transform.scale) ||
            !isfinite(transform.rotation.x) || !isfinite(transform.rotation.y) ||
            !isfinite(transform.rotation.z) || !isfinite(transform.rotation.w))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        inverse_rotation = (henka_quat){
            -transform.rotation.x, -transform.rotation.y,
            -transform.rotation.z, transform.rotation.w};
        axis = henka_quat_rotate_vec3(inverse_rotation, requested_axis);
    }
    else if (orientation_mode == SANDBOX3D_AUTHORING_ORIENTATION_NORMAL)
    {
        uint32_t* face_ids = NULL;
        size_t face_count = 0U;
        size_t face_index;
        henka_vec3 weighted_normal = {0.0f, 0.0f, 0.0f};
        if (sandbox3d_authoring_collect_normal_faces(
                object, &face_ids, &face_count) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (face_index = 0U; face_index < face_count; ++face_index)
        {
            const henka_authoring_face* face = henka_authoring_mesh_get_face(
                object->mesh, (henka_authoring_face_id)face_ids[face_index]);
            const henka_authoring_vertex* first;
            size_t corner;
            if (face == NULL || face->corner_count < 3U ||
                (first = henka_authoring_mesh_get_vertex(object->mesh, face->vertices[0])) == NULL)
            {
                henka_free(face_ids);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            for (corner = 1U; corner + 1U < face->corner_count; ++corner)
            {
                const henka_authoring_vertex* second = henka_authoring_mesh_get_vertex(
                    object->mesh, face->vertices[corner]);
                const henka_authoring_vertex* third = henka_authoring_mesh_get_vertex(
                    object->mesh, face->vertices[corner + 1U]);
                if (second == NULL || third == NULL)
                {
                    henka_free(face_ids);
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
                weighted_normal = henka_vec3_add(weighted_normal, henka_vec3_cross(
                    henka_vec3_subtract(second->position, first->position),
                    henka_vec3_subtract(third->position, first->position)));
            }
        }
        henka_free(face_ids);
        if (henka_vec3_length(weighted_normal) <= 0.000001f)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        axis = weighted_normal;
    }
    if (henka_vec3_length(axis) <= 0.000001f || !sandbox3d_authoring_finite_vec3(axis))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_axis = henka_vec3_normalize(axis);
    return sandbox3d_authoring_finite_vec3(*out_axis)
        ? HENKA_SUCCESS
        : HENKA_ERROR_INVALID_ARGUMENT;
}

static henka_vec3 sandbox3d_authoring_apply_component_transform(
    henka_vec3 position,
    henka_vec3 pivot,
    henka_vec3 scale,
    henka_quat rotation)
{
    henka_vec3 relative = henka_vec3_subtract(position, pivot);
    relative = (henka_vec3){
        relative.x * scale.x,
        relative.y * scale.y,
        relative.z * scale.z};
    relative = henka_quat_rotate_vec3(rotation, relative);
    return henka_vec3_add(pivot, relative);
}

static henka_result sandbox3d_authoring_transform_selected_components(
    sandbox3d_authoring_object* object,
    henka_vec3 scale,
    henka_vec3 axis,
    float radians,
    sandbox3d_authoring_pivot_mode pivot_mode,
    sandbox3d_authoring_orientation_mode orientation_mode)
{
    henka_authoring_mesh* candidate = NULL;
    uint32_t* vertex_ids = NULL;
    size_t vertex_count = 0U;
    const uint32_t* selected_ids;
    size_t selected_count = 0U;
    henka_vec3 local_axis;
    henka_quat rotation;
    henka_vec3 pivot = {0.0f, 0.0f, 0.0f};
    size_t index;
    henka_result result;

    if (object == NULL || !sandbox3d_authoring_finite_vec3(scale) ||
        scale.x <= 0.0f || scale.y <= 0.0f || scale.z <= 0.0f ||
        scale.x > 4.0f || scale.y > 4.0f || scale.z > 4.0f ||
        !isfinite(radians) || pivot_mode < SANDBOX3D_AUTHORING_PIVOT_MEDIAN ||
        pivot_mode > SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL ||
        (pivot_mode == SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL &&
         object->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    selected_ids = sandbox3d_authoring_selected_ids_const(object, &selected_count);
    if (selected_ids == NULL || selected_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_collect_selected_vertices(
        object, &vertex_ids, &vertex_count);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_authoring_get_orientation_axis(
        object, axis, orientation_mode, &local_axis);
    if (result != HENKA_SUCCESS)
    {
        henka_free(vertex_ids);
        return result;
    }
    rotation = henka_quat_from_axis_angle(local_axis, radians);
    if (pivot_mode != SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL)
    {
        result = sandbox3d_authoring_get_component_pivot(
            object, vertex_ids, vertex_count, pivot_mode, &pivot);
        if (result != HENKA_SUCCESS)
        {
            henka_free(vertex_ids);
            return result;
        }
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS && pivot_mode == SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL)
    {
        for (index = 0U; index < selected_count && result == HENKA_SUCCESS; ++index)
        {
            const henka_authoring_face* face = henka_authoring_mesh_get_face(
                object->mesh, (henka_authoring_face_id)selected_ids[index]);
            size_t corner;
            if (face == NULL || sandbox3d_authoring_get_face_centroid(
                    object->mesh, face->id, &pivot) != HENKA_SUCCESS)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                break;
            }
            for (corner = 0U; corner < face->corner_count && result == HENKA_SUCCESS; ++corner)
            {
                const henka_authoring_vertex_id vertex_id = face->vertices[corner];
                const henka_authoring_vertex* candidate_vertex = henka_authoring_mesh_get_vertex(
                    candidate, vertex_id);
                henka_vec3 face_axis = local_axis;
                henka_vec3 transformed;
                if (candidate_vertex == NULL)
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                    break;
                }
                if (orientation_mode == SANDBOX3D_AUTHORING_ORIENTATION_NORMAL)
                {
                    const henka_authoring_vertex* first = henka_authoring_mesh_get_vertex(
                        object->mesh, face->vertices[0]);
                    const henka_authoring_vertex* second = henka_authoring_mesh_get_vertex(
                        object->mesh, face->vertices[1]);
                    const henka_authoring_vertex* third = henka_authoring_mesh_get_vertex(
                        object->mesh, face->vertices[2]);
                    if (first == NULL || second == NULL || third == NULL)
                    {
                        result = HENKA_ERROR_INVALID_ARGUMENT;
                        break;
                    }
                    face_axis = henka_vec3_cross(
                        henka_vec3_subtract(second->position, first->position),
                        henka_vec3_subtract(third->position, first->position));
                    if (!sandbox3d_authoring_finite_vec3(face_axis) ||
                        henka_vec3_length(face_axis) <= 0.000001f)
                    {
                        result = HENKA_ERROR_INVALID_ARGUMENT;
                        break;
                    }
                    face_axis = henka_vec3_normalize(face_axis);
                }
                transformed = sandbox3d_authoring_apply_component_transform(
                    candidate_vertex->position,
                    pivot,
                    scale,
                    henka_quat_from_axis_angle(face_axis, radians));
                result = henka_authoring_mesh_set_vertex_position(
                    candidate, vertex_id, transformed);
            }
        }
    }
    else if (result == HENKA_SUCCESS)
    {
        for (index = 0U; index < vertex_count && result == HENKA_SUCCESS; ++index)
        {
            const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(
                candidate, (henka_authoring_vertex_id)vertex_ids[index]);
            if (vertex == NULL)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                break;
            }
            result = henka_authoring_mesh_set_vertex_position(
                candidate,
                vertex->id,
                sandbox3d_authoring_apply_component_transform(
                    vertex->position, pivot, scale, rotation));
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(object, candidate, true, object->selected_face);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(candidate);
    }
    henka_free(vertex_ids);
    return result;
}

henka_result sandbox3d_authoring_object_scale_selected_components_with_pivot(
    sandbox3d_authoring_object* object,
    henka_vec3 scale,
    sandbox3d_authoring_pivot_mode pivot_mode)
{
    return sandbox3d_authoring_transform_selected_components(
        object,
        scale,
        (henka_vec3){0.0f, 1.0f, 0.0f},
        0.0f,
        pivot_mode,
        SANDBOX3D_AUTHORING_ORIENTATION_LOCAL);
}

henka_result sandbox3d_authoring_object_scale_selected_components(
    sandbox3d_authoring_object* object,
    henka_vec3 scale)
{
    return sandbox3d_authoring_object_scale_selected_components_with_pivot(
        object, scale, SANDBOX3D_AUTHORING_PIVOT_MEDIAN);
}

henka_result sandbox3d_authoring_object_rotate_selected_components(
    sandbox3d_authoring_object* object,
    henka_vec3 axis,
    float radians,
    sandbox3d_authoring_pivot_mode pivot_mode,
    sandbox3d_authoring_orientation_mode orientation_mode)
{
    return sandbox3d_authoring_transform_selected_components(
        object,
        (henka_vec3){1.0f, 1.0f, 1.0f},
        axis,
        radians,
        pivot_mode,
        orientation_mode);
}

static bool sandbox3d_authoring_valid_region_transform(
    const sandbox3d_authoring_region_transform* transform)
{
    return transform != NULL &&
        sandbox3d_authoring_finite_vec3(transform->minimum) &&
        sandbox3d_authoring_finite_vec3(transform->maximum) &&
        sandbox3d_authoring_finite_vec3(transform->pivot) &&
        sandbox3d_authoring_finite_vec3(transform->scale) &&
        sandbox3d_authoring_finite_vec3(transform->offset) &&
        transform->minimum.x <= transform->maximum.x &&
        transform->minimum.y <= transform->maximum.y &&
        transform->minimum.z <= transform->maximum.z &&
        transform->scale.x > 0.0f && transform->scale.y > 0.0f && transform->scale.z > 0.0f &&
        transform->scale.x <= 4.0f && transform->scale.y <= 4.0f && transform->scale.z <= 4.0f;
}

henka_result sandbox3d_authoring_object_transform_vertex_regions(
    sandbox3d_authoring_object* object,
    const sandbox3d_authoring_region_transform* transforms,
    size_t transform_count,
    size_t* out_affected_vertices)
{
    henka_authoring_mesh* candidate = NULL;
    size_t affected_vertices = 0U;
    size_t transform_index;
    uint32_t vertex_id;
    henka_result result;

    if (out_affected_vertices != NULL)
    {
        *out_affected_vertices = 0U;
    }
    if (object == NULL || transforms == NULL || transform_count == 0U ||
        transform_count > SANDBOX3D_AUTHORING_MAX_REGION_TRANSFORMS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (transform_index = 0U; transform_index < transform_count; ++transform_index)
    {
        if (!sandbox3d_authoring_valid_region_transform(&transforms[transform_index]))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    for (vertex_id = 1U; vertex_id <= HENKA_AUTHORING_MESH_HARD_MAX_VERTICES; ++vertex_id)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(
            candidate, (henka_authoring_vertex_id)vertex_id);
        const henka_vec3 original_position = vertex != NULL ? vertex->position : (henka_vec3){0.0f, 0.0f, 0.0f};
        henka_vec3 position;
        bool matched = false;

        if (vertex == NULL)
        {
            continue;
        }
        position = original_position;
        for (transform_index = 0U; transform_index < transform_count; ++transform_index)
        {
            const sandbox3d_authoring_region_transform* transform = &transforms[transform_index];
            henka_vec3 relative;
            if (original_position.x < transform->minimum.x || original_position.x > transform->maximum.x ||
                original_position.y < transform->minimum.y || original_position.y > transform->maximum.y ||
                original_position.z < transform->minimum.z || original_position.z > transform->maximum.z)
            {
                continue;
            }
            relative = (henka_vec3){
                position.x - transform->pivot.x,
                position.y - transform->pivot.y,
                position.z - transform->pivot.z};
            position = (henka_vec3){
                transform->pivot.x + relative.x * transform->scale.x + transform->offset.x,
                transform->pivot.y + relative.y * transform->scale.y + transform->offset.y,
                transform->pivot.z + relative.z * transform->scale.z + transform->offset.z};
            matched = true;
        }
        if (!matched)
        {
            continue;
        }
        result = henka_authoring_mesh_set_vertex_position(candidate, vertex->id, position);
        if (result != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(candidate);
            return result;
        }
        ++affected_vertices;
    }
    if (affected_vertices == 0U)
    {
        henka_authoring_mesh_destroy(candidate);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_authoring_publish_candidate(object, candidate, true, object->selected_face);
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(candidate);
        return result;
    }
    if (out_affected_vertices != NULL)
    {
        *out_affected_vertices = affected_vertices;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_object_transform_vertex_region(
    sandbox3d_authoring_object* object,
    const sandbox3d_authoring_region_transform* transform,
    size_t* out_affected_vertices)
{
    return sandbox3d_authoring_object_transform_vertex_regions(
        object, transform, 1U, out_affected_vertices);
}

static henka_result sandbox3d_authoring_merge_selected_vertices(
    sandbox3d_authoring_object* object,
    bool by_distance,
    henka_authoring_vertex_merge_mode mode)
{
    henka_authoring_mesh* candidate = NULL;
    const uint32_t* selected_ids;
    henka_authoring_vertex_id* survivors = NULL;
    henka_authoring_modeling_report report = {0};
    size_t selected_count = 0U;
    size_t survivor_count = 0U;
    uint32_t prior_active;
    henka_result result;

    if (object == NULL || object->selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    selected_ids = sandbox3d_authoring_selected_ids_const(object, &selected_count);
    if (selected_ids == NULL || selected_count < 2U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    survivors = henka_malloc(selected_count * sizeof(*survivors));
    if (survivors == NULL) return HENKA_ERROR_OUT_OF_MEMORY;
    prior_active = object->active_component_id;
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = by_distance
            ? henka_authoring_mesh_merge_vertices_by_distance(
                candidate,
                (const henka_authoring_vertex_id*)selected_ids,
                selected_count,
                object->merge_distance,
                survivors,
                selected_count,
                &survivor_count,
                &report)
            : henka_authoring_mesh_merge_vertices(
                candidate,
                (const henka_authoring_vertex_id*)selected_ids,
                selected_count,
                mode,
                (henka_authoring_vertex_id)prior_active,
                survivors,
                selected_count,
                &survivor_count,
                &report);
    }
    if (result == HENKA_SUCCESS && !report.changed)
    {
        henka_authoring_mesh_destroy(candidate);
        henka_free(survivors);
        return HENKA_SUCCESS;
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, HENKA_AUTHORING_INVALID_ID);
        if (result == HENKA_SUCCESS)
        {
            uint32_t active_hint = survivor_count > 0U ? survivors[0] : HENKA_AUTHORING_INVALID_ID;
            if (henka_authoring_mesh_get_vertex(object->mesh, (henka_authoring_vertex_id)prior_active) != NULL)
            {
                active_hint = prior_active;
            }
            result = sandbox3d_authoring_replace_current_selection(
                object, survivors, survivor_count, active_hint);
        }
        else
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    henka_free(survivors);
    return result;
}

henka_result sandbox3d_authoring_object_merge_selected_vertices_center(
    sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_merge_selected_vertices(
        object, false, HENKA_AUTHORING_VERTEX_MERGE_CENTER);
}

henka_result sandbox3d_authoring_object_merge_selected_vertices_active(
    sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_merge_selected_vertices(
        object, false, HENKA_AUTHORING_VERTEX_MERGE_ACTIVE);
}

henka_result sandbox3d_authoring_object_merge_selected_vertices_by_distance(
    sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_merge_selected_vertices(
        object, true, HENKA_AUTHORING_VERTEX_MERGE_CENTER);
}

typedef enum sandbox3d_selected_vertex_topology_operation
{
    SANDBOX3D_SELECTED_VERTEX_DISSOLVE = 0,
    SANDBOX3D_SELECTED_VERTEX_DELETE,
    SANDBOX3D_SELECTED_VERTEX_CONNECT
} sandbox3d_selected_vertex_topology_operation;

static henka_result sandbox3d_authoring_apply_selected_vertex_topology(
    sandbox3d_authoring_object* object,
    sandbox3d_selected_vertex_topology_operation operation)
{
    const uint32_t* selected_ids;
    henka_authoring_vertex_id* vertex_ids = NULL;
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_modeling_report report = {0};
    size_t selected_count = 0U;
    size_t index;
    henka_result result;

    if (object == NULL || object->selection_mode != SANDBOX3D_AUTHORING_SELECTION_VERTEX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    selected_ids = sandbox3d_authoring_selected_ids_const(object, &selected_count);
    if (selected_ids == NULL || selected_count == 0U ||
        (operation == SANDBOX3D_SELECTED_VERTEX_CONNECT && selected_count != 2U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    vertex_ids = henka_malloc(selected_count * sizeof(*vertex_ids));
    if (vertex_ids == NULL) return HENKA_ERROR_OUT_OF_MEMORY;
    for (index = 0U; index < selected_count; ++index)
    {
        vertex_ids[index] = (henka_authoring_vertex_id)selected_ids[index];
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        if (operation == SANDBOX3D_SELECTED_VERTEX_DISSOLVE)
        {
            result = henka_authoring_mesh_dissolve_vertices(
                candidate, vertex_ids, selected_count, &report);
        }
        else if (operation == SANDBOX3D_SELECTED_VERTEX_DELETE)
        {
            result = henka_authoring_mesh_delete_vertices(
                candidate, vertex_ids, selected_count, &report);
        }
        else
        {
            henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
            result = henka_authoring_mesh_connect_vertices(
                candidate, vertex_ids[0], vertex_ids[1], &new_face_id, &report);
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, HENKA_AUTHORING_INVALID_ID);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(candidate);
    }
    henka_free(vertex_ids);
    return result;
}

henka_result sandbox3d_authoring_object_dissolve_selected_vertices(
    sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_apply_selected_vertex_topology(
        object, SANDBOX3D_SELECTED_VERTEX_DISSOLVE);
}

henka_result sandbox3d_authoring_object_delete_selected_vertices(
    sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_apply_selected_vertex_topology(
        object, SANDBOX3D_SELECTED_VERTEX_DELETE);
}

henka_result sandbox3d_authoring_object_connect_selected_vertices(
    sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_apply_selected_vertex_topology(
        object, SANDBOX3D_SELECTED_VERTEX_CONNECT);
}

float sandbox3d_authoring_object_get_merge_distance(
    const sandbox3d_authoring_object* object)
{
    return object != NULL ? object->merge_distance : 0.001f;
}

henka_result sandbox3d_authoring_object_set_merge_distance(
    sandbox3d_authoring_object* object,
    float distance)
{
    if (object == NULL || !isfinite(distance) || distance <= 0.0f || distance > 1000000.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    object->merge_distance = distance;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_object_pick_face(
    sandbox3d_authoring_object* object,
    henka_ray ray,
    float maximum_distance)
{
    henka_transform transform;
    henka_authoring_face_id nearest_face = HENKA_AUTHORING_INVALID_ID;
    float nearest_distance = maximum_distance;
    size_t face_id;

    if (object == NULL || !isfinite(maximum_distance) || maximum_distance <= 0.0f ||
        !sandbox3d_authoring_finite_vec3(ray.origin) || !sandbox3d_authoring_finite_vec3(ray.direction) ||
        henka_vec3_length(ray.direction) <= 0.000001f ||
        henka_scene_get_entity_transform(object->scene, object->entity, &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    ray.direction = henka_vec3_normalize(ray.direction);
    for (face_id = 1U; face_id <= HENKA_AUTHORING_MESH_HARD_MAX_FACES; ++face_id)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            object->mesh, (henka_authoring_face_id)face_id);
        size_t corner;
        if (face == NULL)
        {
            continue;
        }
        for (corner = 1U; corner + 1U < face->corner_count; ++corner)
        {
            const henka_authoring_vertex* first = henka_authoring_mesh_get_vertex(
                object->mesh, face->vertices[0]);
            const henka_authoring_vertex* second = henka_authoring_mesh_get_vertex(
                object->mesh, face->vertices[corner]);
            const henka_authoring_vertex* third = henka_authoring_mesh_get_vertex(
                object->mesh, face->vertices[corner + 1U]);
            float distance;
            if (first != NULL && second != NULL && third != NULL)
            {
                const henka_vec3 world_first = sandbox3d_authoring_transform_point(transform, first->position);
                const henka_vec3 world_second = sandbox3d_authoring_transform_point(transform, second->position);
                const henka_vec3 world_third = sandbox3d_authoring_transform_point(transform, third->position);
                if (sandbox3d_authoring_ray_triangle(
                        ray,
                        world_first,
                        world_second,
                        world_third,
                        nearest_distance,
                        &distance))
                {
                    nearest_distance = distance;
                    nearest_face = face->id;
                }
            }
        }
    }
    if (nearest_face == HENKA_AUTHORING_INVALID_ID)
    {
        /* A viewport click can land just outside a triangulated polygon's
         * diagonal or edge because the rendered surface and the input ray
         * are sampled at different resolutions.  Preserve exact triangle
         * hits as the primary path, then accept the closest triangle edge
         * using the same bounded tolerance as vertex/edge authoring picks.
         * This keeps misses outside the visible surface rejected while
         * making face selection behave like a real editor. */
        float nearest_edge_distance = maximum_distance;
        for (face_id = 1U; face_id <= HENKA_AUTHORING_MESH_HARD_MAX_FACES; ++face_id)
        {
            const henka_authoring_face* face = henka_authoring_mesh_get_face(
                object->mesh, (henka_authoring_face_id)face_id);
            size_t corner;
            if (face == NULL)
            {
                continue;
            }
            for (corner = 1U; corner + 1U < face->corner_count; ++corner)
            {
                const henka_authoring_vertex* first = henka_authoring_mesh_get_vertex(
                    object->mesh, face->vertices[0]);
                const henka_authoring_vertex* second = henka_authoring_mesh_get_vertex(
                    object->mesh, face->vertices[corner]);
                const henka_authoring_vertex* third = henka_authoring_mesh_get_vertex(
                    object->mesh, face->vertices[corner + 1U]);
                const henka_vec3 points[3] = {
                    first != NULL ? sandbox3d_authoring_transform_point(transform, first->position) : (henka_vec3){0.0f, 0.0f, 0.0f},
                    second != NULL ? sandbox3d_authoring_transform_point(transform, second->position) : (henka_vec3){0.0f, 0.0f, 0.0f},
                    third != NULL ? sandbox3d_authoring_transform_point(transform, third->position) : (henka_vec3){0.0f, 0.0f, 0.0f}};
                size_t edge;
                if (first == NULL || second == NULL || third == NULL)
                {
                    continue;
                }
                for (edge = 0U; edge < 3U; ++edge)
                {
                    float edge_distance;
                    if (sandbox3d_authoring_ray_segment_distance(
                            ray,
                            points[edge],
                            points[(edge + 1U) % 3U],
                            nearest_edge_distance,
                            &edge_distance))
                    {
                        nearest_edge_distance = edge_distance;
                        nearest_face = face->id;
                    }
                }
            }
        }
        if (nearest_face == HENKA_AUTHORING_INVALID_ID)
        {
            return HENKA_ERROR_UNKNOWN;
        }
    }
    object->selected_face = nearest_face;
    object->selection_mode = SANDBOX3D_AUTHORING_SELECTION_FACE;
    (void)sandbox3d_authoring_object_select_component(object, nearest_face, false);
    return HENKA_SUCCESS;
}

henka_result sandbox3d_authoring_object_select_face(
    sandbox3d_authoring_object* object,
    henka_authoring_face_id face_id)
{
    if (object == NULL || henka_authoring_mesh_get_face(object->mesh, face_id) == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    object->selection_mode = SANDBOX3D_AUTHORING_SELECTION_FACE;
    return sandbox3d_authoring_object_select_component(object, face_id, false);
}

henka_result sandbox3d_authoring_object_delete_selected_faces(
    sandbox3d_authoring_object* object)
{
    henka_authoring_mesh* candidate = NULL;
    const uint32_t* selected_ids;
    size_t selected_count = 0U;
    size_t index;
    henka_authoring_mesh_counts counts;
    henka_result result;

    if (object == NULL || object->selection_mode != SANDBOX3D_AUTHORING_SELECTION_FACE)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    selected_ids = sandbox3d_authoring_selected_ids_const(object, &selected_count);
    counts = henka_authoring_mesh_get_counts(object->mesh);
    if (selected_ids == NULL || selected_count == 0U || selected_count >= counts.faces)
    {
        /* Keep at least one renderable face so bounds, picking, and the
         * evaluated scene mesh remain well-defined. */
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < selected_count; ++index)
    {
        if (henka_authoring_mesh_get_face(
                object->mesh, (henka_authoring_face_id)selected_ids[index]) == NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        for (index = 0U; index < selected_count && result == HENKA_SUCCESS; ++index)
        {
            result = henka_authoring_mesh_remove_face(
                candidate, (henka_authoring_face_id)selected_ids[index]);
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, HENKA_AUTHORING_INVALID_ID);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_set_selected_face_material_region(
    sandbox3d_authoring_object* object,
    uint32_t material_region)
{
    henka_authoring_mesh* candidate = NULL;
    henka_result result;

    if (object == NULL || henka_authoring_mesh_get_face(object->mesh, object->selected_face) == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_set_face_material_region(
            candidate, object->selected_face, material_region);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, object->selected_face);
        if (result != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_extrude_selected_face(
    sandbox3d_authoring_object* object,
    float distance)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_result result;
    if (object == NULL || !isfinite(distance))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_extrude_face(
            candidate, object->selected_face, distance, &new_face_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, new_face_id);
        if (result == HENKA_SUCCESS)
        {
            object->selected_face = new_face_id;
        }
        else
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_inset_selected_face(
    sandbox3d_authoring_object* object,
    float factor)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_result result;
    if (object == NULL || !isfinite(factor))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_inset_face(
            candidate, object->selected_face, factor, &new_face_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, new_face_id);
        if (result == HENKA_SUCCESS)
        {
            object->selected_face = new_face_id;
        }
        else
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_bevel_selected_face(
    sandbox3d_authoring_object* object,
    float width)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_result result;
    if (object == NULL || !isfinite(width))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_bevel_face(
            candidate, object->selected_face, width, &new_face_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, new_face_id);
        if (result == HENKA_SUCCESS)
        {
            object->selected_face = new_face_id;
        }
        else
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_subdivide_selected_face(
    sandbox3d_authoring_object* object)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_vertex_id center_vertex_id = HENKA_AUTHORING_INVALID_ID;
    henka_result result;
    if (object == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_subdivide_face(
            candidate, object->selected_face, &center_vertex_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, object->selected_face);
        if (result == HENKA_SUCCESS)
        {
            (void)center_vertex_id;
            sandbox3d_authoring_repair_selection(object);
        }
        else
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_project_selected_face_uv(
    sandbox3d_authoring_object* object,
    henka_authoring_uv_projection_axis axis)
{
    henka_authoring_mesh* candidate = NULL;
    henka_result result;
    if (object == NULL || axis > HENKA_AUTHORING_UV_PROJECT_Z)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_project_face_uv(
            candidate, object->selected_face, axis);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, object->selected_face);
        if (result != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_pack_selected_face_uv(
    sandbox3d_authoring_object* object,
    float padding)
{
    henka_authoring_mesh* candidate = NULL;
    henka_result result;
    if (object == NULL || !isfinite(padding))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_pack_face_uv(
            candidate, object->selected_face, padding);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, true, object->selected_face);
        if (result != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_save_source(
    const sandbox3d_authoring_object* object,
    const char* path)
{
    if (object == NULL || path == NULL || path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    {
        const henka_result result = henka_authoring_mesh_save_file(object->mesh, path);
        if (result == HENKA_SUCCESS)
        {
            (void)sandbox3d_authoring_set_source_path((sandbox3d_authoring_object*)object, path);
        }
        return result;
    }
}

henka_result sandbox3d_authoring_object_reload_source(
    sandbox3d_authoring_object* object,
    const char* path)
{
    henka_authoring_mesh* candidate = NULL;
    henka_result result;
    if (object == NULL || path == NULL || path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_load_file(candidate, path);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_replace_loaded_source(object, candidate);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_save_project(
    const sandbox3d_authoring_object* object,
    const char* project_path,
    const char* source_path)
{
    henka_settings* settings = NULL;
    henka_transform transform;
    henka_result result;

    if (object == NULL || project_path == NULL || project_path[0] == '\0' ||
        source_path == NULL || source_path[0] == '\0' ||
        henka_scene_get_entity_transform(object->scene, object->entity, &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_save_file(object->mesh, source_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_settings_create(&settings);
    if (result == HENKA_SUCCESS) result = henka_settings_set_int(settings, "project.version", 1);
    if (result == HENKA_SUCCESS) result = henka_settings_set_string(settings, "project.source_path", source_path);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.position.x", transform.position.x);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.position.y", transform.position.y);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.position.z", transform.position.z);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.rotation.x", transform.rotation.x);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.rotation.y", transform.rotation.y);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.rotation.z", transform.rotation.z);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.rotation.w", transform.rotation.w);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.scale.x", transform.scale.x);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.scale.y", transform.scale.y);
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, "transform.scale.z", transform.scale.z);
    if (result == HENKA_SUCCESS) result = henka_settings_set_bool(
        settings, "entity.visible", henka_scene_is_entity_visible(object->scene, object->entity));
    if (result == HENKA_SUCCESS) result = henka_settings_save_file(settings, project_path);
    henka_settings_destroy(settings);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_set_source_path((sandbox3d_authoring_object*)object, source_path);
    }
    return result;
}

henka_result sandbox3d_authoring_object_load_project(
    sandbox3d_authoring_object* object,
    const char* project_path)
{
    henka_settings* settings = NULL;
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_mesh* rollback_mesh = NULL;
    henka_transform transform;
    henka_transform previous_transform = henka_transform_identity();
    const char* source_path;
    bool visible = false;
    bool previous_visible;
    bool have_previous_scene_state = false;
    int version;
    henka_result result;

    if (object == NULL || project_path == NULL || project_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_settings_create(&settings);
    if (result == HENKA_SUCCESS) result = henka_settings_load_file(settings, project_path);
    version = result == HENKA_SUCCESS ? henka_settings_get_int(settings, "project.version", 0) : 0;
    source_path = result == HENKA_SUCCESS ? henka_settings_get_string(settings, "project.source_path", NULL) : NULL;
    if (result == HENKA_SUCCESS && (version != 1 || source_path == NULL || source_path[0] == '\0'))
        result = HENKA_ERROR_UNKNOWN;
    transform = henka_transform_identity();
    if (result == HENKA_SUCCESS)
    {
        transform.position.x = henka_settings_get_float(settings, "transform.position.x", NAN);
        transform.position.y = henka_settings_get_float(settings, "transform.position.y", NAN);
        transform.position.z = henka_settings_get_float(settings, "transform.position.z", NAN);
        transform.rotation.x = henka_settings_get_float(settings, "transform.rotation.x", NAN);
        transform.rotation.y = henka_settings_get_float(settings, "transform.rotation.y", NAN);
        transform.rotation.z = henka_settings_get_float(settings, "transform.rotation.z", NAN);
        transform.rotation.w = henka_settings_get_float(settings, "transform.rotation.w", NAN);
        transform.scale.x = henka_settings_get_float(settings, "transform.scale.x", NAN);
        transform.scale.y = henka_settings_get_float(settings, "transform.scale.y", NAN);
        transform.scale.z = henka_settings_get_float(settings, "transform.scale.z", NAN);
        visible = henka_settings_get_bool(settings, "entity.visible", false);
        if (!sandbox3d_authoring_finite_vec3(transform.position) ||
            !sandbox3d_authoring_finite_vec3(transform.scale) ||
            !isfinite(transform.rotation.x) || !isfinite(transform.rotation.y) ||
            !isfinite(transform.rotation.z) || !isfinite(transform.rotation.w) ||
            transform.scale.x <= 0.0f || transform.scale.y <= 0.0f || transform.scale.z <= 0.0f)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    if (result == HENKA_SUCCESS) result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result == HENKA_SUCCESS) result = henka_authoring_mesh_clone(object->mesh, &rollback_mesh);
    if (result == HENKA_SUCCESS) result = henka_authoring_mesh_load_file(candidate, source_path);
    if (result == HENKA_SUCCESS) result = henka_scene_get_entity_transform(object->scene, object->entity, &previous_transform);
    if (result == HENKA_SUCCESS) have_previous_scene_state = true;
    previous_visible = result == HENKA_SUCCESS ? henka_scene_is_entity_visible(object->scene, object->entity) : false;
    if (result == HENKA_SUCCESS) result = sandbox3d_authoring_replace_loaded_source(object, candidate);
    if (result == HENKA_SUCCESS) result = henka_scene_set_entity_transform(object->scene, object->entity, transform);
    if (result == HENKA_SUCCESS) result = henka_scene_set_entity_visible(object->scene, object->entity, visible);
    if (result == HENKA_SUCCESS) result = sandbox3d_authoring_set_source_path(object, source_path);
    if (result != HENKA_SUCCESS)
    {
        if (candidate != NULL && candidate != object->mesh) henka_authoring_mesh_destroy(candidate);
        if (rollback_mesh != NULL)
        {
            (void)sandbox3d_authoring_replace_loaded_source(object, rollback_mesh);
            rollback_mesh = NULL;
        }
        if (have_previous_scene_state)
        {
            (void)henka_scene_set_entity_transform(object->scene, object->entity, previous_transform);
            (void)henka_scene_set_entity_visible(object->scene, object->entity, previous_visible);
        }
    }
    henka_authoring_mesh_destroy(rollback_mesh);
    henka_settings_destroy(settings);
    return result;
}

const char* sandbox3d_authoring_object_get_source_path(
    const sandbox3d_authoring_object* object)
{
    return object != NULL && object->source_path != NULL ? object->source_path : "";
}

static henka_result sandbox3d_authoring_history_move(
    sandbox3d_authoring_object* object,
    bool undo)
{
    henka_authoring_mesh* candidate = NULL;
    henka_result result;
    if (object == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(object->mesh, &candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = undo
        ? henka_authoring_mesh_history_undo(object->history, candidate)
        : henka_authoring_mesh_history_redo(object->history, candidate);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_authoring_publish_candidate(
            object, candidate, false, object->selected_face);
        if (result == HENKA_SUCCESS && object->selection_history_count > 0U)
        {
            if (undo)
            {
                if (object->selection_history_cursor > 0U)
                {
                    --object->selection_history_cursor;
                }
            }
            else if (object->selection_history_cursor + 1U < object->selection_history_count)
            {
                ++object->selection_history_cursor;
            }
            sandbox3d_authoring_restore_selection_history(object);
        }
        if (result != HENKA_SUCCESS)
        {
            (void)(undo
                ? henka_authoring_mesh_history_redo(object->history, object->mesh)
                : henka_authoring_mesh_history_undo(object->history, object->mesh));
            henka_authoring_mesh_destroy(candidate);
        }
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}

henka_result sandbox3d_authoring_object_undo(sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_history_move(object, true);
}

henka_result sandbox3d_authoring_object_redo(sandbox3d_authoring_object* object)
{
    return sandbox3d_authoring_history_move(object, false);
}
