#include <henka/authoring_uv.h>

#include <math.h>
#include <stdint.h>

#include <henka/memory.h>

static bool uv_finite_vec2(henka_vec2 value)
{
    return isfinite(value.x) && isfinite(value.y);
}

static henka_result uv_commit(henka_authoring_mesh* mesh, henka_authoring_mesh* candidate)
{
    henka_result result = henka_authoring_mesh_copy(mesh, candidate);
    henka_authoring_mesh_destroy(candidate);
    return result;
}

static bool uv_face_valid(const henka_authoring_mesh* mesh, henka_authoring_face_id face_id)
{
    const henka_authoring_face* face = henka_authoring_mesh_get_face(mesh, face_id);
    size_t corner;
    if (face == NULL || face->uvs == NULL)
    {
        return false;
    }
    for (corner = 0U; corner < face->corner_count; ++corner)
    {
        if (!uv_finite_vec2(face->uvs[corner]))
        {
            return false;
        }
    }
    return true;
}

static size_t uv_face_id_hash(
    henka_authoring_face_id face_id,
    size_t capacity)
{
    uint64_t key = (uint64_t)face_id * UINT64_C(0x9e3779b97f4a7c15);
    key ^= key >> 30U;
    key *= UINT64_C(0xbf58476d1ce4e5b9);
    key ^= key >> 27U;
    return (size_t)(key & (uint64_t)(capacity - 1U));
}

static bool uv_face_id_set_contains(
    const henka_authoring_face_id* seen_ids,
    size_t capacity,
    henka_authoring_face_id face_id)
{
    size_t probe;
    size_t attempt;

    if (seen_ids == NULL || capacity == 0U || face_id == HENKA_AUTHORING_INVALID_ID)
    {
        return false;
    }
    probe = uv_face_id_hash(face_id, capacity);
    for (attempt = 0U; attempt < capacity; ++attempt)
    {
        if (seen_ids[probe] == 0U)
        {
            return false;
        }
        if (seen_ids[probe] == face_id)
        {
            return true;
        }
        probe = (probe + 1U) & (capacity - 1U);
    }
    return false;
}

static bool uv_face_id_set_insert(
    henka_authoring_face_id* seen_ids,
    size_t capacity,
    henka_authoring_face_id face_id)
{
    size_t probe;
    size_t attempt;

    if (seen_ids == NULL || capacity == 0U || face_id == HENKA_AUTHORING_INVALID_ID)
    {
        return false;
    }
    probe = uv_face_id_hash(face_id, capacity);
    for (attempt = 0U; attempt < capacity; ++attempt)
    {
        if (seen_ids[probe] == 0U || seen_ids[probe] == face_id)
        {
            seen_ids[probe] = face_id;
            return true;
        }
        probe = (probe + 1U) & (capacity - 1U);
    }
    return false;
}

static henka_result uv_collect_island(
    const henka_authoring_mesh* mesh,
    henka_authoring_face_id seed_face_id,
    henka_authoring_face_id** out_face_ids,
    size_t* out_face_count)
{
    const henka_authoring_mesh_counts counts = mesh == NULL
        ? (henka_authoring_mesh_counts){0U, 0U, 0U}
        : henka_authoring_mesh_get_counts(mesh);
    henka_authoring_face_id* face_ids = NULL;
    henka_authoring_face_id* seen_ids = NULL;
    size_t face_count = 0U;
    size_t set_capacity = 1U;
    size_t queue_index;

    if (mesh == NULL || out_face_ids == NULL || out_face_count == NULL ||
        seed_face_id == HENKA_AUTHORING_INVALID_ID || counts.faces == 0U ||
        counts.faces > SIZE_MAX / sizeof(*face_ids) ||
        counts.faces > SIZE_MAX / (2U * sizeof(*seen_ids)) ||
        !henka_authoring_mesh_validate(mesh) || !uv_face_valid(mesh, seed_face_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    while (set_capacity < counts.faces * 2U)
    {
        if (set_capacity > SIZE_MAX / 2U)
        {
            return HENKA_ERROR_LIMIT;
        }
        set_capacity *= 2U;
    }
    face_ids = henka_calloc(counts.faces, sizeof(*face_ids));
    seen_ids = henka_calloc(set_capacity, sizeof(*seen_ids));
    if (face_ids == NULL || seen_ids == NULL)
    {
        henka_free(face_ids);
        henka_free(seen_ids);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    face_ids[face_count++] = seed_face_id;
    if (!uv_face_id_set_insert(seen_ids, set_capacity, seed_face_id))
    {
        henka_free(face_ids);
        henka_free(seen_ids);
        return HENKA_ERROR_LIMIT;
    }
    for (queue_index = 0U; queue_index < face_count; ++queue_index)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            mesh, face_ids[queue_index]);
        size_t corner;
        if (face == NULL || face->edges == NULL || !uv_face_valid(mesh, face->id))
        {
            henka_free(face_ids);
            henka_free(seen_ids);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                mesh, face->edges[corner]);
            size_t incident;
            if (edge == NULL || edge->face_count == 0U || edge->face_count > 2U)
            {
                henka_free(face_ids);
                henka_free(seen_ids);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            for (incident = 0U; incident < edge->face_count; ++incident)
            {
                henka_authoring_face_id neighbor_id = HENKA_AUTHORING_INVALID_ID;
                const henka_authoring_face* neighbor;
                if (henka_authoring_mesh_get_edge_face_at(
                        mesh, edge->id, incident, &neighbor_id) != HENKA_SUCCESS ||
                    neighbor_id == face->id)
                {
                    continue;
                }
                neighbor = henka_authoring_mesh_get_face(mesh, neighbor_id);
                if (neighbor == NULL || !uv_face_valid(mesh, neighbor_id))
                {
                    henka_free(face_ids);
                    henka_free(seen_ids);
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
                if (!henka_authoring_mesh_faces_share_uv_seam(
                        mesh, face->id, neighbor_id) &&
                    !uv_face_id_set_contains(seen_ids, set_capacity, neighbor_id))
                {
                    if (face_count >= counts.faces)
                    {
                        henka_free(face_ids);
                        henka_free(seen_ids);
                        return HENKA_ERROR_LIMIT;
                    }
                    if (!uv_face_id_set_insert(seen_ids, set_capacity, neighbor_id))
                    {
                        henka_free(face_ids);
                        henka_free(seen_ids);
                        return HENKA_ERROR_LIMIT;
                    }
                    face_ids[face_count++] = neighbor_id;
                }
            }
        }
    }
    *out_face_ids = face_ids;
    *out_face_count = face_count;
    henka_free(seen_ids);
    return HENKA_SUCCESS;
}

static henka_result uv_apply_island_transform(
    henka_authoring_mesh* mesh,
    const henka_authoring_face_id* face_ids,
    size_t face_count,
    henka_vec2 scale,
    henka_vec2 offset)
{
    size_t face_index;

    for (face_index = 0U; face_index < face_count; ++face_index)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            mesh, face_ids[face_index]);
        size_t corner;
        if (face == NULL || face->uvs == NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const henka_vec2 source = face->uvs[corner];
            const henka_vec2 value = {
                source.x * scale.x + offset.x,
                source.y * scale.y + offset.y};
            if (!uv_finite_vec2(value))
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            {
                const henka_result result = henka_authoring_mesh_set_face_corner_uv(
                    mesh, face_ids[face_index], corner, value);
                if (result != HENKA_SUCCESS)
                {
                    return result;
                }
            }
        }
    }
    return HENKA_SUCCESS;
}

bool henka_authoring_mesh_face_uvs_are_finite(const henka_authoring_mesh* mesh, henka_authoring_face_id face_id)
{
    return uv_face_valid(mesh, face_id);
}

static henka_vec2 uv_projection(henka_vec3 position, henka_authoring_uv_projection_axis axis)
{
    switch (axis)
    {
    case HENKA_AUTHORING_UV_PROJECT_X:
        return (henka_vec2){position.z, position.y};
    case HENKA_AUTHORING_UV_PROJECT_Y:
        return (henka_vec2){position.x, position.z};
    case HENKA_AUTHORING_UV_PROJECT_Z:
        return (henka_vec2){position.x, position.y};
    default:
        return (henka_vec2){0.0f, 0.0f};
    }
}

henka_result henka_authoring_mesh_project_face_uv(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    henka_authoring_uv_projection_axis axis)
{
    henka_authoring_mesh* candidate = NULL;
    const henka_authoring_face* source;
    henka_vec2 projected[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_vec2 minimum = {0.0f, 0.0f};
    henka_vec2 maximum = {0.0f, 0.0f};
    size_t corner;
    henka_result result;
    if (mesh == NULL || axis > HENKA_AUTHORING_UV_PROJECT_Z || !uv_face_valid(mesh, face_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    source = henka_authoring_mesh_get_face(mesh, face_id);
    if (source->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
    {
        return HENKA_ERROR_LIMIT;
    }
    for (corner = 0U; corner < source->corner_count; ++corner)
    {
        projected[corner] = uv_projection(
            henka_authoring_mesh_get_vertex(mesh, source->vertices[corner])->position, axis);
        if (corner == 0U || projected[corner].x < minimum.x) minimum.x = projected[corner].x;
        if (corner == 0U || projected[corner].y < minimum.y) minimum.y = projected[corner].y;
        if (corner == 0U || projected[corner].x > maximum.x) maximum.x = projected[corner].x;
        if (corner == 0U || projected[corner].y > maximum.y) maximum.y = projected[corner].y;
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        for (corner = 0U; corner < source->corner_count; ++corner)
        {
            henka_vec2 value = {
                maximum.x - minimum.x > 0.000001f ?
                    (projected[corner].x - minimum.x) / (maximum.x - minimum.x) : 0.5f,
                maximum.y - minimum.y > 0.000001f ?
                    (projected[corner].y - minimum.y) / (maximum.y - minimum.y) : 0.5f};
            result = henka_authoring_mesh_set_face_corner_uv(candidate, face_id, corner, value);
            if (result != HENKA_SUCCESS) break;
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = uv_commit(mesh, candidate);
        candidate = NULL;
    }
    henka_authoring_mesh_destroy(candidate);
    return result;
}

henka_result henka_authoring_mesh_transform_face_uv(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    henka_vec2 scale,
    henka_vec2 offset)
{
    henka_authoring_mesh* candidate = NULL;
    const henka_authoring_face* source;
    size_t corner;
    henka_result result;
    if (mesh == NULL || !uv_finite_vec2(scale) || !uv_finite_vec2(offset) ||
        !uv_face_valid(mesh, face_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    source = henka_authoring_mesh_get_face(mesh, face_id);
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        for (corner = 0U; corner < source->corner_count; ++corner)
        {
            const henka_vec2 uv = source->uvs[corner];
            result = henka_authoring_mesh_set_face_corner_uv(candidate, face_id, corner,
                (henka_vec2){uv.x * scale.x + offset.x, uv.y * scale.y + offset.y});
            if (result != HENKA_SUCCESS ||
                !uv_finite_vec2(henka_authoring_mesh_get_face(candidate, face_id)->uvs[corner]))
            {
                break;
            }
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = uv_commit(mesh, candidate);
        candidate = NULL;
    }
    henka_authoring_mesh_destroy(candidate);
    return result;
}

henka_result henka_authoring_mesh_transform_uv_island(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id seed_face_id,
    henka_vec2 scale,
    henka_vec2 offset)
{
    henka_authoring_face_id* face_ids = NULL;
    henka_authoring_mesh* candidate = NULL;
    size_t face_count = 0U;
    henka_result result;

    if (mesh == NULL || !uv_finite_vec2(scale) || !uv_finite_vec2(offset))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = uv_collect_island(mesh, seed_face_id, &face_ids, &face_count);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_clone(mesh, &candidate);
    }
    if (result == HENKA_SUCCESS)
    {
        result = uv_apply_island_transform(
            candidate, face_ids, face_count, scale, offset);
    }
    if (result == HENKA_SUCCESS)
    {
        result = uv_commit(mesh, candidate);
        candidate = NULL;
    }
    henka_authoring_mesh_destroy(candidate);
    henka_free(face_ids);
    return result;
}

henka_result henka_authoring_mesh_pack_face_uv(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    float padding)
{
    henka_authoring_mesh* candidate = NULL;
    const henka_authoring_face* source;
    henka_vec2 minimum = {0.0f, 0.0f};
    henka_vec2 maximum = {0.0f, 0.0f};
    float scale;
    size_t corner;
    henka_result result;
    if (mesh == NULL || !isfinite(padding) || padding < 0.0f || padding >= 0.5f ||
        !uv_face_valid(mesh, face_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    source = henka_authoring_mesh_get_face(mesh, face_id);
    for (corner = 0U; corner < source->corner_count; ++corner)
    {
        const henka_vec2 uv = source->uvs[corner];
        if (corner == 0U || uv.x < minimum.x) minimum.x = uv.x;
        if (corner == 0U || uv.y < minimum.y) minimum.y = uv.y;
        if (corner == 0U || uv.x > maximum.x) maximum.x = uv.x;
        if (corner == 0U || uv.y > maximum.y) maximum.y = uv.y;
    }
    {
        const float width = maximum.x - minimum.x;
        const float height = maximum.y - minimum.y;
        const float extent = width > height ? width : height;
        scale = extent > 0.000001f ? (1.0f - 2.0f * padding) / extent : 1.0f;
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        for (corner = 0U; corner < source->corner_count; ++corner)
        {
            const henka_vec2 uv = source->uvs[corner];
            result = henka_authoring_mesh_set_face_corner_uv(candidate, face_id, corner,
                (henka_vec2){padding + (uv.x - minimum.x) * scale,
                    padding + (uv.y - minimum.y) * scale});
            if (result != HENKA_SUCCESS) break;
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = uv_commit(mesh, candidate);
        candidate = NULL;
    }
    henka_authoring_mesh_destroy(candidate);
    return result;
}

henka_result henka_authoring_mesh_pack_uv_island(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id seed_face_id,
    float padding)
{
    henka_authoring_face_id* face_ids = NULL;
    henka_authoring_mesh* candidate = NULL;
    henka_vec2 minimum = {0.0f, 0.0f};
    henka_vec2 maximum = {0.0f, 0.0f};
    size_t face_count = 0U;
    size_t face_index;
    float scale;
    henka_result result;

    if (mesh == NULL || !isfinite(padding) || padding < 0.0f || padding >= 0.5f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = uv_collect_island(mesh, seed_face_id, &face_ids, &face_count);
    for (face_index = 0U; result == HENKA_SUCCESS && face_index < face_count; ++face_index)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            mesh, face_ids[face_index]);
        size_t corner;
        if (face == NULL || face->uvs == NULL)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            break;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const henka_vec2 uv = face->uvs[corner];
            if (face_index == 0U && corner == 0U)
            {
                minimum = uv;
                maximum = uv;
            }
            else
            {
                if (uv.x < minimum.x) minimum.x = uv.x;
                if (uv.y < minimum.y) minimum.y = uv.y;
                if (uv.x > maximum.x) maximum.x = uv.x;
                if (uv.y > maximum.y) maximum.y = uv.y;
            }
        }
    }
    if (result == HENKA_SUCCESS)
    {
        const float width = maximum.x - minimum.x;
        const float height = maximum.y - minimum.y;
        const float extent = width > height ? width : height;
        scale = extent > 0.000001f ? (1.0f - 2.0f * padding) / extent : 1.0f;
        result = henka_authoring_mesh_clone(mesh, &candidate);
        if (result == HENKA_SUCCESS)
        {
            result = uv_apply_island_transform(
                candidate,
                face_ids,
                face_count,
                (henka_vec2){scale, scale},
                (henka_vec2){
                    padding - minimum.x * scale,
                    padding - minimum.y * scale});
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = uv_commit(mesh, candidate);
        candidate = NULL;
    }
    henka_authoring_mesh_destroy(candidate);
    henka_free(face_ids);
    return result;
}

typedef struct uv_island_layout
{
    henka_authoring_face_id* face_ids;
    size_t face_count;
    henka_vec2 minimum;
    henka_vec2 maximum;
} uv_island_layout;

static void uv_destroy_island_layouts(
    uv_island_layout* layouts,
    size_t layout_count)
{
    size_t index;
    if (layouts == NULL)
    {
        return;
    }
    for (index = 0U; index < layout_count; ++index)
    {
        henka_free(layouts[index].face_ids);
    }
    henka_free(layouts);
}

henka_result henka_authoring_mesh_pack_uv_islands(
    henka_authoring_mesh* mesh,
    float padding)
{
    const henka_authoring_mesh_counts counts = henka_authoring_mesh_get_counts(mesh);
    const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
    uv_island_layout* layouts = NULL;
    henka_authoring_face_id* seen_ids = NULL;
    henka_authoring_mesh* candidate = NULL;
    size_t set_capacity = 1U;
    size_t island_count = 0U;
    size_t face_slot;
    size_t column_count;
    size_t row_count;
    size_t island_index;
    float interior_width = 0.0f;
    float interior_height = 0.0f;
    float cell_width = 0.0f;
    float cell_height = 0.0f;
    float available_width = 0.0f;
    float available_height = 0.0f;
    henka_result result = HENKA_SUCCESS;

    if (mesh == NULL || !isfinite(padding) || padding < 0.0f || padding >= 0.5f ||
        !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (counts.faces == 0U)
    {
        return HENKA_SUCCESS;
    }
    if (counts.faces > SIZE_MAX / 2U)
    {
        return HENKA_ERROR_LIMIT;
    }
    while (set_capacity < counts.faces * 2U)
    {
        if (set_capacity > SIZE_MAX / 2U)
        {
            return HENKA_ERROR_LIMIT;
        }
        set_capacity *= 2U;
    }
    if (counts.faces > SIZE_MAX / sizeof(*layouts) ||
        set_capacity > SIZE_MAX / sizeof(*seen_ids))
    {
        return HENKA_ERROR_LIMIT;
    }
    layouts = henka_calloc(counts.faces, sizeof(*layouts));
    seen_ids = henka_calloc(set_capacity, sizeof(*seen_ids));
    if (layouts == NULL || seen_ids == NULL)
    {
        uv_destroy_island_layouts(layouts, island_count);
        henka_free(seen_ids);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (face_slot = 0U; face_slot < desc.max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
        henka_authoring_face_id* island_face_ids = NULL;
        size_t island_face_count = 0U;
        size_t face_index;

        if (henka_authoring_mesh_get_face_id_at(mesh, face_slot, &face_id) != HENKA_SUCCESS ||
            uv_face_id_set_contains(seen_ids, set_capacity, face_id))
        {
            continue;
        }
        if (island_count >= counts.faces)
        {
            result = HENKA_ERROR_LIMIT;
            break;
        }
        result = uv_collect_island(mesh, face_id, &island_face_ids, &island_face_count);
        if (result != HENKA_SUCCESS)
        {
            break;
        }
        for (face_index = 0U; face_index < island_face_count; ++face_index)
        {
            const henka_authoring_face* face = henka_authoring_mesh_get_face(
                mesh, island_face_ids[face_index]);
            size_t corner;
            if (face == NULL || face->uvs == NULL || face->corner_count == 0U)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                break;
            }
            if (!uv_face_id_set_insert(
                    seen_ids, set_capacity, island_face_ids[face_index]))
            {
                result = HENKA_ERROR_LIMIT;
                break;
            }
            for (corner = 0U; corner < face->corner_count; ++corner)
            {
                const henka_vec2 uv = face->uvs[corner];
                if (!uv_finite_vec2(uv))
                {
                    result = HENKA_ERROR_NUMERIC_RANGE;
                    break;
                }
                if (face_index == 0U && corner == 0U)
                {
                    layouts[island_count].minimum = uv;
                    layouts[island_count].maximum = uv;
                }
                else
                {
                    if (uv.x < layouts[island_count].minimum.x)
                        layouts[island_count].minimum.x = uv.x;
                    if (uv.y < layouts[island_count].minimum.y)
                        layouts[island_count].minimum.y = uv.y;
                    if (uv.x > layouts[island_count].maximum.x)
                        layouts[island_count].maximum.x = uv.x;
                    if (uv.y > layouts[island_count].maximum.y)
                        layouts[island_count].maximum.y = uv.y;
                }
            }
            if (result != HENKA_SUCCESS)
            {
                break;
            }
        }
        if (result != HENKA_SUCCESS)
        {
            henka_free(island_face_ids);
            break;
        }
        layouts[island_count].face_ids = island_face_ids;
        layouts[island_count].face_count = island_face_count;
        ++island_count;
    }
    if (result == HENKA_SUCCESS && island_count == 0U)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    column_count = 1U;
    while (result == HENKA_SUCCESS &&
           column_count <= island_count / column_count &&
           column_count * column_count < island_count)
    {
        if (column_count == SIZE_MAX)
        {
            result = HENKA_ERROR_LIMIT;
            break;
        }
        ++column_count;
    }
    if (result == HENKA_SUCCESS)
    {
        row_count = (island_count + column_count - 1U) / column_count;
        interior_width = 1.0f - 2.0f * padding * (float)column_count;
        interior_height = 1.0f - 2.0f * padding * (float)row_count;
        cell_width = interior_width / (float)column_count;
        cell_height = interior_height / (float)row_count;
        available_width = cell_width;
        available_height = cell_height;
        if (!isfinite(cell_width) || !isfinite(cell_height) ||
            available_width <= 0.0f || available_height <= 0.0f)
        {
            result = HENKA_ERROR_LIMIT;
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_clone(mesh, &candidate);
    }
    for (island_index = 0U; result == HENKA_SUCCESS && island_index < island_count;
         ++island_index)
    {
        const uv_island_layout* layout = &layouts[island_index];
        const float width = layout->maximum.x - layout->minimum.x;
        const float height = layout->maximum.y - layout->minimum.y;
        float scale = 1.0f;
        const size_t column = island_index % column_count;
        const size_t row = island_index / column_count;
        const float cell_origin_x = padding + (float)column * (cell_width + 2.0f * padding);
        const float cell_origin_y = padding + (float)row * (cell_height + 2.0f * padding);
        float packed_width;
        float packed_height;
        henka_vec2 offset;

        if (!isfinite(width) || !isfinite(height) || width < 0.0f || height < 0.0f)
        {
            result = HENKA_ERROR_NUMERIC_RANGE;
            break;
        }
        if (width > 0.000001f)
        {
            scale = available_width / width;
        }
        if (height > 0.000001f && available_height / height < scale)
        {
            scale = available_height / height;
        }
        if (!isfinite(scale) || scale <= 0.0f)
        {
            result = HENKA_ERROR_NUMERIC_RANGE;
            break;
        }
        packed_width = width * scale;
        packed_height = height * scale;
        offset = (henka_vec2){
            cell_origin_x + (available_width - packed_width) * 0.5f -
                layout->minimum.x * scale,
            cell_origin_y + (available_height - packed_height) * 0.5f -
                layout->minimum.y * scale};
        if (!uv_finite_vec2(offset) ||
            !uv_finite_vec2((henka_vec2){scale, scale}))
        {
            result = HENKA_ERROR_NUMERIC_RANGE;
            break;
        }
        result = uv_apply_island_transform(
            candidate, layout->face_ids, layout->face_count,
            (henka_vec2){scale, scale}, offset);
    }
    if (result == HENKA_SUCCESS)
    {
        result = uv_commit(mesh, candidate);
        candidate = NULL;
    }
    henka_authoring_mesh_destroy(candidate);
    uv_destroy_island_layouts(layouts, island_count);
    henka_free(seen_ids);
    return result;
}

bool henka_authoring_mesh_faces_share_uv_seam(
    const henka_authoring_mesh* mesh,
    henka_authoring_face_id first_face_id,
    henka_authoring_face_id second_face_id)
{
    const henka_authoring_face* first = henka_authoring_mesh_get_face(mesh, first_face_id);
    const henka_authoring_face* second = henka_authoring_mesh_get_face(mesh, second_face_id);
    size_t first_corner;
    size_t second_corner;
    if (first == NULL || second == NULL || first->uvs == NULL || second->uvs == NULL)
    {
        return false;
    }
    for (first_corner = 0U; first_corner < first->corner_count; ++first_corner)
    {
        const henka_authoring_vertex_id first_vertex = first->vertices[first_corner];
        const henka_authoring_vertex_id first_next = first->vertices[(first_corner + 1U) % first->corner_count];
        for (second_corner = 0U; second_corner < second->corner_count; ++second_corner)
        {
            const henka_authoring_vertex_id second_vertex = second->vertices[second_corner];
            const henka_authoring_vertex_id second_next = second->vertices[(second_corner + 1U) % second->corner_count];
            if ((first_vertex == second_vertex && first_next == second_next) ||
                (first_vertex == second_next && first_next == second_vertex))
            {
                const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(mesh, first->edges[first_corner]);
                if (edge != NULL && edge->seam)
                {
                    return true;
                }
                const henka_vec2 first_uv = first->uvs[first_corner];
                const henka_vec2 first_next_uv = first->uvs[(first_corner + 1U) % first->corner_count];
                const bool reversed = first_vertex == second_next && first_next == second_vertex;
                const henka_vec2 second_uv = second->uvs[reversed ?
                    (second_corner + 1U) % second->corner_count : second_corner];
                const henka_vec2 second_next_uv = second->uvs[reversed ?
                    second_corner : (second_corner + 1U) % second->corner_count];
                return fabsf(first_uv.x - second_uv.x) > 0.0001f || fabsf(first_uv.y - second_uv.y) > 0.0001f ||
                    fabsf(first_next_uv.x - second_next_uv.x) > 0.0001f || fabsf(first_next_uv.y - second_next_uv.y) > 0.0001f;
            }
        }
    }
    return false;
}
