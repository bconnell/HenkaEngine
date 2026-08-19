#include <henka/authoring_topology.h>

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct henka_topology_cell_bucket
{
    int64_t x;
    int64_t y;
    int64_t z;
    size_t head;
    bool occupied;
} henka_topology_cell_bucket;

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

    for (edge_id = 1U;
         edge_id <=
            HENKA_AUTHORING_MESH_HARD_MAX_EDGES;
         ++edge_id)
    {
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

henka_result henka_authoring_topology_analyze(
    const henka_authoring_mesh* mesh,
    const henka_authoring_topology_options* options,
    henka_authoring_topology_report* out_report)
{
    henka_authoring_mesh_counts counts;
    henka_authoring_vertex_id* vertex_ids = NULL;
    henka_vec3* positions = NULL;
    size_t* vertex_index_by_id = NULL;
    size_t* parent = NULL;
    unsigned char* rank = NULL;
    size_t* next_in_cell = NULL;
    henka_topology_cell_bucket* cell_buckets = NULL;
    henka_authoring_face_id* face_slots = NULL;

    size_t cell_capacity = 0U;
    size_t face_slot_capacity = 0U;
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

    vertex_index_by_id =
        (size_t*)malloc(
            (HENKA_AUTHORING_MESH_HARD_MAX_VERTICES + 1U) *
            sizeof(*vertex_index_by_id));

    if (vertex_index_by_id == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    for (index = 0U;
         index <=
            HENKA_AUTHORING_MESH_HARD_MAX_VERTICES;
         ++index)
    {
        vertex_index_by_id[index] =
            (size_t)-1;
    }

    if (counts.vertices > 0U)
    {
        vertex_ids =
            (henka_authoring_vertex_id*)calloc(
                counts.vertices,
                sizeof(*vertex_ids));

        positions =
            (henka_vec3*)calloc(
                counts.vertices,
                sizeof(*positions));

        parent =
            (size_t*)calloc(
                counts.vertices,
                sizeof(*parent));

        rank =
            (unsigned char*)calloc(
                counts.vertices,
                sizeof(*rank));

        next_in_cell =
            (size_t*)malloc(
                counts.vertices *
                sizeof(*next_in_cell));

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

    for (vertex_id = 1U;
         vertex_id <=
            HENKA_AUTHORING_MESH_HARD_MAX_VERTICES;
         ++vertex_id)
    {
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

        vertex_index_by_id[vertex_id] =
            vertex_found;

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

    for (edge_id = 1U;
         edge_id <=
            HENKA_AUTHORING_MESH_HARD_MAX_EDGES;
         ++edge_id)
    {
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

        if (edge->vertices[0] <=
                HENKA_AUTHORING_MESH_HARD_MAX_VERTICES &&
            edge->vertices[1] <=
                HENKA_AUTHORING_MESH_HARD_MAX_VERTICES)
        {
            const size_t first_index =
                vertex_index_by_id[
                    edge->vertices[0]];

            const size_t second_index =
                vertex_index_by_id[
                    edge->vertices[1]];

            if (first_index != (size_t)-1 &&
                second_index != (size_t)-1)
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
        const size_t requested =
            counts.faces * 2U + 1U;

        face_slot_capacity =
            henka_topology_next_power_of_two(
                requested);

        if (face_slot_capacity == 0U)
        {
            result =
                HENKA_ERROR_LIMIT;

            goto cleanup;
        }

        face_slots =
            (henka_authoring_face_id*)malloc(
                face_slot_capacity *
                sizeof(*face_slots));

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

    for (face_id = 1U;
         face_id <=
            HENKA_AUTHORING_MESH_HARD_MAX_FACES;
         ++face_id)
    {
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
        const size_t requested =
            counts.vertices * 2U + 1U;

        cell_capacity =
            henka_topology_next_power_of_two(
                requested);

        if (cell_capacity == 0U)
        {
            result =
                HENKA_ERROR_LIMIT;

            goto cleanup;
        }

        cell_buckets =
            (henka_topology_cell_bucket*)calloc(
                cell_capacity,
                sizeof(*cell_buckets));

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
    free(face_slots);
    free(cell_buckets);
    free(next_in_cell);
    free(rank);
    free(parent);
    free(vertex_index_by_id);
    free(positions);
    free(vertex_ids);

    if (result != HENKA_SUCCESS)
    {
        memset(
            out_report,
            0,
            sizeof(*out_report));
    }

    return result;
}