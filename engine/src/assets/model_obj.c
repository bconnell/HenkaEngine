#include "henka_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>
#include <henka/model.h>

typedef struct henka_obj_index
{
    int position_index;
    int uv_index;
    int normal_index;
} henka_obj_index;

typedef struct henka_obj_face
{
    henka_obj_index indices[4];
    int count;
} henka_obj_face;

typedef struct henka_obj_vec2_array
{
    henka_vec2* items;
    size_t count;
    size_t capacity;
} henka_obj_vec2_array;

typedef struct henka_obj_vec3_array
{
    henka_vec3* items;
    size_t count;
    size_t capacity;
} henka_obj_vec3_array;

typedef struct henka_obj_vertex_array
{
    henka_model_vertex* items;
    size_t count;
    size_t capacity;
} henka_obj_vertex_array;

typedef struct henka_obj_index_array
{
    uint32_t* items;
    size_t count;
    size_t capacity;
} henka_obj_index_array;

static char* henka_read_binary_text_file(const char* path)
{
    char* buffer;
    FILE* file;
    long file_length;
    size_t bytes_read;

    if (path == NULL)
    {
        return NULL;
    }

    if (fopen_s(&file, path, "rb") != 0 || file == NULL)
    {
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    file_length = ftell(file);
    rewind(file);

    if (file_length < 0L)
    {
        fclose(file);
        return NULL;
    }

    buffer = henka_malloc((size_t)file_length + 1U);
    if (buffer == NULL)
    {
        fclose(file);
        return NULL;
    }

    bytes_read = fread(buffer, 1U, (size_t)file_length, file);
    fclose(file);
    buffer[bytes_read] = '\0';
    return buffer;
}

static void henka_obj_vec2_array_destroy(henka_obj_vec2_array* array)
{
    henka_free(array->items);
    array->items = NULL;
    array->count = 0U;
    array->capacity = 0U;
}

static void henka_obj_vec3_array_destroy(henka_obj_vec3_array* array)
{
    henka_free(array->items);
    array->items = NULL;
    array->count = 0U;
    array->capacity = 0U;
}

static void henka_obj_vertex_array_destroy(henka_obj_vertex_array* array)
{
    henka_free(array->items);
    array->items = NULL;
    array->count = 0U;
    array->capacity = 0U;
}

static void henka_obj_index_array_destroy(henka_obj_index_array* array)
{
    henka_free(array->items);
    array->items = NULL;
    array->count = 0U;
    array->capacity = 0U;
}

static henka_result henka_obj_vec2_array_push(henka_obj_vec2_array* array, henka_vec2 value)
{
    henka_vec2* items;
    size_t new_capacity;

    if (array->count == array->capacity)
    {
        new_capacity = array->capacity == 0U ? 16U : array->capacity * 2U;
        items = henka_realloc(array->items, new_capacity * sizeof(*items));
        if (items == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }

        array->items = items;
        array->capacity = new_capacity;
    }

    array->items[array->count] = value;
    array->count += 1U;
    return HENKA_SUCCESS;
}

static henka_result henka_obj_vec3_array_push(henka_obj_vec3_array* array, henka_vec3 value)
{
    henka_vec3* items;
    size_t new_capacity;

    if (array->count == array->capacity)
    {
        new_capacity = array->capacity == 0U ? 16U : array->capacity * 2U;
        items = henka_realloc(array->items, new_capacity * sizeof(*items));
        if (items == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }

        array->items = items;
        array->capacity = new_capacity;
    }

    array->items[array->count] = value;
    array->count += 1U;
    return HENKA_SUCCESS;
}

static henka_result henka_obj_vertex_array_push(henka_obj_vertex_array* array, henka_model_vertex value, uint32_t* out_index)
{
    henka_model_vertex* items;
    size_t new_capacity;

    if (array->count == array->capacity)
    {
        new_capacity = array->capacity == 0U ? 24U : array->capacity * 2U;
        items = henka_realloc(array->items, new_capacity * sizeof(*items));
        if (items == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }

        array->items = items;
        array->capacity = new_capacity;
    }

    array->items[array->count] = value;
    if (out_index != NULL)
    {
        *out_index = (uint32_t)array->count;
    }
    array->count += 1U;
    return HENKA_SUCCESS;
}

static henka_result henka_obj_index_array_push(henka_obj_index_array* array, uint32_t value)
{
    uint32_t* items;
    size_t new_capacity;

    if (array->count == array->capacity)
    {
        new_capacity = array->capacity == 0U ? 36U : array->capacity * 2U;
        items = henka_realloc(array->items, new_capacity * sizeof(*items));
        if (items == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }

        array->items = items;
        array->capacity = new_capacity;
    }

    array->items[array->count] = value;
    array->count += 1U;
    return HENKA_SUCCESS;
}

static char* henka_trim_whitespace(char* value)
{
    char* end;

    while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n')
    {
        value += 1;
    }

    end = value + strlen(value);
    while (end > value && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
    {
        end -= 1;
    }

    *end = '\0';
    return value;
}

static bool henka_parse_float_token(const char* token, float* out_value)
{
    char* end;
    float value;

    if (token == NULL || out_value == NULL)
    {
        return false;
    }

    value = strtof(token, &end);
    if (end == token || *end != '\0')
    {
        return false;
    }

    *out_value = value;
    return true;
}

static bool henka_parse_face_index(const char* token, henka_obj_index* out_index)
{
    const char* cursor;
    char* end;
    long parsed_value;

    if (token == NULL || out_index == NULL)
    {
        return false;
    }

    out_index->position_index = -1;
    out_index->uv_index = -1;
    out_index->normal_index = -1;

    cursor = token;
    parsed_value = strtol(cursor, &end, 10);
    if (end == cursor || parsed_value <= 0L)
    {
        return false;
    }

    out_index->position_index = (int)(parsed_value - 1L);
    cursor = end;

    if (*cursor == '\0')
    {
        return true;
    }

    if (*cursor != '/')
    {
        return false;
    }

    cursor += 1;
    if (*cursor != '/' && *cursor != '\0')
    {
        parsed_value = strtol(cursor, &end, 10);
        if (end == cursor || parsed_value <= 0L)
        {
            return false;
        }

        out_index->uv_index = (int)(parsed_value - 1L);
        cursor = end;
    }

    if (*cursor == '\0')
    {
        return true;
    }

    if (*cursor != '/')
    {
        return false;
    }

    cursor += 1;
    if (*cursor == '\0')
    {
        return false;
    }

    parsed_value = strtol(cursor, &end, 10);
    if (end == cursor || parsed_value <= 0L || *end != '\0')
    {
        return false;
    }

    out_index->normal_index = (int)(parsed_value - 1L);
    return true;
}

static henka_result henka_parse_vertex_line(const char* line, henka_obj_vec3_array* positions)
{
    char local_copy[256];
    char* context;
    char* token;
    henka_vec3 value;

    if (strcpy_s(local_copy, sizeof(local_copy), line) != 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    token = strtok_s(local_copy, " \t", &context);
    token = strtok_s(NULL, " \t", &context);
    if (!henka_parse_float_token(token, &value.x))
    {
        return HENKA_ERROR_UNKNOWN;
    }

    token = strtok_s(NULL, " \t", &context);
    if (!henka_parse_float_token(token, &value.y))
    {
        return HENKA_ERROR_UNKNOWN;
    }

    token = strtok_s(NULL, " \t", &context);
    if (!henka_parse_float_token(token, &value.z))
    {
        return HENKA_ERROR_UNKNOWN;
    }

    return henka_obj_vec3_array_push(positions, value);
}

static henka_result henka_parse_uv_line(const char* line, henka_obj_vec2_array* texcoords)
{
    char local_copy[256];
    char* context;
    char* token;
    henka_vec2 value;

    if (strcpy_s(local_copy, sizeof(local_copy), line) != 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    token = strtok_s(local_copy, " \t", &context);
    token = strtok_s(NULL, " \t", &context);
    if (!henka_parse_float_token(token, &value.x))
    {
        return HENKA_ERROR_UNKNOWN;
    }

    token = strtok_s(NULL, " \t", &context);
    if (!henka_parse_float_token(token, &value.y))
    {
        return HENKA_ERROR_UNKNOWN;
    }

    return henka_obj_vec2_array_push(texcoords, value);
}

static henka_result henka_parse_normal_line(const char* line, henka_obj_vec3_array* normals)
{
    char local_copy[256];
    char* context;
    char* token;
    henka_vec3 value;

    if (strcpy_s(local_copy, sizeof(local_copy), line) != 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    token = strtok_s(local_copy, " \t", &context);
    token = strtok_s(NULL, " \t", &context);
    if (!henka_parse_float_token(token, &value.x))
    {
        return HENKA_ERROR_UNKNOWN;
    }

    token = strtok_s(NULL, " \t", &context);
    if (!henka_parse_float_token(token, &value.y))
    {
        return HENKA_ERROR_UNKNOWN;
    }

    token = strtok_s(NULL, " \t", &context);
    if (!henka_parse_float_token(token, &value.z))
    {
        return HENKA_ERROR_UNKNOWN;
    }

    return henka_obj_vec3_array_push(normals, value);
}

static henka_result henka_parse_face_line(const char* line, henka_obj_face* out_face)
{
    char local_copy[512];
    char* context;
    char* token;

    if (strcpy_s(local_copy, sizeof(local_copy), line) != 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    out_face->count = 0;
    token = strtok_s(local_copy, " \t", &context);
    token = strtok_s(NULL, " \t", &context);

    while (token != NULL)
    {
        if (out_face->count >= 4)
        {
            return HENKA_ERROR_UNKNOWN;
        }

        if (!henka_parse_face_index(token, &out_face->indices[out_face->count]))
        {
            return HENKA_ERROR_UNKNOWN;
        }

        out_face->count += 1;
        token = strtok_s(NULL, " \t", &context);
    }

    if (out_face->count < 3)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    return HENKA_SUCCESS;
}

static henka_result henka_build_face_vertices(
    const henka_obj_face* face,
    const henka_obj_vec3_array* positions,
    const henka_obj_vec2_array* texcoords,
    const henka_obj_vec3_array* normals,
    henka_model_vertex out_vertices[4])
{
    henka_vec3 computed_normal;
    bool needs_computed_normal;
    int index;

    needs_computed_normal = false;
    for (index = 0; index < face->count; ++index)
    {
        const henka_obj_index* obj_index;

        obj_index = &face->indices[index];
        if (obj_index->position_index < 0 || (size_t)obj_index->position_index >= positions->count)
        {
            return HENKA_ERROR_UNKNOWN;
        }

        out_vertices[index].position = positions->items[obj_index->position_index];
        out_vertices[index].uv = (henka_vec2){0.0f, 0.0f};
        out_vertices[index].normal = (henka_vec3){0.0f, 1.0f, 0.0f};

        if (obj_index->uv_index >= 0)
        {
            if ((size_t)obj_index->uv_index >= texcoords->count)
            {
                return HENKA_ERROR_UNKNOWN;
            }

            out_vertices[index].uv = texcoords->items[obj_index->uv_index];
        }

        if (obj_index->normal_index >= 0)
        {
            if ((size_t)obj_index->normal_index >= normals->count)
            {
                return HENKA_ERROR_UNKNOWN;
            }

            out_vertices[index].normal = normals->items[obj_index->normal_index];
        }
        else
        {
            needs_computed_normal = true;
        }
    }

    if (needs_computed_normal)
    {
        henka_vec3 edge_a;
        henka_vec3 edge_b;

        edge_a = henka_vec3_subtract(out_vertices[1].position, out_vertices[0].position);
        edge_b = henka_vec3_subtract(out_vertices[2].position, out_vertices[0].position);
        computed_normal = henka_vec3_normalize(henka_vec3_cross(edge_a, edge_b));
        for (index = 0; index < face->count; ++index)
        {
            out_vertices[index].normal = computed_normal;
        }
    }

    return HENKA_SUCCESS;
}

static henka_result henka_append_triangle(
    henka_obj_vertex_array* vertices,
    henka_obj_index_array* indices,
    const henka_model_vertex* a,
    const henka_model_vertex* b,
    const henka_model_vertex* c)
{
    uint32_t base_index;
    henka_result result;

    result = henka_obj_vertex_array_push(vertices, *a, &base_index);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_obj_vertex_array_push(vertices, *b, NULL);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_obj_vertex_array_push(vertices, *c, NULL);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_obj_index_array_push(indices, base_index + 0U);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_obj_index_array_push(indices, base_index + 1U);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    return henka_obj_index_array_push(indices, base_index + 2U);
}

static henka_result henka_emit_face(
    const henka_obj_face* face,
    const henka_obj_vec3_array* positions,
    const henka_obj_vec2_array* texcoords,
    const henka_obj_vec3_array* normals,
    henka_obj_vertex_array* vertices,
    henka_obj_index_array* indices)
{
    henka_model_vertex face_vertices[4];
    henka_result result;

    result = henka_build_face_vertices(face, positions, texcoords, normals, face_vertices);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_append_triangle(vertices, indices, &face_vertices[0], &face_vertices[1], &face_vertices[2]);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    if (face->count == 4)
    {
        result = henka_append_triangle(vertices, indices, &face_vertices[0], &face_vertices[2], &face_vertices[3]);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }

    return HENKA_SUCCESS;
}

henka_result henka_model_data_load_obj_from_memory(const char* source, const char* label, henka_model_data* out_model)
{
    char* context;
    char* line;
    char* text;
    char* trimmed_line;
    henka_obj_vec2_array texcoords;
    henka_obj_vec3_array normals;
    henka_obj_vec3_array positions;
    henka_obj_vertex_array vertices;
    henka_obj_index_array indices;
    henka_obj_face face;
    henka_result result;

    if (source == NULL || out_model == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(out_model, 0, sizeof(*out_model));
    memset(&positions, 0, sizeof(positions));
    memset(&texcoords, 0, sizeof(texcoords));
    memset(&normals, 0, sizeof(normals));
    memset(&vertices, 0, sizeof(vertices));
    memset(&indices, 0, sizeof(indices));

    text = henka_malloc(strlen(source) + 1U);
    if (text == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    strcpy_s(text, strlen(source) + 1U, source);

    result = HENKA_SUCCESS;
    line = strtok_s(text, "\n", &context);
    while (line != NULL)
    {
        trimmed_line = henka_trim_whitespace(line);
        if (trimmed_line[0] == '\0' || trimmed_line[0] == '#')
        {
            line = strtok_s(NULL, "\n", &context);
            continue;
        }

        if (strncmp(trimmed_line, "v ", 2) == 0)
        {
            result = henka_parse_vertex_line(trimmed_line, &positions);
        }
        else if (strncmp(trimmed_line, "vt ", 3) == 0)
        {
            result = henka_parse_uv_line(trimmed_line, &texcoords);
        }
        else if (strncmp(trimmed_line, "vn ", 3) == 0)
        {
            result = henka_parse_normal_line(trimmed_line, &normals);
        }
        else if (strncmp(trimmed_line, "f ", 2) == 0)
        {
            result = henka_parse_face_line(trimmed_line, &face);
            if (result == HENKA_SUCCESS)
            {
                result = henka_emit_face(&face, &positions, &texcoords, &normals, &vertices, &indices);
            }
        }

        if (result != HENKA_SUCCESS)
        {
            HENKA_LOG_ERROR("Unable to parse OBJ data from '%s'", label != NULL ? label : "<memory>");
            break;
        }

        line = strtok_s(NULL, "\n", &context);
    }

    if (result == HENKA_SUCCESS && vertices.count == 0U)
    {
        HENKA_LOG_ERROR("OBJ data from '%s' did not contain any renderable faces", label != NULL ? label : "<memory>");
        result = HENKA_ERROR_UNKNOWN;
    }

    if (result == HENKA_SUCCESS)
    {
        out_model->vertices = vertices.items;
        out_model->vertex_count = (uint32_t)vertices.count;
        out_model->indices = indices.items;
        out_model->index_count = (uint32_t)indices.count;
        vertices.items = NULL;
        indices.items = NULL;
    }

    henka_free(text);
    henka_obj_vec3_array_destroy(&positions);
    henka_obj_vec2_array_destroy(&texcoords);
    henka_obj_vec3_array_destroy(&normals);
    henka_obj_vertex_array_destroy(&vertices);
    henka_obj_index_array_destroy(&indices);
    return result;
}

henka_result henka_model_data_load_obj(const char* path, henka_model_data* out_model)
{
    char* source;
    henka_result result;

    if (path == NULL || out_model == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    source = henka_read_binary_text_file(path);
    if (source == NULL)
    {
        HENKA_LOG_ERROR("Unable to open OBJ model '%s'", path);
        return HENKA_ERROR_PLATFORM;
    }

    result = henka_model_data_load_obj_from_memory(source, path, out_model);
    henka_free(source);
    return result;
}

void henka_model_data_destroy(henka_model_data* model)
{
    if (model == NULL)
    {
        return;
    }

    henka_free(model->vertices);
    henka_free(model->indices);
    model->vertices = NULL;
    model->indices = NULL;
    model->vertex_count = 0U;
    model->index_count = 0U;
}

henka_result henka_mesh_create_from_model_data(henka_engine* engine, const henka_model_data* model, henka_mesh** out_mesh)
{
    if (engine == NULL || model == NULL || out_mesh == NULL ||
        model->vertices == NULL || model->indices == NULL ||
        model->vertex_count == 0U || model->index_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_renderer_create_mesh_from_data(
        engine->renderer,
        (const henka_vertex*)model->vertices,
        (int)model->vertex_count,
        (const unsigned int*)model->indices,
        (int)model->index_count,
        HENKA_MESH_PRIMITIVE_TRIANGLES,
        out_mesh);
}

henka_result henka_mesh_create_from_obj(henka_engine* engine, const char* path, henka_mesh** out_mesh)
{
    henka_model_data model;
    henka_result result;

    memset(&model, 0, sizeof(model));
    result = henka_model_data_load_obj(path, &model);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_mesh_create_from_model_data(engine, &model, out_mesh);
    henka_model_data_destroy(&model);
    return result;
}
