#include "henka_internal.h"

#include <math.h>
#include <string.h>

#include <henka/core.h>
#include <henka/log.h>
#include <henka/memory.h>

#include "../core/checked.h"

static void henka_terrain_mesh_append_skirt_segment(
    henka_terrain_mesh_data* mesh,
    uint32_t first_source,
    uint32_t second_source,
    float skirt_depth,
    henka_vec3 normal,
    bool forward)
{
    henka_terrain_mesh_vertex first = mesh->vertices[first_source];
    henka_terrain_mesh_vertex second = mesh->vertices[second_source];
    henka_terrain_mesh_vertex first_bottom = first;
    henka_terrain_mesh_vertex second_bottom = second;
    uint32_t first_vertex = mesh->vertex_count;

    first.normal[0] = normal.x;
    first.normal[1] = normal.y;
    first.normal[2] = normal.z;
    second.normal[0] = normal.x;
    second.normal[1] = normal.y;
    second.normal[2] = normal.z;
    first_bottom.normal[0] = normal.x;
    first_bottom.normal[1] = normal.y;
    first_bottom.normal[2] = normal.z;
    second_bottom.normal[0] = normal.x;
    second_bottom.normal[1] = normal.y;
    second_bottom.normal[2] = normal.z;
    first_bottom.position[1] -= skirt_depth;
    second_bottom.position[1] -= skirt_depth;
    mesh->vertices[mesh->vertex_count++] = first;
    mesh->vertices[mesh->vertex_count++] = second;
    mesh->vertices[mesh->vertex_count++] = first_bottom;
    mesh->vertices[mesh->vertex_count++] = second_bottom;
    if (forward)
    {
        mesh->indices[mesh->index_count++] = first_vertex;
        mesh->indices[mesh->index_count++] = first_vertex + 1U;
        mesh->indices[mesh->index_count++] = first_vertex + 2U;
        mesh->indices[mesh->index_count++] = first_vertex + 1U;
        mesh->indices[mesh->index_count++] = first_vertex + 3U;
        mesh->indices[mesh->index_count++] = first_vertex + 2U;
    }
    else
    {
        mesh->indices[mesh->index_count++] = first_vertex;
        mesh->indices[mesh->index_count++] = first_vertex + 2U;
        mesh->indices[mesh->index_count++] = first_vertex + 1U;
        mesh->indices[mesh->index_count++] = first_vertex + 1U;
        mesh->indices[mesh->index_count++] = first_vertex + 2U;
        mesh->indices[mesh->index_count++] = first_vertex + 3U;
    }
}

static henka_result henka_terrain_mesh_append_skirts(
    henka_terrain_mesh_data* mesh,
    uint32_t samples_per_side,
    float skirt_depth,
    uint32_t skirt_mask)
{
    uint32_t segment;
    uint32_t base_vertex_count;
    uint32_t base_index_count;
    uint64_t segment_count;
    uint64_t required_vertices;
    uint64_t required_indices;
    uint32_t edge_count = 0U;

    if (mesh == NULL || samples_per_side < 2U ||
        (skirt_mask & ~HENKA_TERRAIN_MESH_EDGE_ALL) != 0U ||
        !isfinite(skirt_depth) || skirt_depth <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if ((skirt_mask & HENKA_TERRAIN_MESH_EDGE_NORTH) != 0U) ++edge_count;
    if ((skirt_mask & HENKA_TERRAIN_MESH_EDGE_EAST) != 0U) ++edge_count;
    if ((skirt_mask & HENKA_TERRAIN_MESH_EDGE_SOUTH) != 0U) ++edge_count;
    if ((skirt_mask & HENKA_TERRAIN_MESH_EDGE_WEST) != 0U) ++edge_count;
    base_vertex_count = mesh->vertex_count;
    base_index_count = mesh->index_count;
    segment_count = (uint64_t)(samples_per_side - 1U) * edge_count;
    required_vertices = (uint64_t)base_vertex_count + segment_count * 4U;
    required_indices = (uint64_t)base_index_count + segment_count * 6U;
    if (required_vertices > mesh->vertex_capacity || required_indices > mesh->index_capacity ||
        required_vertices > UINT32_MAX || required_indices > UINT32_MAX)
    {
        return HENKA_ERROR_LIMIT;
    }
    for (segment = 0U; segment + 1U < samples_per_side; ++segment)
    {
        if ((skirt_mask & HENKA_TERRAIN_MESH_EDGE_NORTH) != 0U)
        {
            henka_terrain_mesh_append_skirt_segment(
                mesh, segment, segment + 1U, skirt_depth,
                (henka_vec3){0.0f, 0.0f, -1.0f}, true);
        }
        if ((skirt_mask & HENKA_TERRAIN_MESH_EDGE_SOUTH) != 0U)
        {
            henka_terrain_mesh_append_skirt_segment(
                mesh,
                (samples_per_side - 1U) * samples_per_side + segment,
                (samples_per_side - 1U) * samples_per_side + segment + 1U,
                skirt_depth, (henka_vec3){0.0f, 0.0f, 1.0f}, false);
        }
        if ((skirt_mask & HENKA_TERRAIN_MESH_EDGE_WEST) != 0U)
        {
            henka_terrain_mesh_append_skirt_segment(
                mesh, segment * samples_per_side,
                (segment + 1U) * samples_per_side,
                skirt_depth, (henka_vec3){-1.0f, 0.0f, 0.0f}, false);
        }
        if ((skirt_mask & HENKA_TERRAIN_MESH_EDGE_EAST) != 0U)
        {
            henka_terrain_mesh_append_skirt_segment(
                mesh, segment * samples_per_side + samples_per_side - 1U,
                (segment + 1U) * samples_per_side + samples_per_side - 1U,
                skirt_depth, (henka_vec3){1.0f, 0.0f, 0.0f}, true);
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_mesh_create_cube(henka_engine* engine, henka_mesh** out_mesh)
{
    static const henka_vertex vertices[] =
    {
        {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}}
    };
    static const unsigned int indices[] =
    {
         0U,  1U,  2U,  2U,  3U,  0U,
         4U,  5U,  6U,  6U,  7U,  4U,
         8U,  9U, 10U, 10U, 11U,  8U,
        12U, 13U, 14U, 14U, 15U, 12U,
        16U, 17U, 18U, 18U, 19U, 16U,
        20U, 21U, 22U, 22U, 23U, 20U
    };

    if (engine == NULL || out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_renderer_create_mesh_from_data(
        engine->renderer,
        vertices,
        (int)(sizeof(vertices) / sizeof(vertices[0])),
        indices,
        (int)(sizeof(indices) / sizeof(indices[0])),
        HENKA_MESH_PRIMITIVE_TRIANGLES,
        out_mesh);
}

static bool henka_mesh_vec3_is_finite(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

henka_result henka_mesh_create_uv_sphere(
    henka_engine* engine,
    float radius,
    int segments,
    int rings,
    henka_mesh** out_mesh)
{
    size_t interior_vertex_count;
    size_t index_count_size;
    size_t vertex_count_size;
    unsigned int* indices;
    henka_result result;
    henka_vertex* vertices;
    int index;
    int vertex_count;
    int index_count;

    if (engine == NULL || out_mesh == NULL || !isfinite(radius) || radius <= 0.0f ||
        segments < 12 || segments > 128 || rings < 6 || rings > 64 ||
        !henka_checked_size_multiply((size_t)(rings - 1), (size_t)(segments + 1), &interior_vertex_count) ||
        !henka_checked_size_add(interior_vertex_count, (size_t)segments * 2U, &vertex_count_size) ||
        !henka_checked_size_multiply((size_t)segments, (size_t)(rings - 1), &index_count_size) ||
        !henka_checked_size_multiply(index_count_size, 6U, &index_count_size) ||
        !henka_checked_size_to_int(vertex_count_size, &vertex_count) ||
        !henka_checked_size_to_int(index_count_size, &index_count))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    vertices = henka_calloc(vertex_count_size, sizeof(*vertices));
    indices = henka_calloc(index_count_size, sizeof(*indices));
    if (vertices == NULL || indices == NULL)
    {
        henka_free(indices);
        henka_free(vertices);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    {
        int segment;
        for (segment = 0; segment < segments; ++segment)
        {
            float u = (float)segment / (float)segments;
            float theta = u * HENKA_PI * 2.0f;
            henka_vec3 tangent = {-sinf(theta), 0.0f, cosf(theta)};

            vertices[segment] = (henka_vertex){
                {0.0f, radius, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {u, 1.0f},
                {0.0f, 0.0f},
                {1.0f, 1.0f, 1.0f, 1.0f},
                true,
                {tangent.x, tangent.y, tangent.z, 1.0f},
                true};
        }
        for (index = 0; index < (int)interior_vertex_count; ++index)
        {
            int ring = index / (segments + 1) + 1;
            int interior_segment = index % (segments + 1);
            float v = (float)ring / (float)rings;
            float theta = (float)interior_segment / (float)segments * HENKA_PI * 2.0f;
            float phi = v * HENKA_PI;
            float sin_phi = sinf(phi);
            float cos_phi = cosf(phi);
            float sin_theta = sinf(theta);
            float cos_theta = cosf(theta);
            henka_vec3 normal = {sin_phi * cos_theta, cos_phi, sin_phi * sin_theta};
            henka_vec3 tangent = {-sin_theta, 0.0f, cos_theta};

            vertices[(size_t)segments + (size_t)index] = (henka_vertex){
                {normal.x * radius, normal.y * radius, normal.z * radius},
                normal,
                {(float)interior_segment / (float)segments, 1.0f - v},
                {0.0f, 0.0f},
                {1.0f, 1.0f, 1.0f, 1.0f},
                true,
                {tangent.x, tangent.y, tangent.z, 1.0f},
                true};
        }
        for (segment = 0; segment < segments; ++segment)
        {
            float u = (float)segment / (float)segments;
            float theta = u * HENKA_PI * 2.0f;
            size_t bottom_index = (size_t)segments + interior_vertex_count + (size_t)segment;
            henka_vec3 tangent = {-sinf(theta), 0.0f, cosf(theta)};

            vertices[bottom_index] = (henka_vertex){
                {0.0f, -radius, 0.0f},
                {0.0f, -1.0f, 0.0f},
                {u, 0.0f},
                {0.0f, 0.0f},
                {1.0f, 1.0f, 1.0f, 1.0f},
                true,
                {tangent.x, tangent.y, tangent.z, 1.0f},
                true};
        }
    }

    index = 0;
    {
        int segment;
        const unsigned int interior_base = (unsigned int)segments;
        const unsigned int bottom_base = (unsigned int)((size_t)segments + interior_vertex_count);
        for (segment = 0; segment < segments; ++segment)
        {
            unsigned int top = (unsigned int)segment;
            unsigned int first = interior_base + (unsigned int)segment;
            indices[index++] = top;
            indices[index++] = first;
            indices[index++] = first + 1U;
        }
        for (int ring = 0; ring < rings - 2; ++ring)
        {
            for (segment = 0; segment < segments; ++segment)
            {
                unsigned int first = interior_base + (unsigned int)(ring * (segments + 1) + segment);
                unsigned int second = first + (unsigned int)(segments + 1);
                indices[index++] = first;
                indices[index++] = second;
                indices[index++] = first + 1U;
                indices[index++] = second;
                indices[index++] = second + 1U;
                indices[index++] = first + 1U;
            }
        }
        for (segment = 0; segment < segments; ++segment)
        {
            unsigned int first = interior_base + (unsigned int)((rings - 2) * (segments + 1) + segment);
            unsigned int bottom = bottom_base + (unsigned int)segment;
            indices[index++] = first;
            indices[index++] = bottom;
            indices[index++] = first + 1U;
        }
    }

    result = henka_renderer_create_mesh_from_data(
        engine->renderer,
        vertices,
        vertex_count,
        indices,
        index_count,
        HENKA_MESH_PRIMITIVE_TRIANGLES,
        out_mesh);
    henka_free(indices);
    henka_free(vertices);
    return result;
}

henka_result henka_mesh_create_plane(henka_engine* engine, float width, float depth, henka_mesh** out_mesh)
{
    float half_depth;
    float half_width;
    unsigned int indices[6];
    henka_vertex vertices[4];

    if (engine == NULL || out_mesh == NULL ||
        !isfinite(width) || !isfinite(depth) || width <= 0.0f || depth <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    half_width = width * 0.5f;
    half_depth = depth * 0.5f;
    if (!isfinite(half_width) || !isfinite(half_depth))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    vertices[0] = (henka_vertex){{-half_width, 0.0f, -half_depth}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
    vertices[1] = (henka_vertex){{ half_width, 0.0f, -half_depth}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}};
    vertices[2] = (henka_vertex){{ half_width, 0.0f,  half_depth}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}};
    vertices[3] = (henka_vertex){{-half_width, 0.0f,  half_depth}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}};

    indices[0] = 0U;
    indices[1] = 1U;
    indices[2] = 2U;
    indices[3] = 2U;
    indices[4] = 3U;
    indices[5] = 0U;

    return henka_renderer_create_mesh_from_data(
        engine->renderer,
        vertices,
        4,
        indices,
        6,
        HENKA_MESH_PRIMITIVE_TRIANGLES,
        out_mesh);
}

henka_result henka_mesh_create_debug_grid(henka_engine* engine, int half_extent, float spacing, henka_mesh** out_mesh)
{
    int axis;
    size_t axis_count;
    float extent;
    int index_count;
    size_t index_count_size;
    unsigned int* indices;
    size_t line_count;
    int offset;
    henka_result result;
    int vertex_count;
    size_t vertex_count_size;
    henka_vertex* vertices;

    if (engine == NULL || out_mesh == NULL || half_extent <= 0 ||
        half_extent > HENKA_MAX_DEBUG_GRID_HALF_EXTENT ||
        !isfinite(spacing) || spacing <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    extent = (float)half_extent * spacing;
    if (!isfinite(extent) ||
        !henka_checked_size_multiply((size_t)half_extent, 2U, &axis_count) ||
        !henka_checked_size_add(axis_count, 1U, &axis_count) ||
        !henka_checked_size_multiply(axis_count, 2U, &line_count) ||
        !henka_checked_size_multiply(line_count, 2U, &vertex_count_size) ||
        !henka_checked_size_to_int(vertex_count_size, &vertex_count))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    index_count_size = vertex_count_size;
    if (!henka_checked_size_to_int(index_count_size, &index_count))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    vertices = henka_calloc(vertex_count_size, sizeof(*vertices));
    indices = henka_calloc(index_count_size, sizeof(*indices));
    if (vertices == NULL || indices == NULL)
    {
        henka_free(indices);
        henka_free(vertices);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    offset = 0;
    for (axis = -half_extent; axis <= half_extent; ++axis)
    {
        float position;

        position = (float)axis * spacing;
        vertices[offset + 0] = (henka_vertex){{position, 0.01f, -extent}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
        vertices[offset + 1] = (henka_vertex){{position, 0.01f,  extent}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
        indices[offset + 0] = (unsigned int)(offset + 0);
        indices[offset + 1] = (unsigned int)(offset + 1);
        offset += 2;

        vertices[offset + 0] = (henka_vertex){{-extent, 0.01f, position}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
        vertices[offset + 1] = (henka_vertex){{ extent, 0.01f, position}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
        indices[offset + 0] = (unsigned int)(offset + 0);
        indices[offset + 1] = (unsigned int)(offset + 1);
        offset += 2;
    }

    result = henka_renderer_create_mesh_from_data(
        engine->renderer,
        vertices,
        vertex_count,
        indices,
        index_count,
        HENKA_MESH_PRIMITIVE_LINES,
        out_mesh);

    henka_free(indices);
    henka_free(vertices);
    return result;
}

henka_result henka_mesh_create_line(henka_engine* engine, henka_vec3 start, henka_vec3 end, henka_mesh** out_mesh)
{
    unsigned int indices[2];
    henka_vertex vertices[2];

    if (engine == NULL || out_mesh == NULL ||
        !henka_mesh_vec3_is_finite(start) || !henka_mesh_vec3_is_finite(end))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    vertices[0] = (henka_vertex){start, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
    vertices[1] = (henka_vertex){end, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
    indices[0] = 0U;
    indices[1] = 1U;
    return henka_renderer_create_mesh_from_data(
        engine->renderer,
        vertices,
        2,
        indices,
        2,
        HENKA_MESH_PRIMITIVE_LINES,
        out_mesh);
}

henka_result henka_mesh_create_circle_ring(henka_engine* engine, float radius, int segments, henka_mesh** out_mesh)
{
    int index;
    int index_count;
    size_t index_count_size;
    unsigned int* indices;
    henka_result result;
    henka_vertex* vertices;

    if (engine == NULL || out_mesh == NULL || !isfinite(radius) || radius <= 0.0f ||
        segments < 8 || segments > HENKA_MAX_CIRCLE_SEGMENTS ||
        !henka_checked_size_multiply((size_t)segments, 2U, &index_count_size) ||
        !henka_checked_size_to_int(index_count_size, &index_count))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    vertices = henka_calloc((size_t)segments, sizeof(*vertices));
    indices = henka_calloc(index_count_size, sizeof(*indices));
    if (vertices == NULL || indices == NULL)
    {
        henka_free(indices);
        henka_free(vertices);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    for (index = 0; index < segments; ++index)
    {
        float angle;

        angle = ((float)index / (float)segments) * HENKA_PI * 2.0f;
        vertices[index] = (henka_vertex)
        {
            {cosf(angle) * radius, sinf(angle) * radius, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f}
        };
        indices[index * 2] = (unsigned int)index;
        indices[index * 2 + 1] = (unsigned int)((index + 1) % segments);
    }

    result = henka_renderer_create_mesh_from_data(
        engine->renderer,
        vertices,
        segments,
        indices,
        index_count,
        HENKA_MESH_PRIMITIVE_LINES,
        out_mesh);
    henka_free(indices);
    henka_free(vertices);
    return result;
}

henka_result henka_mesh_create_from_terrain_chunk_with_edge_mask(
    henka_engine* engine,
    const henka_terrain_world* world,
    henka_terrain_chunk_id chunk_id,
    uint32_t lod_level,
    uint32_t edge_transition_mask,
    uint32_t fallback_skirt_mask,
    henka_mesh** out_mesh,
    henka_terrain_revision* out_revision,
    henka_terrain_generation* out_generation)
{
    henka_terrain_mesh_data terrain_mesh = {0};
    henka_terrain_mesh_vertex* terrain_vertices = NULL;
    henka_vertex* render_vertices = NULL;
    unsigned int* indices = NULL;
    henka_terrain_world_desc terrain_desc;
    henka_result result;
    int vertex_count;
    int index_count;
    uint32_t samples_per_side;
    uint32_t skirt_edge_count = 0U;
    uint64_t skirt_segments;
    uint64_t total_vertices;
    uint64_t total_indices;
    uint32_t index;

    if (out_mesh == NULL || engine == NULL || world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_mesh = NULL;
    if (out_revision != NULL)
    {
        *out_revision = 0U;
    }
    if (out_generation != NULL)
    {
        *out_generation = 0U;
    }

    if ((edge_transition_mask & ~HENKA_TERRAIN_MESH_EDGE_ALL) != 0U ||
        (fallback_skirt_mask & ~HENKA_TERRAIN_MESH_EDGE_ALL) != 0U ||
        (edge_transition_mask & fallback_skirt_mask) != 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    terrain_mesh.vertex_capacity = 0U;
    terrain_mesh.index_capacity = 0U;
    result = henka_terrain_mesh_build_chunk_with_edge_mask(
        world, chunk_id, lod_level, edge_transition_mask, &terrain_mesh);
    if (result != HENKA_ERROR_LIMIT || terrain_mesh.vertex_count == 0U || terrain_mesh.index_count == 0U)
    {
        return result == HENKA_SUCCESS ? HENKA_ERROR_INVALID_ARGUMENT : result;
    }
    if (henka_terrain_world_get_desc(world, &terrain_desc) != HENKA_SUCCESS ||
        terrain_desc.samples_per_chunk < 2U ||
        (terrain_desc.samples_per_chunk - 1U) % (1U << lod_level) != 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    samples_per_side = (terrain_desc.samples_per_chunk - 1U) / (1U << lod_level) + 1U;
    if ((fallback_skirt_mask & HENKA_TERRAIN_MESH_EDGE_NORTH) != 0U) ++skirt_edge_count;
    if ((fallback_skirt_mask & HENKA_TERRAIN_MESH_EDGE_EAST) != 0U) ++skirt_edge_count;
    if ((fallback_skirt_mask & HENKA_TERRAIN_MESH_EDGE_SOUTH) != 0U) ++skirt_edge_count;
    if ((fallback_skirt_mask & HENKA_TERRAIN_MESH_EDGE_WEST) != 0U) ++skirt_edge_count;
    skirt_segments = (uint64_t)(samples_per_side - 1U) * skirt_edge_count;
    total_vertices = (uint64_t)terrain_mesh.vertex_count + skirt_segments * 4U;
    total_indices = (uint64_t)terrain_mesh.index_count + skirt_segments * 6U;
    if (total_vertices > UINT32_MAX || total_indices > UINT32_MAX ||
        total_vertices > HENKA_MAX_MESH_ELEMENTS || total_indices > HENKA_MAX_MESH_ELEMENTS)
    {
        return HENKA_ERROR_LIMIT;
    }
    terrain_vertices = henka_calloc((size_t)total_vertices, sizeof(*terrain_vertices));
    render_vertices = henka_calloc((size_t)total_vertices, sizeof(*render_vertices));
    indices = henka_calloc((size_t)total_indices, sizeof(*indices));
    if (terrain_vertices == NULL || render_vertices == NULL || indices == NULL)
    {
        henka_free(indices);
        henka_free(render_vertices);
        henka_free(terrain_vertices);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    terrain_mesh.vertex_capacity = (uint32_t)total_vertices;
    terrain_mesh.index_capacity = (uint32_t)total_indices;
    terrain_mesh.vertices = terrain_vertices;
    terrain_mesh.indices = indices;
    result = henka_terrain_mesh_build_chunk_with_edge_mask(
        world, chunk_id, lod_level, edge_transition_mask, &terrain_mesh);
    if (result != HENKA_SUCCESS)
    {
        henka_free(indices);
        henka_free(render_vertices);
        henka_free(terrain_vertices);
        return result;
    }
    result = henka_terrain_mesh_append_skirts(
        &terrain_mesh,
        samples_per_side,
        fmaxf(8.0f, (float)terrain_desc.chunk_edge_meters * 0.25f),
        fallback_skirt_mask);
    if (result != HENKA_SUCCESS)
    {
        henka_free(indices);
        henka_free(render_vertices);
        henka_free(terrain_vertices);
        return result;
    }
    if (!henka_checked_size_to_int((size_t)terrain_mesh.vertex_count, &vertex_count) ||
        !henka_checked_size_to_int((size_t)terrain_mesh.index_count, &index_count))
    {
        henka_free(indices);
        henka_free(render_vertices);
        henka_free(terrain_vertices);
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    for (index = 0U; index < terrain_mesh.vertex_count; ++index)
    {
        const henka_terrain_mesh_vertex* source = &terrain_vertices[index];
        render_vertices[index] = (henka_vertex){
            {source->position[0], source->position[1], source->position[2]},
            {source->normal[0], source->normal[1], source->normal[2]},
            {source->uv[0], source->uv[1]},
            {0.0f, 0.0f},
            {(float)source->material_weights[0] / 255.0f,
             (float)source->material_weights[1] / 255.0f,
             (float)source->material_weights[2] / 255.0f,
             (float)source->material_weights[3] / 255.0f},
            true,
            {source->tangent[0], source->tangent[1], source->tangent[2], source->tangent[3]},
            true};
    }
    for (index = 0U; index < terrain_mesh.index_count; ++index)
    {
        if (indices[index] >= (unsigned int)terrain_mesh.vertex_count)
        {
            HENKA_LOG_WARN(
                "Terrain fallback skirt index %u is out of range at %u/%u",
                indices[index],
                index,
                terrain_mesh.index_count);
            break;
        }
    }
    for (index = 0U; index < terrain_mesh.vertex_count; ++index)
    {
        const henka_vertex* vertex = &render_vertices[index];
        if (!isfinite(vertex->position.x) || !isfinite(vertex->position.y) || !isfinite(vertex->position.z) ||
            !isfinite(vertex->normal.x) || !isfinite(vertex->normal.y) || !isfinite(vertex->normal.z) ||
            !isfinite(vertex->uv.x) || !isfinite(vertex->uv.y) ||
            !isfinite(vertex->tangent.x) || !isfinite(vertex->tangent.y) ||
            !isfinite(vertex->tangent.z) || !isfinite(vertex->tangent.w) ||
            fabsf(vertex->tangent.w) < 0.5f ||
            !isfinite(vertex->color.x) || !isfinite(vertex->color.y) ||
            !isfinite(vertex->color.z) || !isfinite(vertex->color.w) ||
            vertex->color.x < 0.0f || vertex->color.x > 1.0f ||
            vertex->color.y < 0.0f || vertex->color.y > 1.0f ||
            vertex->color.z < 0.0f || vertex->color.z > 1.0f ||
            vertex->color.w < 0.0f || vertex->color.w > 1.0f)
        {
            HENKA_LOG_WARN("Terrain fallback skirt vertex %u failed finite validation", index);
            break;
        }
    }
    result = henka_renderer_create_mesh_from_data(
        engine->renderer,
        render_vertices,
        vertex_count,
        indices,
        index_count,
        HENKA_MESH_PRIMITIVE_TRIANGLES,
        out_mesh);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_WARN(
            "Terrain GPU upload rejected chunk (%d,%d) LOD %u with %u vertices and %u indices: %s",
            chunk_id.x,
            chunk_id.z,
            lod_level,
            terrain_mesh.vertex_count,
            terrain_mesh.index_count,
            henka_result_to_string(result));
    }
    if (result == HENKA_SUCCESS)
    {
        if (out_revision != NULL)
        {
            *out_revision = terrain_mesh.revision;
        }
        if (out_generation != NULL)
        {
            *out_generation = terrain_mesh.generation;
        }
    }
    henka_free(indices);
    henka_free(render_vertices);
    henka_free(terrain_vertices);
    return result;
}

henka_result henka_mesh_create_from_terrain_chunk(
    henka_engine* engine,
    const henka_terrain_world* world,
    henka_terrain_chunk_id chunk_id,
    uint32_t lod_level,
    henka_mesh** out_mesh,
    henka_terrain_revision* out_revision,
    henka_terrain_generation* out_generation)
{
    return henka_mesh_create_from_terrain_chunk_with_edge_mask(
        engine, world, chunk_id, lod_level, 0U,
        HENKA_TERRAIN_MESH_EDGE_ALL, out_mesh, out_revision, out_generation);
}

void henka_mesh_destroy(henka_mesh* mesh)
{
    if (mesh == NULL)
    {
        return;
    }

    if (mesh->asset_manager_owned)
    {
        HENKA_LOG_WARN(
            "ignored an attempt to destroy a manager-owned borrowed mesh");
        return;
    }

    henka_renderer_destroy_mesh(mesh);
}

void henka_mesh_destroy_owned(henka_mesh* mesh)
{
    if (mesh == NULL)
    {
        return;
    }

    mesh->asset_manager_owned = false;
    henka_renderer_destroy_mesh(mesh);
}
