#include <henka/authoring_mesh.h>

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <stdatomic.h>
#include <unistd.h>
#endif

#include <henka/memory.h>
#include <henka/persistence.h>

#include "../core/checked.h"
#include "authoring_mesh_internal.h"

#define HENKA_AUTHORING_MESH_FILE_VERSION 3U
#define HENKA_AUTHORING_MESH_LEGACY_FILE_VERSION 2U
#define HENKA_AUTHORING_TEMP_PATH_SUFFIX_CAPACITY 96U

#ifdef _WIN32
static volatile LONG g_authoring_save_sequence = 0L;
#else
static atomic_uint g_authoring_save_sequence = 0U;
#endif

typedef struct authoring_edge_lookup_entry
{
    henka_authoring_vertex_id low;
    henka_authoring_vertex_id high;
    henka_authoring_edge_id edge_id;
    bool occupied;
} authoring_edge_lookup_entry;

struct henka_authoring_mesh
{
    henka_authoring_mesh_desc desc;
    henka_authoring_vertex* vertices;
    henka_authoring_edge* edges;
    henka_authoring_face* faces;
    authoring_edge_lookup_entry* edge_lookup;
    size_t edge_lookup_capacity;
    bool edge_lookup_ready;
    size_t vertex_slots;
    size_t edge_slots;
    size_t face_slots;
};

static bool authoring_finite_vec2(henka_vec2 value)
{
    return isfinite(value.x) && isfinite(value.y);
}

static bool authoring_finite_vec3(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool authoring_desc_valid(const henka_authoring_mesh_desc* desc)
{
    return desc != NULL && desc->max_vertices > 0U &&
        desc->max_vertices <= HENKA_AUTHORING_MESH_HARD_MAX_VERTICES &&
        desc->max_edges > 0U && desc->max_edges <= HENKA_AUTHORING_MESH_HARD_MAX_EDGES &&
        desc->max_faces > 0U && desc->max_faces <= HENKA_AUTHORING_MESH_HARD_MAX_FACES &&
        desc->max_face_corners >= 3U &&
        desc->max_face_corners <= HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS;
}

static size_t authoring_vertex_slot(const henka_authoring_mesh* mesh, henka_authoring_vertex_id id)
{
    (void)mesh;
    if (id == HENKA_AUTHORING_INVALID_ID || id == 0U)
    {
        return SIZE_MAX;
    }
    return (size_t)id - 1U;
}

static size_t authoring_edge_slot(const henka_authoring_mesh* mesh, henka_authoring_edge_id id)
{
    (void)mesh;
    if (id == HENKA_AUTHORING_INVALID_ID || id == 0U)
    {
        return SIZE_MAX;
    }
    return (size_t)id - 1U;
}

static size_t authoring_face_slot(const henka_authoring_mesh* mesh, henka_authoring_face_id id)
{
    (void)mesh;
    if (id == HENKA_AUTHORING_INVALID_ID || id == 0U)
    {
        return SIZE_MAX;
    }
    return (size_t)id - 1U;
}

static size_t authoring_edge_lookup_capacity_for(size_t max_edges)
{
    size_t capacity = 1U;

    if (max_edges > SIZE_MAX / 2U)
    {
        return 0U;
    }
    while (capacity < max_edges * 2U)
    {
        if (capacity > SIZE_MAX / 2U)
        {
            return 0U;
        }
        capacity <<= 1U;
    }
    return capacity;
}

static size_t authoring_edge_lookup_hash(
    henka_authoring_vertex_id low,
    henka_authoring_vertex_id high,
    size_t capacity)
{
    uint64_t key = ((uint64_t)low << 32U) | (uint64_t)high;

    key ^= key >> 30U;
    key *= UINT64_C(0xbf58476d1ce4e5b9);
    key ^= key >> 27U;
    key *= UINT64_C(0x94d049bb133111eb);
    key ^= key >> 31U;
    return (size_t)(key & (uint64_t)(capacity - 1U));
}

static bool authoring_edge_lookup_insert_raw(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id low,
    henka_authoring_vertex_id high,
    henka_authoring_edge_id edge_id)
{
    size_t probe;

    if (mesh == NULL || mesh->edge_lookup == NULL || mesh->edge_lookup_capacity == 0U)
    {
        return false;
    }
    probe = authoring_edge_lookup_hash(low, high, mesh->edge_lookup_capacity);
    for (size_t attempt = 0U; attempt < mesh->edge_lookup_capacity; ++attempt)
    {
        authoring_edge_lookup_entry* entry = &mesh->edge_lookup[probe];
        if (!entry->occupied || (entry->low == low && entry->high == high))
        {
            entry->low = low;
            entry->high = high;
            entry->edge_id = edge_id;
            entry->occupied = true;
            return true;
        }
        probe = (probe + 1U) & (mesh->edge_lookup_capacity - 1U);
    }
    return false;
}

static bool authoring_edge_lookup_rebuild(henka_authoring_mesh* mesh)
{
    size_t index;

    if (mesh == NULL || mesh->edge_lookup == NULL || mesh->edge_lookup_capacity == 0U)
    {
        return false;
    }
    memset(
        mesh->edge_lookup,
        0,
        mesh->edge_lookup_capacity * sizeof(*mesh->edge_lookup));
    for (index = 0U; index < mesh->edge_slots; ++index)
    {
        const henka_authoring_edge* edge = &mesh->edges[index];
        if (edge->active && !authoring_edge_lookup_insert_raw(
                mesh,
                edge->vertices[0],
                edge->vertices[1],
                edge->id))
        {
            return false;
        }
    }
    mesh->edge_lookup_ready = true;
    return true;
}

static bool authoring_edge_lookup_insert(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id first,
    henka_authoring_vertex_id second,
    henka_authoring_edge_id edge_id)
{
    const henka_authoring_vertex_id low = first < second ? first : second;
    const henka_authoring_vertex_id high = first < second ? second : first;

    if (mesh == NULL)
    {
        return false;
    }
    if (!mesh->edge_lookup_ready && !authoring_edge_lookup_rebuild(mesh))
    {
        return false;
    }
    return authoring_edge_lookup_insert_raw(mesh, low, high, edge_id);
}

static henka_authoring_vertex* authoring_vertex(henka_authoring_mesh* mesh, henka_authoring_vertex_id id)
{
    size_t slot = authoring_vertex_slot(mesh, id);
    if (mesh == NULL || slot >= mesh->vertex_slots)
    {
        return NULL;
    }
    return &mesh->vertices[slot];
}

static const henka_authoring_vertex* authoring_vertex_const(const henka_authoring_mesh* mesh, henka_authoring_vertex_id id)
{
    size_t slot = authoring_vertex_slot(mesh, id);
    if (mesh == NULL || slot >= mesh->vertex_slots)
    {
        return NULL;
    }
    return &mesh->vertices[slot];
}

static henka_authoring_edge* authoring_edge(henka_authoring_mesh* mesh, henka_authoring_edge_id id)
{
    size_t slot = authoring_edge_slot(mesh, id);
    if (mesh == NULL || slot >= mesh->edge_slots)
    {
        return NULL;
    }
    return &mesh->edges[slot];
}

static const henka_authoring_edge* authoring_edge_const(const henka_authoring_mesh* mesh, henka_authoring_edge_id id)
{
    size_t slot = authoring_edge_slot(mesh, id);
    if (mesh == NULL || slot >= mesh->edge_slots)
    {
        return NULL;
    }
    return &mesh->edges[slot];
}

static henka_authoring_face* authoring_face(henka_authoring_mesh* mesh, henka_authoring_face_id id)
{
    size_t slot = authoring_face_slot(mesh, id);
    if (mesh == NULL || slot >= mesh->face_slots)
    {
        return NULL;
    }
    return &mesh->faces[slot];
}

static const henka_authoring_face* authoring_face_const(const henka_authoring_mesh* mesh, henka_authoring_face_id id)
{
    size_t slot = authoring_face_slot(mesh, id);
    if (mesh == NULL || slot >= mesh->face_slots)
    {
        return NULL;
    }
    return &mesh->faces[slot];
}

static size_t authoring_active_vertex_count(const henka_authoring_mesh* mesh)
{
    size_t count = 0U;
    size_t index;
    for (index = 0U; index < mesh->vertex_slots; ++index)
    {
        count += mesh->vertices[index].active ? 1U : 0U;
    }
    return count;
}

static size_t authoring_active_edge_count(const henka_authoring_mesh* mesh)
{
    size_t count = 0U;
    size_t index;
    for (index = 0U; index < mesh->edge_slots; ++index)
    {
        count += mesh->edges[index].active ? 1U : 0U;
    }
    return count;
}

static size_t authoring_active_face_count(const henka_authoring_mesh* mesh)
{
    size_t count = 0U;
    size_t index;
    for (index = 0U; index < mesh->face_slots; ++index)
    {
        count += mesh->faces[index].active ? 1U : 0U;
    }
    return count;
}

static henka_authoring_edge* authoring_find_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id first,
    henka_authoring_vertex_id second)
{
    size_t index;
    henka_authoring_vertex_id low = first < second ? first : second;
    henka_authoring_vertex_id high = first < second ? second : first;

    if (mesh != NULL && mesh->edge_lookup != NULL && mesh->edge_lookup_capacity > 0U)
    {
        size_t probe;

        if (!mesh->edge_lookup_ready && !authoring_edge_lookup_rebuild(mesh))
        {
            goto linear_search;
        }
        probe = authoring_edge_lookup_hash(low, high, mesh->edge_lookup_capacity);
        for (size_t attempt = 0U; attempt < mesh->edge_lookup_capacity; ++attempt)
        {
            const authoring_edge_lookup_entry* entry = &mesh->edge_lookup[probe];
            if (!entry->occupied)
            {
                return NULL;
            }
            if (entry->low == low && entry->high == high)
            {
                henka_authoring_edge* edge = authoring_edge(mesh, entry->edge_id);
                return edge != NULL && edge->active ? edge : NULL;
            }
            probe = (probe + 1U) & (mesh->edge_lookup_capacity - 1U);
        }
        return NULL;
    }

linear_search:
    for (index = 0U; index < mesh->edge_slots; ++index)
    {
        henka_authoring_edge* edge = &mesh->edges[index];
        if (edge->active && edge->vertices[0] == low && edge->vertices[1] == high)
        {
            return edge;
        }
    }
    return NULL;
}

static henka_result authoring_append_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id first,
    henka_authoring_vertex_id second,
    henka_authoring_face_id face_id,
    henka_authoring_edge_id* out_id)
{
    henka_authoring_edge* edge;
    henka_authoring_vertex_id low = first < second ? first : second;
    henka_authoring_vertex_id high = first < second ? second : first;
    if (mesh->edge_slots >= mesh->desc.max_edges)
    {
        return HENKA_ERROR_LIMIT;
    }
    edge = &mesh->edges[mesh->edge_slots];
    memset(edge, 0, sizeof(*edge));
    edge->id = (henka_authoring_edge_id)(mesh->edge_slots + 1U);
    edge->vertices[0] = low;
    edge->vertices[1] = high;
    edge->faces[0] = face_id;
    edge->face_count = 1U;
    edge->active = true;
    ++mesh->edge_slots;
    if (!authoring_edge_lookup_insert(mesh, low, high, edge->id))
    {
        --mesh->edge_slots;
        memset(edge, 0, sizeof(*edge));
        return HENKA_ERROR_LIMIT;
    }
    *out_id = edge->id;
    return HENKA_SUCCESS;
}

henka_authoring_mesh_desc henka_authoring_mesh_desc_default(void)
{
    return (henka_authoring_mesh_desc){
        HENKA_AUTHORING_MESH_DEFAULT_MAX_VERTICES,
        HENKA_AUTHORING_MESH_DEFAULT_MAX_EDGES,
        HENKA_AUTHORING_MESH_DEFAULT_MAX_FACES,
        HENKA_AUTHORING_MESH_DEFAULT_MAX_FACE_CORNERS};
}

henka_result henka_authoring_mesh_create(const henka_authoring_mesh_desc* desc, henka_authoring_mesh** out_mesh)
{
    henka_authoring_mesh* mesh;
    size_t edge_lookup_capacity;
    if (!authoring_desc_valid(desc) || out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_mesh = NULL;
    edge_lookup_capacity = authoring_edge_lookup_capacity_for(desc->max_edges);
    if (edge_lookup_capacity == 0U)
    {
        return HENKA_ERROR_LIMIT;
    }
    mesh = henka_calloc(1U, sizeof(*mesh));
    if (mesh == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    mesh->desc = *desc;
    mesh->vertices = henka_calloc(desc->max_vertices, sizeof(*mesh->vertices));
    mesh->edges = henka_calloc(desc->max_edges, sizeof(*mesh->edges));
    mesh->faces = henka_calloc(desc->max_faces, sizeof(*mesh->faces));
    mesh->edge_lookup = henka_calloc(
        edge_lookup_capacity,
        sizeof(*mesh->edge_lookup));
    mesh->edge_lookup_capacity = edge_lookup_capacity;
    mesh->edge_lookup_ready = true;
    if (mesh->vertices == NULL || mesh->edges == NULL || mesh->faces == NULL ||
        mesh->edge_lookup == NULL)
    {
        henka_free(mesh->vertices);
        henka_free(mesh->edges);
        henka_free(mesh->faces);
        henka_free(mesh->edge_lookup);
        henka_free(mesh);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    *out_mesh = mesh;
    return HENKA_SUCCESS;
}

void henka_authoring_mesh_destroy(henka_authoring_mesh* mesh)
{
    size_t index;
    if (mesh == NULL)
    {
        return;
    }
    for (index = 0U; index < mesh->face_slots; ++index)
    {
        henka_free(mesh->faces[index].vertices);
        henka_free(mesh->faces[index].edges);
        henka_free(mesh->faces[index].uvs);
    }
    henka_free(mesh->vertices);
    henka_free(mesh->edges);
    henka_free(mesh->faces);
    henka_free(mesh->edge_lookup);
    henka_free(mesh);
}

henka_authoring_mesh_counts henka_authoring_mesh_get_counts(const henka_authoring_mesh* mesh)
{
    henka_authoring_mesh_counts counts = {0};
    if (mesh != NULL)
    {
        counts.vertices = authoring_active_vertex_count(mesh);
        counts.edges = authoring_active_edge_count(mesh);
        counts.faces = authoring_active_face_count(mesh);
    }
    return counts;
}

henka_authoring_mesh_desc henka_authoring_mesh_get_desc(const henka_authoring_mesh* mesh)
{
    return mesh != NULL ? mesh->desc : (henka_authoring_mesh_desc){0};
}

bool henka_authoring_mesh_validate(const henka_authoring_mesh* mesh)
{
    size_t index;
    if (mesh == NULL || !authoring_desc_valid(&mesh->desc) ||
        mesh->vertex_slots > mesh->desc.max_vertices ||
        mesh->edge_slots > mesh->desc.max_edges || mesh->face_slots > mesh->desc.max_faces)
    {
        return false;
    }
    for (index = 0U; index < mesh->vertex_slots; ++index)
    {
        const henka_authoring_vertex* vertex = &mesh->vertices[index];
        if (vertex->active && (vertex->id != (henka_authoring_vertex_id)(index + 1U) ||
            !authoring_finite_vec3(vertex->position) || !authoring_finite_vec2(vertex->uv)))
        {
            return false;
        }
    }
    for (index = 0U; index < mesh->edge_slots; ++index)
    {
        const henka_authoring_edge* edge = &mesh->edges[index];
        size_t face_index;
        if (!edge->active)
        {
            continue;
        }
        if (edge->id != (henka_authoring_edge_id)(index + 1U) || edge->vertices[0] >= edge->vertices[1] ||
            edge->face_count == 0U || edge->face_count > 2U ||
            henka_authoring_mesh_get_vertex(mesh, edge->vertices[0]) == NULL ||
            henka_authoring_mesh_get_vertex(mesh, edge->vertices[1]) == NULL)
        {
            return false;
        }
        for (face_index = 0U; face_index < edge->face_count; ++face_index)
        {
            const henka_authoring_face* face = authoring_face_const(mesh, edge->faces[face_index]);
            size_t corner;
            bool found = false;
            if (face == NULL || !face->active)
            {
                return false;
            }
            for (corner = 0U; corner < face->corner_count; ++corner)
            {
                if (face->edges[corner] == edge->id)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                return false;
            }
        }
    }
    for (index = 0U; index < mesh->face_slots; ++index)
    {
        const henka_authoring_face* face = &mesh->faces[index];
        size_t corner;
        if (!face->active)
        {
            continue;
        }
        if (face->id != (henka_authoring_face_id)(index + 1U) || face->corner_count < 3U ||
            face->corner_count > mesh->desc.max_face_corners || face->vertices == NULL ||
            face->edges == NULL || face->uvs == NULL)
        {
            return false;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            size_t next = (corner + 1U) % face->corner_count;
            const henka_authoring_edge* edge = authoring_edge_const(mesh, face->edges[corner]);
            const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(mesh, face->vertices[corner]);
            const henka_authoring_vertex* next_vertex = henka_authoring_mesh_get_vertex(mesh, face->vertices[next]);
            henka_authoring_vertex_id low = face->vertices[corner] < face->vertices[next] ?
                face->vertices[corner] : face->vertices[next];
            henka_authoring_vertex_id high = face->vertices[corner] < face->vertices[next] ?
                face->vertices[next] : face->vertices[corner];
            if (vertex == NULL || next_vertex == NULL || edge == NULL ||
                edge->vertices[0] != low || edge->vertices[1] != high ||
                !authoring_finite_vec2(face->uvs[corner]))
            {
                return false;
            }
        }
    }
    return true;
}

henka_result henka_authoring_mesh_get_bounds(
    const henka_authoring_mesh* mesh,
    henka_vec3* out_center,
    henka_vec3* out_extents)
{
    henka_vec3 minimum = {0.0f, 0.0f, 0.0f};
    henka_vec3 maximum = {0.0f, 0.0f, 0.0f};
    size_t index;
    bool found = false;

    if (mesh == NULL || out_center == NULL || out_extents == NULL ||
        !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < mesh->vertex_slots; ++index)
    {
        const henka_authoring_vertex* vertex = &mesh->vertices[index];
        if (!vertex->active)
        {
            continue;
        }
        if (!found)
        {
            minimum = vertex->position;
            maximum = vertex->position;
            found = true;
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
    }
    if (!found)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_center = (henka_vec3){
        (minimum.x + maximum.x) * 0.5f,
        (minimum.y + maximum.y) * 0.5f,
        (minimum.z + maximum.z) * 0.5f};
    *out_extents = (henka_vec3){
        (maximum.x - minimum.x) * 0.5f,
        (maximum.y - minimum.y) * 0.5f,
        (maximum.z - minimum.z) * 0.5f};
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_add_vertex(henka_authoring_mesh* mesh, henka_vec3 position, henka_vec2 uv, uint32_t material_region, henka_authoring_vertex_id* out_id)
{
    henka_authoring_vertex* vertex;
    if (mesh == NULL || out_id == NULL || !authoring_finite_vec3(position) || !authoring_finite_vec2(uv))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (mesh->vertex_slots >= mesh->desc.max_vertices || mesh->vertex_slots >= UINT32_MAX - 1U)
    {
        return HENKA_ERROR_LIMIT;
    }
    vertex = &mesh->vertices[mesh->vertex_slots];
    memset(vertex, 0, sizeof(*vertex));
    vertex->id = (henka_authoring_vertex_id)(mesh->vertex_slots + 1U);
    vertex->position = position;
    vertex->uv = uv;
    vertex->material_region = material_region;
    vertex->active = true;
    ++mesh->vertex_slots;
    *out_id = vertex->id;
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_remove_vertex(henka_authoring_mesh* mesh, henka_authoring_vertex_id id)
{
    henka_authoring_vertex* vertex = authoring_vertex(mesh, id);
    size_t index;
    size_t corner;
    if (vertex == NULL || !vertex->active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < mesh->face_slots; ++index)
    {
        const henka_authoring_face* face = &mesh->faces[index];
        if (!face->active)
        {
            continue;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            if (face->vertices[corner] == id)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    vertex->active = false;
    return HENKA_SUCCESS;
}

const henka_authoring_vertex* henka_authoring_mesh_get_vertex(const henka_authoring_mesh* mesh, henka_authoring_vertex_id id)
{
    const henka_authoring_vertex* vertex = authoring_vertex_const(mesh, id);
    return vertex != NULL && vertex->active ? vertex : NULL;
}

henka_result henka_authoring_mesh_set_vertex_position(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id id,
    henka_vec3 position)
{
    henka_authoring_vertex* vertex = authoring_vertex(mesh, id);
    if (vertex == NULL || !vertex->active || !authoring_finite_vec3(position))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    vertex->position = position;
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_set_vertex_uv(henka_authoring_mesh* mesh, henka_authoring_vertex_id id, henka_vec2 uv)
{
    henka_authoring_vertex* vertex = authoring_vertex(mesh, id);
    if (vertex == NULL || !vertex->active || !authoring_finite_vec2(uv))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    vertex->uv = uv;
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_add_face(henka_authoring_mesh* mesh, const henka_authoring_vertex_id* vertices, size_t corner_count, uint32_t material_region, bool smooth, henka_authoring_face_id* out_id)
{
    henka_authoring_face* face;
    henka_authoring_edge_id* new_edges;
    henka_vec2* new_uvs;
    henka_authoring_vertex_id* new_vertices;
    size_t corner;
    size_t edge_start;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;
    if (mesh == NULL || vertices == NULL || out_id == NULL || corner_count < 3U ||
        corner_count > mesh->desc.max_face_corners || mesh->face_slots >= mesh->desc.max_faces ||
        mesh->face_slots >= UINT32_MAX - 1U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (corner = 0U; corner < corner_count; ++corner)
    {
        size_t other;
        if (henka_authoring_mesh_get_vertex(mesh, vertices[corner]) == NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (other = corner + 1U; other < corner_count; ++other)
        {
            if (vertices[corner] == vertices[other])
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    new_vertices = henka_malloc(corner_count * sizeof(*new_vertices));
    new_edges = henka_malloc(corner_count * sizeof(*new_edges));
    new_uvs = henka_malloc(corner_count * sizeof(*new_uvs));
    if (new_vertices == NULL || new_edges == NULL || new_uvs == NULL)
    {
        henka_free(new_vertices);
        henka_free(new_edges);
        henka_free(new_uvs);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (corner = 0U; corner < corner_count; ++corner)
    {
        new_edges[corner] = HENKA_AUTHORING_INVALID_ID;
    }
    memcpy(new_vertices, vertices, corner_count * sizeof(*new_vertices));
    for (corner = 0U; corner < corner_count; ++corner)
    {
        new_uvs[corner] = henka_authoring_mesh_get_vertex(mesh, vertices[corner])->uv;
    }
    face = &mesh->faces[mesh->face_slots];
    memset(face, 0, sizeof(*face));
    face->id = (henka_authoring_face_id)(mesh->face_slots + 1U);
    face->corner_count = corner_count;
    face->vertices = new_vertices;
    face->edges = new_edges;
    face->uvs = new_uvs;
    face->material_region = material_region;
    face->smooth = smooth;
    face->active = true;
    edge_start = mesh->edge_slots;
    for (corner = 0U; corner < corner_count; ++corner)
    {
        henka_authoring_edge* edge = authoring_find_edge(
            mesh, vertices[corner], vertices[(corner + 1U) % corner_count]);
        if (edge != NULL)
        {
            if (edge->face_count >= 2U)
            {
                goto rollback;
            }
            edge->faces[edge->face_count] = face->id;
            ++edge->face_count;
            new_edges[corner] = edge->id;
        }
        else
        {
            result = authoring_append_edge(
                mesh, vertices[corner], vertices[(corner + 1U) % corner_count], face->id, &new_edges[corner]);
            if (result != HENKA_SUCCESS)
            {
                goto rollback;
            }
        }
    }
    ++mesh->face_slots;
    *out_id = face->id;
    return HENKA_SUCCESS;

rollback:
    for (corner = 0U; corner < corner_count; ++corner)
    {
        henka_authoring_edge* edge = authoring_edge(mesh, new_edges[corner]);
        size_t face_index;
        if (edge == NULL || !edge->active)
        {
            continue;
        }
        for (face_index = 0U; face_index < edge->face_count; ++face_index)
        {
            if (edge->faces[face_index] == face->id)
            {
                for (; face_index + 1U < edge->face_count; ++face_index)
                {
                    edge->faces[face_index] = edge->faces[face_index + 1U];
                }
                --edge->face_count;
                if (edge->face_count == 0U)
                {
                    edge->active = false;
                }
                break;
            }
        }
    }
    /* Keep the high-water mark after a failed append. Edge IDs are slot
     * identities and must never be reused, including when an add-face
     * transaction rolls back after creating one or more edges. */
    (void)edge_start;
    henka_free(face->vertices);
    henka_free(face->edges);
    henka_free(face->uvs);
    memset(face, 0, sizeof(*face));
    return result;
}

henka_result henka_authoring_mesh_remove_face(henka_authoring_mesh* mesh, henka_authoring_face_id id)
{
    henka_authoring_face* face = authoring_face(mesh, id);
    size_t corner;
    if (face == NULL || !face->active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (corner = 0U; corner < face->corner_count; ++corner)
    {
        henka_authoring_edge* edge = authoring_edge(mesh, face->edges[corner]);
        size_t face_index;
        if (edge == NULL || !edge->active)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (face_index = 0U; face_index < edge->face_count; ++face_index)
        {
            if (edge->faces[face_index] == id)
            {
                for (; face_index + 1U < edge->face_count; ++face_index)
                {
                    edge->faces[face_index] = edge->faces[face_index + 1U];
                }
                --edge->face_count;
                if (edge->face_count == 0U)
                {
                    edge->active = false;
                }
                break;
            }
        }
    }
    face->active = false;
    henka_free(face->vertices);
    henka_free(face->edges);
    henka_free(face->uvs);
    face->vertices = NULL;
    face->edges = NULL;
    face->uvs = NULL;
    return HENKA_SUCCESS;
}

typedef struct authoring_face_loop_work
{
    bool active;
    size_t corner_count;
    henka_authoring_vertex_id* vertices;
    henka_authoring_edge_id* edges;
    henka_vec2* uvs;
    uint32_t material_region;
    bool smooth;
} authoring_face_loop_work;

typedef struct authoring_edge_relation
{
    henka_authoring_vertex_id low;
    henka_authoring_vertex_id high;
    henka_authoring_face_id face_id;
    size_t corner;
    henka_authoring_edge_id edge_id;
    bool hard;
} authoring_edge_relation;

static int authoring_edge_relation_compare(const void* left_pointer, const void* right_pointer)
{
    const authoring_edge_relation* left = (const authoring_edge_relation*)left_pointer;
    const authoring_edge_relation* right = (const authoring_edge_relation*)right_pointer;
    if (left->low != right->low) return left->low < right->low ? -1 : 1;
    if (left->high != right->high) return left->high < right->high ? -1 : 1;
    if (left->face_id != right->face_id) return left->face_id < right->face_id ? -1 : 1;
    if (left->corner != right->corner) return left->corner < right->corner ? -1 : 1;
    return 0;
}

static const henka_authoring_face_loop_update* authoring_find_face_loop_update(
    const henka_authoring_face_loop_update* updates,
    size_t update_count,
    henka_authoring_face_id face_id)
{
    size_t index;
    for (index = 0U; index < update_count; ++index)
    {
        if (updates[index].face_id == face_id) return &updates[index];
    }
    return NULL;
}

static henka_authoring_edge* authoring_find_edge_by_id(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id)
{
    return authoring_edge(mesh, edge_id);
}

henka_result henka_authoring_mesh_apply_face_loop_updates_internal(
    henka_authoring_mesh* mesh,
    const henka_authoring_face_loop_update* updates,
    size_t update_count)
{
    authoring_face_loop_work* work = NULL;
    authoring_edge_relation* relations = NULL;
    size_t relation_count = 0U;
    size_t relation_capacity = 0U;
    size_t unique_relation_count = 0U;
    size_t new_edge_count = 0U;
    size_t index;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    if (mesh == NULL || (update_count > 0U && updates == NULL) ||
        !henka_authoring_mesh_validate(mesh) || update_count > mesh->face_slots)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < update_count; ++index)
    {
        const henka_authoring_face_loop_update* update = &updates[index];
        size_t corner;
        if (update->face_id == HENKA_AUTHORING_INVALID_ID ||
            henka_authoring_mesh_get_face(mesh, update->face_id) == NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        {
            size_t prior;
            for (prior = 0U; prior < index; ++prior)
            {
                if (updates[prior].face_id == update->face_id)
                {
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
            }
        }
        if (update->remove) continue;
        if (update->vertices == NULL || update->uvs == NULL ||
            update->corner_count < 3U || update->corner_count > mesh->desc.max_face_corners)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (corner = 0U; corner < update->corner_count; ++corner)
        {
            size_t other;
            if (henka_authoring_mesh_get_vertex(mesh, update->vertices[corner]) == NULL ||
                !authoring_finite_vec2(update->uvs[corner]))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            for (other = corner + 1U; other < update->corner_count; ++other)
            {
                if (update->vertices[corner] == update->vertices[other])
                {
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
            }
        }
    }

    if (mesh->face_slots > 0U)
    {
        work = henka_calloc(mesh->face_slots, sizeof(*work));
        if (work == NULL) return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < mesh->face_slots; ++index)
    {
        const henka_authoring_face* source = &mesh->faces[index];
        const henka_authoring_face_loop_update* update;
        authoring_face_loop_work* target = &work[index];
        size_t bytes;
        if (!source->active) continue;
        update = authoring_find_face_loop_update(updates, update_count, source->id);
        if (update != NULL && update->remove) continue;
        target->active = true;
        target->corner_count = update != NULL ? update->corner_count : source->corner_count;
        target->material_region = update != NULL ? update->material_region : source->material_region;
        target->smooth = update != NULL ? update->smooth : source->smooth;
        if (!henka_checked_size_multiply(target->corner_count, sizeof(*target->vertices), &bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        target->vertices = henka_malloc(bytes);
        if (!henka_checked_size_multiply(target->corner_count, sizeof(*target->edges), &bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        target->edges = henka_malloc(bytes);
        if (!henka_checked_size_multiply(target->corner_count, sizeof(*target->uvs), &bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        target->uvs = henka_malloc(bytes);
        if (target->vertices == NULL || target->edges == NULL || target->uvs == NULL)
        {
            result = HENKA_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }
        if (update != NULL)
        {
            memcpy(target->vertices, update->vertices, target->corner_count * sizeof(*target->vertices));
            memcpy(target->uvs, update->uvs, target->corner_count * sizeof(*target->uvs));
        }
        else
        {
            memcpy(target->vertices, source->vertices, target->corner_count * sizeof(*target->vertices));
            memcpy(target->uvs, source->uvs, target->corner_count * sizeof(*target->uvs));
        }
        if (relation_capacity > SIZE_MAX - target->corner_count)
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        relation_capacity += target->corner_count;
    }
    if (relation_capacity > 0U)
    {
        size_t bytes;
        if (!henka_checked_size_multiply(relation_capacity, sizeof(*relations), &bytes))
        {
            result = HENKA_ERROR_LIMIT;
            goto cleanup;
        }
        relations = henka_calloc(relation_capacity, sizeof(*relations));
        if (relations == NULL)
        {
            result = HENKA_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }
    }
    for (index = 0U; index < mesh->face_slots; ++index)
    {
        const authoring_face_loop_work* face = &work[index];
        size_t corner;
        if (!face->active) continue;
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const henka_authoring_vertex_id first = face->vertices[corner];
            const henka_authoring_vertex_id second = face->vertices[(corner + 1U) % face->corner_count];
            authoring_edge_relation* relation = &relations[relation_count++];
            if (first == second)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            relation->low = first < second ? first : second;
            relation->high = first < second ? second : first;
            relation->face_id = (henka_authoring_face_id)(index + 1U);
            relation->corner = corner;
        }
    }
    if (relation_count > 1U)
    {
        qsort(relations, relation_count, sizeof(*relations), authoring_edge_relation_compare);
    }
    index = 0U;
    while (index < relation_count)
    {
        size_t end = index + 1U;
        henka_authoring_edge* existing;
        while (end < relation_count && relations[end].low == relations[index].low &&
            relations[end].high == relations[index].high)
        {
            ++end;
        }
        if (end - index > 2U)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        existing = authoring_find_edge(mesh, relations[index].low, relations[index].high);
        if (existing != NULL)
        {
            relations[index].edge_id = existing->id;
            relations[index].hard = existing->hard;
        }
        else
        {
            if (mesh->edge_slots > mesh->desc.max_edges - new_edge_count)
            {
                result = HENKA_ERROR_LIMIT;
                goto cleanup;
            }
            relations[index].edge_id = (henka_authoring_edge_id)(mesh->edge_slots + new_edge_count + 1U);
            relations[index].hard = false;
            ++new_edge_count;
        }
        {
            size_t owner;
            for (owner = index + 1U; owner < end; ++owner)
            {
                relations[owner].edge_id = relations[index].edge_id;
                relations[owner].hard = relations[index].hard;
            }
        }
        ++unique_relation_count;
        index = end;
    }
    (void)unique_relation_count;
    for (index = 0U; index < relation_count; ++index)
    {
        authoring_face_loop_work* face = &work[(size_t)relations[index].face_id - 1U];
        face->edges[relations[index].corner] = relations[index].edge_id;
    }

    for (index = 0U; index < mesh->edge_slots; ++index)
    {
        if (mesh->edges[index].active)
        {
            mesh->edges[index].active = false;
            mesh->edges[index].face_count = 0U;
            mesh->edges[index].faces[0] = HENKA_AUTHORING_INVALID_ID;
            mesh->edges[index].faces[1] = HENKA_AUTHORING_INVALID_ID;
        }
    }
    for (index = 0U; index < relation_count; )
    {
        const size_t start = index;
        henka_authoring_edge* edge;
        size_t end = index + 1U;
        while (end < relation_count && relations[end].low == relations[start].low &&
            relations[end].high == relations[start].high) ++end;
        edge = authoring_find_edge_by_id(mesh, relations[start].edge_id);
        if (edge == NULL)
        {
            edge = &mesh->edges[mesh->edge_slots++];
            memset(edge, 0, sizeof(*edge));
            edge->id = relations[start].edge_id;
        }
        edge->vertices[0] = relations[start].low;
        edge->vertices[1] = relations[start].high;
        edge->face_count = end - start;
        edge->faces[0] = relations[start].face_id;
        edge->faces[1] = end - start > 1U ? relations[start + 1U].face_id : HENKA_AUTHORING_INVALID_ID;
        edge->hard = relations[start].hard;
        edge->active = true;
        index = end;
    }
    for (index = 0U; index < mesh->face_slots; ++index)
    {
        henka_authoring_face* target = &mesh->faces[index];
        authoring_face_loop_work* replacement = &work[index];
        if (target->active)
        {
            henka_free(target->vertices);
            henka_free(target->edges);
            henka_free(target->uvs);
        }
        if (!replacement->active)
        {
            target->active = false;
            target->corner_count = 0U;
            target->vertices = NULL;
            target->edges = NULL;
            target->uvs = NULL;
            continue;
        }
        target->corner_count = replacement->corner_count;
        target->vertices = replacement->vertices;
        target->edges = replacement->edges;
        target->uvs = replacement->uvs;
        target->material_region = replacement->material_region;
        target->smooth = replacement->smooth;
        target->active = true;
        replacement->vertices = NULL;
        replacement->edges = NULL;
        replacement->uvs = NULL;
    }
    mesh->edge_lookup_ready = false;
    result = henka_authoring_mesh_validate(mesh) ? HENKA_SUCCESS : HENKA_ERROR_UNKNOWN;

cleanup:
    if (work != NULL)
    {
        for (index = 0U; index < mesh->face_slots; ++index)
        {
            henka_free(work[index].vertices);
            henka_free(work[index].edges);
            henka_free(work[index].uvs);
        }
    }
    henka_free(work);
    henka_free(relations);
    return result;
}

const henka_authoring_face* henka_authoring_mesh_get_face(const henka_authoring_mesh* mesh, henka_authoring_face_id id)
{
    const henka_authoring_face* face = authoring_face_const(mesh, id);
    return face != NULL && face->active ? face : NULL;
}

const henka_authoring_edge* henka_authoring_mesh_get_edge(const henka_authoring_mesh* mesh, henka_authoring_edge_id id)
{
    const henka_authoring_edge* edge = authoring_edge_const(mesh, id);
    return edge != NULL && edge->active ? edge : NULL;
}

henka_result henka_authoring_mesh_set_face_material_region(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id id,
    uint32_t material_region)
{
    henka_authoring_face* face = authoring_face(mesh, id);
    if (face == NULL || !face->active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    face->material_region = material_region;
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_set_face_smoothing(henka_authoring_mesh* mesh, henka_authoring_face_id id, bool smooth)
{
    henka_authoring_face* face = authoring_face(mesh, id);
    if (face == NULL || !face->active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    face->smooth = smooth;
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_set_face_corner_uv(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id id,
    size_t corner,
    henka_vec2 uv)
{
    henka_authoring_face* face = authoring_face(mesh, id);
    if (face == NULL || !face->active || face->uvs == NULL || corner >= face->corner_count ||
        !authoring_finite_vec2(uv))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    face->uvs[corner] = uv;
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_get_face_corner_uv(
    const henka_authoring_mesh* mesh,
    henka_authoring_face_id id,
    size_t corner,
    henka_vec2* out_uv)
{
    const henka_authoring_face* face = authoring_face_const(mesh, id);
    if (face == NULL || !face->active || face->uvs == NULL || out_uv == NULL || corner >= face->corner_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_uv = face->uvs[corner];
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_set_edge_hard(henka_authoring_mesh* mesh, henka_authoring_edge_id id, bool hard)
{
    henka_authoring_edge* edge = authoring_edge(mesh, id);
    if (edge == NULL || !edge->active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    edge->hard = hard;
    return HENKA_SUCCESS;
}

size_t henka_authoring_mesh_get_vertex_edge_count(const henka_authoring_mesh* mesh, henka_authoring_vertex_id vertex_id)
{
    size_t count = 0U;
    size_t index;
    if (henka_authoring_mesh_get_vertex(mesh, vertex_id) == NULL)
    {
        return 0U;
    }
    for (index = 0U; index < mesh->edge_slots; ++index)
    {
        if (mesh->edges[index].active && (mesh->edges[index].vertices[0] == vertex_id ||
            mesh->edges[index].vertices[1] == vertex_id))
        {
            ++count;
        }
    }
    return count;
}

henka_result henka_authoring_mesh_get_vertex_edge_at(const henka_authoring_mesh* mesh, henka_authoring_vertex_id vertex_id, size_t ordinal, henka_authoring_edge_id* out_edge_id)
{
    size_t index;
    if (out_edge_id == NULL || henka_authoring_mesh_get_vertex(mesh, vertex_id) == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < mesh->edge_slots; ++index)
    {
        const henka_authoring_edge* edge = &mesh->edges[index];
        if (edge->active && (edge->vertices[0] == vertex_id || edge->vertices[1] == vertex_id))
        {
            if (ordinal == 0U)
            {
                *out_edge_id = edge->id;
                return HENKA_SUCCESS;
            }
            --ordinal;
        }
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

size_t henka_authoring_mesh_get_edge_face_count(const henka_authoring_mesh* mesh, henka_authoring_edge_id edge_id)
{
    const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(mesh, edge_id);
    return edge == NULL ? 0U : edge->face_count;
}

henka_result henka_authoring_mesh_get_edge_face_at(const henka_authoring_mesh* mesh, henka_authoring_edge_id edge_id, size_t ordinal, henka_authoring_face_id* out_face_id)
{
    const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(mesh, edge_id);
    if (edge == NULL || out_face_id == NULL || ordinal >= edge->face_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_face_id = edge->faces[ordinal];
    return HENKA_SUCCESS;
}

bool henka_authoring_mesh_edge_is_boundary(const henka_authoring_mesh* mesh, henka_authoring_edge_id edge_id)
{
    const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(mesh, edge_id);
    return edge != NULL && edge->face_count == 1U;
}

static henka_vec3 authoring_face_normal(const henka_authoring_mesh* mesh, const henka_authoring_face* face)
{
    const henka_vec3 a = authoring_vertex_const(mesh, face->vertices[0])->position;
    const henka_vec3 b = authoring_vertex_const(mesh, face->vertices[1])->position;
    const henka_vec3 c = authoring_vertex_const(mesh, face->vertices[2])->position;
    return henka_vec3_normalize(henka_vec3_cross(henka_vec3_subtract(b, a), henka_vec3_subtract(c, a)));
}

static bool authoring_face_has_hard_edge_at_vertex(
    const henka_authoring_mesh* mesh,
    const henka_authoring_face* face,
    henka_authoring_vertex_id vertex_id)
{
    size_t corner;
    if (mesh == NULL || face == NULL || !face->active || !face->smooth)
    {
        return false;
    }
    for (corner = 0U; corner < face->corner_count; ++corner)
    {
        const henka_authoring_edge* edge = authoring_edge_const(mesh, face->edges[corner]);
        if (edge != NULL && edge->hard &&
            (edge->vertices[0] == vertex_id || edge->vertices[1] == vertex_id))
        {
            return true;
        }
    }
    return false;
}

henka_result henka_authoring_mesh_evaluate(const henka_authoring_mesh* mesh, henka_authoring_render_data* out_data)
{
    henka_vec3* face_normals = NULL;
    henka_vec3* smooth_vertex_normals = NULL;
    unsigned char* smooth_vertex_normal_valid = NULL;
    size_t required_vertices = 0U;
    size_t required_indices = 0U;
    size_t face_index;
    size_t output_vertex = 0U;
    size_t output_index = 0U;
    henka_result result = HENKA_SUCCESS;
    if (mesh == NULL || out_data == NULL || !henka_authoring_mesh_validate(mesh) ||
        out_data->vertices == NULL || out_data->indices == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (face_index = 0U; face_index < mesh->face_slots; ++face_index)
    {
        const henka_authoring_face* face = &mesh->faces[face_index];
        if (face->active)
        {
            required_vertices += face->corner_count;
            required_indices += (face->corner_count - 2U) * 3U;
        }
    }
    if (required_vertices > out_data->vertex_capacity || required_indices > out_data->index_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }

    if (mesh->face_slots > 0U)
    {
        face_normals = henka_calloc(mesh->face_slots, sizeof(*face_normals));
        if (face_normals == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }
    if (mesh->vertex_slots > 0U)
    {
        smooth_vertex_normals = henka_calloc(mesh->vertex_slots, sizeof(*smooth_vertex_normals));
        smooth_vertex_normal_valid = henka_calloc(mesh->vertex_slots, sizeof(*smooth_vertex_normal_valid));
        if (smooth_vertex_normals == NULL || smooth_vertex_normal_valid == NULL)
        {
            result = HENKA_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }
    }

    for (face_index = 0U; face_index < mesh->face_slots; ++face_index)
    {
        const henka_authoring_face* face = &mesh->faces[face_index];
        size_t corner;
        if (!face->active)
        {
            continue;
        }
        face_normals[face_index] = authoring_face_normal(mesh, face);
        if (henka_vec3_length(face_normals[face_index]) <= 0.00001f)
        {
            result = HENKA_ERROR_NUMERIC_RANGE;
            goto cleanup;
        }
        if (!face->smooth)
        {
            continue;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const henka_authoring_vertex_id vertex_id = face->vertices[corner];
            const size_t vertex_index = (size_t)vertex_id - 1U;
            if (vertex_index >= mesh->vertex_slots)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            if (!authoring_face_has_hard_edge_at_vertex(mesh, face, vertex_id))
            {
                smooth_vertex_normals[vertex_index] = henka_vec3_add(
                    smooth_vertex_normals[vertex_index],
                    face_normals[face_index]);
                smooth_vertex_normal_valid[vertex_index] = 1U;
            }
        }
    }
    for (face_index = 0U; face_index < mesh->vertex_slots; ++face_index)
    {
        if (smooth_vertex_normal_valid[face_index] != 0U &&
            henka_vec3_length(smooth_vertex_normals[face_index]) > 0.00001f)
        {
            smooth_vertex_normals[face_index] = henka_vec3_normalize(smooth_vertex_normals[face_index]);
        }
        else
        {
            smooth_vertex_normal_valid[face_index] = 0U;
        }
    }

    for (face_index = 0U; face_index < mesh->face_slots; ++face_index)
    {
        const henka_authoring_face* face = &mesh->faces[face_index];
        const henka_vec3 face_normal = face_normals[face_index];
        size_t corner;
        if (!face->active)
        {
            continue;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const henka_authoring_vertex* source = authoring_vertex_const(mesh, face->vertices[corner]);
            const size_t vertex_index = (size_t)source->id - 1U;
            out_data->vertices[output_vertex].position = source->position;
            out_data->vertices[output_vertex].uv = face->uvs[corner];
            out_data->vertices[output_vertex].material_region = face->material_region;
            out_data->vertices[output_vertex].normal = face->smooth &&
                    vertex_index < mesh->vertex_slots &&
                    smooth_vertex_normal_valid[vertex_index] != 0U
                ? smooth_vertex_normals[vertex_index]
                : face_normal;
            out_data->vertices[output_vertex].tangent = (henka_vec4){1.0f, 0.0f, 0.0f, 1.0f};
            ++output_vertex;
        }
        for (corner = 1U; corner + 1U < face->corner_count; ++corner)
        {
            out_data->indices[output_index++] = (uint32_t)(output_vertex - face->corner_count);
            out_data->indices[output_index++] = (uint32_t)(output_vertex - face->corner_count + corner);
            out_data->indices[output_index++] = (uint32_t)(output_vertex - face->corner_count + corner + 1U);
        }
    }
    out_data->vertex_count = required_vertices;
    out_data->index_count = required_indices;
cleanup:
    henka_free(smooth_vertex_normal_valid);
    henka_free(smooth_vertex_normals);
    henka_free(face_normals);
    return result;
}

static bool authoring_desc_equal(const henka_authoring_mesh_desc* left, const henka_authoring_mesh_desc* right)
{
    return left->max_vertices == right->max_vertices && left->max_edges == right->max_edges &&
        left->max_faces == right->max_faces && left->max_face_corners == right->max_face_corners;
}

static henka_result authoring_mesh_clone_internal(const henka_authoring_mesh* source, henka_authoring_mesh** out_mesh)
{
    henka_authoring_mesh* clone;
    size_t index;
    henka_result result;
    if (source == NULL || out_mesh == NULL || !henka_authoring_mesh_validate(source))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_authoring_mesh_create(&source->desc, &clone);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    clone->vertex_slots = source->vertex_slots;
    clone->edge_slots = source->edge_slots;
    clone->face_slots = source->face_slots;
    memcpy(clone->vertices, source->vertices, source->vertex_slots * sizeof(*source->vertices));
    memcpy(clone->edges, source->edges, source->edge_slots * sizeof(*source->edges));
    clone->edge_lookup_ready = false;
    for (index = 0U; index < source->face_slots; ++index)
    {
        const henka_authoring_face* source_face = &source->faces[index];
        henka_authoring_face* clone_face = &clone->faces[index];
        *clone_face = *source_face;
        clone_face->vertices = NULL;
        clone_face->edges = NULL;
        clone_face->uvs = NULL;
        if (source_face->active)
        {
            clone_face->vertices = henka_malloc(source_face->corner_count * sizeof(*clone_face->vertices));
            clone_face->edges = henka_malloc(source_face->corner_count * sizeof(*clone_face->edges));
            clone_face->uvs = henka_malloc(source_face->corner_count * sizeof(*clone_face->uvs));
            if (clone_face->vertices == NULL || clone_face->edges == NULL || clone_face->uvs == NULL)
            {
                henka_authoring_mesh_destroy(clone);
                return HENKA_ERROR_OUT_OF_MEMORY;
            }
            memcpy(clone_face->vertices, source_face->vertices,
                source_face->corner_count * sizeof(*clone_face->vertices));
            memcpy(clone_face->edges, source_face->edges,
                source_face->corner_count * sizeof(*clone_face->edges));
            memcpy(clone_face->uvs, source_face->uvs,
                source_face->corner_count * sizeof(*clone_face->uvs));
        }
    }
    *out_mesh = clone;
    return HENKA_SUCCESS;
}

henka_result henka_authoring_mesh_clone(const henka_authoring_mesh* source, henka_authoring_mesh** out_mesh)
{
    if (out_mesh != NULL)
    {
        *out_mesh = NULL;
    }
    return authoring_mesh_clone_internal(source, out_mesh);
}

static void authoring_mesh_swap(henka_authoring_mesh* left, henka_authoring_mesh* right)
{
    henka_authoring_mesh temporary = *left;
    *left = *right;
    *right = temporary;
}

henka_result henka_authoring_mesh_copy(henka_authoring_mesh* destination, const henka_authoring_mesh* source)
{
    henka_authoring_mesh* replacement = NULL;
    henka_result result;
    if (destination == NULL || source == NULL || !authoring_desc_equal(&destination->desc, &source->desc))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = authoring_mesh_clone_internal(source, &replacement);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    authoring_mesh_swap(destination, replacement);
    henka_authoring_mesh_destroy(replacement);
    return HENKA_SUCCESS;
}

typedef struct authoring_quad_boundary
{
    henka_authoring_vertex_id previous;
    henka_authoring_vertex_id opposite;
    henka_authoring_vertex_id next;
    henka_vec2 previous_uv;
    henka_vec2 opposite_uv;
    henka_vec2 next_uv;
} authoring_quad_boundary;

typedef struct authoring_quad_candidate
{
    henka_authoring_edge_id edge_id;
    henka_authoring_face_id first_face;
    henka_authoring_face_id second_face;
    float score;
} authoring_quad_candidate;

static bool authoring_quad_uv_near(
    henka_vec2 left,
    henka_vec2 right,
    float epsilon)
{
    return fabsf(left.x - right.x) <= epsilon &&
        fabsf(left.y - right.y) <= epsilon;
}

static float authoring_quad_vec3_dot(
    henka_vec3 left,
    henka_vec3 right)
{
    return left.x * right.x +
        left.y * right.y +
        left.z * right.z;
}

static bool authoring_quad_triangle_boundary(
    const henka_authoring_face* face,
    henka_authoring_vertex_id shared_first,
    henka_authoring_vertex_id shared_second,
    authoring_quad_boundary* out_boundary)
{
    size_t opposite_index;
    size_t previous_index;
    size_t next_index;
    henka_authoring_vertex_id opposite;

    if (face == NULL ||
        out_boundary == NULL ||
        face->corner_count != 3U)
    {
        return false;
    }

    for (opposite_index = 0U;
         opposite_index < 3U;
         ++opposite_index)
    {
        opposite = face->vertices[opposite_index];

        if (opposite != shared_first &&
            opposite != shared_second)
        {
            break;
        }
    }

    if (opposite_index >= 3U)
    {
        return false;
    }

    previous_index = (opposite_index + 2U) % 3U;
    next_index = (opposite_index + 1U) % 3U;

    if (!(
            (face->vertices[previous_index] == shared_first &&
             face->vertices[next_index] == shared_second) ||
            (face->vertices[previous_index] == shared_second &&
             face->vertices[next_index] == shared_first)))
    {
        return false;
    }

    out_boundary->previous = face->vertices[previous_index];
    out_boundary->opposite = face->vertices[opposite_index];
    out_boundary->next = face->vertices[next_index];

    out_boundary->previous_uv = face->uvs[previous_index];
    out_boundary->opposite_uv = face->uvs[opposite_index];
    out_boundary->next_uv = face->uvs[next_index];

    return true;
}

static bool authoring_quad_candidate_from_edge(
    const henka_authoring_mesh* mesh,
    const henka_authoring_edge* edge,
    float minimum_normal_dot,
    float minimum_diagonal_ratio,
    float uv_epsilon,
    authoring_quad_candidate* out_candidate)
{
    const henka_authoring_face* first;
    const henka_authoring_face* second;
    authoring_quad_boundary first_boundary;
    authoring_quad_boundary second_boundary;
    const henka_authoring_vertex* previous_vertex;
    const henka_authoring_vertex* first_opposite_vertex;
    const henka_authoring_vertex* next_vertex;
    const henka_authoring_vertex* second_opposite_vertex;
    henka_vec3 first_normal;
    henka_vec3 second_normal;
    float normal_dot;
    float shared_length;
    float perimeter_total;
    float average_perimeter;
    float diagonal_ratio;
    float first_area;
    float second_area;
    float area_balance;

    if (mesh == NULL ||
        edge == NULL ||
        out_candidate == NULL ||
        !edge->active ||
        edge->hard ||
        edge->face_count != 2U)
    {
        return false;
    }

    first = henka_authoring_mesh_get_face(
        mesh,
        edge->faces[0]);
    second = henka_authoring_mesh_get_face(
        mesh,
        edge->faces[1]);

    if (first == NULL ||
        second == NULL ||
        first->corner_count != 3U ||
        second->corner_count != 3U ||
        first->material_region != second->material_region ||
        first->smooth != second->smooth)
    {
        return false;
    }

    if (!authoring_quad_triangle_boundary(
            first,
            edge->vertices[0],
            edge->vertices[1],
            &first_boundary) ||
        !authoring_quad_triangle_boundary(
            second,
            edge->vertices[0],
            edge->vertices[1],
            &second_boundary))
    {
        return false;
    }

    if (first_boundary.previous != second_boundary.next ||
        first_boundary.next != second_boundary.previous)
    {
        return false;
    }

    if (!authoring_quad_uv_near(
            first_boundary.previous_uv,
            second_boundary.next_uv,
            uv_epsilon) ||
        !authoring_quad_uv_near(
            first_boundary.next_uv,
            second_boundary.previous_uv,
            uv_epsilon))
    {
        return false;
    }

    first_normal = authoring_face_normal(mesh, first);
    second_normal = authoring_face_normal(mesh, second);

    normal_dot = authoring_quad_vec3_dot(
        first_normal,
        second_normal);

    if (!isfinite(normal_dot) ||
        normal_dot < minimum_normal_dot)
    {
        return false;
    }

    previous_vertex = henka_authoring_mesh_get_vertex(
        mesh,
        first_boundary.previous);
    first_opposite_vertex = henka_authoring_mesh_get_vertex(
        mesh,
        first_boundary.opposite);
    next_vertex = henka_authoring_mesh_get_vertex(
        mesh,
        first_boundary.next);
    second_opposite_vertex = henka_authoring_mesh_get_vertex(
        mesh,
        second_boundary.opposite);

    if (previous_vertex == NULL ||
        first_opposite_vertex == NULL ||
        next_vertex == NULL ||
        second_opposite_vertex == NULL)
    {
        return false;
    }

    shared_length = henka_vec3_length(
        henka_vec3_subtract(
            previous_vertex->position,
            next_vertex->position));

    perimeter_total =
        henka_vec3_length(
            henka_vec3_subtract(
                previous_vertex->position,
                first_opposite_vertex->position)) +
        henka_vec3_length(
            henka_vec3_subtract(
                first_opposite_vertex->position,
                next_vertex->position)) +
        henka_vec3_length(
            henka_vec3_subtract(
                next_vertex->position,
                second_opposite_vertex->position)) +
        henka_vec3_length(
            henka_vec3_subtract(
                second_opposite_vertex->position,
                previous_vertex->position));

    average_perimeter = perimeter_total * 0.25f;

    if (!isfinite(shared_length) ||
        !isfinite(average_perimeter) ||
        average_perimeter <= 0.000001f)
    {
        return false;
    }

    diagonal_ratio = shared_length / average_perimeter;

    if (!isfinite(diagonal_ratio) ||
        diagonal_ratio < minimum_diagonal_ratio)
    {
        return false;
    }

    first_area =
        0.5f *
        henka_vec3_length(
            henka_vec3_cross(
                henka_vec3_subtract(
                    first_opposite_vertex->position,
                    previous_vertex->position),
                henka_vec3_subtract(
                    next_vertex->position,
                    previous_vertex->position)));

    second_area =
        0.5f *
        henka_vec3_length(
            henka_vec3_cross(
                henka_vec3_subtract(
                    second_opposite_vertex->position,
                    next_vertex->position),
                henka_vec3_subtract(
                    previous_vertex->position,
                    next_vertex->position)));

    if (first_area <= 0.0000001f ||
        second_area <= 0.0000001f)
    {
        return false;
    }

    area_balance =
        first_area < second_area
            ? first_area / second_area
            : second_area / first_area;

    out_candidate->edge_id = edge->id;
    out_candidate->first_face = first->id;
    out_candidate->second_face = second->id;
    out_candidate->score =
        normal_dot * 4.0f +
        diagonal_ratio * 2.0f +
        area_balance;

    return true;
}

static int authoring_quad_candidate_compare(
    const void* left_pointer,
    const void* right_pointer)
{
    const authoring_quad_candidate* left =
        (const authoring_quad_candidate*)left_pointer;
    const authoring_quad_candidate* right =
        (const authoring_quad_candidate*)right_pointer;

    if (left->score > right->score)
    {
        return -1;
    }
    if (left->score < right->score)
    {
        return 1;
    }
    if (left->edge_id < right->edge_id)
    {
        return -1;
    }
    if (left->edge_id > right->edge_id)
    {
        return 1;
    }
    return 0;
}

henka_result henka_authoring_mesh_recover_quads(
    henka_authoring_mesh* mesh,
    float minimum_normal_dot,
    float minimum_diagonal_ratio,
    float uv_epsilon,
    size_t* out_merged_pairs)
{
    authoring_quad_candidate* candidates = NULL;
    henka_authoring_face_id* merge_with = NULL;
    bool* merge_owner = NULL;
    henka_authoring_vertex_id* vertex_map = NULL;
    henka_authoring_mesh* replacement = NULL;
    size_t candidate_count = 0U;
    size_t merged_pairs = 0U;
    size_t edge_index;
    size_t candidate_index;
    uint32_t vertex_id;
    uint32_t face_id;
    henka_result result = HENKA_SUCCESS;

    if (out_merged_pairs != NULL)
    {
        *out_merged_pairs = 0U;
    }

    if (mesh == NULL ||
        out_merged_pairs == NULL ||
        !henka_authoring_mesh_validate(mesh) ||
        !isfinite(minimum_normal_dot) ||
        !isfinite(minimum_diagonal_ratio) ||
        !isfinite(uv_epsilon) ||
        minimum_normal_dot < 0.0f ||
        minimum_normal_dot > 1.0f ||
        minimum_diagonal_ratio <= 0.0f ||
        uv_epsilon < 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    candidates = henka_calloc(
        mesh->edge_slots > 0U ? mesh->edge_slots : 1U,
        sizeof(*candidates));

    merge_with = henka_calloc(
        mesh->face_slots + 1U,
        sizeof(*merge_with));

    merge_owner = henka_calloc(
        mesh->face_slots + 1U,
        sizeof(*merge_owner));

    if (candidates == NULL ||
        merge_with == NULL ||
        merge_owner == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }

    for (edge_index = 0U;
         edge_index < mesh->edge_slots;
         ++edge_index)
    {
        if (authoring_quad_candidate_from_edge(
                mesh,
                &mesh->edges[edge_index],
                minimum_normal_dot,
                minimum_diagonal_ratio,
                uv_epsilon,
                &candidates[candidate_count]))
        {
            ++candidate_count;
        }
    }

    if (candidate_count == 0U)
    {
        goto cleanup;
    }

    qsort(
        candidates,
        candidate_count,
        sizeof(*candidates),
        authoring_quad_candidate_compare);

    for (candidate_index = 0U;
         candidate_index < candidate_count;
         ++candidate_index)
    {
        const authoring_quad_candidate* candidate =
            &candidates[candidate_index];

        if (candidate->first_face > mesh->face_slots ||
            candidate->second_face > mesh->face_slots ||
            merge_with[candidate->first_face] != 0U ||
            merge_with[candidate->second_face] != 0U)
        {
            continue;
        }

        merge_with[candidate->first_face] =
            candidate->second_face;
        merge_with[candidate->second_face] =
            candidate->first_face;

        merge_owner[candidate->first_face] = true;
        ++merged_pairs;
    }

    if (merged_pairs == 0U)
    {
        goto cleanup;
    }

    result = henka_authoring_mesh_create(
        &mesh->desc,
        &replacement);

    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    vertex_map = henka_calloc(
        mesh->vertex_slots + 1U,
        sizeof(*vertex_map));

    if (vertex_map == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }

    for (vertex_id = 1U;
         vertex_id <= mesh->vertex_slots;
         ++vertex_id)
    {
        const henka_authoring_vertex* source_vertex =
            henka_authoring_mesh_get_vertex(
                mesh,
                (henka_authoring_vertex_id)vertex_id);

        if (source_vertex == NULL)
        {
            continue;
        }

        result = henka_authoring_mesh_add_vertex(
            replacement,
            source_vertex->position,
            source_vertex->uv,
            source_vertex->material_region,
            &vertex_map[vertex_id]);

        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }

    for (face_id = 1U;
         face_id <= mesh->face_slots;
         ++face_id)
    {
        const henka_authoring_face* source_face =
            henka_authoring_mesh_get_face(
                mesh,
                (henka_authoring_face_id)face_id);
        henka_authoring_face_id new_face_id;

        if (source_face == NULL)
        {
            continue;
        }

        if (merge_with[face_id] != 0U)
        {
            if (!merge_owner[face_id])
            {
                continue;
            }

            {
                const henka_authoring_face* second_face =
                    henka_authoring_mesh_get_face(
                        mesh,
                        merge_with[face_id]);
                const henka_authoring_edge* shared_edge = NULL;
                authoring_quad_boundary first_boundary;
                authoring_quad_boundary second_boundary;
                henka_authoring_vertex_id quad[4];
                henka_vec2 quad_uvs[4];
                size_t corner;

                for (corner = 0U;
                     corner < source_face->corner_count;
                     ++corner)
                {
                    const henka_authoring_edge* edge =
                        henka_authoring_mesh_get_edge(
                            mesh,
                            source_face->edges[corner]);

                    if (edge != NULL &&
                        edge->face_count == 2U &&
                        ((edge->faces[0] == face_id &&
                          edge->faces[1] == merge_with[face_id]) ||
                         (edge->faces[1] == face_id &&
                          edge->faces[0] == merge_with[face_id])))
                    {
                        shared_edge = edge;
                        break;
                    }
                }

                if (second_face == NULL ||
                    shared_edge == NULL ||
                    !authoring_quad_triangle_boundary(
                        source_face,
                        shared_edge->vertices[0],
                        shared_edge->vertices[1],
                        &first_boundary) ||
                    !authoring_quad_triangle_boundary(
                        second_face,
                        shared_edge->vertices[0],
                        shared_edge->vertices[1],
                        &second_boundary))
                {
                    result = HENKA_ERROR_UNKNOWN;
                    goto cleanup;
                }

                quad[0] = vertex_map[first_boundary.previous];
                quad[1] = vertex_map[first_boundary.opposite];
                quad[2] = vertex_map[first_boundary.next];
                quad[3] = vertex_map[second_boundary.opposite];

                quad_uvs[0] = first_boundary.previous_uv;
                quad_uvs[1] = first_boundary.opposite_uv;
                quad_uvs[2] = first_boundary.next_uv;
                quad_uvs[3] = second_boundary.opposite_uv;

                result = henka_authoring_mesh_add_face(
                    replacement,
                    quad,
                    4U,
                    source_face->material_region,
                    source_face->smooth,
                    &new_face_id);

                for (corner = 0U;
                     result == HENKA_SUCCESS &&
                     corner < 4U;
                     ++corner)
                {
                    result =
                        henka_authoring_mesh_set_face_corner_uv(
                            replacement,
                            new_face_id,
                            corner,
                            quad_uvs[corner]);
                }

                if (result != HENKA_SUCCESS)
                {
                    goto cleanup;
                }
            }

            continue;
        }

        {
            henka_authoring_vertex_id
                mapped_vertices[
                    HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS];
            size_t corner;

            if (source_face->corner_count >
                HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS)
            {
                result = HENKA_ERROR_LIMIT;
                goto cleanup;
            }

            for (corner = 0U;
                 corner < source_face->corner_count;
                 ++corner)
            {
                mapped_vertices[corner] =
                    vertex_map[source_face->vertices[corner]];
            }

            result = henka_authoring_mesh_add_face(
                replacement,
                mapped_vertices,
                source_face->corner_count,
                source_face->material_region,
                source_face->smooth,
                &new_face_id);

            for (corner = 0U;
                 result == HENKA_SUCCESS &&
                 corner < source_face->corner_count;
                 ++corner)
            {
                result =
                    henka_authoring_mesh_set_face_corner_uv(
                        replacement,
                        new_face_id,
                        corner,
                        source_face->uvs[corner]);
            }

            if (result != HENKA_SUCCESS)
            {
                goto cleanup;
            }
        }
    }

    for (edge_index = 0U;
         edge_index < mesh->edge_slots;
         ++edge_index)
    {
        const henka_authoring_edge* source_edge =
            &mesh->edges[edge_index];

        if (source_edge->active &&
            source_edge->hard)
        {
            henka_authoring_edge* replacement_edge =
                authoring_find_edge(
                    replacement,
                    vertex_map[source_edge->vertices[0]],
                    vertex_map[source_edge->vertices[1]]);

            if (replacement_edge == NULL)
            {
                result = HENKA_ERROR_UNKNOWN;
                goto cleanup;
            }

            replacement_edge->hard = true;
        }
    }

    if (!henka_authoring_mesh_validate(replacement))
    {
        result = HENKA_ERROR_UNKNOWN;
        goto cleanup;
    }

    authoring_mesh_swap(mesh, replacement);
    *out_merged_pairs = merged_pairs;

cleanup:
    henka_authoring_mesh_destroy(replacement);
    henka_free(vertex_map);
    henka_free(merge_owner);
    henka_free(merge_with);
    henka_free(candidates);

    return result;
}
#define HENKA_AUTHORING_MESH_MAX_HISTORY_STEPS 64U

struct henka_authoring_mesh_history
{
    size_t max_steps;
    size_t undo_count;
    size_t redo_count;
    henka_authoring_mesh** undo;
    henka_authoring_mesh** redo;
};

static void authoring_history_discard_oldest(henka_authoring_mesh** snapshots, size_t* count)
{
    if (*count == 0U)
    {
        return;
    }
    henka_authoring_mesh_destroy(snapshots[0]);
    if (*count > 1U)
    {
        memmove(&snapshots[0], &snapshots[1], (*count - 1U) * sizeof(*snapshots));
    }
    --*count;
}

static henka_result authoring_history_append(
    henka_authoring_mesh** snapshots,
    size_t* count,
    size_t max_steps,
    henka_authoring_mesh* snapshot)
{
    if (*count >= max_steps)
    {
        authoring_history_discard_oldest(snapshots, count);
    }
    snapshots[(*count)++] = snapshot;
    return HENKA_SUCCESS;
}

static bool authoring_history_mesh_matches(
    const henka_authoring_mesh_history* history,
    const henka_authoring_mesh* mesh)
{
    return history != NULL && mesh != NULL && history->undo_count > 0U &&
        authoring_desc_equal(&history->undo[0]->desc, &mesh->desc);
}

henka_result henka_authoring_mesh_history_create(const henka_authoring_mesh* initial_mesh, size_t max_steps, henka_authoring_mesh_history** out_history)
{
    henka_authoring_mesh_history* history;
    henka_authoring_mesh* initial = NULL;
    henka_result result;
    if (out_history == NULL || initial_mesh == NULL || max_steps == 0U ||
        max_steps > HENKA_AUTHORING_MESH_MAX_HISTORY_STEPS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_history = NULL;
    result = authoring_mesh_clone_internal(initial_mesh, &initial);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    history = henka_calloc(1U, sizeof(*history));
    if (history != NULL)
    {
        history->undo = henka_calloc(max_steps, sizeof(*history->undo));
        history->redo = henka_calloc(max_steps, sizeof(*history->redo));
    }
    if (history == NULL || history->undo == NULL || history->redo == NULL)
    {
        henka_authoring_mesh_destroy(initial);
        if (history != NULL)
        {
            henka_free(history->undo);
            henka_free(history->redo);
            henka_free(history);
        }
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    history->max_steps = max_steps;
    history->undo[0] = initial;
    history->undo_count = 1U;
    *out_history = history;
    return HENKA_SUCCESS;
}

void henka_authoring_mesh_history_destroy(henka_authoring_mesh_history* history)
{
    size_t index;
    if (history == NULL)
    {
        return;
    }
    for (index = 0U; index < history->undo_count; ++index)
    {
        henka_authoring_mesh_destroy(history->undo[index]);
    }
    for (index = 0U; index < history->redo_count; ++index)
    {
        henka_authoring_mesh_destroy(history->redo[index]);
    }
    henka_free(history->undo);
    henka_free(history->redo);
    henka_free(history);
}

henka_result henka_authoring_mesh_history_checkpoint(henka_authoring_mesh_history* history, const henka_authoring_mesh* mesh)
{
    henka_authoring_mesh* snapshot = NULL;
    henka_result result;
    if (!authoring_history_mesh_matches(history, mesh) || !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = authoring_mesh_clone_internal(mesh, &snapshot);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    for (result = HENKA_SUCCESS; history->redo_count > 0U; --history->redo_count)
    {
        henka_authoring_mesh_destroy(history->redo[history->redo_count - 1U]);
    }
    return authoring_history_append(history->undo, &history->undo_count, history->max_steps, snapshot);
}

bool henka_authoring_mesh_history_can_undo(const henka_authoring_mesh_history* history)
{
    return history != NULL && history->undo_count > 1U;
}

bool henka_authoring_mesh_history_can_redo(const henka_authoring_mesh_history* history)
{
    return history != NULL && history->redo_count > 0U;
}

static henka_result authoring_history_restore(
    henka_authoring_mesh_history* history,
    henka_authoring_mesh* mesh,
    henka_authoring_mesh** target,
    size_t target_index,
    henka_authoring_mesh** opposite,
    size_t* target_count,
    size_t* opposite_count)
{
    henka_authoring_mesh* current = NULL;
    henka_authoring_mesh* replacement = NULL;
    henka_result result;
    result = authoring_mesh_clone_internal(mesh, &current);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = authoring_mesh_clone_internal(target[target_index], &replacement);
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(current);
        return result;
    }
    authoring_mesh_swap(mesh, replacement);
    henka_authoring_mesh_destroy(replacement);
    henka_authoring_mesh_destroy(target[*target_count - 1U]);
    --*target_count;
    return authoring_history_append(opposite, opposite_count, history->max_steps, current);
}

henka_result henka_authoring_mesh_history_undo(henka_authoring_mesh_history* history, henka_authoring_mesh* mesh)
{
    if (!authoring_history_mesh_matches(history, mesh) || !henka_authoring_mesh_history_can_undo(history))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return authoring_history_restore(history, mesh, history->undo, history->undo_count - 2U,
        history->redo, &history->undo_count, &history->redo_count);
}

henka_result henka_authoring_mesh_history_redo(henka_authoring_mesh_history* history, henka_authoring_mesh* mesh)
{
    if (!authoring_history_mesh_matches(history, mesh) || !henka_authoring_mesh_history_can_redo(history))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return authoring_history_restore(history, mesh, history->redo, history->redo_count - 1U,
        history->undo, &history->redo_count, &history->undo_count);
}

static bool authoring_write_u32(FILE* file, uint32_t value)
{
    const unsigned char bytes[sizeof(value)] = {
        (unsigned char)(value & 0xffU),
        (unsigned char)((value >> 8U) & 0xffU),
        (unsigned char)((value >> 16U) & 0xffU),
        (unsigned char)((value >> 24U) & 0xffU)};
    return fwrite(bytes, sizeof(bytes), 1U, file) == 1U;
}

static bool authoring_read_u32(FILE* file, uint32_t* out_value)
{
    unsigned char bytes[sizeof(uint32_t)];
    if (out_value == NULL || fread(bytes, sizeof(bytes), 1U, file) != 1U)
    {
        return false;
    }
    *out_value = (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8U) |
        ((uint32_t)bytes[2] << 16U) |
        ((uint32_t)bytes[3] << 24U);
    return true;
}

static bool authoring_write_f32(FILE* file, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return authoring_write_u32(file, bits);
}

static bool authoring_read_f32(FILE* file, float* out_value)
{
    uint32_t bits;
    if (out_value == NULL || !authoring_read_u32(file, &bits))
    {
        return false;
    }
    memcpy(out_value, &bits, sizeof(*out_value));
    return true;
}

static bool authoring_write_byte(FILE* file, unsigned char value)
{
    return fwrite(&value, sizeof(value), 1U, file) == 1U;
}

static bool authoring_read_byte(FILE* file, unsigned char* out_value)
{
    return out_value != NULL && fread(out_value, sizeof(*out_value), 1U, file) == 1U;
}

static bool authoring_write_vec3(FILE* file, henka_vec3 value)
{
    return authoring_write_f32(file, value.x) && authoring_write_f32(file, value.y) && authoring_write_f32(file, value.z);
}

static bool authoring_read_vec3(FILE* file, henka_vec3* out_value)
{
    return out_value != NULL && authoring_read_f32(file, &out_value->x) && authoring_read_f32(file, &out_value->y) && authoring_read_f32(file, &out_value->z);
}

static bool authoring_write_vec2(FILE* file, henka_vec2 value)
{
    return authoring_write_f32(file, value.x) && authoring_write_f32(file, value.y);
}

static FILE* authoring_open_file(const char* path, const char* mode)
{
    FILE* file = NULL;
#ifdef _WIN32
    if (fopen_s(&file, path, mode) != 0)
    {
        return NULL;
    }
#else
    file = fopen(path, mode);
#endif
    return file;
}

static uint32_t authoring_next_save_sequence(void)
{
#ifdef _WIN32
    return (uint32_t)InterlockedIncrement(&g_authoring_save_sequence);
#else
    return atomic_fetch_add_explicit(
        &g_authoring_save_sequence,
        1U,
        memory_order_relaxed) + 1U;
#endif
}

static bool authoring_make_temporary_path(const char* path, char** out_path)
{
    const size_t path_length = strlen(path);
    size_t allocation_size;
    char* temporary_path;
    int written;
    if (out_path == NULL || path_length > SIZE_MAX - HENKA_AUTHORING_TEMP_PATH_SUFFIX_CAPACITY ||
        !henka_checked_size_add(path_length, HENKA_AUTHORING_TEMP_PATH_SUFFIX_CAPACITY, &allocation_size))
    {
        return false;
    }
    temporary_path = henka_malloc(allocation_size);
    if (temporary_path == NULL)
    {
        return false;
    }
#ifdef _WIN32
    written = _snprintf_s(
        temporary_path,
        allocation_size,
        _TRUNCATE,
        "%s.tmp.%lu.%lu.%lu",
        path,
        (unsigned long)GetCurrentProcessId(),
        (unsigned long)GetCurrentThreadId(),
        (unsigned long)authoring_next_save_sequence());
#else
    written = snprintf(
        temporary_path,
        allocation_size,
        "%s.tmp.%ld.%lu",
        path,
        (long)getpid(),
        (unsigned long)authoring_next_save_sequence());
#endif
    if (written < 0 || (size_t)written >= allocation_size)
    {
        henka_free(temporary_path);
        return false;
    }
    *out_path = temporary_path;
    return true;
}

static bool authoring_read_vec2(FILE* file, henka_vec2* out_value)
{
    return out_value != NULL && authoring_read_f32(file, &out_value->x) && authoring_read_f32(file, &out_value->y);
}

henka_result henka_authoring_mesh_save_file(const henka_authoring_mesh* mesh, const char* path)
{
    FILE* file = NULL;
    char* temporary_path = NULL;
    size_t index;
    bool ok = false;
    if (mesh == NULL || path == NULL || path[0] == '\0' || !henka_authoring_mesh_validate(mesh))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_path_ensure_parent_directory(path) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (!authoring_make_temporary_path(path, &temporary_path))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    file = authoring_open_file(temporary_path, "wb");
    if (file == NULL)
    {
        henka_free(temporary_path);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    ok = fwrite("HAMS", 4U, 1U, file) == 1U && authoring_write_u32(file, HENKA_AUTHORING_MESH_FILE_VERSION) &&
        authoring_write_u32(file, (uint32_t)mesh->desc.max_vertices) &&
        authoring_write_u32(file, (uint32_t)mesh->desc.max_edges) &&
        authoring_write_u32(file, (uint32_t)mesh->desc.max_faces) &&
        authoring_write_u32(file, (uint32_t)mesh->desc.max_face_corners) &&
        authoring_write_u32(file, (uint32_t)mesh->vertex_slots) &&
        authoring_write_u32(file, (uint32_t)mesh->edge_slots) &&
        authoring_write_u32(file, (uint32_t)mesh->face_slots);
    for (index = 0U; ok && index < mesh->vertex_slots; ++index)
    {
        const henka_authoring_vertex* vertex = &mesh->vertices[index];
        ok = authoring_write_byte(file, vertex->active ? 1U : 0U);
        if (ok && vertex->active)
        {
            ok = authoring_write_vec3(file, vertex->position) && authoring_write_vec2(file, vertex->uv) &&
                authoring_write_u32(file, vertex->material_region);
        }
    }
    for (index = 0U; ok && index < mesh->edge_slots; ++index)
    {
        const henka_authoring_edge* edge = &mesh->edges[index];
        ok = authoring_write_byte(file, edge->active ? 1U : 0U);
        if (ok && edge->active)
        {
            ok = authoring_write_u32(file, edge->vertices[0]) && authoring_write_u32(file, edge->vertices[1]) &&
                authoring_write_u32(file, edge->faces[0]) && authoring_write_u32(file, edge->faces[1]) &&
                authoring_write_u32(file, (uint32_t)edge->face_count) && authoring_write_byte(file, edge->hard ? 1U : 0U);
        }
    }
    for (index = 0U; ok && index < mesh->face_slots; ++index)
    {
        const henka_authoring_face* face = &mesh->faces[index];
        size_t corner;
        ok = authoring_write_byte(file, face->active ? 1U : 0U);
        if (ok && face->active)
        {
            ok = authoring_write_u32(file, (uint32_t)face->corner_count) &&
                authoring_write_u32(file, face->material_region) && authoring_write_byte(file, face->smooth ? 1U : 0U);
            for (corner = 0U; ok && corner < face->corner_count; ++corner)
            {
                ok = authoring_write_u32(file, face->vertices[corner]);
            }
            for (corner = 0U; ok && corner < face->corner_count; ++corner)
            {
                ok = authoring_write_vec2(file, face->uvs[corner]);
            }
            for (corner = 0U; ok && corner < face->corner_count; ++corner)
            {
                ok = authoring_write_u32(file, face->edges[corner]);
            }
        }
    }
    ok = ok && fflush(file) == 0 && fclose(file) == 0;
    file = NULL;
    if (ok)
    {
#ifdef _WIN32
    ok = MoveFileExA(temporary_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        ok = rename(temporary_path, path) == 0;
#endif
    }
    if (!ok)
    {
        if (file != NULL)
        {
            fclose(file);
        }
        remove(temporary_path);
    }
    henka_free(temporary_path);
    return ok ? HENKA_SUCCESS : HENKA_ERROR_ASSET_SOURCE;
}

henka_result henka_authoring_mesh_load_file(henka_authoring_mesh* mesh, const char* path)
{
    FILE* file = NULL;
    henka_authoring_mesh* candidate = NULL;
    henka_authoring_mesh_desc file_desc;
    uint32_t version;
    uint32_t slots[3];
    uint32_t capacities[4];
    size_t index;
    henka_result result = HENKA_ERROR_ASSET_SOURCE;
    if (mesh == NULL || path == NULL || path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    file = authoring_open_file(path, "rb");
    if (file == NULL)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    {
        char magic[4];
        if (fread(magic, sizeof(magic), 1U, file) != 1U || memcmp(magic, "HAMS", 4U) != 0 ||
            !authoring_read_u32(file, &version) ||
            (version != HENKA_AUTHORING_MESH_LEGACY_FILE_VERSION &&
             version != HENKA_AUTHORING_MESH_FILE_VERSION) ||
            !authoring_read_u32(file, &capacities[0]) || !authoring_read_u32(file, &capacities[1]) ||
            !authoring_read_u32(file, &capacities[2]) || !authoring_read_u32(file, &capacities[3]) ||
            !authoring_read_u32(file, &slots[0]) || !authoring_read_u32(file, &slots[1]) ||
            !authoring_read_u32(file, &slots[2]))
        {
            goto cleanup;
        }
    }
    file_desc = (henka_authoring_mesh_desc){capacities[0], capacities[1], capacities[2], capacities[3]};
    if (!authoring_desc_valid(&file_desc) || !authoring_desc_equal(&file_desc, &mesh->desc) ||
        slots[0] > capacities[0] || slots[1] > capacities[1] || slots[2] > capacities[2])
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    result = henka_authoring_mesh_create(&mesh->desc, &candidate);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    candidate->vertex_slots = slots[0];
    candidate->edge_slots = slots[1];
    candidate->face_slots = slots[2];
    for (index = 0U; index < candidate->vertex_slots; ++index)
    {
        unsigned char active;
        henka_authoring_vertex* vertex = &candidate->vertices[index];
        if (!authoring_read_byte(file, &active) || active > 1U)
        {
            goto cleanup;
        }
        vertex->id = (henka_authoring_vertex_id)(index + 1U);
        vertex->active = active != 0U;
        if (vertex->active && (!authoring_read_vec3(file, &vertex->position) ||
            !authoring_read_vec2(file, &vertex->uv) || !authoring_read_u32(file, &vertex->material_region)))
        {
            goto cleanup;
        }
    }
    for (index = 0U; index < candidate->edge_slots; ++index)
    {
        unsigned char active;
        henka_authoring_edge* edge = &candidate->edges[index];
        uint32_t face_count = 0U;
        if (!authoring_read_byte(file, &active) || active > 1U)
        {
            goto cleanup;
        }
        edge->id = (henka_authoring_edge_id)(index + 1U);
        edge->active = active != 0U;
        if (edge->active && (!authoring_read_u32(file, &edge->vertices[0]) ||
            !authoring_read_u32(file, &edge->vertices[1]) || !authoring_read_u32(file, &edge->faces[0]) ||
            !authoring_read_u32(file, &edge->faces[1]) || !authoring_read_u32(file, &face_count) ||
            !authoring_read_byte(file, &active) || active > 1U || face_count > 2U))
        {
            goto cleanup;
        }
        if (edge->active)
        {
            edge->face_count = face_count;
            edge->hard = active != 0U;
        }
    }
    for (index = 0U; index < candidate->face_slots; ++index)
    {
        unsigned char active;
        henka_authoring_face* face = &candidate->faces[index];
        uint32_t corner_count;
        size_t corner_bytes;
        size_t uv_bytes;
        size_t corner;
        if (!authoring_read_byte(file, &active) || active > 1U)
        {
            goto cleanup;
        }
        face->id = (henka_authoring_face_id)(index + 1U);
        face->active = active != 0U;
        if (!face->active)
        {
            continue;
        }
        if (!authoring_read_u32(file, &corner_count) || corner_count < 3U ||
            corner_count > candidate->desc.max_face_corners || !authoring_read_u32(file, &face->material_region) ||
            !authoring_read_byte(file, &active) || active > 1U)
        {
            goto cleanup;
        }
        face->corner_count = corner_count;
        face->smooth = active != 0U;
        if (!henka_checked_size_multiply(
                (size_t)corner_count,
                sizeof(*face->vertices),
                &corner_bytes) ||
            !henka_checked_size_multiply(
                (size_t)corner_count,
                sizeof(*face->uvs),
                &uv_bytes))
        {
            goto cleanup;
        }
        face->vertices = henka_malloc(corner_bytes);
        face->edges = henka_malloc(corner_bytes);
        face->uvs = henka_malloc(uv_bytes);
        if (face->vertices == NULL || face->edges == NULL || face->uvs == NULL)
        {
            goto cleanup;
        }
        for (corner = 0U; corner < corner_count; ++corner)
        {
            if (!authoring_read_u32(file, &face->vertices[corner]))
            {
                goto cleanup;
            }
        }
        for (corner = 0U; corner < corner_count; ++corner)
        {
            if (!authoring_read_vec2(file, &face->uvs[corner]))
            {
                goto cleanup;
            }
        }
        for (corner = 0U; corner < corner_count; ++corner)
        {
            if (!authoring_read_u32(file, &face->edges[corner]))
            {
                goto cleanup;
            }
        }
    }
    candidate->edge_lookup_ready = false;
    if (fgetc(file) != EOF || !henka_authoring_mesh_validate(candidate))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    authoring_mesh_swap(mesh, candidate);
    result = HENKA_SUCCESS;

cleanup:
    fclose(file);
    henka_authoring_mesh_destroy(candidate);
    return result;
}

henka_result henka_authoring_mesh_load_file_new(const char* path, henka_authoring_mesh** out_mesh)
{
    FILE* file = NULL;
    henka_authoring_mesh_desc desc;
    henka_authoring_mesh* candidate = NULL;
    henka_result result;

    if (out_mesh == NULL || *out_mesh != NULL || path == NULL || path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    file = authoring_open_file(path, "rb");
    if (file == NULL)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    {
        char magic[4];
        uint32_t version;
        uint32_t capacities[4] = {0U, 0U, 0U, 0U};
        const bool header_ok = fread(magic, sizeof(magic), 1U, file) == 1U &&
            memcmp(magic, "HAMS", sizeof(magic)) == 0 &&
            authoring_read_u32(file, &version) &&
            (version == HENKA_AUTHORING_MESH_LEGACY_FILE_VERSION ||
             version == HENKA_AUTHORING_MESH_FILE_VERSION) &&
            authoring_read_u32(file, &capacities[0]) &&
            authoring_read_u32(file, &capacities[1]) &&
            authoring_read_u32(file, &capacities[2]) &&
            authoring_read_u32(file, &capacities[3]);
        desc = (henka_authoring_mesh_desc){
            capacities[0], capacities[1], capacities[2], capacities[3]};
        if (!header_ok || !authoring_desc_valid(&desc))
        {
            fclose(file);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    if (fclose(file) != 0)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }
    result = henka_authoring_mesh_create(&desc, &candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_load_file(candidate, path);
    }
    if (result == HENKA_SUCCESS)
    {
        *out_mesh = candidate;
    }
    else
    {
        henka_authoring_mesh_destroy(candidate);
    }
    return result;
}
