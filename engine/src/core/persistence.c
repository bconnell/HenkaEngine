#include <henka/persistence.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/log.h>
#include <henka/memory.h>

#include "../henka_internal.h"

typedef struct henka_settings_entry
{
    char* key;
    char* value;
} henka_settings_entry;

struct henka_settings
{
    henka_settings_entry* entries;
    size_t count;
    size_t capacity;
};

static bool henka_path_is_separator(char character)
{
    return character == '/' || character == '\\';
}

static bool henka_path_is_absolute(const char* path)
{
    if (path == NULL || path[0] == '\0')
    {
        return false;
    }

    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':')
    {
        return true;
    }

    if (henka_path_is_separator(path[0]) && henka_path_is_separator(path[1]))
    {
        return true;
    }

    return henka_path_is_separator(path[0]);
}

static char* henka_duplicate_string(const char* value)
{
    char* copy;
    size_t length;

    if (value == NULL)
    {
        return NULL;
    }

    length = strlen(value);
    copy = henka_malloc(length + 1U);
    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(copy, value, length + 1U);
    return copy;
}

static char* henka_duplicate_range(const char* start, size_t length)
{
    char* copy;

    copy = henka_malloc(length + 1U);
    if (copy == NULL)
    {
        return NULL;
    }

    if (length > 0U)
    {
        memcpy(copy, start, length);
    }

    copy[length] = '\0';
    return copy;
}

static char* henka_trimmed_copy(const char* value)
{
    const char* end;
    const char* start;

    if (value == NULL)
    {
        return NULL;
    }

    start = value;
    while (*start != '\0' && isspace((unsigned char)*start))
    {
        ++start;
    }

    end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1)))
    {
        --end;
    }

    return henka_duplicate_range(start, (size_t)(end - start));
}

static henka_settings_entry* henka_settings_find_entry(const henka_settings* settings, const char* key)
{
    size_t index;

    if (settings == NULL || key == NULL)
    {
        return NULL;
    }

    for (index = 0U; index < settings->count; ++index)
    {
        if (strcmp(settings->entries[index].key, key) == 0)
        {
            return &settings->entries[index];
        }
    }

    return NULL;
}

static henka_result henka_settings_ensure_capacity(henka_settings* settings)
{
    henka_settings_entry* entries;
    size_t new_capacity;

    if (settings == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (settings->count < settings->capacity)
    {
        return HENKA_SUCCESS;
    }

    new_capacity = settings->capacity == 0U ? 8U : settings->capacity * 2U;
    entries = henka_realloc(settings->entries, new_capacity * sizeof(*entries));
    if (entries == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    settings->entries = entries;
    settings->capacity = new_capacity;
    return HENKA_SUCCESS;
}

henka_result henka_path_resolve(const char* base_path, const char* relative_path, char** out_path)
{
    char* resolved_path;
    size_t base_length;
    size_t relative_length;
    bool needs_separator;

    if (relative_path == NULL || out_path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_path = NULL;

    if (base_path == NULL || base_path[0] == '\0' || henka_path_is_absolute(relative_path))
    {
        resolved_path = henka_duplicate_string(relative_path);
        if (resolved_path == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }

        *out_path = resolved_path;
        return HENKA_SUCCESS;
    }

    base_length = strlen(base_path);
    relative_length = strlen(relative_path);

    while (base_length > 0U && henka_path_is_separator(base_path[base_length - 1U]))
    {
        --base_length;
    }

    needs_separator = base_length > 0U && !henka_path_is_separator(relative_path[0]);
    resolved_path = henka_malloc(base_length + relative_length + (needs_separator ? 2U : 1U));
    if (resolved_path == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    memcpy(resolved_path, base_path, base_length);
    if (needs_separator)
    {
        resolved_path[base_length] = '/';
        memcpy(resolved_path + base_length + 1U, relative_path, relative_length + 1U);
    }
    else
    {
        memcpy(resolved_path + base_length, relative_path, relative_length + 1U);
    }

    *out_path = resolved_path;
    return HENKA_SUCCESS;
}

henka_result henka_path_parent_directory(const char* path, char** out_parent_directory)
{
    const char* cursor;
    size_t length;

    if (path == NULL || out_parent_directory == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_parent_directory = NULL;
    length = strlen(path);
    if (length == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    cursor = path + length;
    while (cursor > path && !henka_path_is_separator(*(cursor - 1)))
    {
        --cursor;
    }

    if (cursor == path)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    while (cursor > path && henka_path_is_separator(*(cursor - 1)))
    {
        --cursor;
    }

    *out_parent_directory = henka_duplicate_range(path, (size_t)(cursor - path));
    if (*out_parent_directory == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    return HENKA_SUCCESS;
}

henka_result henka_settings_create(henka_settings** out_settings)
{
    henka_settings* settings;

    if (out_settings == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_settings = NULL;
    settings = henka_calloc(1U, sizeof(*settings));
    if (settings == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    *out_settings = settings;
    return HENKA_SUCCESS;
}

void henka_settings_destroy(henka_settings* settings)
{
    size_t index;

    if (settings == NULL)
    {
        return;
    }

    for (index = 0U; index < settings->count; ++index)
    {
        henka_free(settings->entries[index].key);
        henka_free(settings->entries[index].value);
    }

    henka_free(settings->entries);
    henka_free(settings);
}

henka_result henka_settings_load_file(henka_settings* settings, const char* path)
{
    FILE* file;
    char line[1024];
    bool had_parse_error;
    unsigned int line_number;

    if (settings == NULL || path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    file = NULL;
    if (fopen_s(&file, path, "r") != 0 || file == NULL)
    {
        HENKA_LOG_WARN("Settings file could not be opened: %s", path);
        return HENKA_ERROR_UNKNOWN;
    }

    had_parse_error = false;
    line_number = 0U;

    while (fgets(line, (int)sizeof(line), file) != NULL)
    {
        char* equals;
        char* key;
        char* value;
        char* trimmed_line;
        size_t line_length;

        ++line_number;
        line_length = strlen(line);
        if (line_length > 0U && line[line_length - 1U] != '\n' && !feof(file))
        {
            HENKA_LOG_WARN("Skipping overlong settings line %u in %s", line_number, path);
            had_parse_error = true;

            do
            {
                if (fgets(line, (int)sizeof(line), file) == NULL)
                {
                    break;
                }
                line_length = strlen(line);
            } while (line_length == 0U || line[line_length - 1U] != '\n');

            continue;
        }

        trimmed_line = henka_trimmed_copy(line);
        if (trimmed_line == NULL)
        {
            fclose(file);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }

        if (trimmed_line[0] == '\0' || trimmed_line[0] == '#' || trimmed_line[0] == ';')
        {
            henka_free(trimmed_line);
            continue;
        }

        equals = strchr(trimmed_line, '=');
        if (equals == NULL)
        {
            HENKA_LOG_WARN("Skipping malformed settings line %u in %s", line_number, path);
            henka_free(trimmed_line);
            had_parse_error = true;
            continue;
        }

        *equals = '\0';
        key = henka_trimmed_copy(trimmed_line);
        value = henka_trimmed_copy(equals + 1);
        henka_free(trimmed_line);

        if (key == NULL || value == NULL)
        {
            henka_free(key);
            henka_free(value);
            fclose(file);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }

        if (key[0] == '\0')
        {
            HENKA_LOG_WARN("Skipping settings line %u with an empty key in %s", line_number, path);
            henka_free(key);
            henka_free(value);
            had_parse_error = true;
            continue;
        }

        if (henka_settings_set_string(settings, key, value) != HENKA_SUCCESS)
        {
            henka_free(key);
            henka_free(value);
            fclose(file);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }

        henka_free(key);
        henka_free(value);
    }

    fclose(file);
    return had_parse_error ? HENKA_ERROR_UNKNOWN : HENKA_SUCCESS;
}

henka_result henka_settings_save_file(const henka_settings* settings, const char* path)
{
    FILE* file;
    char* directory_path;
    size_t index;
    henka_result result;

    if (settings == NULL || path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    directory_path = NULL;
    result = henka_path_parent_directory(path, &directory_path);
    if (result == HENKA_SUCCESS)
    {
        result = henka_platform_create_directory_tree(directory_path);
        henka_free(directory_path);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }

    file = NULL;
    if (fopen_s(&file, path, "w") != 0 || file == NULL)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    for (index = 0U; index < settings->count; ++index)
    {
        if (fprintf(file, "%s=%s\n", settings->entries[index].key, settings->entries[index].value) < 0)
        {
            fclose(file);
            return HENKA_ERROR_UNKNOWN;
        }
    }

    fclose(file);
    return HENKA_SUCCESS;
}

bool henka_settings_has_key(const henka_settings* settings, const char* key)
{
    return henka_settings_find_entry(settings, key) != NULL;
}

const char* henka_settings_get_string(const henka_settings* settings, const char* key, const char* default_value)
{
    henka_settings_entry* entry;

    entry = henka_settings_find_entry(settings, key);
    if (entry == NULL)
    {
        return default_value;
    }

    return entry->value;
}

int henka_settings_get_int(const henka_settings* settings, const char* key, int default_value)
{
    const char* value;
    char* end;
    long parsed;

    value = henka_settings_get_string(settings, key, NULL);
    if (value == NULL)
    {
        return default_value;
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0')
    {
        return default_value;
    }

    return (int)parsed;
}

float henka_settings_get_float(const henka_settings* settings, const char* key, float default_value)
{
    const char* value;
    char* end;
    float parsed;

    value = henka_settings_get_string(settings, key, NULL);
    if (value == NULL)
    {
        return default_value;
    }

    errno = 0;
    parsed = strtof(value, &end);
    if (errno != 0 || end == value || *end != '\0')
    {
        return default_value;
    }

    return parsed;
}

bool henka_settings_get_bool(const henka_settings* settings, const char* key, bool default_value)
{
    const char* value;

    value = henka_settings_get_string(settings, key, NULL);
    if (value == NULL)
    {
        return default_value;
    }

    if (_stricmp(value, "true") == 0 || strcmp(value, "1") == 0 || _stricmp(value, "yes") == 0 || _stricmp(value, "on") == 0)
    {
        return true;
    }

    if (_stricmp(value, "false") == 0 || strcmp(value, "0") == 0 || _stricmp(value, "no") == 0 || _stricmp(value, "off") == 0)
    {
        return false;
    }

    return default_value;
}

henka_result henka_settings_set_string(henka_settings* settings, const char* key, const char* value)
{
    henka_settings_entry* entry;
    char* key_copy;
    char* value_copy;
    henka_result result;

    if (settings == NULL || key == NULL || value == NULL || key[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    entry = henka_settings_find_entry(settings, key);
    value_copy = henka_duplicate_string(value);
    if (value_copy == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    if (entry != NULL)
    {
        henka_free(entry->value);
        entry->value = value_copy;
        return HENKA_SUCCESS;
    }

    result = henka_settings_ensure_capacity(settings);
    if (result != HENKA_SUCCESS)
    {
        henka_free(value_copy);
        return result;
    }

    key_copy = henka_duplicate_string(key);
    if (key_copy == NULL)
    {
        henka_free(value_copy);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    settings->entries[settings->count].key = key_copy;
    settings->entries[settings->count].value = value_copy;
    settings->count += 1U;
    return HENKA_SUCCESS;
}

henka_result henka_settings_set_int(henka_settings* settings, const char* key, int value)
{
    char buffer[32];

    if (sprintf_s(buffer, sizeof(buffer), "%d", value) < 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    return henka_settings_set_string(settings, key, buffer);
}

henka_result henka_settings_set_float(henka_settings* settings, const char* key, float value)
{
    char buffer[64];

    if (sprintf_s(buffer, sizeof(buffer), "%.6f", value) < 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    return henka_settings_set_string(settings, key, buffer);
}

henka_result henka_settings_set_bool(henka_settings* settings, const char* key, bool value)
{
    return henka_settings_set_string(settings, key, value ? "true" : "false");
}

henka_result henka_settings_remove(henka_settings* settings, const char* key)
{
    size_t index;

    if (settings == NULL || key == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < settings->count; ++index)
    {
        if (strcmp(settings->entries[index].key, key) == 0)
        {
            henka_free(settings->entries[index].key);
            henka_free(settings->entries[index].value);

            if (index + 1U < settings->count)
            {
                memmove(&settings->entries[index], &settings->entries[index + 1U], (settings->count - index - 1U) * sizeof(*settings->entries));
            }

            settings->count -= 1U;
            return HENKA_SUCCESS;
        }
    }

    return HENKA_SUCCESS;
}
