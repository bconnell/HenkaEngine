#include "henka_internal.h"

#include <henka/memory.h>

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

henka_result henka_mesh_create_plane(henka_engine* engine, float width, float depth, henka_mesh** out_mesh)
{
    henka_vertex vertices[4];
    unsigned int indices[6];
    float half_depth;
    float half_width;

    if (engine == NULL || out_mesh == NULL || width <= 0.0f || depth <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    half_width = width * 0.5f;
    half_depth = depth * 0.5f;

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

    return henka_renderer_create_mesh_from_data(engine->renderer, vertices, 4, indices, 6, HENKA_MESH_PRIMITIVE_TRIANGLES, out_mesh);
}

henka_result henka_mesh_create_debug_grid(henka_engine* engine, int half_extent, float spacing, henka_mesh** out_mesh)
{
    henka_vertex* vertices;
    unsigned int* indices;
    int line_count;
    int vertex_count;
    int index_count;
    int axis;
    int offset;
    henka_result result;

    if (engine == NULL || out_mesh == NULL || half_extent <= 0 || spacing <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    line_count = ((half_extent * 2) + 1) * 2;
    vertex_count = line_count * 2;
    index_count = vertex_count;

    vertices = henka_calloc((size_t)vertex_count, sizeof(*vertices));
    indices = henka_calloc((size_t)index_count, sizeof(*indices));
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

        vertices[offset + 0] = (henka_vertex){{position, 0.01f, -(float)half_extent * spacing}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
        vertices[offset + 1] = (henka_vertex){{position, 0.01f,  (float)half_extent * spacing}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
        indices[offset + 0] = (unsigned int)(offset + 0);
        indices[offset + 1] = (unsigned int)(offset + 1);
        offset += 2;

        vertices[offset + 0] = (henka_vertex){{-(float)half_extent * spacing, 0.01f, position}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
        vertices[offset + 1] = (henka_vertex){{ (float)half_extent * spacing, 0.01f, position}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}};
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

void henka_mesh_destroy(henka_mesh* mesh)
{
    henka_renderer_destroy_mesh(mesh);
}
