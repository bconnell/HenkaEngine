#include "test_suite.h"

#include <henka/engine.h>
#include <henka/ui.h>
#include <henka/ui_font.h>

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
    uint8_t* font_bytes;
} henka_platform_scale_profile_fixture;

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
    henka_tool_window_state tool_state;
    henka_tool_window_desc tool_desc;
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
    memset(&config, 0, sizeof(config));
    config.application_name = "Henka display scale contract";
    config.window_width = 640;
    config.window_height = 360;
    config.window_start_hidden = true;
    config.enable_vsync = false;
    config.asset_base_path = ".";
    if (henka_engine_create(&config, &engine) != HENKA_SUCCESS || engine == NULL ||
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

    passed = true;

cleanup:
    if (tool_window_open)
    {
        (void)henka_engine_close_tool_window(engine, tool_window_id);
    }
    if (engine != NULL)
    {
        (void)henka_engine_set_ui_context(engine, NULL);
    }
    henka_ui_destroy(profile_fixture.ui);
    henka_ui_destroy(tool_ui);
    henka_ui_font_raster_profile_destroy(profile_fixture.wrong_context_profile);
    henka_ui_font_raster_profile_destroy(profile_fixture.replacement_profile);
    henka_ui_font_raster_profile_destroy(profile_fixture.first_profile);
    henka_ui_font_resource_destroy(profile_fixture.font);
    free(profile_fixture.font_bytes);
    henka_engine_destroy(engine);
    return passed;
}

void henka_test_platform_display_scale(void)
{
    HENKA_TEST_ASSERT(henka_test_platform_display_scale_contract());
}
