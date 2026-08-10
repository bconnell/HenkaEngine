#include <henka/authoring_uv.h>

#include <math.h>

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
