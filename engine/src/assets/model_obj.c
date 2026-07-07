#include "henka_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>
#include <henka/model.h>

enum
{
    HENKA_OBJ_MAX_FACE_VERTICES = 128,
    HENKA_OBJ_MAX_LINE_TOKENS = HENKA_OBJ_MAX_FACE_VERTICES + 1
};

typedef struct henka_obj_index
{
    int position_index;
    int uv_index;
    int normal_index;
} henka_obj_index;

typedef struct henka_obj_face
{
    henka_obj_index indices[HENKA_OBJ_MAX_FACE_VERTICES];
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

typedef struct henka_obj_parse_context
{
    const char* label;
    int line_number;
    const char* error_message;
} henka_obj_parse_context;

static const size_t g_henka_obj_max_line_length = 4096U;

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

static void henka_obj_set_error(henka_obj_parse_context* context, const char* message)
{
    if (context != NULL && context->error_message == NULL)
    {
        context->error_message = message;
    }
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

static bool henka_obj_is_ignored_statement(const char* keyword)
{
    return strcmp(keyword, "o") == 0 ||
        strcmp(keyword, "g") == 0 ||
        strcmp(keyword, "s") == 0 ||
        strcmp(keyword, "mtllib") == 0 ||
        strcmp(keyword, "usemtl") == 0;
}

static int henka_obj_split_tokens(char* line, char** tokens, int max_tokens)
{
    char* cursor;
    int count;

    count = 0;
    cursor = line;
    while (*cursor != '\0')
    {
        while (*cursor == ' ' || *cursor == '\t')
        {
            cursor += 1;
        }

        if (*cursor == '\0')
        {
            break;
        }

        if (count >= max_tokens)
        {
            return -1;
        }

        tokens[count] = cursor;
        count += 1;

        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t')
        {
            cursor += 1;
        }

        if (*cursor == '\0')
        {
            break;
        }

        *cursor = '\0';
        cursor += 1;
    }

    return count;
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

static bool henka_parse_face_sub_index(const char* token, int* out_index, henka_obj_parse_context* context)
{
    char* end;
    long parsed_value;

    parsed_value = strtol(token, &end, 10);
    if (end == token || *end != '\0' || parsed_value == 0L)
    {
        henka_obj_set_error(context, "face indices must be non-zero integers");
        return false;
    }

    if (parsed_value < (long)INT_MIN || parsed_value > (long)INT_MAX)
    {
        henka_obj_set_error(context, "face index is outside the supported integer range");
        return false;
    }

    if (parsed_value > 0L)
    {
        *out_index = (int)(parsed_value - 1L);
    }
    else
    {
        *out_index = (int)parsed_value;
    }
    return true;
}
static bool henka_parse_face_index(const char* token, henka_obj_index* out_index, henka_obj_parse_context* context)
{
    char local_copy[128];
    char* first_separator;
    char* second_separator;
    char* normal_token;
    char* uv_token;

    if (token == NULL || out_index == NULL)
    {
        return false;
    }

    if (strcpy_s(local_copy, sizeof(local_copy), token) != 0)
    {
        henka_obj_set_error(context, "face token is too long");
        return false;
    }

    out_index->position_index = -1;
    out_index->uv_index = -1;
    out_index->normal_index = -1;

    first_separator = strchr(local_copy, '/');
    if (first_separator == NULL)
    {
        return henka_parse_face_sub_index(local_copy, &out_index->position_index, context);
    }

    *first_separator = '\0';
    uv_token = first_separator + 1;
    if (!henka_parse_face_sub_index(local_copy, &out_index->position_index, context))
    {
        return false;
    }

    second_separator = strchr(uv_token, '/');
    if (second_separator == NULL)
    {
        if (*uv_token == '\0')
        {
            return true;
        }

        return henka_parse_face_sub_index(uv_token, &out_index->uv_index, context);
    }

    *second_separator = '\0';
    normal_token = second_separator + 1;

    if (*uv_token != '\0' && !henka_parse_face_sub_index(uv_token, &out_index->uv_index, context))
    {
        return false;
    }

    if (*normal_token == '\0')
    {
        henka_obj_set_error(context, "face normal index is missing after '//'");
        return false;
    }

    return henka_parse_face_sub_index(normal_token, &out_index->normal_index, context);
}

static henka_result henka_parse_vertex_tokens(char** tokens, int token_count, henka_obj_vec3_array* positions, henka_obj_parse_context* context)
{
    henka_vec3 value;

    if (token_count < 4)
    {
        henka_obj_set_error(context, "vertex positions require three numeric components");
        return HENKA_ERROR_UNKNOWN;
    }

    if (!henka_parse_float_token(tokens[1], &value.x) ||
        !henka_parse_float_token(tokens[2], &value.y) ||
        !henka_parse_float_token(tokens[3], &value.z))
    {
        henka_obj_set_error(context, "vertex positions require valid numeric components");
        return HENKA_ERROR_UNKNOWN;
    }

    return henka_obj_vec3_array_push(positions, value);
}

static henka_result henka_parse_uv_tokens(char** tokens, int token_count, henka_obj_vec2_array* texcoords, henka_obj_parse_context* context)
{
    henka_vec2 value;

    if (token_count < 3)
    {
        henka_obj_set_error(context, "texture coordinates require two numeric components");
        return HENKA_ERROR_UNKNOWN;
    }

    if (!henka_parse_float_token(tokens[1], &value.x) ||
        !henka_parse_float_token(tokens[2], &value.y))
    {
        henka_obj_set_error(context, "texture coordinates require valid numeric components");
        return HENKA_ERROR_UNKNOWN;
    }

    return henka_obj_vec2_array_push(texcoords, value);
}

static henka_result henka_parse_normal_tokens(char** tokens, int token_count, henka_obj_vec3_array* normals, henka_obj_parse_context* context)
{
    henka_vec3 value;

    if (token_count < 4)
    {
        henka_obj_set_error(context, "vertex normals require three numeric components");
        return HENKA_ERROR_UNKNOWN;
    }

    if (!henka_parse_float_token(tokens[1], &value.x) ||
        !henka_parse_float_token(tokens[2], &value.y) ||
        !henka_parse_float_token(tokens[3], &value.z))
    {
        henka_obj_set_error(context, "vertex normals require valid numeric components");
        return HENKA_ERROR_UNKNOWN;
    }

    return henka_obj_vec3_array_push(normals, value);
}

static henka_result henka_parse_face_tokens(char** tokens, int token_count, henka_obj_face* out_face, henka_obj_parse_context* context)
{
    int token_index;

    if (token_count < 4)
    {
        henka_obj_set_error(context, "faces require at least three vertices");
        return HENKA_ERROR_UNKNOWN;
    }

    if (token_count > HENKA_OBJ_MAX_LINE_TOKENS)
    {
        henka_obj_set_error(context, "face has more vertices than the current safe OBJ limit");
        return HENKA_ERROR_UNKNOWN;
    }

    out_face->count = 0;
    for (token_index = 1; token_index < token_count; ++token_index)
    {
        if (!henka_parse_face_index(tokens[token_index], &out_face->indices[out_face->count], context))
        {
            return HENKA_ERROR_UNKNOWN;
        }

        out_face->count += 1;
    }

    return HENKA_SUCCESS;
}
static henka_result henka_resolve_face_index(int parsed_index, size_t count, henka_obj_parse_context* context, size_t* out_index)
{
    size_t relative_from_end;

    if (out_index == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (count == 0U)
    {
        henka_obj_set_error(context, "face index references data that does not exist");
        return HENKA_ERROR_UNKNOWN;
    }

    if (parsed_index >= 0)
    {
        if ((size_t)parsed_index >= count)
        {
            henka_obj_set_error(context, "face index references data that does not exist");
            return HENKA_ERROR_UNKNOWN;
        }

        *out_index = (size_t)parsed_index;
        return HENKA_SUCCESS;
    }

    relative_from_end = (size_t)(-(parsed_index + 1)) + 1U;
    if (relative_from_end > count)
    {
        henka_obj_set_error(context, "negative face index references data that does not exist");
        return HENKA_ERROR_UNKNOWN;
    }

    *out_index = count - relative_from_end;
    return HENKA_SUCCESS;
}

static henka_vec3 henka_obj_triangle_cross(
    const henka_model_vertex* a,
    const henka_model_vertex* b,
    const henka_model_vertex* c)
{
    henka_vec3 edge_a;
    henka_vec3 edge_b;

    edge_a = henka_vec3_subtract(b->position, a->position);
    edge_b = henka_vec3_subtract(c->position, a->position);
    return henka_vec3_cross(edge_a, edge_b);
}

static bool henka_obj_cross_is_degenerate(henka_vec3 cross_value)
{
    const float epsilon = 0.0000001f;
    const float length_squared =
        cross_value.x * cross_value.x +
        cross_value.y * cross_value.y +
        cross_value.z * cross_value.z;
    return length_squared <= epsilon;
}

static bool henka_obj_triangle_is_degenerate(
    const henka_model_vertex* a,
    const henka_model_vertex* b,
    const henka_model_vertex* c)
{
    return henka_obj_cross_is_degenerate(henka_obj_triangle_cross(a, b, c));
}
static char* henka_copy_line_range(const char* start, size_t length)
{
    char* line;

    line = henka_malloc(length + 1U);
    if (line == NULL)
    {
        return NULL;
    }

    memcpy(line, start, length);
    line[length] = '\0';
    return line;
}

static henka_result henka_get_next_line(const char** cursor, char** out_line, henka_obj_parse_context* context)
{
    const char* start;
    size_t length;

    if (cursor == NULL || *cursor == NULL || out_line == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (**cursor == '\0')
    {
        *out_line = NULL;
        return HENKA_SUCCESS;
    }

    start = *cursor;
    length = 0U;
    while ((*cursor)[length] != '\0' && (*cursor)[length] != '\n')
    {
        length += 1U;
    }

    if (length > g_henka_obj_max_line_length)
    {
        henka_obj_set_error(context, "a line is longer than the current safe OBJ limit");
        return HENKA_ERROR_UNKNOWN;
    }

    *out_line = henka_copy_line_range(start, length);
    if (*out_line == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    *cursor += length;
    if (**cursor == '\n')
    {
        *cursor += 1;
    }

    return HENKA_SUCCESS;
}

static henka_result henka_build_face_vertices(
    const henka_obj_face* face,
    const henka_obj_vec3_array* positions,
    const henka_obj_vec2_array* texcoords,
    const henka_obj_vec3_array* normals,
    henka_obj_parse_context* context,
    henka_model_vertex out_vertices[HENKA_OBJ_MAX_FACE_VERTICES])
{
    henka_vec3 computed_normal;
    bool has_computed_normal;
    bool needs_computed_normal;
    int index;

    has_computed_normal = false;
    needs_computed_normal = false;
    for (index = 0; index < face->count; ++index)
    {
        const henka_obj_index* obj_index;
        size_t normal_index;
        size_t position_index;
        size_t uv_index;

        obj_index = &face->indices[index];
        if (henka_resolve_face_index(obj_index->position_index, positions->count, context, &position_index) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_UNKNOWN;
        }

        out_vertices[index].position = positions->items[position_index];
        out_vertices[index].uv = (henka_vec2){0.0f, 0.0f};
        out_vertices[index].normal = (henka_vec3){0.0f, 1.0f, 0.0f};

        if (obj_index->uv_index != -1)
        {
            if (henka_resolve_face_index(obj_index->uv_index, texcoords->count, context, &uv_index) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_UNKNOWN;
            }

            out_vertices[index].uv = texcoords->items[uv_index];
        }

        if (obj_index->normal_index != -1)
        {
            if (henka_resolve_face_index(obj_index->normal_index, normals->count, context, &normal_index) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_UNKNOWN;
            }

            out_vertices[index].normal = normals->items[normal_index];
        }
        else
        {
            needs_computed_normal = true;
        }
    }

    if (needs_computed_normal)
    {
        int triangle_index;

        computed_normal = (henka_vec3){0.0f, 1.0f, 0.0f};
        for (triangle_index = 1; triangle_index + 1 < face->count; ++triangle_index)
        {
            const henka_vec3 candidate_cross = henka_obj_triangle_cross(
                &out_vertices[0],
                &out_vertices[triangle_index],
                &out_vertices[triangle_index + 1]);

            if (!henka_obj_cross_is_degenerate(candidate_cross))
            {
                computed_normal = henka_vec3_normalize(candidate_cross);
                has_computed_normal = true;
                break;
            }
        }

        if (!has_computed_normal)
        {
            henka_obj_set_error(context, "face is degenerate and cannot produce a normal");
            return HENKA_ERROR_UNKNOWN;
        }

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
    henka_obj_parse_context* context,
    henka_obj_vertex_array* vertices,
    henka_obj_index_array* indices)
{
    henka_model_vertex face_vertices[HENKA_OBJ_MAX_FACE_VERTICES];
    henka_result result;
    int triangle_index;

    result = henka_build_face_vertices(face, positions, texcoords, normals, context, face_vertices);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    for (triangle_index = 1; triangle_index + 1 < face->count; ++triangle_index)
    {
        if (henka_obj_triangle_is_degenerate(
                &face_vertices[0],
                &face_vertices[triangle_index],
                &face_vertices[triangle_index + 1]))
        {
            henka_obj_set_error(context, "face triangulation produced a degenerate triangle");
            return HENKA_ERROR_UNKNOWN;
        }

        result = henka_append_triangle(
            vertices,
            indices,
            &face_vertices[0],
            &face_vertices[triangle_index],
            &face_vertices[triangle_index + 1]);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }

    return HENKA_SUCCESS;
}
henka_result henka_model_data_load_obj_from_memory(const char* source, const char* label, henka_model_data* out_model)
{
    const char* cursor;
    char* inline_comment;
    char* line;
    char* trimmed_line;
    char* tokens[HENKA_OBJ_MAX_LINE_TOKENS];
    int token_count;
    henka_obj_vec2_array texcoords;
    henka_obj_vec3_array normals;
    henka_obj_vec3_array positions;
    henka_obj_vertex_array vertices;
    henka_obj_index_array indices;
    henka_obj_face face;
    henka_obj_parse_context context;
    henka_result result;
    bool saw_face;
    bool saw_vertex_statement;

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

    context.label = label != NULL ? label : "<memory>";
    context.line_number = 0;
    context.error_message = NULL;
    cursor = source;
    saw_face = false;
    saw_vertex_statement = false;
    result = HENKA_SUCCESS;

    while (*cursor != '\0')
    {
        result = henka_get_next_line(&cursor, &line, &context);
        if (result != HENKA_SUCCESS)
        {
            break;
        }

        context.line_number += 1;
        trimmed_line = henka_trim_whitespace(line);
        inline_comment = strchr(trimmed_line, '#');
        if (inline_comment != NULL)
        {
            *inline_comment = '\0';
            trimmed_line = henka_trim_whitespace(trimmed_line);
        }

        if (trimmed_line[0] == '\0')
        {
            henka_free(line);
            continue;
        }

        token_count = henka_obj_split_tokens(trimmed_line, tokens, (int)(sizeof(tokens) / sizeof(tokens[0])));
        if (token_count < 0)
        {
            henka_obj_set_error(&context, "a line contains too many tokens");
            result = HENKA_ERROR_UNKNOWN;
        }
        else if (token_count == 0)
        {
            result = HENKA_SUCCESS;
        }
        else if (strcmp(tokens[0], "v") == 0)
        {
            saw_vertex_statement = true;
            result = henka_parse_vertex_tokens(tokens, token_count, &positions, &context);
        }
        else if (strcmp(tokens[0], "vt") == 0)
        {
            result = henka_parse_uv_tokens(tokens, token_count, &texcoords, &context);
        }
        else if (strcmp(tokens[0], "vn") == 0)
        {
            result = henka_parse_normal_tokens(tokens, token_count, &normals, &context);
        }
        else if (strcmp(tokens[0], "f") == 0)
        {
            saw_face = true;
            result = henka_parse_face_tokens(tokens, token_count, &face, &context);
            if (result == HENKA_SUCCESS)
            {
                result = henka_emit_face(&face, &positions, &texcoords, &normals, &context, &vertices, &indices);
            }
        }
        else if (!henka_obj_is_ignored_statement(tokens[0]))
        {
            henka_obj_set_error(&context, "this OBJ statement is not supported yet");
            result = HENKA_ERROR_UNKNOWN;
        }

        henka_free(line);

        if (result != HENKA_SUCCESS)
        {
            if (context.error_message != NULL)
            {
                HENKA_LOG_ERROR("OBJ '%s' line %d: %s", context.label, context.line_number, context.error_message);
            }
            else
            {
                HENKA_LOG_ERROR("OBJ '%s' line %d: unable to parse this line", context.label, context.line_number);
            }
            break;
        }
    }

    if (result == HENKA_SUCCESS && !saw_vertex_statement && !saw_face)
    {
        HENKA_LOG_ERROR("OBJ '%s' is empty or only contains comments", context.label);
        result = HENKA_ERROR_UNKNOWN;
    }
    else if (result == HENKA_SUCCESS && !saw_face)
    {
        HENKA_LOG_ERROR("OBJ '%s' contains vertex data but no faces", context.label);
        result = HENKA_ERROR_UNKNOWN;
    }
    else if (result == HENKA_SUCCESS && vertices.count == 0U)
    {
        HENKA_LOG_ERROR("OBJ '%s' did not produce any renderable triangles", context.label);
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
