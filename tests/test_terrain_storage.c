#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/terrain_storage.h>

static long test_terrain_storage_file_size(const char* path)
{
    FILE* file = fopen(path, "rb");
    long size;
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0)
    {
        if (file != NULL) { fclose(file); }
        return -1L;
    }
    size = ftell(file);
    fclose(file);
    return size;
}

static int test_codec_and_transaction_recovery(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_sample* samples = NULL;
    henka_terrain_sample* decoded = NULL;
    uint8_t* packet = NULL;
    size_t packet_size = 0U;
    henka_terrain_region_storage_info info;
    henka_terrain_storage* storage = NULL;
    henka_terrain_region_id region = {2, 3};
    uint32_t index;
    int result = 0;

    if (henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS)
    {
        return 0;
    }
    samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
    decoded = henka_calloc(layout.samples_per_region, sizeof(*decoded));
    packet = henka_malloc(HENKA_TERRAIN_MAX_REGION_RECORD_BYTES);
    if (samples == NULL || decoded == NULL || packet == NULL)
    {
        goto cleanup;
    }
    for (index = 0U; index < layout.samples_per_region; ++index)
    {
        samples[index].height_millimeters = (int32_t)index - 1000;
        samples[index].material_weights[0] = 255U;
    }
    if (henka_terrain_region_encode(
            &desc, region, 4U, 9U, samples, layout.samples_per_region,
            packet, HENKA_TERRAIN_MAX_REGION_RECORD_BYTES, &packet_size) != HENKA_SUCCESS ||
        henka_terrain_region_decode(
            &desc, packet, packet_size, &info, decoded, layout.samples_per_region) != HENKA_SUCCESS ||
        !henka_terrain_region_id_equal(info.id, region) || info.revision != 4U ||
        info.generation != 9U || memcmp(samples, decoded, layout.samples_per_region * sizeof(*samples)) != 0)
    {
        goto cleanup;
    }
    packet[packet_size - 1U] ^= 1U;
    if (henka_terrain_region_decode(
            &desc, packet, packet_size, &info, decoded, layout.samples_per_region) == HENKA_SUCCESS)
    {
        goto cleanup;
    }
    packet[packet_size - 1U] ^= 1U;
    if (henka_terrain_storage_create(&desc, "build/test_tmp/terrain_storage_v1", &storage) != HENKA_SUCCESS ||
        henka_terrain_storage_ensure_manifest(storage) != HENKA_SUCCESS ||
        henka_terrain_storage_load_manifest(storage, &desc) != HENKA_SUCCESS ||
        henka_terrain_storage_begin(storage, 100U) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(
            storage, region, 4U, 9U, samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_storage_commit(storage, 100U) != HENKA_SUCCESS ||
        henka_terrain_storage_load_region(
            storage, region, &info, decoded, layout.samples_per_region) != HENKA_SUCCESS ||
        info.revision != 4U || memcmp(samples, decoded, layout.samples_per_region * sizeof(*samples)) != 0)
    {
        goto cleanup;
    }
    samples[0].height_millimeters = 7777;
    if (henka_terrain_storage_begin(storage, 101U) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(
            storage, region, 5U, 10U, samples, layout.samples_per_region) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_storage_destroy(storage);
    storage = NULL;
    if (henka_terrain_storage_create(&desc, "build/test_tmp/terrain_storage_v1", &storage) != HENKA_SUCCESS ||
        henka_terrain_storage_recover(storage) != HENKA_SUCCESS ||
        henka_terrain_storage_load_region(
            storage, region, &info, decoded, layout.samples_per_region) != HENKA_SUCCESS ||
        info.revision != 4U || decoded[0].height_millimeters != -1000)
    {
        goto cleanup;
    }
    if (henka_terrain_storage_begin(storage, 102U) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(
            storage, region, 5U, 10U, samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_storage_commit(storage, 102U) != HENKA_SUCCESS ||
        henka_terrain_storage_load_region(
            storage, region, &info, decoded, layout.samples_per_region) != HENKA_SUCCESS ||
        info.revision != 5U || decoded[0].height_millimeters != 7777)
    {
        goto cleanup;
    }
    if (henka_terrain_storage_compact(storage) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_storage_destroy(storage);
    storage = NULL;
    if (henka_terrain_storage_create(&desc, "build/test_tmp/terrain_storage_v1", &storage) != HENKA_SUCCESS ||
        henka_terrain_storage_recover(storage) != HENKA_SUCCESS ||
        henka_terrain_storage_load_region(
            storage, region, &info, decoded, layout.samples_per_region) != HENKA_SUCCESS ||
        info.revision != 5U || decoded[0].height_millimeters != 7777)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_storage_destroy(storage);
    henka_free(packet);
    henka_free(decoded);
    henka_free(samples);
    return result;
}

static int test_save_resident_regions_transactionally(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_terrain_sample* samples = NULL;
    henka_terrain_sample* decoded = NULL;
    henka_terrain_region_storage_info info;
    henka_terrain_region_state state;
    henka_terrain_region_id first = {0, 0};
    henka_terrain_region_id second = {1, 0};
    uint32_t saved_count = 0U;
    uint32_t index;
    int result = 0;

    desc.max_resident_regions = 2U;
    if (henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&desc, &world) != HENKA_SUCCESS ||
        henka_terrain_storage_create(&desc, "terrain_storage_resident_save", &storage) != HENKA_SUCCESS ||
        henka_terrain_storage_ensure_manifest(storage) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
    decoded = henka_calloc(layout.samples_per_region, sizeof(*decoded));
    if (samples == NULL || decoded == NULL)
    {
        goto cleanup;
    }
    for (index = 0U; index < layout.samples_per_region; ++index)
    {
        samples[index].height_millimeters = 100;
        samples[index].material_weights[0] = 255U;
    }
    if (henka_terrain_world_apply_region_snapshot(
            world, (henka_terrain_region_storage_info){first, 3U, 1U},
            samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_apply_region_snapshot(
            world, (henka_terrain_region_storage_info){second, 4U, 1U},
            samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_revision(world, first, 3U, 1U, true) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_revision(world, second, 4U, 1U, true) != HENKA_SUCCESS ||
        henka_terrain_storage_save_resident_regions(
            storage, world, 200U, &saved_count) != HENKA_SUCCESS ||
        saved_count != 2U ||
        henka_terrain_storage_load_region(
            storage, first, &info, decoded, layout.samples_per_region) != HENKA_SUCCESS ||
        info.revision != 3U ||
        henka_terrain_storage_load_region(
            storage, second, &info, decoded, layout.samples_per_region) != HENKA_SUCCESS ||
        info.revision != 4U ||
        henka_terrain_world_get_region_state(world, first, &state) != HENKA_SUCCESS ||
        state.dirty ||
        henka_terrain_world_get_region_state(world, second, &state) != HENKA_SUCCESS ||
        state.dirty)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(world);
    henka_free(decoded);
    henka_free(samples);
    return result;
}

static int test_committed_journal_stays_bounded_across_repeated_runs(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_storage* storage = NULL;
    henka_terrain_sample* samples = NULL;
    uint32_t index;
    uint64_t transaction_id;
    int result = 0;

    if (henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_storage_create(&desc, "build/test_tmp/terrain_storage_bounded_v1", &storage) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
    if (samples == NULL)
    {
        goto cleanup;
    }
    for (index = 0U; index < layout.samples_per_region; ++index)
    {
        samples[index].material_weights[0] = 255U;
    }
    for (transaction_id = 1U; transaction_id <= 8U; ++transaction_id)
    {
        if (henka_terrain_storage_begin(storage, transaction_id) != HENKA_SUCCESS ||
            henka_terrain_storage_write_region(
                storage, (henka_terrain_region_id){0, 0},
                (henka_terrain_revision)transaction_id, transaction_id,
                samples, layout.samples_per_region) != HENKA_SUCCESS ||
            henka_terrain_storage_commit(storage, transaction_id) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (test_terrain_storage_file_size("build/test_tmp/terrain_storage_bounded_v1/terrain.journal") < 0L ||
        test_terrain_storage_file_size("build/test_tmp/terrain_storage_bounded_v1/terrain.journal") >=
            (long)HENKA_TERRAIN_STORAGE_AUTO_COMPACT_THRESHOLD_BYTES)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_storage_destroy(storage);
    henka_free(samples);
    return result;
}

int main(void)
{
    return test_codec_and_transaction_recovery() &&
        test_save_resident_regions_transactionally() &&
        test_committed_journal_stays_bounded_across_repeated_runs() ? 0 : 1;
}
