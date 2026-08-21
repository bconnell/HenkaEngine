#include <henka/scene_behavior_runtime.h>

#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/script_asset.h>

struct henka_scene_behavior_runtime
{
    henka_script_behavior_runtime* behavior_runtime;
    henka_script_host* host;
    size_t asset_count;
    henka_script_behavior_asset* assets[HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS];
    henka_script_behavior_handle handles[HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS];
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
            runtime->handles[runtime->asset_count - 1U] = ignored_handle;
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

henka_result henka_scene_behavior_runtime_dispatch_signal_for_entity(
    henka_scene_behavior_runtime* runtime,
    uint64_t entity_id,
    henka_script_lifecycle_event event,
    float delta_seconds,
    uint64_t frame_index,
    uint32_t event_id,
    uint64_t source_entity,
    uint64_t other_entity,
    uint32_t event_type,
    henka_script_behavior_batch_report* out_report)
{
    if (runtime == NULL || runtime->behavior_runtime == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_script_behavior_runtime_dispatch_signal_for_entity(
        runtime->behavior_runtime,
        entity_id,
        event,
        delta_seconds,
        frame_index,
        event_id,
        source_entity,
        other_entity,
        event_type,
        out_report);
}

henka_result henka_scene_behavior_runtime_reload_behavior_with_diagnostic(
    henka_scene_behavior_runtime* runtime,
    const char* project_root,
    const henka_scene_document_behavior* behavior,
    uint64_t entity_id,
    henka_script_source_diagnostic* out_diagnostic)
{
    henka_script_behavior_asset* candidate_asset = NULL;
    henka_script_behavior_desc candidate_desc;
    size_t index;
    henka_result result;
    if (out_diagnostic != NULL)
    {
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
        out_diagnostic->result = HENKA_ERROR_INVALID_ARGUMENT;
        (void)snprintf(
            out_diagnostic->message,
            sizeof(out_diagnostic->message),
            "Behavior reload rejected");
    }
    if (runtime == NULL || project_root == NULL || project_root[0] == '\0' ||
        behavior == NULL || behavior->id == HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID ||
        entity_id == 0U || runtime->behavior_runtime == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_behavior_asset_create_with_diagnostic(
        project_root,
        behavior,
        entity_id,
        behavior->enabled,
        HENKA_SCRIPT_DEFAULT_BEHAVIOR_INSTRUCTION_BUDGET,
        &candidate_asset,
        out_diagnostic);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_script_behavior_asset_get_runtime_desc(
        candidate_asset, &candidate_desc);
    if (result != HENKA_SUCCESS)
    {
        if (out_diagnostic != NULL)
        {
            out_diagnostic->result = result;
        }
        henka_script_behavior_asset_destroy(candidate_asset);
        return result;
    }
    candidate_desc.behavior_id = behavior->id;
    candidate_desc.host = runtime->host;
    for (index = 0U; index < runtime->asset_count; ++index)
    {
        henka_script_behavior_snapshot snapshot;
        if (henka_script_behavior_runtime_get(
                runtime->behavior_runtime,
                runtime->handles[index],
                &snapshot) != HENKA_SUCCESS ||
            snapshot.entity_id != entity_id ||
            snapshot.behavior_id != behavior->id)
        {
            continue;
        }
        candidate_desc.instruction_budget = snapshot.instruction_budget;
        result = henka_script_behavior_runtime_rebind(
            runtime->behavior_runtime,
            runtime->handles[index],
            &candidate_desc);
        if (result == HENKA_SUCCESS)
        {
            henka_script_behavior_asset_destroy(runtime->assets[index]);
            runtime->assets[index] = candidate_asset;
            candidate_asset = NULL;
            if (out_diagnostic != NULL)
            {
                memset(out_diagnostic, 0, sizeof(*out_diagnostic));
                out_diagnostic->result = HENKA_SUCCESS;
            }
        }
        else if (out_diagnostic != NULL)
        {
            out_diagnostic->result = result;
            if (out_diagnostic->message[0] == '\0')
            {
                (void)snprintf(
                    out_diagnostic->message,
                    sizeof(out_diagnostic->message),
                    "Behavior reload rejected");
            }
        }
        henka_script_behavior_asset_destroy(candidate_asset);
        return result;
    }
    henka_script_behavior_asset_destroy(candidate_asset);
    if (out_diagnostic != NULL)
    {
        out_diagnostic->result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

henka_result henka_scene_behavior_runtime_reload_behavior(
    henka_scene_behavior_runtime* runtime,
    const char* project_root,
    const henka_scene_document_behavior* behavior,
    uint64_t entity_id)
{
    return henka_scene_behavior_runtime_reload_behavior_with_diagnostic(
        runtime,
        project_root,
        behavior,
        entity_id,
        NULL);
}

size_t henka_scene_behavior_runtime_get_behavior_count(
    const henka_scene_behavior_runtime* runtime)
{
    return runtime == NULL || runtime->behavior_runtime == NULL
        ? 0U
        : henka_script_behavior_runtime_get_count(runtime->behavior_runtime);
}
