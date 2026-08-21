#include <henka/scene_behavior_runtime.h>

#include <henka/memory.h>
#include <henka/script_asset.h>

struct henka_scene_behavior_runtime
{
    henka_script_behavior_runtime* behavior_runtime;
    henka_script_host* host;
    size_t asset_count;
    henka_script_behavior_asset* assets[HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS];
};

static void henka_scene_behavior_runtime_release(
    henka_scene_behavior_runtime* runtime)
{
    size_t index;
    if (runtime == NULL)
    {
        return;
    }
    henka_script_behavior_runtime_destroy(runtime->behavior_runtime);
    runtime->behavior_runtime = NULL;
    for (index = 0U; index < runtime->asset_count; ++index)
    {
        henka_script_behavior_asset_destroy(runtime->assets[index]);
        runtime->assets[index] = NULL;
    }
    runtime->asset_count = 0U;
}

henka_result henka_scene_behavior_runtime_create_with_host(
    const henka_scene_document* document,
    const char* project_root,
    uint32_t instruction_budget,
    henka_script_host* host,
    henka_scene_behavior_runtime** out_runtime)
{
    henka_scene_behavior_runtime* runtime = NULL;
    size_t object_index;
    size_t object_count;
    henka_result result;
    if (out_runtime == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_runtime = NULL;
    if (document == NULL || project_root == NULL || project_root[0] == '\0' ||
        henka_scene_document_validate(document) != HENKA_SUCCESS ||
        instruction_budget > HENKA_SCRIPT_MAX_BEHAVIOR_INSTRUCTION_BUDGET)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (instruction_budget == 0U)
    {
        instruction_budget = HENKA_SCRIPT_DEFAULT_BEHAVIOR_INSTRUCTION_BUDGET;
    }
    runtime = (henka_scene_behavior_runtime*)henka_calloc(1U, sizeof(*runtime));
    if (runtime == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    runtime->host = host;
    result = henka_script_behavior_runtime_create(&runtime->behavior_runtime);
    if (result != HENKA_SUCCESS)
    {
        henka_free(runtime);
        return result;
    }
    object_count = henka_scene_document_get_object_count(document);
    for (object_index = 0U; object_index < object_count; ++object_index)
    {
        henka_scene_document_object object;
        size_t behavior_index;
        if (henka_scene_document_get_object_at(
                document, object_index, &object) != HENKA_SUCCESS)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto fail;
        }
        for (behavior_index = 0U;
             behavior_index < object.behavior_count;
             ++behavior_index)
        {
            henka_script_behavior_asset* asset = NULL;
            henka_script_behavior_desc desc;
            henka_script_behavior_handle ignored_handle;
            if (runtime->asset_count >= HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS)
            {
                result = HENKA_ERROR_LIMIT;
                goto fail;
            }
            result = henka_script_behavior_asset_create(
                project_root,
                &object.behaviors[behavior_index],
                object.id,
                object.behaviors[behavior_index].enabled,
                instruction_budget,
                &asset);
            if (result != HENKA_SUCCESS ||
                henka_script_behavior_asset_get_runtime_desc(asset, &desc) != HENKA_SUCCESS)
            {
                henka_script_behavior_asset_destroy(asset);
                if (result == HENKA_SUCCESS)
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                }
                goto fail;
            }
            desc.host = host;
            desc.behavior_id = object.behaviors[behavior_index].id;
            result = henka_script_behavior_runtime_add(
                runtime->behavior_runtime, &desc, &ignored_handle);
            if (result != HENKA_SUCCESS)
            {
                henka_script_behavior_asset_destroy(asset);
                goto fail;
            }
            runtime->assets[runtime->asset_count++] = asset;
        }
    }
    *out_runtime = runtime;
    return HENKA_SUCCESS;

fail:
    henka_scene_behavior_runtime_release(runtime);
    henka_free(runtime);
    return result;
}

henka_result henka_scene_behavior_runtime_create(
    const henka_scene_document* document,
    const char* project_root,
    uint32_t instruction_budget,
    henka_scene_behavior_runtime** out_runtime)
{
    return henka_scene_behavior_runtime_create_with_host(
        document,
        project_root,
        instruction_budget,
        NULL,
        out_runtime);
}

void henka_scene_behavior_runtime_destroy(
    henka_scene_behavior_runtime* runtime)
{
    if (runtime != NULL)
    {
        henka_scene_behavior_runtime_release(runtime);
        henka_free(runtime);
    }
}

henka_result henka_scene_behavior_runtime_dispatch(
    henka_scene_behavior_runtime* runtime,
    henka_script_lifecycle_event event,
    float delta_seconds,
    uint64_t frame_index,
    henka_script_behavior_batch_report* out_report)
{
    if (runtime == NULL || runtime->behavior_runtime == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_script_behavior_runtime_dispatch_all(
        runtime->behavior_runtime,
        event,
        delta_seconds,
        frame_index,
        out_report);
}

henka_result henka_scene_behavior_runtime_dispatch_events(
    henka_scene_behavior_runtime* runtime,
    henka_script_behavior_batch_report* out_report)
{
    size_t pending_count;
    size_t index;
    henka_result first_error = HENKA_SUCCESS;
    if (out_report != NULL)
    {
        *out_report = (henka_script_behavior_batch_report){
            HENKA_SCRIPT_LIFECYCLE_EVENT, 0U, 0U, 0U, 0U, HENKA_SUCCESS};
    }
    if (runtime == NULL || runtime->behavior_runtime == NULL || runtime->host == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    pending_count = henka_script_host_get_pending_event_count(runtime->host);
    if (pending_count > HENKA_SCRIPT_HOST_MAX_EVENTS)
    {
        return HENKA_ERROR_LIMIT;
    }
    for (index = 0U; index < pending_count; ++index)
    {
        henka_script_event event;
        henka_script_behavior_batch_report event_report;
        henka_result result;
        result = henka_script_host_poll_event(runtime->host, &event);
        if (result != HENKA_SUCCESS)
        {
            if (first_error == HENKA_SUCCESS)
            {
                first_error = result;
            }
            break;
        }
        result = henka_script_behavior_runtime_dispatch_event_all(
            runtime->behavior_runtime,
            event.event_id,
            event.source_entity,
            event.frame_index,
            &event_report);
        if (result != HENKA_SUCCESS && first_error == HENKA_SUCCESS)
        {
            first_error = result;
        }
        if (out_report != NULL)
        {
            out_report->attempted += event_report.attempted;
            out_report->executed += event_report.executed;
            out_report->skipped += event_report.skipped;
            out_report->failed += event_report.failed;
        }
    }
    if (out_report != NULL)
    {
        out_report->first_error = first_error;
    }
    return first_error;
}

size_t henka_scene_behavior_runtime_get_behavior_count(
    const henka_scene_behavior_runtime* runtime)
{
    return runtime == NULL || runtime->behavior_runtime == NULL
        ? 0U
        : henka_script_behavior_runtime_get_count(runtime->behavior_runtime);
}
