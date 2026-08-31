#include "henka_internal.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>
#include <henka/authoring_mesh.h>
#include <henka/model.h>

#include "../core/checked.h"

enum
{
    HENKA_OBJ_MAX_FACE_VERTICES = 128,
    HENKA_OBJ_MAX_LINE_TOKENS = HENKA_OBJ_MAX_FACE_VERTICES + 1
};

/*
 * Parsed positive indices are converted to zero-based values and therefore
 * cannot equal INT_MAX. Parsed negative indices remain negative, so INT_MAX is
 * a distinct marker for an omitted optional UV or normal index.
 */
#define HENKA_OBJ_INDEX_MISSING INT_MAX

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
    size_t allocation_size;
    char* buffer;
    size_t bytes_read;
    FILE* file;
    long file_length;
    size_t length;

    if (path == NULL)
    {
        return NULL;
    }

    file = NULL;
    if (fopen_s(&file, path, "rb") != 0 || file == NULL)
    {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0)
    {
        fclose(file);
        return NULL;
    }

    file_length = ftell(file);
    if (file_length < 0L || (size_t)file_length > HENKA_MAX_OBJ_SOURCE_BYTES)
    {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0L, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }

    length = (size_t)file_length;
    if (!henka_checked_size_add(length, 1U, &allocation_size))
    {
        fclose(file);
        return NULL;
    }

    buffer = henka_malloc(allocation_size);
    if (buffer == NULL)
    {
        fclose(file);
        return NULL;
    }

    bytes_read = fread(buffer, 1U, length, file);
    if (bytes_read != length || ferror(file))
    {
        fclose(file);
        henka_free(buffer);
        return NULL;
    }

    if (fclose(file) != 0)
    {
        henka_free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
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

static henka_result henka_obj_array_reserve(
    void** items,
    size_t element_size,
    size_t* capacity,
    size_t required,
    size_t initial_capacity,
    size_t maximum)
{
    size_t allocation_size;
    size_t next_capacity;
    void* resized;

    if (items == NULL || capacity == NULL || element_size == 0U ||
        !henka_checked_capacity(*capacity, required, initial_capacity, maximum, &next_capacity) ||
        !henka_checked_size_multiply(element_size, next_capacity, &allocation_size))
    {
        return HENKA_ERROR_UNKNOWN;
    }

    if (next_capacity == *capacity)
    {
        return HENKA_SUCCESS;
    }

    resized = henka_realloc(*items, allocation_size);
    if (resized == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    *items = resized;
    *capacity = next_capacity;
    return HENKA_SUCCESS;
}

static henka_result henka_obj_vec2_array_push(henka_obj_vec2_array* array, henka_vec2 value)
{
    size_t required;
    henka_result result;

    if (array == NULL || !henka_checked_size_add(array->count, 1U, &required) ||
        required > HENKA_MAX_OBJ_RECORDS)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    result = henka_obj_array_reserve(
        (void**)&array->items,
        sizeof(*array->items),
        &array->capacity,
        required,
        16U,
        HENKA_MAX_OBJ_RECORDS);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    array->items[array->count] = value;
    ++array->count;
    return HENKA_SUCCESS;
}

static henka_result henka_obj_vec3_array_push(henka_obj_vec3_array* array, henka_vec3 value)
{
    size_t required;
    henka_result result;

    if (array == NULL || !henka_checked_size_add(array->count, 1U, &required) ||
        required > HENKA_MAX_OBJ_RECORDS)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    result = henka_obj_array_reserve(
        (void**)&array->items,
        sizeof(*array->items),
        &array->capacity,
        required,
        16U,
        HENKA_MAX_OBJ_RECORDS);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    array->items[array->count] = value;
    ++array->count;
    return HENKA_SUCCESS;
}

static henka_result henka_obj_vertex_array_push(
    henka_obj_vertex_array* array,
    henka_model_vertex value,
    uint32_t* out_index)
{
    size_t required;
    henka_result result;
    uint32_t index;

    if (array == NULL || !henka_checked_size_add(array->count, 1U, &required) ||
        required > HENKA_MAX_OBJ_OUTPUT_ELEMENTS ||
        !henka_checked_size_to_u32(array->count, &index))
    {
        return HENKA_ERROR_UNKNOWN;
    }

    result = henka_obj_array_reserve(
        (void**)&array->items,
        sizeof(*array->items),
        &array->capacity,
        required,
        24U,
        HENKA_MAX_OBJ_OUTPUT_ELEMENTS);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    array->items[array->count] = value;
    if (out_index != NULL)
    {
        *out_index = index;
    }

    ++array->count;
    return HENKA_SUCCESS;
}

static henka_result henka_obj_index_array_push(henka_obj_index_array* array, uint32_t value)
{
    size_t required;
    henka_result result;

    if (array == NULL || !henka_checked_size_add(array->count, 1U, &required) ||
        required > HENKA_MAX_OBJ_OUTPUT_ELEMENTS)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    result = henka_obj_array_reserve(
        (void**)&array->items,
        sizeof(*array->items),
        &array->capacity,
        required,
        36U,
        HENKA_MAX_OBJ_OUTPUT_ELEMENTS);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    array->items[array->count] = value;
    ++array->count;
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

    errno = 0;
    value = strtof(token, &end);
    if (errno == ERANGE || end == token || *end != '\0' || !isfinite(value))
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

    if (token == NULL || out_index == NULL)
    {
        henka_obj_set_error(context, "face index input is invalid");
        return false;
    }

    errno = 0;
    parsed_value = strtol(token, &end, 10);
    if (errno == ERANGE || end == token || *end != '\0' || parsed_value == 0L)
    {
        henka_obj_set_error(context, "face indices must be non-zero integers in the supported range");
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
    out_index->uv_index = HENKA_OBJ_INDEX_MISSING;
    out_index->normal_index = HENKA_OBJ_INDEX_MISSING;

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
        out_vertices[index].color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};

        if (obj_index->uv_index != HENKA_OBJ_INDEX_MISSING)
        {
            if (henka_resolve_face_index(obj_index->uv_index, texcoords->count, context, &uv_index) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_UNKNOWN;
            }

            out_vertices[index].uv = texcoords->items[uv_index];
        }

        if (obj_index->normal_index != HENKA_OBJ_INDEX_MISSING)
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
    henka_obj_parse_context context;
    const char* cursor;
    henka_obj_face face;
    henka_obj_index_array indices;
    char* inline_comment;
    char* line;
    size_t line_count;
    henka_obj_vec3_array normals;
    henka_obj_vec3_array positions;
    henka_result result;
    bool saw_face;
    bool saw_vertex_statement;
    size_t source_length;
    henka_obj_vec2_array texcoords;
    int token_count;
    char* tokens[HENKA_OBJ_MAX_LINE_TOKENS];
    char* trimmed_line;
    henka_obj_vertex_array vertices;
    uint32_t index_count = 0U;
    uint32_t vertex_count = 0U;
    henka_model_data candidate;

    if (source == NULL || out_model == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(&candidate, 0, sizeof(candidate));

    source_length = 0U;
    while (source[source_length] != '\0')
    {
        if (source_length >= HENKA_MAX_OBJ_SOURCE_BYTES)
        {
            HENKA_LOG_ERROR("OBJ '%s' exceeds the supported source-size limit", label != NULL ? label : "<memory>");
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        ++source_length;
    }

    memset(&positions, 0, sizeof(positions));
    memset(&texcoords, 0, sizeof(texcoords));
    memset(&normals, 0, sizeof(normals));
    memset(&vertices, 0, sizeof(vertices));
    memset(&indices, 0, sizeof(indices));

    context.label = label != NULL ? label : "<memory>";
    context.line_number = 0;
    context.error_message = NULL;
    cursor = source;
    line_count = 0U;
    saw_face = false;
    saw_vertex_statement = false;
    result = HENKA_SUCCESS;

    while (*cursor != '\0')
    {
        if (line_count >= HENKA_MAX_OBJ_RECORDS)
        {
            henka_obj_set_error(&context, "the file contains too many lines");
            result = HENKA_ERROR_UNKNOWN;
            break;
        }

        result = henka_get_next_line(&cursor, &line, &context);
        if (result != HENKA_SUCCESS)
        {
            break;
        }

        ++line_count;
        context.line_number = (int)line_count;
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
            henka_obj_set_error(&context, "this OBJ statement is not supported");
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
    else if (result == HENKA_SUCCESS &&
        (!henka_checked_size_to_u32(vertices.count, &vertex_count) ||
         !henka_checked_size_to_u32(indices.count, &index_count)))
    {
        HENKA_LOG_ERROR("OBJ '%s' exceeds renderer count limits", context.label);
        result = HENKA_ERROR_UNKNOWN;
    }

    if (result == HENKA_SUCCESS)
    {
        candidate.vertices = vertices.items;
        candidate.vertex_count = vertex_count;
        candidate.indices = indices.items;
        candidate.index_count = index_count;
        vertices.items = NULL;
        indices.items = NULL;
        henka_model_data_destroy(out_model);
        *out_model = candidate;
        memset(&candidate, 0, sizeof(candidate));
    }

    henka_obj_vec3_array_destroy(&positions);
    henka_obj_vec2_array_destroy(&texcoords);
    henka_obj_vec3_array_destroy(&normals);
    henka_obj_vertex_array_destroy(&vertices);
    henka_obj_index_array_destroy(&indices);
    henka_model_data_destroy(&candidate);
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
    henka_free(model->material_source.name);
    henka_free(model->material_source.base_color_uri);
    henka_free(model->material_source.normal_uri);
    henka_free(model->material_source.metallic_roughness_uri);
    henka_free(model->material_source.occlusion_uri);
    henka_free(model->material_source.emissive_uri);
    henka_free(model->material_source.transmission_uri);
    henka_free(model->material_source.thickness_uri);
    henka_free(model->material_source.base_color_embedded_data);
    henka_free(model->material_source.normal_embedded_data);
    henka_free(model->material_source.metallic_roughness_embedded_data);
    henka_free(model->material_source.occlusion_embedded_data);
    henka_free(model->material_source.emissive_embedded_data);
    henka_free(model->material_source.transmission_embedded_data);
    henka_free(model->material_source.thickness_embedded_data);
    memset(&model->material_source, 0, sizeof(model->material_source));
    model->has_material = false;
    model->vertices = NULL;
    model->indices = NULL;
    model->vertex_count = 0U;
    model->index_count = 0U;
}

henka_result henka_mesh_create_from_model_data(henka_engine* engine, const henka_model_data* model, henka_mesh** out_mesh)
{
    henka_vertex* vertices;
    int index_count;
    int vertex_count;
    uint32_t vertex_index;

    if (engine == NULL || model == NULL || out_mesh == NULL ||
        model->vertices == NULL || model->indices == NULL ||
        model->vertex_count == 0U || model->index_count == 0U ||
        model->vertex_count > HENKA_MAX_MESH_ELEMENTS ||
        model->index_count > HENKA_MAX_MESH_ELEMENTS ||
        !henka_checked_size_to_int((size_t)model->vertex_count, &vertex_count) ||
        !henka_checked_size_to_int((size_t)model->index_count, &index_count))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    vertices = henka_calloc((size_t)model->vertex_count, sizeof(*vertices));
    if (vertices == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (vertex_index = 0U; vertex_index < model->vertex_count; ++vertex_index)
    {
        vertices[vertex_index].position = model->vertices[vertex_index].position;
        vertices[vertex_index].normal = model->vertices[vertex_index].normal;
        vertices[vertex_index].uv = model->vertices[vertex_index].uv;
        vertices[vertex_index].uv1 = model->vertices[vertex_index].uv1;
        vertices[vertex_index].color = model->vertices[vertex_index].color;
        vertices[vertex_index].color_valid = true;
        vertices[vertex_index].tangent = model->vertices[vertex_index].tangent;
        vertices[vertex_index].tangent_valid = model->vertices[vertex_index].tangent_valid;
        vertices[vertex_index].material_region = model->vertices[vertex_index].material_region;
    }

    *out_mesh = NULL;
    {
        henka_result result = henka_renderer_create_mesh_from_data(
            engine->renderer,
            vertices,
            vertex_count,
            (const unsigned int*)model->indices,
            index_count,
            HENKA_MESH_PRIMITIVE_TRIANGLES,
            out_mesh);
        henka_free(vertices);
        return result;
    }
}

static void henka_authoring_make_renderer_vertex(
    const henka_authoring_vertex* source,
    henka_vertex* out_vertex)
{
    if (source == NULL || out_vertex == NULL)
    {
        return;
    }
    *out_vertex = (henka_vertex){
        source->position,
        {0.0f, 1.0f, 0.0f},
        source->uv,
        {0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        true,
        {1.0f, 0.0f, 0.0f, 1.0f},
        true,
        source->material_region};
}

static bool henka_authoring_vertex_is_face_referenced(
    const henka_authoring_mesh* source,
    henka_authoring_vertex_id vertex_id)
{
    henka_authoring_mesh_desc desc;
    size_t face_slot;

    if (source == NULL)
    {
        return false;
    }
    desc = henka_authoring_mesh_get_desc(source);
    for (face_slot = 0U; face_slot < desc.max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face;
        size_t corner;

        if (henka_authoring_mesh_get_face_id_at(source, face_slot, &face_id) != HENKA_SUCCESS)
        {
            continue;
        }
        face = henka_authoring_mesh_get_face(source, face_id);
        if (face == NULL)
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
    }
    return false;
}

static henka_result henka_mesh_create_from_authoring_loose_source(
    henka_engine* engine,
    const henka_authoring_mesh* source,
    henka_authoring_mesh_counts counts,
    henka_mesh_primitive primitive,
    henka_mesh** out_mesh)
{
    henka_authoring_mesh_desc desc;
    henka_vertex* vertices = NULL;
    unsigned int* indices = NULL;
    size_t vertex_count;
    size_t index_count;
    size_t vertex_slot;
    size_t edge_slot;
    size_t index;
    size_t loose_edge_count = 0U;
    int renderer_vertex_count;
    int renderer_index_count;
    henka_result result;

    if (engine == NULL || source == NULL || out_mesh == NULL || counts.vertices == 0U ||
        (primitive != HENKA_MESH_PRIMITIVE_LINES && primitive != HENKA_MESH_PRIMITIVE_POINTS))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    desc = henka_authoring_mesh_get_desc(source);
    for (edge_slot = 0U; edge_slot < desc.max_edges; ++edge_slot)
    {
        henka_authoring_edge_id edge_id;
        if (henka_authoring_mesh_get_edge_id_at(source, edge_slot, &edge_id) == HENKA_SUCCESS &&
            henka_authoring_mesh_get_edge_face_count(source, edge_id) == 0U)
        {
            ++loose_edge_count;
        }
    }
    if (primitive == HENKA_MESH_PRIMITIVE_LINES)
    {
        if (loose_edge_count == 0U || !henka_checked_size_multiply(loose_edge_count, 2U, &vertex_count))
        {
            return HENKA_ERROR_LIMIT;
        }
        index_count = vertex_count;
    }
    else
    {
        vertex_count = 0U;
        for (vertex_slot = 0U; vertex_slot < desc.max_vertices; ++vertex_slot)
        {
            henka_authoring_vertex_id vertex_id;
            if (henka_authoring_mesh_get_vertex_id_at(source, vertex_slot, &vertex_id) == HENKA_SUCCESS &&
                henka_authoring_mesh_get_vertex_edge_count(source, vertex_id) == 0U &&
                !henka_authoring_vertex_is_face_referenced(source, vertex_id))
            {
                ++vertex_count;
            }
        }
        index_count = vertex_count;
    }
    if (vertex_count == 0U || index_count == 0U ||
        vertex_count > HENKA_MAX_MESH_ELEMENTS ||
        index_count > HENKA_MAX_MESH_ELEMENTS ||
        !henka_checked_size_to_int(vertex_count, &renderer_vertex_count) ||
        !henka_checked_size_to_int(index_count, &renderer_index_count))
    {
        return HENKA_ERROR_LIMIT;
    }
    vertices = henka_calloc(vertex_count, sizeof(*vertices));
    indices = henka_calloc(index_count, sizeof(*indices));
    if (vertices == NULL || indices == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    index = 0U;
    if (primitive == HENKA_MESH_PRIMITIVE_LINES)
    {
        size_t output_vertex = 0U;
        for (edge_slot = 0U; edge_slot < desc.max_edges; ++edge_slot)
        {
            henka_authoring_edge_id edge_id;
            const henka_authoring_edge* edge;
            const henka_authoring_vertex* first;
            const henka_authoring_vertex* second;
            if (henka_authoring_mesh_get_edge_id_at(source, edge_slot, &edge_id) != HENKA_SUCCESS)
            {
                continue;
            }
            if (henka_authoring_mesh_get_edge_face_count(source, edge_id) != 0U)
            {
                continue;
            }
            edge = henka_authoring_mesh_get_edge(source, edge_id);
            first = edge == NULL ? NULL : henka_authoring_mesh_get_vertex(source, edge->vertices[0]);
            second = edge == NULL ? NULL : henka_authoring_mesh_get_vertex(source, edge->vertices[1]);
            if (edge == NULL || first == NULL || second == NULL || output_vertex + 1U >= vertex_count)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            henka_authoring_make_renderer_vertex(first, &vertices[output_vertex]);
            henka_authoring_make_renderer_vertex(second, &vertices[output_vertex + 1U]);
            indices[index++] = (unsigned int)output_vertex;
            indices[index++] = (unsigned int)(output_vertex + 1U);
            output_vertex += 2U;
        }
        if (output_vertex != vertex_count || index != index_count)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }
    else
    {
        size_t output_vertex = 0U;
        for (vertex_slot = 0U; vertex_slot < desc.max_vertices; ++vertex_slot)
        {
            henka_authoring_vertex_id vertex_id;
            const henka_authoring_vertex* vertex;
            if (henka_authoring_mesh_get_vertex_id_at(source, vertex_slot, &vertex_id) != HENKA_SUCCESS ||
                henka_authoring_mesh_get_vertex_edge_count(source, vertex_id) != 0U ||
                henka_authoring_vertex_is_face_referenced(source, vertex_id))
            {
                continue;
            }
            vertex = henka_authoring_mesh_get_vertex(source, vertex_id);
            if (vertex == NULL || output_vertex >= vertex_count)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            henka_authoring_make_renderer_vertex(vertex, &vertices[output_vertex]);
            indices[index++] = (unsigned int)output_vertex++;
        }
        if (output_vertex != vertex_count || index != index_count)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }
    result = henka_renderer_create_mesh_from_data(
        engine->renderer,
        vertices,
        renderer_vertex_count,
        indices,
        renderer_index_count,
        primitive,
        out_mesh);

cleanup:
    henka_free(indices);
    henka_free(vertices);
    return result;
}

static henka_result henka_mesh_create_from_authoring_surface_source(
    henka_engine* engine,
    const henka_authoring_mesh* source,
    henka_mesh** out_mesh)
{
    henka_authoring_render_vertex* render_vertices = NULL;
    uint32_t* render_indices = NULL;
    henka_model_vertex* model_vertices = NULL;
    uint32_t* model_indices = NULL;
    henka_authoring_render_data render;
    henka_model_data model;
    size_t vertex_count = 0U;
    size_t index_count = 0U;
    size_t face_slot;
    size_t vertex_index;
    size_t index;
    henka_result result;
    henka_authoring_mesh_counts counts;

    if (out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (*out_mesh != NULL || engine == NULL || source == NULL || !henka_authoring_mesh_validate(source))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    counts = henka_authoring_mesh_get_counts(source);
    if (counts.faces == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* Enumerate active faces by deterministic physical storage order; logical
     * IDs are opaque handles and are not array indices. */
    for (face_slot = 0U; face_slot < henka_authoring_mesh_get_desc(source).max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face = henka_authoring_mesh_get_face_id_at(source, face_slot, &face_id) == HENKA_SUCCESS
            ? henka_authoring_mesh_get_face(source, face_id)
            : NULL;
        size_t face_indices;

        if (face == NULL)
        {
            continue;
        }
        if (face->corner_count < 3U ||
            face->corner_count > HENKA_AUTHORING_MESH_HARD_MAX_FACE_CORNERS ||
            face->corner_count > SIZE_MAX / 3U)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        face_indices = (face->corner_count - 2U) * 3U;
        if (vertex_count > SIZE_MAX - face->corner_count ||
            index_count > SIZE_MAX - face_indices)
        {
            return HENKA_ERROR_LIMIT;
        }
        vertex_count += face->corner_count;
        index_count += face_indices;
    }
    if (vertex_count == 0U || index_count == 0U ||
        vertex_count > HENKA_MAX_MESH_ELEMENTS ||
        index_count > HENKA_MAX_MESH_ELEMENTS ||
        vertex_count > UINT32_MAX || index_count > UINT32_MAX)
    {
        return HENKA_ERROR_LIMIT;
    }

    render_vertices = henka_calloc(vertex_count, sizeof(*render_vertices));
    render_indices = henka_calloc(index_count, sizeof(*render_indices));
    model_vertices = henka_calloc(vertex_count, sizeof(*model_vertices));
    model_indices = henka_calloc(index_count, sizeof(*model_indices));
    if (render_vertices == NULL || render_indices == NULL ||
        model_vertices == NULL || model_indices == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }

    render = (henka_authoring_render_data){
        render_vertices, vertex_count, 0U, render_indices, index_count, 0U};
    result = henka_authoring_mesh_evaluate(source, &render);
    if (result != HENKA_SUCCESS || render.vertex_count == 0U || render.index_count == 0U ||
        render.vertex_count > UINT32_MAX || render.index_count > UINT32_MAX)
    {
        if (result == HENKA_SUCCESS)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
        }
        goto cleanup;
    }
    for (vertex_index = 0U; vertex_index < render.vertex_count; ++vertex_index)
    {
        const henka_authoring_render_vertex* vertex = &render.vertices[vertex_index];
        if (!isfinite(vertex->position.x) || !isfinite(vertex->position.y) ||
            !isfinite(vertex->position.z) || !isfinite(vertex->normal.x) ||
            !isfinite(vertex->normal.y) || !isfinite(vertex->normal.z) ||
            !isfinite(vertex->tangent.x) || !isfinite(vertex->tangent.y) ||
            !isfinite(vertex->tangent.z) || !isfinite(vertex->tangent.w) ||
            !isfinite(vertex->uv.x) || !isfinite(vertex->uv.y))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        model_vertices[vertex_index].position = vertex->position;
        model_vertices[vertex_index].normal = vertex->normal;
        model_vertices[vertex_index].uv = vertex->uv;
        model_vertices[vertex_index].color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};
        model_vertices[vertex_index].tangent = vertex->tangent;
        /* The bounded authoring evaluator supplies stable non-authoritative
         * tangent metadata, not a UV-derived tangent basis.  Let the renderer
         * derive and orthogonalize the tangent so axis-aligned faces cannot
         * present that metadata as authoritative shading data. */
        model_vertices[vertex_index].tangent_valid = false;
        model_vertices[vertex_index].material_region = vertex->material_region;
    }
    for (index = 0U; index < render.index_count; ++index)
    {
        if (render.indices[index] >= render.vertex_count)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        model_indices[index] = render.indices[index];
    }
    model.vertices = model_vertices;
    model.vertex_count = (uint32_t)render.vertex_count;
    model.indices = model_indices;
    model.index_count = (uint32_t)render.index_count;
    {
        henka_mesh* candidate = NULL;
        result = henka_mesh_create_from_model_data(engine, &model, &candidate);
        if (result == HENKA_SUCCESS)
        {
            *out_mesh = candidate;
        }
        else if (candidate != NULL)
        {
            henka_mesh_destroy(candidate);
        }
    }

cleanup:
    henka_free(model_indices);
    henka_free(model_vertices);
    henka_free(render_indices);
    henka_free(render_vertices);
    return result;
}

static henka_result henka_mesh_compose_authoring_parts(
    henka_engine* engine,
    henka_mesh** parts,
    size_t part_count,
    henka_mesh** out_mesh)
{
    henka_mesh* composite;
    size_t index;

    if (engine == NULL || parts == NULL || out_mesh == NULL || *out_mesh != NULL ||
        part_count < 2U || part_count > HENKA_MESH_MAX_PRIMITIVE_PARTS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    composite = henka_calloc(1U, sizeof(*composite));
    if (composite == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    composite->renderer = engine->renderer;
    composite->part_count = (uint32_t)part_count;
    for (index = 0U; index < part_count; ++index)
    {
        if (parts[index] == NULL || parts[index]->part_count != 1U ||
            parts[index]->parts[0].backend_data == NULL)
        {
            henka_free(composite);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < part_count; ++index)
    {
        composite->parts[index] = parts[index]->parts[0];
        composite->parts[index].material_region_min = parts[index]->material_region_min;
        composite->parts[index].material_region_max = parts[index]->material_region_max;
        composite->parts[index].backend_data = parts[index]->backend_data;
        parts[index]->backend_data = NULL;
        parts[index]->parts[0].backend_data = NULL;
        henka_free(parts[index]);
        parts[index] = NULL;
    }
    composite->primitive = composite->parts[0].primitive;
    composite->vertex_count = composite->parts[0].vertex_count;
    composite->index_count = composite->parts[0].index_count;
    composite->material_region_min = composite->parts[0].material_region_min;
    composite->material_region_max = composite->parts[0].material_region_max;
    composite->backend_data = composite->parts[0].backend_data;
    for (index = 1U; index < part_count; ++index)
    {
        if (composite->parts[index].material_region_min < composite->material_region_min)
        {
            composite->material_region_min = composite->parts[index].material_region_min;
        }
        if (composite->parts[index].material_region_max > composite->material_region_max)
        {
            composite->material_region_max = composite->parts[index].material_region_max;
        }
    }
    *out_mesh = composite;
    return HENKA_SUCCESS;
}

henka_result henka_mesh_create_from_authoring_mesh(
    henka_engine* engine,
    const henka_authoring_mesh* source,
    henka_mesh** out_mesh)
{
    henka_authoring_mesh_counts counts;
    henka_authoring_mesh_desc desc;
    henka_mesh* parts[HENKA_MESH_MAX_PRIMITIVE_PARTS] = {NULL, NULL, NULL};
    size_t part_count = 0U;
    size_t vertex_slot;
    size_t edge_slot;
    size_t loose_edge_count = 0U;
    size_t isolated_count = 0U;
    henka_result result;

    if (out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (*out_mesh != NULL || engine == NULL || source == NULL || !henka_authoring_mesh_validate(source))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    counts = henka_authoring_mesh_get_counts(source);
    desc = henka_authoring_mesh_get_desc(source);
    for (edge_slot = 0U; edge_slot < desc.max_edges; ++edge_slot)
    {
        henka_authoring_edge_id edge_id;
        if (henka_authoring_mesh_get_edge_id_at(source, edge_slot, &edge_id) == HENKA_SUCCESS &&
            henka_authoring_mesh_get_edge_face_count(source, edge_id) == 0U)
        {
            ++loose_edge_count;
        }
    }
    for (vertex_slot = 0U; vertex_slot < desc.max_vertices; ++vertex_slot)
    {
        henka_authoring_vertex_id vertex_id;
        if (henka_authoring_mesh_get_vertex_id_at(source, vertex_slot, &vertex_id) == HENKA_SUCCESS &&
            henka_authoring_mesh_get_vertex_edge_count(source, vertex_id) == 0U &&
            !henka_authoring_vertex_is_face_referenced(source, vertex_id))
        {
            ++isolated_count;
        }
    }
    if (counts.faces > 0U)
    {
        result = henka_mesh_create_from_authoring_surface_source(engine, source, &parts[part_count]);
        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        ++part_count;
    }
    if (loose_edge_count > 0U)
    {
        result = henka_mesh_create_from_authoring_loose_source(
            engine, source, counts, HENKA_MESH_PRIMITIVE_LINES, &parts[part_count]);
        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        ++part_count;
    }
    if (isolated_count > 0U)
    {
        result = henka_mesh_create_from_authoring_loose_source(
            engine, source, counts, HENKA_MESH_PRIMITIVE_POINTS, &parts[part_count]);
        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        ++part_count;
    }
    if (part_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (part_count == 1U)
    {
        *out_mesh = parts[0];
        parts[0] = NULL;
        return HENKA_SUCCESS;
    }
    result = henka_mesh_compose_authoring_parts(engine, parts, part_count, out_mesh);

cleanup:
    for (vertex_slot = 0U; vertex_slot < HENKA_MESH_MAX_PRIMITIVE_PARTS; ++vertex_slot)
    {
        if (parts[vertex_slot] != NULL)
        {
            henka_mesh_destroy(parts[vertex_slot]);
        }
    }
    return result;
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
