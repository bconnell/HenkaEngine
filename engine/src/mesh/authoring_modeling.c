#include <henka/authoring_modeling.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <henka/memory.h>

#include "../core/checked.h"
#include "authoring_mesh_internal.h"

static bool modeling_finite_vec3(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool modeling_finite_scalar(float value)
{
    return isfinite(value);
}

static void modeling_report_reset(henka_authoring_modeling_report* report)
{
    if (report == NULL) return;
    memset(report, 0, sizeof(*report));
    report->primary_vertex_id = HENKA_AUTHORING_INVALID_ID;
    report->primary_edge_id = HENKA_AUTHORING_INVALID_ID;
    report->primary_face_id = HENKA_AUTHORING_INVALID_ID;
}

static int modeling_vertex_id_compare(const void* left_pointer, const void* right_pointer)
{
    const henka_authoring_vertex_id left = *(const henka_authoring_vertex_id*)left_pointer;
    const henka_authoring_vertex_id right = *(const henka_authoring_vertex_id*)right_pointer;
    return left < right ? -1 : left > right ? 1 : 0;
}

static size_t modeling_active_vertex_count(const henka_authoring_mesh* mesh)
{
    const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
    size_t count = 0U;
    size_t id;
    for (id = 1U; id <= desc.max_vertices; ++id)
    {
        count += henka_authoring_mesh_get_vertex(mesh, (henka_authoring_vertex_id)id) != NULL ? 1U : 0U;
    }
    return count;
}

static bool modeling_face_geometry_is_valid(const henka_authoring_mesh* mesh)
{
    const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
    size_t face_id;
    for (face_id = 1U; face_id <= desc.max_faces; ++face_id)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            mesh, (henka_authoring_face_id)face_id);
        henka_vec3 normal = {0.0f, 0.0f, 0.0f};
        size_t corner;
        if (face == NULL) continue;
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const henka_authoring_vertex* first = henka_authoring_mesh_get_vertex(
                mesh, face->vertices[corner]);
            const henka_authoring_vertex* second = henka_authoring_mesh_get_vertex(
                mesh, face->vertices[(corner + 1U) % face->corner_count]);
            if (first == NULL || second == NULL ||
                !modeling_finite_vec3(first->position) || !modeling_finite_vec3(second->position) ||
                henka_vec3_length(henka_vec3_subtract(second->position, first->position)) <= 1.0e-7f)
            {
                return false;
            }
            normal.x += (first->position.y - second->position.y) *
                (first->position.z + second->position.z);
            normal.y += (first->position.z - second->position.z) *
                (first->position.x + second->position.x);
            normal.z += (first->position.x - second->position.x) *
                (first->position.y + second->position.y);
        }
        if (henka_vec3_length(normal) <= 1.0e-7f) return false;
    }
    return modeling_active_vertex_count(mesh) > 0U;
}

static henka_authoring_vertex_id modeling_mapped_vertex(
    const henka_authoring_vertex_id* selected,
    const henka_authoring_vertex_id* representatives,
    size_t count,
    henka_authoring_vertex_id id)
{
    size_t index;
    for (index = 0U; index < count; ++index)
    {
        if (selected[index] == id) return representatives[index];
    }
    return id;
}

static henka_result modeling_find_edge_for_pair(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id first,
    henka_authoring_vertex_id second,
    henka_authoring_edge_id* out_edge_id)
{
    const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
    size_t edge_id;
    const henka_authoring_vertex_id low = first < second ? first : second;
    const henka_authoring_vertex_id high = first < second ? second : first;
    if (out_edge_id == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    *out_edge_id = HENKA_AUTHORING_INVALID_ID;
    for (edge_id = 1U; edge_id <= desc.max_edges; ++edge_id)
    {
        const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
            mesh, (henka_authoring_edge_id)edge_id);
        if (edge != NULL && edge->vertices[0] == low && edge->vertices[1] == high)
        {
            *out_edge_id = edge->id;
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

static henka_result modeling_build_merge_updates(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* selected,
    const henka_authoring_vertex_id* representatives,
    size_t selected_count,
    henka_authoring_face_loop_update** out_updates,
    size_t* out_update_count)
{
    const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
    henka_authoring_face_loop_update* updates;
    size_t update_count = 0U;
    size_t face_id;
    if (out_updates == NULL || out_update_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_updates = NULL;
    *out_update_count = 0U;
    updates = henka_calloc(desc.max_faces, sizeof(*updates));
    if (updates == NULL) return HENKA_ERROR_OUT_OF_MEMORY;
    for (face_id = 1U; face_id <= desc.max_faces; ++face_id)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            mesh, (henka_authoring_face_id)face_id);
        henka_authoring_vertex_id mapped[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
        henka_vec2 mapped_uvs[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
        size_t mapped_count = 0U;
        size_t corner;
        if (face == NULL) continue;
        if (face->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
        {
            henka_free(updates);
            return HENKA_ERROR_LIMIT;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const henka_authoring_vertex_id mapped_id = modeling_mapped_vertex(
                selected, representatives, selected_count, face->vertices[corner]);
            if (mapped_count == 0U || mapped[mapped_count - 1U] != mapped_id)
            {
                mapped[mapped_count++] = mapped_id;
            }
        }
        if (mapped_count > 1U && mapped[0] == mapped[mapped_count - 1U]) --mapped_count;
        updates[update_count].face_id = (henka_authoring_face_id)face_id;
        updates[update_count].material_region = face->material_region;
        updates[update_count].smooth = face->smooth;
        if (mapped_count < 3U)
        {
            updates[update_count].remove = true;
            ++update_count;
            continue;
        }
        updates[update_count].vertices = henka_malloc(mapped_count * sizeof(*updates[update_count].vertices));
        updates[update_count].uvs = henka_malloc(mapped_count * sizeof(*updates[update_count].uvs));
        if (updates[update_count].vertices == NULL || updates[update_count].uvs == NULL)
        {
            henka_free((void*)(uintptr_t)updates[update_count].vertices);
            henka_free((void*)(uintptr_t)updates[update_count].uvs);
            henka_free(updates);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        memcpy((void*)(uintptr_t)updates[update_count].vertices, mapped, mapped_count * sizeof(mapped[0]));
        updates[update_count].corner_count = mapped_count;
        for (corner = 0U; corner < mapped_count; ++corner)
        {
            size_t source_corner;
            size_t chosen = SIZE_MAX;
            henka_authoring_vertex_id lowest_source = HENKA_AUTHORING_INVALID_ID;
            for (source_corner = 0U; source_corner < face->corner_count; ++source_corner)
            {
                const henka_authoring_vertex_id source_id = face->vertices[source_corner];
                const henka_authoring_vertex_id source_mapped = modeling_mapped_vertex(
                    selected, representatives, selected_count, source_id);
                if (source_mapped != mapped[corner]) continue;
                if (source_id == mapped[corner])
                {
                    chosen = source_corner;
                    break;
                }
                if (lowest_source == HENKA_AUTHORING_INVALID_ID || source_id < lowest_source)
                {
                    lowest_source = source_id;
                    chosen = source_corner;
                }
            }
            if (chosen == SIZE_MAX)
            {
                henka_free((void*)(uintptr_t)updates[update_count].vertices);
                henka_free((void*)(uintptr_t)updates[update_count].uvs);
                henka_free(updates);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            mapped_uvs[corner] = face->uvs[chosen];
        }
        memcpy((void*)(uintptr_t)updates[update_count].uvs, mapped_uvs, mapped_count * sizeof(mapped_uvs[0]));
        ++update_count;
    }
    *out_updates = updates;
    *out_update_count = update_count;
    return HENKA_SUCCESS;
}

static void modeling_destroy_merge_updates(
    henka_authoring_face_loop_update* updates,
    size_t update_count)
{
    size_t index;
    if (updates == NULL) return;
    for (index = 0U; index < update_count; ++index)
    {
        henka_free((void*)(uintptr_t)updates[index].vertices);
        henka_free((void*)(uintptr_t)updates[index].uvs);
    }
    henka_free(updates);
}

static henka_result modeling_apply_merge(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* selected,
    const henka_authoring_vertex_id* mappings,
    size_t selected_count,
    const henka_authoring_vertex_id* survivors,
    const henka_vec3* representative_position,
    size_t representative_count,
    henka_authoring_vertex_id* out_survivors,
    size_t survivor_capacity,
    size_t* out_survivor_count,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_face_loop_update* updates = NULL;
    size_t update_count = 0U;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_result result;
    size_t index;

    if (mesh == NULL || selected == NULL || mappings == NULL || survivors == NULL || selected_count < 2U ||
        representative_position == NULL || representative_count == 0U || out_survivors == NULL ||
        out_survivor_count == NULL ||
        survivor_capacity < representative_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_survivor_count = 0U;
    modeling_report_reset(out_report);
    for (index = 0U; index < representative_count; ++index)
    {
        if (out_survivors != NULL) out_survivors[index] = survivors[index];
    }
    before = henka_authoring_mesh_get_counts(mesh);
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result != HENKA_SUCCESS) return result;
    for (index = 0U; index < representative_count; ++index)
    {
        result = henka_authoring_mesh_set_vertex_position(
            candidate, survivors[index], representative_position[index]);
        if (result != HENKA_SUCCESS) goto cleanup;
    }
    result = modeling_build_merge_updates(
        candidate, selected, mappings, selected_count, &updates, &update_count);
    if (result != HENKA_SUCCESS) goto cleanup;
    result = henka_authoring_mesh_apply_face_loop_updates_internal(candidate, updates, update_count);
    if (result != HENKA_SUCCESS) goto cleanup;

    /* Hardness is an OR over all old edges that map to the same surviving
     * endpoint pair. The reconciliation seam preserves exact endpoint-pair
     * IDs; this pass covers newly created pairs caused by a collapse. */
    {
        const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
        size_t edge_id;
        for (edge_id = 1U; edge_id <= desc.max_edges; ++edge_id)
        {
            const henka_authoring_edge* old_edge = henka_authoring_mesh_get_edge(
                mesh, (henka_authoring_edge_id)edge_id);
            henka_authoring_vertex_id first;
            henka_authoring_vertex_id second;
            henka_authoring_edge_id new_edge_id;
            if (old_edge == NULL || !old_edge->hard) continue;
            first = modeling_mapped_vertex(selected, mappings, selected_count, old_edge->vertices[0]);
            second = modeling_mapped_vertex(selected, mappings, selected_count, old_edge->vertices[1]);
            if (first == second) continue;
            if (modeling_find_edge_for_pair(candidate, first, second, &new_edge_id) == HENKA_SUCCESS)
            {
                result = henka_authoring_mesh_set_edge_hard(candidate, new_edge_id, true);
                if (result != HENKA_SUCCESS) goto cleanup;
            }
        }
    }
    for (index = 0U; index < selected_count; ++index)
    {
        bool is_representative = false;
        size_t representative_index;
        for (representative_index = 0U; representative_index < representative_count; ++representative_index)
        {
            if (selected[index] == survivors[representative_index])
            {
                is_representative = true;
                break;
            }
        }
        if (!is_representative)
        {
            /* Each non-representative selected ID is removed exactly once. */
            result = henka_authoring_mesh_remove_vertex(candidate, selected[index]);
            if (result != HENKA_SUCCESS) goto cleanup;
        }
    }
    if (!henka_authoring_mesh_validate(candidate) ||
        henka_authoring_mesh_get_counts(candidate).faces == 0U ||
        !modeling_face_geometry_is_valid(candidate))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(candidate);
    result = henka_authoring_mesh_copy(mesh, candidate);
    if (result == HENKA_SUCCESS)
    {
        *out_survivor_count = representative_count;
        if (out_report != NULL)
        {
            out_report->changed = true;
            out_report->removed_vertices = before.vertices > after.vertices ? before.vertices - after.vertices : 0U;
            out_report->created_vertices = after.vertices > before.vertices ? after.vertices - before.vertices : 0U;
            out_report->removed_edges = before.edges > after.edges ? before.edges - after.edges : 0U;
            out_report->created_edges = after.edges > before.edges ? after.edges - before.edges : 0U;
            out_report->removed_faces = before.faces > after.faces ? before.faces - after.faces : 0U;
            out_report->created_faces = after.faces > before.faces ? after.faces - before.faces : 0U;
            out_report->primary_vertex_id = survivors[0];
        }
    }

cleanup:
    modeling_destroy_merge_updates(updates, update_count);
    henka_authoring_mesh_destroy(candidate);
    return result;
}

henka_result henka_authoring_mesh_merge_vertices(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    henka_authoring_vertex_merge_mode mode,
    henka_authoring_vertex_id active_vertex_id,
    henka_authoring_vertex_id* out_surviving_vertices,
    size_t survivor_capacity,
    size_t* out_survivor_count,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_vertex_id* selected = NULL;
    henka_authoring_vertex_id representatives[1];
    henka_vec3 representative_position[1];
    size_t index;
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_z = 0.0;
    henka_result result;
    if (out_survivor_count != NULL) *out_survivor_count = 0U;
    modeling_report_reset(out_report);
    if (mesh == NULL || vertex_ids == NULL || vertex_count < 2U || mode > HENKA_AUTHORING_VERTEX_MERGE_ACTIVE ||
        out_surviving_vertices == NULL || out_survivor_count == NULL || survivor_capacity < 1U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    selected = henka_malloc(vertex_count * sizeof(*selected));
    if (selected == NULL) return HENKA_ERROR_OUT_OF_MEMORY;
    memcpy(selected, vertex_ids, vertex_count * sizeof(*selected));
    qsort(selected, vertex_count, sizeof(*selected), modeling_vertex_id_compare);
    for (index = 0U; index < vertex_count; ++index)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(mesh, selected[index]);
        if (vertex == NULL || (index > 0U && selected[index - 1U] == selected[index]))
        {
            henka_free(selected);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        sum_x += (double)vertex->position.x;
        sum_y += (double)vertex->position.y;
        sum_z += (double)vertex->position.z;
    }
    representatives[0] = mode == HENKA_AUTHORING_VERTEX_MERGE_ACTIVE
        ? active_vertex_id : selected[0];
    if (mode == HENKA_AUTHORING_VERTEX_MERGE_ACTIVE)
    {
        bool active_selected = false;
        for (index = 0U; index < vertex_count; ++index)
            active_selected = active_selected || selected[index] == active_vertex_id;
        if (!active_selected)
        {
            henka_free(selected);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        representative_position[0] = henka_authoring_mesh_get_vertex(mesh, active_vertex_id)->position;
    }
    else
    {
        representative_position[0] = (henka_vec3){
            (float)(sum_x / (double)vertex_count),
            (float)(sum_y / (double)vertex_count),
            (float)(sum_z / (double)vertex_count)};
        if (!modeling_finite_vec3(representative_position[0]))
        {
            henka_free(selected);
            return HENKA_ERROR_NUMERIC_RANGE;
        }
    }
    {
        henka_authoring_vertex_id* maps = henka_malloc(vertex_count * sizeof(*maps));
        if (maps == NULL)
        {
            henka_free(selected);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        for (index = 0U; index < vertex_count; ++index) maps[index] = representatives[0];
        result = modeling_apply_merge(mesh, selected, maps, vertex_count,
            representatives, representative_position, 1U, out_surviving_vertices, survivor_capacity,
            out_survivor_count, out_report);
        henka_free(maps);
    }
    henka_free(selected);
    return result;
}

typedef struct modeling_spatial_cell
{
    int64_t x;
    int64_t y;
    int64_t z;
    size_t head;
    bool used;
} modeling_spatial_cell;

static size_t modeling_spatial_hash(int64_t x, int64_t y, int64_t z, size_t capacity)
{
    uint64_t value = (uint64_t)x * UINT64_C(73856093) ^
        (uint64_t)y * UINT64_C(19349663) ^ (uint64_t)z * UINT64_C(83492791);
    return (size_t)(value % (uint64_t)capacity);
}

static bool modeling_spatial_cell_lookup(
    modeling_spatial_cell* table,
    size_t capacity,
    int64_t x,
    int64_t y,
    int64_t z,
    size_t* out_slot)
{
    size_t probe;
    size_t slot = modeling_spatial_hash(x, y, z, capacity);
    for (probe = 0U; probe < capacity; ++probe)
    {
        modeling_spatial_cell* cell = &table[slot];
        if (!cell->used)
        {
            if (out_slot != NULL) *out_slot = slot;
            return false;
        }
        if (cell->x == x && cell->y == y && cell->z == z)
        {
            if (out_slot != NULL) *out_slot = slot;
            return true;
        }
        slot = (slot + 1U) % capacity;
    }
    return false;
}

static bool modeling_quantize_cell(float value, float tolerance, int64_t* out_cell)
{
    const double scaled = floor((double)value / (double)tolerance);
    if (out_cell == NULL || !isfinite(scaled) || scaled < (double)INT64_MIN || scaled > (double)INT64_MAX)
    {
        return false;
    }
    *out_cell = (int64_t)scaled;
    return true;
}

henka_result henka_authoring_mesh_merge_vertices_by_distance(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    float tolerance,
    henka_authoring_vertex_id* out_surviving_vertices,
    size_t survivor_capacity,
    size_t* out_survivor_count,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_vertex_id* selected = NULL;
    henka_authoring_vertex_id* representatives = NULL;
    henka_authoring_vertex_id* mappings = NULL;
    henka_vec3* representative_positions = NULL;
    size_t* parent = NULL;
    size_t* next = NULL;
    int64_t* cell_x = NULL;
    int64_t* cell_y = NULL;
    int64_t* cell_z = NULL;
    modeling_spatial_cell* table = NULL;
    size_t table_capacity;
    size_t index;
    size_t representative_count = 0U;
    bool changed = false;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    if (out_survivor_count != NULL) *out_survivor_count = 0U;
    modeling_report_reset(out_report);
    if (mesh == NULL || vertex_ids == NULL || vertex_count < 2U ||
        !modeling_finite_scalar(tolerance) || tolerance <= 0.0f || out_surviving_vertices == NULL ||
        out_survivor_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!henka_checked_size_multiply(vertex_count, sizeof(*selected), &table_capacity) ||
        vertex_count > (SIZE_MAX - 1U) / 2U || survivor_capacity < 1U)
    {
        return HENKA_ERROR_LIMIT;
    }
    selected = henka_malloc(vertex_count * sizeof(*selected));
    parent = henka_malloc(vertex_count * sizeof(*parent));
    next = henka_malloc(vertex_count * sizeof(*next));
    cell_x = henka_malloc(vertex_count * sizeof(*cell_x));
    cell_y = henka_malloc(vertex_count * sizeof(*cell_y));
    cell_z = henka_malloc(vertex_count * sizeof(*cell_z));
    representatives = henka_malloc(vertex_count * sizeof(*representatives));
    mappings = henka_malloc(vertex_count * sizeof(*mappings));
    representative_positions = henka_malloc(vertex_count * sizeof(*representative_positions));
    table_capacity = vertex_count * 2U + 1U;
    table = henka_calloc(table_capacity, sizeof(*table));
    if (selected == NULL || parent == NULL || next == NULL || cell_x == NULL || cell_y == NULL ||
        cell_z == NULL || representatives == NULL || mappings == NULL ||
        representative_positions == NULL || table == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    memcpy(selected, vertex_ids, vertex_count * sizeof(*selected));
    qsort(selected, vertex_count, sizeof(*selected), modeling_vertex_id_compare);
    for (index = 0U; index < vertex_count; ++index)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(mesh, selected[index]);
        if (vertex == NULL || (index > 0U && selected[index - 1U] == selected[index]) ||
            !modeling_quantize_cell(vertex->position.x, tolerance, &cell_x[index]) ||
            !modeling_quantize_cell(vertex->position.y, tolerance, &cell_y[index]) ||
            !modeling_quantize_cell(vertex->position.z, tolerance, &cell_z[index]))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        parent[index] = index;
        next[index] = SIZE_MAX;
    }
    for (index = 0U; index < vertex_count; ++index)
    {
        size_t slot;
        const bool found = modeling_spatial_cell_lookup(
            table, table_capacity, cell_x[index], cell_y[index], cell_z[index], &slot);
        if (!found)
        {
            table[slot].used = true;
            table[slot].x = cell_x[index];
            table[slot].y = cell_y[index];
            table[slot].z = cell_z[index];
            table[slot].head = index;
        }
        else
        {
            next[index] = table[slot].head;
            table[slot].head = index;
        }
    }
    for (index = 0U; index < vertex_count; ++index)
    {
        int64_t dx;
        int64_t dy;
        int64_t dz;
        for (dx = -1; dx <= 1; ++dx)
        {
            for (dy = -1; dy <= 1; ++dy)
            {
                for (dz = -1; dz <= 1; ++dz)
                {
                    size_t slot;
                    int64_t neighbor_x;
                    int64_t neighbor_y;
                    int64_t neighbor_z;
                    size_t other;
                    if ((dx < 0 && cell_x[index] == INT64_MIN) ||
                        (dy < 0 && cell_y[index] == INT64_MIN) ||
                        (dz < 0 && cell_z[index] == INT64_MIN) ||
                        (dx > 0 && cell_x[index] == INT64_MAX) ||
                        (dy > 0 && cell_y[index] == INT64_MAX) ||
                        (dz > 0 && cell_z[index] == INT64_MAX))
                    {
                        continue;
                    }
                    neighbor_x = cell_x[index] + dx;
                    neighbor_y = cell_y[index] + dy;
                    neighbor_z = cell_z[index] + dz;
                    if (!modeling_spatial_cell_lookup(table, table_capacity,
                        neighbor_x, neighbor_y, neighbor_z, &slot)) continue;
                    other = table[slot].head;
                    while (other != SIZE_MAX)
                    {
                        if (other > index)
                        {
                            const henka_authoring_vertex* left = henka_authoring_mesh_get_vertex(
                                mesh, selected[index]);
                            const henka_authoring_vertex* right = henka_authoring_mesh_get_vertex(
                                mesh, selected[other]);
                            const double x = (double)left->position.x - (double)right->position.x;
                            const double y = (double)left->position.y - (double)right->position.y;
                            const double z = (double)left->position.z - (double)right->position.z;
                            const double distance_squared = x * x + y * y + z * z;
                            const double tolerance_squared = (double)tolerance * (double)tolerance;
                            if (distance_squared <= tolerance_squared)
                            {
                                size_t left_root = index;
                                size_t right_root = other;
                                while (parent[left_root] != left_root) left_root = parent[left_root];
                                while (parent[right_root] != right_root) right_root = parent[right_root];
                                if (left_root != right_root)
                                {
                                    if (left_root < right_root) parent[right_root] = left_root;
                                    else parent[left_root] = right_root;
                                    changed = true;
                                }
                            }
                        }
                        other = next[other];
                    }
                }
            }
        }
    }
    for (index = 0U; index < vertex_count; ++index)
    {
        size_t root = index;
        while (parent[root] != root) root = parent[root];
        parent[index] = root;
    }
    for (index = 0U; index < vertex_count; ++index)
    {
        size_t member;
        if (parent[index] != index) continue;
        representatives[representative_count] = selected[index];
        {
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;
            size_t count = 0U;
            for (member = 0U; member < vertex_count; ++member)
            {
                if (parent[member] == index)
                {
                    const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(
                        mesh, selected[member]);
                    x += (double)vertex->position.x;
                    y += (double)vertex->position.y;
                    z += (double)vertex->position.z;
                    ++count;
                }
            }
            representative_positions[representative_count] = (henka_vec3){
                (float)(x / (double)count), (float)(y / (double)count), (float)(z / (double)count)};
            if (!modeling_finite_vec3(representative_positions[representative_count]))
            {
                result = HENKA_ERROR_NUMERIC_RANGE;
                goto cleanup;
            }
        }
        ++representative_count;
    }
    if (survivor_capacity < representative_count)
    {
        result = HENKA_ERROR_LIMIT;
        goto cleanup;
    }
    for (index = 0U; index < vertex_count; ++index)
    {
        size_t representative_index;
        size_t root = parent[index];
        for (representative_index = 0U; representative_index < representative_count; ++representative_index)
        {
            if (representatives[representative_index] == selected[root]) break;
        }
        if (representative_index == representative_count)
        {
            result = HENKA_ERROR_UNKNOWN;
            goto cleanup;
        }
        mappings[index] = representatives[representative_index];
    }
    if (!changed)
    {
        memcpy(out_surviving_vertices, representatives, representative_count * sizeof(*representatives));
        *out_survivor_count = representative_count;
        if (out_report != NULL) out_report->changed = false;
        result = HENKA_SUCCESS;
        goto cleanup;
    }
    result = modeling_apply_merge(mesh, selected, mappings, vertex_count,
        representatives, representative_positions, representative_count, out_surviving_vertices,
        survivor_capacity, out_survivor_count, out_report);

cleanup:
    henka_free(selected);
    henka_free(representatives);
    henka_free(mappings);
    henka_free(representative_positions);
    henka_free(parent);
    henka_free(next);
    henka_free(cell_x);
    henka_free(cell_y);
    henka_free(cell_z);
    henka_free(table);
    return result;
}

static henka_result modeling_commit(
    henka_authoring_mesh* destination,
    henka_authoring_mesh* candidate)
{
    henka_result result = henka_authoring_mesh_copy(destination, candidate);
    henka_authoring_mesh_destroy(candidate);
    return result;
}

static bool modeling_face_contains_vertex(
    const henka_authoring_face* face,
    henka_authoring_vertex_id vertex_id,
    size_t* out_corner)
{
    size_t corner;
    if (face == NULL) return false;
    for (corner = 0U; corner < face->corner_count; ++corner)
    {
        if (face->vertices[corner] == vertex_id)
        {
            if (out_corner != NULL) *out_corner = corner;
            return true;
        }
    }
    return false;
}

static bool modeling_uv_near(henka_vec2 left, henka_vec2 right)
{
    return isfinite(left.x) && isfinite(left.y) && isfinite(right.x) && isfinite(right.y) &&
        fabsf(left.x - right.x) <= 1.0e-5f && fabsf(left.y - right.y) <= 1.0e-5f;
}

static henka_result modeling_dissolve_one_vertex(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id vertex_id)
{
    henka_authoring_mesh_desc desc;
    henka_authoring_face_id* incident = NULL;
    size_t incident_count = 0U;
    size_t face_id;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;
    if (mesh == NULL || henka_authoring_mesh_get_vertex(mesh, vertex_id) == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    desc = henka_authoring_mesh_get_desc(mesh);
    incident = henka_malloc(desc.max_faces * sizeof(*incident));
    if (incident == NULL) return HENKA_ERROR_OUT_OF_MEMORY;
    for (face_id = 1U; face_id <= desc.max_faces; ++face_id)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            mesh, (henka_authoring_face_id)face_id);
        if (modeling_face_contains_vertex(face, vertex_id, NULL)) incident[incident_count++] = (henka_authoring_face_id)face_id;
    }
    if (incident_count == 0U)
    {
        result = henka_authoring_mesh_remove_vertex(mesh, vertex_id);
        goto cleanup;
    }
    if (incident_count == 1U)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(mesh, incident[0]);
        henka_authoring_face_loop_update update = {0};
        henka_authoring_vertex_id vertices[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
        henka_vec2 uvs[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
        size_t corner;
        size_t write = 0U;
        if (face == NULL || face->corner_count <= 3U)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            if (face->vertices[corner] != vertex_id)
            {
                vertices[write] = face->vertices[corner];
                uvs[write++] = face->uvs[corner];
            }
        }
        update.face_id = face->id;
        update.vertices = vertices;
        update.uvs = uvs;
        update.corner_count = write;
        update.material_region = face->material_region;
        update.smooth = face->smooth;
        result = henka_authoring_mesh_apply_face_loop_updates_internal(mesh, &update, 1U);
        if (result == HENKA_SUCCESS) result = henka_authoring_mesh_remove_vertex(mesh, vertex_id);
        goto cleanup;
    }
    {
        henka_authoring_vertex_id* neighbors = NULL;
        henka_authoring_vertex_id* adjacent_first = NULL;
        henka_authoring_vertex_id* adjacent_second = NULL;
        henka_vec2* neighbor_uvs = NULL;
        henka_authoring_face_loop_update* updates = NULL;
        size_t neighbor_count = 0U;
        size_t adjacency_count = 0U;
        size_t index;
        size_t start_index = SIZE_MAX;
        size_t current_index;
        size_t previous_index = SIZE_MAX;
        size_t boundary_count = 0U;
        henka_authoring_vertex_id boundary[HENKA_AUTHORING_MESH_HARD_MAX_FACES];
        henka_vec2 boundary_uvs[HENKA_AUTHORING_MESH_HARD_MAX_FACES];
        uint32_t material_region = 0U;
        bool smooth = false;
        bool metadata_initialized = false;

        if (incident_count > HENKA_AUTHORING_MESH_HARD_MAX_FACES)
        {
            result = HENKA_ERROR_LIMIT;
            goto dissolve_cleanup;
        }
        neighbors = henka_calloc(incident_count * 2U, sizeof(*neighbors));
        adjacent_first = henka_calloc(incident_count, sizeof(*adjacent_first));
        adjacent_second = henka_calloc(incident_count, sizeof(*adjacent_second));
        neighbor_uvs = henka_calloc(incident_count * 2U, sizeof(*neighbor_uvs));
        updates = henka_calloc(incident_count, sizeof(*updates));
        if (neighbors == NULL || adjacent_first == NULL || adjacent_second == NULL ||
            neighbor_uvs == NULL || updates == NULL)
        {
            result = HENKA_ERROR_OUT_OF_MEMORY;
            goto dissolve_cleanup;
        }
        for (index = 0U; index < incident_count; ++index)
        {
            const henka_authoring_face* face = henka_authoring_mesh_get_face(mesh, incident[index]);
            size_t corner;
            const henka_authoring_edge* previous_edge;
            const henka_authoring_edge* next_edge;
            if (face == NULL || face->corner_count != 3U ||
                !modeling_face_contains_vertex(face, vertex_id, &corner))
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto dissolve_cleanup;
            }
            previous_edge = henka_authoring_mesh_get_edge(
                mesh, face->edges[(corner + face->corner_count - 1U) % face->corner_count]);
            next_edge = henka_authoring_mesh_get_edge(mesh, face->edges[corner]);
            if (previous_edge == NULL || next_edge == NULL || previous_edge->face_count != 2U ||
                next_edge->face_count != 2U || previous_edge->hard || next_edge->hard)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto dissolve_cleanup;
            }
            if (!metadata_initialized)
            {
                material_region = face->material_region;
                smooth = face->smooth;
                metadata_initialized = true;
            }
            else if (material_region != face->material_region || smooth != face->smooth)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto dissolve_cleanup;
            }
            if (!modeling_uv_near(face->uvs[corner], face->uvs[corner]))
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto dissolve_cleanup;
            }
            adjacent_first[adjacency_count] = face->vertices[(corner + 2U) % 3U];
            adjacent_second[adjacency_count] = face->vertices[(corner + 1U) % 3U];
            ++adjacency_count;
            {
                const henka_authoring_vertex_id pair[2] = {
                    adjacent_first[adjacency_count - 1U], adjacent_second[adjacency_count - 1U]};
                size_t pair_index;
                for (pair_index = 0U; pair_index < 2U; ++pair_index)
                {
                    size_t neighbor_index;
                    bool found = false;
                    for (neighbor_index = 0U; neighbor_index < neighbor_count; ++neighbor_index)
                    {
                        if (neighbors[neighbor_index] == pair[pair_index])
                        {
                            if (!modeling_uv_near(neighbor_uvs[neighbor_index],
                                face->uvs[(corner + (pair_index == 0U ? 2U : 1U)) % 3U]))
                            {
                                result = HENKA_ERROR_INVALID_ARGUMENT;
                                goto dissolve_cleanup;
                            }
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        neighbors[neighbor_count] = pair[pair_index];
                        neighbor_uvs[neighbor_count] = face->uvs[
                            (corner + (pair_index == 0U ? 2U : 1U)) % 3U];
                        ++neighbor_count;
                    }
                }
            }
        }
        if (neighbor_count < 3U || adjacency_count != neighbor_count)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto dissolve_cleanup;
        }
        for (index = 0U; index < neighbor_count; ++index)
        {
            if (start_index == SIZE_MAX || neighbors[index] < neighbors[start_index]) start_index = index;
        }
        current_index = start_index;
        while (boundary_count < neighbor_count)
        {
            size_t edge_index;
            size_t next_index = SIZE_MAX;
            if (current_index == SIZE_MAX || current_index == previous_index)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto dissolve_cleanup;
            }
            boundary[boundary_count] = neighbors[current_index];
            boundary_uvs[boundary_count] = neighbor_uvs[current_index];
            ++boundary_count;
            for (edge_index = 0U; edge_index < adjacency_count; ++edge_index)
            {
                if (adjacent_first[edge_index] == neighbors[current_index])
                {
                    size_t candidate = 0U;
                    while (candidate < neighbor_count && neighbors[candidate] != adjacent_second[edge_index]) ++candidate;
                    if (candidate < neighbor_count && candidate != previous_index && candidate != start_index)
                    {
                        next_index = candidate;
                        break;
                    }
                }
                else if (adjacent_second[edge_index] == neighbors[current_index])
                {
                    size_t candidate = 0U;
                    while (candidate < neighbor_count && neighbors[candidate] != adjacent_first[edge_index]) ++candidate;
                    if (candidate < neighbor_count && candidate != previous_index && candidate != start_index)
                    {
                        next_index = candidate;
                        break;
                    }
                }
            }
            if (boundary_count == neighbor_count)
            {
                next_index = start_index;
            }
            if (next_index == SIZE_MAX)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto dissolve_cleanup;
            }
            previous_index = current_index;
            current_index = next_index;
        }
        for (index = 0U; index < incident_count; ++index)
        {
            updates[index].face_id = incident[index];
            updates[index].remove = index != 0U;
        }
        updates[0].vertices = boundary;
        updates[0].uvs = boundary_uvs;
        updates[0].corner_count = boundary_count;
        updates[0].material_region = material_region;
        updates[0].smooth = smooth;
        result = henka_authoring_mesh_apply_face_loop_updates_internal(mesh, updates, incident_count);
        if (result == HENKA_SUCCESS) result = henka_authoring_mesh_remove_vertex(mesh, vertex_id);

dissolve_cleanup:
        henka_free(neighbors);
        henka_free(adjacent_first);
        henka_free(adjacent_second);
        henka_free(neighbor_uvs);
        henka_free(updates);
    }
cleanup:
    henka_free(incident);
    return result;
}

static bool modeling_sorted_unique_vertex_ids(
    const henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* input,
    size_t count,
    henka_authoring_vertex_id** out_sorted)
{
    henka_authoring_vertex_id* sorted;
    const henka_authoring_mesh_desc desc = mesh != NULL
        ? henka_authoring_mesh_get_desc(mesh)
        : (henka_authoring_mesh_desc){0};
    size_t index;
    if (out_sorted == NULL || mesh == NULL || input == NULL || count == 0U ||
        count > desc.max_vertices || count > SIZE_MAX / sizeof(*sorted)) return false;
    sorted = henka_malloc(count * sizeof(*sorted));
    if (sorted == NULL) return false;
    memcpy(sorted, input, count * sizeof(*sorted));
    qsort(sorted, count, sizeof(*sorted), modeling_vertex_id_compare);
    for (index = 0U; index < count; ++index)
    {
        if (henka_authoring_mesh_get_vertex(mesh, sorted[index]) == NULL ||
            (index > 0U && sorted[index - 1U] == sorted[index]))
        {
            henka_free(sorted);
            return false;
        }
    }
    *out_sorted = sorted;
    return true;
}

static void modeling_report_count_delta(
    const henka_authoring_mesh_counts* before,
    const henka_authoring_mesh_counts* after,
    henka_authoring_modeling_report* report)
{
    if (report == NULL) return;
    report->changed = true;
    report->created_vertices = after->vertices > before->vertices ? after->vertices - before->vertices : 0U;
    report->removed_vertices = before->vertices > after->vertices ? before->vertices - after->vertices : 0U;
    report->created_edges = after->edges > before->edges ? after->edges - before->edges : 0U;
    report->removed_edges = before->edges > after->edges ? before->edges - after->edges : 0U;
    report->created_faces = after->faces > before->faces ? after->faces - before->faces : 0U;
    report->removed_faces = before->faces > after->faces ? before->faces - after->faces : 0U;
}

henka_result henka_authoring_mesh_dissolve_vertices(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_vertex_id* sorted = NULL;
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_result result;
    size_t index;
    modeling_report_reset(out_report);
    if (mesh == NULL || !modeling_sorted_unique_vertex_ids(mesh, vertex_ids, vertex_count, &sorted))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    result = henka_authoring_mesh_clone(mesh, &candidate);
    for (index = 0U; result == HENKA_SUCCESS && index < vertex_count; ++index)
    {
        result = modeling_dissolve_one_vertex(candidate, sorted[index]);
    }
    if (result == HENKA_SUCCESS &&
        (!henka_authoring_mesh_validate(candidate) || henka_authoring_mesh_get_counts(candidate).faces == 0U ||
            !modeling_face_geometry_is_valid(candidate)))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        after = henka_authoring_mesh_get_counts(candidate);
        result = henka_authoring_mesh_copy(mesh, candidate);
        if (result == HENKA_SUCCESS) modeling_report_count_delta(&before, &after, out_report);
    }
    henka_authoring_mesh_destroy(candidate);
    henka_free(sorted);
    return result;
}

henka_result henka_authoring_mesh_delete_vertices(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_vertex_id* sorted = NULL;
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_authoring_mesh_desc desc;
    henka_authoring_vertex_id* affected_vertices = NULL;
    size_t affected_count = 0U;
    henka_result result;
    size_t face_id;
    size_t index;
    modeling_report_reset(out_report);
    if (mesh == NULL || !modeling_sorted_unique_vertex_ids(mesh, vertex_ids, vertex_count, &sorted))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    desc = henka_authoring_mesh_get_desc(mesh);
    before = henka_authoring_mesh_get_counts(mesh);
    result = henka_authoring_mesh_clone(mesh, &candidate);
    affected_vertices = henka_malloc(desc.max_vertices * sizeof(*affected_vertices));
    if (result == HENKA_SUCCESS && affected_vertices == NULL) result = HENKA_ERROR_OUT_OF_MEMORY;
    for (face_id = 1U; result == HENKA_SUCCESS && face_id <= desc.max_faces; ++face_id)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            candidate, (henka_authoring_face_id)face_id);
        bool affected = false;
        if (face == NULL) continue;
        for (index = 0U; index < vertex_count; ++index)
        {
            if (modeling_face_contains_vertex(face, sorted[index], NULL))
            {
                affected = true;
                break;
            }
        }
        if (!affected) continue;
        for (index = 0U; index < face->corner_count; ++index)
        {
            size_t prior;
            bool duplicate = false;
            for (prior = 0U; prior < affected_count; ++prior)
            {
                if (affected_vertices[prior] == face->vertices[index]) duplicate = true;
            }
            if (!duplicate) affected_vertices[affected_count++] = face->vertices[index];
        }
        result = henka_authoring_mesh_remove_face(candidate, face->id);
    }
    for (index = 0U; result == HENKA_SUCCESS && index < vertex_count; ++index)
    {
        result = henka_authoring_mesh_remove_vertex(candidate, sorted[index]);
    }
    for (index = 0U; result == HENKA_SUCCESS && index < affected_count; ++index)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(candidate, affected_vertices[index]);
        if (vertex != NULL) (void)henka_authoring_mesh_remove_vertex(candidate, vertex->id);
    }
    if (result == HENKA_SUCCESS &&
        (!henka_authoring_mesh_validate(candidate) || henka_authoring_mesh_get_counts(candidate).faces == 0U ||
            !modeling_face_geometry_is_valid(candidate)))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        after = henka_authoring_mesh_get_counts(candidate);
        result = henka_authoring_mesh_copy(mesh, candidate);
        if (result == HENKA_SUCCESS) modeling_report_count_delta(&before, &after, out_report);
    }
    henka_authoring_mesh_destroy(candidate);
    henka_free(affected_vertices);
    henka_free(sorted);
    return result;
}

henka_result henka_authoring_mesh_connect_vertices(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id first_vertex_id,
    henka_authoring_vertex_id second_vertex_id,
    henka_authoring_face_id* out_new_face_id,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_loop_update update = {0};
    henka_authoring_vertex_id first_path[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id second_path[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_vec2 first_uvs[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_vec2 second_uvs[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    size_t first_index = SIZE_MAX;
    size_t second_index = SIZE_MAX;
    size_t corner;
    size_t first_count;
    size_t second_count;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;
    if (out_new_face_id != NULL) *out_new_face_id = HENKA_AUTHORING_INVALID_ID;
    modeling_report_reset(out_report);
    if (mesh == NULL || out_new_face_id == NULL || first_vertex_id == second_vertex_id ||
        henka_authoring_mesh_get_vertex(mesh, first_vertex_id) == NULL ||
        henka_authoring_mesh_get_vertex(mesh, second_vertex_id) == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    {
        const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
        size_t face_id;
        for (face_id = 1U; face_id <= desc.max_faces; ++face_id)
        {
            const henka_authoring_face* face = henka_authoring_mesh_get_face(mesh, (henka_authoring_face_id)face_id);
            if (face != NULL && modeling_face_contains_vertex(face, first_vertex_id, &corner) &&
                modeling_face_contains_vertex(face, second_vertex_id, NULL))
            {
                bool second_found = false;
                for (second_index = 0U; second_index < face->corner_count; ++second_index)
                {
                    if (face->vertices[second_index] == second_vertex_id) { second_found = true; break; }
                }
                if (!second_found) continue;
                first_index = corner;
                if ((first_index + 1U) % face->corner_count == second_index ||
                    (second_index + 1U) % face->corner_count == first_index)
                {
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
                if (face->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
                {
                    return HENKA_ERROR_LIMIT;
                }
                first_count = 0U;
                corner = first_index;
                while (true)
                {
                    first_path[first_count] = face->vertices[corner];
                    first_uvs[first_count++] = face->uvs[corner];
                    if (corner == second_index) break;
                    corner = (corner + 1U) % face->corner_count;
                }
                second_count = 0U;
                corner = second_index;
                while (true)
                {
                    second_path[second_count] = face->vertices[corner];
                    second_uvs[second_count++] = face->uvs[corner];
                    if (corner == first_index) break;
                    corner = (corner + 1U) % face->corner_count;
                }
                if (first_count < 3U || second_count < 3U) return HENKA_ERROR_INVALID_ARGUMENT;
                before = henka_authoring_mesh_get_counts(mesh);
                result = henka_authoring_mesh_clone(mesh, &candidate);
                if (result == HENKA_SUCCESS)
                {
                    result = henka_authoring_mesh_add_face(candidate, second_path, second_count,
                        face->material_region, face->smooth, &new_face_id);
                }
                for (corner = 0U; result == HENKA_SUCCESS && corner < second_count; ++corner)
                {
                    result = henka_authoring_mesh_set_face_corner_uv(
                        candidate, new_face_id, corner, second_uvs[corner]);
                }
                update.face_id = face->id;
                update.vertices = first_path;
                update.uvs = first_uvs;
                update.corner_count = first_count;
                update.material_region = face->material_region;
                update.smooth = face->smooth;
                if (result == HENKA_SUCCESS) result = henka_authoring_mesh_apply_face_loop_updates_internal(
                    candidate, &update, 1U);
                if (result == HENKA_SUCCESS &&
                    (!henka_authoring_mesh_validate(candidate) || !modeling_face_geometry_is_valid(candidate)))
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                }
                if (result == HENKA_SUCCESS)
                {
                    after = henka_authoring_mesh_get_counts(candidate);
                    result = henka_authoring_mesh_copy(mesh, candidate);
                    if (result == HENKA_SUCCESS)
                    {
                        *out_new_face_id = new_face_id;
                        modeling_report_count_delta(&before, &after, out_report);
                        if (out_report != NULL) out_report->primary_face_id = new_face_id;
                    }
                }
                henka_authoring_mesh_destroy(candidate);
                return result;
            }
        }
    }
    return result;
}

static henka_vec3 modeling_face_normal(
    const henka_authoring_mesh* mesh,
    const henka_authoring_face* face)
{
    const henka_vec3 a = henka_authoring_mesh_get_vertex(mesh, face->vertices[0])->position;
    const henka_vec3 b = henka_authoring_mesh_get_vertex(mesh, face->vertices[1])->position;
    const henka_vec3 c = henka_authoring_mesh_get_vertex(mesh, face->vertices[2])->position;
    return henka_vec3_normalize(henka_vec3_cross(
        henka_vec3_subtract(b, a), henka_vec3_subtract(c, a)));
}

static bool modeling_face_is_valid(const henka_authoring_mesh* mesh, henka_authoring_face_id face_id)
{
    const henka_authoring_face* face = henka_authoring_mesh_get_face(mesh, face_id);
    return face != NULL && face->corner_count >= 3U;
}

static henka_result modeling_add_offset_face(
    henka_authoring_mesh* mesh,
    const henka_authoring_face* source,
    henka_vec3 offset,
    henka_authoring_face_id* out_face_id)
{
    henka_authoring_vertex_id vertices[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    size_t corner;
    if (source->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
    {
        return HENKA_ERROR_LIMIT;
    }
    for (corner = 0U; corner < source->corner_count; ++corner)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(mesh, source->vertices[corner]);
        if (vertex == NULL || henka_authoring_mesh_add_vertex(
            mesh, henka_vec3_add(vertex->position, offset), vertex->uv,
            vertex->material_region, &vertices[corner]) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_LIMIT;
        }
    }
    if (henka_authoring_mesh_add_face(
        mesh, vertices, source->corner_count, source->material_region, source->smooth, out_face_id) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_LIMIT;
    }
    for (corner = 0U; corner < source->corner_count; ++corner)
    {
        if (henka_authoring_mesh_set_face_corner_uv(mesh, *out_face_id, corner, source->uvs[corner]) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_create_plane(
    const henka_authoring_mesh_desc* desc,
    float width,
    float depth,
    henka_authoring_mesh** out_mesh)
{
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertices[4];
    henka_authoring_vertex_id face[] = {1U, 2U, 3U, 4U};
    size_t index;
    henka_result result;
    if (out_mesh == NULL || !modeling_finite_scalar(width) || !modeling_finite_scalar(depth) ||
        width <= 0.0f || depth <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_mesh = NULL;
    result = henka_authoring_mesh_create(desc, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    {
        const float half_width = width * 0.5f;
        const float half_depth = depth * 0.5f;
        const henka_vec3 positions[4] = {
            {-half_width, 0.0f, -half_depth}, {half_width, 0.0f, -half_depth},
            {half_width, 0.0f, half_depth}, {-half_width, 0.0f, half_depth}};
        const henka_vec2 uvs[4] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
        for (index = 0U; index < 4U; ++index)
        {
            result = henka_authoring_mesh_add_vertex(mesh, positions[index], uvs[index], 0U, &vertices[index]);
            if (result != HENKA_SUCCESS)
            {
                henka_authoring_mesh_destroy(mesh);
                return result;
            }
        }
    }
    result = henka_authoring_mesh_add_face(mesh, face, 4U, 0U, true, &(henka_authoring_face_id){0U});
    if (result != HENKA_SUCCESS || !henka_authoring_mesh_validate(mesh))
    {
        henka_authoring_mesh_destroy(mesh);
        return result == HENKA_SUCCESS ? HENKA_ERROR_UNKNOWN : result;
    }
    *out_mesh = mesh;
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_create_box(
    const henka_authoring_mesh_desc* desc,
    float width,
    float height,
    float depth,
    henka_authoring_mesh** out_mesh)
{
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertices[8];
    const henka_authoring_vertex_id faces[6][4] = {
        {5U, 6U, 7U, 8U}, {4U, 3U, 2U, 1U}, {2U, 3U, 7U, 6U},
        {1U, 5U, 8U, 4U}, {4U, 8U, 7U, 3U}, {1U, 2U, 6U, 5U}};
    size_t index;
    size_t face_index;
    henka_result result;
    if (out_mesh == NULL || !modeling_finite_scalar(width) || !modeling_finite_scalar(height) ||
        !modeling_finite_scalar(depth) || width <= 0.0f || height <= 0.0f || depth <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_mesh = NULL;
    result = henka_authoring_mesh_create(desc, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    {
        const float x = width * 0.5f;
        const float y = height * 0.5f;
        const float z = depth * 0.5f;
        const henka_vec3 positions[8] = {
            {-x, -y, -z}, {x, -y, -z}, {x, y, -z}, {-x, y, -z},
            {-x, -y, z}, {x, -y, z}, {x, y, z}, {-x, y, z}};
        for (index = 0U; index < 8U; ++index)
        {
            result = henka_authoring_mesh_add_vertex(mesh, positions[index],
                (henka_vec2){(positions[index].x / width) + 0.5f,
                    (positions[index].z / depth) + 0.5f}, 0U, &vertices[index]);
            if (result != HENKA_SUCCESS)
            {
                henka_authoring_mesh_destroy(mesh);
                return result;
            }
        }
    }
    for (face_index = 0U; face_index < 6U; ++face_index)
    {
        henka_authoring_vertex_id face_vertices[4];
        henka_authoring_face_id face_id;
        for (index = 0U; index < 4U; ++index)
        {
            face_vertices[index] = vertices[faces[face_index][index] - 1U];
        }
        result = henka_authoring_mesh_add_face(mesh, face_vertices, 4U, 0U, false, &face_id);
        if (result != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(mesh);
            return result;
        }
    }
    *out_mesh = mesh;
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_duplicate_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    henka_vec3 offset,
    henka_authoring_face_id* out_face_id)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_result result;
    const henka_authoring_face* source;
    if (mesh == NULL || out_face_id == NULL || !modeling_finite_vec3(offset) ||
        !modeling_face_is_valid(mesh, face_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    source = henka_authoring_mesh_get_face(mesh, face_id);
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = modeling_add_offset_face(candidate,
            henka_authoring_mesh_get_face(candidate, face_id), offset, &new_face_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = modeling_commit(mesh, candidate);
        candidate = NULL;
    }
    henka_authoring_mesh_destroy(candidate);
    if (result == HENKA_SUCCESS)
    {
        *out_face_id = new_face_id;
    }
    (void)source;
    return result;
}

henka_result henka_authoring_mesh_extrude_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    float distance,
    henka_authoring_face_id* out_face_id)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id original[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id duplicated[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_vec2 source_uvs[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    const henka_authoring_face* source;
    henka_vec3 offset;
    size_t corner_count;
    uint32_t material_region;
    bool smooth;
    bool replace_source;
    size_t corner;
    henka_result result;
    if (mesh == NULL || out_face_id == NULL || !modeling_finite_scalar(distance) ||
        !modeling_face_is_valid(mesh, face_id))
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
        original[corner] = source->vertices[corner];
        source_uvs[corner] = source->uvs[corner];
    }
    corner_count = source->corner_count;
    material_region = source->material_region;
    smooth = source->smooth;
    replace_source = false;
    for (corner = 0U; corner < corner_count; ++corner)
    {
        const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(mesh, source->edges[corner]);
        if (edge != NULL && edge->face_count > 1U)
        {
            replace_source = true;
            break;
        }
    }
    offset = henka_vec3_scale(modeling_face_normal(mesh, source), distance);
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = replace_source ? henka_authoring_mesh_remove_face(candidate, face_id) : HENKA_SUCCESS;
    {
        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        for (corner = 0U; corner < corner_count; ++corner)
        {
            const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(candidate, original[corner]);
            result = henka_authoring_mesh_add_vertex(candidate,
                henka_vec3_add(vertex->position, offset), vertex->uv, vertex->material_region, &duplicated[corner]);
            if (result != HENKA_SUCCESS)
            {
                goto cleanup;
            }
        }
        for (corner = 0U; corner < corner_count; ++corner)
        {
            henka_authoring_vertex_id side[4] = {
                original[corner], original[(corner + 1U) % corner_count],
                duplicated[(corner + 1U) % corner_count], duplicated[corner]};
            result = henka_authoring_mesh_add_face(candidate, side, 4U, material_region, false, &new_face_id);
            if (result != HENKA_SUCCESS)
            {
                goto cleanup;
            }
        }
        result = henka_authoring_mesh_add_face(candidate, duplicated, corner_count,
            material_region, smooth, &new_face_id);
        for (corner = 0U; result == HENKA_SUCCESS && corner < corner_count; ++corner)
        {
            result = henka_authoring_mesh_set_face_corner_uv(
                candidate, new_face_id, corner, source_uvs[corner]);
        }
    }
cleanup:
    if (result == HENKA_SUCCESS)
    {
        result = modeling_commit(mesh, candidate);
        candidate = NULL;
    }
    henka_authoring_mesh_destroy(candidate);
    if (result == HENKA_SUCCESS)
    {
        *out_face_id = new_face_id;
    }
    return result;
}

static henka_result modeling_inset_candidate(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    float factor,
    henka_authoring_face_id* out_face_id)
{
    const henka_authoring_face* source = henka_authoring_mesh_get_face(mesh, face_id);
    henka_authoring_vertex_id outer[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id inner[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_vec3 center = {0.0f, 0.0f, 0.0f};
    henka_vec2 source_uvs[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    size_t corner_count;
    uint32_t material_region;
    bool smooth;
    size_t corner;
    henka_result result;
    if (source == NULL || source->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    corner_count = source->corner_count;
    material_region = source->material_region;
    smooth = source->smooth;
    for (corner = 0U; corner < source->corner_count; ++corner)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(mesh, source->vertices[corner]);
        outer[corner] = source->vertices[corner];
        source_uvs[corner] = source->uvs[corner];
        center = henka_vec3_add(center, vertex->position);
    }
    center = henka_vec3_scale(center, 1.0f / (float)source->corner_count);
    for (corner = 0U; corner < source->corner_count; ++corner)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(mesh, outer[corner]);
        const henka_vec3 position = henka_vec3_add(center,
            henka_vec3_scale(henka_vec3_subtract(vertex->position, center), factor));
        result = henka_authoring_mesh_add_vertex(mesh, position, source_uvs[corner],
            vertex->material_region, &inner[corner]);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    result = henka_authoring_mesh_remove_face(mesh, face_id);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    for (corner = 0U; corner < corner_count; ++corner)
    {
        henka_authoring_vertex_id ring[4] = {
            outer[corner], outer[(corner + 1U) % corner_count],
            inner[(corner + 1U) % corner_count], inner[corner]};
        result = henka_authoring_mesh_add_face(mesh, ring, 4U, material_region, smooth, out_face_id);
        if (result == HENKA_SUCCESS)
        {
            result = henka_authoring_mesh_set_face_corner_uv(mesh, *out_face_id, 0U, source_uvs[corner]);
            if (result == HENKA_SUCCESS) result = henka_authoring_mesh_set_face_corner_uv(mesh, *out_face_id, 1U, source_uvs[(corner + 1U) % corner_count]);
            if (result == HENKA_SUCCESS) result = henka_authoring_mesh_set_face_corner_uv(mesh, *out_face_id, 2U, source_uvs[(corner + 1U) % corner_count]);
            if (result == HENKA_SUCCESS) result = henka_authoring_mesh_set_face_corner_uv(mesh, *out_face_id, 3U, source_uvs[corner]);
        }
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    result = henka_authoring_mesh_add_face(mesh, inner, corner_count,
        material_region, smooth, out_face_id);
    for (corner = 0U; result == HENKA_SUCCESS && corner < corner_count; ++corner)
    {
        result = henka_authoring_mesh_set_face_corner_uv(mesh, *out_face_id, corner, source_uvs[corner]);
    }
    return result;
}

henka_result henka_authoring_mesh_inset_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    float factor,
    henka_authoring_face_id* out_face_id)
{
    henka_authoring_mesh* candidate = NULL;
    henka_result result;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    if (mesh == NULL || out_face_id == NULL || !modeling_finite_scalar(factor) ||
        factor <= 0.0f || factor >= 1.0f || !modeling_face_is_valid(mesh, face_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = modeling_inset_candidate(candidate, face_id, factor, &new_face_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = modeling_commit(mesh, candidate);
        candidate = NULL;
    }
    henka_authoring_mesh_destroy(candidate);
    if (result == HENKA_SUCCESS)
    {
        *out_face_id = new_face_id;
    }
    return result;
}

henka_result henka_authoring_mesh_bevel_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    float width,
    henka_authoring_face_id* out_face_id)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_face_id inset_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id beveled_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_result result;
    if (mesh == NULL || out_face_id == NULL || !modeling_finite_scalar(width) ||
        width <= 0.0f || width >= 1.0f || !modeling_face_is_valid(mesh, face_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = modeling_inset_candidate(candidate, face_id, 1.0f - width, &inset_face_id);
    }
    if (result == HENKA_SUCCESS)
    {
        /* The inset ring is the bounded planar bevel. A later hard-surface
         * pass can add a profile or normal-weighted extrusion without changing
         * this operation's transactional contract. */
        beveled_face_id = inset_face_id;
    }
    if (result == HENKA_SUCCESS)
    {
        result = modeling_commit(mesh, candidate);
        candidate = NULL;
    }
    henka_authoring_mesh_destroy(candidate);
    if (result == HENKA_SUCCESS)
    {
        *out_face_id = beveled_face_id;
    }
    return result;
}

henka_result henka_authoring_mesh_subdivide_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    henka_authoring_vertex_id* out_center_vertex_id)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_vertex_id outer[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id midpoints[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id center_id;
    henka_authoring_face_id ignored_face;
    henka_result result;
    size_t corner;
    const henka_authoring_face* source;
    henka_vec3 center = {0.0f, 0.0f, 0.0f};
    henka_vec2 source_uvs[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_vec2 center_uv = {0.0f, 0.0f};
    size_t corner_count;
    uint32_t material_region;
    bool smooth;
    if (mesh == NULL || out_center_vertex_id == NULL || !modeling_face_is_valid(mesh, face_id))
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
        outer[corner] = source->vertices[corner];
        source_uvs[corner] = source->uvs[corner];
        center_uv.x += source_uvs[corner].x;
        center_uv.y += source_uvs[corner].y;
        center = henka_vec3_add(center,
            henka_authoring_mesh_get_vertex(mesh, outer[corner])->position);
    }
    center = henka_vec3_scale(center, 1.0f / (float)source->corner_count);
    center_uv.x /= (float)source->corner_count;
    center_uv.y /= (float)source->corner_count;
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    source = henka_authoring_mesh_get_face(candidate, face_id);
    corner_count = source->corner_count;
    material_region = source->material_region;
    smooth = source->smooth;
    result = henka_authoring_mesh_add_vertex(candidate, center, center_uv,
        material_region, &center_id);
    for (corner = 0U; result == HENKA_SUCCESS && corner < corner_count; ++corner)
    {
        const henka_authoring_vertex* first = henka_authoring_mesh_get_vertex(candidate, outer[corner]);
        const henka_authoring_vertex* second = henka_authoring_mesh_get_vertex(candidate,
            outer[(corner + 1U) % source->corner_count]);
        result = henka_authoring_mesh_add_vertex(candidate,
            henka_vec3_scale(henka_vec3_add(first->position, second->position), 0.5f),
            (henka_vec2){(first->uv.x + second->uv.x) * 0.5f,
                (first->uv.y + second->uv.y) * 0.5f}, material_region, &midpoints[corner]);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_remove_face(candidate, face_id);
    }
    for (corner = 0U; result == HENKA_SUCCESS && corner < corner_count; ++corner)
    {
        henka_authoring_vertex_id quad[4] = {
            outer[corner], midpoints[corner], center_id,
            midpoints[(corner + corner_count - 1U) % corner_count]};
        result = henka_authoring_mesh_add_face(candidate, quad, 4U,
            material_region, smooth, &ignored_face);
        if (result == HENKA_SUCCESS)
        {
            henka_vec2 midpoint_uv = {
                (source_uvs[corner].x + source_uvs[(corner + 1U) % corner_count].x) * 0.5f,
                (source_uvs[corner].y + source_uvs[(corner + 1U) % corner_count].y) * 0.5f};
            henka_vec2 previous_uv = {
                (source_uvs[(corner + corner_count - 1U) % corner_count].x + source_uvs[corner].x) * 0.5f,
                (source_uvs[(corner + corner_count - 1U) % corner_count].y + source_uvs[corner].y) * 0.5f};
            result = henka_authoring_mesh_set_face_corner_uv(candidate, ignored_face, 0U, source_uvs[corner]);
            if (result == HENKA_SUCCESS) result = henka_authoring_mesh_set_face_corner_uv(candidate, ignored_face, 1U, midpoint_uv);
            if (result == HENKA_SUCCESS) result = henka_authoring_mesh_set_face_corner_uv(candidate, ignored_face, 2U, center_uv);
            if (result == HENKA_SUCCESS) result = henka_authoring_mesh_set_face_corner_uv(candidate, ignored_face, 3U, previous_uv);
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = modeling_commit(mesh, candidate);
        candidate = NULL;
    }
    henka_authoring_mesh_destroy(candidate);
    if (result == HENKA_SUCCESS)
    {
        *out_center_vertex_id = center_id;
    }
    return result;
}
