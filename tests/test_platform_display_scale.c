#include "test_suite.h"

#include <henka/engine.h>
#include <henka/ui.h>
#include <henka/ui_font.h>
#include <SDL3/SDL.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../engine/src/ui/ui_internal.h"

#define HENKA_PLATFORM_SCALE_FONT_FIXTURE "tests/fixtures/fonts/AtkinsonHyperlegible-Regular.ttf"
#define HENKA_PLATFORM_SCALE_FONT_SHA256 \
    "7FB917C89019896D0B52EE84B7CBB3304C18CB90B19A62F5E32712BD23E97669"

typedef struct henka_platform_scale_profile_fixture
{
    henka_ui_context* ui;
    henka_ui_font_resource* font;
    henka_ui_font_raster_profile* first_profile;
    henka_ui_font_raster_profile* replacement_profile;
    henka_ui_font_raster_profile* wrong_context_profile;
    henka_ui_font_raster_profile* detached_profile;
    henka_ui_font_raster_profile* detached_replacement_profile;
    henka_ui_font_raster_profile* detached_second_replacement_profile;
    henka_ui_font_raster_profile* detached_wrong_context_profile;
    uint8_t* font_bytes;
} henka_platform_scale_profile_fixture;

typedef struct henka_platform_tool_window_close_fixture
{
    henka_window_id window_id;
    uint32_t native_window_id;
    henka_result state_result;
    henka_result close_result;
    bool observe_close_request;
    bool update_called;
    bool close_request_observed;
    bool owner_closed_window;
    bool main_close_request_seen;
    bool update_after_deferred_main_close;
} henka_platform_tool_window_close_fixture;

static bool henka_test_platform_main_close_requested(
    henka_engine* engine,
    void* user_data)
{
    henka_platform_tool_window_close_fixture* fixture =
        (henka_platform_tool_window_close_fixture*)user_data;

    (void)engine;
    if (fixture == NULL)
    {
        return true;
    }
    fixture->main_close_request_seen = true;
    return false;
}

static void henka_test_platform_tool_window_close_update(
    henka_engine* engine,
    double delta_seconds,
    void* user_data)
{
    henka_platform_tool_window_close_fixture* fixture =
        (henka_platform_tool_window_close_fixture*)user_data;
    henka_tool_window_state state;

    (void)delta_seconds;
    if (engine == NULL || fixture == NULL || !fixture->observe_close_request)
    {
        return;
    }

    fixture->observe_close_request = false;
    fixture->update_called = true;
    fixture->update_after_deferred_main_close =
        fixture->main_close_request_seen;
    fixture->state_result = henka_engine_get_tool_window_state(
        engine, fixture->window_id, &state);
    if (fixture->state_result == HENKA_SUCCESS &&
        state.id == fixture->window_id &&
        state.native_window_id == fixture->native_window_id &&
        state.close_requested)
    {
        fixture->close_request_observed = true;
        fixture->close_result = henka_engine_close_tool_window(
            engine, fixture->window_id);
        fixture->owner_closed_window = fixture->close_result == HENKA_SUCCESS;
    }
    henka_engine_request_exit(engine);
}

static bool henka_test_platform_scale_load_font(
    uint8_t** out_bytes,
    size_t* out_size,
    uint32_t* codepoints,
    size_t codepoint_capacity,
    size_t* out_codepoint_count)
{
    FILE* file = NULL;
    long file_size;
    size_t index;

    if (out_bytes != NULL)
    {
        *out_bytes = NULL;
    }
    if (out_size != NULL)
    {
        *out_size = 0U;
    }
    if (out_codepoint_count != NULL)
    {
        *out_codepoint_count = 0U;
    }
    if (out_bytes == NULL || out_size == NULL || codepoints == NULL ||
        codepoint_capacity < 96U || out_codepoint_count == NULL)
    {
        return false;
    }
#if defined(_MSC_VER)
    if (fopen_s(&file, HENKA_PLATFORM_SCALE_FONT_FIXTURE, "rb") != 0)
    {
        return false;
    }
#else
    file = fopen(HENKA_PLATFORM_SCALE_FONT_FIXTURE, "rb");
#endif
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0)
    {
        if (file != NULL)
        {
            (void)fclose(file);
        }
        return false;
    }
    file_size = ftell(file);
    if (file_size <= 0L || (unsigned long)file_size > HENKA_UI_FONT_MAX_SOURCE_BYTES ||
        fseek(file, 0L, SEEK_SET) != 0)
    {
        (void)fclose(file);
        return false;
    }
    *out_bytes = (uint8_t*)malloc((size_t)file_size);
    if (*out_bytes == NULL ||
        fread(*out_bytes, 1U, (size_t)file_size, file) != (size_t)file_size)
    {
        free(*out_bytes);
        *out_bytes = NULL;
        (void)fclose(file);
        return false;
    }
    (void)fclose(file);
    *out_size = (size_t)file_size;
    for (index = 0U; index < 95U; ++index)
    {
        codepoints[index] = (uint32_t)(32U + index);
    }
    codepoints[95U] = 0x2026U;
    *out_codepoint_count = 96U;
    return true;
}

static bool henka_test_platform_scale_profile_replacement(
    henka_engine* engine,
    henka_platform_scale_profile_fixture* out_fixture)
{
    henka_ui_context* ui = NULL;
    henka_ui_font_resource* font = NULL;
    henka_ui_font_raster_profile* first_profile = NULL;
    henka_ui_font_raster_profile* replacement_profile = NULL;
    henka_ui_font_raster_profile* wrong_context_profile = NULL;
    henka_engine_ui_font_resource_update* update = NULL;
    henka_ui_font_context_scale_binding binding;
    henka_ui_font_asset_desc asset;
    henka_ui_font_raster_profile_desc profile_desc;
    henka_ui_font_diagnostic diagnostic;
    uint8_t* font_bytes = NULL;
    uint32_t codepoints[96U];
    size_t font_byte_count = 0U;
    size_t codepoint_count = 0U;
    const char* stage = "font fixture";
    bool passed = false;

    if (out_fixture != NULL)
    {
        memset(out_fixture, 0, sizeof(*out_fixture));
    }
    if (engine == NULL || out_fixture == NULL ||
        !henka_test_platform_scale_load_font(
            &font_bytes, &font_byte_count, codepoints,
            sizeof(codepoints) / sizeof(codepoints[0]), &codepoint_count) ||
        henka_ui_create(&ui) != HENKA_SUCCESS || ui == NULL ||
        henka_engine_set_ui_context(engine, ui) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    memset(&asset, 0, sizeof(asset));
    stage = "font resource";
    asset.bytes = font_bytes;
    asset.byte_count = font_byte_count;
    asset.asset_identity = "test/AtkinsonHyperlegible-Regular.ttf";
    asset.expected_sha256_hex = HENKA_PLATFORM_SCALE_FONT_SHA256;
    asset.required_codepoints = codepoints;
    asset.required_codepoint_count = codepoint_count;
    asset.raster_pixel_height = 16U;
    asset.atlas_width = 1024U;
    asset.atlas_height = 1024U;
    if (henka_ui_font_resource_create_from_asset(
            &asset, &font, &diagnostic) != HENKA_SUCCESS || font == NULL)
    {
        goto cleanup;
    }

    memset(&profile_desc, 0, sizeof(profile_desc));
    stage = "initial profile";
    profile_desc.graphics_context_identity = UINT64_C(0x48454e4b415001);
    profile_desc.output_scale_numerator = 1U;
    profile_desc.output_scale_denominator = 1U;
    profile_desc.atlas_width = 1024U;
    profile_desc.atlas_height = 1024U;
    if (henka_ui_font_raster_profile_create(
            font, &profile_desc, &first_profile, &diagnostic) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    binding = (henka_ui_font_context_scale_binding){ui, first_profile};
    stage = "initial renderer publication";
    if (henka_engine_ui_font_resource_update_prepare(
            engine, &binding, 1U, 100U, &update) != HENKA_SUCCESS || update == NULL ||
        ui->font_profile != NULL)
    {
        goto cleanup;
    }
    henka_engine_ui_font_resource_update_publish(engine, update);
    update = NULL;
    if (ui->font_profile != first_profile)
    {
        goto cleanup;
    }

    profile_desc.output_scale_numerator = 3U;
    profile_desc.output_scale_denominator = 2U;
    stage = "replacement profile creation";
    if (henka_ui_font_raster_profile_create(
            font, &profile_desc, &replacement_profile, &diagnostic) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    binding.profile = replacement_profile;
    stage = "same-context renderer replacement";
    if (henka_engine_ui_font_resource_update_prepare(
            engine, &binding, 1U, 100U, &update) != HENKA_SUCCESS || update == NULL ||
        ui->font_profile != first_profile)
    {
        goto cleanup;
    }
    henka_engine_ui_font_resource_update_publish(engine, update);
    update = NULL;
    if (ui->font_profile != replacement_profile)
    {
        goto cleanup;
    }

    profile_desc.graphics_context_identity += 1U;
    stage = "wrong-context negative control";
    if (henka_ui_font_raster_profile_create(
            font, &profile_desc, &wrong_context_profile, &diagnostic) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    binding.profile = wrong_context_profile;
    if (henka_engine_ui_font_resource_update_prepare(
            engine, &binding, 1U, 100U, &update) == HENKA_SUCCESS ||
        update != NULL || ui->font_profile != replacement_profile)
    {
        goto cleanup;
    }
    passed = true;

cleanup:
    if (!passed)
    {
        fprintf(stderr, "display-scale font profile regression failed at %s\n", stage);
    }
    if (update != NULL)
    {
        henka_engine_ui_font_resource_update_discard(engine, update);
    }
    out_fixture->ui = ui;
    out_fixture->font = font;
    out_fixture->first_profile = first_profile;
    out_fixture->replacement_profile = replacement_profile;
    out_fixture->wrong_context_profile = wrong_context_profile;
    out_fixture->font_bytes = font_bytes;
    return passed;
}

static bool henka_test_platform_display_scale_context_replacement_reuses_and_retires_profile(
    henka_engine* engine,
    henka_ui_context* detached_ui,
    uint32_t initial_scale_numerator,
    uint32_t initial_scale_denominator,
    henka_platform_scale_profile_fixture* fixture)
{
    henka_ui_font_context_scale_binding bindings[2U];
    henka_engine_ui_font_resource_update* update = NULL;
    henka_ui_font_raster_profile_desc profile_desc;
    henka_ui_font_diagnostic diagnostic;
    uint32_t replacement_scale_numerator = 3U;
    uint32_t replacement_scale_denominator = 2U;
    uint32_t second_replacement_scale_numerator = 1U;
    uint32_t second_replacement_scale_denominator = 1U;
    const uint64_t detached_identity = UINT64_C(0x48454e4b415002);
    const char* stage = "detached profile inputs";
    bool passed = false;

    if (engine == NULL || detached_ui == NULL || fixture == NULL ||
        fixture->ui == NULL || fixture->font == NULL ||
        fixture->replacement_profile == NULL ||
        initial_scale_numerator == 0U || initial_scale_denominator == 0U)
    {
        goto cleanup;
    }
    if ((uint64_t)initial_scale_numerator * replacement_scale_denominator ==
        (uint64_t)replacement_scale_numerator * initial_scale_denominator)
    {
        replacement_scale_numerator = 5U;
        replacement_scale_denominator = 4U;
    }
    if (initial_scale_numerator == initial_scale_denominator)
    {
        second_replacement_scale_numerator = 2U;
    }

    memset(&profile_desc, 0, sizeof(profile_desc));
    profile_desc.graphics_context_identity = detached_identity;
    profile_desc.output_scale_numerator = initial_scale_numerator;
    profile_desc.output_scale_denominator = initial_scale_denominator;
    profile_desc.atlas_width = 1024U;
    profile_desc.atlas_height = 1024U;
    stage = "detached initial profile creation";
    if (henka_ui_font_raster_profile_create(
            fixture->font, &profile_desc, &fixture->detached_profile,
            &diagnostic) != HENKA_SUCCESS || fixture->detached_profile == NULL)
    {
        goto cleanup;
    }

    bindings[0U] = (henka_ui_font_context_scale_binding){
        fixture->ui, fixture->replacement_profile};
    bindings[1U] = (henka_ui_font_context_scale_binding){
        detached_ui, fixture->detached_profile};
    stage = "detached profile initial publication prepare";
    if (henka_engine_ui_font_resource_update_prepare(
            engine, bindings, 2U, 100U, &update) != HENKA_SUCCESS || update == NULL ||
        fixture->ui->font_profile != fixture->replacement_profile ||
        detached_ui->font_profile != NULL)
    {
        goto cleanup;
    }
    henka_engine_ui_font_resource_update_publish(engine, update);
    update = NULL;
    if (fixture->ui->font_profile != fixture->replacement_profile ||
        fabsf(fixture->ui->output_scale - 1.5f) > 0.0001f ||
        detached_ui->font_profile != fixture->detached_profile ||
        detached_ui->font_context_identity != detached_identity ||
        detached_ui->font_app_scale_percent != 100U ||
        fabsf(detached_ui->output_scale -
            (float)initial_scale_numerator / (float)initial_scale_denominator) > 0.0001f)
    {
        goto cleanup;
    }

    profile_desc.output_scale_numerator = replacement_scale_numerator;
    profile_desc.output_scale_denominator = replacement_scale_denominator;
    stage = "detached replacement profile creation";
    if (henka_ui_font_raster_profile_create(
            fixture->font, &profile_desc, &fixture->detached_replacement_profile,
            &diagnostic) != HENKA_SUCCESS ||
        fixture->detached_replacement_profile == NULL)
    {
        goto cleanup;
    }
    bindings[1U].profile = fixture->detached_replacement_profile;
    stage = "detached replacement prepare remains unpublished";
    if (henka_engine_ui_font_resource_update_prepare(
            engine, bindings, 2U, 100U, &update) != HENKA_SUCCESS || update == NULL ||
        fixture->ui->font_profile != fixture->replacement_profile ||
        detached_ui->font_profile != fixture->detached_profile)
    {
        goto cleanup;
    }
    henka_engine_ui_font_resource_update_publish(engine, update);
    update = NULL;
    if (fixture->ui->font_profile != fixture->replacement_profile ||
        fabsf(fixture->ui->output_scale - 1.5f) > 0.0001f ||
        detached_ui->font_profile != fixture->detached_replacement_profile ||
        detached_ui->font_context_identity != detached_identity ||
        detached_ui->font_app_scale_percent != 100U ||
        fabsf(detached_ui->output_scale -
            (float)replacement_scale_numerator / (float)replacement_scale_denominator) > 0.0001f)
    {
        goto cleanup;
    }

    profile_desc.output_scale_numerator = second_replacement_scale_numerator;
    profile_desc.output_scale_denominator = second_replacement_scale_denominator;
    stage = "second detached replacement profile creation";
    if (henka_ui_font_raster_profile_create(
            fixture->font, &profile_desc,
            &fixture->detached_second_replacement_profile, &diagnostic) != HENKA_SUCCESS ||
        fixture->detached_second_replacement_profile == NULL)
    {
        goto cleanup;
    }
    bindings[1U].profile = fixture->detached_second_replacement_profile;
    stage = "second detached replacement prepare remains unpublished";
    if (henka_engine_ui_font_resource_update_prepare(
            engine, bindings, 2U, 100U, &update) != HENKA_SUCCESS || update == NULL ||
        fixture->ui->font_profile != fixture->replacement_profile ||
        fabsf(fixture->ui->output_scale - 1.5f) > 0.0001f ||
        detached_ui->font_profile != fixture->detached_replacement_profile ||
        fabsf(detached_ui->output_scale -
            (float)replacement_scale_numerator / (float)replacement_scale_denominator) > 0.0001f)
    {
        goto cleanup;
    }
    henka_engine_ui_font_resource_update_publish(engine, update);
    update = NULL;
    if (fixture->ui->font_profile != fixture->replacement_profile ||
        fabsf(fixture->ui->output_scale - 1.5f) > 0.0001f ||
        detached_ui->font_profile != fixture->detached_second_replacement_profile ||
        detached_ui->font_context_identity != detached_identity ||
        detached_ui->font_app_scale_percent != 100U ||
        fabsf(detached_ui->output_scale -
            (float)second_replacement_scale_numerator /
                (float)second_replacement_scale_denominator) > 0.0001f)
    {
        goto cleanup;
    }

    profile_desc.graphics_context_identity = detached_identity + 1U;
    stage = "detached wrong-context negative control creation";
    if (henka_ui_font_raster_profile_create(
            fixture->font, &profile_desc,
            &fixture->detached_wrong_context_profile, &diagnostic) != HENKA_SUCCESS ||
        fixture->detached_wrong_context_profile == NULL)
    {
        goto cleanup;
    }
    bindings[1U].profile = fixture->detached_wrong_context_profile;
    stage = "detached wrong-context rejection preserves both profiles";
    if (henka_engine_ui_font_resource_update_prepare(
            engine, bindings, 2U, 100U, &update) == HENKA_SUCCESS || update != NULL ||
        fixture->ui->font_profile != fixture->replacement_profile ||
        fabsf(fixture->ui->output_scale - 1.5f) > 0.0001f ||
        detached_ui->font_profile != fixture->detached_second_replacement_profile ||
        detached_ui->font_context_identity != detached_identity ||
        fabsf(detached_ui->output_scale -
            (float)second_replacement_scale_numerator /
                (float)second_replacement_scale_denominator) > 0.0001f)
    {
        goto cleanup;
    }
    passed = true;

cleanup:
    if (!passed)
    {
        fprintf(stderr, "detached display-scale font profile regression failed at %s\n", stage);
    }
    if (update != NULL)
    {
        henka_engine_ui_font_resource_update_discard(engine, update);
    }
    return passed;
}

static bool henka_test_platform_display_scale_matches_content_size(
    int framebuffer_width,
    int framebuffer_height,
    int content_width,
    int content_height,
    uint32_t scale_numerator,
    uint32_t scale_denominator)
{
    const double scale = (double)scale_numerator / (double)scale_denominator;
    const double expected_width = (double)content_width * scale;
    const double expected_height = (double)content_height * scale;

    return framebuffer_width > 0 && framebuffer_height > 0 &&
        content_width > 0 && content_height > 0 &&
        scale_numerator > 0U && scale_denominator > 0U &&
        isfinite(scale) && scale >= 0.25 && scale <= 4.0 &&
        fabs((double)framebuffer_width - expected_width) <= 1.0 &&
        fabs((double)framebuffer_height - expected_height) <= 1.0;
}

static bool henka_test_platform_display_scale_contract(void)
{
    henka_engine_config config;
    henka_engine* engine = NULL;
    henka_ui_context* tool_ui = NULL;
    henka_platform_scale_profile_fixture profile_fixture;
    henka_platform_tool_window_close_fixture close_fixture;
    henka_tool_window_state tool_state;
    henka_tool_window_desc tool_desc;
    SDL_Event close_event;
    henka_window_id tool_window_id = HENKA_INVALID_WINDOW_ID;
    uint32_t display_scale_numerator = 0U;
    uint32_t display_scale_denominator = 0U;
    int content_width = 0;
    int content_height = 0;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    bool tool_window_open = false;
    bool passed = false;

    memset(&profile_fixture, 0, sizeof(profile_fixture));
    memset(&close_fixture, 0, sizeof(close_fixture));
    memset(&config, 0, sizeof(config));
    config.application_name = "Henka display scale contract";
    config.window_width = 640;
    config.window_height = 360;
    config.window_start_hidden = true;
    config.enable_vsync = false;
    config.asset_base_path = ".";
    config.on_update = henka_test_platform_tool_window_close_update;
    config.on_close_requested = henka_test_platform_main_close_requested;
    config.user_data = &close_fixture;
    if (!SDL_SetHintWithPriority(
            SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE,
            "1",
            SDL_HINT_OVERRIDE) ||
        henka_engine_create(&config, &engine) != HENKA_SUCCESS || engine == NULL ||
        SDL_GetHintBoolean(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, true) ||
        henka_engine_get_window_display_scale(
            engine,
            &display_scale_numerator,
            &display_scale_denominator) != HENKA_SUCCESS ||
        henka_engine_get_window_content_size(
            engine, &content_width, &content_height) != HENKA_SUCCESS ||
        henka_engine_get_framebuffer_size(
            engine, &framebuffer_width, &framebuffer_height) != HENKA_SUCCESS ||
        !henka_test_platform_display_scale_matches_content_size(
            framebuffer_width,
            framebuffer_height,
            content_width,
            content_height,
            display_scale_numerator,
            display_scale_denominator))
    {
        goto cleanup;
    }

    if (!henka_test_platform_scale_profile_replacement(engine, &profile_fixture))
    {
        goto cleanup;
    }

    if (henka_ui_create(&tool_ui) != HENKA_SUCCESS || tool_ui == NULL)
    {
        goto cleanup;
    }
    memset(&tool_desc, 0, sizeof(tool_desc));
    tool_desc.title = "Henka display scale tool contract";
    tool_desc.width = 480;
    tool_desc.height = 270;
    tool_desc.minimum_width = 240;
    tool_desc.minimum_height = 160;
    if (henka_engine_open_tool_window(
            engine, &tool_desc, tool_ui, &tool_window_id) != HENKA_SUCCESS ||
        tool_window_id == HENKA_INVALID_WINDOW_ID)
    {
        goto cleanup;
    }
    tool_window_open = true;
    if (henka_engine_get_tool_window_state(
            engine, tool_window_id, &tool_state) != HENKA_SUCCESS ||
        !tool_state.open ||
        !henka_test_platform_display_scale_matches_content_size(
            tool_state.width,
            tool_state.height,
            tool_state.logical_width,
            tool_state.logical_height,
            tool_state.display_scale_numerator,
            tool_state.display_scale_denominator))
    {
        goto cleanup;
    }

    if (!henka_test_platform_display_scale_context_replacement_reuses_and_retires_profile(
            engine,
            tool_ui,
            tool_state.display_scale_numerator,
            tool_state.display_scale_denominator,
            &profile_fixture))
    {
        goto cleanup;
    }

    close_fixture.window_id = tool_window_id;
    close_fixture.native_window_id = tool_state.native_window_id;
    close_fixture.observe_close_request = true;
    memset(&close_event, 0, sizeof(close_event));
    close_event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    close_event.window.windowID = tool_state.native_window_id;
    if (!SDL_PushEvent(&close_event))
    {
        goto cleanup;
    }
    memset(&close_event, 0, sizeof(close_event));
    close_event.type = SDL_EVENT_QUIT;
    if (!SDL_PushEvent(&close_event) ||
        henka_engine_run(engine) != HENKA_SUCCESS ||
        !close_fixture.update_called ||
        close_fixture.state_result != HENKA_SUCCESS ||
        !close_fixture.close_request_observed ||
        close_fixture.close_result != HENKA_SUCCESS ||
        !close_fixture.main_close_request_seen ||
        !close_fixture.update_after_deferred_main_close ||
        !close_fixture.owner_closed_window)
    {
        goto cleanup;
    }
    tool_window_open = false;

    passed = true;

cleanup:
    if (tool_window_open)
    {
        (void)henka_engine_close_tool_window(engine, tool_window_id);
    }
    if (engine != NULL)
    {
        (void)henka_engine_set_ui_context(engine, NULL);
        henka_engine_destroy(engine);
        engine = NULL;
    }
    (void)SDL_ResetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE);
    henka_ui_destroy(profile_fixture.ui);
    henka_ui_destroy(tool_ui);
    henka_ui_font_raster_profile_destroy(profile_fixture.wrong_context_profile);
    henka_ui_font_raster_profile_destroy(profile_fixture.detached_wrong_context_profile);
    henka_ui_font_raster_profile_destroy(profile_fixture.detached_second_replacement_profile);
    henka_ui_font_raster_profile_destroy(profile_fixture.detached_replacement_profile);
    henka_ui_font_raster_profile_destroy(profile_fixture.detached_profile);
    henka_ui_font_raster_profile_destroy(profile_fixture.replacement_profile);
    henka_ui_font_raster_profile_destroy(profile_fixture.first_profile);
    henka_ui_font_resource_destroy(profile_fixture.font);
    free(profile_fixture.font_bytes);
    return passed;
}

void henka_test_platform_display_scale(void)
{
    HENKA_TEST_ASSERT(henka_test_platform_display_scale_contract());
}
