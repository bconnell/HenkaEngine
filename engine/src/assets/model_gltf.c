#include <henka/model.h>

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/persistence.h>
#include <henka/core.h>

#include "../core/checked.h"

#define HENKA_MAX_GLTF_SOURCE_BYTES ((size_t)67108864U)
#define HENKA_MAX_GLTF_ARRAY_ITEMS 256U
#define HENKA_MAX_GLTF_JSON_DEPTH 32U

typedef struct henka_gltf_buffer
{
    const unsigned char* data;
    size_t size;
    unsigned char* owned_data;
} henka_gltf_buffer;

typedef struct henka_gltf_buffer_view
{
    int buffer;
    size_t byte_offset;
    size_t byte_length;
    size_t byte_stride;
} henka_gltf_buffer_view;

typedef struct henka_gltf_accessor
{
    int buffer_view;
    size_t byte_offset;
    size_t count;
    int component_type;
    int component_count;
    bool normalized;
} henka_gltf_accessor;

typedef struct henka_gltf_context
{
    char* json;
    size_t json_size;
    const unsigned char* glb_binary;
    size_t glb_binary_size;
    char* base_directory;
    henka_gltf_buffer buffers[HENKA_MAX_GLTF_ARRAY_ITEMS];
    size_t buffer_count;
    henka_gltf_buffer_view views[HENKA_MAX_GLTF_ARRAY_ITEMS];
    size_t view_count;
    henka_gltf_accessor accessors[HENKA_MAX_GLTF_ARRAY_ITEMS];
    size_t accessor_count;
} henka_gltf_context;

typedef struct henka_gltf_builder
{
    henka_model_vertex* vertices;
    size_t count;
    size_t capacity;
} henka_gltf_builder;

static bool henka_gltf_validate_extensions(const henka_gltf_context* context);

static const char* henka_gltf_skip_space(const char* cursor, const char* end)
{
    while (cursor < end && isspace((unsigned char)*cursor)) cursor += 1;
    return cursor;
}

static const char* henka_gltf_find_bytes(
    const char* begin,
    const char* end,
    const char* needle,
    size_t needle_size)
{
    const char* cursor;
    if (begin == NULL || end == NULL || needle == NULL || needle_size == 0U || begin > end) return NULL;
    if ((size_t)(end - begin) < needle_size) return NULL;
    for (cursor = begin; cursor <= end - needle_size; ++cursor)
        if (memcmp(cursor, needle, needle_size) == 0) return cursor;
    return NULL;
}

static bool henka_gltf_string_end(const char* cursor, const char* end, const char** out_end)
{
    bool escaped = false;
    if (cursor >= end || *cursor != '"' || out_end == NULL) return false;
    cursor += 1;
    while (cursor < end)
    {
        if (escaped) escaped = false;
        else if (*cursor == '\\') escaped = true;
        else if (*cursor == '"')
        {
            *out_end = cursor + 1;
            return true;
        }
        cursor += 1;
    }
    return false;
}

static bool henka_gltf_value_end_depth(
    const char* cursor,
    const char* end,
    size_t depth,
    const char** out_end)
{
    char opening;
    char closing;
    const char* child_end;

    cursor = henka_gltf_skip_space(cursor, end);
    if (cursor >= end || out_end == NULL) return false;
    if (*cursor == '"') return henka_gltf_string_end(cursor, end, out_end);
    if (*cursor != '{' && *cursor != '[')
    {
        while (cursor < end && *cursor != ',' && *cursor != '}' && *cursor != ']') cursor += 1;
        *out_end = cursor;
        return true;
    }
    if (depth >= HENKA_MAX_GLTF_JSON_DEPTH) return false;
    opening = *cursor;
    closing = opening == '{' ? '}' : ']';
    cursor += 1;
    while (true)
    {
        cursor = henka_gltf_skip_space(cursor, end);
        if (cursor >= end) return false;
        if (*cursor == closing)
        {
            *out_end = cursor + 1;
            return true;
        }
        if (opening == '{')
        {
            if (!henka_gltf_string_end(cursor, end, &child_end)) return false;
            cursor = henka_gltf_skip_space(child_end, end);
            if (cursor >= end || *cursor++ != ':') return false;
        }
        if (!henka_gltf_value_end_depth(cursor, end, depth + 1U, &child_end)) return false;
        cursor = henka_gltf_skip_space(child_end, end);
        if (cursor < end && *cursor == ',')
        {
            cursor += 1;
            if (henka_gltf_skip_space(cursor, end) >= end ||
                *henka_gltf_skip_space(cursor, end) == closing) return false;
            continue;
        }
        if (cursor < end && *cursor == closing)
        {
            *out_end = cursor + 1;
            return true;
        }
        return false;
    }
}

static bool henka_gltf_value_end(const char* cursor, const char* end, const char** out_end)
{
    const char* start = henka_gltf_skip_space(cursor, end);
    const char* value_end;
    if (start >= end || out_end == NULL) return false;
    if (*start == '{' || *start == '[' || *start == '"')
    {
        return henka_gltf_value_end_depth(start, end, 0U, out_end);
    }
    value_end = start;
    while (value_end < end && *value_end != ',' && *value_end != '}' && *value_end != ']') value_end += 1;
    while (value_end > start && isspace((unsigned char)value_end[-1])) value_end -= 1;
    *out_end = value_end;
    return value_end > start;
}

static bool henka_gltf_read_string(
    const char* cursor,
    const char* end,
    char* output,
    size_t output_size,
    const char** out_end)
{
    size_t length = 0U;
    bool escaped = false;
    unsigned char value;

    cursor = henka_gltf_skip_space(cursor, end);
    if (cursor >= end || *cursor != '"' || output == NULL || output_size == 0U) return false;
    cursor += 1;
    while (cursor < end && *cursor != '"')
    {
        value = (unsigned char)*cursor++;
        if (escaped)
        {
            escaped = false;
            if (value == 'n') value = '\n';
            else if (value == 'r') value = '\r';
            else if (value == 't') value = '\t';
            else if (value != '"' && value != '\\' && value != '/') return false;
        }
        else if (value == '\\')
        {
            escaped = true;
            continue;
        }
        if (value < 0x20U || length + 1U >= output_size) return false;
        output[length++] = (char)value;
    }
    if (cursor >= end || *cursor != '"' || escaped) return false;
    output[length] = '\0';
    if (out_end != NULL) *out_end = cursor + 1;
    return true;
}

static bool henka_gltf_find_member(
    const char* object,
    const char* object_end,
    const char* wanted,
    const char** out_value,
    const char** out_value_end)
{
    char key[96];
    const char* cursor;
    const char* key_end;
    const char* value_end;

    if (out_value != NULL) *out_value = NULL;
    if (out_value_end != NULL) *out_value_end = NULL;
    if (object == NULL || object_end == NULL || wanted == NULL ||
        object >= object_end || *henka_gltf_skip_space(object, object_end) != '{' ||
        out_value == NULL || out_value_end == NULL)
    {
        return false;
    }
    cursor = henka_gltf_skip_space(object + 1, object_end);
    while (cursor < object_end && *cursor != '}')
    {
        if (!henka_gltf_read_string(cursor, object_end, key, sizeof(key), &key_end)) return false;
        cursor = henka_gltf_skip_space(key_end, object_end);
        if (cursor >= object_end || *cursor++ != ':') return false;
        cursor = henka_gltf_skip_space(cursor, object_end);
        if (!henka_gltf_value_end(cursor, object_end, &value_end)) return false;
        if (strcmp(key, wanted) == 0)
        {
            *out_value = cursor;
            *out_value_end = value_end;
            return true;
        }
        cursor = henka_gltf_skip_space(value_end, object_end);
        if (cursor < object_end && *cursor == ',') cursor += 1;
        else if (cursor < object_end && *cursor != '}') return false;
        cursor = henka_gltf_skip_space(cursor, object_end);
    }
    return false;
}

static bool henka_gltf_version_is_supported(const henka_gltf_context* context)
{
    const char* asset;
    const char* asset_end;
    const char* version;
    const char* version_end;
    char version_text[8];

    return context != NULL &&
        context->json != NULL &&
        henka_gltf_find_member(
            context->json,
            context->json + context->json_size,
            "asset",
            &asset,
            &asset_end) &&
        henka_gltf_find_member(
            asset,
            asset_end,
            "version",
            &version,
            &version_end) &&
        henka_gltf_read_string(
            version,
            version_end,
            version_text,
            sizeof(version_text),
            NULL) &&
        strcmp(version_text, "2.0") == 0;
}

static bool henka_gltf_array_item(
    const char* array,
    const char* array_end,
    size_t wanted_index,
    const char** out_value,
    const char** out_value_end)
{
    const char* cursor;
    const char* value_end;
    size_t index = 0U;

    if (out_value != NULL) *out_value = NULL;
    if (out_value_end != NULL) *out_value_end = NULL;
    cursor = henka_gltf_skip_space(array, array_end);
    if (cursor >= array_end || *cursor++ != '[' || out_value == NULL || out_value_end == NULL) return false;
    while (true)
    {
        cursor = henka_gltf_skip_space(cursor, array_end);
        if (cursor >= array_end) return false;
        if (*cursor == ']') return false;
        if (!henka_gltf_value_end(cursor, array_end, &value_end)) return false;
        if (index == wanted_index)
        {
            *out_value = cursor;
            *out_value_end = value_end;
            return true;
        }
        index += 1U;
        cursor = henka_gltf_skip_space(value_end, array_end);
        if (cursor < array_end && *cursor == ',') { cursor += 1; continue; }
        return false;
    }
}

static bool henka_gltf_number(const char* value, const char* value_end, double* out_number)
{
    char buffer[96];
    size_t length;
    char* end_pointer;

    value = henka_gltf_skip_space(value, value_end);
    while (value_end > value && isspace((unsigned char)value_end[-1])) value_end -= 1;
    if (value == NULL || value_end == NULL || value >= value_end || out_number == NULL) return false;
    length = (size_t)(value_end - value);
    if (length == 0U || length >= sizeof(buffer)) return false;
    memcpy(buffer, value, length);
    buffer[length] = '\0';
    *out_number = strtod(buffer, &end_pointer);
    return end_pointer != buffer && *end_pointer == '\0' && isfinite(*out_number);
}

static bool henka_gltf_integer(const char* value, const char* value_end, int* out_integer)
{
    double number;
    if (out_integer != NULL) *out_integer = 0;
    if (!henka_gltf_number(value, value_end, &number) ||
        number < (double)INT_MIN || number > (double)INT_MAX || floor(number) != number || out_integer == NULL)
    {
        return false;
    }
    *out_integer = (int)number;
    return true;
}

static bool henka_gltf_size_number(const char* value, const char* value_end, size_t* out_size)
{
    double number;
    if (out_size != NULL) *out_size = 0U;
    if (!henka_gltf_number(value, value_end, &number) || number < 0.0 || number > (double)SIZE_MAX ||
        floor(number) != number || out_size == NULL)
    {
        return false;
    }
    *out_size = (size_t)number;
    return true;
}

static bool henka_gltf_member_size(const char* object, const char* object_end, const char* key, size_t* out_size)
{
    const char* value;
    const char* value_end;
    return henka_gltf_find_member(object, object_end, key, &value, &value_end) &&
        henka_gltf_size_number(value, value_end, out_size);
}

static bool henka_gltf_member_int(const char* object, const char* object_end, const char* key, int* out_value)
{
    const char* value;
    const char* value_end;
    return henka_gltf_find_member(object, object_end, key, &value, &value_end) &&
        henka_gltf_integer(value, value_end, out_value);
}

static bool henka_gltf_member_float(const char* object, const char* object_end, const char* key, float* out_value)
{
    const char* value;
    const char* value_end;
    double number;

    if (out_value != NULL && henka_gltf_find_member(object, object_end, key, &value, &value_end) &&
        henka_gltf_number(value, value_end, &number) && isfinite(number) &&
        number >= -(double)FLT_MAX && number <= (double)FLT_MAX)
    {
        *out_value = (float)number;
        return true;
    }
    return false;
}

static bool henka_gltf_member_bool(const char* object, const char* object_end, const char* key, bool* out_value)
{
    const char* value;
    const char* value_end;
    const char* cursor;

    if (out_value != NULL) *out_value = false;
    if (object == NULL || object_end == NULL || key == NULL || out_value == NULL ||
        !henka_gltf_find_member(object, object_end, key, &value, &value_end)) return false;
    cursor = henka_gltf_skip_space(value, value_end);
    if ((size_t)(value_end - cursor) == 4U && strncmp(cursor, "true", 4U) == 0)
    {
        *out_value = true;
        return true;
    }
    return (size_t)(value_end - cursor) == 5U && strncmp(cursor, "false", 5U) == 0;
}

static bool henka_gltf_member_vec3(
    const char* object,
    const char* object_end,
    const char* key,
    henka_vec3* out_value)
{
    const char* array;
    const char* array_end;
    const char* value;
    const char* value_end;
    double number;
    size_t index;

    if (out_value == NULL || !henka_gltf_find_member(object, object_end, key, &array, &array_end)) return false;
    for (index = 0U; index < 3U; ++index)
    {
        if (!henka_gltf_array_item(array, array_end, index, &value, &value_end) ||
            !henka_gltf_number(value, value_end, &number) || number < -(double)FLT_MAX || number > (double)FLT_MAX)
        {
            return false;
        }
        ((float*)&out_value->x)[index] = (float)number;
    }
    return !henka_gltf_array_item(array, array_end, 3U, &value, &value_end);
}

static bool henka_gltf_member_vec4(
    const char* object,
    const char* object_end,
    const char* key,
    henka_vec4* out_value)
{
    const char* array;
    const char* array_end;
    const char* value;
    const char* value_end;
    double number;
    size_t index;

    if (out_value == NULL || !henka_gltf_find_member(object, object_end, key, &array, &array_end)) return false;
    for (index = 0U; index < 4U; ++index)
    {
        if (!henka_gltf_array_item(array, array_end, index, &value, &value_end) ||
            !henka_gltf_number(value, value_end, &number) || number < -(double)FLT_MAX || number > (double)FLT_MAX)
        {
            return false;
        }
        ((float*)&out_value->x)[index] = (float)number;
    }
    return !henka_gltf_array_item(array, array_end, 4U, &value, &value_end);
}

static char* henka_gltf_duplicate_string(const char* value)
{
    size_t length;
    char* copy;

    if (value == NULL || !henka_checked_c_string_length(value, HENKA_MAX_ASSET_PATH_BYTES, &length)) return NULL;
    copy = henka_malloc(length + 1U);
    if (copy == NULL) return NULL;
    memcpy(copy, value, length + 1U);
    return copy;
}

static int henka_gltf_component_count(const char* type, const char* type_end)
{
    size_t length = (size_t)(type_end - type);
    if (length == 6U && strncmp(type, "SCALAR", length) == 0) return 1;
    if (length == 4U && strncmp(type, "VEC2", length) == 0) return 2;
    if (length == 4U && strncmp(type, "VEC3", length) == 0) return 3;
    if (length == 4U && strncmp(type, "VEC4", length) == 0) return 4;
    return 0;
}

static size_t henka_gltf_component_size(int component_type)
{
    switch (component_type)
    {
        case 5120: case 5121: return 1U;
        case 5122: case 5123: return 2U;
        case 5125: case 5126: return 4U;
        default: return 0U;
    }
}

static bool henka_gltf_decode_base64(const char* source, size_t source_length, unsigned char** out_data, size_t* out_size)
{
    size_t index;
    size_t output_index = 0U;
    unsigned char* data;
    int values[4];
    if (out_data != NULL) *out_data = NULL;
    if (out_size != NULL) *out_size = 0U;
    if (source == NULL || out_data == NULL || out_size == NULL || source_length % 4U != 0U) return false;
    if (!henka_checked_size_multiply(source_length / 4U, 3U, out_size)) return false;
    data = henka_malloc(*out_size == 0U ? 1U : *out_size);
    if (data == NULL) return false;
    for (index = 0U; index < source_length; index += 4U)
    {
        bool final_quartet = index + 4U == source_length;
        size_t component;
        for (component = 0U; component < 4U; ++component)
        {
            unsigned char value = (unsigned char)source[index + component];
            if (value >= 'A' && value <= 'Z') values[component] = value - 'A';
            else if (value >= 'a' && value <= 'z') values[component] = value - 'a' + 26;
            else if (value >= '0' && value <= '9') values[component] = value - '0' + 52;
            else if (value == '+') values[component] = 62;
            else if (value == '/') values[component] = 63;
            else if (value == '=' && final_quartet && component >= 2U) values[component] = -1;
            else { henka_free(data); return false; }
        }
        if (values[0] < 0 || values[1] < 0 ||
            (values[2] < 0 && values[3] >= 0) ||
            (!final_quartet && (values[2] < 0 || values[3] < 0)) ||
            (values[2] < 0 && (values[1] & 0x0F) != 0) ||
            (values[3] < 0 && values[2] >= 0 && (values[2] & 0x03) != 0))
        {
            henka_free(data);
            return false;
        }
        data[output_index++] = (unsigned char)((values[0] << 2) | (values[1] >> 4));
        if (values[2] >= 0)
        {
            data[output_index++] = (unsigned char)((values[1] << 4) | (values[2] >> 2));
            if (values[3] >= 0) data[output_index++] = (unsigned char)((values[2] << 6) | values[3]);
        }
    }
    *out_size = output_index;
    *out_data = data;
    return true;
}

static char* henka_gltf_read_file(const char* path, size_t* out_size)
{
    FILE* file = NULL;
    long length;
    char* data;
    size_t size;
    if (out_size != NULL) *out_size = 0U;
    if (path == NULL || out_size == NULL || fopen_s(&file, path, "rb") != 0 || file == NULL) return NULL;
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        (unsigned long long)length > HENKA_MAX_GLTF_SOURCE_BYTES || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }
    size = (size_t)length;
    data = henka_malloc(size + 1U);
    if (data == NULL || fread(data, 1U, size, file) != size)
    {
        henka_free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    data[size] = '\0';
    *out_size = size;
    return data;
}

static void henka_gltf_context_destroy(henka_gltf_context* context)
{
    size_t index;
    if (context == NULL) return;
    for (index = 0U; index < context->buffer_count; ++index) henka_free(context->buffers[index].owned_data);
    henka_free(context->json);
    henka_free(context->base_directory);
    memset(context, 0, sizeof(*context));
}

static bool henka_gltf_parse_buffers(henka_gltf_context* context)
{
    const char* array;
    const char* array_end;
    const char* item;
    const char* item_end;
    size_t index;
    if (!henka_gltf_find_member(context->json, context->json + context->json_size, "buffers", &array, &array_end)) return false;
    for (index = 0U; index < HENKA_MAX_GLTF_ARRAY_ITEMS && henka_gltf_array_item(array, array_end, index, &item, &item_end); ++index)
    {
        size_t expected_size;
        char uri[HENKA_MAX_ASSET_PATH_BYTES];
        const char* uri_end;
        size_t uri_length;
        if (!henka_gltf_member_size(item, item_end, "byteLength", &expected_size) || expected_size > HENKA_MAX_GLTF_SOURCE_BYTES) return false;
        context->buffers[index].data = NULL;
        context->buffers[index].size = 0U;
        context->buffers[index].owned_data = NULL;
        if (henka_gltf_find_member(item, item_end, "uri", &item, &uri_end))
        {
            if (!henka_gltf_read_string(item, uri_end, uri, sizeof(uri), &uri_end)) return false;
            uri_length = strlen(uri);
            if (strncmp(uri, "data:", 5U) == 0)
            {
                const char* separator = strstr(uri, ";base64,");
                size_t encoded_offset;
                if (separator == NULL) return false;
                encoded_offset = (size_t)(separator - uri) + 8U;
                if (!henka_gltf_decode_base64(uri + encoded_offset, uri_length - encoded_offset, &context->buffers[index].owned_data, &context->buffers[index].size)) return false;
            }
            else
            {
                char* resolved = NULL;
                size_t external_size = 0U;
                if (context->base_directory == NULL || henka_path_resolve_confined(context->base_directory, uri, &resolved) != HENKA_SUCCESS) return false;
                context->buffers[index].owned_data = (unsigned char*)henka_gltf_read_file(resolved, &external_size);
                henka_free(resolved);
                context->buffers[index].size = external_size;
            }
            context->buffers[index].data = context->buffers[index].owned_data;
        }
        else if (context->glb_binary != NULL && index == 0U)
        {
            context->buffers[index].data = context->glb_binary;
            context->buffers[index].size = context->glb_binary_size;
        }
        else return false;
        if (context->buffers[index].data == NULL || context->buffers[index].size < expected_size) return false;
    }
    context->buffer_count = index;
    return index > 0U && index < HENKA_MAX_GLTF_ARRAY_ITEMS;
}

static bool henka_gltf_parse_views(henka_gltf_context* context)
{
    const char* array; const char* array_end; const char* item; const char* item_end; size_t index;
    if (!henka_gltf_find_member(context->json, context->json + context->json_size, "bufferViews", &array, &array_end)) return false;
    for (index = 0U; index < HENKA_MAX_GLTF_ARRAY_ITEMS && henka_gltf_array_item(array, array_end, index, &item, &item_end); ++index)
    {
        henka_gltf_buffer_view* view = &context->views[index];
        view->buffer = -1; view->byte_offset = 0U; view->byte_stride = 0U;
        if (!henka_gltf_member_int(item, item_end, "buffer", &view->buffer) ||
            !henka_gltf_member_size(item, item_end, "byteLength", &view->byte_length) ||
            view->buffer < 0 || (size_t)view->buffer >= context->buffer_count ||
            view->byte_length > context->buffers[view->buffer].size) return false;
        { const char* value; const char* value_end; if (henka_gltf_find_member(item, item_end, "byteOffset", &value, &value_end) && !henka_gltf_size_number(value, value_end, &view->byte_offset)) return false; }
        { size_t view_end; if (!henka_checked_size_add(view->byte_offset, view->byte_length, &view_end) || view_end > context->buffers[view->buffer].size) return false; }
        { const char* value; const char* value_end; if (henka_gltf_find_member(item, item_end, "byteStride", &value, &value_end) && !henka_gltf_size_number(value, value_end, &view->byte_stride)) return false; }
        if (view->byte_stride > 0U && (view->byte_stride < 1U || view->byte_stride > 256U)) return false;
    }
    context->view_count = index;
    return index > 0U && index < HENKA_MAX_GLTF_ARRAY_ITEMS;
}

static bool henka_gltf_accessor_span_is_valid(
    const henka_gltf_buffer_view* view,
    size_t byte_offset,
    size_t count,
    size_t element_size)
{
    size_t stride;
    size_t last_element_offset;
    size_t end_offset;

    if (view == NULL || element_size == 0U || byte_offset > view->byte_length)
    {
        return false;
    }
    stride = view->byte_stride == 0U ? element_size : view->byte_stride;
    if (stride < element_size)
    {
        return false;
    }
    if (count == 0U)
    {
        return true;
    }
    return henka_checked_size_multiply(count - 1U, stride, &last_element_offset) &&
        henka_checked_size_add(last_element_offset, byte_offset, &last_element_offset) &&
        henka_checked_size_add(last_element_offset, element_size, &end_offset) &&
        end_offset <= view->byte_length;
}

static bool henka_gltf_parse_accessors(henka_gltf_context* context)
{
    const char* array; const char* array_end; const char* item; const char* item_end; size_t index;
    if (!henka_gltf_find_member(context->json, context->json + context->json_size, "accessors", &array, &array_end)) return false;
    for (index = 0U; index < HENKA_MAX_GLTF_ARRAY_ITEMS && henka_gltf_array_item(array, array_end, index, &item, &item_end); ++index)
    {
        henka_gltf_accessor* accessor = &context->accessors[index];
        size_t accessor_count;
        char type[16]; const char* type_value; const char* type_end;
        accessor->buffer_view = -1; accessor->byte_offset = 0U; accessor->normalized = false;
        /* Sparse accessors require applying index/value patches before any
         * attribute is read. Rejecting them is safer than rendering a valid
         * but materially different base buffer. */
        if (henka_gltf_find_member(item, item_end, "sparse", &type_value, &type_end)) return false;
        if (!henka_gltf_member_size(item, item_end, "count", &accessor_count) ||
            !henka_gltf_member_int(item, item_end, "componentType", &accessor->component_type) ||
            !henka_gltf_find_member(item, item_end, "type", &type_value, &type_end) ||
            !henka_gltf_read_string(type_value, type_end, type, sizeof(type), NULL)) return false;
        accessor->count = accessor_count;
        if (accessor->count > HENKA_MAX_MESH_ELEMENTS || (accessor->component_count = henka_gltf_component_count(type, type + strlen(type))) == 0 || henka_gltf_component_size(accessor->component_type) == 0) return false;
        { const char* value; const char* value_end; if (henka_gltf_find_member(item, item_end, "bufferView", &value, &value_end) && !henka_gltf_member_int(item, item_end, "bufferView", &accessor->buffer_view)) return false; }
        { const char* value; const char* value_end; if (henka_gltf_find_member(item, item_end, "byteOffset", &value, &value_end) && !henka_gltf_size_number(value, value_end, &accessor->byte_offset)) return false; }
        if (henka_gltf_find_member(item, item_end, "normalized", &type_value, &type_end) &&
            !henka_gltf_member_bool(item, item_end, "normalized", &accessor->normalized)) return false;
        if (accessor->buffer_view >= 0)
        {
            size_t component_size = henka_gltf_component_size(accessor->component_type);
            size_t element_size;
            const henka_gltf_buffer_view* view;
            if ((size_t)accessor->buffer_view >= context->view_count ||
                !henka_checked_size_multiply(component_size, (size_t)accessor->component_count, &element_size)) return false;
            view = &context->views[accessor->buffer_view];
            if (accessor->byte_offset % component_size != 0U ||
                (view->byte_stride != 0U && (view->byte_stride < element_size ||
                    view->byte_stride > 252U || view->byte_stride % 4U != 0U)) ||
                !henka_gltf_accessor_span_is_valid(
                    view, accessor->byte_offset, accessor->count, element_size)) return false;
        }
    }
    context->accessor_count = index;
    return index > 0U && index < HENKA_MAX_GLTF_ARRAY_ITEMS;
}

static bool henka_gltf_accessor_address(
    const henka_gltf_context* context,
    int accessor_index,
    size_t element_index,
    size_t component_index,
    const unsigned char** out_address)
{
    const henka_gltf_accessor* accessor;
    const henka_gltf_buffer_view* view;
    size_t component_size;
    size_t element_size;
    size_t stride;
    size_t offset;
    size_t component_offset;
    if (out_address != NULL) *out_address = NULL;
    if (context == NULL || out_address == NULL || accessor_index < 0 || (size_t)accessor_index >= context->accessor_count) return false;
    accessor = &context->accessors[accessor_index];
    if (accessor->buffer_view < 0 || (size_t)accessor->buffer_view >= context->view_count || element_index >= accessor->count || component_index >= (size_t)accessor->component_count) return false;
    view = &context->views[accessor->buffer_view];
    component_size = henka_gltf_component_size(accessor->component_type);
    if (!henka_checked_size_multiply(component_size, (size_t)accessor->component_count, &element_size)) return false;
    stride = view->byte_stride == 0U ? element_size : view->byte_stride;
    if (stride < element_size || !henka_checked_size_multiply(element_index, stride, &offset) ||
        !henka_checked_size_add(offset, accessor->byte_offset, &offset) ||
        !henka_checked_size_multiply(component_index, component_size, &component_offset) ||
        !henka_checked_size_add(offset, component_offset, &offset) ||
        !henka_checked_size_add(offset, component_size, &offset) ||
        offset > view->byte_length) return false;
    *out_address = context->buffers[view->buffer].data + view->byte_offset + offset - component_size;
    return true;
}

static bool henka_gltf_uv_accessor_is_valid(const henka_gltf_accessor* accessor)
{
    return accessor != NULL && accessor->component_count == 2 &&
        (accessor->component_type == 5126 ||
         (accessor->normalized &&
          (accessor->component_type == 5121 || accessor->component_type == 5123)));
}

static bool henka_gltf_color_accessor_is_valid(const henka_gltf_accessor* accessor)
{
    return accessor != NULL &&
        (accessor->component_count == 3 || accessor->component_count == 4) &&
        (accessor->component_type == 5126 ||
         (accessor->normalized &&
          (accessor->component_type == 5121 || accessor->component_type == 5123)));
}

static bool henka_gltf_read_component(const henka_gltf_context* context, int accessor_index, size_t element_index, size_t component_index, float* out_value)
{
    const unsigned char* address; const henka_gltf_accessor* accessor; double value = 0.0;
    if (out_value != NULL) *out_value = 0.0f;
    if (context == NULL || out_value == NULL || accessor_index < 0 || (size_t)accessor_index >= context->accessor_count || !henka_gltf_accessor_address(context, accessor_index, element_index, component_index, &address)) return false;
    accessor = &context->accessors[accessor_index];
    switch (accessor->component_type)
    {
        case 5120: value = *(const int8_t*)address; break;
        case 5121: value = *address; break;
        case 5122: { int16_t v; memcpy(&v, address, sizeof(v)); value = v; break; }
        case 5123: { uint16_t v; memcpy(&v, address, sizeof(v)); value = v; break; }
        case 5125: { uint32_t v; memcpy(&v, address, sizeof(v)); value = v; break; }
        case 5126: { float v; memcpy(&v, address, sizeof(v)); value = v; break; }
        default: return false;
    }
    if (accessor->normalized)
    {
        if (accessor->component_type == 5120) value = value < 0.0 ? value / 128.0 : value / 127.0;
        else if (accessor->component_type == 5121) value /= 255.0;
        else if (accessor->component_type == 5122) value = value < 0.0 ? value / 32768.0 : value / 32767.0;
        else if (accessor->component_type == 5123) value /= 65535.0;
        else if (accessor->component_type == 5125) value /= 4294967295.0;
    }
    if (!isfinite(value) || value < -FLT_MAX || value > FLT_MAX) return false;
    *out_value = (float)value;
    return true;
}

static bool henka_gltf_builder_push(henka_gltf_builder* builder, const henka_model_vertex* vertex)
{
    size_t capacity; henka_model_vertex* vertices;
    if (builder == NULL || vertex == NULL || builder->count >= HENKA_MAX_MESH_ELEMENTS || !henka_checked_capacity(builder->capacity, builder->count + 1U, 128U, HENKA_MAX_MESH_ELEMENTS, &capacity)) return false;
    if (capacity != builder->capacity)
    {
        vertices = henka_realloc(builder->vertices, capacity * sizeof(*vertices));
        if (vertices == NULL) return false;
        builder->vertices = vertices; builder->capacity = capacity;
    }
    builder->vertices[builder->count++] = *vertex;
    return true;
}

static bool henka_gltf_parse_primitive(const henka_gltf_context* context, const char* primitive, const char* primitive_end, henka_gltf_builder* builder)
{
    const char* attributes; const char* attributes_end; const char* value; const char* value_end;
    int position_accessor; int normal_accessor = -1; int uv_accessor = -1; int uv1_accessor = -1; int color_accessor = -1; int tangent_accessor = -1; int index_accessor = -1; int mode = 4;
    size_t base_vertex_count; size_t index_count; size_t index;
    if (henka_gltf_find_member(primitive, primitive_end, "mode", &value, &value_end) && !henka_gltf_integer(value, value_end, &mode)) return false;
    if (mode != 4 || !henka_gltf_find_member(primitive, primitive_end, "attributes", &attributes, &attributes_end) ||
        !henka_gltf_member_int(attributes, attributes_end, "POSITION", &position_accessor)) return false;
    if (henka_gltf_find_member(attributes, attributes_end, "NORMAL", &value, &value_end) &&
        (!henka_gltf_member_int(attributes, attributes_end, "NORMAL", &normal_accessor) || normal_accessor < 0)) return false;
    if (henka_gltf_find_member(attributes, attributes_end, "TEXCOORD_0", &value, &value_end) &&
        (!henka_gltf_member_int(attributes, attributes_end, "TEXCOORD_0", &uv_accessor) || uv_accessor < 0)) return false;
    if (henka_gltf_find_member(attributes, attributes_end, "TEXCOORD_1", &value, &value_end) &&
        (!henka_gltf_member_int(attributes, attributes_end, "TEXCOORD_1", &uv1_accessor) || uv1_accessor < 0)) return false;
    if (henka_gltf_find_member(attributes, attributes_end, "COLOR_0", &value, &value_end) &&
        (!henka_gltf_member_int(attributes, attributes_end, "COLOR_0", &color_accessor) || color_accessor < 0)) return false;
    if (henka_gltf_find_member(attributes, attributes_end, "TANGENT", &value, &value_end) &&
        (!henka_gltf_member_int(attributes, attributes_end, "TANGENT", &tangent_accessor) || tangent_accessor < 0)) return false;
    if (henka_gltf_find_member(primitive, primitive_end, "indices", &value, &value_end) &&
        (!henka_gltf_integer(value, value_end, &index_accessor) || index_accessor < 0)) return false;
    if (position_accessor < 0 || (size_t)position_accessor >= context->accessor_count || context->accessors[position_accessor].component_count != 3 || context->accessors[position_accessor].component_type != 5126) return false;
    if ((normal_accessor >= 0 && (size_t)normal_accessor >= context->accessor_count) ||
        (uv_accessor >= 0 && (size_t)uv_accessor >= context->accessor_count) ||
        (uv1_accessor >= 0 && (size_t)uv1_accessor >= context->accessor_count) ||
        (color_accessor >= 0 && (size_t)color_accessor >= context->accessor_count) ||
        (tangent_accessor >= 0 && (size_t)tangent_accessor >= context->accessor_count)) return false;
    if (normal_accessor >= 0 &&
        (context->accessors[normal_accessor].component_count != 3 ||
         context->accessors[normal_accessor].component_type != 5126 ||
         context->accessors[normal_accessor].normalized)) return false;
    if (tangent_accessor >= 0 &&
        (context->accessors[tangent_accessor].component_count != 4 ||
         context->accessors[tangent_accessor].component_type != 5126 ||
         context->accessors[tangent_accessor].normalized)) return false;
    if ((uv_accessor >= 0 && !henka_gltf_uv_accessor_is_valid(&context->accessors[uv_accessor])) ||
        (uv1_accessor >= 0 && !henka_gltf_uv_accessor_is_valid(&context->accessors[uv1_accessor])) ||
        (color_accessor >= 0 && !henka_gltf_color_accessor_is_valid(&context->accessors[color_accessor]))) return false;
    if (index_accessor >= 0 &&
        (context->accessors[index_accessor].component_count != 1 ||
         context->accessors[index_accessor].normalized ||
         (context->accessors[index_accessor].component_type != 5121 &&
          context->accessors[index_accessor].component_type != 5123 &&
          context->accessors[index_accessor].component_type != 5125))) return false;
    index_count = index_accessor >= 0 ? context->accessors[index_accessor].count : context->accessors[position_accessor].count;
    if (index_count == 0U || index_count % 3U != 0U || (index_accessor >= 0 && (size_t)index_accessor >= context->accessor_count)) return false;
    base_vertex_count = builder->count;
    for (index = 0U; index < index_count; ++index)
    {
        size_t source_index = index;
        henka_model_vertex vertex;
        float component;
        if (index_accessor >= 0)
        {
            if (!henka_gltf_read_component(context, index_accessor, index, 0U, &component) || component < 0.0f || floor(component) != component || component >= (float)context->accessors[position_accessor].count) return false;
            source_index = (size_t)component;
        }
        memset(&vertex, 0, sizeof(vertex));
        vertex.color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};
        if (!henka_gltf_read_component(context, position_accessor, source_index, 0U, &vertex.position.x) || !henka_gltf_read_component(context, position_accessor, source_index, 1U, &vertex.position.y) || !henka_gltf_read_component(context, position_accessor, source_index, 2U, &vertex.position.z)) return false;
        if (normal_accessor >= 0 && context->accessors[normal_accessor].component_count == 3 && (!henka_gltf_read_component(context, normal_accessor, source_index, 0U, &vertex.normal.x) || !henka_gltf_read_component(context, normal_accessor, source_index, 1U, &vertex.normal.y) || !henka_gltf_read_component(context, normal_accessor, source_index, 2U, &vertex.normal.z))) return false;
        if (uv_accessor >= 0 && context->accessors[uv_accessor].component_count == 2 && (!henka_gltf_read_component(context, uv_accessor, source_index, 0U, &vertex.uv.x) || !henka_gltf_read_component(context, uv_accessor, source_index, 1U, &vertex.uv.y))) return false;
        if (uv1_accessor >= 0 && context->accessors[uv1_accessor].component_count == 2 && (!henka_gltf_read_component(context, uv1_accessor, source_index, 0U, &vertex.uv1.x) || !henka_gltf_read_component(context, uv1_accessor, source_index, 1U, &vertex.uv1.y))) return false;
        vertex.uv1_valid = uv1_accessor >= 0 && context->accessors[uv1_accessor].component_count == 2;
        if (color_accessor >= 0 && (context->accessors[color_accessor].component_count == 3 || context->accessors[color_accessor].component_count == 4) && (!henka_gltf_read_component(context, color_accessor, source_index, 0U, &vertex.color.x) || !henka_gltf_read_component(context, color_accessor, source_index, 1U, &vertex.color.y) || !henka_gltf_read_component(context, color_accessor, source_index, 2U, &vertex.color.z) || (context->accessors[color_accessor].component_count == 4 && !henka_gltf_read_component(context, color_accessor, source_index, 3U, &vertex.color.w)))) return false;
        if (tangent_accessor >= 0 && context->accessors[tangent_accessor].component_count == 4 && (!henka_gltf_read_component(context, tangent_accessor, source_index, 0U, &vertex.tangent.x) || !henka_gltf_read_component(context, tangent_accessor, source_index, 1U, &vertex.tangent.y) || !henka_gltf_read_component(context, tangent_accessor, source_index, 2U, &vertex.tangent.z) || !henka_gltf_read_component(context, tangent_accessor, source_index, 3U, &vertex.tangent.w))) return false;
        if (tangent_accessor >= 0 && vertex.tangent.w != 1.0f && vertex.tangent.w != -1.0f) return false;
        vertex.tangent_valid = tangent_accessor >= 0;
        if (!henka_gltf_builder_push(builder, &vertex)) return false;
    }
    if (normal_accessor < 0)
    {
        for (index = base_vertex_count; index < builder->count; index += 3U)
        {
            henka_model_vertex* a = &builder->vertices[index]; henka_model_vertex* b = &builder->vertices[index + 1U]; henka_model_vertex* c = &builder->vertices[index + 2U];
            henka_vec3 ab = {b->position.x - a->position.x, b->position.y - a->position.y, b->position.z - a->position.z};
            henka_vec3 ac = {c->position.x - a->position.x, c->position.y - a->position.y, c->position.z - a->position.z};
            henka_vec3 normal = {ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
            float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (!isfinite(length) || length <= FLT_EPSILON) return false;
            normal.x /= length; normal.y /= length; normal.z /= length; a->normal = normal; b->normal = normal; c->normal = normal;
        }
    }
    return true;
}

static bool henka_gltf_primitive_material_index(
    const char* primitive,
    const char* primitive_end,
    bool* out_has_material,
    int* out_material_index)
{
    const char* value;
    const char* value_end;

    if (out_has_material != NULL) *out_has_material = false;
    if (out_material_index != NULL) *out_material_index = -1;
    if (primitive == NULL || primitive_end == NULL || out_has_material == NULL || out_material_index == NULL) return false;
    if (!henka_gltf_find_member(primitive, primitive_end, "material", &value, &value_end)) return true;
    if (!henka_gltf_integer(value, value_end, out_material_index) || *out_material_index < 0) return false;
    *out_has_material = true;
    return true;
}

static bool henka_gltf_material_texture_uri(
    const henka_gltf_context* context,
    int texture_index,
    char** out_uri,
    unsigned char** out_data,
    size_t* out_data_size)
{
    const char* textures;
    const char* textures_end;
    const char* texture;
    const char* texture_end;
    const char* images;
    const char* images_end;
    const char* image;
    const char* image_end;
    const char* value;
    const char* value_end;
    char uri[HENKA_MAX_ASSET_PATH_BYTES];
    int image_index;

    if (out_uri != NULL) *out_uri = NULL;
    if (out_data != NULL) *out_data = NULL;
    if (out_data_size != NULL) *out_data_size = 0U;
    if (context == NULL || out_uri == NULL || out_data == NULL || out_data_size == NULL || texture_index < 0 ||
        !henka_gltf_find_member(context->json, context->json + context->json_size, "textures", &textures, &textures_end) ||
        !henka_gltf_array_item(textures, textures_end, (size_t)texture_index, &texture, &texture_end) ||
        !henka_gltf_find_member(context->json, context->json + context->json_size, "images", &images, &images_end))
    {
        return false;
    }
    if (!henka_gltf_member_int(texture, texture_end, "source", &image_index))
    {
        if (!henka_gltf_find_member(texture, texture_end, "extensions", &value, &value_end) ||
            !henka_gltf_find_member(value, value_end, "KHR_texture_basisu", &value, &value_end) ||
            !henka_gltf_member_int(value, value_end, "source", &image_index)) return false;
    }
    if (image_index < 0 || !henka_gltf_array_item(images, images_end, (size_t)image_index, &image, &image_end) ||
        (!henka_gltf_find_member(image, image_end, "uri", &value, &value_end) &&
         !henka_gltf_find_member(image, image_end, "bufferView", &value, &value_end))) return false;
    if (henka_gltf_find_member(image, image_end, "uri", &value, &value_end))
    {
        const char* content = henka_gltf_skip_space(value, value_end) + 1;
        const char* content_end = value_end - 1;
        const char* base64_marker = henka_gltf_find_bytes(content, content_end, ";base64,", 8U);
        if (base64_marker != NULL && base64_marker < content_end && strncmp(content, "data:", 5U) == 0)
        {
            size_t encoded_offset = (size_t)(base64_marker - content) + 8U;
            size_t encoded_size = (size_t)(content_end - content) - encoded_offset;
            return henka_gltf_decode_base64(content + encoded_offset, encoded_size, out_data, out_data_size);
        }
        if (!henka_gltf_read_string(value, value_end, uri, sizeof(uri), NULL)) return false;
        *out_uri = henka_gltf_duplicate_string(uri);
        return *out_uri != NULL;
    }
    {
        int buffer_view;
        const henka_gltf_buffer_view* view;
        size_t end_offset;
        if (!henka_gltf_member_int(image, image_end, "bufferView", &buffer_view) || buffer_view < 0 ||
            (size_t)buffer_view >= context->view_count) return false;
        view = &context->views[buffer_view];
        if (view->buffer < 0 || (size_t)view->buffer >= context->buffer_count ||
            !henka_checked_size_add(view->byte_offset, view->byte_length, &end_offset) ||
            end_offset > context->buffers[view->buffer].size) return false;
        *out_data = henka_malloc(view->byte_length == 0U ? 1U : view->byte_length);
        if (*out_data == NULL) return false;
        memcpy(*out_data, context->buffers[view->buffer].data + view->byte_offset, view->byte_length);
        *out_data_size = view->byte_length;
        return true;
    }
}

static bool henka_gltf_material_texture(
    const henka_gltf_context* context,
    const char* object,
    const char* object_end,
    const char* key,
    char** out_uri,
    unsigned char** out_data,
    size_t* out_data_size,
    int* out_texcoord)
{
    const char* texture_info;
    const char* texture_info_end;
    int texture_index;

    if (out_uri != NULL) *out_uri = NULL;
    if (out_data != NULL) *out_data = NULL;
    if (out_data_size != NULL) *out_data_size = 0U;
    if (out_texcoord != NULL) *out_texcoord = 0;
    if (context == NULL || object == NULL || out_uri == NULL || out_data == NULL || out_data_size == NULL) return false;
    if (!henka_gltf_find_member(object, object_end, key, &texture_info, &texture_info_end)) return true;
    if (!henka_gltf_member_int(texture_info, texture_info_end, "index", &texture_index)) return false;
    if (henka_gltf_find_member(texture_info, texture_info_end, "texCoord", &texture_info, &texture_info_end))
    {
        if (out_texcoord == NULL || !henka_gltf_integer(texture_info, texture_info_end, out_texcoord) ||
            *out_texcoord < 0 || *out_texcoord > 1) return false;
    }
    return henka_gltf_material_texture_uri(context, texture_index, out_uri, out_data, out_data_size);
}

static bool henka_gltf_parse_material(
    const henka_gltf_context* context,
    const char* primitive,
    const char* primitive_end,
    henka_model_material_source* out_source)
{
    const char* materials;
    const char* materials_end;
    const char* material_object;
    const char* material_end;
    const char* pbr;
    const char* pbr_end;
    const char* value;
    const char* value_end;
    char name[HENKA_MAX_ASSET_PATH_BYTES];
    char alpha_mode[16];
    int material_index;

    if (context == NULL || primitive == NULL || primitive_end == NULL || out_source == NULL ||
        !henka_gltf_primitive_material_index(primitive, primitive_end, &(bool){false}, &material_index) ||
        !henka_gltf_find_member(context->json, context->json + context->json_size, "materials", &materials, &materials_end) ||
        !henka_gltf_array_item(materials, materials_end, (size_t)material_index, &material_object, &material_end))
    {
        return false;
    }

    out_source->material = henka_material_default();
    if (henka_gltf_find_member(material_object, material_end, "name", &value, &value_end))
    {
        if (!henka_gltf_read_string(value, value_end, name, sizeof(name), NULL)) return false;
        out_source->name = henka_gltf_duplicate_string(name);
        if (out_source->name == NULL) return false;
        out_source->material.name = out_source->name;
    }

    if (henka_gltf_find_member(material_object, material_end, "pbrMetallicRoughness", &pbr, &pbr_end))
    {
        if (henka_gltf_find_member(pbr, pbr_end, "baseColorFactor", &value, &value_end) &&
            !henka_gltf_member_vec4(pbr, pbr_end, "baseColorFactor", &out_source->material.base_color)) return false;
        if (henka_gltf_find_member(pbr, pbr_end, "metallicFactor", &value, &value_end) &&
            !henka_gltf_member_float(pbr, pbr_end, "metallicFactor", &out_source->material.metallic)) return false;
        if (henka_gltf_find_member(pbr, pbr_end, "roughnessFactor", &value, &value_end) &&
            !henka_gltf_member_float(pbr, pbr_end, "roughnessFactor", &out_source->material.roughness)) return false;
        if (!henka_gltf_material_texture(context, pbr, pbr_end, "baseColorTexture", &out_source->base_color_uri,
                &out_source->base_color_embedded_data, &out_source->base_color_embedded_size,
                &out_source->material.base_color_uv_set) ||
            !henka_gltf_material_texture(context, pbr, pbr_end, "metallicRoughnessTexture", &out_source->metallic_roughness_uri,
                &out_source->metallic_roughness_embedded_data, &out_source->metallic_roughness_embedded_size,
                &out_source->material.metallic_roughness_uv_set)) return false;
    }
    if (!henka_gltf_material_texture(context, material_object, material_end, "normalTexture", &out_source->normal_uri,
            &out_source->normal_embedded_data, &out_source->normal_embedded_size,
            &out_source->material.normal_uv_set) ||
        !henka_gltf_material_texture(context, material_object, material_end, "occlusionTexture", &out_source->occlusion_uri,
            &out_source->occlusion_embedded_data, &out_source->occlusion_embedded_size,
            &out_source->material.occlusion_uv_set) ||
        !henka_gltf_material_texture(context, material_object, material_end, "emissiveTexture", &out_source->emissive_uri,
            &out_source->emissive_embedded_data, &out_source->emissive_embedded_size,
            &out_source->material.emissive_uv_set)) return false;

    if (henka_gltf_find_member(material_object, material_end, "normalTexture", &value, &value_end) &&
        henka_gltf_member_float(value, value_end, "scale", &out_source->material.normal_scale) == false &&
        henka_gltf_find_member(value, value_end, "scale", &value, &value_end)) return false;
    if (henka_gltf_find_member(material_object, material_end, "occlusionTexture", &value, &value_end) &&
        henka_gltf_member_float(value, value_end, "strength", &out_source->material.occlusion_strength) == false &&
        henka_gltf_find_member(value, value_end, "strength", &value, &value_end)) return false;
    if (henka_gltf_find_member(material_object, material_end, "emissiveFactor", &value, &value_end) &&
        !henka_gltf_member_vec3(material_object, material_end, "emissiveFactor", &out_source->material.emissive_color)) return false;
    if (henka_gltf_find_member(material_object, material_end, "alphaMode", &value, &value_end))
    {
        if (!henka_gltf_read_string(value, value_end, alpha_mode, sizeof(alpha_mode), NULL)) return false;
        if (strcmp(alpha_mode, "MASK") == 0) out_source->material.alpha_mode = HENKA_MATERIAL_ALPHA_MASKED;
        else if (strcmp(alpha_mode, "BLEND") == 0) out_source->material.alpha_mode = HENKA_MATERIAL_ALPHA_BLENDED;
        else if (strcmp(alpha_mode, "OPAQUE") != 0) return false;
    }
    if (henka_gltf_find_member(material_object, material_end, "alphaCutoff", &value, &value_end) &&
        !henka_gltf_member_float(material_object, material_end, "alphaCutoff", &out_source->material.alpha_cutoff)) return false;
    if (henka_gltf_find_member(material_object, material_end, "doubleSided", &value, &value_end))
    {
        if (!henka_gltf_member_bool(material_object, material_end, "doubleSided", &out_source->material.double_sided)) return false;
    }

    /* KHR_materials_* values map into the existing renderer-facing controls. */
    if (henka_gltf_find_member(material_object, material_end, "extensions", &value, &value_end))
    {
        const char* extension;
        const char* extension_end;
        const char* extensions = value;
        const char* extensions_end = value_end;
        if (henka_gltf_find_member(extensions, extensions_end, "KHR_materials_ior", &extension, &extension_end) &&
            henka_gltf_find_member(extension, extension_end, "ior", &value, &value_end) &&
            !henka_gltf_member_float(extension, extension_end, "ior", &out_source->material.ior)) return false;
        if (henka_gltf_find_member(extensions, extensions_end, "KHR_materials_transmission", &extension, &extension_end))
        {
            if (!henka_gltf_material_texture(context, extension, extension_end, "transmissionTexture",
                    &out_source->transmission_uri, &out_source->transmission_embedded_data,
                    &out_source->transmission_embedded_size, &out_source->material.transmission_uv_set)) return false;
            if (henka_gltf_find_member(extension, extension_end, "transmissionFactor", &value, &value_end) &&
                !henka_gltf_member_float(extension, extension_end, "transmissionFactor", &out_source->material.transmission)) return false;
        }
        if (henka_gltf_find_member(extensions, extensions_end, "KHR_materials_volume", &extension, &extension_end))
        {
            if (!henka_gltf_material_texture(context, extension, extension_end, "thicknessTexture",
                    &out_source->thickness_uri, &out_source->thickness_embedded_data,
                    &out_source->thickness_embedded_size, &out_source->material.thickness_uv_set)) return false;
            if (henka_gltf_find_member(extension, extension_end, "thicknessFactor", &value, &value_end) &&
                !henka_gltf_member_float(extension, extension_end, "thicknessFactor", &out_source->material.thickness)) return false;
            if (henka_gltf_find_member(extension, extension_end, "attenuationDistance", &value, &value_end) &&
                !henka_gltf_member_float(extension, extension_end, "attenuationDistance", &out_source->material.attenuation_distance)) return false;
            if (henka_gltf_find_member(extension, extension_end, "attenuationColor", &value, &value_end) &&
                !henka_gltf_member_vec3(extension, extension_end, "attenuationColor", &out_source->material.attenuation_color)) return false;
        }
        if (henka_gltf_find_member(extensions, extensions_end, "KHR_materials_specular", &extension, &extension_end))
        {
            if (henka_gltf_find_member(extension, extension_end, "specularFactor", &value, &value_end) &&
                !henka_gltf_member_float(extension, extension_end, "specularFactor", &out_source->material.specular_factor)) return false;
            if (henka_gltf_find_member(extension, extension_end, "specularColorFactor", &value, &value_end) &&
                !henka_gltf_member_vec3(extension, extension_end, "specularColorFactor", &out_source->material.specular_color)) return false;
        }
        if (henka_gltf_find_member(extensions, extensions_end, "KHR_materials_clearcoat", &extension, &extension_end))
        {
            if (henka_gltf_find_member(extension, extension_end, "clearcoatFactor", &value, &value_end) &&
                !henka_gltf_member_float(extension, extension_end, "clearcoatFactor", &out_source->material.clearcoat)) return false;
            if (henka_gltf_find_member(extension, extension_end, "clearcoatRoughnessFactor", &value, &value_end) &&
                !henka_gltf_member_float(extension, extension_end, "clearcoatRoughnessFactor", &out_source->material.clearcoat_roughness)) return false;
        }
        if (henka_gltf_find_member(extensions, extensions_end, "KHR_materials_sheen", &extension, &extension_end))
        {
            if (henka_gltf_find_member(extension, extension_end, "sheenColorFactor", &value, &value_end) &&
                !henka_gltf_member_vec3(extension, extension_end, "sheenColorFactor", &out_source->material.sheen_color)) return false;
            if (henka_gltf_find_member(extension, extension_end, "sheenRoughnessFactor", &value, &value_end) &&
                !henka_gltf_member_float(extension, extension_end, "sheenRoughnessFactor", &out_source->material.sheen_roughness)) return false;
        }
        if (henka_gltf_find_member(extensions, extensions_end, "KHR_materials_emissive_strength", &extension, &extension_end) &&
            henka_gltf_find_member(extension, extension_end, "emissiveStrength", &value, &value_end) &&
            !henka_gltf_member_float(extension, extension_end, "emissiveStrength", &out_source->material.emissive_strength)) return false;
    }

    return true;
}

static henka_result henka_gltf_parse_context(henka_gltf_context* context, henka_model_data* out_model)
{
    const char* meshes; const char* meshes_end; const char* mesh; const char* mesh_end; const char* primitives; const char* primitives_end; size_t primitive_index; henka_gltf_builder builder;
    bool first_has_material; int first_material_index;
    memset(&builder, 0, sizeof(builder));
    if (!henka_gltf_validate_extensions(context) || !henka_gltf_parse_buffers(context) ||
        !henka_gltf_parse_views(context) || !henka_gltf_parse_accessors(context) ||
        !henka_gltf_find_member(context->json, context->json + context->json_size, "meshes", &meshes, &meshes_end) ||
        !henka_gltf_array_item(meshes, meshes_end, 0U, &mesh, &mesh_end) ||
        !henka_gltf_find_member(mesh, mesh_end, "primitives", &primitives, &primitives_end))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!henka_gltf_array_item(primitives, primitives_end, 0U, &mesh, &mesh_end) ||
        !henka_gltf_primitive_material_index(mesh, mesh_end, &first_has_material, &first_material_index))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (first_has_material)
    {
        if (!henka_gltf_parse_material(context, mesh, mesh_end, &out_model->material_source))
        {
            henka_model_data_destroy(out_model);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        out_model->has_material = true;
    }
    for (primitive_index = 0U; primitive_index < HENKA_MAX_GLTF_ARRAY_ITEMS && henka_gltf_array_item(primitives, primitives_end, primitive_index, &mesh, &mesh_end); ++primitive_index)
    {
        bool has_material; int material_index;
        if (!henka_gltf_primitive_material_index(mesh, mesh_end, &has_material, &material_index) ||
            has_material != first_has_material || (has_material && material_index != first_material_index))
        {
            henka_free(builder.vertices);
            henka_model_data_destroy(out_model);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (!henka_gltf_parse_primitive(context, mesh, mesh_end, &builder))
        {
            henka_free(builder.vertices);
            henka_model_data_destroy(out_model);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    if (builder.count == 0U || builder.count > HENKA_MAX_MESH_ELEMENTS || builder.count % 3U != 0U)
    {
        henka_free(builder.vertices);
        henka_model_data_destroy(out_model);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    out_model->vertices = builder.vertices;
    out_model->vertex_count = (uint32_t)builder.count;
    out_model->indices = henka_malloc(builder.count * sizeof(*out_model->indices));
    if (out_model->indices == NULL)
    {
        henka_model_data_destroy(out_model);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (primitive_index = 0U; primitive_index < builder.count; ++primitive_index) out_model->indices[primitive_index] = (uint32_t)primitive_index;
    out_model->index_count = (uint32_t)builder.count;
    return HENKA_SUCCESS;
}

static bool henka_gltf_array_float_values(
    const char* array,
    const char* array_end,
    float* output,
    size_t value_count)
{
    const char* value;
    const char* value_end;
    double number;
    size_t index;

    if (array == NULL || array_end == NULL || output == NULL || value_count == 0U) return false;
    for (index = 0U; index < value_count; ++index)
    {
        if (!henka_gltf_array_item(array, array_end, index, &value, &value_end) ||
            !henka_gltf_number(value, value_end, &number) || number < -(double)FLT_MAX || number > (double)FLT_MAX)
        {
            return false;
        }
        output[index] = (float)number;
    }
    return true;
}

static bool henka_gltf_copy_optional_name(
    const char* object,
    const char* object_end,
    char** out_name)
{
    const char* value;
    const char* value_end;
    char name[HENKA_MAX_ASSET_PATH_BYTES];

    if (out_name != NULL) *out_name = NULL;
    if (object == NULL || object_end == NULL || out_name == NULL) return false;
    if (!henka_gltf_find_member(object, object_end, "name", &value, &value_end)) return true;
    if (!henka_gltf_read_string(value, value_end, name, sizeof(name), NULL)) return false;
    *out_name = henka_gltf_duplicate_string(name);
    return *out_name != NULL;
}

static bool henka_gltf_extension_supported(const char* extension)
{
    static const char* supported[] =
    {
        "KHR_lights_punctual",
        "KHR_texture_basisu",
        "KHR_materials_ior",
        "KHR_materials_transmission",
        "KHR_materials_specular",
        "KHR_materials_clearcoat",
        "KHR_materials_sheen",
        "KHR_materials_emissive_strength",
        "KHR_materials_volume"
    };
    size_t index;
    if (extension == NULL) return false;
    for (index = 0U; index < sizeof(supported) / sizeof(supported[0]); ++index)
        if (strcmp(extension, supported[index]) == 0) return true;
    return false;
}

static bool henka_gltf_validate_extensions(const henka_gltf_context* context)
{
    const char* array;
    const char* array_end;
    const char* value;
    const char* value_end;
    size_t set_index;
    size_t extension_index;
    char extension[HENKA_MAX_ASSET_PATH_BYTES];

    if (context == NULL) return false;
    for (set_index = 0U; set_index < 2U; ++set_index)
    {
        const char* member = set_index == 0U ? "extensionsUsed" : "extensionsRequired";
        if (!henka_gltf_find_member(context->json, context->json + context->json_size,
            member, &array, &array_end)) continue;
        for (extension_index = 0U;
            henka_gltf_array_item(array, array_end, extension_index, &value, &value_end);
            ++extension_index)
        {
            if (!henka_gltf_read_string(value, value_end, extension, sizeof(extension), NULL) ||
                !henka_gltf_extension_supported(extension)) return false;
        }
        if (henka_gltf_array_item(array, array_end, extension_index, &value, &value_end)) return false;
    }
    return true;
}

static bool henka_gltf_parse_scene_materials(
    const henka_gltf_context* context,
    henka_model_scene_data* scene,
    const char* primitive,
    const char* primitive_end,
    int material_index)
{
    if (material_index < 0) return true;
    if ((size_t)material_index >= HENKA_MODEL_MAX_SCENE_ITEMS) return false;
    if (scene->material_present[material_index]) return true;
    scene->material_present[material_index] = true;
    if ((size_t)(material_index + 1) > scene->material_count) scene->material_count = (size_t)material_index + 1U;
    if (!henka_gltf_parse_material(context, primitive, primitive_end, &scene->materials[material_index])) return false;
    return true;
}

static bool henka_gltf_parse_scene_meshes(
    const henka_gltf_context* context,
    henka_model_scene_data* scene)
{
    const char* meshes;
    const char* meshes_end;
    const char* mesh;
    const char* mesh_end;
    const char* primitives;
    const char* primitives_end;
    size_t mesh_index;

    /* A valid glTF scene may contain only cameras, lights, or empty node
     * structure. Geometry is optional at the scene level; when meshes are
     * present, every mesh still must contain at least one bounded primitive. */
    if (!henka_gltf_find_member(context->json, context->json + context->json_size, "meshes", &meshes, &meshes_end)) return true;
    if (henka_gltf_skip_space(meshes, meshes_end) >= meshes_end ||
        *henka_gltf_skip_space(meshes, meshes_end) != '[') return false;
    if (!henka_gltf_array_item(meshes, meshes_end, 0U, &mesh, &mesh_end)) return true;
    for (mesh_index = 0U; mesh_index < HENKA_MODEL_MAX_SCENE_ITEMS &&
        henka_gltf_array_item(meshes, meshes_end, mesh_index, &mesh, &mesh_end); ++mesh_index)
    {
        size_t primitive_index;
        if (!henka_gltf_find_member(mesh, mesh_end, "primitives", &primitives, &primitives_end)) return false;
        for (primitive_index = 0U; primitive_index < HENKA_MODEL_MAX_SCENE_ITEMS &&
            henka_gltf_array_item(primitives, primitives_end, primitive_index, &mesh, &mesh_end); ++primitive_index)
        {
            henka_gltf_builder builder;
            henka_model_scene_primitive* output;
            bool has_material;
            int material_index;
            size_t vertex_index;

            if (scene->primitive_count >= HENKA_MODEL_MAX_SCENE_ITEMS ||
                !henka_gltf_primitive_material_index(mesh, mesh_end, &has_material, &material_index))
            {
                return false;
            }
            memset(&builder, 0, sizeof(builder));
            if (!henka_gltf_parse_primitive(context, mesh, mesh_end, &builder))
            {
                henka_free(builder.vertices);
                return false;
            }
            output = &scene->primitives[scene->primitive_count];
            output->vertices = builder.vertices;
            output->vertex_count = (uint32_t)builder.count;
            output->indices = henka_malloc(builder.count * sizeof(*output->indices));
            if (output->indices == NULL)
            {
                henka_free(builder.vertices);
                return false;
            }
            for (vertex_index = 0U; vertex_index < builder.count; ++vertex_index)
                output->indices[vertex_index] = (uint32_t)vertex_index;
            output->index_count = (uint32_t)builder.count;
            output->mesh_index = (uint32_t)mesh_index;
            output->material_index = has_material ? material_index : -1;
            scene->primitive_count += 1U;
            if (!henka_gltf_parse_scene_materials(context, scene, mesh, mesh_end, output->material_index)) return false;
        }
        if (primitive_index == 0U) return false;
        if (henka_gltf_array_item(primitives, primitives_end, HENKA_MODEL_MAX_SCENE_ITEMS, &mesh, &mesh_end)) return false;
    }
    return mesh_index > 0U && !henka_gltf_array_item(meshes, meshes_end, HENKA_MODEL_MAX_SCENE_ITEMS, &mesh, &mesh_end);
}

static bool henka_gltf_parse_scene_cameras(
    const henka_gltf_context* context,
    henka_model_scene_data* scene)
{
    const char* cameras;
    const char* cameras_end;
    const char* camera;
    const char* camera_end;
    size_t index;

    if (!henka_gltf_find_member(context->json, context->json + context->json_size, "cameras", &cameras, &cameras_end)) return true;
    for (index = 0U; index < HENKA_MODEL_MAX_SCENE_ITEMS &&
        henka_gltf_array_item(cameras, cameras_end, index, &camera, &camera_end); ++index)
    {
        const char* type_value;
        const char* type_end;
        char type[24];
        const char* projection;
        const char* projection_end;
        float yfov = 60.0f * HENKA_DEG_TO_RAD;
        float aspect = 1.0f;
        float znear = 0.1f;
        float zfar = 10000.0f;
        float xmag = 1.0f;
        float ymag = 1.0f;
        float number;

        if (scene->camera_count != index) return false;
        scene->camera_count = index + 1U;
        if (!henka_gltf_copy_optional_name(camera, camera_end, &scene->cameras[index].name) ||
            !henka_gltf_find_member(camera, camera_end, "type", &type_value, &type_end) ||
            !henka_gltf_read_string(type_value, type_end, type, sizeof(type), NULL)) return false;
        if (strcmp(type, "perspective") == 0)
        {
            if (!henka_gltf_find_member(camera, camera_end, "perspective", &projection, &projection_end) ||
                !henka_gltf_member_float(projection, projection_end, "yfov", &yfov) ||
                !henka_gltf_member_float(projection, projection_end, "znear", &znear)) return false;
            if (henka_gltf_find_member(projection, projection_end, "aspectRatio", &type_value, &type_end) &&
                !henka_gltf_member_float(projection, projection_end, "aspectRatio", &aspect)) return false;
            if (henka_gltf_find_member(projection, projection_end, "zfar", &type_value, &type_end) &&
                !henka_gltf_member_float(projection, projection_end, "zfar", &zfar)) return false;
            if (!isfinite(yfov) || yfov <= 0.001f || yfov >= HENKA_PI - 0.001f ||
                !isfinite(aspect) || aspect <= 0.0f || !isfinite(znear) || znear <= 0.0f ||
                !isfinite(zfar) || zfar <= znear) return false;
            scene->cameras[index].camera = henka_camera_create_perspective(yfov, aspect, znear, zfar);
        }
        else if (strcmp(type, "orthographic") == 0)
        {
            if (!henka_gltf_find_member(camera, camera_end, "orthographic", &projection, &projection_end) ||
                !henka_gltf_member_float(projection, projection_end, "xmag", &xmag) ||
                !henka_gltf_member_float(projection, projection_end, "ymag", &ymag) ||
                !henka_gltf_member_float(projection, projection_end, "znear", &znear) ||
                !henka_gltf_member_float(projection, projection_end, "zfar", &zfar)) return false;
            number = ymag * 2.0f;
            if (!isfinite(xmag) || !isfinite(ymag) || xmag <= 0.0f || ymag <= 0.0f ||
                !isfinite(znear) || znear <= 0.0f || !isfinite(zfar) || zfar <= znear) return false;
            scene->cameras[index].camera = henka_camera_create_orthographic(number, xmag / ymag, znear, zfar);
        }
        else return false;
    }
    return !henka_gltf_array_item(cameras, cameras_end, HENKA_MODEL_MAX_SCENE_ITEMS, &camera, &camera_end);
}

static bool henka_gltf_parse_scene_lights(
    const henka_gltf_context* context,
    henka_model_scene_data* scene)
{
    const char* root_extensions;
    const char* root_extensions_end;
    const char* punctual;
    const char* punctual_end;
    const char* lights;
    const char* lights_end;
    const char* light;
    const char* light_end;
    size_t index;

    if (!henka_gltf_find_member(context->json, context->json + context->json_size, "extensions", &root_extensions, &root_extensions_end) ||
        !henka_gltf_find_member(root_extensions, root_extensions_end, "KHR_lights_punctual", &punctual, &punctual_end) ||
        !henka_gltf_find_member(punctual, punctual_end, "lights", &lights, &lights_end)) return true;
    for (index = 0U; index < HENKA_MODEL_MAX_SCENE_ITEMS &&
        henka_gltf_array_item(lights, lights_end, index, &light, &light_end); ++index)
    {
        const char* value;
        const char* value_end;
        char type[20];
        henka_model_scene_light* output = &scene->lights[index];
        henka_vec3 color = {1.0f, 1.0f, 1.0f};
        float intensity = 1.0f;
        /* Henka local lights are bounded; glTF's omitted range maps to a large finite range. */
        float range = 10000.0f;
        float inner = 0.0f;
        float outer = HENKA_PI * 0.25f;
        const char* spot;
        const char* spot_end;

        if (scene->light_count != index) return false;
        scene->light_count = index + 1U;
        if (!henka_gltf_copy_optional_name(light, light_end, &output->name) ||
            !henka_gltf_find_member(light, light_end, "type", &value, &value_end) ||
            !henka_gltf_read_string(value, value_end, type, sizeof(type), NULL)) return false;
        if (henka_gltf_find_member(light, light_end, "color", &value, &value_end) &&
            !henka_gltf_member_vec3(light, light_end, "color", &color)) return false;
        if (henka_gltf_find_member(light, light_end, "intensity", &value, &value_end) &&
            !henka_gltf_member_float(light, light_end, "intensity", &intensity)) return false;
        if (henka_gltf_find_member(light, light_end, "range", &value, &value_end) &&
            !henka_gltf_member_float(light, light_end, "range", &range)) return false;
        if (strcmp(type, "point") == 0) output->type = HENKA_MODEL_SCENE_LIGHT_POINT;
        else if (strcmp(type, "spot") == 0)
        {
            output->type = HENKA_MODEL_SCENE_LIGHT_SPOT;
            if (!henka_gltf_find_member(light, light_end, "spot", &spot, &spot_end)) return false;
            if (henka_gltf_find_member(spot, spot_end, "innerConeAngle", &value, &value_end) &&
                !henka_gltf_member_float(spot, spot_end, "innerConeAngle", &inner)) return false;
            if (henka_gltf_find_member(spot, spot_end, "outerConeAngle", &value, &value_end) &&
                !henka_gltf_member_float(spot, spot_end, "outerConeAngle", &outer)) return false;
        }
        else if (strcmp(type, "directional") == 0) output->type = HENKA_MODEL_SCENE_LIGHT_DIRECTIONAL;
        else return false;
        if (!isfinite(color.x) || !isfinite(color.y) || !isfinite(color.z) || color.x < 0.0f || color.y < 0.0f || color.z < 0.0f ||
            !isfinite(intensity) || intensity < 0.0f || !isfinite(range) || range < 0.0f ||
            !isfinite(inner) || !isfinite(outer) || inner < 0.0f || outer <= inner || outer > HENKA_PI * 0.5f) return false;
        output->color = color;
        output->intensity = intensity;
        output->range = range;
        output->inner_cone_cosine = cosf(inner);
        output->outer_cone_cosine = cosf(outer);
    }
    return !henka_gltf_array_item(lights, lights_end, HENKA_MODEL_MAX_SCENE_ITEMS, &light, &light_end);
}

static bool henka_gltf_matrix_to_transform(
    const henka_mat4* matrix,
    henka_transform* out_transform)
{
    henka_transform transform = henka_transform_identity();
    henka_vec3 column_x;
    henka_vec3 column_y;
    henka_vec3 column_z;
    henka_vec3 normalized_x;
    henka_vec3 normalized_y;
    henka_vec3 normalized_z;
    float sx;
    float sy;
    float sz;
    float determinant;
    float trace;
    int index;

    if (out_transform != NULL) *out_transform = transform;
    if (matrix == NULL || out_transform == NULL) return false;
    for (index = 0; index < 16; ++index)
        if (!isfinite(matrix->m[index])) return false;
    if (fabsf(matrix->m[3]) > 0.0001f || fabsf(matrix->m[7]) > 0.0001f ||
        fabsf(matrix->m[11]) > 0.0001f || fabsf(matrix->m[15] - 1.0f) > 0.0001f)
        return false;
    column_x = (henka_vec3){matrix->m[0], matrix->m[1], matrix->m[2]};
    column_y = (henka_vec3){matrix->m[4], matrix->m[5], matrix->m[6]};
    column_z = (henka_vec3){matrix->m[8], matrix->m[9], matrix->m[10]};
    sx = henka_vec3_length(column_x);
    sy = henka_vec3_length(column_y);
    sz = henka_vec3_length(column_z);
    if (!isfinite(sx) || !isfinite(sy) || !isfinite(sz) ||
        sx <= FLT_EPSILON || sy <= FLT_EPSILON || sz <= FLT_EPSILON)
        return false;
    determinant = henka_vec3_dot(henka_vec3_cross(column_x, column_y), column_z);
    if (!isfinite(determinant) || fabsf(determinant) <= FLT_EPSILON) return false;
    if (determinant < 0.0f) sx = -sx;
    normalized_x = henka_vec3_scale(column_x, 1.0f / sx);
    normalized_y = henka_vec3_scale(column_y, 1.0f / sy);
    normalized_z = henka_vec3_scale(column_z, 1.0f / sz);
    if (fabsf(henka_vec3_dot(normalized_x, normalized_y)) > 0.001f ||
        fabsf(henka_vec3_dot(normalized_x, normalized_z)) > 0.001f ||
        fabsf(henka_vec3_dot(normalized_y, normalized_z)) > 0.001f)
        return false;
    transform.position = (henka_vec3){matrix->m[12], matrix->m[13], matrix->m[14]};
    transform.scale = (henka_vec3){sx, sy, sz};
    trace = normalized_x.x + normalized_y.y + normalized_z.z;
    if (trace > 0.0f)
    {
        float s = sqrtf(trace + 1.0f) * 2.0f;
        transform.rotation.w = 0.25f * s;
        transform.rotation.x = (normalized_y.z - normalized_z.y) / s;
        transform.rotation.y = (normalized_z.x - normalized_x.z) / s;
        transform.rotation.z = (normalized_x.y - normalized_y.x) / s;
    }
    else if (normalized_x.x > normalized_y.y && normalized_x.x > normalized_z.z)
    {
        float s = sqrtf(1.0f + normalized_x.x - normalized_y.y - normalized_z.z) * 2.0f;
        transform.rotation.w = (normalized_y.z - normalized_z.y) / s;
        transform.rotation.x = 0.25f * s;
        transform.rotation.y = (normalized_y.x + normalized_x.y) / s;
        transform.rotation.z = (normalized_z.x + normalized_x.z) / s;
    }
    else if (normalized_y.y > normalized_z.z)
    {
        float s = sqrtf(1.0f + normalized_y.y - normalized_x.x - normalized_z.z) * 2.0f;
        transform.rotation.w = (normalized_z.x - normalized_x.z) / s;
        transform.rotation.x = (normalized_y.x + normalized_x.y) / s;
        transform.rotation.y = 0.25f * s;
        transform.rotation.z = (normalized_z.y + normalized_y.z) / s;
    }
    else
    {
        float s = sqrtf(1.0f + normalized_z.z - normalized_x.x - normalized_y.y) * 2.0f;
        transform.rotation.w = (normalized_x.y - normalized_y.x) / s;
        transform.rotation.x = (normalized_z.x + normalized_x.z) / s;
        transform.rotation.y = (normalized_z.y + normalized_y.z) / s;
        transform.rotation.z = 0.25f * s;
    }
    transform.rotation = henka_quat_normalize(transform.rotation);
    if (!isfinite(transform.rotation.x) || !isfinite(transform.rotation.y) ||
        !isfinite(transform.rotation.z) || !isfinite(transform.rotation.w)) return false;
    *out_transform = transform;
    return true;
}

static bool henka_gltf_compute_scene_world_matrix(
    henka_model_scene_data* scene,
    size_t node_index,
    unsigned char* state)
{
    henka_model_scene_node* node;
    if (node_index >= scene->node_count || state[node_index] == 1U) return false;
    if (state[node_index] == 2U) return true;
    state[node_index] = 1U;
    node = &scene->nodes[node_index];
    if (node->parent_index < 0) node->world_matrix = node->local_matrix;
    else if ((size_t)node->parent_index >= scene->node_count ||
        !henka_gltf_compute_scene_world_matrix(scene, (size_t)node->parent_index, state)) return false;
    else node->world_matrix = henka_mat4_multiply(scene->nodes[node->parent_index].world_matrix, node->local_matrix);
    if (!henka_gltf_matrix_to_transform(&node->world_matrix, &node->world_transform)) return false;
    state[node_index] = 2U;
    return true;
}

static bool henka_gltf_parse_scene_nodes(
    const henka_gltf_context* context,
    henka_model_scene_data* scene)
{
    const char* meshes;
    const char* meshes_end;
    const char* nodes;
    const char* nodes_end;
    const char* node;
    const char* node_end;
    const char* value;
    const char* value_end;
    size_t index;
    size_t mesh_count = 0U;
    unsigned char state[HENKA_MODEL_MAX_SCENE_ITEMS];

    if (!henka_gltf_find_member(context->json, context->json + context->json_size, "nodes", &nodes, &nodes_end)) return true;
    if (henka_gltf_find_member(context->json, context->json + context->json_size, "meshes", &meshes, &meshes_end))
    {
        while (mesh_count < HENKA_MODEL_MAX_SCENE_ITEMS &&
            henka_gltf_array_item(meshes, meshes_end, mesh_count, &value, &value_end)) ++mesh_count;
        if (henka_gltf_array_item(meshes, meshes_end, HENKA_MODEL_MAX_SCENE_ITEMS, &value, &value_end)) return false;
    }
    for (index = 0U; index < HENKA_MODEL_MAX_SCENE_ITEMS &&
        henka_gltf_array_item(nodes, nodes_end, index, &node, &node_end); ++index)
    {
        henka_model_scene_node* output = &scene->nodes[index];
        const char* matrix;
        const char* matrix_end;
        float matrix_values[16];
        bool has_matrix;
        bool has_trs;
        henka_vec4 rotation_values;
        henka_quat rotation;
        float rotation_length;

        memset(output, 0, sizeof(*output));
        output->parent_index = -1;
        output->mesh_index = -1;
        output->camera_index = -1;
        output->light_index = -1;
        scene->node_count = index + 1U;
        output->local_transform = henka_transform_identity();
        output->local_matrix = henka_mat4_identity();
        if (!henka_gltf_copy_optional_name(node, node_end, &output->name)) return false;
        has_matrix = henka_gltf_find_member(node, node_end, "matrix", &matrix, &matrix_end);
        has_trs = henka_gltf_find_member(node, node_end, "translation", &value, &value_end) ||
            henka_gltf_find_member(node, node_end, "rotation", &value, &value_end) ||
            henka_gltf_find_member(node, node_end, "scale", &value, &value_end);
        if (has_matrix && has_trs) return false;
        if (henka_gltf_find_member(node, node_end, "translation", &value, &value_end) &&
            !henka_gltf_member_vec3(node, node_end, "translation", &output->local_transform.position)) return false;
        if (henka_gltf_find_member(node, node_end, "rotation", &value, &value_end) &&
            !henka_gltf_member_vec4(node, node_end, "rotation", &rotation_values)) return false;
        else rotation_values = (henka_vec4){0.0f, 0.0f, 0.0f, 1.0f};
        rotation = (henka_quat){rotation_values.x, rotation_values.y, rotation_values.z, rotation_values.w};
        rotation_length = sqrtf(rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z + rotation.w * rotation.w);
        if (!isfinite(rotation_length) || rotation_length <= FLT_EPSILON) return false;
        output->local_transform.rotation = henka_quat_normalize(rotation);
        if (henka_gltf_find_member(node, node_end, "scale", &value, &value_end) &&
            !henka_gltf_member_vec3(node, node_end, "scale", &output->local_transform.scale)) return false;
        if (!isfinite(output->local_transform.position.x) || !isfinite(output->local_transform.position.y) ||
            !isfinite(output->local_transform.position.z) || !isfinite(output->local_transform.scale.x) ||
            !isfinite(output->local_transform.scale.y) || !isfinite(output->local_transform.scale.z) ||
            output->local_transform.scale.x == 0.0f || output->local_transform.scale.y == 0.0f || output->local_transform.scale.z == 0.0f) return false;
        if (has_matrix)
        {
            henka_mat4 matrix_value;
            if (!henka_gltf_array_float_values(matrix, matrix_end, matrix_values, 16U)) return false;
            memcpy(output->local_matrix.m, matrix_values, sizeof(matrix_values));
            memcpy(matrix_value.m, matrix_values, sizeof(matrix_values));
            if (!henka_gltf_matrix_to_transform(&matrix_value, &output->local_transform)) return false;
        }
        else output->local_matrix = henka_transform_to_mat4(output->local_transform);
        if (henka_gltf_find_member(node, node_end, "mesh", &value, &value_end) &&
            (!henka_gltf_member_int(node, node_end, "mesh", &output->mesh_index) || output->mesh_index < 0)) return false;
        if (henka_gltf_find_member(node, node_end, "camera", &value, &value_end) &&
            (!henka_gltf_member_int(node, node_end, "camera", &output->camera_index) || output->camera_index < 0)) return false;
        if (output->camera_index >= 0 && (size_t)output->camera_index >= scene->camera_count) return false;
        if (output->mesh_index >= 0 && (size_t)output->mesh_index >= mesh_count) return false;
        if (henka_gltf_find_member(node, node_end, "extensions", &value, &value_end))
        {
            const char* punctual;
            const char* punctual_end;
            if (henka_gltf_find_member(value, value_end, "KHR_lights_punctual", &punctual, &punctual_end))
            {
                if (!henka_gltf_member_int(punctual, punctual_end, "light", &output->light_index) ||
                    output->light_index < 0 || (size_t)output->light_index >= scene->light_count) return false;
            }
        }
    }
    if (henka_gltf_array_item(nodes, nodes_end, HENKA_MODEL_MAX_SCENE_ITEMS, &node, &node_end)) return false;
    for (index = 0U; index < scene->node_count; ++index)
    {
        const char* children;
        const char* children_end;
        size_t child_index;
        if (!henka_gltf_array_item(nodes, nodes_end, index, &node, &node_end)) return false;
        if (!henka_gltf_find_member(node, node_end, "children", &children, &children_end)) continue;
        for (child_index = 0U; child_index < HENKA_MODEL_MAX_SCENE_ITEMS &&
            henka_gltf_array_item(children, children_end, child_index, &value, &value_end); ++child_index)
        {
            int child;
            if (!henka_gltf_integer(value, value_end, &child) || child < 0 || (size_t)child >= scene->node_count ||
                scene->nodes[child].parent_index >= 0) return false;
            scene->nodes[child].parent_index = (int)index;
        }
        if (henka_gltf_array_item(children, children_end, HENKA_MODEL_MAX_SCENE_ITEMS, &value, &value_end)) return false;
    }
    memset(state, 0, sizeof(state));
    for (index = 0U; index < scene->node_count; ++index)
        if (!henka_gltf_compute_scene_world_matrix(scene, index, state)) return false;
    return true;
}

static bool henka_gltf_parse_scene_selections(
    const henka_gltf_context* context,
    henka_model_scene_data* scene)
{
    const char* scenes;
    const char* scenes_end;
    const char* scene_object;
    const char* scene_end;
    const char* value;
    const char* value_end;
    size_t index;
    int default_scene = 0;

    if (henka_gltf_find_member(context->json, context->json + context->json_size, "scene", &value, &value_end) &&
        !henka_gltf_integer(value, value_end, &default_scene)) return false;
    if (!henka_gltf_find_member(context->json, context->json + context->json_size, "scenes", &scenes, &scenes_end))
    {
        if (default_scene != 0) return false;
        scene->scene_count = 1U;
        scene->active_scene_index = 0U;
        scene->scene_root_offsets[0] = 0U;
        for (index = 0U; index < scene->node_count; ++index)
            if (scene->nodes[index].parent_index < 0)
            {
                if (scene->scene_root_node_count >= HENKA_MODEL_MAX_SCENE_ITEMS) return false;
                scene->scene_root_nodes[scene->scene_root_node_count++] = (int)index;
            }
        scene->scene_root_counts[0] = scene->scene_root_node_count;
        return true;
    }
    for (index = 0U; index < HENKA_MODEL_MAX_SCENE_ITEMS &&
        henka_gltf_array_item(scenes, scenes_end, index, &scene_object, &scene_end); ++index)
    {
        const char* roots;
        const char* roots_end;
        size_t root_index;
        scene->scene_root_offsets[index] = scene->scene_root_node_count;
        if (henka_gltf_find_member(scene_object, scene_end, "nodes", &roots, &roots_end))
        {
            for (root_index = 0U; root_index < HENKA_MODEL_MAX_SCENE_ITEMS &&
                henka_gltf_array_item(roots, roots_end, root_index, &value, &value_end); ++root_index)
            {
                int root;
                if (scene->scene_root_node_count >= HENKA_MODEL_MAX_SCENE_ITEMS ||
                    !henka_gltf_integer(value, value_end, &root) || root < 0 || (size_t)root >= scene->node_count ||
                    scene->nodes[root].parent_index >= 0) return false;
                {
                    size_t previous_root;
                    for (previous_root = scene->scene_root_offsets[index];
                        previous_root < scene->scene_root_node_count; ++previous_root)
                        if (scene->scene_root_nodes[previous_root] == root) return false;
                }
                scene->scene_root_nodes[scene->scene_root_node_count++] = root;
            }
            if (henka_gltf_array_item(roots, roots_end, HENKA_MODEL_MAX_SCENE_ITEMS, &value, &value_end)) return false;
        }
        scene->scene_root_counts[index] = scene->scene_root_node_count - scene->scene_root_offsets[index];
        scene->scene_count += 1U;
    }
    if (henka_gltf_array_item(scenes, scenes_end, HENKA_MODEL_MAX_SCENE_ITEMS, &scene_object, &scene_end) ||
        scene->scene_count == 0U || default_scene < 0 || (size_t)default_scene >= scene->scene_count) return false;
    scene->active_scene_index = (size_t)default_scene;
    return true;
}

static henka_result henka_gltf_parse_scene_context(
    henka_gltf_context* context,
    henka_model_scene_data* out_scene)
{
    const char* meshes;
    const char* meshes_end;
    bool has_meshes;

    has_meshes = henka_gltf_find_member(
        context->json, context->json + context->json_size, "meshes", &meshes, &meshes_end) &&
        henka_gltf_array_item(meshes, meshes_end, 0U, &meshes, &meshes_end);
    if (!henka_gltf_validate_extensions(context) ||
        (has_meshes && (!henka_gltf_parse_buffers(context) ||
            !henka_gltf_parse_views(context) || !henka_gltf_parse_accessors(context))) ||
        !henka_gltf_parse_scene_cameras(context, out_scene) || !henka_gltf_parse_scene_lights(context, out_scene) ||
        !henka_gltf_parse_scene_meshes(context, out_scene) || !henka_gltf_parse_scene_nodes(context, out_scene) ||
        !henka_gltf_parse_scene_selections(context, out_scene)) return HENKA_ERROR_INVALID_ARGUMENT;
    return HENKA_SUCCESS;
}

static bool henka_gltf_prepare_json(
    const unsigned char* data,
    size_t data_size,
    henka_gltf_context* context)
{
    size_t offset = 0U;
    size_t json_size = 0U;
    const unsigned char* json_data = NULL;
    const char* parsed_end;
    if (data == NULL || data_size == 0U || data_size > HENKA_MAX_GLTF_SOURCE_BYTES || context == NULL) return false;
    if (data_size >= 12U && memcmp(data, "glTF", 4U) == 0)
    {
        uint32_t version; uint32_t total_length;
        bool json_seen = false;
        bool binary_seen = false;
        size_t chunk_index = 0U;
        memcpy(&version, data + 4U, sizeof(version)); memcpy(&total_length, data + 8U, sizeof(total_length));
        if (version != 2U || total_length != data_size || total_length < 12U) return false;
        offset = 12U;
        while (offset < total_length)
        {
            uint32_t chunk_length; uint32_t chunk_type;
            if (total_length - offset < 8U) return false;
            memcpy(&chunk_length, data + offset, sizeof(chunk_length)); memcpy(&chunk_type, data + offset + 4U, sizeof(chunk_type)); offset += 8U;
            if ((chunk_length & 3U) != 0U || chunk_length > total_length - offset) return false;
            if (chunk_index == 0U && chunk_type != 0x4E4F534AU) return false;
            if (chunk_type == 0x4E4F534AU)
            {
                if (json_seen || chunk_length == 0U) return false;
                json_seen = true;
                json_data = data + offset;
                json_size = chunk_length;
            }
            else if (chunk_type == 0x004E4942U)
            {
                if (binary_seen) return false;
                binary_seen = true;
                context->glb_binary = data + offset;
                context->glb_binary_size = chunk_length;
            }
            offset += chunk_length;
            ++chunk_index;
        }
        if (offset != total_length || !json_seen || json_data == NULL) return false;
    }
    else
    {
        json_data = data;
        json_size = data_size;
    }
    while (json_size > 0U && isspace(json_data[json_size - 1U])) json_size -= 1U;
    context->json = henka_malloc(json_size + 1U);
    if (context->json == NULL) return false;
    memcpy(context->json, json_data, json_size);
    context->json[json_size] = '\0';
    context->json_size = json_size;
    if (henka_gltf_skip_space(context->json, context->json + json_size) >= context->json + json_size ||
        *henka_gltf_skip_space(context->json, context->json + json_size) != '{' ||
        !henka_gltf_value_end(context->json, context->json + json_size, &parsed_end) ||
        henka_gltf_skip_space(parsed_end, context->json + json_size) != context->json + json_size ||
        !henka_gltf_version_is_supported(context))
    {
        return false;
    }
    return true;
}

henka_result henka_model_data_load_gltf_from_memory(
    const void* data,
    size_t data_size,
    const char* label,
    henka_model_data* out_model)
{
    henka_model_data candidate;
    henka_gltf_context context;
    henka_result result;
    (void)label;
    if (data == NULL || out_model == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    memset(&candidate, 0, sizeof(candidate));
    memset(&context, 0, sizeof(context));
    if (!henka_gltf_prepare_json((const unsigned char*)data, data_size, &context))
    {
        henka_gltf_context_destroy(&context);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_gltf_parse_context(&context, &candidate);
    henka_gltf_context_destroy(&context);
    if (result == HENKA_SUCCESS)
    {
        henka_model_data_destroy(out_model);
        *out_model = candidate;
        memset(&candidate, 0, sizeof(candidate));
    }
    henka_model_data_destroy(&candidate);
    return result;
}

henka_result henka_model_data_load_gltf(const char* path, henka_model_data* out_model)
{
    char* data; size_t data_size; henka_gltf_context context; henka_result result; const char* separator; size_t directory_length; henka_model_data candidate;
    if (path == NULL || out_model == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    memset(&candidate, 0, sizeof(candidate));
    data = henka_gltf_read_file(path, &data_size);
    if (data == NULL) return HENKA_ERROR_PLATFORM;
    memset(&context, 0, sizeof(context));
    separator = strrchr(path, '/');
    { const char* backslash = strrchr(path, '\\'); if (backslash != NULL && (separator == NULL || backslash > separator)) separator = backslash; }
    directory_length = separator == NULL ? 0U : (size_t)(separator - path);
    context.base_directory = henka_malloc(directory_length + 1U);
    if (context.base_directory == NULL) { henka_free(data); return HENKA_ERROR_OUT_OF_MEMORY; }
    memcpy(context.base_directory, path, directory_length); context.base_directory[directory_length] = '\0';
    if (!henka_gltf_prepare_json((const unsigned char*)data, data_size, &context)) result = HENKA_ERROR_INVALID_ARGUMENT;
    else result = henka_gltf_parse_context(&context, &candidate);
    henka_gltf_context_destroy(&context);
    henka_free(data);
    if (result == HENKA_SUCCESS)
    {
        henka_model_data_destroy(out_model);
        *out_model = candidate;
        memset(&candidate, 0, sizeof(candidate));
    }
    henka_model_data_destroy(&candidate);
    return result;
}

void henka_model_scene_data_destroy(henka_model_scene_data* scene)
{
    size_t index;

    if (scene == NULL) return;
    for (index = 0U; index < scene->primitive_count; ++index)
    {
        henka_free(scene->primitives[index].vertices);
        henka_free(scene->primitives[index].indices);
    }
    for (index = 0U; index < scene->material_count; ++index)
    {
        if (!scene->material_present[index]) continue;
        henka_free(scene->materials[index].name);
        henka_free(scene->materials[index].base_color_uri);
        henka_free(scene->materials[index].normal_uri);
        henka_free(scene->materials[index].metallic_roughness_uri);
        henka_free(scene->materials[index].occlusion_uri);
        henka_free(scene->materials[index].emissive_uri);
        henka_free(scene->materials[index].transmission_uri);
        henka_free(scene->materials[index].thickness_uri);
        henka_free(scene->materials[index].base_color_embedded_data);
        henka_free(scene->materials[index].normal_embedded_data);
        henka_free(scene->materials[index].metallic_roughness_embedded_data);
        henka_free(scene->materials[index].occlusion_embedded_data);
        henka_free(scene->materials[index].emissive_embedded_data);
        henka_free(scene->materials[index].transmission_embedded_data);
        henka_free(scene->materials[index].thickness_embedded_data);
    }
    for (index = 0U; index < scene->node_count; ++index) henka_free(scene->nodes[index].name);
    for (index = 0U; index < scene->camera_count; ++index) henka_free(scene->cameras[index].name);
    for (index = 0U; index < scene->light_count; ++index) henka_free(scene->lights[index].name);
    memset(scene, 0, sizeof(*scene));
}

henka_result henka_model_scene_data_load_gltf_from_memory(
    const void* data,
    size_t data_size,
    const char* label,
    henka_model_scene_data* out_scene)
{
    henka_model_scene_data candidate;
    henka_gltf_context context;
    henka_result result;
    (void)label;

    if (data == NULL || out_scene == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    memset(&candidate, 0, sizeof(candidate));
    memset(&context, 0, sizeof(context));
    if (!henka_gltf_prepare_json((const unsigned char*)data, data_size, &context))
    {
        henka_gltf_context_destroy(&context);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_gltf_parse_scene_context(&context, &candidate);
    henka_gltf_context_destroy(&context);
    if (result == HENKA_SUCCESS)
    {
        henka_model_scene_data_destroy(out_scene);
        *out_scene = candidate;
        memset(&candidate, 0, sizeof(candidate));
    }
    henka_model_scene_data_destroy(&candidate);
    return result;
}

henka_result henka_model_scene_data_set_active_scene(
    henka_model_scene_data* scene,
    size_t scene_index)
{
    if (scene == NULL || scene_index >= scene->scene_count) return HENKA_ERROR_INVALID_ARGUMENT;
    scene->active_scene_index = scene_index;
    return HENKA_SUCCESS;
}

henka_result henka_model_scene_data_load_gltf(
    const char* path,
    henka_model_scene_data* out_scene)
{
    char* data;
    size_t data_size;
    henka_gltf_context context;
    henka_result result;
    henka_model_scene_data candidate;
    const char* separator;
    size_t directory_length;

    if (path == NULL || out_scene == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    memset(&candidate, 0, sizeof(candidate));
    data = henka_gltf_read_file(path, &data_size);
    if (data == NULL) return HENKA_ERROR_PLATFORM;
    memset(&context, 0, sizeof(context));
    separator = strrchr(path, '/');
    {
        const char* backslash = strrchr(path, '\\');
        if (backslash != NULL && (separator == NULL || backslash > separator)) separator = backslash;
    }
    directory_length = separator == NULL ? 0U : (size_t)(separator - path);
    context.base_directory = henka_malloc(directory_length + 1U);
    if (context.base_directory == NULL)
    {
        henka_free(data);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    memcpy(context.base_directory, path, directory_length);
    context.base_directory[directory_length] = '\0';
    if (!henka_gltf_prepare_json((const unsigned char*)data, data_size, &context)) result = HENKA_ERROR_INVALID_ARGUMENT;
    else result = henka_gltf_parse_scene_context(&context, &candidate);
    henka_gltf_context_destroy(&context);
    henka_free(data);
    if (result == HENKA_SUCCESS)
    {
        henka_model_scene_data_destroy(out_scene);
        *out_scene = candidate;
        memset(&candidate, 0, sizeof(candidate));
    }
    henka_model_scene_data_destroy(&candidate);
    return result;
}

henka_result henka_mesh_create_from_gltf(
    henka_engine* engine,
    const char* path,
    henka_mesh** out_mesh)
{
    henka_model_data model;
    henka_result result;
    memset(&model, 0, sizeof(model));
    if (out_mesh != NULL) *out_mesh = NULL;
    if (engine == NULL || path == NULL || out_mesh == NULL) return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_model_data_load_gltf(path, &model);
    if (result == HENKA_SUCCESS) result = henka_mesh_create_from_model_data(engine, &model, out_mesh);
    henka_model_data_destroy(&model);
    return result;
}
