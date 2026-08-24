#include <henka/authoring_topology.h>
#include <henka/memory.h>

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../core/checked.h"

typedef struct henka_topology_cell_bucket
{
    int64_t x;
    int64_t y;
    int64_t z;
    size_t head;
    bool occupied;
} henka_topology_cell_bucket;

typedef struct henka_topology_vertex_index_entry
{
    henka_authoring_vertex_id id;
    size_t index;
    bool occupied;
} henka_topology_vertex_index_entry;

typedef struct henka_topology_face_visit_entry
{
    henka_authoring_face_id id;
    bool occupied;
} henka_topology_face_visit_entry;

static size_t henka_topology_next_power_of_two(size_t requested)
{
    size_t capacity = 8U;

    while (capacity < requested)
    {
        if (capacity > ((size_t)-1) / 2U)
        {
            return 0U;
        }

        capacity *= 2U;
    }

    return capacity;
}

static uint64_t henka_topology_mix64(uint64_t value)
{
    value ^= value >> 30U;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27U;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31U;
    return value;
}

static bool henka_topology_face_visit_contains(
    const henka_topology_face_visit_entry* entries,
    size_t capacity,
    henka_authoring_face_id id)
{
    size_t slot;
    size_t attempt;
    if (entries == NULL || capacity == 0U || id == HENKA_AUTHORING_INVALID_ID)
    {
        return false;
    }
    slot = (size_t)(henka_topology_mix64((uint64_t)id) & (uint64_t)(capacity - 1U));
    for (attempt = 0U; attempt < capacity; ++attempt)
    {
        const henka_topology_face_visit_entry* entry = &entries[slot];
        if (!entry->occupied)
        {
            return false;
        }
        if (entry->id == id)
        {
            return true;
        }
        slot = (slot + 1U) & (capacity - 1U);
    }
    return false;
}

static bool henka_topology_face_visit_insert(
    henka_topology_face_visit_entry* entries,
    size_t capacity,
    henka_authoring_face_id id)
{
    size_t slot;
    size_t attempt;
    if (entries == NULL || capacity == 0U || id == HENKA_AUTHORING_INVALID_ID)
    {
        return false;
    }
    slot = (size_t)(henka_topology_mix64((uint64_t)id) & (uint64_t)(capacity - 1U));
    for (attempt = 0U; attempt < capacity; ++attempt)
    {
        henka_topology_face_visit_entry* entry = &entries[slot];
        if (entry->occupied)
        {
            if (entry->id == id)
            {
                return false;
            }
        }
        else
        {
            entry->id = id;
            entry->occupied = true;
            return true;
        }
        slot = (slot + 1U) & (capacity - 1U);
    }
    return false;
}

static size_t henka_topology_vertex_index_find(
    const henka_topology_vertex_index_entry* entries,
    size_t capacity,
    henka_authoring_vertex_id id)
{
    size_t slot;
    if (entries == NULL || capacity == 0U || id == HENKA_AUTHORING_INVALID_ID)
    {
        return SIZE_MAX;
    }
    slot = (size_t)(henka_topology_mix64((uint64_t)id) & (uint64_t)(capacity - 1U));
    for (size_t attempt = 0U; attempt < capacity; ++attempt)
    {
        const henka_topology_vertex_index_entry* entry = &entries[slot];
        if (!entry->occupied)
        {
            return SIZE_MAX;
        }
        if (entry->id == id)
        {
            return entry->index;
        }
        slot = (slot + 1U) & (capacity - 1U);
    }
    return SIZE_MAX;
}

static bool henka_topology_vertex_index_insert(
    henka_topology_vertex_index_entry* entries,
    size_t capacity,
    henka_authoring_vertex_id id,
    size_t index)
{
    size_t slot;
    if (entries == NULL || capacity == 0U || id == 0U || id == HENKA_AUTHORING_INVALID_ID)
    {
        return false;
    }
    slot = (size_t)(henka_topology_mix64((uint64_t)id) & (uint64_t)(capacity - 1U));
    for (size_t attempt = 0U; attempt < capacity; ++attempt)
    {
        henka_topology_vertex_index_entry* entry = &entries[slot];
        if (entry->occupied)
        {
            if (entry->id == id) return false;
        }
        else
        {
            entry->id = id;
            entry->index = index;
            entry->occupied = true;
            return true;
        }
        slot = (slot + 1U) & (capacity - 1U);
    }
    return false;
}

static uint64_t henka_topology_hash_cell(
    int64_t x,
    int64_t y,
    int64_t z)
{
    uint64_t hash =
        henka_topology_mix64((uint64_t)x);

    hash ^=
        henka_topology_mix64((uint64_t)y) +
        UINT64_C(0x9e3779b97f4a7c15);

    hash ^=
        henka_topology_mix64((uint64_t)z) +
        UINT64_C(0x517cc1b727220a95);

    return henka_topology_mix64(hash);
}

static size_t henka_topology_find_cell_slot(
    const henka_topology_cell_bucket* buckets,
    size_t capacity,
    int64_t x,
    int64_t y,
    int64_t z,
    bool* out_found)
{
    size_t slot;
    size_t probe;

    *out_found = false;

    if (buckets == NULL ||
        capacity == 0U)
    {
        return 0U;
    }

    slot =
        (size_t)(
            henka_topology_hash_cell(x, y, z) &
            (uint64_t)(capacity - 1U));

    for (probe = 0U;
         probe < capacity;
         ++probe)
    {
        const henka_topology_cell_bucket* bucket =
            &buckets[slot];

        if (!bucket->occupied)
        {
            return slot;
        }

        if (bucket->x == x &&
            bucket->y == y &&
            bucket->z == z)
        {
            *out_found = true;
            return slot;
        }

        slot =
            (slot + 1U) &
            (capacity - 1U);
    }

    return capacity;
}

static int64_t henka_topology_cell_coordinate(
    float value,
    float tolerance)
{
    const double scaled =
        floor(
            (double)value /
            (double)tolerance);

    if (scaled >= (double)LLONG_MAX)
    {
        return (int64_t)LLONG_MAX;
    }

    if (scaled <= (double)LLONG_MIN)
    {
        return (int64_t)LLONG_MIN;
    }

    return (int64_t)scaled;
}

static bool henka_topology_offset_cell(
    int64_t base,
    int offset,
    int64_t* out_value)
{
    if (out_value == NULL)
    {
        return false;
    }

    if (offset < 0 &&
        base == (int64_t)LLONG_MIN)
    {
        return false;
    }

    if (offset > 0 &&
        base == (int64_t)LLONG_MAX)
    {
        return false;
    }

    *out_value =
        base + (int64_t)offset;

    return true;
}

static bool henka_topology_positions_coincident(
    henka_vec3 first,
    henka_vec3 second,
    float tolerance)
{
    const double dx =
        (double)first.x - (double)second.x;
    const double dy =
        (double)first.y - (double)second.y;
    const double dz =
        (double)first.z - (double)second.z;
    const double distance_squared =
        dx * dx + dy * dy + dz * dz;
    const double tolerance_squared =
        (double)tolerance *
        (double)tolerance;

    return distance_squared <=
        tolerance_squared;
}

static size_t henka_topology_find_root(
    size_t* parent,
    size_t index)
{
    while (parent[index] != index)
    {
        parent[index] =
            parent[parent[index]];

        index =
            parent[index];
    }

    return index;
}

static void henka_topology_union(
    size_t* parent,
    unsigned char* rank,
    size_t first,
    size_t second)
{
    size_t first_root =
        henka_topology_find_root(
            parent,
            first);

    size_t second_root =
        henka_topology_find_root(
            parent,
            second);

    if (first_root == second_root)
    {
        return;
    }

    if (rank[first_root] <
        rank[second_root])
    {
        parent[first_root] =
            second_root;
    }
    else if (rank[first_root] >
        rank[second_root])
    {
        parent[second_root] =
            first_root;
    }
    else
    {
        parent[second_root] =
            first_root;

        rank[first_root] += 1U;
    }
}

static void henka_topology_sort_ids(
    uint32_t* ids,
    size_t count)
{
    size_t index;

    for (index = 1U;
         index < count;
         ++index)
    {
        const uint32_t value =
            ids[index];

        size_t cursor =
            index;

        while (cursor > 0U &&
            ids[cursor - 1U] > value)
        {
            ids[cursor] =
                ids[cursor - 1U];

            cursor -= 1U;
        }

        ids[cursor] =
            value;
    }
}

static uint64_t henka_topology_face_hash(
    const henka_authoring_face* face)
{
    uint32_t sorted[
        HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];

    uint64_t hash =
        UINT64_C(1469598103934665603);

    size_t index;

    if (face == NULL ||
        face->corner_count >
            HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
    {
        return 0U;
    }

    for (index = 0U;
         index < face->corner_count;
         ++index)
    {
        sorted[index] =
            face->vertices[index];
    }

    henka_topology_sort_ids(
        sorted,
        face->corner_count);

    hash ^=
        (uint64_t)face->corner_count;

    hash *=
        UINT64_C(1099511628211);

    for (index = 0U;
         index < face->corner_count;
         ++index)
    {
        hash ^=
            (uint64_t)sorted[index];

        hash *=
            UINT64_C(1099511628211);
    }

    return hash;
}

static bool henka_topology_faces_same_vertex_set(
    const henka_authoring_face* first,
    const henka_authoring_face* second)
{
    uint32_t first_sorted[
        HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];

    uint32_t second_sorted[
        HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];

    size_t index;

    if (first == NULL ||
        second == NULL ||
        first->corner_count !=
            second->corner_count ||
        first->corner_count >
            HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
    {
        return false;
    }

    for (index = 0U;
         index < first->corner_count;
         ++index)
    {
        first_sorted[index] =
            first->vertices[index];

        second_sorted[index] =
            second->vertices[index];
    }

    henka_topology_sort_ids(
        first_sorted,
        first->corner_count);

    henka_topology_sort_ids(
        second_sorted,
        second->corner_count);

    for (index = 0U;
         index < first->corner_count;
         ++index)
    {
        if (first_sorted[index] !=
            second_sorted[index])
        {
            return false;
        }
    }

    return true;
}

static int henka_topology_face_edge_direction(
    const henka_authoring_face* face,
    henka_authoring_vertex_id first,
    henka_authoring_vertex_id second)
{
    size_t corner;

    if (face == NULL ||
        face->corner_count < 2U)
    {
        return 0;
    }

    for (corner = 0U;
         corner < face->corner_count;
         ++corner)
    {
        const size_t next =
            (corner + 1U) %
            face->corner_count;

        if (face->vertices[corner] == first &&
            face->vertices[next] == second)
        {
            return 1;
        }

        if (face->vertices[corner] == second &&
            face->vertices[next] == first)
        {
            return -1;
        }
    }

    return 0;
}

static bool henka_topology_face_vertex_uv(
    const henka_authoring_face* face,
    henka_authoring_vertex_id vertex_id,
    henka_vec2* out_uv)
{
    size_t corner;

    if (face == NULL ||
        out_uv == NULL ||
        face->vertices == NULL ||
        face->uvs == NULL)
    {
        return false;
    }

    for (corner = 0U;
         corner < face->corner_count;
         ++corner)
    {
        if (face->vertices[corner] ==
            vertex_id)
        {
            *out_uv =
                face->uvs[corner];

            return true;
        }
    }

    return false;
}

static bool henka_topology_uv_differs(
    henka_vec2 first,
    henka_vec2 second,
    float tolerance)
{
    return
        fabsf(first.x - second.x) >
            tolerance ||
        fabsf(first.y - second.y) >
            tolerance;
}

static bool henka_topology_edge_is_uv_seam(
    const henka_authoring_mesh* mesh,
    const henka_authoring_edge* edge,
    float tolerance)
{
    const henka_authoring_face* first_face;
    const henka_authoring_face* second_face;
    henka_vec2 first_uv_a;
    henka_vec2 first_uv_b;
    henka_vec2 second_uv_a;
    henka_vec2 second_uv_b;

    if (mesh == NULL ||
        edge == NULL ||
        edge->face_count != 2U)
    {
        return false;
    }

    first_face =
        henka_authoring_mesh_get_face(
            mesh,
            edge->faces[0]);

    second_face =
        henka_authoring_mesh_get_face(
            mesh,
            edge->faces[1]);

    if (first_face == NULL ||
        second_face == NULL ||
        !henka_topology_face_vertex_uv(
            first_face,
            edge->vertices[0],
            &first_uv_a) ||
        !henka_topology_face_vertex_uv(
            first_face,
            edge->vertices[1],
            &first_uv_b) ||
        !henka_topology_face_vertex_uv(
            second_face,
            edge->vertices[0],
            &second_uv_a) ||
        !henka_topology_face_vertex_uv(
            second_face,
            edge->vertices[1],
            &second_uv_b))
    {
        return false;
    }

    return
        henka_topology_uv_differs(
            first_uv_a,
            second_uv_a,
            tolerance) ||
        henka_topology_uv_differs(
            first_uv_b,
            second_uv_b,
            tolerance);
}

static bool henka_topology_face_is_degenerate(
    const henka_authoring_mesh* mesh,
    const henka_authoring_face* face,
    double epsilon)
{
    double nx = 0.0;
    double ny = 0.0;
    double nz = 0.0;
    size_t corner;

    if (mesh == NULL ||
        face == NULL ||
        face->corner_count < 3U)
    {
        return true;
    }

    for (corner = 0U;
         corner < face->corner_count;
         ++corner)
    {
        const size_t next =
            (corner + 1U) %
            face->corner_count;

        const henka_authoring_vertex* first =
            henka_authoring_mesh_get_vertex(
                mesh,
                face->vertices[corner]);

        const henka_authoring_vertex* second =
            henka_authoring_mesh_get_vertex(
                mesh,
                face->vertices[next]);

        if (first == NULL ||
            second == NULL)
        {
            return true;
        }

        nx +=
            ((double)first->position.y -
             (double)second->position.y) *
            ((double)first->position.z +
             (double)second->position.z);

        ny +=
            ((double)first->position.z -
             (double)second->position.z) *
            ((double)first->position.x +
             (double)second->position.x);

        nz +=
            ((double)first->position.x -
             (double)second->position.x) *
            ((double)first->position.y +
             (double)second->position.y);
    }

    return
        nx * nx +
        ny * ny +
        nz * nz <=
        epsilon * epsilon;
}

henka_authoring_topology_options
henka_authoring_topology_options_default(void)
{
    henka_authoring_topology_options options;

    options.coincident_vertex_tolerance =
        0.00001f;

    options.uv_seam_tolerance =
        0.00001f;

    options.degenerate_normal_epsilon =
        0.00000001;

    return options;
}

henka_authoring_topology_profile_guidance
henka_authoring_topology_profile_get_guidance(
    henka_authoring_topology_profile profile)
{
    henka_authoring_topology_profile_guidance guidance;

    guidance.recommended_minimum_quad_ratio =
        0.0;

    guidance.prefer_manifold =
        true;

    guidance.allow_planar_ngons =
        true;

    if (profile ==
        HENKA_AUTHORING_TOPOLOGY_PROFILE_ORGANIC)
    {
        guidance.recommended_minimum_quad_ratio =
            0.85;

        guidance.allow_planar_ngons =
            false;
    }
    else if (profile ==
        HENKA_AUTHORING_TOPOLOGY_PROFILE_HARD_SURFACE)
    {
        guidance.recommended_minimum_quad_ratio =
            0.60;
    }

    return guidance;
}

henka_result henka_authoring_topology_get_cage_edges(
    const henka_authoring_mesh* mesh,
    henka_authoring_cage_edge* out_edges,
    size_t capacity,
    size_t* out_count)
{
    const henka_authoring_mesh_counts counts =
        henka_authoring_mesh_get_counts(mesh);

    henka_authoring_edge_id edge_id;
    size_t edge_slot;
    size_t written = 0U;

    if (out_count != NULL)
    {
        *out_count = 0U;
    }

    if (mesh == NULL ||
        out_count == NULL ||
        (out_edges == NULL &&
         capacity != 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_count =
        counts.edges;

    if (out_edges == NULL)
    {
        return HENKA_SUCCESS;
    }

    if (capacity <
        counts.edges)
    {
        return HENKA_ERROR_LIMIT;
    }

    for (edge_slot = 0U;
         edge_slot < henka_authoring_mesh_get_desc(mesh).max_edges;
         ++edge_slot)
    {
        if (henka_authoring_mesh_get_edge_id_at(mesh, edge_slot, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        const henka_authoring_edge* edge =
            henka_authoring_mesh_get_edge(
                mesh,
                edge_id);

        if (edge == NULL)
        {
            continue;
        }

        if (written >= capacity)
        {
            return HENKA_ERROR_LIMIT;
        }

        out_edges[written].edge_id =
            edge->id;

        out_edges[written].vertices[0] =
            edge->vertices[0];

        out_edges[written].vertices[1] =
            edge->vertices[1];

        out_edges[written].boundary =
            edge->face_count == 1U;

        out_edges[written].hard =
            edge->hard;

        written += 1U;
    }

    if (written != counts.edges)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    *out_count =
        written;

    return HENKA_SUCCESS;
}

static bool henka_topology_face_edge_corner(
    const henka_authoring_face* face,
    henka_authoring_edge_id edge_id,
    size_t* out_corner)
{
    size_t corner;
    if (face == NULL || out_corner == NULL)
    {
        return false;
    }
    for (corner = 0U; corner < face->corner_count; ++corner)
    {
        if (face->edges[corner] == edge_id)
        {
            *out_corner = corner;
            return true;
        }
    }
    return false;
}

static bool henka_topology_quad_faces_compatible(
    const henka_authoring_mesh* mesh,
    const henka_authoring_face* first,
    const henka_authoring_face* second,
    const henka_authoring_edge* edge)
{
    if (mesh == NULL || first == NULL || second == NULL || edge == NULL ||
        first->corner_count != 4U || second->corner_count != 4U ||
        first->material_region != second->material_region ||
        first->smooth != second->smooth || edge->hard ||
        henka_topology_edge_is_uv_seam(mesh, edge, 0.00001f))
    {
        return false;
    }
    return true;
}

henka_result henka_authoring_topology_walk_quad_strip(
    const henka_authoring_mesh* mesh,
    henka_authoring_edge_id start_edge_id,
    henka_authoring_quad_strip_step* out_steps,
    size_t capacity,
    size_t* out_count,
    bool* out_closed)
{
    const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
    const henka_authoring_edge* start_edge;
    henka_topology_face_visit_entry* visited = NULL;
    henka_authoring_quad_strip_step* steps = NULL;
    size_t visit_capacity;
    size_t visit_request;
    size_t allocation_bytes;
    size_t step_count = 0U;
    henka_authoring_face_id current_face_id;
    henka_authoring_edge_id entry_edge_id = start_edge_id;
    bool closed = false;
    bool terminated = false;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    if (out_count != NULL) *out_count = 0U;
    if (out_closed != NULL) *out_closed = false;
    if (mesh == NULL || out_count == NULL ||
        (out_steps == NULL && capacity != 0U) ||
        !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    start_edge = henka_authoring_mesh_get_edge(mesh, start_edge_id);
    if (start_edge == NULL || start_edge->face_count == 0U ||
        start_edge->face_count > 2U || start_edge->hard)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    current_face_id = start_edge->faces[0];
    if (start_edge->face_count == 2U && start_edge->faces[1] < current_face_id)
    {
        current_face_id = start_edge->faces[1];
    }
    if (!henka_checked_size_add(desc.max_faces, 1U, &visit_request) ||
        !henka_checked_size_multiply(visit_request, 2U, &visit_request) ||
        (visit_capacity = henka_topology_next_power_of_two(visit_request)) == 0U ||
        !henka_checked_size_multiply(
            visit_capacity, sizeof(*visited), &allocation_bytes))
    {
        return HENKA_ERROR_LIMIT;
    }
    visited = (henka_topology_face_visit_entry*)henka_calloc(1U, allocation_bytes);
    if (visited == NULL ||
        !henka_checked_size_multiply(
            desc.max_faces, sizeof(*steps), &allocation_bytes))
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    steps = (henka_authoring_quad_strip_step*)henka_malloc(allocation_bytes);
    if (steps == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    while (step_count < desc.max_faces)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(mesh, current_face_id);
        const henka_authoring_edge* entry_edge = henka_authoring_mesh_get_edge(mesh, entry_edge_id);
        const henka_authoring_edge* exit_edge;
        henka_authoring_face_id next_face_id = HENKA_AUTHORING_INVALID_ID;
        size_t entry_corner;
        size_t exit_corner;
        size_t face_ordinal;
        if (face == NULL || entry_edge == NULL || face->corner_count != 4U ||
            !henka_topology_face_edge_corner(face, entry_edge_id, &entry_corner) ||
            entry_edge->hard || !henka_topology_face_visit_insert(
                visited, visit_capacity, face->id))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        exit_corner = (entry_corner + 2U) % 4U;
        exit_edge = henka_authoring_mesh_get_edge(mesh, face->edges[exit_corner]);
        if (exit_edge == NULL || exit_edge->face_count == 0U ||
            exit_edge->face_count > 2U || exit_edge->hard)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        if (exit_edge->face_count == 2U)
        {
            for (face_ordinal = 0U; face_ordinal < exit_edge->face_count; ++face_ordinal)
            {
                if (exit_edge->faces[face_ordinal] != face->id)
                {
                    next_face_id = exit_edge->faces[face_ordinal];
                    break;
                }
            }
            if (next_face_id == HENKA_AUTHORING_INVALID_ID ||
                !henka_topology_quad_faces_compatible(
                    mesh, face, henka_authoring_mesh_get_face(mesh, next_face_id), exit_edge))
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            if (exit_edge->id == start_edge_id)
            {
                closed = true;
            }
            else if (henka_topology_face_visit_contains(
                         visited, visit_capacity, next_face_id))
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
        }
        steps[step_count].face_id = face->id;
        steps[step_count].entry_edge_id = entry_edge_id;
        steps[step_count].exit_edge_id = exit_edge->id;
        ++step_count;
        if (exit_edge->face_count == 1U || closed)
        {
            terminated = true;
            break;
        }
        current_face_id = next_face_id;
        entry_edge_id = exit_edge->id;
    }
    if (step_count == 0U || !terminated)
    {
        result = HENKA_ERROR_LIMIT;
        goto cleanup;
    }
    *out_count = step_count;
    if (out_steps != NULL)
    {
        if (capacity < step_count)
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        memcpy(out_steps, steps, step_count * sizeof(*out_steps));
    }
    if (out_closed != NULL) *out_closed = closed;
    result = HENKA_SUCCESS;

cleanup:
    if (result != HENKA_SUCCESS) *out_count = 0U;
    henka_free(steps);
    henka_free(visited);
    return result;
}

henka_result henka_authoring_topology_order_edge_loop(
    const henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    henka_authoring_edge_id* out_edge_ids,
    size_t capacity,
    size_t* out_count,
    bool* out_closed)
{
    henka_authoring_edge_id* sorted_edges = NULL;
    unsigned char* visited = NULL;
    henka_authoring_mesh_desc desc;
    henka_authoring_vertex_id start_vertex = HENKA_AUTHORING_INVALID_ID;
    size_t start_index = SIZE_MAX;
    size_t edge_bytes;
    size_t visited_bytes;
    size_t endpoint_index;
    size_t edge_index;
    size_t degree_one_count = 0U;
    bool closed;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    if (out_count != NULL) *out_count = 0U;
    if (out_closed != NULL) *out_closed = false;
    if (mesh == NULL || edge_ids == NULL || edge_count == 0U ||
        out_edge_ids == NULL || out_count == NULL || out_closed == NULL ||
        !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    desc = henka_authoring_mesh_get_desc(mesh);
    if (edge_count > desc.max_edges || capacity < edge_count ||
        !henka_checked_size_multiply(edge_count, sizeof(*sorted_edges), &edge_bytes) ||
        !henka_checked_size_multiply(edge_count, sizeof(*visited), &visited_bytes))
    {
        return HENKA_ERROR_LIMIT;
    }
    sorted_edges = (henka_authoring_edge_id*)henka_malloc(edge_bytes);
    visited = (unsigned char*)henka_calloc(1U, visited_bytes);
    if (sorted_edges == NULL || visited == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    memcpy(sorted_edges, edge_ids, edge_bytes);
    henka_topology_sort_ids(sorted_edges, edge_count);
    for (edge_index = 0U; edge_index < edge_count; ++edge_index)
    {
        const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
            mesh, sorted_edges[edge_index]);
        if (edge == NULL || edge->vertices[0] == HENKA_AUTHORING_INVALID_ID ||
            edge->vertices[1] == HENKA_AUTHORING_INVALID_ID ||
            edge->vertices[0] == edge->vertices[1] ||
            henka_authoring_mesh_get_vertex(mesh, edge->vertices[0]) == NULL ||
            henka_authoring_mesh_get_vertex(mesh, edge->vertices[1]) == NULL ||
            (edge_index > 0U && sorted_edges[edge_index - 1U] == sorted_edges[edge_index]))
        {
            goto cleanup;
        }
        for (endpoint_index = 0U; endpoint_index < 2U; ++endpoint_index)
        {
            const henka_authoring_vertex_id vertex_id = edge->vertices[endpoint_index];
            size_t degree = 0U;
            size_t candidate_index;
            for (candidate_index = 0U; candidate_index < edge_count; ++candidate_index)
            {
                const henka_authoring_edge* candidate = henka_authoring_mesh_get_edge(
                    mesh, sorted_edges[candidate_index]);
                if (candidate != NULL &&
                    (candidate->vertices[0] == vertex_id || candidate->vertices[1] == vertex_id))
                {
                    if (degree == 2U)
                    {
                        goto cleanup;
                    }
                    ++degree;
                }
            }
            if (degree == 1U)
            {
                ++degree_one_count;
                if (start_vertex == HENKA_AUTHORING_INVALID_ID || vertex_id < start_vertex)
                {
                    start_vertex = vertex_id;
                }
            }
            else if (degree != 2U)
            {
                goto cleanup;
            }
        }
    }
    if (degree_one_count != 0U && degree_one_count != 2U)
    {
        goto cleanup;
    }
    closed = degree_one_count == 0U;
    if (closed)
    {
        const henka_authoring_edge* first_edge = henka_authoring_mesh_get_edge(
            mesh, sorted_edges[0]);
        start_index = 0U;
        start_vertex = first_edge->vertices[0] < first_edge->vertices[1]
            ? first_edge->vertices[0] : first_edge->vertices[1];
    }
    else
    {
        for (edge_index = 0U; edge_index < edge_count; ++edge_index)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                mesh, sorted_edges[edge_index]);
            if (edge->vertices[0] == start_vertex || edge->vertices[1] == start_vertex)
            {
                start_index = edge_index;
                break;
            }
        }
    }
    if (start_index == SIZE_MAX)
    {
        goto cleanup;
    }
    {
        henka_authoring_vertex_id current_vertex = start_vertex;
        size_t current_index = start_index;
        size_t ordered_count = 0U;
        while (ordered_count < edge_count)
        {
            const henka_authoring_edge* edge;
            size_t candidate_index;
            size_t next_index = SIZE_MAX;
            henka_authoring_vertex_id next_vertex;
            if (current_index >= edge_count || visited[current_index] != 0U)
            {
                goto cleanup;
            }
            edge = henka_authoring_mesh_get_edge(mesh, sorted_edges[current_index]);
            if (edge == NULL)
            {
                goto cleanup;
            }
            out_edge_ids[ordered_count++] = edge->id;
            visited[current_index] = 1U;
            next_vertex = edge->vertices[0] == current_vertex
                ? edge->vertices[1] : edge->vertices[0];
            current_vertex = next_vertex;
            if (ordered_count == edge_count)
            {
                break;
            }
            for (candidate_index = 0U; candidate_index < edge_count; ++candidate_index)
            {
                const henka_authoring_edge* candidate;
                if (visited[candidate_index] != 0U)
                {
                    continue;
                }
                candidate = henka_authoring_mesh_get_edge(mesh, sorted_edges[candidate_index]);
                if (candidate != NULL &&
                    (candidate->vertices[0] == current_vertex ||
                     candidate->vertices[1] == current_vertex))
                {
                    next_index = candidate_index;
                    break;
                }
            }
            if (next_index == SIZE_MAX)
            {
                goto cleanup;
            }
            current_index = next_index;
        }
        if ((closed && current_vertex != start_vertex) ||
            (!closed && current_vertex == HENKA_AUTHORING_INVALID_ID))
        {
            goto cleanup;
        }
    }
    *out_count = edge_count;
    *out_closed = closed;
    result = HENKA_SUCCESS;

cleanup:
    if (result != HENKA_SUCCESS)
    {
        *out_count = 0U;
        *out_closed = false;
    }
    henka_free(visited);
    henka_free(sorted_edges);
    return result;
}

henka_result henka_authoring_topology_analyze(
    const henka_authoring_mesh* mesh,
    const henka_authoring_topology_options* options,
    henka_authoring_topology_report* out_report)
{
    henka_authoring_mesh_counts counts;
    henka_authoring_vertex_id* vertex_ids = NULL;
    henka_vec3* positions = NULL;
    henka_topology_vertex_index_entry* vertex_index_by_id = NULL;
    size_t* parent = NULL;
    unsigned char* rank = NULL;
    size_t* next_in_cell = NULL;
    henka_topology_cell_bucket* cell_buckets = NULL;
    henka_authoring_face_id* face_slots = NULL;

    size_t cell_capacity = 0U;
    size_t face_slot_capacity = 0U;
    size_t vertex_index_capacity = 0U;
    size_t allocation_bytes = 0U;
    size_t vertex_found = 0U;
    size_t edge_found = 0U;
    size_t face_found = 0U;
    size_t index;

    henka_authoring_vertex_id vertex_id;
    henka_authoring_edge_id edge_id;
    henka_authoring_face_id face_id;

    henka_result result =
        HENKA_SUCCESS;

    if (out_report != NULL)
    {
        memset(
            out_report,
            0,
            sizeof(*out_report));
    }

    if (mesh == NULL ||
        options == NULL ||
        out_report == NULL ||
        !isfinite(
            options->coincident_vertex_tolerance) ||
        options->coincident_vertex_tolerance <=
            0.0f ||
        !isfinite(
            options->uv_seam_tolerance) ||
        options->uv_seam_tolerance < 0.0f ||
        !isfinite(
            options->degenerate_normal_epsilon) ||
        options->degenerate_normal_epsilon < 0.0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    counts =
        henka_authoring_mesh_get_counts(mesh);

    if (counts.vertices >
            HENKA_AUTHORING_MESH_HARD_MAX_VERTICES ||
        counts.edges >
            HENKA_AUTHORING_MESH_HARD_MAX_EDGES ||
        counts.faces >
            HENKA_AUTHORING_MESH_HARD_MAX_FACES)
    {
        return HENKA_ERROR_LIMIT;
    }

    out_report->vertex_count =
        counts.vertices;

    out_report->edge_count =
        counts.edges;

    out_report->face_count =
        counts.faces;

    if (!henka_checked_size_multiply(
            counts.vertices,
            2U,
            &vertex_index_capacity) ||
        !henka_checked_size_add(
            vertex_index_capacity,
            1U,
            &vertex_index_capacity) ||
        (vertex_index_capacity = henka_topology_next_power_of_two(vertex_index_capacity)) == 0U ||
        !henka_checked_size_multiply(
            vertex_index_capacity,
            sizeof(*vertex_index_by_id),
            &allocation_bytes))
    {
        return HENKA_ERROR_LIMIT;
    }
    vertex_index_by_id =
        (henka_topology_vertex_index_entry*)henka_calloc(1U, allocation_bytes);

    if (vertex_index_by_id == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    if (counts.vertices > 0U)
    {
        size_t vertex_bytes;
        if (!henka_checked_size_multiply(
                counts.vertices,
                sizeof(*vertex_ids),
                &vertex_bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        vertex_ids =
            (henka_authoring_vertex_id*)henka_calloc(1U, vertex_bytes);

        if (!henka_checked_size_multiply(
                counts.vertices,
                sizeof(*positions),
                &vertex_bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        positions =
            (henka_vec3*)henka_calloc(1U, vertex_bytes);

        if (!henka_checked_size_multiply(
                counts.vertices,
                sizeof(*parent),
                &vertex_bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        parent =
            (size_t*)henka_calloc(1U, vertex_bytes);

        if (!henka_checked_size_multiply(
                counts.vertices,
                sizeof(*rank),
                &vertex_bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        rank =
            (unsigned char*)henka_calloc(1U, vertex_bytes);

        if (!henka_checked_size_multiply(
                counts.vertices,
                sizeof(*next_in_cell),
                &vertex_bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        next_in_cell =
            (size_t*)henka_calloc(1U, vertex_bytes);

        if (vertex_ids == NULL ||
            positions == NULL ||
            parent == NULL ||
            rank == NULL ||
            next_in_cell == NULL)
        {
            result =
                HENKA_ERROR_OUT_OF_MEMORY;

            goto cleanup;
        }

        for (index = 0U;
             index < counts.vertices;
             ++index)
        {
            next_in_cell[index] =
                (size_t)-1;
        }
    }

    for (index = 0U;
         index < henka_authoring_mesh_get_desc(mesh).max_vertices;
         ++index)
    {
        if (henka_authoring_mesh_get_vertex_id_at(mesh, index, &vertex_id) != HENKA_SUCCESS)
        {
            continue;
        }
        const henka_authoring_vertex* vertex =
            henka_authoring_mesh_get_vertex(
                mesh,
                vertex_id);

        if (vertex == NULL)
        {
            continue;
        }

        if (vertex_found >=
            counts.vertices)
        {
            result =
                HENKA_ERROR_UNKNOWN;

            goto cleanup;
        }

        vertex_ids[vertex_found] =
            vertex_id;

        positions[vertex_found] =
            vertex->position;

        if (!henka_topology_vertex_index_insert(
                vertex_index_by_id,
                vertex_index_capacity,
                vertex_id,
                vertex_found))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }

        parent[vertex_found] =
            vertex_found;

        vertex_found += 1U;
    }

    if (vertex_found !=
        counts.vertices)
    {
        result =
            HENKA_ERROR_UNKNOWN;

        goto cleanup;
    }

    for (index = 0U; index < henka_authoring_mesh_get_desc(mesh).max_edges; ++index)
    {
        if (henka_authoring_mesh_get_edge_id_at(mesh, index, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        const henka_authoring_edge* edge =
            henka_authoring_mesh_get_edge(
                mesh,
                edge_id);

        if (edge == NULL)
        {
            continue;
        }

        edge_found += 1U;

        if (edge->face_count == 1U)
        {
            out_report->boundary_edge_count +=
                1U;
        }

        if (edge->face_count > 2U)
        {
            out_report->nonmanifold_edge_count +=
                1U;
        }

        if (edge->hard)
        {
            out_report->hard_edge_count +=
                1U;
        }

        {
            const size_t first_index =
                henka_topology_vertex_index_find(
                    vertex_index_by_id,
                    vertex_index_capacity,
                    edge->vertices[0]);

            const size_t second_index =
                henka_topology_vertex_index_find(
                    vertex_index_by_id,
                    vertex_index_capacity,
                    edge->vertices[1]);

            if (first_index != SIZE_MAX && second_index != SIZE_MAX)
            {
                henka_topology_union(
                    parent,
                    rank,
                    first_index,
                    second_index);
            }
        }

        if (edge->face_count == 2U)
        {
            const henka_authoring_face* first_face =
                henka_authoring_mesh_get_face(
                    mesh,
                    edge->faces[0]);

            const henka_authoring_face* second_face =
                henka_authoring_mesh_get_face(
                    mesh,
                    edge->faces[1]);

            const int first_direction =
                henka_topology_face_edge_direction(
                    first_face,
                    edge->vertices[0],
                    edge->vertices[1]);

            const int second_direction =
                henka_topology_face_edge_direction(
                    second_face,
                    edge->vertices[0],
                    edge->vertices[1]);

            if (first_direction != 0 &&
                second_direction != 0 &&
                first_direction ==
                    second_direction)
            {
                out_report->
                    inconsistent_winding_edge_count +=
                    1U;
            }

            if (henka_topology_edge_is_uv_seam(
                    mesh,
                    edge,
                    options->uv_seam_tolerance))
            {
                out_report->uv_seam_edge_count +=
                    1U;
            }
        }
    }

    if (edge_found !=
        counts.edges)
    {
        result =
            HENKA_ERROR_UNKNOWN;

        goto cleanup;
    }

    for (index = 0U;
         index < counts.vertices;
         ++index)
    {
        const size_t valence =
            henka_authoring_mesh_get_vertex_edge_count(
                mesh,
                vertex_ids[index]);

        if (valence == 0U)
        {
            out_report->isolated_vertex_count +=
                1U;
        }

        if (valence <= 2U)
        {
            out_report->
                valence_two_or_less_vertex_count +=
                1U;
        }
        else if (valence == 3U)
        {
            out_report->
                valence_three_vertex_count +=
                1U;
        }
        else if (valence == 4U)
        {
            out_report->
                valence_four_vertex_count +=
                1U;
        }
        else
        {
            out_report->
                valence_five_plus_vertex_count +=
                1U;
        }

        if (valence >
            out_report->max_vertex_valence)
        {
            out_report->max_vertex_valence =
                valence;
        }
    }

    for (index = 0U;
         index < counts.vertices;
         ++index)
    {
        if (henka_topology_find_root(
                parent,
                index) == index)
        {
            out_report->
                connected_component_count +=
                1U;
        }
    }

    if (counts.faces > 0U)
    {
        size_t requested;
        size_t face_bytes;

        if (!henka_checked_size_multiply(counts.faces, 2U, &requested) ||
            !henka_checked_size_add(requested, 1U, &requested))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }

        face_slot_capacity =
            henka_topology_next_power_of_two(
                requested);

        if (face_slot_capacity == 0U)
        {
            result =
                HENKA_ERROR_LIMIT;

            goto cleanup;
        }

        if (!henka_checked_size_multiply(
                face_slot_capacity,
                sizeof(*face_slots),
                &face_bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        face_slots =
            (henka_authoring_face_id*)henka_calloc(1U, face_bytes);

        if (face_slots == NULL)
        {
            result =
                HENKA_ERROR_OUT_OF_MEMORY;

            goto cleanup;
        }

        for (index = 0U;
             index < face_slot_capacity;
             ++index)
        {
            face_slots[index] =
                HENKA_AUTHORING_INVALID_ID;
        }
    }

    for (index = 0U; index < henka_authoring_mesh_get_desc(mesh).max_faces; ++index)
    {
        if (henka_authoring_mesh_get_face_id_at(mesh, index, &face_id) != HENKA_SUCCESS)
        {
            continue;
        }
        const henka_authoring_face* face =
            henka_authoring_mesh_get_face(
                mesh,
                face_id);

        if (face == NULL)
        {
            continue;
        }

        face_found += 1U;

        if (face->corner_count == 3U)
        {
            out_report->triangle_count +=
                1U;
        }
        else if (face->corner_count == 4U)
        {
            out_report->quad_count +=
                1U;
        }
        else if (face->corner_count > 4U)
        {
            out_report->ngon_count +=
                1U;
        }

        if (face->corner_count >
            out_report->max_face_corners)
        {
            out_report->max_face_corners =
                face->corner_count;
        }

        if (henka_topology_face_is_degenerate(
                mesh,
                face,
                options->
                    degenerate_normal_epsilon))
        {
            out_report->degenerate_face_count +=
                1U;
        }

        if (face_slot_capacity > 0U)
        {
            const uint64_t hash =
                henka_topology_face_hash(face);

            size_t slot =
                (size_t)(
                    hash &
                    (uint64_t)(
                        face_slot_capacity - 1U));

            size_t probe;
            bool inserted = false;

            for (probe = 0U;
                 probe < face_slot_capacity;
                 ++probe)
            {
                if (face_slots[slot] ==
                    HENKA_AUTHORING_INVALID_ID)
                {
                    face_slots[slot] =
                        face_id;

                    inserted =
                        true;

                    break;
                }

                if (henka_topology_faces_same_vertex_set(
                        face,
                        henka_authoring_mesh_get_face(
                            mesh,
                            face_slots[slot])))
                {
                    out_report->
                        duplicate_face_count +=
                        1U;

                    inserted =
                        true;

                    break;
                }

                slot =
                    (slot + 1U) &
                    (face_slot_capacity - 1U);
            }

            if (!inserted)
            {
                result =
                    HENKA_ERROR_LIMIT;

                goto cleanup;
            }
        }
    }

    if (face_found !=
        counts.faces)
    {
        result =
            HENKA_ERROR_UNKNOWN;

        goto cleanup;
    }

    if (counts.faces > 0U)
    {
        out_report->quad_face_ratio =
            (double)out_report->quad_count /
            (double)counts.faces;
    }

    if (counts.vertices > 0U)
    {
        size_t requested;
        size_t cell_bytes;

        if (!henka_checked_size_multiply(counts.vertices, 2U, &requested) ||
            !henka_checked_size_add(requested, 1U, &requested))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }

        cell_capacity =
            henka_topology_next_power_of_two(
                requested);

        if (cell_capacity == 0U)
        {
            result =
                HENKA_ERROR_LIMIT;

            goto cleanup;
        }

        if (!henka_checked_size_multiply(
                cell_capacity,
                sizeof(*cell_buckets),
                &cell_bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        cell_buckets =
            (henka_topology_cell_bucket*)henka_calloc(1U, cell_bytes);

        if (cell_buckets == NULL)
        {
            result =
                HENKA_ERROR_OUT_OF_MEMORY;

            goto cleanup;
        }

        for (index = 0U;
             index < counts.vertices;
             ++index)
        {
            const int64_t base_x =
                henka_topology_cell_coordinate(
                    positions[index].x,
                    options->
                        coincident_vertex_tolerance);

            const int64_t base_y =
                henka_topology_cell_coordinate(
                    positions[index].y,
                    options->
                        coincident_vertex_tolerance);

            const int64_t base_z =
                henka_topology_cell_coordinate(
                    positions[index].z,
                    options->
                        coincident_vertex_tolerance);

            int dx;
            int dy;
            int dz;

            for (dx = -1;
                 dx <= 1;
                 ++dx)
            {
                int64_t query_x;

                if (!henka_topology_offset_cell(
                        base_x,
                        dx,
                        &query_x))
                {
                    continue;
                }

                for (dy = -1;
                     dy <= 1;
                     ++dy)
                {
                    int64_t query_y;

                    if (!henka_topology_offset_cell(
                            base_y,
                            dy,
                            &query_y))
                    {
                        continue;
                    }

                    for (dz = -1;
                         dz <= 1;
                         ++dz)
                    {
                        int64_t query_z;
                        bool found;
                        size_t slot;

                        if (!henka_topology_offset_cell(
                                base_z,
                                dz,
                                &query_z))
                        {
                            continue;
                        }

                        slot =
                            henka_topology_find_cell_slot(
                                cell_buckets,
                                cell_capacity,
                                query_x,
                                query_y,
                                query_z,
                                &found);

                        if (found)
                        {
                            size_t prior =
                                cell_buckets[slot].head;

                            while (prior !=
                                (size_t)-1)
                            {
                                if (henka_topology_positions_coincident(
                                        positions[index],
                                        positions[prior],
                                        options->
                                            coincident_vertex_tolerance))
                                {
                                    out_report->
                                        coincident_vertex_pair_count +=
                                        1U;
                                }

                                prior =
                                    next_in_cell[prior];
                            }
                        }
                    }
                }
            }

            {
                bool found;
                size_t slot =
                    henka_topology_find_cell_slot(
                        cell_buckets,
                        cell_capacity,
                        base_x,
                        base_y,
                        base_z,
                        &found);

                if (slot >=
                    cell_capacity)
                {
                    result =
                        HENKA_ERROR_LIMIT;

                    goto cleanup;
                }

                if (!found)
                {
                    cell_buckets[slot].occupied =
                        true;

                    cell_buckets[slot].x =
                        base_x;

                    cell_buckets[slot].y =
                        base_y;

                    cell_buckets[slot].z =
                        base_z;

                    cell_buckets[slot].head =
                        (size_t)-1;
                }

                next_in_cell[index] =
                    cell_buckets[slot].head;

                cell_buckets[slot].head =
                    index;
            }
        }
    }

cleanup:
    henka_free(face_slots);
    henka_free(cell_buckets);
    henka_free(next_in_cell);
    henka_free(rank);
    henka_free(parent);
    henka_free(vertex_index_by_id);
    henka_free(positions);
    henka_free(vertex_ids);

    if (result != HENKA_SUCCESS)
    {
        memset(
            out_report,
            0,
            sizeof(*out_report));
    }

    return result;
}

henka_authoring_topology_repair_options
henka_authoring_topology_repair_options_default(void)
{
    henka_authoring_topology_repair_options options;

    options.policy_version = HENKA_AUTHORING_TOPOLOGY_POLICY_VERSION;
    options.profile = HENKA_AUTHORING_TOPOLOGY_PROFILE_GENERAL;
    options.analysis = henka_authoring_topology_options_default();
    options.max_passes = 4U;
    options.remove_isolated_vertices = false;
    options.remove_duplicate_faces = false;
    options.remove_degenerate_faces = false;
    return options;
}

static bool henka_topology_faces_same_cycle(
    const henka_authoring_face* first,
    const henka_authoring_face* second,
    float uv_tolerance)
{
    size_t offset;
    size_t corner;
    if (first == NULL || second == NULL ||
        first->corner_count != second->corner_count ||
        first->material_region != second->material_region ||
        first->smooth != second->smooth ||
        first->corner_count == 0U || !isfinite(uv_tolerance) ||
        uv_tolerance < 0.0f)
    {
        return false;
    }
    for (offset = 0U; offset < second->corner_count; ++offset)
    {
        bool matches = true;
        for (corner = 0U; corner < first->corner_count; ++corner)
        {
            const size_t second_corner =
                (corner + offset) % second->corner_count;
            if (first->vertices[corner] != second->vertices[second_corner] ||
                fabsf(first->uvs[corner].x - second->uvs[second_corner].x) > uv_tolerance ||
                fabsf(first->uvs[corner].y - second->uvs[second_corner].y) > uv_tolerance)
            {
                matches = false;
                break;
            }
        }
        if (matches)
        {
            return true;
        }
    }
    return false;
}

static henka_result henka_topology_mark_repair_faces(
    const henka_authoring_mesh* mesh,
    const henka_authoring_topology_repair_options* options,
    unsigned char* remove_faces,
    size_t remove_face_capacity,
    size_t* out_duplicate_count,
    size_t* out_degenerate_count)
{
    const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
    size_t face_slot;
    if (mesh == NULL || options == NULL || remove_faces == NULL ||
        remove_face_capacity < desc.max_faces || out_duplicate_count == NULL ||
        out_degenerate_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_duplicate_count = 0U;
    *out_degenerate_count = 0U;
    memset(remove_faces, 0, remove_face_capacity);
    for (face_slot = 0U; face_slot < desc.max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            mesh,
            henka_authoring_mesh_get_face_id_at(mesh, face_slot, &face_id) == HENKA_SUCCESS
                ? face_id
                : HENKA_AUTHORING_INVALID_ID);
        if (face == NULL)
        {
            continue;
        }
        if (options->remove_degenerate_faces &&
            henka_topology_face_is_degenerate(
                mesh, face, options->analysis.degenerate_normal_epsilon))
        {
            remove_faces[face_slot] = 1U;
            *out_degenerate_count += 1U;
            continue;
        }
        if (!options->remove_duplicate_faces)
        {
            continue;
        }
        for (size_t prior_slot = 0U; prior_slot < face_slot; ++prior_slot)
        {
            henka_authoring_face_id prior_id;
            const henka_authoring_face* prior = henka_authoring_mesh_get_face_id_at(
                mesh, prior_slot, &prior_id) == HENKA_SUCCESS
                ? henka_authoring_mesh_get_face(mesh, prior_id)
                : NULL;
            if (prior == NULL || !henka_topology_faces_same_vertex_set(face, prior))
            {
                continue;
            }
            if (!henka_topology_faces_same_cycle(
                    prior, face, options->analysis.uv_seam_tolerance))
            {
                /* Same vertex set with different winding, material, smoothing,
                 * or UV data is not a safe duplicate to discard. */
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            remove_faces[face_slot] = 1U;
            *out_duplicate_count += 1U;
            break;
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_repair_topology(
    henka_authoring_mesh* mesh,
    const henka_authoring_topology_repair_options* options,
    henka_authoring_topology_repair_report* out_report)
{
    const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_topology_report before;
    henka_authoring_topology_report after;
    unsigned char* remove_faces = NULL;
    unsigned char* remove_vertices = NULL;
    size_t face_capacity;
    size_t vertex_capacity;
    size_t face_bytes;
    size_t vertex_bytes;
    size_t pass;
    henka_result result;

    if (out_report != NULL)
    {
        memset(out_report, 0, sizeof(*out_report));
    }
    if (mesh == NULL || options == NULL || out_report == NULL ||
        options->policy_version != HENKA_AUTHORING_TOPOLOGY_POLICY_VERSION ||
        options->profile < HENKA_AUTHORING_TOPOLOGY_PROFILE_GENERAL ||
        options->profile >= HENKA_AUTHORING_TOPOLOGY_PROFILE_COUNT ||
        options->max_passes == 0U || !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_topology_analyze(
        mesh, &options->analysis, &before);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    out_report->before = before;
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (!henka_checked_size_add(desc.max_faces, 1U, &face_capacity) ||
        !henka_checked_size_add(desc.max_vertices, 1U, &vertex_capacity) ||
        !henka_checked_size_multiply(face_capacity, sizeof(*remove_faces), &face_bytes) ||
        !henka_checked_size_multiply(vertex_capacity, sizeof(*remove_vertices), &vertex_bytes))
    {
        result = HENKA_ERROR_LIMIT;
        goto repair_cleanup;
    }
    remove_faces = (unsigned char*)henka_calloc(1U, face_bytes);
    remove_vertices = (unsigned char*)henka_calloc(1U, vertex_bytes);
    if (remove_faces == NULL || remove_vertices == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto repair_cleanup;
    }

    for (pass = 0U; pass < options->max_passes; ++pass)
    {
        size_t duplicate_count = 0U;
        size_t degenerate_count = 0U;
        size_t face_slot;
        size_t vertex_slot;
        bool changed = false;

        out_report->passes = pass + 1U;
        result = henka_topology_mark_repair_faces(
            candidate,
            options,
            remove_faces,
            face_capacity,
            &duplicate_count,
            &degenerate_count);
        if (result != HENKA_SUCCESS)
        {
            goto repair_cleanup;
        }
        for (face_slot = 0U; face_slot < desc.max_faces; ++face_slot)
        {
            henka_authoring_face_id face_id;
            if (remove_faces[face_slot] != 0U &&
                henka_authoring_mesh_get_face_id_at(candidate, face_slot, &face_id) == HENKA_SUCCESS)
            {
                result = henka_authoring_mesh_remove_face(
                    candidate, face_id);
                if (result != HENKA_SUCCESS)
                {
                    goto repair_cleanup;
                }
                changed = true;
            }
        }
        if (options->remove_isolated_vertices)
        {
            memset(remove_vertices, 0, vertex_bytes);
            for (vertex_slot = 0U; vertex_slot < desc.max_vertices; ++vertex_slot)
            {
                henka_authoring_vertex_id vertex_id;
                if (henka_authoring_mesh_get_vertex_id_at(candidate, vertex_slot, &vertex_id) != HENKA_SUCCESS)
                {
                    continue;
                }
                if (henka_authoring_mesh_get_vertex_edge_count(
                        candidate, vertex_id) == 0U &&
                    henka_authoring_mesh_get_vertex(
                        candidate, vertex_id) != NULL)
                {
                    remove_vertices[vertex_slot] = 1U;
                }
            }
            for (vertex_slot = 0U; vertex_slot < desc.max_vertices; ++vertex_slot)
            {
                henka_authoring_vertex_id vertex_id;
                if (remove_vertices[vertex_slot] != 0U &&
                    henka_authoring_mesh_get_vertex_id_at(candidate, vertex_slot, &vertex_id) == HENKA_SUCCESS)
                {
                    result = henka_authoring_mesh_remove_vertex(
                        candidate, vertex_id);
                    if (result != HENKA_SUCCESS)
                    {
                        goto repair_cleanup;
                    }
                    out_report->removed_isolated_vertices += 1U;
                    changed = true;
                }
            }
        }
        out_report->removed_duplicate_faces += duplicate_count;
        out_report->removed_degenerate_faces += degenerate_count;
        if (!changed)
        {
            break;
        }
        if (pass + 1U == options->max_passes)
        {
            result = HENKA_ERROR_LIMIT;
            goto repair_cleanup;
        }
    }

    if (!henka_authoring_mesh_validate(candidate))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto repair_cleanup;
    }
    result = henka_authoring_topology_analyze(
        candidate, &options->analysis, &after);
    if (result != HENKA_SUCCESS || after.nonmanifold_edge_count != 0U)
    {
        result = result == HENKA_SUCCESS ? HENKA_ERROR_INVALID_ARGUMENT : result;
        goto repair_cleanup;
    }
    result = henka_authoring_mesh_copy(mesh, candidate);
    if (result == HENKA_SUCCESS)
    {
        out_report->after = after;
        out_report->changed = out_report->before.vertex_count != after.vertex_count ||
            out_report->before.edge_count != after.edge_count ||
            out_report->before.face_count != after.face_count;
    }

repair_cleanup:
    henka_free(remove_faces);
    henka_free(remove_vertices);
    henka_authoring_mesh_destroy(candidate);
    if (result != HENKA_SUCCESS)
    {
        memset(out_report, 0, sizeof(*out_report));
    }
    return result;
}
