#ifndef SANDBOX3D_EDITOR_SCALE_TRANSACTION_H
#define SANDBOX3D_EDITOR_SCALE_TRANSACTION_H

#include <stdbool.h>

#include <henka/engine.h>
#include <henka/ui_font.h>

#include "editor_preferences.h"

typedef enum sandbox3d_editor_scale_failure_stage
{
    SANDBOX3D_EDITOR_SCALE_FAILURE_NONE = 0,
    SANDBOX3D_EDITOR_SCALE_FAILURE_ARGUMENT,
    SANDBOX3D_EDITOR_SCALE_FAILURE_RESOLVE,
    SANDBOX3D_EDITOR_SCALE_FAILURE_PREPARE,
    SANDBOX3D_EDITOR_SCALE_FAILURE_PERSIST
} sandbox3d_editor_scale_failure_stage;

typedef struct sandbox3d_editor_scale_transition_ops
{
    void* user_data;
    henka_result (*prepare_candidate)(
        void* user_data,
        const sandbox3d_editor_ui_preferences* candidate_preferences,
        void** out_prepared_candidate);
    henka_result (*persist_candidate)(
        void* user_data,
        int candidate_scale_percent,
        bool migration_pending);
    void (*publish_candidate)(
        void* user_data,
        const sandbox3d_editor_ui_preferences* candidate_preferences,
        void* prepared_candidate);
    void (*discard_candidate)(void* user_data, void* prepared_candidate);
} sandbox3d_editor_scale_transition_ops;

/*
 * Runs prepare -> durable settings commit -> no-fail publish. Prepared
 * resources are discarded on every failure; live preferences change only
 * after publication. publish_candidate must perform only a no-fail swap.
 */
henka_result sandbox3d_editor_ui_scale_transition_apply(
    sandbox3d_editor_ui_preferences* in_out_preferences,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    bool migration_pending,
    const sandbox3d_editor_scale_transition_ops* operations,
    sandbox3d_editor_scale_failure_stage* out_failure_stage);

/* Resolves stored startup preference and applies it through the same
 * prepare -> publish transaction; it persists only a pending legacy migration. */
henka_result sandbox3d_editor_ui_scale_startup_transition_apply(
    const henka_settings* settings,
    sandbox3d_editor_ui_preferences* in_out_preferences,
    const sandbox3d_editor_scale_transition_ops* operations,
    bool* out_migration_pending,
    sandbox3d_editor_scale_failure_stage* out_failure_stage);

/* Production adapter: prepare every supplied UI font/context binding through
 * the engine and renderer, persist the preference, then publish CPU metrics
 * and GPU atlas residency as one no-fail transaction. */
henka_result sandbox3d_editor_ui_scale_transition_apply_contexts(
    henka_engine* engine,
    henka_settings* settings,
    const char* settings_path,
    sandbox3d_editor_ui_preferences* in_out_preferences,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    bool migration_pending,
    const henka_ui_font_context_scale_binding* bindings,
    size_t binding_count,
    sandbox3d_editor_scale_failure_stage* out_failure_stage);

henka_result sandbox3d_editor_ui_scale_startup_transition_apply_contexts(
    henka_engine* engine,
    henka_settings* settings,
    const char* settings_path,
    sandbox3d_editor_ui_preferences* in_out_preferences,
    const henka_ui_font_context_scale_binding* bindings,
    size_t binding_count,
    bool* out_migration_pending,
    sandbox3d_editor_scale_failure_stage* out_failure_stage);

#endif
