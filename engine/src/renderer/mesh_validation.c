#include "mesh_validation.h"

#include <math.h>

#include "../core/checked.h"

henka_result henka_mesh_validate_terrain_upload_data(
    const henka_vertex* vertices,
    size_t vertex_count,
    const unsigned int* indices,
    size_t index_count)
{
    size_t vertex_index;
    size_t index;

    if (vertices == NULL || indices == NULL || vertex_count == 0U || index_count == 0U ||
        vertex_count > HENKA_MAX_MESH_ELEMENTS || index_count > HENKA_MAX_MESH_ELEMENTS ||
        (index_count % 3U) != 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (vertex_index = 0U; vertex_index < vertex_count; ++vertex_index)
    {
        const henka_vertex* vertex = &vertices[vertex_index];
        if (!isfinite(vertex->position.x) || !isfinite(vertex->position.y) ||
            !isfinite(vertex->position.z) || !isfinite(vertex->normal.x) ||
            !isfinite(vertex->normal.y) || !isfinite(vertex->normal.z) ||
            !isfinite(vertex->uv.x) || !isfinite(vertex->uv.y) ||
            !isfinite(vertex->uv1.x) || !isfinite(vertex->uv1.y) ||
            (vertex->color_valid &&
                (!isfinite(vertex->color.x) || !isfinite(vertex->color.y) ||
                 !isfinite(vertex->color.z) || !isfinite(vertex->color.w) ||
                 vertex->color.x < 0.0f || vertex->color.x > 1.0f ||
                 vertex->color.y < 0.0f || vertex->color.y > 1.0f ||
                 vertex->color.z < 0.0f || vertex->color.z > 1.0f ||
                 vertex->color.w < 0.0f || vertex->color.w > 1.0f)) ||
            (vertex->tangent_valid &&
                (!isfinite(vertex->tangent.x) || !isfinite(vertex->tangent.y) ||
                 !isfinite(vertex->tangent.z) || !isfinite(vertex->tangent.w) ||
                 henka_vec3_length((henka_vec3){
                     vertex->tangent.x, vertex->tangent.y, vertex->tangent.z}) <= 0.000001f ||
                 fabsf(vertex->tangent.w) < 0.5f)))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < index_count; ++index)
    {
        if ((size_t)indices[index] >= vertex_count)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    return HENKA_SUCCESS;
}
