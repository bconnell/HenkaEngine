#include <henka/persistence.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>
#include <math.h>
#include <stdint.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

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

struct henka_save_data
{
    int version;
    char* scene_id;
    henka_vec3 camera_position;
    float camera_yaw_radians;
    float camera_pitch_radians;
    henka_settings* flags;
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

#define HENKA_SETTINGS_MAX_ENTRIES 4096U
#define HENKA_SETTINGS_MAX_KEY_LENGTH 127U
#define HENKA_SETTINGS_MAX_VALUE_LENGTH 767U
#define HENKA_SAVE_SLOT_MAX_LENGTH 64U
#define HENKA_PERSISTENCE_TEMP_SUFFIX ".henka-tmp"

static bool henka_ascii_equals_ignore_case(const char* left, size_t left_length, const char* right)
{
    size_t index;
    size_t right_length;

    if (left == NULL || right == NULL)
    {
        return false;
    }

    right_length = strlen(right);
    if (left_length != right_length)
    {
        return false;
    }

    for (index = 0U; index < left_length; ++index)
    {
        if (tolower((unsigned char)left[index]) != tolower((unsigned char)right[index]))
        {
            return false;
        }
    }

    return true;
}

static bool henka_windows_name_is_reserved(const char* name, size_t length)
{
    size_t base_length;
    size_t index;

    if (name == NULL || length == 0U)
    {
        return false;
    }

    base_length = length;
    for (index = 0U; index < length; ++index)
    {
        if (name[index] == '.')
        {
            base_length = index;
            break;
        }
    }

    if (henka_ascii_equals_ignore_case(name, base_length, "CON") ||
        henka_ascii_equals_ignore_case(name, base_length, "PRN") ||
        henka_ascii_equals_ignore_case(name, base_length, "AUX") ||
        henka_ascii_equals_ignore_case(name, base_length, "NUL"))
    {
        return true;
    }

    if (base_length == 4U &&
        tolower((unsigned char)name[0]) == 'c' &&
        tolower((unsigned char)name[1]) == 'o' &&
        tolower((unsigned char)name[2]) == 'm' &&
        name[3] >= '1' && name[3] <= '9')
    {
        return true;
    }

    if (base_length == 4U &&
        tolower((unsigned char)name[0]) == 'l' &&
        tolower((unsigned char)name[1]) == 'p' &&
        tolower((unsigned char)name[2]) == 't' &&
        name[3] >= '1' && name[3] <= '9')
    {
        return true;
    }

    return false;
}

static bool henka_settings_key_is_valid(const char* key)
{
    size_t index;
    size_t length;

    if (key == NULL)
    {
        return false;
    }

    length = strlen(key);
    if (length == 0U || length > HENKA_SETTINGS_MAX_KEY_LENGTH)
    {
        return false;
    }

    for (index = 0U; index < length; ++index)
    {
        unsigned char character = (unsigned char)key[index];
        if (!isalnum(character) && character != '.' && character != '_' && character != '-')
        {
            return false;
        }
    }

    return true;
}

static bool henka_settings_value_is_valid(const char* value)
{
    size_t index;
    size_t length;

    if (value == NULL)
    {
        return false;
    }

    length = strlen(value);
    if (length > HENKA_SETTINGS_MAX_VALUE_LENGTH)
    {
        return false;
    }

    for (index = 0U; index < length; ++index)
    {
        unsigned char character = (unsigned char)value[index];
        if (character < 0x20U || character == 0x7fU)
        {
            return false;
        }
    }

    return true;
}

static bool henka_save_slot_name_is_valid(const char* slot_name)
{
    size_t index;
    size_t length;

    if (slot_name == NULL)
    {
        return false;
    }

    length = strlen(slot_name);
    if (length == 0U || length > HENKA_SAVE_SLOT_MAX_LENGTH)
    {
        return false;
    }

    for (index = 0U; index < length; ++index)
    {
        unsigned char character = (unsigned char)slot_name[index];
        if (!isalnum(character) && character != '_' && character != '-')
        {
            return false;
        }
    }

    return !henka_windows_name_is_reserved(slot_name, length);
}

static bool henka_relative_path_is_confined(const char* relative_path)
{
    const char* cursor;
    const char* segment_start;

    if (relative_path == NULL || relative_path[0] == '\0' || henka_path_is_absolute(relative_path))
    {
        return false;
    }

    cursor = relative_path;
    segment_start = relative_path;

    for (;;)
    {
        unsigned char character = (unsigned char)*cursor;
        bool at_end = character == '\0';
        bool at_separator = !at_end && henka_path_is_separator((char)character);

        if (!at_end && !at_separator)
        {
            if (character < 0x20U || character == 0x7fU ||
                character == ':' || character == '<' || character == '>' ||
                character == '"' || character == '|' || character == '?' || character == '*')
            {
                return false;
            }

            ++cursor;
            continue;
        }

        {
            size_t segment_length = (size_t)(cursor - segment_start);
            char last_character;

            if (segment_length == 0U)
            {
                return false;
            }

            last_character = segment_start[segment_length - 1U];
            if ((segment_length == 1U && segment_start[0] == '.') ||
                (segment_length == 2U && segment_start[0] == '.' && segment_start[1] == '.') ||
                last_character == '.' || last_character == ' ' ||
                henka_windows_name_is_reserved(segment_start, segment_length))
            {
                return false;
            }
        }

        if (at_end)
        {
            break;
        }

        segment_start = cursor + 1;
        ++cursor;
    }

    return true;
}

static bool henka_parse_int_value(const char* value, int* out_value)
{
    char* end;
    long parsed;

    if (value == NULL || out_value == NULL)
    {
        return false;
    }

    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX)
    {
        return false;
    }

    *out_value = (int)parsed;
    return true;
}

static bool henka_parse_float_value(const char* value, float* out_value)
{
    char* end;
    float parsed;

    if (value == NULL || out_value == NULL)
    {
        return false;
    }

    errno = 0;
    parsed = strtof(value, &end);
    if (errno != 0 || end == value || *end != '\0' || !isfinite(parsed))
    {
        return false;
    }

    *out_value = parsed;
    return true;
}

static bool henka_parse_bool_value(const char* value, bool* out_value)
{
    if (value == NULL || out_value == NULL)
    {
        return false;
    }

    if (henka_ascii_equals_ignore_case(value, strlen(value), "true") ||
        strcmp(value, "1") == 0 ||
        henka_ascii_equals_ignore_case(value, strlen(value), "yes") ||
        henka_ascii_equals_ignore_case(value, strlen(value), "on"))
    {
        *out_value = true;
        return true;
    }

    if (henka_ascii_equals_ignore_case(value, strlen(value), "false") ||
        strcmp(value, "0") == 0 ||
        henka_ascii_equals_ignore_case(value, strlen(value), "no") ||
        henka_ascii_equals_ignore_case(value, strlen(value), "off"))
    {
        *out_value = false;
        return true;
    }

    return false;
}

static void henka_settings_clear(henka_settings* settings)
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
    settings->entries = NULL;
    settings->count = 0U;
    settings->capacity = 0U;
}

static void henka_settings_move_assign(henka_settings* destination, henka_settings* source)
{
    henka_settings_clear(destination);
    destination->entries = source->entries;
    destination->count = source->count;
    destination->capacity = source->capacity;
    source->entries = NULL;
    source->count = 0U;
    source->capacity = 0U;
}

static void henka_save_data_swap(henka_save_data* left, henka_save_data* right)
{
    henka_save_data temporary = *left;
    *left = *right;
    *right = temporary;
}

static char* henka_persistence_build_temp_path(const char* path)
{
    char* temp_path;
    size_t path_length;
    size_t suffix_length;

    if (path == NULL)
    {
        return NULL;
    }

    path_length = strlen(path);
    suffix_length = strlen(HENKA_PERSISTENCE_TEMP_SUFFIX);
    if (path_length > SIZE_MAX - suffix_length - 1U)
    {
        return NULL;
    }

    temp_path = henka_malloc(path_length + suffix_length + 1U);
    if (temp_path == NULL)
    {
        return NULL;
    }

    memcpy(temp_path, path, path_length);
    memcpy(temp_path + path_length, HENKA_PERSISTENCE_TEMP_SUFFIX, suffix_length + 1U);
    return temp_path;
}

static henka_result henka_persistence_flush_file(FILE* file)
{
    if (file == NULL || fflush(file) != 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }

#ifdef _WIN32
    if (_commit(_fileno(file)) != 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }
#else
    if (fsync(fileno(file)) != 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }
#endif

    return HENKA_SUCCESS;
}

static henka_result henka_persistence_replace_file(const char* temp_path, const char* destination_path)
{
#ifdef _WIN32
    if (!MoveFileExA(temp_path, destination_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        return HENKA_ERROR_UNKNOWN;
    }
#else
    if (rename(temp_path, destination_path) != 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }
#endif

    return HENKA_SUCCESS;
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

    if (settings->capacity >= HENKA_SETTINGS_MAX_ENTRIES)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    if (settings->capacity == 0U)
    {
        new_capacity = 8U;
    }
    else if (settings->capacity > HENKA_SETTINGS_MAX_ENTRIES / 2U)
    {
        new_capacity = HENKA_SETTINGS_MAX_ENTRIES;
    }
    else
    {
        new_capacity = settings->capacity * 2U;
    }

    if (new_capacity > SIZE_MAX / sizeof(*entries))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

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
    size_t allocation_size;
    size_t base_length;
    bool needs_separator;
    size_t relative_length;
    char* resolved_path;

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

    needs_separator = base_length > 0U && relative_length > 0U && !henka_path_is_separator(relative_path[0]);
    if (base_length > SIZE_MAX - relative_length - (needs_separator ? 2U : 1U))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    allocation_size = base_length + relative_length + (needs_separator ? 2U : 1U);
    resolved_path = henka_malloc(allocation_size);
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

henka_result henka_path_resolve_confined(const char* base_path, const char* relative_path, char** out_path)
{
    size_t index;
    size_t length;
    char* normalized_path;
    henka_result result;

    if (relative_path == NULL || out_path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_path = NULL;
    if (!henka_relative_path_is_confined(relative_path))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    length = strlen(relative_path);
    normalized_path = henka_duplicate_string(relative_path);
    if (normalized_path == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    for (index = 0U; index < length; ++index)
    {
        if (normalized_path[index] == '\\')
        {
            normalized_path[index] = '/';
        }
    }

    result = henka_path_resolve(base_path, normalized_path, out_path);
    henka_free(normalized_path);
    return result;
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
    bool had_parse_error;
    char line[1024];
    unsigned int line_number;
    henka_settings* loaded;
    henka_result result;

    if (settings == NULL || path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_settings_create(&loaded);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    file = NULL;
    if (fopen_s(&file, path, "rb") != 0 || file == NULL)
    {
        HENKA_LOG_WARN("Settings file could not be opened: %s", path);
        henka_settings_destroy(loaded);
        return HENKA_ERROR_UNKNOWN;
    }

    had_parse_error = false;
    line_number = 0U;

    while (fgets(line, (int)sizeof(line), file) != NULL)
    {
        char* equals;
        char* key;
        size_t line_length;
        char* trimmed_line;
        char* value;

        ++line_number;
        line_length = strlen(line);
        if (line_length > 0U && line[line_length - 1U] != '\n' && !feof(file))
        {
            HENKA_LOG_WARN("Rejecting overlong settings line %u in %s", line_number, path);
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
            henka_settings_destroy(loaded);
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
            HENKA_LOG_WARN("Rejecting malformed settings line %u in %s", line_number, path);
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
            henka_settings_destroy(loaded);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }

        result = henka_settings_set_string(loaded, key, value);
        henka_free(key);
        henka_free(value);

        if (result == HENKA_ERROR_OUT_OF_MEMORY)
        {
            fclose(file);
            henka_settings_destroy(loaded);
            return result;
        }
        if (result != HENKA_SUCCESS)
        {
            HENKA_LOG_WARN("Rejecting unsafe settings line %u in %s", line_number, path);
            had_parse_error = true;
        }
    }

    result = ferror(file) ? HENKA_ERROR_UNKNOWN : HENKA_SUCCESS;
    if (fclose(file) != 0)
    {
        result = HENKA_ERROR_UNKNOWN;
    }
    if (result != HENKA_SUCCESS)
    {
        henka_settings_destroy(loaded);
        return result;
    }

    if (had_parse_error)
    {
        henka_settings_destroy(loaded);
        return HENKA_ERROR_UNKNOWN;
    }

    henka_settings_move_assign(settings, loaded);
    henka_settings_destroy(loaded);
    return HENKA_SUCCESS;
}

henka_result henka_settings_save_file(const henka_settings* settings, const char* path)
{
    char* directory_path;
    FILE* file;
    size_t index;
    henka_result result;
    char* temp_path;

    if (settings == NULL || path == NULL || path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < settings->count; ++index)
    {
        if (!henka_settings_key_is_valid(settings->entries[index].key) ||
            !henka_settings_value_is_valid(settings->entries[index].value))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
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
    else if (result != HENKA_ERROR_INVALID_ARGUMENT)
    {
        return result;
    }

    temp_path = henka_persistence_build_temp_path(path);
    if (temp_path == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    remove(temp_path);
    file = NULL;
    if (fopen_s(&file, temp_path, "wb") != 0 || file == NULL)
    {
        henka_free(temp_path);
        return HENKA_ERROR_UNKNOWN;
    }

    result = HENKA_SUCCESS;
    for (index = 0U; index < settings->count; ++index)
    {
        if (fprintf(file, "%s=%s\n", settings->entries[index].key, settings->entries[index].value) < 0)
        {
            result = HENKA_ERROR_UNKNOWN;
            break;
        }
    }

    if (result == HENKA_SUCCESS)
    {
        result = henka_persistence_flush_file(file);
    }

    if (fclose(file) != 0 && result == HENKA_SUCCESS)
    {
        result = HENKA_ERROR_UNKNOWN;
    }

    if (result == HENKA_SUCCESS)
    {
        result = henka_persistence_replace_file(temp_path, path);
    }

    if (result != HENKA_SUCCESS)
    {
        remove(temp_path);
    }

    henka_free(temp_path);
    return result;
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
    int parsed;
    const char* value;

    value = henka_settings_get_string(settings, key, NULL);
    if (!henka_parse_int_value(value, &parsed))
    {
        return default_value;
    }

    return parsed;
}

float henka_settings_get_float(const henka_settings* settings, const char* key, float default_value)
{
    float parsed;
    const char* value;

    value = henka_settings_get_string(settings, key, NULL);
    if (!henka_parse_float_value(value, &parsed))
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
    henka_result result;
    char* value_copy;

    if (settings == NULL || !henka_settings_key_is_valid(key) || !henka_settings_value_is_valid(value))
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
    ++settings->count;
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

    if (!isfinite(value))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

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

henka_result henka_save_data_build_slot_path(const char* user_data_base_path, const char* slot_name, char** out_path)
{
    char relative_path[96];

    if (out_path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_path = NULL;
    if (!henka_save_slot_name_is_valid(slot_name))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (sprintf_s(relative_path, sizeof(relative_path), "saves/%s.save", slot_name) < 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    return henka_path_resolve_confined(user_data_base_path, relative_path, out_path);
}

henka_result henka_save_data_create(henka_save_data** out_save_data)
{
    henka_save_data* save_data;
    henka_result result;

    if (out_save_data == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_save_data = NULL;
    save_data = henka_calloc(1U, sizeof(*save_data));
    if (save_data == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    save_data->version = 1;
    save_data->scene_id = henka_duplicate_string("sandbox3d");
    if (save_data->scene_id == NULL)
    {
        henka_free(save_data);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    result = henka_settings_create(&save_data->flags);
    if (result != HENKA_SUCCESS)
    {
        henka_free(save_data->scene_id);
        henka_free(save_data);
        return result;
    }

    *out_save_data = save_data;
    return HENKA_SUCCESS;
}

void henka_save_data_destroy(henka_save_data* save_data)
{
    if (save_data == NULL)
    {
        return;
    }

    henka_settings_destroy(save_data->flags);
    henka_free(save_data->scene_id);
    henka_free(save_data);
}

int henka_save_data_get_version(const henka_save_data* save_data)
{
    return save_data != NULL ? save_data->version : 0;
}

const char* henka_save_data_get_scene_id(const henka_save_data* save_data)
{
    return (save_data != NULL && save_data->scene_id != NULL) ? save_data->scene_id : "";
}

henka_result henka_save_data_set_scene_id(henka_save_data* save_data, const char* scene_id)
{
    char* copy;

    if (save_data == NULL || scene_id == NULL || scene_id[0] == '\0' ||
        !henka_settings_value_is_valid(scene_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    copy = henka_duplicate_string(scene_id);
    if (copy == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    henka_free(save_data->scene_id);
    save_data->scene_id = copy;
    return HENKA_SUCCESS;
}

henka_result henka_save_data_set_camera_pose(
    henka_save_data* save_data,
    henka_vec3 position,
    float yaw_radians,
    float pitch_radians)
{
    if (save_data == NULL ||
        !isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z) ||
        !isfinite(yaw_radians) || !isfinite(pitch_radians))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    save_data->camera_position = position;
    save_data->camera_yaw_radians = yaw_radians;
    save_data->camera_pitch_radians = pitch_radians;
    return HENKA_SUCCESS;
}

henka_result henka_save_data_get_camera_pose(
    const henka_save_data* save_data,
    henka_vec3* out_position,
    float* out_yaw_radians,
    float* out_pitch_radians)
{
    if (save_data == NULL || out_position == NULL || out_yaw_radians == NULL || out_pitch_radians == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_position = save_data->camera_position;
    *out_yaw_radians = save_data->camera_yaw_radians;
    *out_pitch_radians = save_data->camera_pitch_radians;
    return HENKA_SUCCESS;
}

henka_result henka_save_data_set_flag_bool(henka_save_data* save_data, const char* key, bool value)
{
    if (save_data == NULL || save_data->flags == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_settings_set_bool(save_data->flags, key, value);
}

bool henka_save_data_get_flag_bool(const henka_save_data* save_data, const char* key, bool default_value)
{
    if (save_data == NULL || save_data->flags == NULL)
    {
        return default_value;
    }

    return henka_settings_get_bool(save_data->flags, key, default_value);
}

henka_result henka_save_data_load_file(henka_save_data* save_data, const char* path)
{
    bool flag_value;
    float camera_pitch;
    henka_vec3 camera_position;
    float camera_yaw;
    henka_save_data* candidate;
    size_t index;
    henka_settings* loaded;
    henka_result result;
    const char* scene_id;
    const char* value;
    int version;

    if (save_data == NULL || path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_settings_create(&loaded);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_settings_load_file(loaded, path);
    if (result != HENKA_SUCCESS)
    {
        henka_settings_destroy(loaded);
        return result;
    }

    value = henka_settings_get_string(loaded, "save.version", NULL);
    if (!henka_parse_int_value(value, &version) || version != 1)
    {
        henka_settings_destroy(loaded);
        return HENKA_ERROR_UNKNOWN;
    }

    scene_id = henka_settings_get_string(loaded, "save.scene_id", NULL);
    if (scene_id == NULL || scene_id[0] == '\0')
    {
        henka_settings_destroy(loaded);
        return HENKA_ERROR_UNKNOWN;
    }

    value = henka_settings_get_string(loaded, "camera.position.x", NULL);
    if (!henka_parse_float_value(value, &camera_position.x))
    {
        henka_settings_destroy(loaded);
        return HENKA_ERROR_UNKNOWN;
    }
    value = henka_settings_get_string(loaded, "camera.position.y", NULL);
    if (!henka_parse_float_value(value, &camera_position.y))
    {
        henka_settings_destroy(loaded);
        return HENKA_ERROR_UNKNOWN;
    }
    value = henka_settings_get_string(loaded, "camera.position.z", NULL);
    if (!henka_parse_float_value(value, &camera_position.z))
    {
        henka_settings_destroy(loaded);
        return HENKA_ERROR_UNKNOWN;
    }
    value = henka_settings_get_string(loaded, "camera.yaw_radians", NULL);
    if (!henka_parse_float_value(value, &camera_yaw))
    {
        henka_settings_destroy(loaded);
        return HENKA_ERROR_UNKNOWN;
    }
    value = henka_settings_get_string(loaded, "camera.pitch_radians", NULL);
    if (!henka_parse_float_value(value, &camera_pitch))
    {
        henka_settings_destroy(loaded);
        return HENKA_ERROR_UNKNOWN;
    }

    result = henka_save_data_create(&candidate);
    if (result != HENKA_SUCCESS)
    {
        henka_settings_destroy(loaded);
        return result;
    }

    result = henka_save_data_set_scene_id(candidate, scene_id);
    if (result == HENKA_SUCCESS)
    {
        result = henka_save_data_set_camera_pose(candidate, camera_position, camera_yaw, camera_pitch);
    }

    for (index = 0U; result == HENKA_SUCCESS && index < loaded->count; ++index)
    {
        if (strncmp(loaded->entries[index].key, "flag.", 5U) == 0)
        {
            const char* flag_key = loaded->entries[index].key + 5U;
            if (flag_key[0] == '\0' ||
                !henka_parse_bool_value(loaded->entries[index].value, &flag_value))
            {
                result = HENKA_ERROR_UNKNOWN;
                break;
            }

            result = henka_settings_set_bool(candidate->flags, flag_key, flag_value);
        }
    }

    if (result == HENKA_SUCCESS)
    {
        henka_save_data_swap(save_data, candidate);
    }

    henka_save_data_destroy(candidate);
    henka_settings_destroy(loaded);
    return result;
}

henka_result henka_save_data_save_file(const henka_save_data* save_data, const char* path)
{
    henka_settings* settings;
    henka_result result;

    if (save_data == NULL || path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_settings_create(&settings);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_settings_set_int(settings, "save.version", save_data->version);
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_string(settings, "save.scene_id", henka_save_data_get_scene_id(save_data));
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_float(settings, "camera.position.x", save_data->camera_position.x);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_float(settings, "camera.position.y", save_data->camera_position.y);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_float(settings, "camera.position.z", save_data->camera_position.z);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_float(settings, "camera.yaw_radians", save_data->camera_yaw_radians);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_float(settings, "camera.pitch_radians", save_data->camera_pitch_radians);
    }

    if (result == HENKA_SUCCESS && save_data->flags != NULL)
    {
        size_t index;
        for (index = 0U; index < save_data->flags->count; ++index)
        {
            char prefixed_key[256];
            if (sprintf_s(prefixed_key, sizeof(prefixed_key), "flag.%s", save_data->flags->entries[index].key) < 0)
            {
                result = HENKA_ERROR_UNKNOWN;
                break;
            }

            result = henka_settings_set_string(settings, prefixed_key, save_data->flags->entries[index].value);
            if (result != HENKA_SUCCESS)
            {
                break;
            }
        }
    }

    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_save_file(settings, path);
    }

    henka_settings_destroy(settings);
    return result;
}
