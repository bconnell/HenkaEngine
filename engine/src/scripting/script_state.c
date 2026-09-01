#include <henka/script_state.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <stdatomic.h>
#include <unistd.h>
#endif

#include <henka/memory.h>
#include <henka/persistence.h>

#define HENKA_SCRIPT_STATE_MAGIC UINT32_C(0x31534B48)
#define HENKA_SCRIPT_STATE_HEADER_BYTES 16U
#define HENKA_SCRIPT_STATE_RECORD_BYTES 40U
#define HENKA_SCRIPT_STATE_TEMP_PATH_CAPACITY 96U

#ifdef _WIN32
static volatile LONG g_henka_script_state_save_sequence = 0L;
#else
static atomic_uint g_henka_script_state_save_sequence = 0U;
#endif

typedef struct henka_script_state_entry
{
    henka_script_state_identity identity;
    uint32_t key;
    henka_script_state_value value;
} henka_script_state_entry;

struct henka_script_state_store
{
    size_t count;
    henka_script_state_entry entries[HENKA_SCRIPT_STATE_MAX_VALUES];
};

static bool henka_script_state_identity_is_valid(
    henka_script_state_identity identity)
{
    return identity.entity_id != 0U && identity.behavior_id != 0U;
}

static bool henka_script_state_value_is_valid(
    henka_script_state_value value)
{
    switch (value.type)
    {
        case HENKA_SCRIPT_STATE_VALUE_BOOL:
            return value.as.boolean == false || value.as.boolean == true;
        case HENKA_SCRIPT_STATE_VALUE_I32:
            return true;
        case HENKA_SCRIPT_STATE_VALUE_FLOAT32:
            return isfinite(value.as.f32) != 0;
        case HENKA_SCRIPT_STATE_VALUE_VEC3:
            return isfinite(value.as.vec3.x) != 0 &&
                isfinite(value.as.vec3.y) != 0 &&
                isfinite(value.as.vec3.z) != 0;
        default:
            return false;
    }
}

static bool henka_script_state_entry_matches(
    const henka_script_state_entry* entry,
    henka_script_state_identity identity,
    uint32_t key)
{
    return entry != NULL &&
        entry->identity.entity_id == identity.entity_id &&
        entry->identity.behavior_id == identity.behavior_id &&
        entry->key == key;
}

static size_t henka_script_state_find_index(
    const henka_script_state_store* store,
    henka_script_state_identity identity,
    uint32_t key)
{
    size_t index;
    if (store == NULL)
    {
        return SIZE_MAX;
    }
    for (index = 0U; index < store->count; ++index)
    {
        if (henka_script_state_entry_matches(
                &store->entries[index], identity, key))
        {
            return index;
        }
    }
    return SIZE_MAX;
}

henka_result henka_script_state_store_create(
    henka_script_state_store** out_store)
{
    henka_script_state_store* store;
    if (out_store == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_store = NULL;
    store = (henka_script_state_store*)henka_calloc(1U, sizeof(*store));
    if (store == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    *out_store = store;
    return HENKA_SUCCESS;
}

henka_result henka_script_state_store_clone(
    const henka_script_state_store* source,
    henka_script_state_store** out_store)
{
    henka_script_state_store* clone = NULL;
    henka_result result;
    if (out_store == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_store = NULL;
    if (source == NULL || source->count > HENKA_SCRIPT_STATE_MAX_VALUES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_state_store_create(&clone);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    memcpy(clone, source, sizeof(*clone));
    *out_store = clone;
    return HENKA_SUCCESS;
}

void henka_script_state_store_destroy(
    henka_script_state_store* store)
{
    henka_free(store);
}

void henka_script_state_store_clear(
    henka_script_state_store* store)
{
    if (store != NULL)
    {
        memset(store->entries, 0, sizeof(store->entries));
        store->count = 0U;
    }
}

henka_result henka_script_state_store_set(
    henka_script_state_store* store,
    henka_script_state_identity identity,
    uint32_t key,
    henka_script_state_value value)
{
    size_t index;
    if (store == NULL || !henka_script_state_identity_is_valid(identity) ||
        key == 0U || !henka_script_state_value_is_valid(value))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    index = henka_script_state_find_index(store, identity, key);
    if (index == SIZE_MAX)
    {
        if (store->count >= HENKA_SCRIPT_STATE_MAX_VALUES)
        {
            return HENKA_ERROR_LIMIT;
        }
        index = store->count++;
        store->entries[index].identity = identity;
        store->entries[index].key = key;
    }
    store->entries[index].value = value;
    return HENKA_SUCCESS;
}

henka_result henka_script_state_store_get(
    const henka_script_state_store* store,
    henka_script_state_identity identity,
    uint32_t key,
    henka_script_state_value* out_value,
    bool* out_present)
{
    size_t index;
    if (out_value != NULL)
    {
        *out_value = (henka_script_state_value){HENKA_SCRIPT_STATE_VALUE_NONE};
    }
    if (out_present != NULL)
    {
        *out_present = false;
    }
    if (store == NULL || !henka_script_state_identity_is_valid(identity) ||
        key == 0U || out_value == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    index = henka_script_state_find_index(store, identity, key);
    if (index == SIZE_MAX)
    {
        return HENKA_SUCCESS;
    }
    *out_value = store->entries[index].value;
    if (out_present != NULL)
    {
        *out_present = true;
    }
    return HENKA_SUCCESS;
}

henka_result henka_script_state_store_remove(
    henka_script_state_store* store,
    henka_script_state_identity identity,
    uint32_t key)
{
    size_t index;
    if (store == NULL || !henka_script_state_identity_is_valid(identity) ||
        key == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    index = henka_script_state_find_index(store, identity, key);
    if (index == SIZE_MAX)
    {
        return HENKA_SUCCESS;
    }
    if (index + 1U < store->count)
    {
        memmove(
            &store->entries[index],
            &store->entries[index + 1U],
            (store->count - index - 1U) * sizeof(store->entries[0]));
    }
    --store->count;
    memset(&store->entries[store->count], 0, sizeof(store->entries[0]));
    return HENKA_SUCCESS;
}

size_t henka_script_state_store_get_count(
    const henka_script_state_store* store)
{
    return store == NULL ? 0U : store->count;
}

static void henka_script_state_write_u32(
    unsigned char* destination,
    uint32_t value)
{
    destination[0] = (unsigned char)(value & 0xFFU);
    destination[1] = (unsigned char)((value >> 8U) & 0xFFU);
    destination[2] = (unsigned char)((value >> 16U) & 0xFFU);
    destination[3] = (unsigned char)((value >> 24U) & 0xFFU);
}

static void henka_script_state_write_u64(
    unsigned char* destination,
    uint64_t value)
{
    henka_script_state_write_u32(destination, (uint32_t)(value & UINT32_MAX));
    henka_script_state_write_u32(destination + 4U, (uint32_t)(value >> 32U));
}

static uint32_t henka_script_state_read_u32(
    const unsigned char* source)
{
    return (uint32_t)source[0] |
        ((uint32_t)source[1] << 8U) |
        ((uint32_t)source[2] << 16U) |
        ((uint32_t)source[3] << 24U);
}

static uint64_t henka_script_state_read_u64(
    const unsigned char* source)
{
    return (uint64_t)henka_script_state_read_u32(source) |
        ((uint64_t)henka_script_state_read_u32(source + 4U) << 32U);
}

static void henka_script_state_write_float(
    unsigned char* destination,
    float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    henka_script_state_write_u32(destination, bits);
}

static float henka_script_state_read_float(
    const unsigned char* source)
{
    const uint32_t bits = henka_script_state_read_u32(source);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool henka_script_state_size_add(
    size_t left,
    size_t right,
    size_t* out_sum)
{
    if (out_sum == NULL || right > SIZE_MAX - left)
    {
        return false;
    }
    *out_sum = left + right;
    return true;
}

static bool henka_script_state_size_for_count(
    uint32_t count,
    size_t* out_size)
{
    size_t records_size;
    if (count > HENKA_SCRIPT_STATE_MAX_VALUES || out_size == NULL ||
        count > (SIZE_MAX - HENKA_SCRIPT_STATE_HEADER_BYTES) /
            HENKA_SCRIPT_STATE_RECORD_BYTES)
    {
        return false;
    }
    records_size = (size_t)count * HENKA_SCRIPT_STATE_RECORD_BYTES;
    return henka_script_state_size_add(
        HENKA_SCRIPT_STATE_HEADER_BYTES, records_size, out_size) &&
        *out_size <= HENKA_SCRIPT_STATE_MAX_FILE_BYTES;
}

static void henka_script_state_encode_entry(
    unsigned char* destination,
    const henka_script_state_entry* entry)
{
    memset(destination, 0, HENKA_SCRIPT_STATE_RECORD_BYTES);
    henka_script_state_write_u64(destination, entry->identity.entity_id);
    henka_script_state_write_u64(destination + 8U, entry->identity.behavior_id);
    henka_script_state_write_u32(destination + 16U, entry->key);
    henka_script_state_write_u32(destination + 20U, (uint32_t)entry->value.type);
    switch (entry->value.type)
    {
        case HENKA_SCRIPT_STATE_VALUE_BOOL:
            henka_script_state_write_u32(destination + 24U, entry->value.as.boolean ? 1U : 0U);
            break;
        case HENKA_SCRIPT_STATE_VALUE_I32:
            henka_script_state_write_u32(destination + 24U, (uint32_t)entry->value.as.i32);
            break;
        case HENKA_SCRIPT_STATE_VALUE_FLOAT32:
            henka_script_state_write_float(destination + 24U, entry->value.as.f32);
            break;
        case HENKA_SCRIPT_STATE_VALUE_VEC3:
            henka_script_state_write_float(destination + 24U, entry->value.as.vec3.x);
            henka_script_state_write_float(destination + 28U, entry->value.as.vec3.y);
            henka_script_state_write_float(destination + 32U, entry->value.as.vec3.z);
            break;
        default:
            break;
    }
}

static bool henka_script_state_decode_entry(
    const unsigned char* source,
    henka_script_state_entry* out_entry)
{
    const uint32_t type = henka_script_state_read_u32(source + 20U);
    const uint32_t scalar = henka_script_state_read_u32(source + 24U);
    if (out_entry == NULL ||
        henka_script_state_read_u32(source + 36U) != 0U ||
        !henka_script_state_identity_is_valid((henka_script_state_identity){
            henka_script_state_read_u64(source),
            henka_script_state_read_u64(source + 8U)}) ||
        henka_script_state_read_u32(source + 16U) == 0U ||
        type <= HENKA_SCRIPT_STATE_VALUE_NONE ||
        type > HENKA_SCRIPT_STATE_VALUE_VEC3)
    {
        return false;
    }
    *out_entry = (henka_script_state_entry){
        {
            henka_script_state_read_u64(source),
            henka_script_state_read_u64(source + 8U)},
        henka_script_state_read_u32(source + 16U),
        {(henka_script_state_value_type)type, {.i32 = 0}}};
    switch (out_entry->value.type)
    {
        case HENKA_SCRIPT_STATE_VALUE_BOOL:
            if (scalar > 1U)
            {
                return false;
            }
            out_entry->value.as.boolean = scalar != 0U;
            break;
        case HENKA_SCRIPT_STATE_VALUE_I32:
            memcpy(&out_entry->value.as.i32, &scalar, sizeof(scalar));
            break;
        case HENKA_SCRIPT_STATE_VALUE_FLOAT32:
            out_entry->value.as.f32 = henka_script_state_read_float(source + 24U);
            break;
        case HENKA_SCRIPT_STATE_VALUE_VEC3:
            out_entry->value.as.vec3.x = henka_script_state_read_float(source + 24U);
            out_entry->value.as.vec3.y = henka_script_state_read_float(source + 28U);
            out_entry->value.as.vec3.z = henka_script_state_read_float(source + 32U);
            break;
        default:
            return false;
    }
    return henka_script_state_value_is_valid(out_entry->value);
}

static henka_result henka_script_state_resolve_path(
    const char* project_root,
    const char* relative_path,
    char** out_path)
{
    if (project_root == NULL || project_root[0] == '\0' ||
        relative_path == NULL || relative_path[0] == '\0' || out_path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_path_resolve_confined(project_root, relative_path, out_path);
}

static uint32_t henka_script_state_next_save_sequence(void)
{
#ifdef _WIN32
    return (uint32_t)InterlockedIncrement(&g_henka_script_state_save_sequence);
#else
    return atomic_fetch_add_explicit(
        &g_henka_script_state_save_sequence,
        1U,
        memory_order_relaxed) + 1U;
#endif
}

static henka_result henka_script_state_make_temporary_path(
    const char* path,
    char** out_temporary_path)
{
    const size_t path_length = path == NULL ? 0U : strlen(path);
    char* temporary_path;
    int written;

    if (path == NULL || path_length > SIZE_MAX - HENKA_SCRIPT_STATE_TEMP_PATH_CAPACITY ||
        out_temporary_path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    temporary_path = (char*)henka_malloc(
        path_length + HENKA_SCRIPT_STATE_TEMP_PATH_CAPACITY);
    if (temporary_path == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
#ifdef _WIN32
    written = _snprintf_s(
        temporary_path,
        path_length + HENKA_SCRIPT_STATE_TEMP_PATH_CAPACITY,
        _TRUNCATE,
        "%s.tmp.%lu.%lu.%lu",
        path,
        (unsigned long)GetCurrentProcessId(),
        (unsigned long)GetCurrentThreadId(),
        (unsigned long)henka_script_state_next_save_sequence());
#else
    written = snprintf(
        temporary_path,
        path_length + HENKA_SCRIPT_STATE_TEMP_PATH_CAPACITY,
        "%s.tmp.%ld.%lu",
        path,
        (long)getpid(),
        (unsigned long)henka_script_state_next_save_sequence());
#endif
    if (written < 0 ||
        (size_t)written >= path_length + HENKA_SCRIPT_STATE_TEMP_PATH_CAPACITY)
    {
        henka_free(temporary_path);
        return HENKA_ERROR_LIMIT;
    }
    *out_temporary_path = temporary_path;
    return HENKA_SUCCESS;
}

static henka_result henka_script_state_flush_file(FILE* file)
{
    if (file == NULL || fflush(file) != 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }
#ifdef _WIN32
    return _commit(_fileno(file)) == 0 ? HENKA_SUCCESS : HENKA_ERROR_UNKNOWN;
#else
    return fsync(fileno(file)) == 0 ? HENKA_SUCCESS : HENKA_ERROR_UNKNOWN;
#endif
}

static henka_result henka_script_state_replace_file(
    const char* temporary_path,
    const char* destination_path)
{
#ifdef _WIN32
    return MoveFileExA(
        temporary_path,
        destination_path,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
        ? HENKA_SUCCESS
        : HENKA_ERROR_UNKNOWN;
#else
    return rename(temporary_path, destination_path) == 0
        ? HENKA_SUCCESS
        : HENKA_ERROR_UNKNOWN;
#endif
}

henka_result henka_script_state_store_save_file(
    const henka_script_state_store* store,
    const char* project_root,
    const char* relative_path)
{
    unsigned char buffer[HENKA_SCRIPT_STATE_MAX_FILE_BYTES];
    char* path = NULL;
    char* temporary_path = NULL;
    FILE* file = NULL;
    size_t size;
    size_t index;
    henka_result result;
    if (store == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_state_resolve_path(project_root, relative_path, &path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(path);
        return result;
    }
    if (store->count > HENKA_SCRIPT_STATE_MAX_VALUES ||
        !henka_script_state_size_for_count((uint32_t)store->count, &size))
    {
        henka_free(path);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    henka_script_state_write_u32(buffer, HENKA_SCRIPT_STATE_MAGIC);
    henka_script_state_write_u32(buffer + 4U, HENKA_SCRIPT_STATE_FORMAT_VERSION);
    henka_script_state_write_u32(buffer + 8U, (uint32_t)store->count);
    henka_script_state_write_u32(buffer + 12U, 0U);
    for (index = 0U; index < store->count; ++index)
    {
        if (!henka_script_state_identity_is_valid(store->entries[index].identity) ||
            store->entries[index].key == 0U ||
            !henka_script_state_value_is_valid(store->entries[index].value))
        {
            henka_free(path);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        henka_script_state_encode_entry(
            buffer + HENKA_SCRIPT_STATE_HEADER_BYTES +
                index * HENKA_SCRIPT_STATE_RECORD_BYTES,
            &store->entries[index]);
    }
    result = henka_script_state_make_temporary_path(path, &temporary_path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(path);
        return result;
    }
    result = henka_path_ensure_parent_directory(path);
    if (result == HENKA_SUCCESS)
    {
#ifdef _WIN32
        if (fopen_s(&file, temporary_path, "wb") != 0)
        {
            file = NULL;
        }
#else
        file = fopen(temporary_path, "wb");
#endif
        result = file == NULL ? HENKA_ERROR_UNKNOWN : HENKA_SUCCESS;
    }
    if (result == HENKA_SUCCESS &&
        fwrite(buffer, 1U, size, file) != size)
    {
        result = HENKA_ERROR_UNKNOWN;
    }
    if (file != NULL && result == HENKA_SUCCESS)
    {
        result = henka_script_state_flush_file(file);
    }
    if (file != NULL && fclose(file) != 0 && result == HENKA_SUCCESS)
    {
        result = HENKA_ERROR_UNKNOWN;
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_script_state_replace_file(temporary_path, path);
    }
    if (result != HENKA_SUCCESS)
    {
        (void)remove(temporary_path);
    }
    henka_free(temporary_path);
    henka_free(path);
    return result;
}

henka_result henka_script_state_store_load_file(
    henka_script_state_store* store,
    const char* project_root,
    const char* relative_path)
{
    unsigned char buffer[HENKA_SCRIPT_STATE_MAX_FILE_BYTES];
    henka_script_state_store* candidate = NULL;
    char* path = NULL;
    FILE* file = NULL;
    long file_size;
    size_t size;
    size_t position;
    uint32_t count;
    size_t index;
    henka_result result;
    if (store == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_state_resolve_path(project_root, relative_path, &path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(path);
        return result;
    }
#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(path, "rb");
#endif
    henka_free(path);
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0)
    {
        if (file != NULL)
        {
            (void)fclose(file);
        }
        return HENKA_ERROR_UNKNOWN;
    }
    file_size = ftell(file);
    if (file_size < 0L || (unsigned long)file_size > HENKA_SCRIPT_STATE_MAX_FILE_BYTES)
    {
        (void)fclose(file);
        return HENKA_ERROR_LIMIT;
    }
    size = (size_t)file_size;
    if (fseek(file, 0L, SEEK_SET) != 0 ||
        (size > 0U && fread(buffer, 1U, size, file) != size))
    {
        (void)fclose(file);
        return HENKA_ERROR_UNKNOWN;
    }
    if (fclose(file) != 0)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    if (size < HENKA_SCRIPT_STATE_HEADER_BYTES ||
        henka_script_state_read_u32(buffer) != HENKA_SCRIPT_STATE_MAGIC ||
        henka_script_state_read_u32(buffer + 4U) != HENKA_SCRIPT_STATE_FORMAT_VERSION ||
        henka_script_state_read_u32(buffer + 12U) != 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    count = henka_script_state_read_u32(buffer + 8U);
    if (!henka_script_state_size_for_count(count, &position) || position != size)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_state_store_create(&candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    position = HENKA_SCRIPT_STATE_HEADER_BYTES;
    for (index = 0U; index < (size_t)count; ++index)
    {
        henka_script_state_entry entry;
        if (!henka_script_state_decode_entry(buffer + position, &entry) ||
            henka_script_state_find_index(
                candidate, entry.identity, entry.key) != SIZE_MAX)
        {
            henka_script_state_store_destroy(candidate);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        result = henka_script_state_store_set(
            candidate, entry.identity, entry.key, entry.value);
        if (result != HENKA_SUCCESS)
        {
            henka_script_state_store_destroy(candidate);
            return result;
        }
        position += HENKA_SCRIPT_STATE_RECORD_BYTES;
    }
    *store = *candidate;
    henka_script_state_store_destroy(candidate);
    return HENKA_SUCCESS;
}
