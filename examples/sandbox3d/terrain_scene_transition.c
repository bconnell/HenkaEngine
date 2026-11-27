#include "terrain_scene_transition.h"

#include <string.h>

#include <henka/memory.h>

typedef struct sandbox3d_terrain_region_snapshot
{
    henka_terrain_region_state state;
    henka_terrain_region_storage_info baseline_info;
    henka_terrain_sample* previous_samples;
    henka_terrain_sample* baseline_samples;
} sandbox3d_terrain_region_snapshot;

static uint64_t sandbox3d_hash_u32(uint64_t hash, uint32_t value)
{
    unsigned int byte_index;
    for (byte_index = 0U; byte_index < 4U; ++byte_index)
    {
        hash ^= (uint8_t)(value >> (byte_index * 8U));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void sandbox3d_terrain_transition_free_snapshots(
    sandbox3d_terrain_region_snapshot* snapshots,
    uint32_t snapshot_count)
{
    uint32_t index;
    if (snapshots == NULL)
    {
        return;
    }
    for (index = 0U; index < snapshot_count; ++index)
    {
        henka_free(snapshots[index].previous_samples);
        henka_free(snapshots[index].baseline_samples);
    }
    henka_free(snapshots);
}

static henka_result sandbox3d_terrain_transition_restore_snapshot(
    henka_terrain_world* world,
    const sandbox3d_terrain_region_snapshot* snapshot,
    size_t sample_count)
{
    henka_result result;
    const henka_terrain_region_storage_info info = {
        snapshot->state.id,
        snapshot->state.revision,
        snapshot->state.generation};

    result = henka_terrain_world_apply_region_snapshot(
        world, info, snapshot->previous_samples, sample_count);
    if (result == HENKA_SUCCESS)
    {
        result = henka_terrain_world_set_region_revision(
            world,
            snapshot->state.id,
            snapshot->state.revision,
            snapshot->state.generation,
            snapshot->state.dirty);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_terrain_world_set_region_residency(
            world,
            snapshot->state.id,
            snapshot->state.physics_resident,
            snapshot->state.render_resident,
            snapshot->state.pending_io);
    }
    return result;
}

static bool sandbox3d_terrain_transition_samples_valid(
    const henka_terrain_sample* samples,
    size_t sample_count)
{
    size_t index;
    if (samples == NULL || sample_count == 0U)
    {
        return false;
    }
    for (index = 0U; index < sample_count; ++index)
    {
        if (!henka_terrain_sample_is_valid(&samples[index]))
        {
            return false;
        }
    }
    return true;
}

henka_result sandbox3d_terrain_scene_transition_reload_discard(
    const sandbox3d_terrain_transition_reload_desc* desc,
    sandbox3d_terrain_transition_reload_report* out_report)
{
    henka_terrain_world_desc world_desc;
    henka_terrain_layout layout = {0};
    henka_terrain_world_stats world_stats = {0};
    henka_terrain_edit_history_stats history_stats;
    henka_terrain_edit_history_desc history_desc;
    henka_terrain_edit_history* replacement_history = NULL;
    sandbox3d_terrain_region_snapshot* snapshots = NULL;
    uint32_t snapshot_count = 0U;
    uint32_t snapshot_allocation_count = 0U;
    uint32_t applied_count = 0U;
    uint32_t resident_index;
    uint32_t dirty_count = 0U;
    size_t sample_bytes;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;
    const char* failure_stage = "validate";
    bool rollback_succeeded = true;

    if (out_report != NULL)
    {
        *out_report = (sandbox3d_terrain_transition_reload_report){0};
    }
    if (desc == NULL || desc->edit_history == NULL ||
        desc->reload_scene_document == NULL)
    {
        goto cleanup;
    }

    /* Scene replacement can leave the optional terrain runtime absent. In
     * that state there is no terrain edit/history to roll back; reload the
     * scene document directly instead of rejecting the scene transition. */
    if (desc->world == NULL && *desc->edit_history == NULL)
    {
        failure_stage = "reload_scene_document";
        result = desc->reload_scene_document(desc->user_data);
        if (result == HENKA_SUCCESS)
        {
            failure_stage = NULL;
        }
        goto cleanup;
    }

    if (desc->world == NULL || *desc->edit_history == NULL ||
        desc->load_baseline == NULL || desc->refresh_collision == NULL ||
        desc->refresh_render == NULL)
    {
        goto cleanup;
    }

    failure_stage = "read_world_state";
    result = henka_terrain_world_get_desc(desc->world, &world_desc);
    if (result == HENKA_SUCCESS)
    {
        result = henka_terrain_world_desc_get_layout(&world_desc, &layout);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_terrain_world_get_stats(desc->world, &world_stats);
    }
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    dirty_count = world_stats.dirty_region_count;
    if (out_report != NULL)
    {
        out_report->dirty_region_count_before = dirty_count;
    }
    if (dirty_count > world_stats.resident_region_count ||
        world_stats.resident_region_count > world_stats.max_resident_regions ||
        layout.samples_per_region == 0U ||
        layout.samples_per_region > SIZE_MAX / sizeof(henka_terrain_sample))
    {
        result = HENKA_ERROR_LIMIT;
        goto cleanup;
    }
    sample_bytes = layout.samples_per_region * sizeof(henka_terrain_sample);

    failure_stage = "allocate_snapshots";
    if (dirty_count > 0U)
    {
        snapshots = henka_calloc(
            dirty_count, sizeof(*snapshots));
        if (snapshots == NULL)
        {
            result = HENKA_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }
    }
    for (resident_index = 0U;
         resident_index < world_stats.resident_region_count;
         ++resident_index)
    {
        sandbox3d_terrain_region_snapshot* snapshot;
        henka_terrain_region_state region_state;
        const henka_terrain_sample* current_samples = NULL;
        size_t current_sample_count = 0U;

        failure_stage = "collect_resident_region";
        result = henka_terrain_world_get_resident_region_at(
            desc->world, resident_index, &region_state);
        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        if (!region_state.dirty)
        {
            continue;
        }
        if (snapshot_count >= dirty_count)
        {
            result = HENKA_ERROR_UNKNOWN;
            failure_stage = "dirty_region_count_changed";
            goto cleanup;
        }
        if (!region_state.cpu_resident || region_state.pending_io)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            failure_stage = "dirty_region_not_stable";
            goto cleanup;
        }
        snapshot = &snapshots[snapshot_count];
        snapshot->state = region_state;
        result = henka_terrain_world_get_region_samples(
            desc->world, snapshot->state.id,
            &current_samples, &current_sample_count);
        if (result != HENKA_SUCCESS ||
            current_sample_count != layout.samples_per_region ||
            !sandbox3d_terrain_transition_samples_valid(
                current_samples, current_sample_count))
        {
            result = result == HENKA_SUCCESS
                ? HENKA_ERROR_INVALID_ARGUMENT
                : result;
            failure_stage = "snapshot_live_region";
            goto cleanup;
        }
        snapshot_allocation_count = snapshot_count + 1U;
        snapshot->previous_samples = henka_malloc(sample_bytes);
        snapshot->baseline_samples = henka_malloc(sample_bytes);
        if (snapshot->previous_samples == NULL ||
            snapshot->baseline_samples == NULL)
        {
            result = HENKA_ERROR_OUT_OF_MEMORY;
            failure_stage = "allocate_region_buffers";
            goto cleanup;
        }
        memcpy(snapshot->previous_samples, current_samples, sample_bytes);

        failure_stage = "load_region_baseline";
        result = desc->load_baseline(
            desc->user_data,
            snapshot->state.id,
            &snapshot->baseline_info,
            snapshot->baseline_samples,
            layout.samples_per_region);
        if (result != HENKA_SUCCESS ||
            !henka_terrain_region_id_equal(
                snapshot->baseline_info.id, snapshot->state.id) ||
            !sandbox3d_terrain_transition_samples_valid(
                snapshot->baseline_samples, layout.samples_per_region))
        {
            result = result == HENKA_SUCCESS
                ? HENKA_ERROR_INVALID_ARGUMENT
                : result;
            goto cleanup;
        }
        ++snapshot_count;
    }
    if (snapshot_count != dirty_count)
    {
        result = HENKA_ERROR_UNKNOWN;
        failure_stage = "dirty_region_count_mismatch";
        goto cleanup;
    }

    failure_stage = "prepare_empty_edit_history";
    henka_terrain_edit_history_get_stats(*desc->edit_history, &history_stats);
    history_desc.max_entries = history_stats.max_entries;
    history_desc.max_bytes = history_stats.max_bytes;
    result = henka_terrain_edit_history_create(
        desc->world, &history_desc, &replacement_history);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    for (applied_count = 0U; applied_count < snapshot_count; ++applied_count)
    {
        failure_stage = "apply_region_baseline";
        result = henka_terrain_world_apply_region_snapshot(
            desc->world,
            snapshots[applied_count].baseline_info,
            snapshots[applied_count].baseline_samples,
            layout.samples_per_region);
        if (result != HENKA_SUCCESS)
        {
            goto rollback;
        }
    }

    if (snapshot_count > 0U)
    {
        failure_stage = "refresh_collision";
        result = desc->refresh_collision(desc->user_data);
        if (result != HENKA_SUCCESS)
        {
            goto rollback;
        }
        failure_stage = "refresh_render";
        result = desc->refresh_render(desc->user_data);
        if (result != HENKA_SUCCESS)
        {
            goto rollback;
        }
    }
    failure_stage = "reload_scene_document";
    result = desc->reload_scene_document(desc->user_data);
    if (result != HENKA_SUCCESS)
    {
        goto rollback;
    }

    {
        henka_terrain_edit_history* previous_history = *desc->edit_history;
        *desc->edit_history = replacement_history;
        replacement_history = NULL;
        henka_terrain_edit_history_destroy(previous_history);
    }
    if (out_report != NULL)
    {
        out_report->restored_region_count = snapshot_count;
        if (henka_terrain_world_get_stats(desc->world, &world_stats) ==
            HENKA_SUCCESS)
        {
            out_report->dirty_region_count_after =
                world_stats.dirty_region_count;
        }
    }
    result = HENKA_SUCCESS;
    failure_stage = NULL;
    goto cleanup;

rollback:
    if (applied_count > 0U)
    {
        uint32_t rollback_index = applied_count;
        if (out_report != NULL)
        {
            out_report->rollback_attempted = true;
        }
        while (rollback_index > 0U)
        {
            --rollback_index;
            if (sandbox3d_terrain_transition_restore_snapshot(
                    desc->world,
                    &snapshots[rollback_index],
                    layout.samples_per_region) != HENKA_SUCCESS)
            {
                rollback_succeeded = false;
            }
        }
        if (desc->refresh_collision(desc->user_data) != HENKA_SUCCESS)
        {
            rollback_succeeded = false;
        }
        if (desc->refresh_render(desc->user_data) != HENKA_SUCCESS)
        {
            rollback_succeeded = false;
        }
        if (out_report != NULL)
        {
            out_report->rollback_succeeded = rollback_succeeded;
        }
        if (!rollback_succeeded)
        {
            result = HENKA_ERROR_UNKNOWN;
            failure_stage = "rollback_failed";
        }
    }

cleanup:
    if (out_report != NULL)
    {
        out_report->failure_stage = failure_stage;
        out_report->operation_result = result;
        if (desc != NULL && desc->world != NULL &&
            henka_terrain_world_get_stats(desc->world, &world_stats) ==
                HENKA_SUCCESS)
        {
            out_report->dirty_region_count_after =
                world_stats.dirty_region_count;
        }
    }
    henka_terrain_edit_history_destroy(replacement_history);
    sandbox3d_terrain_transition_free_snapshots(
        snapshots, snapshot_allocation_count);
    return result;
}

henka_result sandbox3d_terrain_world_content_hash(
    const henka_terrain_world* world,
    uint64_t* out_hash)
{
    henka_terrain_world_desc world_desc;
    henka_terrain_layout layout;
    henka_terrain_world_stats world_stats;
    uint64_t hash = UINT64_C(14695981039346656037);
    uint32_t region_index;

    if (world == NULL || out_hash == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (out_hash != NULL)
    {
        *out_hash = 0U;
    }
    if (henka_terrain_world_get_desc(world, &world_desc) != HENKA_SUCCESS ||
        henka_terrain_world_desc_get_layout(&world_desc, &layout) !=
            HENKA_SUCCESS ||
        henka_terrain_world_get_stats(world, &world_stats) != HENKA_SUCCESS ||
        world_stats.resident_region_count > world_stats.max_resident_regions)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    hash = sandbox3d_hash_u32(hash, world_stats.resident_region_count);
    for (region_index = 0U;
         region_index < world_stats.resident_region_count;
         ++region_index)
    {
        henka_terrain_region_state region_state;
        const henka_terrain_sample* samples = NULL;
        size_t sample_count = 0U;
        size_t sample_index;

        if (henka_terrain_world_get_resident_region_at(
                world, region_index, &region_state) != HENKA_SUCCESS ||
            henka_terrain_world_get_region_samples(
                world, region_state.id, &samples, &sample_count) !=
                HENKA_SUCCESS ||
            sample_count != layout.samples_per_region)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        hash = sandbox3d_hash_u32(hash, (uint32_t)region_state.id.x);
        hash = sandbox3d_hash_u32(hash, (uint32_t)region_state.id.z);
        hash = sandbox3d_hash_u32(hash, (uint32_t)sample_count);
        for (sample_index = 0U; sample_index < sample_count; ++sample_index)
        {
            unsigned int weight_index;
            hash = sandbox3d_hash_u32(
                hash, (uint32_t)samples[sample_index].height_millimeters);
            for (weight_index = 0U;
                 weight_index < HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT;
                 ++weight_index)
            {
                hash ^= samples[sample_index].material_weights[weight_index];
                hash *= UINT64_C(1099511628211);
            }
        }
    }
    *out_hash = hash;
    return HENKA_SUCCESS;
}
