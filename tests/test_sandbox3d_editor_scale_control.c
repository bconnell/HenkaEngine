#include "test_suite.h"

#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
#include <process.h>
#define HENKA_TEST_PROCESS_ID _getpid
#else
#include <unistd.h>
#define HENKA_TEST_PROCESS_ID getpid
#endif

#include <henka/ui.h>
#include "../engine/src/ui/ui_internal.h"

#include "../examples/sandbox3d/editor_scale_control.h"
#include "../examples/sandbox3d/editor_scale_transaction.h"

#define HENKA_TEST_SCALE_CONTEXTS 3U

typedef struct henka_test_editor_scale_transition_state
{
    int live_profiles[HENKA_TEST_SCALE_CONTEXTS];
    int candidate_profiles[HENKA_TEST_SCALE_CONTEXTS];
    int fail_prepare_context;
    int live_scale;
    int persisted_scale;
    bool fail_persist;
    bool persisted;
    bool migration_pending;
    bool publish_before_persist;
    size_t prepare_count;
    size_t publish_count;
    size_t discard_count;
} henka_test_editor_scale_transition_state;

typedef struct henka_test_editor_scale_context_transaction
{
    henka_settings* settings;
    const char* settings_path;
    const henka_ui_font_context_scale_binding* bindings;
    size_t binding_count;
} henka_test_editor_scale_context_transaction;

static henka_result henka_test_editor_scale_context_prepare(
    void* user_data,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    void** out_prepared_candidate)
{
    const henka_test_editor_scale_context_transaction* transaction =
        (const henka_test_editor_scale_context_transaction*)user_data;
    henka_ui_font_context_scale_update* update = NULL;
    henka_result result;

    if (out_prepared_candidate != NULL)
    {
        *out_prepared_candidate = NULL;
    }
    if (transaction == NULL || candidate_preferences == NULL ||
        out_prepared_candidate == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_ui_font_context_scale_update_prepare(
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

static henka_result henka_test_editor_scale_context_persist(
    void* user_data,
    int candidate_scale_percent,
    bool migration_pending)
{
    const henka_test_editor_scale_context_transaction* transaction =
        (const henka_test_editor_scale_context_transaction*)user_data;
    const sandbox3d_editor_ui_preferences candidate = {candidate_scale_percent};

    if (transaction == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_editor_ui_preferences_persist_scale(
        transaction->settings,
        transaction->settings_path,
        &candidate,
        migration_pending);
}

static void henka_test_editor_scale_context_publish(
    void* user_data,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    void* prepared_candidate)
{
    (void)user_data;
    (void)candidate_preferences;
    henka_ui_font_context_scale_update_publish(
        (henka_ui_font_context_scale_update*)prepared_candidate);
}

static void henka_test_editor_scale_context_discard(
    void* user_data,
    void* prepared_candidate)
{
    (void)user_data;
    henka_ui_font_context_scale_update_dispose(
        (henka_ui_font_context_scale_update*)prepared_candidate);
}

static henka_result henka_test_editor_scale_prepare(
    void* user_data,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    void** out_prepared_candidate)
{
    henka_test_editor_scale_transition_state* state =
        (henka_test_editor_scale_transition_state*)user_data;
    size_t index;

    *out_prepared_candidate = state->candidate_profiles;
    for (index = 0U; index < HENKA_TEST_SCALE_CONTEXTS; ++index)
    {
        ++state->prepare_count;
        if ((int)index == state->fail_prepare_context)
        {
            return HENKA_ERROR_RENDERER;
        }
        state->candidate_profiles[index] = candidate_preferences->scale_percent;
    }
    return HENKA_SUCCESS;
}

static henka_result henka_test_editor_scale_persist(
    void* user_data,
    int candidate_scale_percent,
    bool migration_pending)
{
    henka_test_editor_scale_transition_state* state =
        (henka_test_editor_scale_transition_state*)user_data;

    if (state->live_scale != 100)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    state->persisted_scale = candidate_scale_percent;
    state->migration_pending = migration_pending;
    if (state->fail_persist)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    state->persisted = true;
    return HENKA_SUCCESS;
}

static void henka_test_editor_scale_publish(
    void* user_data,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    void* prepared_candidate)
{
    henka_test_editor_scale_transition_state* state =
        (henka_test_editor_scale_transition_state*)user_data;
    const int* candidate_profiles = (const int*)prepared_candidate;
    size_t index;

    if (!state->persisted)
    {
        state->publish_before_persist = true;
    }
    for (index = 0U; index < HENKA_TEST_SCALE_CONTEXTS; ++index)
    {
        state->live_profiles[index] = candidate_profiles[index];
    }
    state->live_scale = candidate_preferences->scale_percent;
    ++state->publish_count;
}

static void henka_test_editor_scale_discard(
    void* user_data,
    void* prepared_candidate)
{
    henka_test_editor_scale_transition_state* state =
        (henka_test_editor_scale_transition_state*)user_data;
    size_t index;
    (void)prepared_candidate;
    ++state->discard_count;
    for (index = 0U; index < HENKA_TEST_SCALE_CONTEXTS; ++index)
    {
        state->candidate_profiles[index] = 0;
    }
}

static sandbox3d_editor_scale_transition_ops henka_test_editor_scale_operations(
    henka_test_editor_scale_transition_state* state)
{
    return (sandbox3d_editor_scale_transition_ops){
        state,
        henka_test_editor_scale_prepare,
        henka_test_editor_scale_persist,
        henka_test_editor_scale_publish,
        henka_test_editor_scale_discard};
}

typedef struct henka_test_editor_scale_startup_state
{
    henka_test_editor_scale_transition_state transition;
    henka_settings* settings;
    bool fail_persist;
    bool persist_before_prepare;
    size_t persist_count;
} henka_test_editor_scale_startup_state;

static henka_result henka_test_editor_scale_startup_prepare(
    void* user_data,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    void** out_prepared_candidate)
{
    henka_test_editor_scale_startup_state* state =
        (henka_test_editor_scale_startup_state*)user_data;
    return henka_test_editor_scale_prepare(
        &state->transition, candidate_preferences, out_prepared_candidate);
}

static henka_result henka_test_editor_scale_startup_persist(
    void* user_data,
    int candidate_scale_percent,
    bool migration_pending)
{
    henka_test_editor_scale_startup_state* state =
        (henka_test_editor_scale_startup_state*)user_data;
    henka_result result;

    ++state->persist_count;
    if (state->transition.prepare_count != HENKA_TEST_SCALE_CONTEXTS ||
        state->transition.publish_count != 0U)
    {
        state->persist_before_prepare = true;
    }
    if (state->fail_persist)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    result = henka_settings_set_int(
        state->settings, "ui.editor.scale_percent", candidate_scale_percent);
    if (result == HENKA_SUCCESS && migration_pending)
    {
        result = henka_settings_set_int(
            state->settings, "ui.editor.scale_migration_version", 1);
    }
    if (result == HENKA_SUCCESS)
    {
        state->transition.persisted = true;
        state->transition.persisted_scale = candidate_scale_percent;
        state->transition.migration_pending = migration_pending;
    }
    return result;
}

static void henka_test_editor_scale_startup_publish(
    void* user_data,
    const sandbox3d_editor_ui_preferences* candidate_preferences,
    void* prepared_candidate)
{
    henka_test_editor_scale_startup_state* state =
        (henka_test_editor_scale_startup_state*)user_data;
    henka_test_editor_scale_publish(
        &state->transition, candidate_preferences, prepared_candidate);
}

static void henka_test_editor_scale_startup_discard(
    void* user_data,
    void* prepared_candidate)
{
    henka_test_editor_scale_startup_state* state =
        (henka_test_editor_scale_startup_state*)user_data;
    henka_test_editor_scale_discard(&state->transition, prepared_candidate);
}

static sandbox3d_editor_scale_transition_ops
henka_test_editor_scale_startup_operations(
    henka_test_editor_scale_startup_state* state)
{
    return (sandbox3d_editor_scale_transition_ops){
        state,
        henka_test_editor_scale_startup_prepare,
        henka_test_editor_scale_startup_persist,
        henka_test_editor_scale_startup_publish,
        henka_test_editor_scale_startup_discard};
}

static void henka_test_editor_scale_startup_rejects_unknown_migration_version_before_prepare(void)
{
    henka_settings* settings = NULL;
    henka_test_editor_scale_startup_state state = {
        .transition = {
            .live_profiles = {100, 100, 100},
            .fail_prepare_context = -1,
            .live_scale = 100}};
    sandbox3d_editor_scale_transition_ops operations =
        henka_test_editor_scale_startup_operations(&state);
    sandbox3d_editor_ui_preferences current = {100};
    sandbox3d_editor_scale_failure_stage failure_stage =
        SANDBOX3D_EDITOR_SCALE_FAILURE_NONE;
    bool migration_pending = false;

    HENKA_TEST_ASSERT(henka_settings_create(&settings) == HENKA_SUCCESS);
    state.settings = settings;
    HENKA_TEST_ASSERT(henka_settings_set_int(
        settings, "ui.editor.scale_percent", 200) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_int(
        settings, "ui.editor.scale_migration_version", 7) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(sandbox3d_editor_ui_scale_startup_transition_apply(
        settings,
        &current,
        &operations,
        &migration_pending,
        &failure_stage) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(failure_stage == SANDBOX3D_EDITOR_SCALE_FAILURE_RESOLVE);
    HENKA_TEST_ASSERT(!migration_pending);
    HENKA_TEST_ASSERT(current.scale_percent == 100);
    HENKA_TEST_ASSERT(state.transition.live_scale == 100);
    HENKA_TEST_ASSERT(state.transition.prepare_count == 0U);
    HENKA_TEST_ASSERT(state.persist_count == 0U);
    HENKA_TEST_ASSERT(state.transition.publish_count == 0U);
    HENKA_TEST_ASSERT(state.transition.discard_count == 0U);
    HENKA_TEST_ASSERT(henka_settings_get_int(
        settings, "ui.editor.scale_percent", -1) == 200);
    HENKA_TEST_ASSERT(henka_settings_get_int(
        settings, "ui.editor.scale_migration_version", -1) == 7);
    henka_settings_destroy(settings);
}

static void henka_test_editor_scale_candidate_prepare_failure_is_atomic(void)
{
    henka_test_editor_scale_transition_state state = {
        .live_profiles = {100, 100, 100},
        .fail_prepare_context = 2,
        .live_scale = 100};
    sandbox3d_editor_scale_transition_ops operations =
        henka_test_editor_scale_operations(&state);
    sandbox3d_editor_ui_preferences current = {100};
    const sandbox3d_editor_ui_preferences candidate = {150};
    sandbox3d_editor_scale_failure_stage failure_stage =
        SANDBOX3D_EDITOR_SCALE_FAILURE_NONE;

    HENKA_TEST_ASSERT(sandbox3d_editor_ui_scale_transition_apply(
        &current, &candidate, true, &operations, &failure_stage) == HENKA_ERROR_RENDERER);
    HENKA_TEST_ASSERT(failure_stage == SANDBOX3D_EDITOR_SCALE_FAILURE_PREPARE);
    HENKA_TEST_ASSERT(current.scale_percent == 100);
    HENKA_TEST_ASSERT(state.live_scale == 100);
    HENKA_TEST_ASSERT(state.live_profiles[0] == 100);
    HENKA_TEST_ASSERT(state.live_profiles[1] == 100);
    HENKA_TEST_ASSERT(state.live_profiles[2] == 100);
    HENKA_TEST_ASSERT(state.prepare_count == 3U);
    HENKA_TEST_ASSERT(state.persisted_scale == 0);
    HENKA_TEST_ASSERT(state.publish_count == 0U);
    HENKA_TEST_ASSERT(state.discard_count == 1U);
    HENKA_TEST_ASSERT(state.candidate_profiles[0] == 0);
    HENKA_TEST_ASSERT(state.candidate_profiles[1] == 0);
    HENKA_TEST_ASSERT(state.candidate_profiles[2] == 0);
}

static void henka_test_editor_scale_persist_failure_is_atomic(void)
{
    henka_test_editor_scale_transition_state state = {
        .live_profiles = {100, 100, 100},
        .fail_prepare_context = -1,
        .live_scale = 100,
        .fail_persist = true};
    sandbox3d_editor_scale_transition_ops operations =
        henka_test_editor_scale_operations(&state);
    sandbox3d_editor_ui_preferences current = {100};
    const sandbox3d_editor_ui_preferences candidate = {125};
    sandbox3d_editor_scale_failure_stage failure_stage =
        SANDBOX3D_EDITOR_SCALE_FAILURE_NONE;

    HENKA_TEST_ASSERT(sandbox3d_editor_ui_scale_transition_apply(
        &current, &candidate, false, &operations, &failure_stage) == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(failure_stage == SANDBOX3D_EDITOR_SCALE_FAILURE_PERSIST);
    HENKA_TEST_ASSERT(current.scale_percent == 100);
    HENKA_TEST_ASSERT(state.live_scale == 100);
    HENKA_TEST_ASSERT(state.live_profiles[0] == 100);
    HENKA_TEST_ASSERT(state.live_profiles[1] == 100);
    HENKA_TEST_ASSERT(state.live_profiles[2] == 100);
    HENKA_TEST_ASSERT(state.persisted_scale == 125);
    HENKA_TEST_ASSERT(state.publish_count == 0U);
    HENKA_TEST_ASSERT(state.discard_count == 1U);
}

static void henka_test_editor_scale_success_persists_before_publishing_all_profiles(void)
{
    henka_test_editor_scale_transition_state state = {
        .live_profiles = {100, 100, 100},
        .fail_prepare_context = -1,
        .live_scale = 100};
    sandbox3d_editor_scale_transition_ops operations =
        henka_test_editor_scale_operations(&state);
    sandbox3d_editor_ui_preferences current = {100};
    const sandbox3d_editor_ui_preferences candidate = {200};
    sandbox3d_editor_scale_failure_stage failure_stage =
        SANDBOX3D_EDITOR_SCALE_FAILURE_ARGUMENT;

    HENKA_TEST_ASSERT(sandbox3d_editor_ui_scale_transition_apply(
        &current, &candidate, true, &operations, &failure_stage) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(failure_stage == SANDBOX3D_EDITOR_SCALE_FAILURE_NONE);
    HENKA_TEST_ASSERT(current.scale_percent == 200);
    HENKA_TEST_ASSERT(state.live_scale == 200);
    HENKA_TEST_ASSERT(state.live_profiles[0] == 200);
    HENKA_TEST_ASSERT(state.live_profiles[1] == 200);
    HENKA_TEST_ASSERT(state.live_profiles[2] == 200);
    HENKA_TEST_ASSERT(state.persisted_scale == 200);
    HENKA_TEST_ASSERT(state.migration_pending);
    HENKA_TEST_ASSERT(!state.publish_before_persist);
    HENKA_TEST_ASSERT(state.publish_count == 1U);
    HENKA_TEST_ASSERT(state.discard_count == 0U);
}

static void henka_test_editor_scale_startup_uses_canonical_value_and_migrates_after_prepare(void)
{
    henka_settings* settings = NULL;
    henka_test_editor_scale_startup_state state = {
        .transition = {
            .live_profiles = {100, 100, 100},
            .fail_prepare_context = -1,
            .live_scale = 100}};
    sandbox3d_editor_scale_transition_ops operations =
        henka_test_editor_scale_startup_operations(&state);
    sandbox3d_editor_ui_preferences current = {100};
    sandbox3d_editor_scale_failure_stage failure_stage =
        SANDBOX3D_EDITOR_SCALE_FAILURE_NONE;
    bool migration_pending = false;

    HENKA_TEST_ASSERT(henka_settings_create(&settings) == HENKA_SUCCESS);
    state.settings = settings;
    HENKA_TEST_ASSERT(henka_settings_set_float(
        settings, "ui.workspace.custom_layout.ui_scale", 2.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_int(
        settings, "ui.editor.scale_percent", 125) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!henka_settings_has_key(
        settings, "ui.editor.scale_migration_version"));

    HENKA_TEST_ASSERT(sandbox3d_editor_ui_scale_startup_transition_apply(
        settings,
        &current,
        &operations,
        &migration_pending,
        &failure_stage) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(failure_stage == SANDBOX3D_EDITOR_SCALE_FAILURE_NONE);
    HENKA_TEST_ASSERT(migration_pending);
    HENKA_TEST_ASSERT(!state.persist_before_prepare);
    HENKA_TEST_ASSERT(state.persist_count == 1U);
    HENKA_TEST_ASSERT(current.scale_percent == 125);
    HENKA_TEST_ASSERT(state.transition.live_scale == 125);
    HENKA_TEST_ASSERT(state.transition.persisted_scale == 125);
    HENKA_TEST_ASSERT(state.transition.publish_count == 1U);
    HENKA_TEST_ASSERT(state.transition.live_profiles[0] == 125);
    HENKA_TEST_ASSERT(state.transition.live_profiles[1] == 125);
    HENKA_TEST_ASSERT(state.transition.live_profiles[2] == 125);
    HENKA_TEST_ASSERT(henka_settings_get_int(
        settings, "ui.editor.scale_percent", -1) == 125);
    HENKA_TEST_ASSERT(henka_settings_get_int(
        settings, "ui.editor.scale_migration_version", -1) == 1);
    HENKA_TEST_ASSERT(henka_settings_get_float(
        settings, "ui.workspace.custom_layout.ui_scale", -1.0f) == 2.0f);
    henka_settings_destroy(settings);
}

static void henka_test_editor_scale_completed_startup_uses_canonical_value_without_persisting(void)
{
    henka_settings* settings = NULL;
    henka_test_editor_scale_startup_state state = {
        .transition = {
            .live_profiles = {100, 100, 100},
            .fail_prepare_context = -1,
            .live_scale = 100},
        .fail_persist = true};
    sandbox3d_editor_scale_transition_ops operations =
        henka_test_editor_scale_startup_operations(&state);
    sandbox3d_editor_ui_preferences current = {100};
    sandbox3d_editor_scale_failure_stage failure_stage =
        SANDBOX3D_EDITOR_SCALE_FAILURE_NONE;
    bool migration_pending = true;

    HENKA_TEST_ASSERT(henka_settings_create(&settings) == HENKA_SUCCESS);
    state.settings = settings;
    HENKA_TEST_ASSERT(henka_settings_set_int(
        settings, "ui.editor.scale_percent", 150) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_int(
        settings, "ui.editor.scale_migration_version", 1) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_float(
        settings, "ui.workspace.custom_layout.ui_scale", 2.0f) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(sandbox3d_editor_ui_scale_startup_transition_apply(
        settings,
        &current,
        &operations,
        &migration_pending,
        &failure_stage) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(failure_stage == SANDBOX3D_EDITOR_SCALE_FAILURE_NONE);
    HENKA_TEST_ASSERT(!migration_pending);
    HENKA_TEST_ASSERT(current.scale_percent == 150);
    HENKA_TEST_ASSERT(state.transition.live_scale == 150);
    HENKA_TEST_ASSERT(state.transition.live_profiles[0] == 150);
    HENKA_TEST_ASSERT(state.transition.live_profiles[1] == 150);
    HENKA_TEST_ASSERT(state.transition.live_profiles[2] == 150);
    HENKA_TEST_ASSERT(state.persist_count == 0U);
    HENKA_TEST_ASSERT(!state.transition.persisted);
    HENKA_TEST_ASSERT(state.transition.publish_count == 1U);
    HENKA_TEST_ASSERT(state.transition.discard_count == 0U);
    HENKA_TEST_ASSERT(henka_settings_get_int(
        settings, "ui.editor.scale_percent", -1) == 150);
    HENKA_TEST_ASSERT(henka_settings_get_int(
        settings, "ui.editor.scale_migration_version", -1) == 1);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        henka_settings_get_float(
            settings, "ui.workspace.custom_layout.ui_scale", 0.0f),
        2.0f,
        0.0001f);
    henka_settings_destroy(settings);
}

static void henka_test_editor_scale_startup_prepare_failure_retains_legacy_state_and_retries(void)
{
    henka_settings* settings = NULL;
    henka_test_editor_scale_startup_state state = {
        .transition = {
            .live_profiles = {100, 100, 100},
            .fail_prepare_context = 2,
            .live_scale = 100}};
    sandbox3d_editor_scale_transition_ops operations =
        henka_test_editor_scale_startup_operations(&state);
    sandbox3d_editor_ui_preferences current = {100};
    sandbox3d_editor_scale_failure_stage failure_stage =
        SANDBOX3D_EDITOR_SCALE_FAILURE_NONE;
    bool migration_pending = false;

    HENKA_TEST_ASSERT(henka_settings_create(&settings) == HENKA_SUCCESS);
    state.settings = settings;
    HENKA_TEST_ASSERT(henka_settings_set_float(
        settings, "ui.workspace.custom_layout.ui_scale", 1.5f) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(sandbox3d_editor_ui_scale_startup_transition_apply(
        settings,
        &current,
        &operations,
        &migration_pending,
        &failure_stage) == HENKA_ERROR_RENDERER);
    HENKA_TEST_ASSERT(failure_stage == SANDBOX3D_EDITOR_SCALE_FAILURE_PREPARE);
    HENKA_TEST_ASSERT(migration_pending);
    HENKA_TEST_ASSERT(current.scale_percent == 100);
    HENKA_TEST_ASSERT(state.transition.live_scale == 100);
    HENKA_TEST_ASSERT(state.transition.live_profiles[0] == 100);
    HENKA_TEST_ASSERT(state.transition.live_profiles[1] == 100);
    HENKA_TEST_ASSERT(state.transition.live_profiles[2] == 100);
    HENKA_TEST_ASSERT(state.persist_count == 0U);
    HENKA_TEST_ASSERT(state.transition.publish_count == 0U);
    HENKA_TEST_ASSERT(!henka_settings_has_key(
        settings, "ui.editor.scale_percent"));
    HENKA_TEST_ASSERT(!henka_settings_has_key(
        settings, "ui.editor.scale_migration_version"));
    HENKA_TEST_ASSERT(henka_settings_get_float(
        settings, "ui.workspace.custom_layout.ui_scale", -1.0f) == 1.5f);

    state.transition.fail_prepare_context = -1;
    HENKA_TEST_ASSERT(sandbox3d_editor_ui_scale_startup_transition_apply(
        settings,
        &current,
        &operations,
        &migration_pending,
        &failure_stage) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(failure_stage == SANDBOX3D_EDITOR_SCALE_FAILURE_NONE);
    HENKA_TEST_ASSERT(current.scale_percent == 150);
    HENKA_TEST_ASSERT(state.transition.live_scale == 150);
    HENKA_TEST_ASSERT(henka_settings_get_int(
        settings, "ui.editor.scale_percent", -1) == 150);
    HENKA_TEST_ASSERT(henka_settings_get_int(
        settings, "ui.editor.scale_migration_version", -1) == 1);
    henka_settings_destroy(settings);
}

static void henka_test_editor_scale_context_startup_is_transactional(void)
{
    henka_settings* settings = NULL;
    henka_settings* reloaded = NULL;
    henka_ui_context* first_context = NULL;
    henka_ui_context* second_context = NULL;
    henka_ui_font_context_scale_binding bindings[2U];
    henka_ui_frame_desc frame_desc = {0};
    sandbox3d_editor_ui_preferences preferences = {100};
    henka_test_editor_scale_context_transaction transaction;
    sandbox3d_editor_scale_transition_ops operations;
    sandbox3d_editor_scale_failure_stage failure_stage = SANDBOX3D_EDITOR_SCALE_FAILURE_NONE;
    bool migration_pending = false;
    char settings_path[192U];
    FILE* existing;
    uint16_t first_initial_scale;
    uint16_t second_initial_scale;

    (void)snprintf(
        settings_path,
        sizeof(settings_path),
        "build/test_tmp/henka-editor-scale-startup-%ld.ini",
        (long)HENKA_TEST_PROCESS_ID());
#if defined(_MSC_VER)
    (void)fopen_s(&existing, settings_path, "rb");
#else
    existing = fopen(settings_path, "rb");
#endif
    HENKA_TEST_ASSERT(existing == NULL);

    HENKA_TEST_ASSERT(henka_settings_create(&settings) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_float(
        settings, "ui.workspace.custom_layout.ui_scale", 1.5f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_save_file(settings, settings_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_create(&first_context) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_create(&second_context) == HENKA_SUCCESS);
    first_initial_scale = first_context->font_app_scale_percent;
    second_initial_scale = second_context->font_app_scale_percent;
    bindings[0] = (henka_ui_font_context_scale_binding){first_context, NULL};
    bindings[1] = (henka_ui_font_context_scale_binding){second_context, NULL};
    transaction = (henka_test_editor_scale_context_transaction){
        settings, settings_path, bindings, 2U};
    operations = (sandbox3d_editor_scale_transition_ops){
        &transaction,
        henka_test_editor_scale_context_prepare,
        henka_test_editor_scale_context_persist,
        henka_test_editor_scale_context_publish,
        henka_test_editor_scale_context_discard};

    frame_desc.framebuffer_width = 160;
    frame_desc.framebuffer_height = 120;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(second_context, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_editor_ui_scale_startup_transition_apply(
        settings,
        &preferences,
        &operations,
        &migration_pending,
        &failure_stage) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(failure_stage == SANDBOX3D_EDITOR_SCALE_FAILURE_PREPARE);
    HENKA_TEST_ASSERT(migration_pending);
    HENKA_TEST_ASSERT(preferences.scale_percent == 100);
    HENKA_TEST_ASSERT(first_context->font_app_scale_percent == first_initial_scale);
    HENKA_TEST_ASSERT(second_context->font_app_scale_percent == second_initial_scale);
    HENKA_TEST_ASSERT(!henka_settings_has_key(settings, "ui.editor.scale_percent"));
    HENKA_TEST_ASSERT(!henka_settings_has_key(settings, "ui.editor.scale_migration_version"));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        henka_settings_get_float(settings, "ui.workspace.custom_layout.ui_scale", 0.0f),
        1.5f,
        0.0001f);
    HENKA_TEST_ASSERT(henka_ui_end_frame(second_context) == HENKA_SUCCESS);

    /* Use the existing fixture directory as a file target to force replace failure. */
    transaction.settings_path = "build/test_tmp";
    migration_pending = false;
    HENKA_TEST_ASSERT(sandbox3d_editor_ui_scale_startup_transition_apply(
        settings,
        &preferences,
        &operations,
        &migration_pending,
        &failure_stage) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(failure_stage == SANDBOX3D_EDITOR_SCALE_FAILURE_PERSIST);
    HENKA_TEST_ASSERT(migration_pending);
    HENKA_TEST_ASSERT(preferences.scale_percent == 100);
    HENKA_TEST_ASSERT(first_context->font_app_scale_percent == first_initial_scale);
    HENKA_TEST_ASSERT(second_context->font_app_scale_percent == second_initial_scale);
    HENKA_TEST_ASSERT(!henka_settings_has_key(settings, "ui.editor.scale_percent"));
    HENKA_TEST_ASSERT(!henka_settings_has_key(settings, "ui.editor.scale_migration_version"));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        henka_settings_get_float(settings, "ui.workspace.custom_layout.ui_scale", 0.0f),
        1.5f,
        0.0001f);
    HENKA_TEST_ASSERT(henka_settings_create(&reloaded) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_load_file(reloaded, settings_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!henka_settings_has_key(reloaded, "ui.editor.scale_percent"));
    HENKA_TEST_ASSERT(!henka_settings_has_key(reloaded, "ui.editor.scale_migration_version"));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        henka_settings_get_float(reloaded, "ui.workspace.custom_layout.ui_scale", 0.0f),
        1.5f,
        0.0001f);
    henka_settings_destroy(reloaded);
    reloaded = NULL;

    transaction.settings_path = settings_path;

    HENKA_TEST_ASSERT(sandbox3d_editor_ui_scale_startup_transition_apply(
        settings,
        &preferences,
        &operations,
        &migration_pending,
        &failure_stage) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(failure_stage == SANDBOX3D_EDITOR_SCALE_FAILURE_NONE);
    HENKA_TEST_ASSERT(migration_pending);
    HENKA_TEST_ASSERT(preferences.scale_percent == 150);
    HENKA_TEST_ASSERT(first_context->font_app_scale_percent == 150U);
    HENKA_TEST_ASSERT(second_context->font_app_scale_percent == 150U);
    HENKA_TEST_ASSERT(henka_settings_create(&reloaded) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_load_file(reloaded, settings_path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_get_int(reloaded, "ui.editor.scale_percent", -1) == 150);
    HENKA_TEST_ASSERT(henka_settings_get_int(reloaded, "ui.editor.scale_migration_version", -1) == 1);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        henka_settings_get_float(reloaded, "ui.workspace.custom_layout.ui_scale", 0.0f),
        1.5f,
        0.0001f);

    henka_settings_destroy(reloaded);
    henka_settings_destroy(settings);
    henka_ui_destroy(second_context);
    henka_ui_destroy(first_context);
    HENKA_TEST_ASSERT(remove(settings_path) == 0);
}

void henka_test_sandbox3d_editor_scale_control(void)
{
    henka_ui_context* ui = NULL;
    henka_ui_frame_desc frame_desc = {0};
    const henka_ui_rect bounds = {16.0f, 16.0f, 240.0f, 56.0f};
    sandbox3d_editor_scale_control_layout_desc geometry;
    const uint16_t current_percent = 100U;
    uint16_t selected_percent = 0U;
    bool changed;
    henka_result result;

    HENKA_TEST_ASSERT(
        sandbox3d_editor_scale_control_layout(
            (henka_ui_rect){32.0f, 16.0f, 320.0f, 44.0f},
            &geometry) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!geometry.stacked);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.label.x, 32.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.label.y, 16.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.label.width, 72.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.choices.x, 104.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.choices.y, 16.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.choices.width, 248.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.choices.height, 24.0f, 0.0001f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_scale_control_layout(
            (henka_ui_rect){12.0f, 20.0f, 204.0f, 44.0f},
            &geometry) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!geometry.stacked);
    HENKA_TEST_ASSERT(geometry.compact);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.label.x, 12.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.label.y, 20.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        geometry.label.width,
        SANDBOX3D_EDITOR_SCALE_COMPACT_LABEL_WIDTH,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.label.height, 24.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        geometry.choices.x,
        12.0f + SANDBOX3D_EDITOR_SCALE_COMPACT_LABEL_WIDTH,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.choices.y, 20.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        geometry.choices.width,
        204.0f - SANDBOX3D_EDITOR_SCALE_COMPACT_LABEL_WIDTH,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.choices.height, 24.0f, 0.0001f);
    HENKA_TEST_ASSERT(geometry.choices.width / 4.0f >= 32.0f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_scale_control_layout(
            (henka_ui_rect){12.0f, 20.0f, 192.0f, 24.0f},
            &geometry) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(geometry.compact);
    HENKA_TEST_ASSERT(!geometry.stacked);
    HENKA_TEST_ASSERT(geometry.choices.width / 4.0f >= 32.0f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_scale_control_layout(
            (henka_ui_rect){12.0f, 20.0f, 191.0f, 44.0f},
            &geometry) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!geometry.compact);
    HENKA_TEST_ASSERT(geometry.stacked);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.label.width, 191.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(geometry.choices.width, 191.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        geometry.choices.y + geometry.choices.height,
        64.0f,
        0.0001f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_scale_control_layout(
            (henka_ui_rect){12.0f, 20.0f, 175.0f, 60.0f},
            &geometry) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(geometry.label.width == 0.0f);
    HENKA_TEST_ASSERT(geometry.choices.width == 0.0f);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_scale_control_layout(
            (henka_ui_rect){12.0f, 20.0f, 191.0f, 43.0f},
            &geometry) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(geometry.label.width == 0.0f);
    HENKA_TEST_ASSERT(geometry.choices.width == 0.0f);

    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    henka_ui_set_visible(ui, true);
    frame_desc.framebuffer_width = 1280;
    frame_desc.framebuffer_height = 720;
    frame_desc.mouse_position = (henka_vec2){166.0f, 50.0f};
    frame_desc.mouse_left_down = true;
    frame_desc.mouse_left_pressed = true;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    result = sandbox3d_editor_scale_control_draw(
        ui, bounds, current_percent, &selected_percent, &changed);
    HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!changed);
    HENKA_TEST_ASSERT(selected_percent == current_percent);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    frame_desc.mouse_left_down = false;
    frame_desc.mouse_left_pressed = false;
    frame_desc.mouse_left_released = true;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    result = sandbox3d_editor_scale_control_draw(
        ui, bounds, current_percent, &selected_percent, &changed);
    HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(changed);
    HENKA_TEST_ASSERT(selected_percent == 150U);
    HENKA_TEST_ASSERT(current_percent == 100U);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    {
        static const uint16_t supported_percentages[] = {
            125U,
            200U};
        size_t index;

        for (index = 0U;
             index < sizeof(supported_percentages) /
                 sizeof(supported_percentages[0]);
             ++index)
        {
            memset(&frame_desc, 0, sizeof(frame_desc));
            frame_desc.framebuffer_width = 1280;
            frame_desc.framebuffer_height = 720;
            HENKA_TEST_ASSERT(
                henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
            result = sandbox3d_editor_scale_control_draw(
                ui,
                bounds,
                supported_percentages[index],
                &selected_percent,
                &changed);
            HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(!changed);
            HENKA_TEST_ASSERT(
                selected_percent == supported_percentages[index]);
            HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
        }
    }

    memset(&frame_desc, 0, sizeof(frame_desc));
    frame_desc.framebuffer_width = 1280;
    frame_desc.framebuffer_height = 720;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    result = sandbox3d_editor_scale_control_draw(
        ui, bounds, 150U, &selected_percent, &changed);
    HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!changed);
    HENKA_TEST_ASSERT(selected_percent == 150U);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    selected_percent = 0U;
    changed = true;
    result = sandbox3d_editor_scale_control_draw(
        ui, bounds, 175U, &selected_percent, &changed);
    HENKA_TEST_ASSERT(result == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(selected_percent == 175U);
    HENKA_TEST_ASSERT(!changed);

    henka_test_editor_scale_candidate_prepare_failure_is_atomic();
    henka_test_editor_scale_persist_failure_is_atomic();
    henka_test_editor_scale_success_persists_before_publishing_all_profiles();
    henka_test_editor_scale_startup_rejects_unknown_migration_version_before_prepare();
    henka_test_editor_scale_startup_uses_canonical_value_and_migrates_after_prepare();
    henka_test_editor_scale_completed_startup_uses_canonical_value_without_persisting();
    henka_test_editor_scale_startup_prepare_failure_retains_legacy_state_and_retries();
    henka_test_editor_scale_context_startup_is_transactional();

    henka_ui_destroy(ui);
}
