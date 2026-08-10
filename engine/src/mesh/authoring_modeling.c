#include <henka/authoring_modeling.h>

#include <math.h>

#include <henka/memory.h>

static bool modeling_finite_vec3(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool modeling_finite_scalar(float value)
{
    return isfinite(value);
}

static henka_result modeling_commit(
    henka_authoring_mesh* destination,
    henka_authoring_mesh* candidate)
{
    henka_result result = henka_authoring_mesh_copy(destination, candidate);
    henka_authoring_mesh_destroy(candidate);
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
    return henka_authoring_mesh_add_face(
        mesh, vertices, source->corner_count, source->material_region, source->smooth, out_face_id);
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
    const henka_authoring_face* source;
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
    }
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    source = henka_authoring_mesh_get_face(candidate, face_id);
    {
        const henka_vec3 normal = modeling_face_normal(candidate, source);
        const henka_vec3 offset = henka_vec3_scale(normal, distance);
        for (corner = 0U; corner < source->corner_count; ++corner)
        {
            const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(candidate, original[corner]);
            result = henka_authoring_mesh_add_vertex(candidate,
                henka_vec3_add(vertex->position, offset), vertex->uv, vertex->material_region, &duplicated[corner]);
            if (result != HENKA_SUCCESS)
            {
                goto cleanup;
            }
        }
        for (corner = 0U; corner < source->corner_count; ++corner)
        {
            henka_authoring_vertex_id side[4] = {
                original[corner], original[(corner + 1U) % source->corner_count],
                duplicated[(corner + 1U) % source->corner_count], duplicated[corner]};
            result = henka_authoring_mesh_add_face(candidate, side, 4U, source->material_region, false, &new_face_id);
            if (result != HENKA_SUCCESS)
            {
                goto cleanup;
            }
        }
        result = henka_authoring_mesh_add_face(candidate, duplicated, source->corner_count,
            source->material_region, source->smooth, &new_face_id);
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
        center = henka_vec3_add(center, vertex->position);
    }
    center = henka_vec3_scale(center, 1.0f / (float)source->corner_count);
    for (corner = 0U; corner < source->corner_count; ++corner)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(mesh, outer[corner]);
        const henka_vec3 position = henka_vec3_add(center,
            henka_vec3_scale(henka_vec3_subtract(vertex->position, center), factor));
        result = henka_authoring_mesh_add_vertex(mesh, position, vertex->uv,
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
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    return henka_authoring_mesh_add_face(mesh, inner, corner_count,
        material_region, smooth, out_face_id);
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
        center = henka_vec3_add(center,
            henka_authoring_mesh_get_vertex(mesh, outer[corner])->position);
    }
    center = henka_vec3_scale(center, 1.0f / (float)source->corner_count);
    result = henka_authoring_mesh_clone(mesh, &candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    source = henka_authoring_mesh_get_face(candidate, face_id);
    corner_count = source->corner_count;
    material_region = source->material_region;
    smooth = source->smooth;
    result = henka_authoring_mesh_add_vertex(candidate, center, (henka_vec2){0.5f, 0.5f},
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
