#include <henka/authoring_modeling.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <henka/core.h>
#include <henka/memory.h>
#include <henka/authoring_topology.h>

#include "../core/checked.h"
#include "authoring_mesh_internal.h"

static henka_authoring_edge_id modeling_find_edge_between_vertices(
    const henka_authoring_mesh* mesh,
    henka_authoring_vertex_id first_vertex_id,
    henka_authoring_vertex_id second_vertex_id);

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
    size_t slot;
    for (slot = 0U; slot < desc.max_vertices; ++slot)
    {
        henka_authoring_vertex_id id;
        count += henka_authoring_mesh_get_vertex_id_at(mesh, slot, &id) == HENKA_SUCCESS ? 1U : 0U;
    }
    return count;
}

static bool modeling_face_geometry_is_valid(const henka_authoring_mesh* mesh)
{
    const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
    size_t slot;
    for (slot = 0U; slot < desc.max_faces; ++slot)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face = henka_authoring_mesh_get_face_id_at(mesh, slot, &face_id) == HENKA_SUCCESS
            ? henka_authoring_mesh_get_face(mesh, face_id)
            : NULL;
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
    size_t slot;
    const henka_authoring_vertex_id low = first < second ? first : second;
    const henka_authoring_vertex_id high = first < second ? second : first;
    if (out_edge_id == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    *out_edge_id = HENKA_AUTHORING_INVALID_ID;
    for (slot = 0U; slot < desc.max_edges; ++slot)
    {
        henka_authoring_edge_id edge_id;
        const henka_authoring_edge* edge = henka_authoring_mesh_get_edge_id_at(mesh, slot, &edge_id) == HENKA_SUCCESS
            ? henka_authoring_mesh_get_edge(mesh, edge_id)
            : NULL;
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
    size_t slot;
    if (out_updates == NULL || out_update_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_updates = NULL;
    *out_update_count = 0U;
    updates = henka_calloc(desc.max_faces, sizeof(*updates));
    if (updates == NULL) return HENKA_ERROR_OUT_OF_MEMORY;
    for (slot = 0U; slot < desc.max_faces; ++slot)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face = henka_authoring_mesh_get_face_id_at(mesh, slot, &face_id) == HENKA_SUCCESS
            ? henka_authoring_mesh_get_face(mesh, face_id)
            : NULL;
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
        updates[update_count].face_id = face_id;
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
        size_t slot;
        for (slot = 0U; slot < desc.max_edges; ++slot)
        {
            henka_authoring_edge_id edge_id;
            const henka_authoring_edge* old_edge = henka_authoring_mesh_get_edge_id_at(mesh, slot, &edge_id) == HENKA_SUCCESS
                ? henka_authoring_mesh_get_edge(mesh, edge_id)
                : NULL;
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

static bool modeling_face_contains_edge(
    const henka_authoring_face* face,
    henka_authoring_edge_id edge_id,
    size_t* out_corner)
{
    size_t corner;
    if (face == NULL || face->edges == NULL)
    {
        return false;
    }
    for (corner = 0U; corner < face->corner_count; ++corner)
    {
        if (face->edges[corner] == edge_id)
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
    henka_authoring_mesh_desc desc = {0};
    henka_authoring_face_id* incident = NULL;
    size_t incident_count = 0U;
    size_t face_slot;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;
    if (mesh == NULL || henka_authoring_mesh_get_vertex(mesh, vertex_id) == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    desc = henka_authoring_mesh_get_desc(mesh);
    incident = henka_malloc(desc.max_faces * sizeof(*incident));
    if (incident == NULL) return HENKA_ERROR_OUT_OF_MEMORY;
    for (face_slot = 0U; face_slot < desc.max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face = henka_authoring_mesh_get_face_id_at(mesh, face_slot, &face_id) == HENKA_SUCCESS
            ? henka_authoring_mesh_get_face(mesh, face_id)
            : NULL;
        if (modeling_face_contains_vertex(face, vertex_id, NULL)) incident[incident_count++] = face_id;
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
    size_t face_slot;
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
    for (face_slot = 0U; result == HENKA_SUCCESS && face_slot < desc.max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face = henka_authoring_mesh_get_face_id_at(candidate, face_slot, &face_id) == HENKA_SUCCESS
            ? henka_authoring_mesh_get_face(candidate, face_id)
            : NULL;
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
        size_t face_slot;
        for (face_slot = 0U; face_slot < desc.max_faces; ++face_slot)
        {
            henka_authoring_face_id face_id;
            const henka_authoring_face* face = henka_authoring_mesh_get_face_id_at(mesh, face_slot, &face_id) == HENKA_SUCCESS
                ? henka_authoring_mesh_get_face(mesh, face_id)
                : NULL;
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

henka_result henka_authoring_mesh_loop_cut_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    size_t edge_offset,
    float factor,
    henka_authoring_face_id* out_new_face_id,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_mesh* candidate = NULL;
    const henka_authoring_face* source_face;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_authoring_face_loop_update update = {0};
    henka_authoring_vertex_id source_vertices[4];
    henka_vec2 source_uvs[4];
    henka_authoring_vertex_id cut_vertices[2] = {
        HENKA_AUTHORING_INVALID_ID, HENKA_AUTHORING_INVALID_ID};
    henka_vec2 cut_uvs[2];
    henka_vec3 cut_positions[2];
    uint32_t cut_material_regions[2];
    henka_authoring_vertex_id first_face_vertices[4];
    henka_vec2 first_face_uvs[4];
    henka_authoring_vertex_id second_face_vertices[4];
    henka_vec2 second_face_uvs[4];
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    size_t corner;
    size_t pair;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    if (out_new_face_id != NULL) *out_new_face_id = HENKA_AUTHORING_INVALID_ID;
    modeling_report_reset(out_report);
    if (mesh == NULL || out_new_face_id == NULL || edge_offset > 1U ||
        !isfinite(factor) || factor <= 0.0f || factor >= 1.0f ||
        !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    source_face = henka_authoring_mesh_get_face(mesh, face_id);
    if (source_face == NULL || source_face->corner_count != 4U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (corner = 0U; corner < 4U; ++corner)
    {
        const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
            mesh, source_face->edges[corner]);
        if (edge == NULL || edge->face_count != 1U || edge->faces[0] != face_id)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        source_vertices[corner] = source_face->vertices[corner];
        source_uvs[corner] = source_face->uvs[corner];
    }
    for (pair = 0U; pair < 2U; ++pair)
    {
        const size_t first_corner = (edge_offset + pair * 2U) % 4U;
        const size_t second_corner = (first_corner + 1U) % 4U;
        const henka_authoring_vertex* first_vertex = henka_authoring_mesh_get_vertex(
            mesh, source_vertices[first_corner]);
        const henka_authoring_vertex* second_vertex = henka_authoring_mesh_get_vertex(
            mesh, source_vertices[second_corner]);
        const henka_vec3 delta = first_vertex != NULL && second_vertex != NULL
            ? henka_vec3_subtract(second_vertex->position, first_vertex->position)
            : (henka_vec3){0.0f, 0.0f, 0.0f};
        const float length = henka_vec3_length(delta);
        if (first_vertex == NULL || second_vertex == NULL ||
            !isfinite(length) || length <= 1.0e-7f)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        cut_positions[pair] = henka_vec3_add(
            first_vertex->position, henka_vec3_scale(delta, factor));
        cut_uvs[pair] = (henka_vec2){
            first_vertex->uv.x + (second_vertex->uv.x - first_vertex->uv.x) * factor,
            first_vertex->uv.y + (second_vertex->uv.y - first_vertex->uv.y) * factor};
        cut_material_regions[pair] = first_vertex->material_region;
        if (!isfinite(cut_positions[pair].x) || !isfinite(cut_positions[pair].y) ||
            !isfinite(cut_positions[pair].z) || !isfinite(cut_uvs[pair].x) ||
            !isfinite(cut_uvs[pair].y))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    before = henka_authoring_mesh_get_counts(mesh);
    {
        const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
        if (desc.max_vertices < before.vertices || desc.max_faces < before.faces ||
            desc.max_vertices - before.vertices < 2U || desc.max_faces == before.faces)
        {
            return HENKA_ERROR_LIMIT;
        }
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result != HENKA_SUCCESS) goto cleanup;
    for (pair = 0U; pair < 2U; ++pair)
    {
        result = henka_authoring_mesh_add_vertex(
            candidate, cut_positions[pair], cut_uvs[pair],
            cut_material_regions[pair], &cut_vertices[pair]);
        if (result != HENKA_SUCCESS) goto cleanup;
    }
    {
        const size_t first_corner = edge_offset;
        const size_t second_corner = (edge_offset + 1U) % 4U;
        const size_t third_corner = (edge_offset + 2U) % 4U;
        const size_t fourth_corner = (edge_offset + 3U) % 4U;
        first_face_vertices[0] = source_vertices[first_corner];
        first_face_vertices[1] = cut_vertices[0];
        first_face_vertices[2] = cut_vertices[1];
        first_face_vertices[3] = source_vertices[fourth_corner];
        first_face_uvs[0] = source_uvs[first_corner];
        first_face_uvs[1] = cut_uvs[0];
        first_face_uvs[2] = cut_uvs[1];
        first_face_uvs[3] = source_uvs[fourth_corner];
        second_face_vertices[0] = cut_vertices[0];
        second_face_vertices[1] = source_vertices[second_corner];
        second_face_vertices[2] = source_vertices[third_corner];
        second_face_vertices[3] = cut_vertices[1];
        second_face_uvs[0] = cut_uvs[0];
        second_face_uvs[1] = source_uvs[second_corner];
        second_face_uvs[2] = source_uvs[third_corner];
        second_face_uvs[3] = cut_uvs[1];
    }
    update.face_id = source_face->id;
    update.vertices = first_face_vertices;
    update.uvs = first_face_uvs;
    update.corner_count = 4U;
    update.material_region = source_face->material_region;
    update.smooth = source_face->smooth;
    result = henka_authoring_mesh_apply_face_loop_updates_internal(candidate, &update, 1U);
    if (result != HENKA_SUCCESS) goto cleanup;
    result = henka_authoring_mesh_add_face(
        candidate, second_face_vertices, 4U,
        source_face->material_region, source_face->smooth, &new_face_id);
    if (result != HENKA_SUCCESS) goto cleanup;
    for (corner = 0U; corner < 4U; ++corner)
    {
        result = henka_authoring_mesh_set_face_corner_uv(
            candidate, new_face_id, corner, second_face_uvs[corner]);
        if (result != HENKA_SUCCESS) goto cleanup;
    }
    if (!henka_authoring_mesh_validate(candidate) ||
        !modeling_face_geometry_is_valid(candidate))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(candidate);
    result = henka_authoring_mesh_copy(mesh, candidate);
    if (result == HENKA_SUCCESS)
    {
        *out_new_face_id = new_face_id;
        modeling_report_count_delta(&before, &after, out_report);
        if (out_report != NULL) out_report->primary_face_id = new_face_id;
    }

cleanup:
    henka_authoring_mesh_destroy(candidate);
    return result;
}

henka_result henka_authoring_mesh_loop_cut_quad_strip(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id start_edge_id,
    float factor,
    henka_authoring_face_id* out_new_face_id,
    henka_authoring_edge_id* out_primary_cut_edge_id,
    bool* out_closed,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_quad_strip_step* steps = NULL;
    henka_authoring_face_loop_update* updates = NULL;
    henka_authoring_face_id* new_face_ids = NULL;
    henka_authoring_vertex_id* cut_vertices = NULL;
    henka_authoring_vertex_id* first_face_vertices = NULL;
    henka_authoring_vertex_id* second_face_vertices = NULL;
    henka_vec2* cut_uvs = NULL;
    henka_vec2* first_face_uvs = NULL;
    henka_vec2* second_face_uvs = NULL;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    const henka_authoring_edge* start_edge;
    size_t step_count = 0U;
    size_t cut_count;
    size_t index;
    size_t bytes;
    size_t face_corner_count;
    bool closed = false;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    if (out_new_face_id != NULL) *out_new_face_id = HENKA_AUTHORING_INVALID_ID;
    if (out_primary_cut_edge_id != NULL)
    {
        *out_primary_cut_edge_id = HENKA_AUTHORING_INVALID_ID;
    }
    if (out_closed != NULL) *out_closed = false;
    modeling_report_reset(out_report);
    if (mesh == NULL || out_new_face_id == NULL || out_primary_cut_edge_id == NULL ||
        out_closed == NULL || !isfinite(factor) || factor <= 0.0f || factor >= 1.0f ||
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
    result = henka_authoring_topology_walk_quad_strip(
        mesh, start_edge_id, NULL, 0U, &step_count, &closed);
    if (result != HENKA_SUCCESS || step_count == 0U ||
        (!closed && start_edge->face_count != 1U))
    {
        return result == HENKA_SUCCESS ? HENKA_ERROR_INVALID_ARGUMENT : result;
    }
    if (closed)
    {
        cut_count = step_count;
    }
    else if (!henka_checked_size_add(step_count, 1U, &cut_count))
    {
        return HENKA_ERROR_LIMIT;
    }
    if (!henka_checked_size_multiply(
            step_count, sizeof(*steps), &bytes))
    {
        return HENKA_ERROR_LIMIT;
    }
    steps = (henka_authoring_quad_strip_step*)henka_malloc(bytes);
    if (steps == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    result = henka_authoring_topology_walk_quad_strip(
        mesh, start_edge_id, steps, step_count, &step_count, &closed);
    if (result != HENKA_SUCCESS)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    {
        const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
        if (desc.max_vertices < before.vertices || desc.max_faces < before.faces ||
            desc.max_vertices - before.vertices < cut_count ||
            desc.max_faces - before.faces < step_count)
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
    }
    if (!henka_checked_size_multiply(cut_count, sizeof(*cut_vertices), &bytes)) goto limit_cleanup;
    cut_vertices = (henka_authoring_vertex_id*)henka_malloc(bytes);
    if (!henka_checked_size_multiply(cut_count, sizeof(*cut_uvs), &bytes)) goto limit_cleanup;
    cut_uvs = (henka_vec2*)henka_malloc(bytes);
    if (!henka_checked_size_multiply(step_count, sizeof(*updates), &bytes)) goto limit_cleanup;
    updates = (henka_authoring_face_loop_update*)henka_calloc(1U, bytes);
    if (!henka_checked_size_multiply(step_count, sizeof(*new_face_ids), &bytes)) goto limit_cleanup;
    new_face_ids = (henka_authoring_face_id*)henka_malloc(bytes);
    if (!henka_checked_size_multiply(step_count, 4U, &face_corner_count) ||
        !henka_checked_size_multiply(face_corner_count, sizeof(*first_face_vertices), &bytes)) goto limit_cleanup;
    first_face_vertices = (henka_authoring_vertex_id*)henka_malloc(bytes);
    if (!henka_checked_size_multiply(face_corner_count, sizeof(*second_face_vertices), &bytes)) goto limit_cleanup;
    second_face_vertices = (henka_authoring_vertex_id*)henka_malloc(bytes);
    if (!henka_checked_size_multiply(face_corner_count, sizeof(*first_face_uvs), &bytes)) goto limit_cleanup;
    first_face_uvs = (henka_vec2*)henka_malloc(bytes);
    if (!henka_checked_size_multiply(face_corner_count, sizeof(*second_face_uvs), &bytes)) goto limit_cleanup;
    second_face_uvs = (henka_vec2*)henka_malloc(bytes);
    if (cut_vertices == NULL || cut_uvs == NULL || updates == NULL || new_face_ids == NULL ||
        first_face_vertices == NULL || second_face_vertices == NULL ||
        first_face_uvs == NULL || second_face_uvs == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    for (index = 0U; index < cut_count; ++index)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            mesh, index == 0U ? steps[0].face_id : steps[index - 1U].face_id);
        const henka_authoring_edge_id edge_id = index == 0U
            ? steps[0].entry_edge_id : steps[index - 1U].exit_edge_id;
        size_t corner;
        const henka_authoring_vertex* first_vertex;
        const henka_authoring_vertex* second_vertex;
        henka_vec3 delta;
        float length;
        if (face == NULL || !modeling_face_contains_edge(face, edge_id, &corner))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        first_vertex = henka_authoring_mesh_get_vertex(mesh, face->vertices[corner]);
        second_vertex = henka_authoring_mesh_get_vertex(mesh, face->vertices[(corner + 1U) % 4U]);
        if (first_vertex == NULL || second_vertex == NULL)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        delta = henka_vec3_subtract(second_vertex->position, first_vertex->position);
        length = henka_vec3_length(delta);
        if (!isfinite(length) || length <= 1.0e-7f)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        cut_uvs[index] = (henka_vec2){
            face->uvs[corner].x +
                (face->uvs[(corner + 1U) % 4U].x - face->uvs[corner].x) * factor,
            face->uvs[corner].y +
                (face->uvs[(corner + 1U) % 4U].y - face->uvs[corner].y) * factor};
        if (!isfinite(cut_uvs[index].x) || !isfinite(cut_uvs[index].y))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result != HENKA_SUCCESS) goto cleanup;
    for (index = 0U; index < cut_count; ++index)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            mesh, index == 0U ? steps[0].face_id : steps[index - 1U].face_id);
        const henka_authoring_edge_id edge_id = index == 0U
            ? steps[0].entry_edge_id : steps[index - 1U].exit_edge_id;
        size_t corner;
        const henka_authoring_vertex* first_vertex;
        const henka_authoring_vertex* second_vertex;
        henka_vec3 position;
        if (!modeling_face_contains_edge(face, edge_id, &corner))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        first_vertex = henka_authoring_mesh_get_vertex(mesh, face->vertices[corner]);
        second_vertex = henka_authoring_mesh_get_vertex(mesh, face->vertices[(corner + 1U) % 4U]);
        position = henka_vec3_add(
            first_vertex->position,
            henka_vec3_scale(
                henka_vec3_subtract(second_vertex->position, first_vertex->position), factor));
        result = henka_authoring_mesh_add_vertex(
            candidate, position, cut_uvs[index], first_vertex->material_region,
            &cut_vertices[index]);
        if (result != HENKA_SUCCESS) goto cleanup;
    }
    for (index = 0U; index < step_count; ++index)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(mesh, steps[index].face_id);
        size_t corner;
        size_t next;
        size_t prior;
        henka_authoring_vertex_id* first = &first_face_vertices[index * 4U];
        henka_authoring_vertex_id* second = &second_face_vertices[index * 4U];
        henka_vec2* first_uv = &first_face_uvs[index * 4U];
        henka_vec2* second_uv = &second_face_uvs[index * 4U];
        if (face == NULL || !modeling_face_contains_edge(face, steps[index].entry_edge_id, &corner))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        next = (corner + 1U) % 4U;
        prior = (corner + 3U) % 4U;
        first[0] = face->vertices[corner];
        first[1] = cut_vertices[index];
        first[2] = cut_vertices[(index + 1U) % cut_count];
        first[3] = face->vertices[prior];
        first_uv[0] = face->uvs[corner];
        first_uv[1] = cut_uvs[index];
        first_uv[2] = cut_uvs[(index + 1U) % cut_count];
        first_uv[3] = face->uvs[prior];
        second[0] = cut_vertices[index];
        second[1] = face->vertices[next];
        second[2] = face->vertices[(corner + 2U) % 4U];
        second[3] = cut_vertices[(index + 1U) % cut_count];
        second_uv[0] = cut_uvs[index];
        second_uv[1] = face->uvs[next];
        second_uv[2] = face->uvs[(corner + 2U) % 4U];
        second_uv[3] = cut_uvs[(index + 1U) % cut_count];
        updates[index].face_id = face->id;
        updates[index].vertices = first;
        updates[index].uvs = first_uv;
        updates[index].corner_count = 4U;
        updates[index].material_region = face->material_region;
        updates[index].smooth = face->smooth;
    }
    result = henka_authoring_mesh_apply_face_loop_updates_internal(candidate, updates, step_count);
    if (result != HENKA_SUCCESS) goto cleanup;
    for (index = 0U; index < step_count; ++index)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(mesh, steps[index].face_id);
        result = henka_authoring_mesh_add_face(
            candidate, &second_face_vertices[index * 4U], 4U,
            face->material_region, face->smooth, &new_face_ids[index]);
        if (result != HENKA_SUCCESS) goto cleanup;
        for (size_t corner = 0U; corner < 4U; ++corner)
        {
            result = henka_authoring_mesh_set_face_corner_uv(
                candidate, new_face_ids[index], corner,
                second_face_uvs[index * 4U + corner]);
            if (result != HENKA_SUCCESS) goto cleanup;
        }
    }
    if (!henka_authoring_mesh_validate(candidate) ||
        !modeling_face_geometry_is_valid(candidate))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(candidate);
    result = henka_authoring_mesh_copy(mesh, candidate);
    if (result == HENKA_SUCCESS)
    {
        *out_new_face_id = new_face_ids[step_count - 1U];
        *out_primary_cut_edge_id = modeling_find_edge_between_vertices(
            mesh, cut_vertices[0], cut_vertices[1]);
        *out_closed = closed;
        modeling_report_count_delta(&before, &after, out_report);
        if (out_report != NULL)
        {
            out_report->primary_face_id = *out_new_face_id;
            out_report->primary_edge_id = *out_primary_cut_edge_id;
        }
    }
    goto cleanup;

limit_cleanup:
    result = HENKA_ERROR_LIMIT;
cleanup:
    henka_free(second_face_uvs);
    henka_free(first_face_uvs);
    henka_free(second_face_vertices);
    henka_free(first_face_vertices);
    henka_free(new_face_ids);
    henka_free(cut_uvs);
    henka_free(cut_vertices);
    henka_free(updates);
    henka_free(steps);
    henka_authoring_mesh_destroy(candidate);
    return result;
}

henka_result henka_authoring_mesh_dissolve_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    const henka_authoring_edge* edge;
    const henka_authoring_face* first_face;
    const henka_authoring_face* second_face;
    henka_authoring_face_loop_update updates[2] = {{0}};
    henka_authoring_vertex_id merged_vertices[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_vec2 merged_uvs[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    size_t first_corner;
    size_t second_corner;
    size_t merged_count;
    size_t corner;
    henka_authoring_face_id primary_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_result result;

    modeling_report_reset(out_report);
    if (mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    edge = henka_authoring_mesh_get_edge(mesh, edge_id);
    if (edge == NULL || edge->face_count != 2U || edge->hard)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    first_face = henka_authoring_mesh_get_face(mesh, edge->faces[0]);
    second_face = henka_authoring_mesh_get_face(mesh, edge->faces[1]);
    if (first_face == NULL || second_face == NULL ||
        first_face->corner_count < 3U || second_face->corner_count < 3U ||
        first_face->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS ||
        second_face->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS ||
        first_face->material_region != second_face->material_region ||
        first_face->smooth != second_face->smooth ||
        !modeling_face_contains_edge(first_face, edge_id, &first_corner) ||
        !modeling_face_contains_edge(second_face, edge_id, &second_corner))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (first_face->vertices[first_corner] !=
            second_face->vertices[(second_corner + 1U) % second_face->corner_count] ||
        first_face->vertices[(first_corner + 1U) % first_face->corner_count] !=
            second_face->vertices[second_corner])
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!modeling_uv_near(
            first_face->uvs[first_corner],
            second_face->uvs[(second_corner + 1U) % second_face->corner_count]) ||
        !modeling_uv_near(
            first_face->uvs[(first_corner + 1U) % first_face->corner_count],
            second_face->uvs[second_corner]))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!henka_checked_size_add(
            first_face->corner_count, second_face->corner_count, &merged_count) ||
        merged_count < 2U)
    {
        return HENKA_ERROR_LIMIT;
    }
    merged_count -= 2U;
    if (merged_count < 3U ||
        merged_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
    {
        return HENKA_ERROR_LIMIT;
    }
    corner = 0U;
    while (corner < first_face->corner_count)
    {
        const size_t source_corner =
            (first_corner + 1U + corner) % first_face->corner_count;
        merged_vertices[corner] = first_face->vertices[source_corner];
        merged_uvs[corner] = first_face->uvs[source_corner];
        if (!isfinite(merged_uvs[corner].x) || !isfinite(merged_uvs[corner].y))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        ++corner;
    }
    {
        size_t source_corner = (second_corner + 2U) % second_face->corner_count;
        while (source_corner != second_corner)
        {
            if (corner >= merged_count)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            merged_vertices[corner] = second_face->vertices[source_corner];
            merged_uvs[corner] = second_face->uvs[source_corner];
            if (!isfinite(merged_uvs[corner].x) || !isfinite(merged_uvs[corner].y))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            ++corner;
            source_corner = (source_corner + 1U) % second_face->corner_count;
        }
    }
    if (corner != merged_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (corner = 0U; corner < merged_count; ++corner)
    {
        size_t prior;
        for (prior = 0U; prior < corner; ++prior)
        {
            if (merged_vertices[prior] == merged_vertices[corner])
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    primary_face_id = first_face->id;

    before = henka_authoring_mesh_get_counts(mesh);
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        updates[0].face_id = primary_face_id;
        updates[0].vertices = merged_vertices;
        updates[0].uvs = merged_uvs;
        updates[0].corner_count = merged_count;
        updates[0].material_region = first_face->material_region;
        updates[0].smooth = first_face->smooth;
        updates[1].face_id = second_face->id;
        updates[1].remove = true;
        result = henka_authoring_mesh_apply_face_loop_updates_internal(
            candidate, updates, 2U);
    }
    if (result == HENKA_SUCCESS &&
        (!henka_authoring_mesh_validate(candidate) ||
         !modeling_face_geometry_is_valid(candidate)))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        after = henka_authoring_mesh_get_counts(candidate);
        result = henka_authoring_mesh_copy(mesh, candidate);
        if (result == HENKA_SUCCESS)
        {
            modeling_report_count_delta(&before, &after, out_report);
            if (out_report != NULL)
            {
                out_report->primary_face_id = primary_face_id;
            }
        }
    }
    henka_authoring_mesh_destroy(candidate);
    return result;
}

henka_result henka_authoring_mesh_delete_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    const henka_authoring_edge* edge;
    henka_authoring_face_id incident_faces[2] = {
        HENKA_AUTHORING_INVALID_ID, HENKA_AUTHORING_INVALID_ID};
    size_t incident_count;
    size_t index;
    henka_result result;

    modeling_report_reset(out_report);
    if (mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    edge = henka_authoring_mesh_get_edge(mesh, edge_id);
    if (edge == NULL || edge->face_count == 0U || edge->face_count > 2U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    incident_count = edge->face_count;
    for (index = 0U; index < incident_count; ++index)
    {
        if (edge->faces[index] == HENKA_AUTHORING_INVALID_ID ||
            henka_authoring_mesh_get_face(mesh, edge->faces[index]) == NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        incident_faces[index] = edge->faces[index];
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (before.faces <= incident_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    for (index = 0U; result == HENKA_SUCCESS && index < incident_count; ++index)
    {
        result = henka_authoring_mesh_remove_face(candidate, incident_faces[index]);
    }
    if (result == HENKA_SUCCESS &&
        (!henka_authoring_mesh_validate(candidate) ||
         henka_authoring_mesh_get_counts(candidate).faces == 0U ||
         !modeling_face_geometry_is_valid(candidate)))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        after = henka_authoring_mesh_get_counts(candidate);
        result = henka_authoring_mesh_copy(mesh, candidate);
        if (result == HENKA_SUCCESS)
        {
            modeling_report_count_delta(&before, &after, out_report);
        }
    }
    henka_authoring_mesh_destroy(candidate);
    return result;
}

static size_t modeling_vertex_incident_face_count(
    const henka_authoring_mesh* mesh,
    henka_authoring_vertex_id vertex_id)
{
    const henka_authoring_mesh_desc desc = mesh != NULL
        ? henka_authoring_mesh_get_desc(mesh)
        : (henka_authoring_mesh_desc){0};
    size_t count = 0U;
    size_t face_slot;
    if (mesh == NULL) return 0U;
    for (face_slot = 0U; face_slot < desc.max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face;
        if (henka_authoring_mesh_get_face_id_at(mesh, face_slot, &face_id) != HENKA_SUCCESS)
        {
            continue;
        }
        face = henka_authoring_mesh_get_face(mesh, face_id);
        if (face != NULL && modeling_face_contains_vertex(face, vertex_id, NULL)) ++count;
    }
    return count;
}

static henka_authoring_edge_id modeling_find_edge_between_vertices(
    const henka_authoring_mesh* mesh,
    henka_authoring_vertex_id first_vertex_id,
    henka_authoring_vertex_id second_vertex_id)
{
    const henka_authoring_mesh_desc desc = mesh != NULL
        ? henka_authoring_mesh_get_desc(mesh)
        : (henka_authoring_mesh_desc){0};
    size_t edge_slot;
    if (mesh == NULL) return HENKA_AUTHORING_INVALID_ID;
    for (edge_slot = 0U; edge_slot < desc.max_edges; ++edge_slot)
    {
        henka_authoring_edge_id edge_id;
        const henka_authoring_edge* edge;
        if (henka_authoring_mesh_get_edge_id_at(mesh, edge_slot, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        edge = henka_authoring_mesh_get_edge(mesh, edge_id);
        if (edge != NULL &&
            ((edge->vertices[0] == first_vertex_id && edge->vertices[1] == second_vertex_id) ||
             (edge->vertices[0] == second_vertex_id && edge->vertices[1] == first_vertex_id)))
        {
            return edge_id;
        }
    }
    return HENKA_AUTHORING_INVALID_ID;
}

typedef struct modeling_edge_slide_vertex
{
    henka_authoring_vertex_id id;
    henka_authoring_vertex_id side_a;
    henka_authoring_vertex_id side_b;
    size_t selected_degree;
    bool has_side_a;
    bool has_side_b;
} modeling_edge_slide_vertex;

static size_t modeling_edge_slide_vertex_index(
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    henka_authoring_vertex_id vertex_id)
{
    size_t first = 0U;
    size_t last = vertex_count;
    while (first < last)
    {
        const size_t middle = first + (last - first) / 2U;
        if (vertex_ids[middle] < vertex_id)
        {
            first = middle + 1U;
        }
        else if (vertex_ids[middle] > vertex_id)
        {
            last = middle;
        }
        else
        {
            return middle;
        }
    }
    return SIZE_MAX;
}

static bool modeling_edge_slide_assign_side(
    modeling_edge_slide_vertex* vertex,
    bool side_a,
    henka_authoring_vertex_id side_id)
{
    henka_authoring_vertex_id* destination;
    bool* present;
    if (vertex == NULL || side_id == vertex->id)
    {
        return false;
    }
    destination = side_a ? &vertex->side_a : &vertex->side_b;
    present = side_a ? &vertex->has_side_a : &vertex->has_side_b;
    if (*present)
    {
        return *destination == side_id;
    }
    if ((side_a && vertex->has_side_b && vertex->side_b == side_id) ||
        (!side_a && vertex->has_side_a && vertex->side_a == side_id))
    {
        return false;
    }
    *destination = side_id;
    *present = true;
    return true;
}

static bool modeling_edge_slide_faces_share_uv(
    const henka_authoring_face* first,
    size_t first_corner,
    const henka_authoring_face* second,
    size_t second_corner)
{
    return first != NULL && second != NULL && first->corner_count == 4U &&
        second->corner_count == 4U &&
        first->vertices[first_corner] ==
            second->vertices[(second_corner + 1U) % 4U] &&
        first->vertices[(first_corner + 1U) % 4U] ==
            second->vertices[second_corner] &&
        modeling_uv_near(
            first->uvs[first_corner],
            second->uvs[(second_corner + 1U) % 4U]) &&
        modeling_uv_near(
            first->uvs[(first_corner + 1U) % 4U],
            second->uvs[second_corner]);
}

henka_result henka_authoring_mesh_slide_edge_loop(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    float factor,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_edge_id* sorted_edges = NULL;
    henka_authoring_vertex_id* raw_vertices = NULL;
    modeling_edge_slide_vertex* vertices = NULL;
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_authoring_mesh_desc desc;
    size_t vertex_count = 0U;
    size_t bytes;
    size_t index;
    size_t endpoint_count = 0U;
    size_t endpoint_vertices = 0U;
    size_t usable_vertex_count = 0U;
    size_t ordered_edge_count = 0U;
    bool ordered_closed = false;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    modeling_report_reset(out_report);
    if (mesh == NULL || edge_ids == NULL || edge_count == 0U ||
        !isfinite(factor) || factor <= -1.0f || factor >= 1.0f ||
        !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    desc = henka_authoring_mesh_get_desc(mesh);
    if (edge_count > desc.max_edges ||
        !henka_checked_size_multiply(edge_count, sizeof(*sorted_edges), &bytes) ||
        !henka_checked_size_multiply(edge_count, 2U, &endpoint_vertices) ||
        !henka_checked_size_multiply(
            endpoint_vertices, sizeof(*raw_vertices), &bytes))
    {
        return HENKA_ERROR_LIMIT;
    }
    sorted_edges = (henka_authoring_edge_id*)henka_malloc(
        edge_count * sizeof(*sorted_edges));
    raw_vertices = (henka_authoring_vertex_id*)henka_malloc(bytes);
    if (sorted_edges == NULL || raw_vertices == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    result = henka_authoring_topology_order_edge_loop(
        mesh, edge_ids, edge_count, sorted_edges, edge_count,
        &ordered_edge_count, &ordered_closed);
    if (result != HENKA_SUCCESS || ordered_edge_count != edge_count)
    {
        result = result == HENKA_SUCCESS ? HENKA_ERROR_INVALID_ARGUMENT : result;
        goto cleanup;
    }
    for (index = 0U; index < edge_count; ++index)
    {
        const henka_authoring_edge* edge =
            henka_authoring_mesh_get_edge(mesh, sorted_edges[index]);
        if (edge == NULL || edge->face_count != 2U || edge->hard ||
            (index > 0U && sorted_edges[index - 1U] == sorted_edges[index]))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        raw_vertices[endpoint_count++] = edge->vertices[0];
        raw_vertices[endpoint_count++] = edge->vertices[1];
    }
    qsort(raw_vertices, endpoint_count, sizeof(*raw_vertices), modeling_vertex_id_compare);
    for (index = 0U; index < endpoint_count; ++index)
    {
        if (vertex_count == 0U || raw_vertices[index] != raw_vertices[vertex_count - 1U])
        {
            raw_vertices[vertex_count++] = raw_vertices[index];
        }
    }
    if (vertex_count == 0U ||
        !henka_checked_size_multiply(vertex_count, sizeof(*vertices), &bytes))
    {
        result = HENKA_ERROR_LIMIT;
        goto cleanup;
    }
    vertices = (modeling_edge_slide_vertex*)henka_calloc(1U, bytes);
    if (vertices == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    for (index = 0U; index < vertex_count; ++index)
    {
        vertices[index].id = raw_vertices[index];
        vertices[index].side_a = HENKA_AUTHORING_INVALID_ID;
        vertices[index].side_b = HENKA_AUTHORING_INVALID_ID;
    }
    for (index = 0U; index < edge_count; ++index)
    {
        const henka_authoring_edge* edge =
            henka_authoring_mesh_get_edge(mesh, sorted_edges[index]);
        const henka_authoring_face* first_face;
        const henka_authoring_face* second_face;
        const henka_authoring_face* ordered_faces[2];
        size_t edge_corners[2] = {SIZE_MAX, SIZE_MAX};
        size_t endpoint_index;
        size_t side_index;
        if (edge == NULL)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        ordered_faces[0] = henka_authoring_mesh_get_face(
            mesh, edge->faces[0] < edge->faces[1] ? edge->faces[0] : edge->faces[1]);
        ordered_faces[1] = henka_authoring_mesh_get_face(
            mesh, edge->faces[0] < edge->faces[1] ? edge->faces[1] : edge->faces[0]);
        first_face = ordered_faces[0];
        second_face = ordered_faces[1];
        if (first_face == NULL || second_face == NULL ||
            !modeling_face_contains_edge(first_face, edge->id, &edge_corners[0]) ||
            !modeling_face_contains_edge(second_face, edge->id, &edge_corners[1]) ||
            first_face->material_region != second_face->material_region ||
            first_face->smooth != second_face->smooth ||
            !modeling_edge_slide_faces_share_uv(
                first_face, edge_corners[0], second_face, edge_corners[1]))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        for (side_index = 0U; side_index < 2U; ++side_index)
        {
            const bool side_a = side_index == 0U;
            const henka_authoring_face* face = side_a ? first_face : second_face;
            const size_t corner = side_a ? edge_corners[0] : edge_corners[1];
            for (endpoint_index = 0U; endpoint_index < 2U; ++endpoint_index)
            {
                const henka_authoring_vertex_id endpoint = edge->vertices[endpoint_index];
                const henka_authoring_vertex_id side_vertex =
                    face->vertices[corner] == endpoint
                        ? face->vertices[(corner + 3U) % 4U]
                        : face->vertices[(corner + 2U) % 4U];
                const size_t vertex_index = modeling_edge_slide_vertex_index(
                    raw_vertices, vertex_count, endpoint);
                if (vertex_index == SIZE_MAX ||
                    !modeling_edge_slide_assign_side(
                        &vertices[vertex_index], side_a, side_vertex))
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (side_index == 1U)
                {
                    ++vertices[vertex_index].selected_degree;
                }
            }
        }
    }
    for (index = 0U; index < vertex_count; ++index)
    {
        if (vertices[index].selected_degree == 0U ||
            vertices[index].selected_degree > 2U ||
            !vertices[index].has_side_a || !vertices[index].has_side_b ||
            modeling_edge_slide_vertex_index(
                raw_vertices, vertex_count, vertices[index].side_a) != SIZE_MAX ||
            modeling_edge_slide_vertex_index(
                raw_vertices, vertex_count, vertices[index].side_b) != SIZE_MAX)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        ++usable_vertex_count;
    }
    if (usable_vertex_count < 2U)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    {
        size_t open_endpoints = 0U;
        for (index = 0U; index < vertex_count; ++index)
        {
            if (vertices[index].selected_degree == 1U) ++open_endpoints;
        }
        if ((ordered_closed && open_endpoints != 0U) ||
            (!ordered_closed && open_endpoints != 2U))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }
    before = henka_authoring_mesh_get_counts(mesh);
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result != HENKA_SUCCESS) goto cleanup;
    for (index = 0U; index < vertex_count; ++index)
    {
        const henka_authoring_vertex* current =
            henka_authoring_mesh_get_vertex(mesh, vertices[index].id);
        const henka_authoring_vertex* side = henka_authoring_mesh_get_vertex(
            mesh, factor >= 0.0f ? vertices[index].side_b : vertices[index].side_a);
        const henka_vec3 delta = side != NULL && current != NULL
            ? henka_vec3_subtract(side->position, current->position)
            : (henka_vec3){0.0f, 0.0f, 0.0f};
        const henka_vec3 position = current != NULL
            ? henka_vec3_add(
                current->position,
                henka_vec3_scale(delta, fabsf(factor)))
            : (henka_vec3){0.0f, 0.0f, 0.0f};
        if (current == NULL || side == NULL || !modeling_finite_vec3(position))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        result = henka_authoring_mesh_set_vertex_position(
            candidate, vertices[index].id, position);
        if (result != HENKA_SUCCESS) goto cleanup;
    }
    if (!henka_authoring_mesh_validate(candidate) ||
        !modeling_face_geometry_is_valid(candidate))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(candidate);
    result = henka_authoring_mesh_copy(mesh, candidate);
    if (result == HENKA_SUCCESS && out_report != NULL)
    {
        out_report->changed = true;
        out_report->created_vertices = after.vertices > before.vertices
            ? after.vertices - before.vertices : 0U;
        out_report->removed_vertices = before.vertices > after.vertices
            ? before.vertices - after.vertices : 0U;
        out_report->created_edges = after.edges > before.edges
            ? after.edges - before.edges : 0U;
        out_report->removed_edges = before.edges > after.edges
            ? before.edges - after.edges : 0U;
        out_report->created_faces = after.faces > before.faces
            ? after.faces - before.faces : 0U;
        out_report->removed_faces = before.faces > after.faces
            ? before.faces - after.faces : 0U;
        out_report->primary_edge_id = sorted_edges[0];
    }

cleanup:
    henka_authoring_mesh_destroy(candidate);
    henka_free(vertices);
    henka_free(raw_vertices);
    henka_free(sorted_edges);
    return result;
}

static henka_result modeling_bevel_interior_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    float width,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_mesh* candidate = NULL;
    const henka_authoring_edge* edge;
    const henka_authoring_face* faces[2];
    henka_authoring_face_loop_update updates[2] = {{0}};
    henka_authoring_vertex_id endpoints[2];
    henka_authoring_vertex_id cut_vertices[2][2] = {{
        HENKA_AUTHORING_INVALID_ID, HENKA_AUTHORING_INVALID_ID}, {
        HENKA_AUTHORING_INVALID_ID, HENKA_AUTHORING_INVALID_ID}};
    henka_vec2 cut_uvs[2][2];
    henka_vec3 cut_positions[2][2];
    henka_authoring_vertex_id updated_vertices[2][HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_vec2 updated_uvs[2][HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id bevel_vertices[4];
    henka_vec2 bevel_uvs[4];
    henka_authoring_face_id bevel_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    size_t edge_corners[2] = {SIZE_MAX, SIZE_MAX};
    size_t face_index;
    size_t corner;
    size_t endpoint_index;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    modeling_report_reset(out_report);
    if (mesh == NULL || !isfinite(width) || width <= 0.0f ||
        !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    edge = henka_authoring_mesh_get_edge(mesh, edge_id);
    if (edge == NULL || edge->face_count != 2U || edge->hard ||
        edge->vertices[0] == edge->vertices[1])
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    faces[0] = henka_authoring_mesh_get_face(mesh, edge->faces[0]);
    faces[1] = henka_authoring_mesh_get_face(mesh, edge->faces[1]);
    if (faces[0] == NULL || faces[1] == NULL || faces[0]->corner_count != 4U ||
        faces[1]->corner_count != 4U ||
        faces[0]->material_region != faces[1]->material_region ||
        faces[0]->smooth != faces[1]->smooth ||
        !modeling_face_contains_edge(faces[0], edge_id, &edge_corners[0]) ||
        !modeling_face_contains_edge(faces[1], edge_id, &edge_corners[1]))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    endpoints[0] = faces[0]->vertices[edge_corners[0]];
    endpoints[1] = faces[0]->vertices[(edge_corners[0] + 1U) % 4U];
    if (faces[1]->vertices[edge_corners[1]] != endpoints[1] ||
        faces[1]->vertices[(edge_corners[1] + 1U) % 4U] != endpoints[0] ||
        !modeling_uv_near(
            faces[0]->uvs[edge_corners[0]],
            faces[1]->uvs[(edge_corners[1] + 1U) % 4U]) ||
        !modeling_uv_near(
            faces[0]->uvs[(edge_corners[0] + 1U) % 4U],
            faces[1]->uvs[edge_corners[1]]) ||
        modeling_vertex_incident_face_count(mesh, endpoints[0]) != 2U ||
        modeling_vertex_incident_face_count(mesh, endpoints[1]) != 2U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (face_index = 0U; face_index < 2U; ++face_index)
    {
        for (corner = 0U; corner < 4U; ++corner)
        {
            const henka_authoring_edge* adjacent = henka_authoring_mesh_get_edge(
                mesh, faces[face_index]->edges[corner]);
            if (adjacent == NULL || (corner != edge_corners[face_index] &&
                (adjacent->face_count != 1U || adjacent->faces[0] != faces[face_index]->id)))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    for (face_index = 0U; face_index < 2U; ++face_index)
    {
        for (endpoint_index = 0U; endpoint_index < 2U; ++endpoint_index)
        {
            const size_t endpoint_corner = endpoint_index == 0U
                ? (face_index == 0U ? edge_corners[0] : (edge_corners[1] + 1U) % 4U)
                : (face_index == 0U ? (edge_corners[0] + 1U) % 4U : edge_corners[1]);
            const bool edge_start = endpoint_corner == edge_corners[face_index];
            const size_t neighbor_corner = edge_start
                ? (endpoint_corner + 3U) % 4U
                : (endpoint_corner + 1U) % 4U;
            const henka_authoring_vertex* endpoint = henka_authoring_mesh_get_vertex(
                mesh, endpoints[endpoint_index]);
            const henka_authoring_vertex* neighbor = henka_authoring_mesh_get_vertex(
                mesh, faces[face_index]->vertices[neighbor_corner]);
            const henka_vec3 delta = endpoint != NULL && neighbor != NULL
                ? henka_vec3_subtract(neighbor->position, endpoint->position)
                : (henka_vec3){0.0f, 0.0f, 0.0f};
            const float length = henka_vec3_length(delta);
            if (endpoint == NULL || neighbor == NULL || !isfinite(length) ||
                length <= 1.0e-7f || !(width < length - 1.0e-5f))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            cut_positions[face_index][endpoint_index] = henka_vec3_add(
                endpoint->position, henka_vec3_scale(delta, width / length));
            cut_uvs[face_index][endpoint_index] = (henka_vec2){
                faces[face_index]->uvs[endpoint_corner].x +
                    (faces[face_index]->uvs[neighbor_corner].x -
                        faces[face_index]->uvs[endpoint_corner].x) * (width / length),
                faces[face_index]->uvs[endpoint_corner].y +
                    (faces[face_index]->uvs[neighbor_corner].y -
                        faces[face_index]->uvs[endpoint_corner].y) * (width / length)};
            if (!isfinite(cut_positions[face_index][endpoint_index].x) ||
                !isfinite(cut_positions[face_index][endpoint_index].y) ||
                !isfinite(cut_positions[face_index][endpoint_index].z) ||
                !isfinite(cut_uvs[face_index][endpoint_index].x) ||
                !isfinite(cut_uvs[face_index][endpoint_index].y))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    before = henka_authoring_mesh_get_counts(mesh);
    {
        const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
        if (desc.max_vertices < before.vertices || desc.max_faces < before.faces ||
            desc.max_vertices - before.vertices < 4U || desc.max_faces == before.faces)
        {
            return HENKA_ERROR_LIMIT;
        }
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result != HENKA_SUCCESS) goto cleanup;
    for (face_index = 0U; face_index < 2U; ++face_index)
    {
        for (endpoint_index = 0U; endpoint_index < 2U; ++endpoint_index)
        {
            const henka_authoring_vertex* endpoint = henka_authoring_mesh_get_vertex(
                mesh, endpoints[endpoint_index]);
            result = henka_authoring_mesh_add_vertex(
                candidate, cut_positions[face_index][endpoint_index],
                cut_uvs[face_index][endpoint_index], endpoint->material_region,
                &cut_vertices[face_index][endpoint_index]);
            if (result != HENKA_SUCCESS) goto cleanup;
        }
        memcpy(updated_vertices[face_index], faces[face_index]->vertices,
            4U * sizeof(*updated_vertices[face_index]));
        memcpy(updated_uvs[face_index], faces[face_index]->uvs,
            4U * sizeof(*updated_uvs[face_index]));
        for (corner = 0U; corner < 4U; ++corner)
        {
            if (updated_vertices[face_index][corner] == endpoints[0])
            {
                updated_vertices[face_index][corner] = cut_vertices[face_index][0];
                updated_uvs[face_index][corner] = cut_uvs[face_index][0];
            }
            else if (updated_vertices[face_index][corner] == endpoints[1])
            {
                updated_vertices[face_index][corner] = cut_vertices[face_index][1];
                updated_uvs[face_index][corner] = cut_uvs[face_index][1];
            }
        }
        updates[face_index].face_id = faces[face_index]->id;
        updates[face_index].vertices = updated_vertices[face_index];
        updates[face_index].uvs = updated_uvs[face_index];
        updates[face_index].corner_count = 4U;
        updates[face_index].material_region = faces[face_index]->material_region;
        updates[face_index].smooth = faces[face_index]->smooth;
    }
    result = henka_authoring_mesh_apply_face_loop_updates_internal(candidate, updates, 2U);
    if (result != HENKA_SUCCESS) goto cleanup;
    bevel_vertices[0] = cut_vertices[0][0];
    bevel_vertices[1] = cut_vertices[0][1];
    bevel_vertices[2] = cut_vertices[1][1];
    bevel_vertices[3] = cut_vertices[1][0];
    bevel_uvs[0] = cut_uvs[0][0];
    bevel_uvs[1] = cut_uvs[0][1];
    bevel_uvs[2] = cut_uvs[1][1];
    bevel_uvs[3] = cut_uvs[1][0];
    result = henka_authoring_mesh_add_face(
        candidate, bevel_vertices, 4U, faces[0]->material_region,
        faces[0]->smooth, &bevel_face_id);
    if (result != HENKA_SUCCESS) goto cleanup;
    for (corner = 0U; corner < 4U; ++corner)
    {
        result = henka_authoring_mesh_set_face_corner_uv(
            candidate, bevel_face_id, corner, bevel_uvs[corner]);
        if (result != HENKA_SUCCESS) goto cleanup;
    }
    for (endpoint_index = 0U; endpoint_index < 2U; ++endpoint_index)
    {
        result = henka_authoring_mesh_remove_vertex(candidate, endpoints[endpoint_index]);
        if (result != HENKA_SUCCESS) goto cleanup;
    }
    if (!henka_authoring_mesh_validate(candidate) ||
        !modeling_face_geometry_is_valid(candidate))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(candidate);
    result = henka_authoring_mesh_copy(mesh, candidate);
    if (result == HENKA_SUCCESS)
    {
        modeling_report_count_delta(&before, &after, out_report);
        if (out_report != NULL)
        {
            out_report->primary_face_id = bevel_face_id;
            out_report->primary_edge_id = modeling_find_edge_between_vertices(
                mesh, cut_vertices[0][0], cut_vertices[0][1]);
        }
    }

cleanup:
    henka_authoring_mesh_destroy(candidate);
    return result;
}

henka_result henka_authoring_mesh_bevel_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    float width,
    henka_authoring_modeling_report* out_report)
{
    return henka_authoring_mesh_bevel_edges(mesh, &edge_id, 1U, width, out_report);
}

typedef struct modeling_boundary_bevel_cut
{
    henka_authoring_edge_id edge_id;
    henka_authoring_vertex_id start_vertex;
    henka_authoring_vertex_id end_vertex;
    henka_authoring_vertex_id cut_start;
    henka_authoring_vertex_id cut_end;
    size_t start_corner;
    size_t end_corner;
    henka_vec3 cut_start_position;
    henka_vec3 cut_end_position;
    henka_vec2 cut_start_uv;
    henka_vec2 cut_end_uv;
} modeling_boundary_bevel_cut;

static henka_result modeling_bevel_boundary_edges_in_place(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    henka_authoring_face_id source_face_id,
    float width,
    henka_authoring_face_id* out_face_id,
    henka_authoring_edge_id* out_edge_id)
{
    modeling_boundary_bevel_cut cuts[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id source_vertices[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_vec2 source_uvs[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id updated_vertices[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_vec2 updated_uvs[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    size_t incoming[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    size_t outgoing[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    const henka_authoring_face* source_face;
    size_t source_corner_count;
    size_t updated_count = 0U;
    size_t index;
    size_t corner;
    uint32_t source_material_region;
    bool source_smooth;
    henka_authoring_face_id first_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id first_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_result result;

    if (out_face_id != NULL) *out_face_id = HENKA_AUTHORING_INVALID_ID;
    if (out_edge_id != NULL) *out_edge_id = HENKA_AUTHORING_INVALID_ID;
    if (mesh == NULL || edge_ids == NULL || edge_count == 0U ||
        edge_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS ||
        !isfinite(width) || width <= 0.0f ||
        !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    source_face = henka_authoring_mesh_get_face(mesh, source_face_id);
    if (source_face == NULL || !source_face->active || source_face->corner_count < 3U ||
        source_face->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    source_corner_count = source_face->corner_count;
    source_material_region = source_face->material_region;
    source_smooth = source_face->smooth;
    memcpy(source_vertices, source_face->vertices,
        source_corner_count * sizeof(*source_vertices));
    memcpy(source_uvs, source_face->uvs,
        source_corner_count * sizeof(*source_uvs));
    for (corner = 0U; corner < source_corner_count; ++corner)
    {
        incoming[corner] = SIZE_MAX;
        outgoing[corner] = SIZE_MAX;
    }

    for (index = 0U; index < edge_count; ++index)
    {
        const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
            mesh, edge_ids[index]);
        bool oriented = false;
        size_t start_corner = SIZE_MAX;
        size_t end_corner = SIZE_MAX;
        size_t previous_corner;
        size_t next_corner;
        const henka_authoring_vertex* start_vertex;
        const henka_authoring_vertex* end_vertex;
        const henka_authoring_vertex* previous_vertex;
        const henka_authoring_vertex* next_vertex;
        float start_length;
        float end_length;
        float start_factor;
        float end_factor;

        if (edge == NULL || edge->face_count != 1U ||
            edge->faces[0] != source_face_id || edge->vertices[0] == edge->vertices[1])
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (corner = 0U; corner < source_corner_count; ++corner)
        {
            const henka_authoring_vertex_id current = source_vertices[corner];
            const henka_authoring_vertex_id next = source_vertices[
                (corner + 1U) % source_corner_count];
            if ((current == edge->vertices[0] && next == edge->vertices[1]) ||
                (current == edge->vertices[1] && next == edge->vertices[0]))
            {
                start_corner = corner;
                end_corner = (corner + 1U) % source_corner_count;
                oriented = true;
                break;
            }
        }
        if (!oriented || outgoing[start_corner] != SIZE_MAX || incoming[end_corner] != SIZE_MAX)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        previous_corner = (start_corner + source_corner_count - 1U) % source_corner_count;
        next_corner = (end_corner + 1U) % source_corner_count;
        start_vertex = henka_authoring_mesh_get_vertex(mesh, source_vertices[start_corner]);
        end_vertex = henka_authoring_mesh_get_vertex(mesh, source_vertices[end_corner]);
        previous_vertex = henka_authoring_mesh_get_vertex(mesh, source_vertices[previous_corner]);
        next_vertex = henka_authoring_mesh_get_vertex(mesh, source_vertices[next_corner]);
        start_length = start_vertex != NULL && previous_vertex != NULL
            ? henka_vec3_length(henka_vec3_subtract(
                previous_vertex->position, start_vertex->position)) : 0.0f;
        end_length = end_vertex != NULL && next_vertex != NULL
            ? henka_vec3_length(henka_vec3_subtract(
                next_vertex->position, end_vertex->position)) : 0.0f;
        if (!isfinite(start_length) || !isfinite(end_length) ||
            start_length <= 1.0e-7f || end_length <= 1.0e-7f ||
            !(width < start_length - 1.0e-5f) ||
            !(width < end_length - 1.0e-5f))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        start_factor = width / start_length;
        end_factor = width / end_length;
        cuts[index].edge_id = edge->id;
        cuts[index].start_vertex = source_vertices[start_corner];
        cuts[index].end_vertex = source_vertices[end_corner];
        cuts[index].start_corner = start_corner;
        cuts[index].end_corner = end_corner;
        cuts[index].cut_start_position = henka_vec3_add(
            start_vertex->position,
            henka_vec3_scale(henka_vec3_subtract(
                previous_vertex->position, start_vertex->position), start_factor));
        cuts[index].cut_end_position = henka_vec3_add(
            end_vertex->position,
            henka_vec3_scale(henka_vec3_subtract(
                next_vertex->position, end_vertex->position), end_factor));
        cuts[index].cut_start_uv = (henka_vec2){
            source_uvs[start_corner].x +
                (source_uvs[previous_corner].x - source_uvs[start_corner].x) * start_factor,
            source_uvs[start_corner].y +
                (source_uvs[previous_corner].y - source_uvs[start_corner].y) * start_factor};
        cuts[index].cut_end_uv = (henka_vec2){
            source_uvs[end_corner].x +
                (source_uvs[next_corner].x - source_uvs[end_corner].x) * end_factor,
            source_uvs[end_corner].y +
                (source_uvs[next_corner].y - source_uvs[end_corner].y) * end_factor};
        outgoing[start_corner] = index;
        incoming[end_corner] = index;
    }

    for (index = 0U; index < edge_count; ++index)
    {
        result = henka_authoring_mesh_add_vertex(
            mesh, cuts[index].cut_start_position, cuts[index].cut_start_uv,
            henka_authoring_mesh_get_vertex(mesh, cuts[index].start_vertex)->material_region,
            &cuts[index].cut_start);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        result = henka_authoring_mesh_add_vertex(
            mesh, cuts[index].cut_end_position, cuts[index].cut_end_uv,
            henka_authoring_mesh_get_vertex(mesh, cuts[index].end_vertex)->material_region,
            &cuts[index].cut_end);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    for (corner = 0U; corner < source_corner_count; ++corner)
    {
        if (incoming[corner] != SIZE_MAX)
        {
            if (updated_count >= HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
            {
                return HENKA_ERROR_LIMIT;
            }
            updated_vertices[updated_count] = cuts[incoming[corner]].cut_end;
            updated_uvs[updated_count++] = cuts[incoming[corner]].cut_end_uv;
        }
        if (outgoing[corner] != SIZE_MAX)
        {
            if (updated_count >= HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
            {
                return HENKA_ERROR_LIMIT;
            }
            updated_vertices[updated_count] = cuts[outgoing[corner]].cut_start;
            updated_uvs[updated_count++] = cuts[outgoing[corner]].cut_start_uv;
        }
        if (incoming[corner] == SIZE_MAX && outgoing[corner] == SIZE_MAX)
        {
            if (updated_count >= HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
            {
                return HENKA_ERROR_LIMIT;
            }
            updated_vertices[updated_count] = source_vertices[corner];
            updated_uvs[updated_count++] = source_uvs[corner];
        }
    }
    if (updated_count < 3U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    {
        henka_authoring_face_loop_update update = {0};
        update.face_id = source_face_id;
        update.vertices = updated_vertices;
        update.uvs = updated_uvs;
        update.corner_count = updated_count;
        update.material_region = source_material_region;
        update.smooth = source_smooth;
        result = henka_authoring_mesh_apply_face_loop_updates_internal(mesh, &update, 1U);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    for (index = 0U; index < edge_count; ++index)
    {
        const henka_authoring_vertex_id vertices[4] = {
            cuts[index].start_vertex, cuts[index].end_vertex,
            cuts[index].cut_end, cuts[index].cut_start};
        const henka_vec2 uvs[4] = {
            source_uvs[cuts[index].start_corner], source_uvs[cuts[index].end_corner],
            cuts[index].cut_end_uv, cuts[index].cut_start_uv};
        henka_authoring_face_id bevel_face_id = HENKA_AUTHORING_INVALID_ID;
        size_t uv_corner;
        result = henka_authoring_mesh_add_face(
            mesh, vertices, 4U, source_material_region, source_smooth, &bevel_face_id);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        for (uv_corner = 0U; uv_corner < 4U; ++uv_corner)
        {
            result = henka_authoring_mesh_set_face_corner_uv(
                mesh, bevel_face_id, uv_corner, uvs[uv_corner]);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
        }
        if (index == 0U)
        {
            first_face_id = bevel_face_id;
            first_edge_id = modeling_find_edge_between_vertices(
                mesh, cuts[index].cut_start, cuts[index].cut_end);
        }
    }
    for (corner = 0U; corner < source_corner_count; ++corner)
    {
        const size_t incoming_index = incoming[corner];
        const size_t outgoing_index = outgoing[corner];
        if (incoming_index != SIZE_MAX && outgoing_index != SIZE_MAX)
        {
            const henka_authoring_vertex_id vertices[3] = {
                source_vertices[corner], cuts[outgoing_index].cut_start,
                cuts[incoming_index].cut_end};
            const henka_vec2 uvs[3] = {
                source_uvs[corner], cuts[outgoing_index].cut_start_uv,
                cuts[incoming_index].cut_end_uv};
            henka_authoring_face_id cap_face_id = HENKA_AUTHORING_INVALID_ID;
            size_t uv_corner;
            result = henka_authoring_mesh_add_face(
                mesh, vertices, 3U, source_material_region, source_smooth, &cap_face_id);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            for (uv_corner = 0U; uv_corner < 3U; ++uv_corner)
            {
                result = henka_authoring_mesh_set_face_corner_uv(
                    mesh, cap_face_id, uv_corner, uvs[uv_corner]);
                if (result != HENKA_SUCCESS)
                {
                    return result;
                }
            }
        }
    }
    if (!henka_authoring_mesh_validate(mesh) || !modeling_face_geometry_is_valid(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (out_face_id != NULL) *out_face_id = first_face_id;
    if (out_edge_id != NULL) *out_edge_id = first_edge_id;
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_bevel_edges(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    float width,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_edge_id* sorted_edges = NULL;
    henka_authoring_vertex_id* edge_endpoints = NULL;
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_authoring_mesh_desc desc;
    henka_authoring_modeling_report first_report = {0};
    size_t edge_bytes;
    size_t endpoint_bytes;
    size_t required_vertices;
    size_t required_edges;
    size_t index;
    size_t other_index;
    henka_authoring_face_id common_face_id = HENKA_AUTHORING_INVALID_ID;
    bool all_same_face = true;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    modeling_report_reset(out_report);
    if (mesh == NULL || edge_ids == NULL || edge_count == 0U ||
        !isfinite(width) || width <= 0.0f ||
        !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (edge_count == 1U)
    {
        const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
            mesh, edge_ids[0]);
        if (edge != NULL && edge->face_count == 2U)
        {
            return modeling_bevel_interior_edge(
                mesh, edge_ids[0], width, out_report);
        }
    }
    desc = henka_authoring_mesh_get_desc(mesh);
    if (edge_count > desc.max_edges ||
        !henka_checked_size_multiply(edge_count, sizeof(*sorted_edges), &edge_bytes) ||
        !henka_checked_size_multiply(edge_count, 2U, &required_vertices) ||
        !henka_checked_size_multiply(edge_count, 3U, &required_edges) ||
        !henka_checked_size_multiply(
            required_vertices, sizeof(*edge_endpoints), &endpoint_bytes))
    {
        return HENKA_ERROR_LIMIT;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (desc.max_vertices < before.vertices || desc.max_faces < before.faces ||
        desc.max_vertices - before.vertices < required_vertices ||
        desc.max_edges < before.edges ||
        desc.max_edges - before.edges < required_edges ||
        desc.max_faces - before.faces < edge_count)
    {
        return HENKA_ERROR_LIMIT;
    }
    sorted_edges = (henka_authoring_edge_id*)henka_malloc(edge_bytes);
    edge_endpoints = (henka_authoring_vertex_id*)henka_malloc(endpoint_bytes);
    if (sorted_edges == NULL || edge_endpoints == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    memcpy(sorted_edges, edge_ids, edge_bytes);
    qsort(sorted_edges, edge_count, sizeof(*sorted_edges), modeling_vertex_id_compare);
    for (index = 0U; index < edge_count; ++index)
    {
        const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
            mesh, sorted_edges[index]);
        if (edge == NULL || edge->face_count != 1U ||
            edge->vertices[0] == edge->vertices[1] ||
            henka_authoring_mesh_get_vertex(mesh, edge->vertices[0]) == NULL ||
            henka_authoring_mesh_get_vertex(mesh, edge->vertices[1]) == NULL ||
            modeling_vertex_incident_face_count(mesh, edge->vertices[0]) != 1U ||
            modeling_vertex_incident_face_count(mesh, edge->vertices[1]) != 1U ||
            (index > 0U && sorted_edges[index - 1U] == sorted_edges[index]))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        edge_endpoints[index * 2U] = edge->vertices[0];
        edge_endpoints[index * 2U + 1U] = edge->vertices[1];
        if (index == 0U)
        {
            common_face_id = edge->faces[0];
        }
        else if (edge->faces[0] != common_face_id)
        {
            all_same_face = false;
        }
    }
    if (!all_same_face)
    {
        for (index = 0U; index < edge_count; ++index)
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                mesh, sorted_edges[index]);
            for (other_index = 0U; other_index < index; ++other_index)
            {
                const henka_authoring_edge* other = henka_authoring_mesh_get_edge(
                    mesh, sorted_edges[other_index]);
                if (edge == NULL || other == NULL || edge->faces[0] == other->faces[0] ||
                    edge->vertices[0] == other->vertices[0] ||
                    edge->vertices[0] == other->vertices[1] ||
                    edge->vertices[1] == other->vertices[0] ||
                    edge->vertices[1] == other->vertices[1])
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                    goto cleanup;
                }
            }
        }
    }
    if (all_same_face && edge_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
    {
        result = HENKA_ERROR_LIMIT;
        goto cleanup;
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (all_same_face)
    {
        henka_authoring_face_id step_face_id = HENKA_AUTHORING_INVALID_ID;
        henka_authoring_edge_id step_edge_id = HENKA_AUTHORING_INVALID_ID;
        result = modeling_bevel_boundary_edges_in_place(
            candidate, sorted_edges, edge_count, common_face_id, width,
            &step_face_id, &step_edge_id);
        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        first_report.primary_face_id = step_face_id;
        first_report.primary_edge_id = step_edge_id;
    }
    else
    {
        for (index = 0U; index < edge_count; ++index)
        {
            henka_authoring_face_id step_face_id = HENKA_AUTHORING_INVALID_ID;
            henka_authoring_edge_id step_edge_id = HENKA_AUTHORING_INVALID_ID;
            const henka_authoring_edge* current_edge;
            const henka_authoring_edge_id current_edge_id =
                modeling_find_edge_between_vertices(
                    candidate, edge_endpoints[index * 2U], edge_endpoints[index * 2U + 1U]);
            if (current_edge_id == HENKA_AUTHORING_INVALID_ID)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            current_edge = henka_authoring_mesh_get_edge(candidate, current_edge_id);
            if (current_edge == NULL || current_edge->face_count != 1U)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            result = modeling_bevel_boundary_edges_in_place(
                candidate, &current_edge_id, 1U, current_edge->faces[0], width,
                &step_face_id, &step_edge_id);
            if (result != HENKA_SUCCESS)
            {
                goto cleanup;
            }
            if (index == 0U)
            {
                first_report.primary_face_id = step_face_id;
                first_report.primary_edge_id = step_edge_id;
            }
        }
    }
    if (!henka_authoring_mesh_validate(candidate) ||
        !modeling_face_geometry_is_valid(candidate))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(candidate);
    result = henka_authoring_mesh_copy(mesh, candidate);
    if (result == HENKA_SUCCESS)
    {
        modeling_report_count_delta(&before, &after, out_report);
        if (out_report != NULL)
        {
            out_report->primary_vertex_id = first_report.primary_vertex_id;
            out_report->primary_edge_id = first_report.primary_edge_id;
            out_report->primary_face_id = first_report.primary_face_id;
        }
    }

cleanup:
    henka_authoring_mesh_destroy(candidate);
    henka_free(edge_endpoints);
    henka_free(sorted_edges);
    return result;
}

typedef struct modeling_bevel_cut
{
    henka_authoring_edge_id edge_id;
    henka_authoring_vertex_id endpoint;
    henka_authoring_vertex_id other_endpoint;
    henka_authoring_vertex_id vertex_id;
    henka_vec3 position;
    henka_vec2 uv_sum;
    size_t uv_samples;
    float factor;
} modeling_bevel_cut;

typedef struct modeling_bevel_face_work
{
    henka_authoring_face_loop_update update;
    henka_authoring_vertex_id* vertices;
    henka_vec2* uvs;
} modeling_bevel_face_work;

typedef struct modeling_bevel_cap
{
    henka_authoring_vertex_id* vertices;
    henka_vec2* uvs;
    size_t count;
    uint32_t material_region;
    henka_authoring_face_id face_id;
} modeling_bevel_cap;

static bool modeling_bevel_selected_contains(
    const henka_authoring_vertex_id* selected,
    size_t selected_count,
    henka_authoring_vertex_id vertex_id)
{
    size_t low = 0U;
    size_t high = selected_count;
    while (low < high)
    {
        const size_t middle = low + (high - low) / 2U;
        if (selected[middle] == vertex_id) return true;
        if (selected[middle] < vertex_id) low = middle + 1U;
        else high = middle;
    }
    return false;
}

static modeling_bevel_cut* modeling_bevel_find_cut(
    modeling_bevel_cut* cuts,
    size_t cut_count,
    henka_authoring_edge_id edge_id,
    henka_authoring_vertex_id endpoint)
{
    size_t index;
    for (index = 0U; index < cut_count; ++index)
    {
        if (cuts[index].edge_id == edge_id && cuts[index].endpoint == endpoint)
        {
            return &cuts[index];
        }
    }
    return NULL;
}

static bool modeling_bevel_append_neighbor(
    henka_authoring_vertex_id* neighbors,
    size_t* neighbor_count,
    size_t neighbor_capacity,
    henka_authoring_vertex_id vertex_id)
{
    size_t index;
    if (neighbors == NULL || neighbor_count == NULL || *neighbor_count >= neighbor_capacity)
    {
        return false;
    }
    for (index = 0U; index < *neighbor_count; ++index)
    {
        if (neighbors[index] == vertex_id) return true;
    }
    neighbors[(*neighbor_count)++] = vertex_id;
    return true;
}

static henka_result modeling_bevel_build_link_order(
    const henka_authoring_mesh* mesh,
    henka_authoring_vertex_id vertex_id,
    modeling_bevel_cut* cuts,
    size_t cut_count,
    henka_authoring_vertex_id* out_cut_vertices,
    size_t cut_capacity,
    size_t* out_cut_count,
    bool* out_boundary,
    uint32_t* out_material_region)
{
    const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
    henka_authoring_vertex_id* neighbors = NULL;
    henka_authoring_vertex_id* link_first = NULL;
    henka_authoring_vertex_id* link_second = NULL;
    size_t* degrees = NULL;
    size_t neighbor_count = 0U;
    size_t link_count = 0U;
    size_t incident_count = 0U;
    size_t face_slot;
    size_t index;
    size_t degree_one_count = 0U;
    size_t start = SIZE_MAX;
    size_t current;
    size_t previous = SIZE_MAX;
    bool boundary;
    bool material_initialized = false;
    bool material_mixed = false;
    uint32_t material_region = 0U;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    if (out_cut_count != NULL) *out_cut_count = 0U;
    if (out_boundary != NULL) *out_boundary = false;
    if (out_material_region != NULL) *out_material_region = 0U;
    if (mesh == NULL || out_cut_vertices == NULL || out_cut_count == NULL ||
        out_boundary == NULL || out_material_region == NULL || cut_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    neighbors = henka_calloc(desc.max_edges, sizeof(*neighbors));
    link_first = henka_calloc(desc.max_faces, sizeof(*link_first));
    link_second = henka_calloc(desc.max_faces, sizeof(*link_second));
    degrees = henka_calloc(desc.max_edges, sizeof(*degrees));
    if (neighbors == NULL || link_first == NULL || link_second == NULL || degrees == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    for (face_slot = 0U; face_slot < desc.max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face = henka_authoring_mesh_get_face_id_at(mesh, face_slot, &face_id) == HENKA_SUCCESS
            ? henka_authoring_mesh_get_face(mesh, face_id)
            : NULL;
        size_t corner;
        if (face == NULL || !modeling_face_contains_vertex(face, vertex_id, &corner)) continue;
        ++incident_count;
        if (!material_initialized)
        {
            material_region = face->material_region;
            material_initialized = true;
        }
        else if (material_region != face->material_region) material_mixed = true;
        if (!modeling_bevel_append_neighbor(
                neighbors, &neighbor_count, desc.max_edges,
                face->vertices[(corner + face->corner_count - 1U) % face->corner_count]) ||
            !modeling_bevel_append_neighbor(
                neighbors, &neighbor_count, desc.max_edges,
                face->vertices[(corner + 1U) % face->corner_count]) ||
            link_count >= desc.max_faces)
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        link_first[link_count] = face->vertices[(corner + face->corner_count - 1U) % face->corner_count];
        link_second[link_count] = face->vertices[(corner + 1U) % face->corner_count];
        ++link_count;
    }
    if (incident_count == 0U || neighbor_count < 2U || link_count != incident_count)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    for (index = 0U; index < link_count; ++index)
    {
        size_t neighbor_index;
        bool first_found = false;
        bool second_found = false;
        for (neighbor_index = 0U; neighbor_index < neighbor_count; ++neighbor_index)
        {
            if (neighbors[neighbor_index] == link_first[index])
            {
                if (first_found) { result = HENKA_ERROR_INVALID_ARGUMENT; goto cleanup; }
                ++degrees[neighbor_index];
                first_found = true;
            }
            if (neighbors[neighbor_index] == link_second[index])
            {
                if (second_found || link_second[index] == link_first[index])
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                    goto cleanup;
                }
                ++degrees[neighbor_index];
                second_found = true;
            }
        }
        if (!first_found || !second_found) { result = HENKA_ERROR_INVALID_ARGUMENT; goto cleanup; }
    }
    for (index = 0U; index < neighbor_count; ++index)
    {
        if (degrees[index] > 2U)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        if (degrees[index] == 1U)
        {
            ++degree_one_count;
            if (start == SIZE_MAX || neighbors[index] < neighbors[start]) start = index;
        }
        else if (degrees[index] != 2U)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }
    boundary = degree_one_count == 2U;
    if (degree_one_count != 0U && !boundary)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    if (!boundary && material_mixed)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    if (!boundary)
    {
        start = SIZE_MAX;
        for (index = 0U; index < neighbor_count; ++index)
        {
            if (start == SIZE_MAX || neighbors[index] < neighbors[start]) start = index;
        }
    }
    current = start;
    while (*out_cut_count < neighbor_count)
    {
        size_t link_index;
        size_t next = SIZE_MAX;
        modeling_bevel_cut* cut;
        if (current == SIZE_MAX || current == previous)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        cut = NULL;
        for (index = 0U; index < cut_count; ++index)
        {
            if (cuts[index].endpoint == vertex_id && cuts[index].other_endpoint == neighbors[current])
            {
                cut = &cuts[index];
                break;
            }
        }
        if (cut == NULL || *out_cut_count >= cut_capacity)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        out_cut_vertices[(*out_cut_count)++] = cut->vertex_id;
        for (link_index = 0U; link_index < link_count; ++link_index)
        {
            size_t candidate = SIZE_MAX;
            if (neighbors[current] == link_first[link_index])
            {
                for (index = 0U; index < neighbor_count; ++index)
                    if (neighbors[index] == link_second[link_index]) candidate = index;
            }
            else if (neighbors[current] == link_second[link_index])
            {
                for (index = 0U; index < neighbor_count; ++index)
                    if (neighbors[index] == link_first[link_index]) candidate = index;
            }
            if (candidate != SIZE_MAX && candidate != previous)
            {
                if (next == SIZE_MAX || candidate < next) next = candidate;
            }
        }
        if (*out_cut_count == neighbor_count)
        {
            if (boundary)
            {
                if (next != SIZE_MAX) { result = HENKA_ERROR_INVALID_ARGUMENT; goto cleanup; }
            }
            else if (next != start)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            break;
        }
        if (next == SIZE_MAX || next == start)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        previous = current;
        current = next;
    }
    *out_boundary = boundary;
    *out_material_region = material_region;
    result = HENKA_SUCCESS;

cleanup:
    henka_free(neighbors);
    henka_free(link_first);
    henka_free(link_second);
    henka_free(degrees);
    if (result != HENKA_SUCCESS && out_cut_count != NULL) *out_cut_count = 0U;
    return result;
}

static henka_result modeling_bevel_cap_uvs(
    const henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertices,
    size_t count,
    henka_vec2* out_uvs)
{
    henka_vec3 normal = {0.0f, 0.0f, 0.0f};
    float minimum_u = 0.0f;
    float maximum_u = 0.0f;
    float minimum_v = 0.0f;
    float maximum_v = 0.0f;
    size_t index;
    int axis;
    if (mesh == NULL || vertices == NULL || out_uvs == NULL || count < 3U) return HENKA_ERROR_INVALID_ARGUMENT;
    for (index = 0U; index < count; ++index)
    {
        const henka_authoring_vertex* first = henka_authoring_mesh_get_vertex(mesh, vertices[index]);
        const henka_authoring_vertex* second = henka_authoring_mesh_get_vertex(mesh, vertices[(index + 1U) % count]);
        if (first == NULL || second == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
        normal.x += (first->position.y - second->position.y) * (first->position.z + second->position.z);
        normal.y += (first->position.z - second->position.z) * (first->position.x + second->position.x);
        normal.z += (first->position.x - second->position.x) * (first->position.y + second->position.y);
    }
    if (!modeling_finite_vec3(normal) || henka_vec3_length(normal) <= 1.0e-7f) return HENKA_ERROR_INVALID_ARGUMENT;
    axis = fabsf(normal.x) >= fabsf(normal.y) && fabsf(normal.x) >= fabsf(normal.z) ? 0 :
        fabsf(normal.y) >= fabsf(normal.z) ? 1 : 2;
    for (index = 0U; index < count; ++index)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(mesh, vertices[index]);
        float u;
        float v;
        if (axis == 0) { u = vertex->position.y; v = vertex->position.z; }
        else if (axis == 1) { u = vertex->position.x; v = vertex->position.z; }
        else { u = vertex->position.x; v = vertex->position.y; }
        if (index == 0U) { minimum_u = maximum_u = u; minimum_v = maximum_v = v; }
        else { minimum_u = fminf(minimum_u, u); maximum_u = fmaxf(maximum_u, u); minimum_v = fminf(minimum_v, v); maximum_v = fmaxf(maximum_v, v); }
    }
    if (maximum_u - minimum_u <= 1.0e-7f || maximum_v - minimum_v <= 1.0e-7f) return HENKA_ERROR_INVALID_ARGUMENT;
    for (index = 0U; index < count; ++index)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(mesh, vertices[index]);
        float u;
        float v;
        if (axis == 0) { u = vertex->position.y; v = vertex->position.z; }
        else if (axis == 1) { u = vertex->position.x; v = vertex->position.z; }
        else { u = vertex->position.x; v = vertex->position.y; }
        out_uvs[index] = (henka_vec2){(u - minimum_u) / (maximum_u - minimum_u), (v - minimum_v) / (maximum_v - minimum_v)};
    }
    return HENKA_SUCCESS;
}

static bool modeling_bevel_loop_convex(
    const henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertices,
    size_t count)
{
    henka_vec3 normal = {0.0f, 0.0f, 0.0f};
    float sign = 0.0f;
    size_t index;
    int axis;
    if (mesh == NULL || vertices == NULL || count < 3U) return false;
    for (index = 0U; index < count; ++index)
    {
        const henka_authoring_vertex* first = henka_authoring_mesh_get_vertex(mesh, vertices[index]);
        const henka_authoring_vertex* second = henka_authoring_mesh_get_vertex(mesh, vertices[(index + 1U) % count]);
        if (first == NULL || second == NULL) return false;
        normal.x += (first->position.y - second->position.y) * (first->position.z + second->position.z);
        normal.y += (first->position.z - second->position.z) * (first->position.x + second->position.x);
        normal.z += (first->position.x - second->position.x) * (first->position.y + second->position.y);
    }
    if (henka_vec3_length(normal) <= 1.0e-7f) return false;
    axis = fabsf(normal.x) >= fabsf(normal.y) && fabsf(normal.x) >= fabsf(normal.z) ? 0 :
        fabsf(normal.y) >= fabsf(normal.z) ? 1 : 2;
    for (index = 0U; index < count; ++index)
    {
        const henka_authoring_vertex* a = henka_authoring_mesh_get_vertex(mesh, vertices[index]);
        const henka_authoring_vertex* b = henka_authoring_mesh_get_vertex(mesh, vertices[(index + 1U) % count]);
        const henka_authoring_vertex* c = henka_authoring_mesh_get_vertex(mesh, vertices[(index + 2U) % count]);
        float cross;
        if (axis == 0) cross = (b->position.y - a->position.y) * (c->position.z - b->position.z) - (b->position.z - a->position.z) * (c->position.y - b->position.y);
        else if (axis == 1) cross = (b->position.x - a->position.x) * (c->position.z - b->position.z) - (b->position.z - a->position.z) * (c->position.x - b->position.x);
        else cross = (b->position.x - a->position.x) * (c->position.y - b->position.y) - (b->position.y - a->position.y) * (c->position.x - b->position.x);
        if (fabsf(cross) <= 1.0e-7f) return false;
        if (sign == 0.0f) sign = cross > 0.0f ? 1.0f : -1.0f;
        else if (cross * sign <= 0.0f) return false;
    }
    return true;
}

static void modeling_destroy_bevel_works(
    modeling_bevel_face_work* works,
    size_t work_capacity,
    modeling_bevel_cap* caps,
    size_t cap_count)
{
    size_t index;
    if (works != NULL)
    {
        for (index = 0U; index < work_capacity; ++index)
        {
            henka_free(works[index].vertices);
            henka_free(works[index].uvs);
        }
    }
    if (caps != NULL)
    {
        for (index = 0U; index < cap_count; ++index)
        {
            henka_free(caps[index].vertices);
            henka_free(caps[index].uvs);
        }
    }
    henka_free(works);
    henka_free(caps);
}

henka_result henka_authoring_mesh_bevel_vertices(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    float width,
    henka_authoring_vertex_id* out_result_vertices,
    size_t result_vertex_capacity,
    size_t* out_result_vertex_count,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_vertex_id* selected = NULL;
    henka_authoring_mesh* candidate = NULL;
    modeling_bevel_cut* cuts = NULL;
    modeling_bevel_face_work* works = NULL;
    modeling_bevel_cap* caps = NULL;
    henka_authoring_face_loop_update* updates = NULL;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_authoring_mesh_desc desc = {0};
    size_t cut_capacity;
    size_t cut_count = 0U;
    size_t update_count = 0U;
    size_t index;
    size_t edge_slot;
    size_t edge_id;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    if (out_result_vertex_count != NULL) *out_result_vertex_count = 0U;
    modeling_report_reset(out_report);
    if (mesh == NULL || vertex_ids == NULL || vertex_count == 0U || !isfinite(width) || width <= 0.0f ||
        out_result_vertex_count == NULL ||
        !modeling_sorted_unique_vertex_ids(mesh, vertex_ids, vertex_count, &selected))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    desc = henka_authoring_mesh_get_desc(mesh);
    if (vertex_count > desc.max_vertices || desc.max_edges > (SIZE_MAX - 1U) / 2U)
    {
        result = HENKA_ERROR_LIMIT;
        goto cleanup;
    }
    cut_capacity = desc.max_edges * 2U;
    cuts = henka_calloc(cut_capacity, sizeof(*cuts));
    works = henka_calloc(desc.max_faces, sizeof(*works));
    caps = henka_calloc(vertex_count, sizeof(*caps));
    updates = henka_calloc(desc.max_faces, sizeof(*updates));
    if (cuts == NULL || works == NULL || caps == NULL || updates == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result != HENKA_SUCCESS) goto cleanup;

    for (edge_slot = 0U; edge_slot < desc.max_edges; ++edge_slot)
    {
        henka_authoring_edge_id edge_handle;
        const henka_authoring_edge* edge = henka_authoring_mesh_get_edge_id_at(mesh, edge_slot, &edge_handle) == HENKA_SUCCESS
            ? henka_authoring_mesh_get_edge(mesh, edge_handle)
            : NULL;
        size_t endpoint_index;
        if (edge == NULL) continue;
        for (endpoint_index = 0U; endpoint_index < 2U; ++endpoint_index)
        {
            const henka_authoring_vertex_id endpoint = edge->vertices[endpoint_index];
            const henka_authoring_vertex_id other = edge->vertices[1U - endpoint_index];
            const henka_authoring_vertex* endpoint_vertex;
            const henka_authoring_vertex* other_vertex;
            henka_vec3 delta;
            float length;
            float factor;
            henka_authoring_vertex_id new_id;
            if (!modeling_bevel_selected_contains(selected, vertex_count, endpoint)) continue;
            endpoint_vertex = henka_authoring_mesh_get_vertex(mesh, endpoint);
            other_vertex = henka_authoring_mesh_get_vertex(mesh, other);
            if (endpoint_vertex == NULL || other_vertex == NULL) { result = HENKA_ERROR_INVALID_ARGUMENT; goto cleanup; }
            delta = henka_vec3_subtract(other_vertex->position, endpoint_vertex->position);
            length = henka_vec3_length(delta);
            if (!isfinite(length) || length <= 1.0e-7f) { result = HENKA_ERROR_INVALID_ARGUMENT; goto cleanup; }
            factor = width / length;
            if (modeling_bevel_selected_contains(selected, vertex_count, other))
            {
                if (!(2.0f * width < length - 1.0e-5f)) { result = HENKA_ERROR_INVALID_ARGUMENT; goto cleanup; }
            }
            else if (!(width < length - 1.0e-5f)) { result = HENKA_ERROR_INVALID_ARGUMENT; goto cleanup; }
            if (cut_count >= cut_capacity) { result = HENKA_ERROR_LIMIT; goto cleanup; }
            result = henka_authoring_mesh_add_vertex(
                candidate, henka_vec3_add(endpoint_vertex->position, henka_vec3_scale(delta, factor)),
                endpoint_vertex->uv, endpoint_vertex->material_region, &new_id);
            if (result != HENKA_SUCCESS) goto cleanup;
            cuts[cut_count++] = (modeling_bevel_cut){
                edge_handle, endpoint, other, new_id,
                henka_vec3_add(endpoint_vertex->position, henka_vec3_scale(delta, factor)),
                {0.0f, 0.0f}, 0U, factor};
        }
    }
    if (cut_count == 0U) { result = HENKA_ERROR_INVALID_ARGUMENT; goto cleanup; }
    for (index = 0U; index < vertex_count; ++index)
    {
        size_t cut_vertices = 0U;
        bool boundary = false;
        uint32_t material_region = 0U;
        caps[index].vertices = henka_malloc(desc.max_edges * sizeof(*caps[index].vertices));
        caps[index].uvs = henka_malloc(desc.max_edges * sizeof(*caps[index].uvs));
        if (caps[index].vertices == NULL || caps[index].uvs == NULL) { result = HENKA_ERROR_OUT_OF_MEMORY; goto cleanup; }
        result = modeling_bevel_build_link_order(
            mesh, selected[index], cuts, cut_count, caps[index].vertices, desc.max_edges,
            &cut_vertices, &boundary, &material_region);
        if (result != HENKA_SUCCESS) goto cleanup;
        caps[index].count = cut_vertices;
        caps[index].material_region = material_region;
        if (!boundary)
        {
            if (cut_vertices < 3U || !modeling_bevel_loop_convex(candidate, caps[index].vertices, cut_vertices) ||
                modeling_bevel_cap_uvs(candidate, caps[index].vertices, cut_vertices, caps[index].uvs) != HENKA_SUCCESS)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
        }
    }
    for (edge_slot = 0U; edge_slot < desc.max_edges; ++edge_slot)
    {
        henka_authoring_edge_id edge_handle;
        const henka_authoring_edge* edge = henka_authoring_mesh_get_edge_id_at(mesh, edge_slot, &edge_handle) == HENKA_SUCCESS
            ? henka_authoring_mesh_get_edge(mesh, edge_handle)
            : NULL;
        if (edge != NULL && edge->hard)
        {
            size_t endpoint_index;
            for (endpoint_index = 0U; endpoint_index < 2U; ++endpoint_index)
            {
                if (modeling_bevel_selected_contains(selected, vertex_count, edge->vertices[endpoint_index]))
                {
                    modeling_bevel_cut* cut = modeling_bevel_find_cut(
                        cuts, cut_count, edge->id, edge->vertices[endpoint_index]);
                    if (cut == NULL) { result = HENKA_ERROR_INVALID_ARGUMENT; goto cleanup; }
                }
            }
        }
    }
    for (index = 0U; index < desc.max_faces; ++index)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face = henka_authoring_mesh_get_face_id_at(mesh, index, &face_id) == HENKA_SUCCESS
            ? henka_authoring_mesh_get_face(mesh, face_id)
            : NULL;
        size_t corner;
        size_t write = 0U;
        bool modified = false;
        if (face == NULL) continue;
        for (corner = 0U; corner < face->corner_count; ++corner)
            if (modeling_bevel_selected_contains(selected, vertex_count, face->vertices[corner])) modified = true;
        if (!modified) continue;
        if (face->corner_count > SIZE_MAX / 2U || face->corner_count * 2U > desc.max_face_corners)
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        works[index].vertices = henka_malloc(face->corner_count * 2U * sizeof(*works[index].vertices));
        works[index].uvs = henka_malloc(face->corner_count * 2U * sizeof(*works[index].uvs));
        if (works[index].vertices == NULL || works[index].uvs == NULL) { result = HENKA_ERROR_OUT_OF_MEMORY; goto cleanup; }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const henka_authoring_vertex_id source_id = face->vertices[corner];
            if (!modeling_bevel_selected_contains(selected, vertex_count, source_id))
            {
                works[index].vertices[write] = source_id;
                works[index].uvs[write++] = face->uvs[corner];
                continue;
            }
            {
                const size_t previous_corner = (corner + face->corner_count - 1U) % face->corner_count;
                const size_t next_corner = (corner + 1U) % face->corner_count;
                const henka_authoring_edge_id edge_ids[2] = {face->edges[previous_corner], face->edges[corner]};
                const size_t neighbor_corners[2] = {previous_corner, next_corner};
                size_t cut_index;
                for (cut_index = 0U; cut_index < 2U; ++cut_index)
                {
                    modeling_bevel_cut* cut = modeling_bevel_find_cut(cuts, cut_count, edge_ids[cut_index], source_id);
                    henka_vec2 source_uv = face->uvs[corner];
                    henka_vec2 neighbor_uv = face->uvs[neighbor_corners[cut_index]];
                    if (cut == NULL || write >= face->corner_count * 2U) { result = HENKA_ERROR_INVALID_ARGUMENT; goto cleanup; }
                    works[index].vertices[write] = cut->vertex_id;
                    works[index].uvs[write] = (henka_vec2){
                        source_uv.x + (neighbor_uv.x - source_uv.x) * cut->factor,
                        source_uv.y + (neighbor_uv.y - source_uv.y) * cut->factor};
                    cut->uv_sum.x += works[index].uvs[write].x;
                    cut->uv_sum.y += works[index].uvs[write].y;
                    ++cut->uv_samples;
                    ++write;
                }
            }
        }
        if (write < 3U || write > desc.max_face_corners) { result = HENKA_ERROR_INVALID_ARGUMENT; goto cleanup; }
        works[index].update.face_id = face->id;
        works[index].update.vertices = works[index].vertices;
        works[index].update.uvs = works[index].uvs;
        works[index].update.corner_count = write;
        works[index].update.material_region = face->material_region;
        works[index].update.smooth = face->smooth;
        updates[update_count++] = works[index].update;
    }
    for (index = 0U; index < cut_count; ++index)
    {
        if (cuts[index].uv_samples == 0U) { result = HENKA_ERROR_INVALID_ARGUMENT; goto cleanup; }
        result = henka_authoring_mesh_set_vertex_uv(candidate, cuts[index].vertex_id, (henka_vec2){
            cuts[index].uv_sum.x / (float)cuts[index].uv_samples,
            cuts[index].uv_sum.y / (float)cuts[index].uv_samples});
        if (result != HENKA_SUCCESS) goto cleanup;
    }
    for (index = 0U; index < vertex_count; ++index)
    {
        if (caps[index].count < 3U) continue;
        result = henka_authoring_mesh_add_face(
            candidate, caps[index].vertices, caps[index].count,
            caps[index].material_region, false, &caps[index].face_id);
        if (result != HENKA_SUCCESS) goto cleanup;
        for (edge_id = 0U; edge_id < caps[index].count; ++edge_id)
        {
            result = henka_authoring_mesh_set_face_corner_uv(
                candidate, caps[index].face_id, edge_id, caps[index].uvs[edge_id]);
            if (result != HENKA_SUCCESS) goto cleanup;
        }
    }
    result = henka_authoring_mesh_apply_face_loop_updates_internal(candidate, updates, update_count);
    if (result != HENKA_SUCCESS) goto cleanup;
    for (index = 0U; index < update_count; ++index)
    {
        if (!modeling_bevel_loop_convex(candidate, updates[index].vertices, updates[index].corner_count))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }
    for (index = 0U; index < vertex_count; ++index)
    {
        result = henka_authoring_mesh_remove_vertex(candidate, selected[index]);
        if (result != HENKA_SUCCESS) goto cleanup;
    }
    for (index = 0U; index < cut_count; ++index)
    {
        const henka_authoring_edge* source_edge = henka_authoring_mesh_get_edge(mesh, cuts[index].edge_id);
        if (source_edge != NULL && source_edge->hard)
        {
            henka_authoring_vertex_id target = cuts[index].other_endpoint;
            modeling_bevel_cut* other_cut = modeling_bevel_find_cut(
                cuts, cut_count, cuts[index].edge_id, cuts[index].other_endpoint);
            henka_authoring_edge_id target_edge = HENKA_AUTHORING_INVALID_ID;
            if (other_cut != NULL) target = other_cut->vertex_id;
            if (modeling_find_edge_for_pair(candidate, cuts[index].vertex_id, target, &target_edge) == HENKA_SUCCESS)
            {
                result = henka_authoring_mesh_set_edge_hard(candidate, target_edge, true);
                if (result != HENKA_SUCCESS) goto cleanup;
            }
        }
    }
    if (!henka_authoring_mesh_validate(candidate) || henka_authoring_mesh_get_counts(candidate).faces == 0U ||
        !modeling_face_geometry_is_valid(candidate))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    if (result_vertex_capacity < cut_count || (cut_count > 0U && out_result_vertices == NULL))
    {
        result = HENKA_ERROR_LIMIT;
        goto cleanup;
    }
    for (index = 0U; index < cut_count; ++index) out_result_vertices[index] = cuts[index].vertex_id;
    if (cut_count > 1U) qsort(out_result_vertices, cut_count, sizeof(*out_result_vertices), modeling_vertex_id_compare);
    after = henka_authoring_mesh_get_counts(candidate);
    result = henka_authoring_mesh_copy(mesh, candidate);
    if (result == HENKA_SUCCESS)
    {
        *out_result_vertex_count = cut_count;
        modeling_report_count_delta(&before, &after, out_report);
        if (out_report != NULL)
        {
            out_report->primary_vertex_id = cut_count > 0U ? out_result_vertices[0] : HENKA_AUTHORING_INVALID_ID;
            out_report->created_faces = after.faces > before.faces ? after.faces - before.faces : 0U;
        }
    }

cleanup:
    henka_authoring_mesh_destroy(candidate);
    modeling_destroy_bevel_works(works, desc.max_faces, caps, vertex_count);
    henka_free(updates);
    henka_free(cuts);
    henka_free(selected);
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

static bool modeling_primitive_segments_are_valid(size_t segments)
{
    return segments >= 3U && segments <= HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS;
}

static henka_result modeling_finish_primitive_constructor(
    henka_authoring_mesh* mesh,
    henka_authoring_mesh** out_mesh)
{
    if (mesh == NULL || out_mesh == NULL)
    {
        henka_authoring_mesh_destroy(mesh);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!henka_authoring_mesh_validate(mesh))
    {
        henka_authoring_mesh_destroy(mesh);
        return HENKA_ERROR_UNKNOWN;
    }
    *out_mesh = mesh;
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_create_cylinder(
    const henka_authoring_mesh_desc* desc,
    float radius,
    float height,
    size_t segments,
    henka_authoring_mesh** out_mesh)
{
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id lower[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id upper[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id cap[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    const float half_height = height * 0.5f;
    size_t index;
    henka_result result;

    if (out_mesh == NULL || !modeling_finite_scalar(radius) || !modeling_finite_scalar(height) ||
        radius <= 0.0f || height <= 0.0f || !modeling_primitive_segments_are_valid(segments))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_mesh = NULL;
    result = henka_authoring_mesh_create(desc, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    for (index = 0U; index < segments; ++index)
    {
        const float fraction = (float)index / (float)segments;
        const float angle = 2.0f * HENKA_PI * fraction;
        const float x = radius * cosf(angle);
        const float z = radius * sinf(angle);
        result = henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){x, -half_height, z}, (henka_vec2){fraction, 0.0f}, 0U, &lower[index]);
        if (result != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(mesh);
            return result;
        }
        result = henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){x, half_height, z}, (henka_vec2){fraction, 1.0f}, 0U, &upper[index]);
        if (result != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(mesh);
            return result;
        }
    }
    for (index = 0U; index < segments; ++index)
    {
        const size_t next = (index + 1U) % segments;
        const henka_authoring_vertex_id side[4] = {
            lower[index], upper[index], upper[next], lower[next]};
        if (henka_authoring_mesh_add_face(mesh, side, 4U, 0U, true, &(henka_authoring_face_id){0U}) != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(mesh);
            return HENKA_ERROR_LIMIT;
        }
        cap[index] = lower[index];
    }
    result = henka_authoring_mesh_add_face(mesh, cap, segments, 0U, false, &(henka_authoring_face_id){0U});
    if (result == HENKA_SUCCESS)
    {
        for (index = 0U; index < segments; ++index)
        {
            cap[index] = upper[segments - 1U - index];
        }
        result = henka_authoring_mesh_add_face(mesh, cap, segments, 0U, false, &(henka_authoring_face_id){0U});
    }
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(mesh);
        return result;
    }
    return modeling_finish_primitive_constructor(mesh, out_mesh);
}

henka_result henka_authoring_mesh_create_cone(
    const henka_authoring_mesh_desc* desc,
    float radius,
    float height,
    size_t segments,
    henka_authoring_mesh** out_mesh)
{
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id base[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id apex = HENKA_AUTHORING_INVALID_ID;
    const float half_height = height * 0.5f;
    size_t index;
    henka_result result;

    if (out_mesh == NULL || !modeling_finite_scalar(radius) || !modeling_finite_scalar(height) ||
        radius <= 0.0f || height <= 0.0f || !modeling_primitive_segments_are_valid(segments))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_mesh = NULL;
    result = henka_authoring_mesh_create(desc, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    for (index = 0U; index < segments; ++index)
    {
        const float fraction = (float)index / (float)segments;
        const float angle = 2.0f * HENKA_PI * fraction;
        result = henka_authoring_mesh_add_vertex(mesh,
            (henka_vec3){radius * cosf(angle), -half_height, radius * sinf(angle)},
            (henka_vec2){fraction, 0.0f}, 0U, &base[index]);
        if (result != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(mesh);
            return result;
        }
    }
    result = henka_authoring_mesh_add_vertex(
        mesh, (henka_vec3){0.0f, half_height, 0.0f}, (henka_vec2){0.5f, 1.0f}, 0U, &apex);
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(mesh);
        return result;
    }
    for (index = 0U; index < segments; ++index)
    {
        const size_t next = (index + 1U) % segments;
        const henka_authoring_vertex_id side[3] = {base[index], apex, base[next]};
        result = henka_authoring_mesh_add_face(mesh, side, 3U, 0U, true, &(henka_authoring_face_id){0U});
        if (result != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(mesh);
            return result;
        }
    }
    result = henka_authoring_mesh_add_face(mesh, base, segments, 0U, false, &(henka_authoring_face_id){0U});
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(mesh);
        return result;
    }
    return modeling_finish_primitive_constructor(mesh, out_mesh);
}

henka_result henka_authoring_mesh_create_uv_sphere(
    const henka_authoring_mesh_desc* desc,
    float radius,
    size_t longitude_segments,
    size_t latitude_segments,
    henka_authoring_mesh** out_mesh)
{
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id previous[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id current[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
    henka_authoring_vertex_id top = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id bottom = HENKA_AUTHORING_INVALID_ID;
    size_t latitude;
    size_t longitude;
    henka_result result;

    if (out_mesh == NULL || !modeling_finite_scalar(radius) || radius <= 0.0f ||
        !modeling_primitive_segments_are_valid(longitude_segments) ||
        !modeling_primitive_segments_are_valid(latitude_segments))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_mesh = NULL;
    result = henka_authoring_mesh_create(desc, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_authoring_mesh_add_vertex(
        mesh, (henka_vec3){0.0f, radius, 0.0f}, (henka_vec2){0.5f, 0.0f}, 0U, &top);
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(mesh);
        return result;
    }
    for (latitude = 1U; latitude < latitude_segments; ++latitude)
    {
        const float vertical_fraction = (float)latitude / (float)latitude_segments;
        const float polar = HENKA_PI * vertical_fraction;
        const float ring_radius = radius * sinf(polar);
        const float y = radius * cosf(polar);
        for (longitude = 0U; longitude < longitude_segments; ++longitude)
        {
            const float horizontal_fraction = (float)longitude / (float)longitude_segments;
            const float angle = 2.0f * HENKA_PI * horizontal_fraction;
            result = henka_authoring_mesh_add_vertex(mesh,
                (henka_vec3){ring_radius * cosf(angle), y, ring_radius * sinf(angle)},
                (henka_vec2){horizontal_fraction, vertical_fraction}, 0U, &current[longitude]);
            if (result != HENKA_SUCCESS)
            {
                henka_authoring_mesh_destroy(mesh);
                return result;
            }
        }
        if (latitude == 1U)
        {
            for (longitude = 0U; longitude < longitude_segments; ++longitude)
            {
                const size_t next = (longitude + 1U) % longitude_segments;
                const henka_authoring_vertex_id face[3] = {top, current[next], current[longitude]};
                result = henka_authoring_mesh_add_face(mesh, face, 3U, 0U, true, &(henka_authoring_face_id){0U});
                if (result != HENKA_SUCCESS)
                {
                    henka_authoring_mesh_destroy(mesh);
                    return result;
                }
            }
        }
        else
        {
            for (longitude = 0U; longitude < longitude_segments; ++longitude)
            {
                const size_t next = (longitude + 1U) % longitude_segments;
                const henka_authoring_vertex_id face[4] = {
                    previous[longitude], previous[next], current[next], current[longitude]};
                result = henka_authoring_mesh_add_face(mesh, face, 4U, 0U, true, &(henka_authoring_face_id){0U});
                if (result != HENKA_SUCCESS)
                {
                    henka_authoring_mesh_destroy(mesh);
                    return result;
                }
            }
        }
        memcpy(previous, current, longitude_segments * sizeof(*previous));
    }
    result = henka_authoring_mesh_add_vertex(
        mesh, (henka_vec3){0.0f, -radius, 0.0f}, (henka_vec2){0.5f, 1.0f}, 0U, &bottom);
    if (result == HENKA_SUCCESS)
    {
        for (longitude = 0U; longitude < longitude_segments; ++longitude)
        {
            const size_t next = (longitude + 1U) % longitude_segments;
            const henka_authoring_vertex_id face[3] = {previous[longitude], previous[next], bottom};
            result = henka_authoring_mesh_add_face(mesh, face, 3U, 0U, true, &(henka_authoring_face_id){0U});
            if (result != HENKA_SUCCESS)
            {
                break;
            }
        }
    }
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(mesh);
        return result;
    }
    return modeling_finish_primitive_constructor(mesh, out_mesh);
}

static bool modeling_quad_sphere_counts(
    size_t subdivisions,
    size_t* out_vertices,
    size_t* out_edges,
    size_t* out_faces)
{
    size_t square;
    size_t vertices;
    size_t edges;
    size_t faces;

    if (subdivisions == 0U || out_vertices == NULL || out_edges == NULL ||
        out_faces == NULL ||
        !henka_checked_size_multiply(subdivisions, subdivisions, &square) ||
        !henka_checked_size_multiply(square, 6U, &faces) ||
        !henka_checked_size_add(faces, 2U, &vertices) ||
        !henka_checked_size_multiply(square, 12U, &edges))
    {
        return false;
    }

    *out_vertices = vertices;
    *out_edges = edges;
    *out_faces = faces;
    return true;
}

static bool modeling_quad_sphere_surface_index(
    size_t subdivisions,
    size_t x,
    size_t y,
    size_t z,
    size_t* out_index)
{
    size_t side;
    size_t plane_stride;
    size_t vertex_count;
    size_t edge_count;
    size_t face_count;
    size_t index;
    size_t offset;
    size_t ring_stride;
    size_t perimeter_offset;
    size_t y_offset;

    if (out_index == NULL || subdivisions == 0U || x > subdivisions ||
        y > subdivisions || z > subdivisions ||
        (x > 0U && x < subdivisions && y > 0U && y < subdivisions &&
         z > 0U && z < subdivisions))
    {
        return false;
    }
    if (!modeling_quad_sphere_counts(
            subdivisions, &vertex_count, &edge_count, &face_count) ||
        !henka_checked_size_add(subdivisions, 1U, &side) ||
        !henka_checked_size_multiply(side, side, &plane_stride))
    {
        return false;
    }

    if (z == 0U)
    {
        if (!henka_checked_size_multiply(y, side, &index) ||
            !henka_checked_size_add(index, x, &index))
        {
            return false;
        }
    }
    else if (z == subdivisions)
    {
        if (!henka_checked_size_multiply(y, side, &offset) ||
            !henka_checked_size_add(offset, x, &offset) ||
            !henka_checked_size_add(plane_stride, offset, &index))
        {
            return false;
        }
    }
    else
    {
        if (!henka_checked_size_multiply(4U, subdivisions, &ring_stride) ||
            !henka_checked_size_multiply(z - 1U, ring_stride, &offset) ||
            !henka_checked_size_multiply(2U, plane_stride, &index) ||
            !henka_checked_size_add(index, offset, &index))
        {
            return false;
        }
        if (y == 0U)
        {
            offset = x;
        }
        else if (y == subdivisions)
        {
            if (!henka_checked_size_add(side, x, &offset))
            {
                return false;
            }
        }
        else
        {
            if (!henka_checked_size_multiply(2U, side, &perimeter_offset) ||
                !henka_checked_size_multiply(2U, y - 1U, &y_offset) ||
                !henka_checked_size_add(
                    perimeter_offset, y_offset, &offset) ||
                (x != 0U && !henka_checked_size_add(offset, 1U, &offset)))
            {
                return false;
            }
        }
        if (!henka_checked_size_add(index, offset, &index))
        {
            return false;
        }
    }

    if (index >= vertex_count)
    {
        return false;
    }
    *out_index = index;
    return true;
}

static henka_result modeling_quad_sphere_add_vertex(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id* vertex_ids,
    size_t subdivisions,
    float radius,
    size_t x,
    size_t y,
    size_t z)
{
    const float scale = 2.0f / (float)subdivisions;
    const henka_vec3 direction = {
        -1.0f + scale * (float)x,
        -1.0f + scale * (float)y,
        -1.0f + scale * (float)z};
    const float direction_length = henka_vec3_length(direction);
    const float radial_scale = radius / direction_length;
    const henka_vec3 position = {
        direction.x * radial_scale,
        direction.y * radial_scale,
        direction.z * radial_scale};
    const float unit_y = fmaxf(-1.0f, fminf(1.0f, position.y / radius));
    const henka_vec2 uv = {
        atan2f(position.z, position.x) / (2.0f * HENKA_PI) + 0.5f,
        acosf(unit_y) / HENKA_PI};
    size_t vertex_index;

    if (!modeling_quad_sphere_surface_index(
            subdivisions, x, y, z, &vertex_index))
    {
        return HENKA_ERROR_UNKNOWN;
    }
    return henka_authoring_mesh_add_vertex(
        mesh, position, uv, 0U, &vertex_ids[vertex_index]);
}

static henka_result modeling_quad_sphere_add_face(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t subdivisions,
    size_t face_index,
    size_t u,
    size_t v)
{
    size_t coordinates[4][3];
    henka_authoring_vertex_id vertices[4];
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    const float u0 = (float)u / (float)subdivisions;
    const float u1 = (float)(u + 1U) / (float)subdivisions;
    const float v0 = (float)v / (float)subdivisions;
    const float v1 = (float)(v + 1U) / (float)subdivisions;
    const float local_u[4] = {u0, u1, u1, u0};
    const float local_v[4] = {v0, v0, v1, v1};
    const size_t atlas_column = face_index % 3U;
    const size_t atlas_row = face_index / 3U;
    size_t corner;
    henka_result result;

    switch (face_index)
    {
        case 0U:
            memcpy(coordinates, (size_t[4][3]){
                {subdivisions, u, v},
                {subdivisions, u + 1U, v},
                {subdivisions, u + 1U, v + 1U},
                {subdivisions, u, v + 1U}}, sizeof(coordinates));
            break;
        case 1U:
            memcpy(coordinates, (size_t[4][3]){
                {0U, v, u},
                {0U, v, u + 1U},
                {0U, v + 1U, u + 1U},
                {0U, v + 1U, u}}, sizeof(coordinates));
            break;
        case 2U:
            memcpy(coordinates, (size_t[4][3]){
                {v, subdivisions, u},
                {v, subdivisions, u + 1U},
                {v + 1U, subdivisions, u + 1U},
                {v + 1U, subdivisions, u}}, sizeof(coordinates));
            break;
        case 3U:
            memcpy(coordinates, (size_t[4][3]){
                {u, 0U, v},
                {u + 1U, 0U, v},
                {u + 1U, 0U, v + 1U},
                {u, 0U, v + 1U}}, sizeof(coordinates));
            break;
        case 4U:
            memcpy(coordinates, (size_t[4][3]){
                {u, v, subdivisions},
                {u + 1U, v, subdivisions},
                {u + 1U, v + 1U, subdivisions},
                {u, v + 1U, subdivisions}}, sizeof(coordinates));
            break;
        case 5U:
            memcpy(coordinates, (size_t[4][3]){
                {v, u, 0U},
                {v, u + 1U, 0U},
                {v + 1U, u + 1U, 0U},
                {v + 1U, u, 0U}}, sizeof(coordinates));
            break;
        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (corner = 0U; corner < 4U; ++corner)
    {
        size_t vertex_index;
        if (!modeling_quad_sphere_surface_index(
                subdivisions,
                coordinates[corner][0],
                coordinates[corner][1],
                coordinates[corner][2],
                &vertex_index) ||
            vertex_ids[vertex_index] == HENKA_AUTHORING_INVALID_ID)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        vertices[corner] = vertex_ids[vertex_index];
    }

    result = henka_authoring_mesh_add_face(
        mesh, vertices, 4U, 0U, true, &new_face_id);
    for (corner = 0U; result == HENKA_SUCCESS && corner < 4U; ++corner)
    {
        const henka_vec2 uv = {
            ((float)atlas_column + local_u[corner]) / 3.0f,
            ((float)atlas_row + local_v[corner]) / 2.0f};
        result = henka_authoring_mesh_set_face_corner_uv(
            mesh, new_face_id, corner, uv);
    }
    return result;
}

henka_result henka_authoring_mesh_create_quad_sphere(
    const henka_authoring_mesh_desc* desc,
    float radius,
    size_t subdivisions,
    henka_authoring_mesh** out_mesh)
{
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id* vertex_ids = NULL;
    size_t vertex_count;
    size_t edge_count;
    size_t face_count;
    size_t vertex_bytes;
    size_t x;
    size_t y;
    size_t z;
    size_t u;
    size_t v;
    size_t face_index;
    henka_result result;

    if (out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_mesh = NULL;
    if (desc == NULL || !modeling_finite_scalar(radius) || radius <= 0.0f ||
        subdivisions == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!modeling_quad_sphere_counts(
            subdivisions, &vertex_count, &edge_count, &face_count) ||
        vertex_count > desc->max_vertices || edge_count > desc->max_edges ||
        face_count > desc->max_faces || desc->max_face_corners < 4U ||
        !henka_checked_size_multiply(
            vertex_count, sizeof(*vertex_ids), &vertex_bytes))
    {
        return HENKA_ERROR_LIMIT;
    }

    result = henka_authoring_mesh_create(desc, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    vertex_ids = henka_malloc(vertex_bytes);
    if (vertex_ids == NULL)
    {
        henka_authoring_mesh_destroy(mesh);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    memset(vertex_ids, 0xff, vertex_bytes);

    for (y = 0U; result == HENKA_SUCCESS && y <= subdivisions; ++y)
    {
        for (x = 0U; result == HENKA_SUCCESS && x <= subdivisions; ++x)
        {
            result = modeling_quad_sphere_add_vertex(
                mesh, vertex_ids, subdivisions, radius, x, y, 0U);
            if (result == HENKA_SUCCESS)
            {
                result = modeling_quad_sphere_add_vertex(
                    mesh, vertex_ids, subdivisions, radius, x, y, subdivisions);
            }
        }
    }
    for (z = 1U; result == HENKA_SUCCESS && z < subdivisions; ++z)
    {
        for (x = 0U; result == HENKA_SUCCESS && x <= subdivisions; ++x)
        {
            result = modeling_quad_sphere_add_vertex(
                mesh, vertex_ids, subdivisions, radius, x, 0U, z);
            if (result == HENKA_SUCCESS)
            {
                result = modeling_quad_sphere_add_vertex(
                    mesh, vertex_ids, subdivisions, radius, x, subdivisions, z);
            }
        }
        for (y = 1U; result == HENKA_SUCCESS && y < subdivisions; ++y)
        {
            result = modeling_quad_sphere_add_vertex(
                mesh, vertex_ids, subdivisions, radius, 0U, y, z);
            if (result == HENKA_SUCCESS)
            {
                result = modeling_quad_sphere_add_vertex(
                    mesh, vertex_ids, subdivisions, radius, subdivisions, y, z);
            }
        }
    }

    for (face_index = 0U; result == HENKA_SUCCESS && face_index < 6U;
         ++face_index)
    {
        for (v = 0U; result == HENKA_SUCCESS && v < subdivisions; ++v)
        {
            for (u = 0U; result == HENKA_SUCCESS && u < subdivisions; ++u)
            {
                result = modeling_quad_sphere_add_face(
                    mesh, vertex_ids, subdivisions, face_index, u, v);
            }
        }
    }

    henka_free(vertex_ids);
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(mesh);
        return result;
    }
    return modeling_finish_primitive_constructor(mesh, out_mesh);
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

typedef struct modeling_vertex_fan_face
{
    henka_authoring_face_id face_id;
    size_t selected_corner;
    size_t corner_count;
} modeling_vertex_fan_face;

static int modeling_vertex_fan_face_compare(const void* left_pointer, const void* right_pointer)
{
    const modeling_vertex_fan_face* left = (const modeling_vertex_fan_face*)left_pointer;
    const modeling_vertex_fan_face* right = (const modeling_vertex_fan_face*)right_pointer;
    return left->face_id < right->face_id ? -1 : left->face_id > right->face_id ? 1 : 0;
}

static size_t modeling_vertex_fan_face_index(
    const modeling_vertex_fan_face* faces,
    size_t face_count,
    henka_authoring_face_id face_id)
{
    size_t first = 0U;
    size_t last = face_count;
    while (first < last)
    {
        const size_t middle = first + (last - first) / 2U;
        if (faces[middle].face_id < face_id)
        {
            first = middle + 1U;
        }
        else if (faces[middle].face_id > face_id)
        {
            last = middle;
        }
        else
        {
            return middle;
        }
    }
    return SIZE_MAX;
}

henka_result henka_authoring_mesh_extrude_vertex(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id vertex_id,
    float distance,
    henka_authoring_vertex_id* out_new_vertex_id,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_mesh* candidate = NULL;
    modeling_vertex_fan_face* fan_faces = NULL;
    henka_authoring_vertex_id* fan_vertices = NULL;
    henka_vec2* fan_uvs = NULL;
    bool* fan_edge_hard = NULL;
    bool* visited = NULL;
    size_t* pending = NULL;
    henka_authoring_mesh_desc desc = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    const henka_authoring_vertex* source_vertex;
    henka_authoring_vertex_id new_vertex_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id cap_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id side_face_id = HENKA_AUTHORING_INVALID_ID;
    size_t boundary_face_indices[2] = {0U, 0U};
    size_t boundary_face_positions[2] = {0U, 0U};
    henka_vec3 fan_normal = {0.0f, 0.0f, 0.0f};
    henka_vec3 offset = {0.0f, 0.0f, 0.0f};
    size_t incident_face_count = 0U;
    size_t boundary_count = 0U;
    size_t edge_count;
    size_t face_slot;
    size_t index;
    size_t corner;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    if (out_new_vertex_id != NULL)
    {
        *out_new_vertex_id = HENKA_AUTHORING_INVALID_ID;
    }
    modeling_report_reset(out_report);
    if (mesh == NULL || out_new_vertex_id == NULL || !modeling_finite_scalar(distance))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!henka_authoring_mesh_validate(mesh) || !modeling_face_geometry_is_valid(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    desc = henka_authoring_mesh_get_desc(mesh);
    source_vertex = henka_authoring_mesh_get_vertex(mesh, vertex_id);
    if (source_vertex == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    edge_count = henka_authoring_mesh_get_vertex_edge_count(mesh, vertex_id);
    if (edge_count < 2U || edge_count > desc.max_faces + 1U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < edge_count; ++index)
    {
        henka_authoring_edge_id edge_id;
        const henka_authoring_edge* edge;
        if (henka_authoring_mesh_get_vertex_edge_at(
                mesh, vertex_id, index, &edge_id) != HENKA_SUCCESS ||
            (edge = henka_authoring_mesh_get_edge(mesh, edge_id)) == NULL ||
            edge->face_count == 0U || edge->face_count > 2U)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (edge->face_count == 1U) ++boundary_count;
    }
    if (boundary_count != 2U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    fan_faces = henka_calloc(desc.max_faces, sizeof(*fan_faces));
    if (fan_faces == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (face_slot = 0U; face_slot < desc.max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face;
        size_t face_corner;
        if (henka_authoring_mesh_get_face_id_at(mesh, face_slot, &face_id) != HENKA_SUCCESS ||
            (face = henka_authoring_mesh_get_face(mesh, face_id)) == NULL ||
            !modeling_face_contains_vertex(face, vertex_id, &face_corner))
        {
            continue;
        }
        if (incident_face_count >= desc.max_faces || face->corner_count < 3U ||
            face->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        fan_faces[incident_face_count].face_id = face_id;
        fan_faces[incident_face_count].selected_corner = face_corner;
        fan_faces[incident_face_count].corner_count = face->corner_count;
        ++incident_face_count;
    }
    if (incident_face_count == 0U || edge_count != incident_face_count + 1U)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }

    qsort(fan_faces, incident_face_count, sizeof(*fan_faces), modeling_vertex_fan_face_compare);
    visited = henka_calloc(incident_face_count, sizeof(*visited));
    {
        size_t pending_bytes;
        if (!henka_checked_size_multiply(incident_face_count, sizeof(*pending), &pending_bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        pending = henka_malloc(pending_bytes);
    }
    if (visited == NULL || pending == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    pending[0] = 0U;
    visited[0] = true;
    {
        size_t pending_count = 1U;
        size_t pending_index = 0U;
        size_t visited_count = 1U;
        while (pending_index < pending_count)
        {
            const size_t fan_index = pending[pending_index++];
            const henka_authoring_face* face = henka_authoring_mesh_get_face(
                mesh, fan_faces[fan_index].face_id);
            size_t selected_corner;
            size_t radial_corners[2];
            size_t radial_index;
            if (face == NULL)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            selected_corner = fan_faces[fan_index].selected_corner;
            radial_corners[0] =
                (selected_corner + face->corner_count - 1U) % face->corner_count;
            radial_corners[1] = selected_corner;
            for (radial_index = 0U; radial_index < 2U; ++radial_index)
            {
                const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(
                    mesh, face->edges[radial_corners[radial_index]]);
                henka_authoring_face_id other_face_id;
                size_t other_index;
                if (edge == NULL || edge->face_count == 1U) continue;
                if (edge->face_count != 2U ||
                    (edge->faces[0] != face->id && edge->faces[1] != face->id))
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                    goto cleanup;
                }
                other_face_id = edge->faces[0] == face->id ? edge->faces[1] : edge->faces[0];
                other_index = modeling_vertex_fan_face_index(
                    fan_faces, incident_face_count, other_face_id);
                if (other_index == SIZE_MAX)
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (!visited[other_index])
                {
                    if (pending_count >= incident_face_count)
                    {
                        result = HENKA_ERROR_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                    visited[other_index] = true;
                    pending[pending_count++] = other_index;
                    ++visited_count;
                }
            }
        }
        if (visited_count != incident_face_count)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }

    {
        size_t value_count;
        size_t bytes;
        if (!henka_checked_size_multiply(
                incident_face_count, desc.max_face_corners, &value_count) ||
            !henka_checked_size_multiply(value_count, sizeof(*fan_vertices), &bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        fan_vertices = henka_calloc(value_count, sizeof(*fan_vertices));
        fan_uvs = henka_calloc(value_count, sizeof(*fan_uvs));
        fan_edge_hard = henka_calloc(value_count, sizeof(*fan_edge_hard));
        if (fan_vertices == NULL || fan_uvs == NULL || fan_edge_hard == NULL)
        {
            result = HENKA_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }
    }

    boundary_count = 0U;
    for (index = 0U; index < incident_face_count; ++index)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(mesh, fan_faces[index].face_id);
        const size_t selected_corner = fan_faces[index].selected_corner;
        const size_t previous_corner =
            (selected_corner + face->corner_count - 1U) % face->corner_count;
        const henka_authoring_edge* previous_edge = henka_authoring_mesh_get_edge(
            mesh, face->edges[previous_corner]);
        const henka_authoring_edge* next_edge = henka_authoring_mesh_get_edge(
            mesh, face->edges[selected_corner]);
        henka_vec3 normal;
        if (face == NULL || previous_edge == NULL || next_edge == NULL)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        normal = modeling_face_normal(mesh, face);
        if (!modeling_finite_vec3(normal) || henka_vec3_length(normal) <= 1.0e-7f ||
            (index > 0U && henka_vec3_dot(fan_normal, normal) < 0.999f))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        fan_normal = index == 0U ? normal : henka_vec3_add(fan_normal, normal);
        if (previous_edge->face_count == 1U)
        {
            if (boundary_count >= 2U)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            boundary_face_indices[boundary_count] = index;
            boundary_face_positions[boundary_count++] = previous_corner;
        }
        if (next_edge->face_count == 1U)
        {
            if (boundary_count >= 2U)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            boundary_face_indices[boundary_count] = index;
            boundary_face_positions[boundary_count++] = selected_corner;
        }
        fan_faces[index].corner_count = face->corner_count;
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const size_t value_index = index * desc.max_face_corners + corner;
            fan_vertices[value_index] = face->vertices[corner];
            fan_uvs[value_index] = face->uvs[corner];
            fan_edge_hard[value_index] = henka_authoring_mesh_get_edge(
                mesh, face->edges[corner])->hard;
        }
    }
    if (boundary_count != 2U || henka_vec3_length(fan_normal) <= 1.0e-7f)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    fan_normal = henka_vec3_normalize(fan_normal);
    offset = henka_vec3_scale(fan_normal, distance);
    before = henka_authoring_mesh_get_counts(mesh);
    if (!modeling_finite_vec3(offset) || before.vertices >= desc.max_vertices ||
        desc.max_faces < 2U ||
        before.faces > desc.max_faces - 2U || before.edges > desc.max_edges - 2U)
    {
        result = HENKA_ERROR_LIMIT;
        goto cleanup;
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    for (index = 0U; result == HENKA_SUCCESS && index < incident_face_count; ++index)
    {
        result = henka_authoring_mesh_remove_face(candidate, fan_faces[index].face_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_add_vertex(
            candidate,
            henka_vec3_add(source_vertex->position, offset),
            source_vertex->uv,
            source_vertex->material_region,
            &new_vertex_id);
    }
    for (index = 0U; result == HENKA_SUCCESS && index < incident_face_count; ++index)
    {
        henka_authoring_vertex_id cap[HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
        const size_t base = index * desc.max_face_corners;
        const modeling_vertex_fan_face* fan_face = &fan_faces[index];
        const henka_authoring_face* source_face = henka_authoring_mesh_get_face(
            mesh, fan_face->face_id);
        for (corner = 0U; corner < fan_face->corner_count; ++corner)
        {
            cap[corner] = fan_vertices[base + corner];
        }
        cap[fan_face->selected_corner] = new_vertex_id;
        result = henka_authoring_mesh_add_face(
            candidate, cap, fan_face->corner_count, source_face->material_region,
            source_face->smooth, &side_face_id);
        if (index == 0U) cap_face_id = side_face_id;
        if (result == HENKA_SUCCESS)
        {
            for (corner = 0U; corner < fan_face->corner_count; ++corner)
            {
                result = henka_authoring_mesh_set_face_corner_uv(
                    candidate, side_face_id, corner, fan_uvs[base + corner]);
                if (result != HENKA_SUCCESS) break;
            }
        }
    }
    for (index = 0U; result == HENKA_SUCCESS && index < 2U; ++index)
    {
        const size_t fan_index = boundary_face_indices[index];
        const size_t base = fan_index * desc.max_face_corners;
        const modeling_vertex_fan_face* fan_face = &fan_faces[fan_index];
        const size_t selected_corner = fan_face->selected_corner;
        const size_t edge_corner = boundary_face_positions[index];
        const size_t next_corner = (selected_corner + 1U) % fan_face->corner_count;
        henka_authoring_vertex_id side[3];
        henka_vec2 side_uvs[3];
        if (edge_corner == selected_corner)
        {
            side[0] = fan_vertices[base + selected_corner];
            side[1] = fan_vertices[base + next_corner];
            side_uvs[0] = fan_uvs[base + selected_corner];
            side_uvs[1] = fan_uvs[base + next_corner];
        }
        else
        {
            side[0] = fan_vertices[base + edge_corner];
            side[1] = fan_vertices[base + selected_corner];
            side_uvs[0] = fan_uvs[base + edge_corner];
            side_uvs[1] = fan_uvs[base + selected_corner];
        }
        side[2] = new_vertex_id;
        side_uvs[2] = fan_uvs[base + selected_corner];
        result = henka_authoring_mesh_add_face(
            candidate, side, 3U, henka_authoring_mesh_get_face(
                mesh, fan_face->face_id)->material_region, false, &side_face_id);
        for (corner = 0U; result == HENKA_SUCCESS && corner < 3U; ++corner)
        {
            result = henka_authoring_mesh_set_face_corner_uv(
                candidate, side_face_id, corner, side_uvs[corner]);
        }
    }
    for (index = 0U; result == HENKA_SUCCESS && index < incident_face_count; ++index)
    {
        const size_t base = index * desc.max_face_corners;
        const modeling_vertex_fan_face* fan_face = &fan_faces[index];
        for (corner = 0U; corner < fan_face->corner_count; ++corner)
        {
            const size_t next_corner = (corner + 1U) % fan_face->corner_count;
            const henka_authoring_vertex_id original_first = fan_vertices[base + corner];
            const henka_authoring_vertex_id original_second = fan_vertices[base + next_corner];
            const henka_authoring_vertex_id cap_first = corner == fan_face->selected_corner
                ? new_vertex_id : original_first;
            const henka_authoring_vertex_id cap_second = next_corner == fan_face->selected_corner
                ? new_vertex_id : original_second;
            henka_authoring_edge_id edge_id = modeling_find_edge_between_vertices(
                candidate, original_first, original_second);
            henka_authoring_edge_id cap_edge_id = modeling_find_edge_between_vertices(
                candidate, cap_first, cap_second);
            if (cap_edge_id == HENKA_AUTHORING_INVALID_ID ||
                (edge_id != HENKA_AUTHORING_INVALID_ID &&
                    henka_authoring_mesh_set_edge_hard(candidate, edge_id, fan_edge_hard[base + corner]) != HENKA_SUCCESS) ||
                henka_authoring_mesh_set_edge_hard(candidate, cap_edge_id, fan_edge_hard[base + corner]) != HENKA_SUCCESS)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                break;
            }
        }
    }
    if (result == HENKA_SUCCESS &&
        (!henka_authoring_mesh_validate(candidate) ||
         !modeling_face_geometry_is_valid(candidate)))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        after = henka_authoring_mesh_get_counts(candidate);
        result = modeling_commit(mesh, candidate);
        candidate = NULL;
        if (result == HENKA_SUCCESS)
        {
            *out_new_vertex_id = new_vertex_id;
            modeling_report_count_delta(&before, &after, out_report);
            if (out_report != NULL)
            {
                out_report->primary_vertex_id = new_vertex_id;
                out_report->primary_face_id = cap_face_id;
            }
        }
    }
cleanup:
    henka_authoring_mesh_destroy(candidate);
    henka_free(pending);
    henka_free(visited);
    henka_free(fan_edge_hard);
    henka_free(fan_uvs);
    henka_free(fan_vertices);
    henka_free(fan_faces);
    return result;
}

henka_result henka_authoring_mesh_extrude_loose_vertex(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id vertex_id,
    henka_vec3 direction,
    float distance,
    henka_authoring_vertex_id* out_new_vertex_id,
    henka_authoring_edge_id* out_new_edge_id,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_mesh_desc desc = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    const henka_authoring_vertex* source_vertex;
    henka_authoring_vertex_id new_vertex_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id new_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_vec3 offset;
    float direction_length;
    henka_result result;

    if (out_new_vertex_id != NULL)
    {
        *out_new_vertex_id = HENKA_AUTHORING_INVALID_ID;
    }
    if (out_new_edge_id != NULL)
    {
        *out_new_edge_id = HENKA_AUTHORING_INVALID_ID;
    }
    modeling_report_reset(out_report);
    if (mesh == NULL || out_new_vertex_id == NULL || out_new_edge_id == NULL ||
        !modeling_finite_vec3(direction) || !modeling_finite_scalar(distance) ||
        fabsf(distance) <= 1.0e-7f || !henka_authoring_mesh_validate(mesh) ||
        !modeling_face_geometry_is_valid(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    source_vertex = henka_authoring_mesh_get_vertex(mesh, vertex_id);
    if (source_vertex == NULL ||
        henka_authoring_mesh_get_vertex_edge_count(mesh, vertex_id) != 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    direction_length = henka_vec3_length(direction);
    if (!isfinite(direction_length) || direction_length <= 1.0e-7f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    offset = henka_vec3_scale(
        henka_vec3_scale(direction, 1.0f / direction_length), distance);
    if (!modeling_finite_vec3(offset))
    {
        return HENKA_ERROR_LIMIT;
    }
    desc = henka_authoring_mesh_get_desc(mesh);
    before = henka_authoring_mesh_get_counts(mesh);
    if (desc.max_vertices == 0U || desc.max_edges == 0U ||
        before.vertices >= desc.max_vertices || before.edges >= desc.max_edges)
    {
        return HENKA_ERROR_LIMIT;
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_add_vertex(
            candidate,
            henka_vec3_add(source_vertex->position, offset),
            source_vertex->uv,
            source_vertex->material_region,
            &new_vertex_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_add_edge(
            candidate, vertex_id, new_vertex_id, false, &new_edge_id);
    }
    if (result == HENKA_SUCCESS &&
        (!henka_authoring_mesh_validate(candidate) ||
         !modeling_face_geometry_is_valid(candidate)))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        after = henka_authoring_mesh_get_counts(candidate);
        result = modeling_commit(mesh, candidate);
        candidate = NULL;
        if (result == HENKA_SUCCESS)
        {
            *out_new_vertex_id = new_vertex_id;
            *out_new_edge_id = new_edge_id;
            modeling_report_count_delta(&before, &after, out_report);
            if (out_report != NULL)
            {
                out_report->primary_vertex_id = new_vertex_id;
                out_report->primary_edge_id = new_edge_id;
            }
        }
    }
    henka_authoring_mesh_destroy(candidate);
    return result;
}

henka_result henka_authoring_mesh_extrude_loose_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    henka_vec3 direction,
    float distance,
    henka_authoring_edge_id* out_new_edge_id,
    henka_authoring_face_id* out_new_face_id,
    henka_authoring_modeling_report* out_report)
{
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_mesh_desc desc = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    const henka_authoring_edge* source_edge;
    const henka_authoring_vertex* source_first;
    const henka_authoring_vertex* source_second;
    henka_authoring_vertex_id new_vertices[2] = {
        HENKA_AUTHORING_INVALID_ID, HENKA_AUTHORING_INVALID_ID};
    henka_authoring_edge_id new_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id face_vertices[4];
    henka_vec2 face_uvs[4];
    henka_vec3 offset;
    float direction_length;
    henka_result result;
    size_t corner;

    if (out_new_edge_id != NULL)
    {
        *out_new_edge_id = HENKA_AUTHORING_INVALID_ID;
    }
    if (out_new_face_id != NULL)
    {
        *out_new_face_id = HENKA_AUTHORING_INVALID_ID;
    }
    modeling_report_reset(out_report);
    if (mesh == NULL || out_new_edge_id == NULL || out_new_face_id == NULL ||
        !modeling_finite_vec3(direction) || !modeling_finite_scalar(distance) ||
        fabsf(distance) <= 1.0e-7f || !henka_authoring_mesh_validate(mesh) ||
        !modeling_face_geometry_is_valid(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    source_edge = henka_authoring_mesh_get_edge(mesh, edge_id);
    if (source_edge == NULL || source_edge->face_count != 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    source_first = henka_authoring_mesh_get_vertex(mesh, source_edge->vertices[0]);
    source_second = henka_authoring_mesh_get_vertex(mesh, source_edge->vertices[1]);
    if (source_first == NULL || source_second == NULL ||
        source_first->material_region != source_second->material_region)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    direction_length = henka_vec3_length(direction);
    if (!isfinite(direction_length) || direction_length <= 1.0e-7f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    offset = henka_vec3_scale(
        henka_vec3_scale(direction, 1.0f / direction_length), distance);
    if (!modeling_finite_vec3(offset))
    {
        return HENKA_ERROR_LIMIT;
    }
    desc = henka_authoring_mesh_get_desc(mesh);
    before = henka_authoring_mesh_get_counts(mesh);
    if (desc.max_vertices < 2U || desc.max_edges < 3U || desc.max_faces == 0U ||
        before.vertices > desc.max_vertices - 2U ||
        before.edges > desc.max_edges - 3U ||
        before.faces >= desc.max_faces)
    {
        return HENKA_ERROR_LIMIT;
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_add_vertex(
            candidate,
            henka_vec3_add(source_first->position, offset),
            source_first->uv,
            source_first->material_region,
            &new_vertices[0]);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_add_vertex(
            candidate,
            henka_vec3_add(source_second->position, offset),
            source_second->uv,
            source_second->material_region,
            &new_vertices[1]);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_add_edge(
            candidate, new_vertices[0], new_vertices[1], source_edge->hard,
            &new_edge_id);
    }
    if (result == HENKA_SUCCESS)
    {
        face_vertices[0] = source_edge->vertices[0];
        face_vertices[1] = source_edge->vertices[1];
        face_vertices[2] = new_vertices[1];
        face_vertices[3] = new_vertices[0];
        face_uvs[0] = source_first->uv;
        face_uvs[1] = source_second->uv;
        face_uvs[2] = source_second->uv;
        face_uvs[3] = source_first->uv;
        result = henka_authoring_mesh_add_face(
            candidate, face_vertices, 4U, source_first->material_region, false,
            &new_face_id);
    }
    for (corner = 0U; result == HENKA_SUCCESS && corner < 4U; ++corner)
    {
        result = henka_authoring_mesh_set_face_corner_uv(
            candidate, new_face_id, corner, face_uvs[corner]);
    }
    if (result == HENKA_SUCCESS &&
        (!henka_authoring_mesh_validate(candidate) ||
         !modeling_face_geometry_is_valid(candidate)))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        after = henka_authoring_mesh_get_counts(candidate);
        result = modeling_commit(mesh, candidate);
        candidate = NULL;
        if (result == HENKA_SUCCESS)
        {
            *out_new_edge_id = new_edge_id;
            *out_new_face_id = new_face_id;
            modeling_report_count_delta(&before, &after, out_report);
            if (out_report != NULL)
            {
                out_report->primary_edge_id = new_edge_id;
                out_report->primary_face_id = new_face_id;
            }
        }
    }
    henka_authoring_mesh_destroy(candidate);
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
