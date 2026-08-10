#include <henka/authoring_mesh.h>

#include <math.h>
#include <string.h>

#include <henka/memory.h>

struct henka_authoring_mesh
{
    henka_authoring_mesh_desc desc;
    henka_authoring_vertex* vertices;
    henka_authoring_edge* edges;
    henka_authoring_face* faces;
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
    if (!authoring_desc_valid(desc) || out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_mesh = NULL;
    mesh = henka_calloc(1U, sizeof(*mesh));
    if (mesh == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    mesh->desc = *desc;
    mesh->vertices = henka_calloc(desc->max_vertices, sizeof(*mesh->vertices));
    mesh->edges = henka_calloc(desc->max_edges, sizeof(*mesh->edges));
    mesh->faces = henka_calloc(desc->max_faces, sizeof(*mesh->faces));
    if (mesh->vertices == NULL || mesh->edges == NULL || mesh->faces == NULL)
    {
        henka_free(mesh->vertices);
        henka_free(mesh->edges);
        henka_free(mesh->faces);
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
    }
    henka_free(mesh->vertices);
    henka_free(mesh->edges);
    henka_free(mesh->faces);
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
            face->corner_count > mesh->desc.max_face_corners || face->vertices == NULL || face->edges == NULL)
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
                edge->vertices[0] != low || edge->vertices[1] != high)
            {
                return false;
            }
        }
    }
    return true;
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
    if (new_vertices == NULL || new_edges == NULL)
    {
        henka_free(new_vertices);
        henka_free(new_edges);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (corner = 0U; corner < corner_count; ++corner)
    {
        new_edges[corner] = HENKA_AUTHORING_INVALID_ID;
    }
    memcpy(new_vertices, vertices, corner_count * sizeof(*new_vertices));
    face = &mesh->faces[mesh->face_slots];
    memset(face, 0, sizeof(*face));
    face->id = (henka_authoring_face_id)(mesh->face_slots + 1U);
    face->corner_count = corner_count;
    face->vertices = new_vertices;
    face->edges = new_edges;
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
    mesh->edge_slots = edge_start;
    henka_free(face->vertices);
    henka_free(face->edges);
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
    face->vertices = NULL;
    face->edges = NULL;
    return HENKA_SUCCESS;
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

static bool authoring_face_contains_vertex(const henka_authoring_face* face, henka_authoring_vertex_id vertex_id)
{
    size_t corner;
    if (face == NULL || !face->active)
    {
        return false;
    }
    for (corner = 0U; corner < face->corner_count; ++corner)
    {
        if (face->vertices[corner] == vertex_id)
        {
            return true;
        }
    }
    return false;
}

static henka_vec3 authoring_corner_normal(const henka_authoring_mesh* mesh, const henka_authoring_face* face, henka_authoring_vertex_id vertex_id, henka_vec3 fallback)
{
    henka_vec3 normal = {0.0f, 0.0f, 0.0f};
    size_t index;
    if (!face->smooth)
    {
        return fallback;
    }
    for (index = 0U; index < mesh->face_slots; ++index)
    {
        const henka_authoring_face* candidate = &mesh->faces[index];
        size_t corner;
        bool shares_hard_edge = false;
        if (!candidate->active || !candidate->smooth || !authoring_face_contains_vertex(candidate, vertex_id))
        {
            continue;
        }
        for (corner = 0U; corner < candidate->corner_count; ++corner)
        {
            const henka_authoring_edge* edge = authoring_edge_const(mesh, candidate->edges[corner]);
            if (edge != NULL && edge->hard && (edge->vertices[0] == vertex_id || edge->vertices[1] == vertex_id))
            {
                shares_hard_edge = true;
                break;
            }
        }
        if (!shares_hard_edge)
        {
            normal = henka_vec3_add(normal, authoring_face_normal(mesh, candidate));
        }
    }
    if (henka_vec3_length(normal) <= 0.00001f)
    {
        return fallback;
    }
    return henka_vec3_normalize(normal);
}

henka_result henka_authoring_mesh_evaluate(const henka_authoring_mesh* mesh, henka_authoring_render_data* out_data)
{
    size_t required_vertices = 0U;
    size_t required_indices = 0U;
    size_t face_index;
    size_t output_vertex = 0U;
    size_t output_index = 0U;
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
    for (face_index = 0U; face_index < mesh->face_slots; ++face_index)
    {
        const henka_authoring_face* face = &mesh->faces[face_index];
        henka_vec3 face_normal;
        size_t corner;
        if (!face->active)
        {
            continue;
        }
        face_normal = authoring_face_normal(mesh, face);
        if (henka_vec3_length(face_normal) <= 0.00001f)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            const henka_authoring_vertex* source = authoring_vertex_const(mesh, face->vertices[corner]);
            out_data->vertices[output_vertex].position = source->position;
            out_data->vertices[output_vertex].uv = source->uv;
            out_data->vertices[output_vertex].material_region = face->material_region;
            out_data->vertices[output_vertex].normal = authoring_corner_normal(mesh, face, source->id, face_normal);
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
    return HENKA_SUCCESS;
}
