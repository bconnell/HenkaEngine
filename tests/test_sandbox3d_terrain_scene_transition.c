#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/terrain_edit_history.h>

#include "../examples/sandbox3d/terrain_scene_transition.h"

typedef struct test_context
{
    henka_terrain_world* world;
    henka_terrain_edit_history* history;
    henka_terrain_layout layout;
    henka_terrain_region_storage_info baseline_info;
    henka_terrain_sample* baseline_samples;
    size_t sample_count;
    henka_result reload_result;
    uint32_t baseline_load_count;
    uint32_t collision_refresh_count;
    uint32_t render_refresh_count;
    uint32_t scene_reload_count;
} test_context;

static bool test_context_create(test_context* context)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    size_t index;

    memset(context, 0, sizeof(*context));
    context->reload_result = HENKA_SUCCESS;
    world_desc.max_resident_regions = 2U;
    if (henka_terrain_world_desc_get_layout(&world_desc, &context->layout) !=
            HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &context->world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(
            context->world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS)
    {
        return false;
    }
    context->sample_count = context->layout.samples_per_region;
    context->baseline_samples = henka_calloc(
        context->sample_count, sizeof(*context->baseline_samples));
    if (context->baseline_samples == NULL)
    {
        return false;
    }
    for (index = 0U; index < context->sample_count; ++index)
    {
        context->baseline_samples[index].height_millimeters = 125;
        context->baseline_samples[index].material_weights[0] = 255U;
    }
    context->baseline_info = (henka_terrain_region_storage_info){
        {0, 0}, 4U, 2U};
    if (henka_terrain_world_apply_region_snapshot(
            context->world,
            context->baseline_info,
            context->baseline_samples,
            context->sample_count) != HENKA_SUCCESS ||
        henka_terrain_edit_history_create(
            context->world, NULL, &context->history) != HENKA_SUCCESS)
    {
        return false;
    }
    return true;
}

static void test_context_destroy(test_context* context)
{
    if (context == NULL)
    {
        return;
    }
    henka_terrain_edit_history_destroy(context->history);
    henka_terrain_world_destroy(context->world);
    henka_free(context->baseline_samples);
    memset(context, 0, sizeof(*context));
}

static bool test_context_edit(test_context* context)
{
    henka_terrain_edit_command command = henka_terrain_edit_command_default();
    command.operation = HENKA_TERRAIN_EDIT_RAISE;
    command.center_sample_x = 24;
    command.center_sample_z = 24;
    command.radius_samples = 4U;
    command.value_millimeters = 800;
    return henka_terrain_edit_history_apply(
        context->history, &command, 5U) == HENKA_SUCCESS;
}

static henka_result test_load_baseline(
    void* user_data,
    henka_terrain_region_id region_id,
    henka_terrain_region_storage_info* out_info,
    henka_terrain_sample* out_samples,
    size_t sample_count)
{
    test_context* context = (test_context*)user_data;
    if (context == NULL || out_info == NULL || out_samples == NULL ||
        !henka_terrain_region_id_equal(region_id, context->baseline_info.id) ||
        sample_count != context->sample_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    ++context->baseline_load_count;
    *out_info = context->baseline_info;
    memcpy(out_samples, context->baseline_samples,
        sample_count * sizeof(*out_samples));
    return HENKA_SUCCESS;
}

static henka_result test_refresh_collision(void* user_data)
{
    test_context* context = (test_context*)user_data;
    if (context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    ++context->collision_refresh_count;
    return HENKA_SUCCESS;
}

static henka_result test_refresh_render(void* user_data)
{
    test_context* context = (test_context*)user_data;
    if (context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    ++context->render_refresh_count;
    return HENKA_SUCCESS;
}

static henka_result test_reload_scene(void* user_data)
{
    test_context* context = (test_context*)user_data;
    if (context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    ++context->scene_reload_count;
    return context->reload_result;
}

static sandbox3d_terrain_transition_reload_desc test_make_desc(
    test_context* context)
{
    return (sandbox3d_terrain_transition_reload_desc){
        context->world,
        &context->history,
        test_load_baseline,
        test_refresh_collision,
        test_refresh_render,
        test_reload_scene,
        context};
}

static bool test_reload_without_terrain_runtime(void)
{
    sandbox3d_terrain_transition_reload_report report;
    henka_terrain_edit_history* history = NULL;
    test_context context = {0};
    sandbox3d_terrain_transition_reload_desc desc = {
        NULL,
        &history,
        test_load_baseline,
        test_refresh_collision,
        test_refresh_render,
        test_reload_scene,
        &context};

    context.reload_result = HENKA_SUCCESS;
    if (sandbox3d_terrain_scene_transition_reload_discard(&desc, &report) !=
            HENKA_SUCCESS ||
        report.operation_result != HENKA_SUCCESS ||
        report.failure_stage != NULL ||
        report.dirty_region_count_before != 0U ||
        report.restored_region_count != 0U ||
        report.dirty_region_count_after != 0U ||
        report.rollback_attempted || report.rollback_succeeded ||
        context.baseline_load_count != 0U ||
        context.collision_refresh_count != 0U ||
        context.render_refresh_count != 0U ||
        context.scene_reload_count != 1U || history != NULL)
    {
        return false;
    }
    return true;
}

static bool test_reload_restores_dirty_terrain_and_retires_history(void)
{
    test_context context;
    sandbox3d_terrain_transition_reload_desc desc;
    sandbox3d_terrain_transition_reload_report report;
    henka_terrain_world_stats world_stats;
    henka_terrain_edit_history_stats history_stats;
    const henka_terrain_sample* samples = NULL;
    uint64_t baseline_hash = 0U;
    uint64_t edited_hash = 0U;
    uint64_t restored_hash = 0U;
    bool success = false;

    if (!test_context_create(&context) ||
        sandbox3d_terrain_world_content_hash(
            context.world, &baseline_hash) != HENKA_SUCCESS ||
        !test_context_edit(&context) ||
        sandbox3d_terrain_world_content_hash(
            context.world, &edited_hash) != HENKA_SUCCESS ||
        baseline_hash == edited_hash)
    {
        test_context_destroy(&context);
        return false;
    }
    desc = test_make_desc(&context);
    if (sandbox3d_terrain_scene_transition_reload_discard(&desc, &report) !=
            HENKA_SUCCESS ||
        report.operation_result != HENKA_SUCCESS ||
        report.failure_stage != NULL ||
        report.dirty_region_count_before != 1U ||
        report.restored_region_count != 1U ||
        report.dirty_region_count_after != 0U ||
        report.rollback_attempted ||
        context.baseline_load_count != 1U ||
        context.collision_refresh_count != 1U ||
        context.render_refresh_count != 1U ||
        context.scene_reload_count != 1U ||
        henka_terrain_world_get_region_samples(
            context.world, (henka_terrain_region_id){0, 0},
            &samples, &context.sample_count) != HENKA_SUCCESS ||
        memcmp(samples, context.baseline_samples,
            context.sample_count * sizeof(*samples)) != 0 ||
        henka_terrain_world_get_stats(context.world, &world_stats) !=
            HENKA_SUCCESS ||
        world_stats.dirty_region_count != 0U ||
        sandbox3d_terrain_world_content_hash(
            context.world, &restored_hash) != HENKA_SUCCESS ||
        restored_hash != baseline_hash)
    {
        test_context_destroy(&context);
        return false;
    }
    henka_terrain_edit_history_get_stats(context.history, &history_stats);
    success = !history_stats.can_undo && !history_stats.can_redo;
    test_context_destroy(&context);
    return success;
}

static bool test_reload_clean_terrain_retires_history_without_changing_samples(void)
{
    test_context context;
    sandbox3d_terrain_transition_reload_desc desc;
    sandbox3d_terrain_transition_reload_report report;
    henka_terrain_region_state region_state;
    henka_terrain_world_stats world_stats;
    henka_terrain_edit_history_stats history_stats;
    const henka_terrain_sample* samples = NULL;
    size_t sample_count = 0U;
    uint64_t saved_content_hash = 0U;
    uint64_t reloaded_content_hash = 0U;
    bool success = false;

    if (!test_context_create(&context) || !test_context_edit(&context) ||
        henka_terrain_world_get_region_state(
            context.world, (henka_terrain_region_id){0, 0},
            &region_state) != HENKA_SUCCESS ||
        henka_terrain_world_set_region_revision(
            context.world,
            region_state.id,
            region_state.revision,
            region_state.generation,
            false) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(
            context.world, (henka_terrain_region_id){0, 0},
            &samples, &sample_count) != HENKA_SUCCESS ||
        sample_count != context.sample_count ||
        sandbox3d_terrain_world_content_hash(
            context.world, &saved_content_hash) != HENKA_SUCCESS)
    {
        test_context_destroy(&context);
        return false;
    }
    henka_terrain_edit_history_get_stats(context.history, &history_stats);
    if (!history_stats.can_undo)
    {
        test_context_destroy(&context);
        return false;
    }

    desc = test_make_desc(&context);
    if (sandbox3d_terrain_scene_transition_reload_discard(&desc, &report) !=
            HENKA_SUCCESS ||
        report.operation_result != HENKA_SUCCESS ||
        report.failure_stage != NULL ||
        report.dirty_region_count_before != 0U ||
        report.restored_region_count != 0U ||
        report.dirty_region_count_after != 0U ||
        report.rollback_attempted ||
        context.baseline_load_count != 0U ||
        context.collision_refresh_count != 0U ||
        context.render_refresh_count != 0U ||
        context.scene_reload_count != 1U ||
        sandbox3d_terrain_world_content_hash(
            context.world, &reloaded_content_hash) != HENKA_SUCCESS ||
        reloaded_content_hash != saved_content_hash ||
        henka_terrain_world_get_stats(context.world, &world_stats) !=
            HENKA_SUCCESS ||
        world_stats.dirty_region_count != 0U)
    {
        test_context_destroy(&context);
        return false;
    }
    henka_terrain_edit_history_get_stats(context.history, &history_stats);
    success = !history_stats.can_undo && !history_stats.can_redo;
    test_context_destroy(&context);
    return success;
}

static bool test_reload_failure_restores_terrain_and_history(void)
{
    test_context context;
    sandbox3d_terrain_transition_reload_desc desc;
    sandbox3d_terrain_transition_reload_report report;
    henka_terrain_region_state before_state;
    henka_terrain_region_state after_state;
    henka_terrain_world_stats world_stats;
    henka_terrain_edit_history_stats history_stats;
    const henka_terrain_sample* samples = NULL;
    henka_terrain_sample* edited_samples = NULL;
    henka_terrain_edit_command undone;
    size_t sample_count = 0U;
    henka_terrain_edit_history* previous_history;
    bool success = false;

    if (!test_context_create(&context) || !test_context_edit(&context) ||
        henka_terrain_world_get_region_samples(
            context.world, (henka_terrain_region_id){0, 0},
            &samples, &sample_count) != HENKA_SUCCESS ||
        sample_count != context.sample_count)
    {
        test_context_destroy(&context);
        return false;
    }
    edited_samples = henka_malloc(sample_count * sizeof(*edited_samples));
    if (edited_samples == NULL ||
        henka_terrain_world_get_region_state(
            context.world, (henka_terrain_region_id){0, 0},
            &before_state) != HENKA_SUCCESS)
    {
        henka_free(edited_samples);
        test_context_destroy(&context);
        return false;
    }
    memcpy(edited_samples, samples, sample_count * sizeof(*edited_samples));
    previous_history = context.history;
    context.reload_result = HENKA_ERROR_PLATFORM;
    desc = test_make_desc(&context);
    if (sandbox3d_terrain_scene_transition_reload_discard(&desc, &report) !=
            HENKA_ERROR_PLATFORM ||
        report.operation_result != HENKA_ERROR_PLATFORM ||
        report.failure_stage == NULL ||
        strcmp(report.failure_stage, "reload_scene_document") != 0 ||
        report.dirty_region_count_before != 1U ||
        report.dirty_region_count_after != 1U ||
        !report.rollback_attempted || !report.rollback_succeeded ||
        context.history != previous_history ||
        context.collision_refresh_count != 2U ||
        context.render_refresh_count != 2U ||
        henka_terrain_world_get_region_samples(
            context.world, (henka_terrain_region_id){0, 0},
            &samples, &sample_count) != HENKA_SUCCESS ||
        memcmp(samples, edited_samples,
            sample_count * sizeof(*samples)) != 0 ||
        henka_terrain_world_get_region_state(
            context.world, (henka_terrain_region_id){0, 0},
            &after_state) != HENKA_SUCCESS ||
        after_state.revision != before_state.revision ||
        after_state.generation != before_state.generation ||
        after_state.dirty != before_state.dirty ||
        after_state.cpu_resident != before_state.cpu_resident ||
        after_state.physics_resident != before_state.physics_resident ||
        after_state.render_resident != before_state.render_resident ||
        after_state.pending_io != before_state.pending_io)
    {
        henka_free(edited_samples);
        test_context_destroy(&context);
        return false;
    }
    henka_terrain_edit_history_get_stats(context.history, &history_stats);
    if (!history_stats.can_undo ||
        henka_terrain_world_get_stats(context.world, &world_stats) !=
            HENKA_SUCCESS ||
        world_stats.dirty_region_count != 1U ||
        henka_terrain_edit_history_undo(context.history, &undone) !=
            HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(
            context.world, (henka_terrain_region_id){0, 0},
            &samples, &sample_count) != HENKA_SUCCESS ||
        memcmp(samples, context.baseline_samples,
            sample_count * sizeof(*samples)) != 0)
    {
        henka_free(edited_samples);
        test_context_destroy(&context);
        return false;
    }
    success = true;
    henka_free(edited_samples);
    test_context_destroy(&context);
    return success;
}

int main(void)
{
    if (!test_reload_without_terrain_runtime())
    {
        fprintf(stderr, "Reload without terrain runtime failed.\n");
        return 1;
    }
    if (!test_reload_restores_dirty_terrain_and_retires_history())
    {
        fprintf(stderr,
            "terrain scene Reload did not restore dirty regions and reset history\n");
        return 1;
    }
    if (!test_reload_clean_terrain_retires_history_without_changing_samples())
    {
        fprintf(stderr,
            "clean terrain Reload did not retain saved samples and retire edit history\n");
        return 1;
    }
    if (!test_reload_failure_restores_terrain_and_history())
    {
        fprintf(stderr,
            "failed terrain scene Reload did not preserve edited terrain and history\n");
        return 1;
    }
    puts("Sandbox3D terrain scene transition tests passed.");
    return 0;
}
