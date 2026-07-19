#include "henka_internal.h"

#include <math.h>

#include <henka/core.h>
#include <henka/memory.h>

#include "../core/checked.h"

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

void henka_mesh_destroy(henka_mesh* mesh)
{
    henka_renderer_destroy_mesh(mesh);
}
