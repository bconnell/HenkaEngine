#include "editor_typography_runtime.h"

#include "editor_font_bootstrap.h"
#include "editor_preferences.h"
#include "editor_scale_control.h"
#include "editor_scale_transaction.h"

#include <henka/memory.h>
#include <henka/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT \
    ((size_t)SANDBOX3D_WORKSPACE_PANEL_COUNT + 2U)
#define SANDBOX3D_TYPOGRAPHY_ATLAS_WIDTH 1024U
#define SANDBOX3D_TYPOGRAPHY_ATLAS_HEIGHT 1024U
#define SANDBOX3D_TYPOGRAPHY_BASE_PIXEL_HEIGHT 16U
#define SANDBOX3D_TYPOGRAPHY_CONTEXT_ID_BASE UINT64_C(0x48454e4b41000000)
#define SANDBOX3D_TYPOGRAPHY_CAPTURE_DIRECTORY_BYTES 1536U
#define SANDBOX3D_TYPOGRAPHY_CAPTURE_LABEL_BYTES 64U
#define SANDBOX3D_TYPOGRAPHY_CAPTURE_PATH_BYTES 2048U

typedef enum sandbox3d_editor_typography_context_slot
{
    SANDBOX3D_TYPOGRAPHY_MAIN_CONTEXT = 0,
    SANDBOX3D_TYPOGRAPHY_NATIVE_CONTEXT = 1,
    SANDBOX3D_TYPOGRAPHY_FIRST_DETACHED_CONTEXT = 2
} sandbox3d_editor_typography_context_slot;

struct sandbox3d_editor_typography_runtime
{
    henka_engine* engine;
    henka_settings* settings;
    henka_ui_font_resource* font;
    henka_ui_font_raster_profile* profiles[
        SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT];
    henka_ui_context* contexts[SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT];
    henka_window_id window_ids[SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT];
    bool active[SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT];
    uint64_t last_failed_display_scale_signature;
    uint64_t last_failed_display_scale_frame;
    bool display_scale_failure_recorded;
    bool display_scale_query_failure_reported;
    char* settings_path;
    sandbox3d_editor_font_candidate font_candidate;
    const sandbox3d_editor_font_file_asset_desc* font_asset;
    henka_ui_rect last_scale_control_bounds;
    int last_scale_control_percent;
    bool scale_control_telemetry_valid;
    bool pending_scale_change;
    uint16_t pending_scale_percent;
    sandbox3d_editor_ui_preferences preferences;
    bool automation_frame_capture_enabled;
    char automation_frame_capture_directory[
        SANDBOX3D_TYPOGRAPHY_CAPTURE_DIRECTORY_BYTES];
    char automation_frame_capture_label[
        SANDBOX3D_TYPOGRAPHY_CAPTURE_LABEL_BYTES];
};

static bool sandbox3d_typography_environment_flag(const char* name)
{
    const char* value = name != NULL ? getenv(name) : NULL;
    return value != NULL && strcmp(value, "1") == 0;
}

static bool sandbox3d_typography_automation_diagnostics_enabled(void)
{
    return sandbox3d_typography_environment_flag("HENKA_AUTOMATION_INPUT_OWNED") &&
        sandbox3d_typography_environment_flag("HENKA_AUTOMATION_DIAGNOSTICS");
}

static bool sandbox3d_typography_capture_label_is_valid(const char* label)
{
    size_t index;
    if (label == NULL || label[0] == '\0' ||
        strlen(label) >= SANDBOX3D_TYPOGRAPHY_CAPTURE_LABEL_BYTES)
    {
        return false;
    }
    for (index = 0U; label[index] != '\0'; ++index)
    {
        const char value = label[index];
        if (!((value >= 'a' && value <= 'z') ||
              (value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9') || value == '-' || value == '_'))
        {
            return false;
        }
    }
    return true;
}

static bool sandbox3d_typography_copy_environment_value(
    const char* name,
    char* destination,
    size_t destination_capacity,
    bool* out_present)
{
    if (name == NULL || destination == NULL || destination_capacity == 0U ||
        out_present == NULL)
    {
        return false;
    }
    destination[0] = '\0';
    *out_present = false;
#if defined(_WIN32)
    {
        size_t required = 0U;
        if (getenv_s(&required, NULL, 0U, name) != 0)
        {
            return false;
        }
        if (required == 0U)
        {
            return true;
        }
        if (required > destination_capacity ||
            getenv_s(&required, destination, destination_capacity, name) != 0)
        {
            destination[0] = '\0';
            return false;
        }
        *out_present = true;
        return true;
    }
#else
    {
        const char* value = getenv(name);
        size_t value_length;
        if (value == NULL)
        {
            return true;
        }
        value_length = strlen(value);
        if (value_length >= destination_capacity)
        {
            return false;
        }
        memcpy(destination, value, value_length + 1U);
        *out_present = true;
        return true;
    }
#endif
}

static bool sandbox3d_typography_configure_automation_frame_capture(
    sandbox3d_editor_typography_runtime* runtime)
{
    char directory[SANDBOX3D_TYPOGRAPHY_CAPTURE_DIRECTORY_BYTES];
    char label[SANDBOX3D_TYPOGRAPHY_CAPTURE_LABEL_BYTES];
    bool directory_present;
    bool label_present;
    size_t directory_length;
    bool directory_is_absolute;
    if (runtime == NULL || !sandbox3d_typography_automation_diagnostics_enabled())
    {
        return runtime != NULL;
    }
    if (!sandbox3d_typography_copy_environment_value(
            "HENKA_AUTOMATION_FRAME_CAPTURE_DIRECTORY",
            directory, sizeof(directory), &directory_present) ||
        !sandbox3d_typography_copy_environment_value(
            "HENKA_AUTOMATION_FRAME_CAPTURE_LABEL",
            label, sizeof(label), &label_present))
    {
        printf("HENKA_AUTOMATION_DIAGNOSTIC editor_frame_capture stage=configuration_failed reason=environment_value_too_long\n");
        fflush(stdout);
        return false;
    }
    if (!directory_present && !label_present)
    {
        return true;
    }
    if (!directory_present || directory[0] == '\0' || !label_present ||
        !sandbox3d_typography_capture_label_is_valid(label))
    {
        printf("HENKA_AUTOMATION_DIAGNOSTIC editor_frame_capture stage=configuration_failed reason=missing_or_invalid_path_authority\n");
        fflush(stdout);
        return false;
    }
    directory_length = strlen(directory);
    directory_is_absolute = directory[0] == '/';
    if (directory_length >= 2U && directory[0] == '\\' && directory[1] == '\\')
    {
        directory_is_absolute = true;
    }
    if (directory_length >= 3U)
    {
        const bool drive_letter =
            (directory[0] >= 'A' && directory[0] <= 'Z') ||
            (directory[0] >= 'a' && directory[0] <= 'z');
        if (drive_letter && directory[1] == ':' &&
            (directory[2] == '/' || directory[2] == '\\'))
        {
            directory_is_absolute = true;
        }
    }
    if (directory_length >= sizeof(runtime->automation_frame_capture_directory) ||
        !directory_is_absolute)
    {
        printf("HENKA_AUTOMATION_DIAGNOSTIC editor_frame_capture stage=configuration_failed reason=directory_must_be_bounded_and_absolute\n");
        fflush(stdout);
        return false;
    }
    memcpy(runtime->automation_frame_capture_directory, directory, directory_length + 1U);
    memcpy(runtime->automation_frame_capture_label, label, strlen(label) + 1U);
    runtime->automation_frame_capture_enabled = true;
    return true;
}

static void sandbox3d_typography_request_automation_frame_capture(
    sandbox3d_editor_typography_runtime* runtime)
{
    char path[SANDBOX3D_TYPOGRAPHY_CAPTURE_PATH_BYTES];
    int written;
    henka_result result;
    if (runtime == NULL || !runtime->automation_frame_capture_enabled)
    {
        return;
    }
    written = snprintf(
        path,
        sizeof(path),
        "%s/%s-ui-scale-%u.bmp",
        runtime->automation_frame_capture_directory,
        runtime->automation_frame_capture_label,
        (unsigned int)runtime->preferences.scale_percent);
    if (written < 0 || (size_t)written >= sizeof(path))
    {
        printf("HENKA_AUTOMATION_DIAGNOSTIC editor_frame_capture stage=request_failed reason=path_capacity ui_scale_percent=%u\n",
            (unsigned int)runtime->preferences.scale_percent);
        fflush(stdout);
        return;
    }
    result = henka_engine_request_frame_capture(runtime->engine, path);
    if (result != HENKA_SUCCESS)
    {
        printf("HENKA_AUTOMATION_DIAGNOSTIC editor_frame_capture stage=request_failed result=%s ui_scale_percent=%u\n",
            henka_result_to_string(result),
            (unsigned int)runtime->preferences.scale_percent);
        fflush(stdout);
        return;
    }
    printf("HENKA_AUTOMATION_DIAGNOSTIC editor_frame_capture stage=requested label=%s ui_scale_percent=%u\n",
        runtime->automation_frame_capture_label,
        (unsigned int)runtime->preferences.scale_percent);
    fflush(stdout);
}

static void sandbox3d_typography_report_font_state(
    const sandbox3d_editor_typography_runtime* runtime,
    const char* stage)
{
    henka_ui_font_raster_profile_info profile_info;
    uint32_t display_scale_numerator = 0U;
    uint32_t display_scale_denominator = 0U;

    if (runtime == NULL || runtime->font_asset == NULL ||
        !sandbox3d_typography_automation_diagnostics_enabled())
    {
        return;
    }
    if (runtime->profiles[SANDBOX3D_TYPOGRAPHY_MAIN_CONTEXT] != NULL &&
        henka_ui_font_raster_profile_get_info(
            runtime->profiles[SANDBOX3D_TYPOGRAPHY_MAIN_CONTEXT],
            (uint16_t)runtime->preferences.scale_percent,
            &profile_info))
    {
        display_scale_numerator = profile_info.key.output_scale_numerator;
        display_scale_denominator = profile_info.key.output_scale_denominator;
    }
    printf(
        "HENKA_AUTOMATION_DIAGNOSTIC editor_typography stage=%s font=%s font_identity=%s font_sha256=%s ui_scale_percent=%d os_display_scale=%u/%u\n",
        stage != NULL ? stage : "state",
        sandbox3d_editor_font_candidate_get_alias(runtime->font_candidate),
        runtime->font_asset->asset_identity,
        runtime->font_asset->expected_sha256_hex,
        runtime->preferences.scale_percent,
        display_scale_numerator,
        display_scale_denominator);
    fflush(stdout);
}

static void sandbox3d_typography_report_scale_control(
    sandbox3d_editor_typography_runtime* runtime,
    henka_ui_rect bounds,
    const sandbox3d_editor_scale_control_layout_desc* layout)
{
    const bool diagnostics_enabled =
        sandbox3d_typography_automation_diagnostics_enabled();
    const bool changed = runtime != NULL &&
        (!runtime->scale_control_telemetry_valid ||
         runtime->last_scale_control_bounds.x != bounds.x ||
         runtime->last_scale_control_bounds.y != bounds.y ||
         runtime->last_scale_control_bounds.width != bounds.width ||
         runtime->last_scale_control_bounds.height != bounds.height ||
         runtime->last_scale_control_percent != runtime->preferences.scale_percent);

    if (!diagnostics_enabled || !changed || layout == NULL)
    {
        return;
    }
    printf(
        "HENKA_AUTOMATION_DIAGNOSTIC editor_scale_control x=%.3f y=%.3f width=%.3f height=%.3f layout=%s choices_x=%.3f choices_y=%.3f choices_width=%.3f choices_height=%.3f selected_percent=%d\n",
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height,
        layout->stacked ? "stacked" : "inline",
        layout->choices.x,
        layout->choices.y,
        layout->choices.width,
        layout->choices.height,
        runtime->preferences.scale_percent);
    fflush(stdout);
    runtime->last_scale_control_bounds = bounds;
    runtime->last_scale_control_percent = runtime->preferences.scale_percent;
    runtime->scale_control_telemetry_valid = true;
}

static void sandbox3d_typography_log_font_failure(
    const char* operation,
    henka_result result,
    const henka_ui_font_diagnostic* diagnostic)
{
    HENKA_LOG_ERROR(
        "Editor typography %s failed: result=%s stage=%d identity=%s detail=%s",
        operation != NULL ? operation : "operation",
        henka_result_to_string(result),
        diagnostic != NULL ? (int)diagnostic->stage : (int)HENKA_UI_FONT_STAGE_NONE,
        diagnostic != NULL ? diagnostic->identity : "",
        diagnostic != NULL ? diagnostic->message : "");
}

static henka_result sandbox3d_typography_create_profile(
    sandbox3d_editor_typography_runtime* runtime,
    size_t slot,
    uint32_t output_scale_numerator,
    uint32_t output_scale_denominator,
    henka_ui_font_raster_profile** out_profile)
{
    henka_ui_font_raster_profile_desc desc;
    henka_ui_font_diagnostic diagnostic;
    henka_result result;

    if (runtime == NULL || slot >= SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT ||
        runtime->font == NULL || out_profile == NULL || *out_profile != NULL ||
        output_scale_numerator == 0U || output_scale_denominator == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(&desc, 0, sizeof(desc));
    desc.graphics_context_identity =
        SANDBOX3D_TYPOGRAPHY_CONTEXT_ID_BASE + (uint64_t)slot + 1U;
    desc.output_scale_numerator = output_scale_numerator;
    desc.output_scale_denominator = output_scale_denominator;
    desc.atlas_width = SANDBOX3D_TYPOGRAPHY_ATLAS_WIDTH;
    desc.atlas_height = SANDBOX3D_TYPOGRAPHY_ATLAS_HEIGHT;
    result = henka_ui_font_raster_profile_create(
        runtime->font, &desc, out_profile, &diagnostic);
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_typography_log_font_failure(
            "raster profile creation", result, &diagnostic);
    }
    return result;
}

static size_t sandbox3d_typography_collect_active_bindings(
    const sandbox3d_editor_typography_runtime* runtime,
    henka_ui_font_raster_profile* const candidates[
        SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT],
    henka_ui_font_context_scale_binding* bindings,
    size_t capacity)
{
    size_t count = 0U;
    size_t slot;

    if (runtime == NULL || bindings == NULL || capacity == 0U)
    {
        return 0U;
    }
    for (slot = 0U; slot < SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT; ++slot)
    {
        if (!runtime->active[slot])
        {
            continue;
        }
        henka_ui_font_raster_profile* profile = candidates != NULL &&
                candidates[slot] != NULL
            ? candidates[slot]
            : runtime->profiles[slot];
        if (runtime->contexts[slot] == NULL || profile == NULL ||
            count >= capacity)
        {
            return 0U;
        }
        bindings[count].context = runtime->contexts[slot];
        bindings[count].profile = profile;
        ++count;
    }
    return count;
}

static henka_result sandbox3d_typography_update_live_contexts(
    sandbox3d_editor_typography_runtime* runtime)
{
    henka_ui_font_context_scale_binding bindings[
        HENKA_MAX_TOOL_WINDOWS + 1U];
    henka_engine_ui_font_resource_update* update = NULL;
    size_t binding_count;
    henka_result result;

    if (runtime == NULL || runtime->engine == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    binding_count = sandbox3d_typography_collect_active_bindings(
        runtime, NULL, bindings, sizeof(bindings) / sizeof(bindings[0]));
    if (binding_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_engine_ui_font_resource_update_prepare(
        runtime->engine,
        bindings,
        binding_count,
        (uint16_t)runtime->preferences.scale_percent,
        &update);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    henka_engine_ui_font_resource_update_publish(runtime->engine, update);
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_typography_get_display_scale(
    const sandbox3d_editor_typography_runtime* runtime,
    size_t slot,
    uint32_t* out_numerator,
    uint32_t* out_denominator)
{
    henka_result result;

    if (out_numerator != NULL)
    {
        *out_numerator = 0U;
    }
    if (out_denominator != NULL)
    {
        *out_denominator = 0U;
    }
    if (runtime == NULL || runtime->engine == NULL ||
        slot >= SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT ||
        out_numerator == NULL || out_denominator == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (slot == SANDBOX3D_TYPOGRAPHY_MAIN_CONTEXT)
    {
        return henka_engine_get_window_display_scale(
            runtime->engine, out_numerator, out_denominator);
    }
    if (runtime->window_ids[slot] == HENKA_INVALID_WINDOW_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    {
        henka_tool_window_state state;
        result = henka_engine_get_tool_window_state(
            runtime->engine, runtime->window_ids[slot], &state);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (!state.open || state.id != runtime->window_ids[slot] ||
            state.display_scale_numerator == 0U ||
            state.display_scale_denominator == 0U)
        {
            return HENKA_ERROR_PLATFORM;
        }
        *out_numerator = state.display_scale_numerator;
        *out_denominator = state.display_scale_denominator;
    }
    return HENKA_SUCCESS;
}

static bool sandbox3d_typography_profile_matches_display_scale(
    const sandbox3d_editor_typography_runtime* runtime,
    size_t slot,
    uint32_t numerator,
    uint32_t denominator)
{
    henka_ui_font_raster_profile_info info;
    return runtime != NULL && slot < SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT &&
        runtime->profiles[slot] != NULL &&
        henka_ui_font_raster_profile_get_info(
            runtime->profiles[slot],
            (uint16_t)runtime->preferences.scale_percent,
            &info) &&
        info.key.output_scale_numerator == numerator &&
        info.key.output_scale_denominator == denominator;
}

henka_result sandbox3d_editor_typography_runtime_sync_display_scales(
    sandbox3d_editor_typography_runtime* runtime)
{
    henka_ui_font_raster_profile* candidates[
        SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT] = {NULL};
    henka_ui_font_context_scale_binding bindings[HENKA_MAX_TOOL_WINDOWS + 1U];
    henka_engine_ui_font_resource_update* update = NULL;
    uint32_t numerators[SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT] = {0U};
    uint32_t denominators[SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT] = {0U};
    uint64_t signature = UINT64_C(1469598103934665603);
    const uint64_t frame_index = runtime != NULL && runtime->engine != NULL
        ? henka_engine_get_frame_index(runtime->engine)
        : 0U;
    size_t binding_count;
    size_t slot;
    bool changed = false;
    henka_result result = HENKA_SUCCESS;

    if (runtime == NULL || runtime->engine == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (slot = 0U; slot < SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT; ++slot)
    {
        if (!runtime->active[slot])
        {
            continue;
        }
        result = sandbox3d_typography_get_display_scale(
            runtime, slot, &numerators[slot], &denominators[slot]);
        if (result != HENKA_SUCCESS)
        {
            if (!runtime->display_scale_query_failure_reported)
            {
                HENKA_LOG_WARN(
                    "Editor display-scale query failed for context %zu: %s; keeping its current font/layout resources.",
                    slot, henka_result_to_string(result));
                runtime->display_scale_query_failure_reported = true;
            }
            return result;
        }
        runtime->display_scale_query_failure_reported = false;
        signature ^= (uint64_t)(slot + 1U);
        signature *= UINT64_C(1099511628211);
        signature ^= (uint64_t)numerators[slot];
        signature *= UINT64_C(1099511628211);
        signature ^= (uint64_t)denominators[slot];
        signature *= UINT64_C(1099511628211);
        if (!sandbox3d_typography_profile_matches_display_scale(
                runtime, slot, numerators[slot], denominators[slot]))
        {
            changed = true;
        }
    }
    if (!changed)
    {
        runtime->display_scale_failure_recorded = false;
        return HENKA_SUCCESS;
    }
    if (runtime->display_scale_failure_recorded &&
        runtime->last_failed_display_scale_signature == signature &&
        frame_index >= runtime->last_failed_display_scale_frame &&
        frame_index - runtime->last_failed_display_scale_frame < 60U)
    {
        return HENKA_SUCCESS;
    }

    for (slot = 0U; slot < SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT; ++slot)
    {
        if (!runtime->active[slot] ||
            sandbox3d_typography_profile_matches_display_scale(
                runtime, slot, numerators[slot], denominators[slot]))
        {
            continue;
        }
        result = sandbox3d_typography_create_profile(
            runtime, slot, numerators[slot], denominators[slot], &candidates[slot]);
        if (result != HENKA_SUCCESS)
        {
            goto fail;
        }
    }

    binding_count = sandbox3d_typography_collect_active_bindings(
        runtime, candidates, bindings, sizeof(bindings) / sizeof(bindings[0]));
    if (binding_count == 0U)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto fail;
    }
    result = henka_engine_ui_font_resource_update_prepare(
        runtime->engine,
        bindings,
        binding_count,
        (uint16_t)runtime->preferences.scale_percent,
        &update);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    henka_engine_ui_font_resource_update_publish(runtime->engine, update);
    update = NULL;
    for (slot = 0U; slot < SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT; ++slot)
    {
        if (candidates[slot] != NULL)
        {
            henka_ui_font_raster_profile* previous = runtime->profiles[slot];
            runtime->profiles[slot] = candidates[slot];
            candidates[slot] = NULL;
            henka_ui_font_raster_profile_destroy(previous);
        }
    }
    runtime->display_scale_failure_recorded = false;
    return HENKA_SUCCESS;

fail:
    if (update != NULL)
    {
        henka_engine_ui_font_resource_update_discard(runtime->engine, update);
    }
    for (slot = 0U; slot < SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT; ++slot)
    {
        henka_ui_font_raster_profile_destroy(candidates[slot]);
    }
    if (!runtime->display_scale_failure_recorded ||
        runtime->last_failed_display_scale_signature != signature ||
        frame_index - runtime->last_failed_display_scale_frame >= 60U)
    {
        HENKA_LOG_WARN(
            "Editor display-scale resource update failed: %s; the previous complete font/layout state remains active.",
            henka_result_to_string(result));
    }
    runtime->last_failed_display_scale_signature = signature;
    runtime->last_failed_display_scale_frame = frame_index;
    runtime->display_scale_failure_recorded = true;
    return result;
}

static bool sandbox3d_typography_bindings(
    sandbox3d_editor_typography_runtime* runtime,
    henka_ui_font_context_scale_binding* bindings,
    size_t capacity,
    size_t* out_count)
{
    if (out_count != NULL)
    {
        *out_count = 0U;
    }
    if (runtime == NULL || bindings == NULL || out_count == NULL ||
        capacity < HENKA_MAX_TOOL_WINDOWS + 1U)
    {
        return false;
    }
    *out_count = sandbox3d_typography_collect_active_bindings(
        runtime, NULL, bindings, capacity);
    return *out_count > 0U;
}

henka_result sandbox3d_editor_typography_runtime_initialize(
    sandbox3d_editor_typography_runtime** out_runtime,
    henka_engine* engine,
    henka_settings* settings,
    henka_ui_context* main_context,
    henka_ui_context* native_context,
    henka_ui_context* detached_contexts[SANDBOX3D_WORKSPACE_PANEL_COUNT],
    const char* asset_base_path,
    const char* settings_path)
{
    sandbox3d_editor_typography_runtime* runtime = NULL;
    sandbox3d_editor_font_file_asset_desc asset;
    const sandbox3d_editor_font_file_asset_desc* selected_asset = NULL;
    const char* requested_font_alias = getenv("HENKA_AUTOMATION_EDITOR_FONT");
    sandbox3d_editor_font_candidate font_candidate =
        SANDBOX3D_EDITOR_FONT_CANDIDATE_ATKINSON;
    henka_ui_font_diagnostic diagnostic;
    henka_ui_font_context_scale_binding bindings[HENKA_MAX_TOOL_WINDOWS + 1U];
    henka_engine_ui_font_resource_update* update = NULL;
    size_t binding_count;
    size_t panel_index;
    size_t settings_path_length;
    uint32_t main_display_scale_numerator = 0U;
    uint32_t main_display_scale_denominator = 0U;
    const bool automation_input_owned =
        sandbox3d_typography_environment_flag("HENKA_AUTOMATION_INPUT_OWNED");
    const bool automation_diagnostics_enabled =
        sandbox3d_typography_environment_flag("HENKA_AUTOMATION_DIAGNOSTICS");
    bool migration_pending = false;
    sandbox3d_editor_scale_failure_stage failure_stage =
        SANDBOX3D_EDITOR_SCALE_FAILURE_NONE;
    henka_result result;

    if (out_runtime == NULL || *out_runtime != NULL || engine == NULL ||
        settings == NULL || main_context == NULL || native_context == NULL ||
        detached_contexts == NULL || asset_base_path == NULL || settings_path == NULL ||
        settings_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_runtime = NULL;

    runtime = (sandbox3d_editor_typography_runtime*)henka_calloc(1U, sizeof(*runtime));
    if (runtime == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    settings_path_length = strlen(settings_path);
    runtime->settings_path = (char*)henka_malloc(settings_path_length + 1U);
    if (runtime->settings_path == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto failure;
    }
    memcpy(runtime->settings_path, settings_path, settings_path_length + 1U);
    runtime->engine = engine;
    runtime->settings = settings;
    if (!sandbox3d_editor_font_candidate_resolve(
            requested_font_alias,
            automation_input_owned,
            automation_diagnostics_enabled,
            &font_candidate))
    {
        HENKA_LOG_ERROR(
            "Editor typography automation font candidate '%s' is unsupported; use only the bundled atkinson or noto candidate.",
            requested_font_alias != NULL ? requested_font_alias : "(missing)");
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto failure;
    }
    selected_asset = sandbox3d_editor_font_candidate_get_asset(font_candidate);
    if (selected_asset == NULL)
    {
        HENKA_LOG_ERROR("Editor typography bundled font descriptor is unavailable.");
        result = HENKA_ERROR_ASSET_SOURCE;
        goto failure;
    }
    runtime->font_candidate = font_candidate;
    runtime->font_asset = selected_asset;
    if (!sandbox3d_typography_configure_automation_frame_capture(runtime))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto failure;
    }
    runtime->contexts[SANDBOX3D_TYPOGRAPHY_MAIN_CONTEXT] = main_context;
    runtime->contexts[SANDBOX3D_TYPOGRAPHY_NATIVE_CONTEXT] = native_context;
    for (panel_index = 0U; panel_index < SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT; ++panel_index)
    {
        runtime->window_ids[panel_index] = HENKA_INVALID_WINDOW_ID;
    }
    for (panel_index = 0U; panel_index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++panel_index)
    {
        if (detached_contexts[panel_index] == NULL)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto failure;
        }
        runtime->contexts[SANDBOX3D_TYPOGRAPHY_FIRST_DETACHED_CONTEXT + panel_index] =
            detached_contexts[panel_index];
    }
    sandbox3d_editor_ui_preferences_defaults(&runtime->preferences);

    asset = *selected_asset;
    result = sandbox3d_editor_font_bootstrap_load(
        asset_base_path, &asset, &runtime->font, &diagnostic);
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_typography_log_font_failure("bundled font load", result, &diagnostic);
        goto failure;
    }
    result = henka_engine_get_window_display_scale(
        engine, &main_display_scale_numerator, &main_display_scale_denominator);
    if (result != HENKA_SUCCESS || main_display_scale_numerator == 0U ||
        main_display_scale_denominator == 0U)
    {
        if (result == HENKA_SUCCESS)
        {
            result = HENKA_ERROR_PLATFORM;
        }
        HENKA_LOG_ERROR("Editor typography main-window display scale is unavailable: %s",
            henka_result_to_string(result));
        goto failure;
    }
    result = sandbox3d_typography_create_profile(
        runtime,
        SANDBOX3D_TYPOGRAPHY_MAIN_CONTEXT,
        main_display_scale_numerator,
        main_display_scale_denominator,
        &runtime->profiles[SANDBOX3D_TYPOGRAPHY_MAIN_CONTEXT]);
    if (result != HENKA_SUCCESS)
    {
        goto failure;
    }
    runtime->active[SANDBOX3D_TYPOGRAPHY_MAIN_CONTEXT] = true;
    binding_count = sandbox3d_typography_collect_active_bindings(
        runtime, NULL, bindings, sizeof(bindings) / sizeof(bindings[0]));
    if (binding_count != 1U)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto failure;
    }
    result = henka_engine_ui_font_resource_update_prepare(
        engine, bindings, binding_count,
        (uint16_t)runtime->preferences.scale_percent, &update);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("Editor typography initial GPU atlas preparation failed: %s",
            henka_result_to_string(result));
        goto failure;
    }
    henka_engine_ui_font_resource_update_publish(engine, update);
    update = NULL;

    result = sandbox3d_editor_ui_scale_startup_transition_apply_contexts(
        engine,
        settings,
        runtime->settings_path,
        &runtime->preferences,
        bindings,
        binding_count,
        &migration_pending,
        &failure_stage);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_WARN(
            "Editor UI scale startup transition failed at %s: %s; retaining the fully installed 100%% font/layout state.",
            sandbox3d_editor_typography_runtime_failure_stage_label((int)failure_stage),
            henka_result_to_string(result));
        sandbox3d_editor_ui_preferences_defaults(&runtime->preferences);
    }
    else if (migration_pending)
    {
        HENKA_LOG_INFO("Editor UI scale preference migration completed at %d%%.",
            runtime->preferences.scale_percent);
    }

    sandbox3d_typography_report_font_state(runtime, "loaded");
    sandbox3d_typography_request_automation_frame_capture(runtime);
    *out_runtime = runtime;
    return HENKA_SUCCESS;

failure:
    if (update != NULL)
    {
        henka_engine_ui_font_resource_update_discard(engine, update);
    }
    sandbox3d_editor_typography_runtime_destroy(runtime);
    return result;
}

static size_t sandbox3d_typography_find_context_slot(
    const sandbox3d_editor_typography_runtime* runtime,
    const henka_ui_context* context)
{
    size_t slot;
    if (runtime == NULL || context == NULL)
    {
        return SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT;
    }
    for (slot = 0U; slot < SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT; ++slot)
    {
        if (runtime->contexts[slot] == context)
        {
            return slot;
        }
    }
    return SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT;
}

henka_result sandbox3d_editor_typography_runtime_attach_context(
    sandbox3d_editor_typography_runtime* runtime,
    henka_ui_context* context,
    henka_window_id window_id)
{
    const size_t slot = sandbox3d_typography_find_context_slot(runtime, context);
    henka_ui_font_raster_profile* candidates[
        SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT] = {NULL};
    henka_ui_font_context_scale_binding bindings[HENKA_MAX_TOOL_WINDOWS + 1U];
    henka_engine_ui_font_resource_update* update = NULL;
    uint32_t display_scale_numerator = 0U;
    uint32_t display_scale_denominator = 0U;
    size_t binding_count;
    henka_result result;

    if (slot == SANDBOX3D_TYPOGRAPHY_MAIN_CONTEXT ||
        slot >= SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT ||
        window_id == HENKA_INVALID_WINDOW_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (runtime->active[slot])
    {
        return runtime->window_ids[slot] == window_id
            ? HENKA_SUCCESS
            : HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime->window_ids[slot] = window_id;
    result = sandbox3d_typography_get_display_scale(
        runtime, slot, &display_scale_numerator, &display_scale_denominator);
    if (result != HENKA_SUCCESS || display_scale_numerator == 0U ||
        display_scale_denominator == 0U)
    {
        runtime->window_ids[slot] = HENKA_INVALID_WINDOW_ID;
        return result != HENKA_SUCCESS ? result : HENKA_ERROR_PLATFORM;
    }
    runtime->window_ids[slot] = window_id;
    if (!sandbox3d_typography_profile_matches_display_scale(
            runtime, slot, display_scale_numerator, display_scale_denominator))
    {
        result = sandbox3d_typography_create_profile(
            runtime, slot, display_scale_numerator,
            display_scale_denominator, &candidates[slot]);
        if (result != HENKA_SUCCESS)
        {
            runtime->window_ids[slot] = HENKA_INVALID_WINDOW_ID;
            return result;
        }
    }
    runtime->active[slot] = true;
    binding_count = sandbox3d_typography_collect_active_bindings(
        runtime, candidates, bindings, sizeof(bindings) / sizeof(bindings[0]));
    result = binding_count > 0U
        ? henka_engine_ui_font_resource_update_prepare(
            runtime->engine,
            bindings,
            binding_count,
            (uint16_t)runtime->preferences.scale_percent,
            &update)
        : HENKA_ERROR_INVALID_ARGUMENT;
    if (result != HENKA_SUCCESS)
    {
        runtime->active[slot] = false;
        runtime->window_ids[slot] = HENKA_INVALID_WINDOW_ID;
        henka_ui_font_raster_profile_destroy(candidates[slot]);
        HENKA_LOG_ERROR(
            "Editor typography tool-window candidate preparation failed: %s; the existing UI/font state was retained.",
            henka_result_to_string(result));
        return result;
    }
    henka_engine_ui_font_resource_update_publish(runtime->engine, update);
    if (candidates[slot] != NULL)
    {
        henka_ui_font_raster_profile* previous = runtime->profiles[slot];
        runtime->profiles[slot] = candidates[slot];
        henka_ui_font_raster_profile_destroy(previous);
    }
    if (sandbox3d_typography_automation_diagnostics_enabled())
    {
        henka_ui_font_raster_profile_info profile_info;
        if (runtime->profiles[slot] != NULL &&
            henka_ui_font_raster_profile_get_info(
                runtime->profiles[slot],
                (uint16_t)runtime->preferences.scale_percent,
                &profile_info))
        {
            printf(
                "HENKA_AUTOMATION_DIAGNOSTIC editor_typography_context stage=attached window_id=%u font=%s font_sha256=%s ui_scale_percent=%d os_display_scale=%u/%u\n",
                (unsigned int)window_id,
                sandbox3d_editor_font_candidate_get_alias(runtime->font_candidate),
                runtime->font_asset->expected_sha256_hex,
                runtime->preferences.scale_percent,
                profile_info.key.output_scale_numerator,
                profile_info.key.output_scale_denominator);
        }
        else
        {
            printf(
                "HENKA_AUTOMATION_DIAGNOSTIC editor_typography_context stage=attached profile=unavailable window_id=%u\n",
                (unsigned int)window_id);
        }
        fflush(stdout);
    }
    return HENKA_SUCCESS;
}

void sandbox3d_editor_typography_runtime_detach_context(
    sandbox3d_editor_typography_runtime* runtime,
    henka_ui_context* context)
{
    const size_t slot = sandbox3d_typography_find_context_slot(runtime, context);
    if (slot < SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT &&
        slot != SANDBOX3D_TYPOGRAPHY_MAIN_CONTEXT)
    {
        runtime->active[slot] = false;
        runtime->window_ids[slot] = HENKA_INVALID_WINDOW_ID;
    }
}

henka_result sandbox3d_editor_typography_runtime_draw_scale_control(
    sandbox3d_editor_typography_runtime* runtime,
    henka_ui_context* ui,
    henka_ui_rect bounds)
{
    sandbox3d_editor_scale_control_layout_desc layout;
    uint16_t selected_percent;
    bool changed;
    henka_result result;

    if (runtime == NULL || ui == NULL || ui != runtime->contexts[0] ||
        !sandbox3d_editor_ui_preferences_validate(&runtime->preferences))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_editor_scale_control_layout(bounds, &layout);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    sandbox3d_typography_report_scale_control(runtime, bounds, &layout);
    henka_ui_label(ui, layout.label.x, layout.label.y, 1.0f, "Scale %");
    result = sandbox3d_editor_scale_control_draw(
        ui,
        layout.choices,
        (uint16_t)runtime->preferences.scale_percent,
        &selected_percent,
        &changed);
    if (result != HENKA_SUCCESS || !changed)
    {
        return result;
    }
    runtime->pending_scale_percent = selected_percent;
    runtime->pending_scale_change = true;
    if (sandbox3d_typography_automation_diagnostics_enabled())
    {
        printf(
            "HENKA_AUTOMATION_DIAGNOSTIC editor_typography stage=scale_requested current_scale_percent=%d requested_scale_percent=%u\n",
            runtime->preferences.scale_percent,
            (unsigned int)selected_percent);
        fflush(stdout);
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_typography_runtime_apply_pending_scale(
    sandbox3d_editor_typography_runtime* runtime)
{
    henka_ui_font_context_scale_binding bindings[HENKA_MAX_TOOL_WINDOWS + 1U];
    sandbox3d_editor_ui_preferences candidate;
    sandbox3d_editor_scale_failure_stage failure_stage =
        SANDBOX3D_EDITOR_SCALE_FAILURE_ARGUMENT;
    size_t binding_count;
    uint16_t selected_percent;
    henka_result result;

    if (runtime == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!runtime->pending_scale_change)
    {
        return HENKA_SUCCESS;
    }

    selected_percent = runtime->pending_scale_percent;
    runtime->pending_scale_change = false;
    if (!sandbox3d_editor_ui_preferences_set_scale_percent(
            &candidate, (int)selected_percent) ||
        !sandbox3d_typography_bindings(
            runtime,
            bindings,
            sizeof(bindings) / sizeof(bindings[0]),
            &binding_count))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    else
    {
        result = sandbox3d_editor_ui_scale_transition_apply_contexts(
            runtime->engine,
            runtime->settings,
            runtime->settings_path,
            &runtime->preferences,
            &candidate,
            false,
            bindings,
            binding_count,
            &failure_stage);
    }
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("Editor UI scale change failed at %s: %s; previous scale remains active.",
            sandbox3d_editor_typography_runtime_failure_stage_label((int)failure_stage),
            henka_result_to_string(result));
    }
    else
    {
        sandbox3d_typography_report_font_state(runtime, "scale_applied");
        sandbox3d_typography_request_automation_frame_capture(runtime);
    }
    return result;
}

int sandbox3d_editor_typography_runtime_get_scale_percent(
    const sandbox3d_editor_typography_runtime* runtime)
{
    return runtime != NULL ? runtime->preferences.scale_percent : 0;
}

float sandbox3d_editor_typography_runtime_get_layout_scale(
    const sandbox3d_editor_typography_runtime* runtime)
{
    return runtime != NULL
        ? (float)runtime->preferences.scale_percent / 100.0f
        : 1.0f;
}

const char* sandbox3d_editor_typography_runtime_failure_stage_label(int stage)
{
    switch ((sandbox3d_editor_scale_failure_stage)stage)
    {
        case SANDBOX3D_EDITOR_SCALE_FAILURE_NONE: return "none";
        case SANDBOX3D_EDITOR_SCALE_FAILURE_ARGUMENT: return "argument";
        case SANDBOX3D_EDITOR_SCALE_FAILURE_RESOLVE: return "settings resolution";
        case SANDBOX3D_EDITOR_SCALE_FAILURE_PREPARE: return "font/atlas preparation";
        case SANDBOX3D_EDITOR_SCALE_FAILURE_PERSIST: return "settings persistence";
        default: return "unknown";
    }
}

void sandbox3d_editor_typography_runtime_destroy(
    sandbox3d_editor_typography_runtime* runtime)
{
    size_t slot;
    if (runtime == NULL)
    {
        return;
    }
    for (slot = 0U; slot < SANDBOX3D_TYPOGRAPHY_CONTEXT_COUNT; ++slot)
    {
        henka_ui_font_raster_profile_destroy(runtime->profiles[slot]);
        runtime->profiles[slot] = NULL;
    }
    henka_ui_font_resource_destroy(runtime->font);
    runtime->font = NULL;
    henka_free(runtime->settings_path);
    runtime->settings_path = NULL;
    henka_free(runtime);
}
