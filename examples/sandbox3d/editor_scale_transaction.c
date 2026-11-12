#include "editor_scale_transaction.h"

typedef struct sandbox3d_editor_scale_context_transaction
{
    henka_engine* engine;
    henka_settings* settings;
    const char* settings_path;
    const henka_ui_font_context_scale_binding* bindings;
    size_t binding_count;
} sandbox3d_editor_scale_context_transaction;

static henka_result sandbox3d_editor_scale_context_prepare(
    void* user_data,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    void** out_prepared_candidate)
{
    sandbox3d_editor_scale_context_transaction* transaction =
        (sandbox3d_editor_scale_context_transaction*)user_data;
    henka_engine_ui_font_resource_update* update = NULL;
    henka_result result;

    if (out_prepared_candidate != NULL)
    {
        *out_prepared_candidate = NULL;
    }
    if (transaction == NULL || transaction->engine == NULL ||
        candidate_preferences == NULL ||
        out_prepared_candidate == NULL ||
        !sandbox3d_editor_ui_preferences_validate(candidate_preferences))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_engine_ui_font_resource_update_prepare(
        transaction->engine,
        transaction->bindings,
        transaction->binding_count,
        (uint16_t)candidate_preferences->scale_percent,
        &update);
    if (result == HENKA_SUCCESS)
    {
        *out_prepared_candidate = update;
    }
    return result;
}

static henka_result sandbox3d_editor_scale_context_persist(
    void* user_data,
    int candidate_scale_percent,
    bool migration_pending)
{
    const sandbox3d_editor_scale_context_transaction* transaction =
        (const sandbox3d_editor_scale_context_transaction*)user_data;
    sandbox3d_editor_ui_preferences candidate = {candidate_scale_percent};

    if (transaction == NULL || transaction->engine == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_editor_ui_preferences_persist_scale(
        transaction->settings,
        transaction->settings_path,
        &candidate,
        migration_pending);
}

static void sandbox3d_editor_scale_context_publish(
    void* user_data,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    void* prepared_candidate)
{
    (void)user_data;
    (void)candidate_preferences;
    henka_engine_ui_font_resource_update_publish(
        ((sandbox3d_editor_scale_context_transaction*)user_data)->engine,
        (henka_engine_ui_font_resource_update*)prepared_candidate);
}

static void sandbox3d_editor_scale_context_discard(
    void* user_data,
    void* prepared_candidate)
{
    henka_engine_ui_font_resource_update_discard(
        ((sandbox3d_editor_scale_context_transaction*)user_data)->engine,
        (henka_engine_ui_font_resource_update*)prepared_candidate);
}

static sandbox3d_editor_scale_transition_ops
sandbox3d_editor_scale_context_operations(
    sandbox3d_editor_scale_context_transaction* transaction)
{
    return (sandbox3d_editor_scale_transition_ops){
        transaction,
        sandbox3d_editor_scale_context_prepare,
        sandbox3d_editor_scale_context_persist,
        sandbox3d_editor_scale_context_publish,
        sandbox3d_editor_scale_context_discard};
}

static henka_result sandbox3d_editor_ui_scale_transition_apply_internal(
    sandbox3d_editor_ui_preferences* in_out_preferences,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    bool migration_pending,
    bool persist_candidate,
    const sandbox3d_editor_scale_transition_ops* operations,
    sandbox3d_editor_scale_failure_stage* out_failure_stage)
{
    sandbox3d_editor_ui_preferences candidate;
    void* prepared_candidate = NULL;
    henka_result result;

    if (out_failure_stage != NULL)
    {
        *out_failure_stage = SANDBOX3D_EDITOR_SCALE_FAILURE_ARGUMENT;
    }
    if (in_out_preferences == NULL || candidate_preferences == NULL ||
        operations == NULL || operations->prepare_candidate == NULL ||
        operations->persist_candidate == NULL ||
        operations->publish_candidate == NULL ||
        operations->discard_candidate == NULL ||
        !sandbox3d_editor_ui_preferences_validate(in_out_preferences) ||
        !sandbox3d_editor_ui_preferences_validate(candidate_preferences))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    candidate = *candidate_preferences;
    if (out_failure_stage != NULL)
    {
        *out_failure_stage = SANDBOX3D_EDITOR_SCALE_FAILURE_PREPARE;
    }
    result = operations->prepare_candidate(
        operations->user_data,
        &candidate,
        &prepared_candidate);
    if (result != HENKA_SUCCESS || prepared_candidate == NULL)
    {
        if (prepared_candidate != NULL)
        {
            operations->discard_candidate(
                operations->user_data, prepared_candidate);
        }
        return result != HENKA_SUCCESS ? result : HENKA_ERROR_UNKNOWN;
    }

    if (persist_candidate && out_failure_stage != NULL)
    {
        *out_failure_stage = SANDBOX3D_EDITOR_SCALE_FAILURE_PERSIST;
    }
    if (persist_candidate)
    {
        result = operations->persist_candidate(
            operations->user_data,
            candidate.scale_percent,
            migration_pending);
        if (result != HENKA_SUCCESS)
        {
            operations->discard_candidate(
                operations->user_data, prepared_candidate);
            return result;
        }
    }

    operations->publish_candidate(
        operations->user_data,
        &candidate,
        prepared_candidate);
    *in_out_preferences = candidate;
    if (out_failure_stage != NULL)
    {
        *out_failure_stage = SANDBOX3D_EDITOR_SCALE_FAILURE_NONE;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_ui_scale_transition_apply(
    sandbox3d_editor_ui_preferences* in_out_preferences,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    bool migration_pending,
    const sandbox3d_editor_scale_transition_ops* operations,
    sandbox3d_editor_scale_failure_stage* out_failure_stage)
{
    return sandbox3d_editor_ui_scale_transition_apply_internal(
        in_out_preferences,
        candidate_preferences,
        migration_pending,
        true,
        operations,
        out_failure_stage);
}

henka_result sandbox3d_editor_ui_scale_startup_transition_apply(
    const henka_settings* settings,
    sandbox3d_editor_ui_preferences* in_out_preferences,
    const sandbox3d_editor_scale_transition_ops* operations,
    bool* out_migration_pending,
    sandbox3d_editor_scale_failure_stage* out_failure_stage)
{
    sandbox3d_editor_ui_preferences candidate;
    bool migration_pending = false;
    henka_result result;

    if (out_failure_stage != NULL)
    {
        *out_failure_stage = SANDBOX3D_EDITOR_SCALE_FAILURE_ARGUMENT;
    }
    if (settings == NULL || in_out_preferences == NULL || operations == NULL ||
        out_migration_pending == NULL || operations->prepare_candidate == NULL ||
        operations->persist_candidate == NULL || operations->publish_candidate == NULL ||
        operations->discard_candidate == NULL ||
        !sandbox3d_editor_ui_preferences_validate(in_out_preferences))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (out_failure_stage != NULL)
    {
        *out_failure_stage = SANDBOX3D_EDITOR_SCALE_FAILURE_RESOLVE;
    }
    result = sandbox3d_editor_ui_preferences_resolve_startup(
        settings, &candidate, &migration_pending);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    *out_migration_pending = migration_pending;
    return sandbox3d_editor_ui_scale_transition_apply_internal(
        in_out_preferences,
        &candidate,
        migration_pending,
        migration_pending,
        operations,
        out_failure_stage);
}

henka_result sandbox3d_editor_ui_scale_transition_apply_contexts(
    henka_engine* engine,
    henka_settings* settings,
    const char* settings_path,
    sandbox3d_editor_ui_preferences* in_out_preferences,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    bool migration_pending,
    const henka_ui_font_context_scale_binding* bindings,
    size_t binding_count,
    sandbox3d_editor_scale_failure_stage* out_failure_stage)
{
    sandbox3d_editor_scale_context_transaction transaction = {
        engine,
        settings,
        settings_path,
        bindings,
        binding_count};
    sandbox3d_editor_scale_transition_ops operations =
        sandbox3d_editor_scale_context_operations(&transaction);

    return sandbox3d_editor_ui_scale_transition_apply(
        in_out_preferences,
        candidate_preferences,
        migration_pending,
        &operations,
        out_failure_stage);
}

henka_result sandbox3d_editor_ui_scale_startup_transition_apply_contexts(
    henka_engine* engine,
    henka_settings* settings,
    const char* settings_path,
    sandbox3d_editor_ui_preferences* in_out_preferences,
    const henka_ui_font_context_scale_binding* bindings,
    size_t binding_count,
    bool* out_migration_pending,
    sandbox3d_editor_scale_failure_stage* out_failure_stage)
{
    sandbox3d_editor_scale_context_transaction transaction = {
        engine,
        settings,
        settings_path,
        bindings,
        binding_count};
    sandbox3d_editor_scale_transition_ops operations =
        sandbox3d_editor_scale_context_operations(&transaction);

    return sandbox3d_editor_ui_scale_startup_transition_apply(
        settings,
        in_out_preferences,
        &operations,
        out_migration_pending,
        out_failure_stage);
}
