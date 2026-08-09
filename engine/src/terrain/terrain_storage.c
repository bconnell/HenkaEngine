#include <henka/terrain_storage.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <henka/memory.h>
#include <henka/persistence.h>

#include "terrain_internal.h"

#define HENKA_TERRAIN_REGION_MAGIC UINT32_C(0x48545231)
#define HENKA_TERRAIN_JOURNAL_MAGIC UINT32_C(0x48544A31)
#define HENKA_TERRAIN_REGION_HEADER_BYTES 56U
#define HENKA_TERRAIN_REGION_CHECKSUM_BYTES 4U
#define HENKA_TERRAIN_MANIFEST_MAGIC UINT32_C(0x48544D31)
#define HENKA_TERRAIN_MANIFEST_BYTES 80U
#define HENKA_TERRAIN_JOURNAL_HEADER_BYTES 20U
#define HENKA_TERRAIN_JOURNAL_BEGIN 1U
#define HENKA_TERRAIN_JOURNAL_REGION 2U
#define HENKA_TERRAIN_JOURNAL_COMMIT 3U
#define HENKA_TERRAIN_MAX_TRANSACTION_REGIONS 64U

typedef struct henka_terrain_journal_region_offset
{
    long payload_offset;
    uint32_t payload_size;
} henka_terrain_journal_region_offset;

struct henka_terrain_storage
{
    henka_terrain_world_desc desc;
    henka_terrain_layout layout;
    char* root_path;
    uint64_t active_transaction_id;
};

static FILE* henka_terrain_open_file(const char* path, const char* mode)
{
    FILE* file = NULL;
#if defined(_WIN32)
    if (fopen_s(&file, path, mode) != 0)
    {
        return NULL;
    }
#else
    file = fopen(path, mode);
#endif
    return file;
}

static void henka_terrain_write_u32(uint8_t* destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8U) & 0xFFU);
    destination[2] = (uint8_t)((value >> 16U) & 0xFFU);
    destination[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static void henka_terrain_write_u64(uint8_t* destination, uint64_t value)
{
    uint32_t index;
    for (index = 0U; index < 8U; ++index)
    {
        destination[index] = (uint8_t)((value >> (index * 8U)) & 0xFFU);
    }
}

static uint32_t henka_terrain_read_u32(const uint8_t* source)
{
    return (uint32_t)source[0] |
        ((uint32_t)source[1] << 8U) |
        ((uint32_t)source[2] << 16U) |
        ((uint32_t)source[3] << 24U);
}

static uint64_t henka_terrain_read_u64(const uint8_t* source)
{
    uint64_t value = 0U;
    uint32_t index;
    for (index = 0U; index < 8U; ++index)
    {
        value |= (uint64_t)source[index] << (index * 8U);
    }
    return value;
}

static uint32_t henka_terrain_checksum(const uint8_t* data, size_t size)
{
    uint32_t checksum = UINT32_C(0xFFFFFFFF);
    size_t index;
    uint32_t bit;
    for (index = 0U; index < size; ++index)
    {
        checksum ^= data[index];
        for (bit = 0U; bit < 8U; ++bit)
        {
            checksum = (checksum & 1U) != 0U
                ? (checksum >> 1U) ^ UINT32_C(0xEDB88320)
                : checksum >> 1U;
        }
    }
    return ~checksum;
}

static bool henka_terrain_weight_sum_is_valid(const henka_terrain_sample* sample)
{
    uint32_t total = 0U;
    uint32_t index;
    for (index = 0U; index < HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT; ++index)
    {
        total += sample->material_weights[index];
    }
    return total == 255U;
}

static henka_result henka_terrain_region_size(
    const henka_terrain_world_desc* desc,
    size_t sample_count,
    size_t* out_size)
{
    henka_terrain_layout layout;
    size_t body_size;
    if (out_size == NULL || henka_terrain_world_desc_get_layout(desc, &layout) != HENKA_SUCCESS ||
        sample_count != layout.samples_per_region ||
        sample_count > (SIZE_MAX - HENKA_TERRAIN_REGION_HEADER_BYTES - HENKA_TERRAIN_REGION_CHECKSUM_BYTES) / 8U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    body_size = sample_count * 8U;
    if (body_size > SIZE_MAX - HENKA_TERRAIN_REGION_HEADER_BYTES - HENKA_TERRAIN_REGION_CHECKSUM_BYTES ||
        HENKA_TERRAIN_REGION_HEADER_BYTES + body_size + HENKA_TERRAIN_REGION_CHECKSUM_BYTES >
            HENKA_TERRAIN_MAX_REGION_RECORD_BYTES)
    {
        return HENKA_ERROR_LIMIT;
    }
    *out_size = HENKA_TERRAIN_REGION_HEADER_BYTES + body_size + HENKA_TERRAIN_REGION_CHECKSUM_BYTES;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_region_encode(
    const henka_terrain_world_desc* desc,
    henka_terrain_region_id region_id,
    henka_terrain_revision revision,
    henka_terrain_generation generation,
    const henka_terrain_sample* samples,
    size_t sample_count,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size)
{
    henka_terrain_layout layout;
    size_t encoded_size;
    size_t index;
    uint8_t* heights;
    uint8_t* weights;

    if (samples == NULL || buffer == NULL ||
        henka_terrain_world_desc_get_layout(desc, &layout) != HENKA_SUCCESS ||
        !henka_terrain_region_id_is_valid(desc, region_id) ||
        henka_terrain_region_size(desc, sample_count, &encoded_size) != HENKA_SUCCESS ||
        buffer_capacity < encoded_size || out_size == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    henka_terrain_write_u32(buffer + 0U, HENKA_TERRAIN_REGION_MAGIC);
    henka_terrain_write_u32(buffer + 4U, HENKA_TERRAIN_FORMAT_VERSION);
    henka_terrain_write_u64(buffer + 8U, desc->world_identity);
    henka_terrain_write_u64(buffer + 16U, desc->base_asset_identity);
    henka_terrain_write_u32(buffer + 24U, (uint32_t)region_id.x);
    henka_terrain_write_u32(buffer + 28U, (uint32_t)region_id.z);
    henka_terrain_write_u64(buffer + 32U, revision);
    henka_terrain_write_u64(buffer + 40U, generation);
    henka_terrain_write_u32(buffer + 48U, layout.samples_per_region_edge);
    henka_terrain_write_u32(buffer + 52U, (uint32_t)sample_count);
    heights = buffer + HENKA_TERRAIN_REGION_HEADER_BYTES;
    weights = heights + sample_count * sizeof(int32_t);
    for (index = 0U; index < sample_count; ++index)
    {
        if (!henka_terrain_weight_sum_is_valid(&samples[index]))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        henka_terrain_write_u32(heights + index * sizeof(int32_t), (uint32_t)samples[index].height_millimeters);
        memcpy(weights + index * HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT,
            samples[index].material_weights,
            HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT);
    }
    henka_terrain_write_u32(buffer + encoded_size - HENKA_TERRAIN_REGION_CHECKSUM_BYTES,
        henka_terrain_checksum(buffer, encoded_size - HENKA_TERRAIN_REGION_CHECKSUM_BYTES));
    *out_size = encoded_size;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_region_decode(
    const henka_terrain_world_desc* desc,
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_region_storage_info* out_info,
    henka_terrain_sample* samples,
    size_t sample_capacity)
{
    henka_terrain_layout layout;
    size_t expected_size;
    size_t index;
    const uint8_t* heights;
    const uint8_t* weights;
    uint32_t sample_count;

    if (desc == NULL || buffer == NULL || out_info == NULL || samples == NULL ||
        henka_terrain_world_desc_get_layout(desc, &layout) != HENKA_SUCCESS ||
        buffer_size < HENKA_TERRAIN_REGION_HEADER_BYTES + HENKA_TERRAIN_REGION_CHECKSUM_BYTES ||
        henka_terrain_read_u32(buffer) != HENKA_TERRAIN_REGION_MAGIC ||
        henka_terrain_read_u32(buffer + 4U) != HENKA_TERRAIN_FORMAT_VERSION ||
        henka_terrain_read_u64(buffer + 8U) != desc->world_identity ||
        henka_terrain_read_u64(buffer + 16U) != desc->base_asset_identity)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    sample_count = henka_terrain_read_u32(buffer + 52U);
    if (henka_terrain_read_u32(buffer + 48U) != layout.samples_per_region_edge ||
        sample_count != layout.samples_per_region || sample_capacity < sample_count ||
        henka_terrain_region_size(desc, sample_count, &expected_size) != HENKA_SUCCESS ||
        buffer_size != expected_size ||
        henka_terrain_read_u32(buffer + expected_size - HENKA_TERRAIN_REGION_CHECKSUM_BYTES) !=
            henka_terrain_checksum(buffer, expected_size - HENKA_TERRAIN_REGION_CHECKSUM_BYTES))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    out_info->id.x = (int32_t)henka_terrain_read_u32(buffer + 24U);
    out_info->id.z = (int32_t)henka_terrain_read_u32(buffer + 28U);
    if (!henka_terrain_region_id_is_valid(desc, out_info->id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    out_info->revision = henka_terrain_read_u64(buffer + 32U);
    out_info->generation = henka_terrain_read_u64(buffer + 40U);
    heights = buffer + HENKA_TERRAIN_REGION_HEADER_BYTES;
    weights = heights + sample_count * sizeof(int32_t);
    for (index = 0U; index < sample_count; ++index)
    {
        samples[index].height_millimeters = (int32_t)henka_terrain_read_u32(heights + index * sizeof(int32_t));
        memcpy(samples[index].material_weights,
            weights + index * HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT,
            HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT);
        if (!henka_terrain_weight_sum_is_valid(&samples[index]))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    return HENKA_SUCCESS;
}

static henka_result henka_terrain_create_directory_tree(const char* path)
{
    char* mutable_path;
    size_t length;
    size_t index;
    if (path == NULL || path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    length = strlen(path);
    mutable_path = henka_malloc(length + 1U);
    if (mutable_path == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    memcpy(mutable_path, path, length + 1U);
    for (index = 1U; index <= length; ++index)
    {
        if (mutable_path[index] != '\0' && mutable_path[index] != '/' && mutable_path[index] != '\\')
        {
            continue;
        }
        mutable_path[index] = '\0';
        if (mutable_path[0] != '\0' &&
#if defined(_WIN32)
            !(length >= 2U && index == 2U && mutable_path[1] == ':') &&
            _mkdir(mutable_path) != 0 && errno != EEXIST
#else
            mkdir(mutable_path, 0777) != 0 && errno != EEXIST
#endif
            )
        {
            henka_free(mutable_path);
            return HENKA_ERROR_PLATFORM;
        }
        mutable_path[index] = path[index];
    }
#if defined(_WIN32)
    if (_mkdir(mutable_path) != 0 && errno != EEXIST)
#else
    if (mkdir(mutable_path, 0777) != 0 && errno != EEXIST)
#endif
    {
        henka_free(mutable_path);
        return HENKA_ERROR_PLATFORM;
    }
    henka_free(mutable_path);
    return HENKA_SUCCESS;
}

static henka_result henka_terrain_storage_resolve(
    const henka_terrain_storage* storage,
    const char* relative_path,
    char** out_path)
{
    return henka_path_resolve_confined(storage->root_path, relative_path, out_path);
}

static henka_result henka_terrain_storage_region_path(
    const henka_terrain_storage* storage,
    henka_terrain_region_id region_id,
    bool temporary,
    char** out_path)
{
    char relative[64];
    int written = snprintf(relative, sizeof(relative), "region_%d_%d.htr%s",
        region_id.x, region_id.z, temporary ? ".tmp" : "");
    if (written <= 0 || (size_t)written >= sizeof(relative))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    return henka_terrain_storage_resolve(storage, relative, out_path);
}

static henka_result henka_terrain_storage_journal_path(
    const henka_terrain_storage* storage,
    char** out_path)
{
    return henka_terrain_storage_resolve(storage, "terrain.journal", out_path);
}

static henka_result henka_terrain_storage_manifest_path(
    const henka_terrain_storage* storage,
    bool temporary,
    char** out_path)
{
    return henka_terrain_storage_resolve(
        storage, temporary ? "terrain.manifest.tmp" : "terrain.manifest", out_path);
}

static void henka_terrain_manifest_write_u32(uint8_t* destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8U) & 0xFFU);
    destination[2] = (uint8_t)((value >> 16U) & 0xFFU);
    destination[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

static void henka_terrain_manifest_write_u64(uint8_t* destination, uint64_t value)
{
    uint32_t index;
    for (index = 0U; index < 8U; ++index)
    {
        destination[index] = (uint8_t)((value >> (index * 8U)) & 0xFFU);
    }
}

static uint32_t henka_terrain_manifest_read_u32(const uint8_t* source)
{
    return (uint32_t)source[0] |
        ((uint32_t)source[1] << 8U) |
        ((uint32_t)source[2] << 16U) |
        ((uint32_t)source[3] << 24U);
}

static uint64_t henka_terrain_manifest_read_u64(const uint8_t* source)
{
    uint64_t value = 0U;
    uint32_t index;
    for (index = 0U; index < 8U; ++index)
    {
        value |= (uint64_t)source[index] << (index * 8U);
    }
    return value;
}

static void henka_terrain_manifest_encode(
    const henka_terrain_world_desc* desc,
    uint8_t buffer[HENKA_TERRAIN_MANIFEST_BYTES])
{
    const uint32_t values[] = {
        desc->world_width_meters,
        desc->world_depth_meters,
        desc->region_edge_meters,
        desc->chunk_edge_meters,
        desc->samples_per_chunk,
        desc->base_sample_spacing_meters,
        desc->chunks_per_region_edge,
        desc->regions_across,
        desc->regions_down,
        desc->max_resident_regions,
        desc->max_resident_chunks,
        desc->max_pending_io,
        desc->max_stream_observers};
    uint32_t index;
    henka_terrain_manifest_write_u32(buffer + 0U, HENKA_TERRAIN_MANIFEST_MAGIC);
    henka_terrain_manifest_write_u32(buffer + 4U, HENKA_TERRAIN_MANIFEST_VERSION);
    henka_terrain_manifest_write_u64(buffer + 8U, desc->world_identity);
    henka_terrain_manifest_write_u64(buffer + 16U, desc->base_asset_identity);
    for (index = 0U; index < sizeof(values) / sizeof(values[0]); ++index)
    {
        henka_terrain_manifest_write_u32(buffer + 24U + index * 4U, values[index]);
    }
    henka_terrain_manifest_write_u32(
        buffer + HENKA_TERRAIN_MANIFEST_BYTES - 4U,
        henka_terrain_checksum(buffer, HENKA_TERRAIN_MANIFEST_BYTES - 4U));
}

static henka_result henka_terrain_manifest_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_world_desc* out_desc)
{
    uint32_t values[13];
    uint32_t index;
    if (buffer == NULL || out_desc == NULL || buffer_size != HENKA_TERRAIN_MANIFEST_BYTES ||
        henka_terrain_manifest_read_u32(buffer) != HENKA_TERRAIN_MANIFEST_MAGIC ||
        henka_terrain_manifest_read_u32(buffer + 4U) != HENKA_TERRAIN_MANIFEST_VERSION ||
        henka_terrain_manifest_read_u32(buffer + HENKA_TERRAIN_MANIFEST_BYTES - 4U) !=
            henka_terrain_checksum(buffer, HENKA_TERRAIN_MANIFEST_BYTES - 4U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_desc = henka_terrain_world_desc_default();
    out_desc->format_version = HENKA_TERRAIN_FORMAT_VERSION;
    out_desc->world_identity = henka_terrain_manifest_read_u64(buffer + 8U);
    out_desc->base_asset_identity = henka_terrain_manifest_read_u64(buffer + 16U);
    for (index = 0U; index < 13U; ++index)
    {
        values[index] = henka_terrain_manifest_read_u32(buffer + 24U + index * 4U);
    }
    out_desc->world_width_meters = values[0];
    out_desc->world_depth_meters = values[1];
    out_desc->region_edge_meters = values[2];
    out_desc->chunk_edge_meters = values[3];
    out_desc->samples_per_chunk = values[4];
    out_desc->base_sample_spacing_meters = values[5];
    out_desc->chunks_per_region_edge = values[6];
    out_desc->regions_across = values[7];
    out_desc->regions_down = values[8];
    out_desc->max_resident_regions = values[9];
    out_desc->max_resident_chunks = values[10];
    out_desc->max_pending_io = values[11];
    out_desc->max_stream_observers = values[12];
    return henka_terrain_world_desc_validate(out_desc);
}

static bool henka_terrain_storage_desc_equal(
    const henka_terrain_world_desc* left,
    const henka_terrain_world_desc* right)
{
    return left->format_version == right->format_version &&
        left->world_identity == right->world_identity &&
        left->base_asset_identity == right->base_asset_identity &&
        left->world_width_meters == right->world_width_meters &&
        left->world_depth_meters == right->world_depth_meters &&
        left->region_edge_meters == right->region_edge_meters &&
        left->chunk_edge_meters == right->chunk_edge_meters &&
        left->samples_per_chunk == right->samples_per_chunk &&
        left->base_sample_spacing_meters == right->base_sample_spacing_meters &&
        left->chunks_per_region_edge == right->chunks_per_region_edge &&
        left->regions_across == right->regions_across &&
        left->regions_down == right->regions_down &&
        left->max_resident_regions == right->max_resident_regions &&
        left->max_resident_chunks == right->max_resident_chunks &&
        left->max_pending_io == right->max_pending_io &&
        left->max_stream_observers == right->max_stream_observers;
}

static henka_result henka_terrain_flush(FILE* file)
{
    if (fflush(file) != 0)
    {
        return HENKA_ERROR_PLATFORM;
    }
#if defined(_WIN32)
    if (_commit(_fileno(file)) != 0)
#else
    if (fsync(fileno(file)) != 0)
#endif
    {
        return HENKA_ERROR_PLATFORM;
    }
    return HENKA_SUCCESS;
}

static henka_result henka_terrain_append_journal_record(
    henka_terrain_storage* storage,
    uint32_t record_type,
    uint64_t transaction_id,
    const uint8_t* payload,
    uint32_t payload_size)
{
    char* path = NULL;
    FILE* file = NULL;
    uint8_t header[HENKA_TERRAIN_JOURNAL_HEADER_BYTES];
    henka_result result;
    if (payload_size > HENKA_TERRAIN_MAX_REGION_RECORD_BYTES ||
        (payload_size > 0U && payload == NULL) ||
        henka_terrain_storage_journal_path(storage, &path) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    file = henka_terrain_open_file(path, "ab");
    if (file == NULL)
    {
        henka_free(path);
        return HENKA_ERROR_PLATFORM;
    }
    henka_terrain_write_u32(header + 0U, HENKA_TERRAIN_JOURNAL_MAGIC);
    henka_terrain_write_u32(header + 4U, record_type);
    henka_terrain_write_u64(header + 8U, transaction_id);
    henka_terrain_write_u32(header + 16U, payload_size);
    result = fwrite(header, sizeof(header), 1U, file) == 1U &&
        (payload_size == 0U || fwrite(payload, payload_size, 1U, file) == 1U)
        ? henka_terrain_flush(file) : HENKA_ERROR_PLATFORM;
    fclose(file);
    henka_free(path);
    return result;
}

static henka_result henka_terrain_storage_replace_snapshot(
    henka_terrain_storage* storage,
    const uint8_t* payload,
    size_t payload_size,
    henka_terrain_region_id region_id)
{
    char* path = NULL;
    char* temporary_path = NULL;
    FILE* file = NULL;
    henka_result result;
    if (henka_terrain_storage_region_path(storage, region_id, false, &path) != HENKA_SUCCESS ||
        henka_terrain_storage_region_path(storage, region_id, true, &temporary_path) != HENKA_SUCCESS)
    {
        henka_free(path);
        henka_free(temporary_path);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    file = henka_terrain_open_file(temporary_path, "wb");
    if (file == NULL)
    {
        henka_free(path);
        henka_free(temporary_path);
        return HENKA_ERROR_PLATFORM;
    }
    result = fwrite(payload, payload_size, 1U, file) == 1U ? henka_terrain_flush(file) : HENKA_ERROR_PLATFORM;
    fclose(file);
    if (result == HENKA_SUCCESS)
    {
#if defined(_WIN32)
        result = MoveFileExA(temporary_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0
            ? HENKA_SUCCESS : HENKA_ERROR_PLATFORM;
#else
        result = rename(temporary_path, path) == 0 ? HENKA_SUCCESS : HENKA_ERROR_PLATFORM;
#endif
    }
    henka_free(path);
    henka_free(temporary_path);
    return result;
}

static henka_result henka_terrain_storage_apply_journal_transaction(
    henka_terrain_storage* storage,
    FILE* file,
    const henka_terrain_journal_region_offset* offsets,
    uint32_t offset_count)
{
    uint8_t* payload = henka_malloc(HENKA_TERRAIN_MAX_REGION_RECORD_BYTES);
    henka_terrain_sample* samples = henka_calloc(storage->layout.samples_per_region, sizeof(*samples));
    uint32_t index;
    henka_result result = HENKA_SUCCESS;
    if (payload == NULL || samples == NULL)
    {
        henka_free(samples);
        henka_free(payload);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < offset_count; ++index)
    {
        henka_terrain_region_storage_info info;
        if (fseek(file, offsets[index].payload_offset, SEEK_SET) != 0 ||
            offsets[index].payload_size > HENKA_TERRAIN_MAX_REGION_RECORD_BYTES ||
            fread(payload, offsets[index].payload_size, 1U, file) != 1U ||
            henka_terrain_region_decode(
                &storage->desc, payload, offsets[index].payload_size, &info,
                samples, storage->layout.samples_per_region) != HENKA_SUCCESS ||
            henka_terrain_storage_replace_snapshot(
                storage, payload, offsets[index].payload_size, info.id) != HENKA_SUCCESS)
        {
            result = HENKA_ERROR_PLATFORM;
            break;
        }
    }
    henka_free(samples);
    henka_free(payload);
    return result;
}

henka_result henka_terrain_storage_create(
    const henka_terrain_world_desc* desc,
    const char* root_path,
    henka_terrain_storage** out_storage)
{
    henka_terrain_storage* storage;
    henka_terrain_layout layout;
    size_t length;
    if (out_storage == NULL || root_path == NULL || root_path[0] == '\0' ||
        henka_terrain_world_desc_get_layout(desc, &layout) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_storage = NULL;
    if (henka_terrain_create_directory_tree(root_path) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_PLATFORM;
    }
    length = strlen(root_path);
    storage = henka_calloc(1U, sizeof(*storage));
    if (storage == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    storage->root_path = henka_malloc(length + 1U);
    if (storage->root_path == NULL)
    {
        henka_free(storage);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    memcpy(storage->root_path, root_path, length + 1U);
    storage->desc = *desc;
    storage->layout = layout;
    *out_storage = storage;
    return HENKA_SUCCESS;
}

void henka_terrain_storage_destroy(henka_terrain_storage* storage)
{
    if (storage == NULL)
    {
        return;
    }
    henka_free(storage->root_path);
    henka_free(storage);
}

henka_result henka_terrain_storage_recover(henka_terrain_storage* storage)
{
    char* path = NULL;
    FILE* file = NULL;
    uint8_t header[HENKA_TERRAIN_JOURNAL_HEADER_BYTES];
    henka_terrain_journal_region_offset offsets[HENKA_TERRAIN_MAX_TRANSACTION_REGIONS];
    uint64_t transaction_id = 0U;
    uint32_t offset_count = 0U;
    bool active = false;
    henka_result result = HENKA_SUCCESS;

    if (storage == NULL || henka_terrain_storage_journal_path(storage, &path) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    file = henka_terrain_open_file(path, "rb");
    henka_free(path);
    if (file == NULL)
    {
        return errno == ENOENT ? HENKA_SUCCESS : HENKA_ERROR_PLATFORM;
    }
    while (fread(header, sizeof(header), 1U, file) == 1U)
    {
        uint32_t record_type = henka_terrain_read_u32(header + 4U);
        uint64_t record_transaction = henka_terrain_read_u64(header + 8U);
        uint32_t payload_size = henka_terrain_read_u32(header + 16U);
        long payload_offset = ftell(file);
        if (henka_terrain_read_u32(header) != HENKA_TERRAIN_JOURNAL_MAGIC ||
            payload_offset < 0L || payload_size > HENKA_TERRAIN_MAX_REGION_RECORD_BYTES)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            break;
        }
        if (record_type == HENKA_TERRAIN_JOURNAL_BEGIN)
        {
            if (payload_size != 0U || record_transaction == 0U)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                break;
            }
            /* A new BEGIN after an incomplete transaction discards that
             * transaction's uncommitted records. */
            active = true;
            transaction_id = record_transaction;
            offset_count = 0U;
        }
        else if (record_type == HENKA_TERRAIN_JOURNAL_REGION)
        {
            if (!active || record_transaction != transaction_id || payload_size == 0U ||
                offset_count >= HENKA_TERRAIN_MAX_TRANSACTION_REGIONS)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                break;
            }
            offsets[offset_count++] = (henka_terrain_journal_region_offset){payload_offset, payload_size};
        }
        else if (record_type == HENKA_TERRAIN_JOURNAL_COMMIT)
        {
            if (!active || record_transaction != transaction_id || payload_size != 0U ||
                henka_terrain_storage_apply_journal_transaction(storage, file, offsets, offset_count) != HENKA_SUCCESS)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                break;
            }
            active = false;
            offset_count = 0U;
        }
        else
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            break;
        }
        if (fseek(file, payload_offset + (long)payload_size, SEEK_SET) != 0)
        {
            result = HENKA_ERROR_PLATFORM;
            break;
        }
    }
    if (ferror(file) != 0)
    {
        result = HENKA_ERROR_PLATFORM;
    }
    fclose(file);
    return result;
}

static henka_result henka_terrain_storage_write_manifest(
    henka_terrain_storage* storage)
{
    char* path = NULL;
    char* temporary_path = NULL;
    FILE* file = NULL;
    uint8_t buffer[HENKA_TERRAIN_MANIFEST_BYTES];
    henka_result result;

    if (storage == NULL ||
        henka_terrain_storage_manifest_path(storage, false, &path) != HENKA_SUCCESS ||
        henka_terrain_storage_manifest_path(storage, true, &temporary_path) != HENKA_SUCCESS)
    {
        henka_free(path);
        henka_free(temporary_path);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    henka_terrain_manifest_encode(&storage->desc, buffer);
    file = henka_terrain_open_file(temporary_path, "wb");
    if (file == NULL)
    {
        henka_free(path);
        henka_free(temporary_path);
        return HENKA_ERROR_PLATFORM;
    }
    result = fwrite(buffer, sizeof(buffer), 1U, file) == 1U
        ? henka_terrain_flush(file) : HENKA_ERROR_PLATFORM;
    fclose(file);
    if (result == HENKA_SUCCESS)
    {
#if defined(_WIN32)
        result = MoveFileExA(
            temporary_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0
            ? HENKA_SUCCESS : HENKA_ERROR_PLATFORM;
#else
        result = rename(temporary_path, path) == 0 ? HENKA_SUCCESS : HENKA_ERROR_PLATFORM;
#endif
    }
    henka_free(path);
    henka_free(temporary_path);
    return result;
}

henka_result henka_terrain_storage_load_manifest(
    henka_terrain_storage* storage,
    henka_terrain_world_desc* out_desc)
{
    char* path = NULL;
    FILE* file = NULL;
    uint8_t buffer[HENKA_TERRAIN_MAX_MANIFEST_BYTES];
    size_t bytes_read;
    if (storage == NULL || out_desc == NULL ||
        henka_terrain_storage_manifest_path(storage, false, &path) != HENKA_SUCCESS)
    {
        henka_free(path);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    file = henka_terrain_open_file(path, "rb");
    henka_free(path);
    if (file == NULL)
    {
        return errno == ENOENT ? HENKA_ERROR_ASSET_SOURCE : HENKA_ERROR_PLATFORM;
    }
    bytes_read = fread(buffer, 1U, sizeof(buffer), file);
    if (ferror(file) != 0 ||
        (bytes_read == sizeof(buffer) && fgetc(file) != EOF) ||
        (bytes_read != sizeof(buffer) && !feof(file)))
    {
        fclose(file);
        return HENKA_ERROR_LIMIT;
    }
    fclose(file);
    return henka_terrain_manifest_decode(buffer, bytes_read, out_desc);
}

henka_result henka_terrain_storage_ensure_manifest(henka_terrain_storage* storage)
{
    henka_terrain_world_desc manifest_desc;
    henka_result result;
    if (storage == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_terrain_storage_load_manifest(storage, &manifest_desc);
    if (result == HENKA_ERROR_ASSET_SOURCE)
    {
        return henka_terrain_storage_write_manifest(storage);
    }
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    return henka_terrain_storage_desc_equal(&manifest_desc, &storage->desc)
        ? HENKA_SUCCESS : HENKA_ERROR_INVALID_ARGUMENT;
}

henka_result henka_terrain_storage_compact(henka_terrain_storage* storage)
{
    char* path = NULL;
    char* temporary_path = NULL;
    FILE* file = NULL;
    henka_result result;
    if (storage == NULL || storage->active_transaction_id != 0U ||
        henka_terrain_storage_journal_path(storage, &path) != HENKA_SUCCESS ||
        henka_terrain_storage_resolve(storage, "terrain.journal.compact", &temporary_path) != HENKA_SUCCESS)
    {
        henka_free(path);
        henka_free(temporary_path);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_terrain_storage_recover(storage);
    if (result != HENKA_SUCCESS)
    {
        henka_free(path);
        henka_free(temporary_path);
        return result;
    }
    file = henka_terrain_open_file(temporary_path, "wb");
    if (file == NULL)
    {
        henka_free(path);
        henka_free(temporary_path);
        return HENKA_ERROR_PLATFORM;
    }
    result = henka_terrain_flush(file);
    fclose(file);
    if (result == HENKA_SUCCESS)
    {
#if defined(_WIN32)
        result = MoveFileExA(temporary_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0
            ? HENKA_SUCCESS : HENKA_ERROR_PLATFORM;
#else
        result = rename(temporary_path, path) == 0 ? HENKA_SUCCESS : HENKA_ERROR_PLATFORM;
#endif
    }
    henka_free(path);
    henka_free(temporary_path);
    return result;
}

henka_result henka_terrain_storage_begin(
    henka_terrain_storage* storage,
    uint64_t transaction_id)
{
    henka_result result;
    if (storage == NULL || transaction_id == 0U || storage->active_transaction_id != 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_terrain_append_journal_record(
        storage, HENKA_TERRAIN_JOURNAL_BEGIN, transaction_id, NULL, 0U);
    if (result == HENKA_SUCCESS)
    {
        storage->active_transaction_id = transaction_id;
    }
    return result;
}

henka_result henka_terrain_storage_write_region(
    henka_terrain_storage* storage,
    henka_terrain_region_id region_id,
    henka_terrain_revision revision,
    henka_terrain_generation generation,
    const henka_terrain_sample* samples,
    size_t sample_count)
{
    uint8_t* buffer;
    size_t encoded_size;
    henka_result result;
    if (storage == NULL || storage->active_transaction_id == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    buffer = henka_malloc(HENKA_TERRAIN_MAX_REGION_RECORD_BYTES);
    if (buffer == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    result = henka_terrain_region_encode(
        &storage->desc, region_id, revision, generation, samples, sample_count,
        buffer, HENKA_TERRAIN_MAX_REGION_RECORD_BYTES, &encoded_size);
    if (result == HENKA_SUCCESS)
    {
        result = henka_terrain_append_journal_record(
            storage, HENKA_TERRAIN_JOURNAL_REGION, storage->active_transaction_id,
            buffer, (uint32_t)encoded_size);
    }
    henka_free(buffer);
    return result;
}

henka_result henka_terrain_storage_commit(
    henka_terrain_storage* storage,
    uint64_t transaction_id)
{
    henka_result result;
    if (storage == NULL || transaction_id == 0U || storage->active_transaction_id != transaction_id)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_terrain_append_journal_record(
        storage, HENKA_TERRAIN_JOURNAL_COMMIT, transaction_id, NULL, 0U);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    storage->active_transaction_id = 0U;
    return henka_terrain_storage_recover(storage);
}

henka_result henka_terrain_storage_abort(
    henka_terrain_storage* storage,
    uint64_t transaction_id)
{
    if (storage == NULL || transaction_id == 0U || storage->active_transaction_id != transaction_id)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    storage->active_transaction_id = 0U;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_storage_load_region(
    henka_terrain_storage* storage,
    henka_terrain_region_id region_id,
    henka_terrain_region_storage_info* out_info,
    henka_terrain_sample* samples,
    size_t sample_capacity)
{
    char* path = NULL;
    FILE* file = NULL;
    uint8_t* buffer = NULL;
    long file_size;
    size_t size;
    henka_result result;
    if (storage == NULL || out_info == NULL || samples == NULL ||
        !henka_terrain_region_id_is_valid(&storage->desc, region_id) ||
        henka_terrain_storage_region_path(storage, region_id, false, &path) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    file = henka_terrain_open_file(path, "rb");
    henka_free(path);
    if (file == NULL)
    {
        return errno == ENOENT ? HENKA_ERROR_ASSET_SOURCE : HENKA_ERROR_PLATFORM;
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (file_size = ftell(file)) < 0L ||
        (uint64_t)file_size > HENKA_TERRAIN_MAX_REGION_RECORD_BYTES ||
        fseek(file, 0L, SEEK_SET) != 0)
    {
        fclose(file);
        return HENKA_ERROR_LIMIT;
    }
    size = (size_t)file_size;
    buffer = henka_malloc(size == 0U ? 1U : size);
    if (buffer == NULL)
    {
        fclose(file);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    result = fread(buffer, size, 1U, file) == 1U
        ? henka_terrain_region_decode(&storage->desc, buffer, size, out_info, samples, sample_capacity)
        : HENKA_ERROR_PLATFORM;
    fclose(file);
    henka_free(buffer);
    return result;
}
