#include "test_suite.h"

#include <float.h>
#include <henka/memory.h>

#include "../examples/sandbox3d/editor_font_bootstrap.h"
#include "../examples/sandbox3d/editor_text_layout.h"
#include "../engine/src/core/memory_internal.h"
#include "../engine/src/platform/window_scale.h"
#include "../engine/src/ui/ui_font_internal.h"
#include "../engine/src/ui/ui_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HENKA_UI_FONT_TEST_FIXTURE "tests/fixtures/fonts/AtkinsonHyperlegible-Regular.ttf"
#define HENKA_UI_FONT_TEST_SHA256 "7FB917C89019896D0B52EE84B7CBB3304C18CB90B19A62F5E32712BD23E97669"
#define HENKA_UI_FONT_TEST_CODEPOINT_COUNT 96U

typedef struct henka_ui_font_test_fixture
{
    uint8_t* bytes;
    size_t size;
    uint32_t codepoints[HENKA_UI_FONT_TEST_CODEPOINT_COUNT];
} henka_ui_font_test_fixture;

static bool henka_ui_font_test_fixture_load(henka_ui_font_test_fixture* fixture)
{
    FILE* file;
    long file_size;
    size_t index;

    if (fixture == NULL)
    {
        return false;
    }
    memset(fixture, 0, sizeof(*fixture));
#if defined(_MSC_VER)
    if (fopen_s(&file, HENKA_UI_FONT_TEST_FIXTURE, "rb") != 0)
    {
        return false;
    }
#else
    file = fopen(HENKA_UI_FONT_TEST_FIXTURE, "rb");
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
    fixture->size = (size_t)file_size;
    fixture->bytes = (uint8_t*)malloc(fixture->size);
    if (fixture->bytes == NULL || fread(fixture->bytes, 1U, fixture->size, file) != fixture->size)
    {
        free(fixture->bytes);
        fixture->bytes = NULL;
        fixture->size = 0U;
        (void)fclose(file);
        return false;
    }
    (void)fclose(file);
    for (index = 0U; index < 95U; ++index)
    {
        fixture->codepoints[index] = (uint32_t)(32U + index);
    }
    fixture->codepoints[95U] = 0x2026U;
    return true;
}

static void henka_ui_font_test_fixture_dispose(henka_ui_font_test_fixture* fixture)
{
    if (fixture != NULL)
    {
        free(fixture->bytes);
        memset(fixture, 0, sizeof(*fixture));
    }
}

static henka_ui_font_desc henka_ui_font_test_desc(
    const henka_ui_font_test_fixture* fixture)
{
    henka_ui_font_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.source_bytes = fixture->bytes;
    desc.source_size = fixture->size;
    desc.identity = "test/AtkinsonHyperlegible-Regular.ttf";
    desc.verified_sha256_hex = HENKA_UI_FONT_TEST_SHA256;
    desc.required_codepoints = fixture->codepoints;
    desc.required_codepoint_count = HENKA_UI_FONT_TEST_CODEPOINT_COUNT;
    desc.raster_pixel_height = 20U;
    desc.atlas_width = 512U;
    desc.atlas_height = 512U;
    return desc;
}

static henka_ui_font_raster_profile_desc henka_ui_font_test_profile_desc(
    uint64_t context_identity,
    uint32_t output_numerator,
    uint32_t output_denominator)
{
    henka_ui_font_raster_profile_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.graphics_context_identity = context_identity;
    desc.output_scale_numerator = output_numerator;
    desc.output_scale_denominator = output_denominator;
    desc.atlas_width = 1024U;
    desc.atlas_height = 1024U;
    return desc;
}

static bool henka_ui_font_test_create_profile(
    const henka_ui_font_test_fixture* fixture,
    uint64_t context_identity,
    uint32_t output_numerator,
    uint32_t output_denominator,
    henka_ui_font_resource** out_font,
    henka_ui_font_raster_profile** out_profile)
{
    henka_ui_font_desc font_desc;
    henka_ui_font_raster_profile_desc profile_desc;
    henka_ui_font_diagnostic diagnostic;

    if (out_font == NULL || out_profile == NULL)
    {
        return false;
    }
    *out_font = NULL;
    *out_profile = NULL;
    font_desc = henka_ui_font_test_desc(fixture);
    font_desc.atlas_width = 1024U;
    font_desc.atlas_height = 1024U;
    if (henka_ui_font_resource_create(&font_desc, out_font, &diagnostic) != HENKA_SUCCESS)
    {
        return false;
    }
    profile_desc = henka_ui_font_test_profile_desc(
        context_identity, output_numerator, output_denominator);
    if (henka_ui_font_raster_profile_create(
            *out_font, &profile_desc, out_profile, &diagnostic) != HENKA_SUCCESS)
    {
        henka_ui_font_resource_destroy(*out_font);
        *out_font = NULL;
        return false;
    }
    return true;
}

static void henka_test_ui_font_sha256_and_static_font_resource(void)
{
    static const char empty_sha256[] =
        "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855";
    henka_ui_font_test_fixture fixture;
    henka_ui_font_desc desc;
    henka_ui_font_resource* resource = NULL;
    henka_ui_font_diagnostic diagnostic;
    henka_ui_font_info info;
    henka_ui_font_glyph_info glyph;
    char actual_sha256[65U];
    uint32_t atlas_width = 0U;
    uint32_t atlas_height = 0U;

    HENKA_TEST_ASSERT(henka_ui_font_sha256_hex(NULL, 0U, actual_sha256));
    HENKA_TEST_ASSERT(strcmp(actual_sha256, empty_sha256) == 0);
    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    HENKA_TEST_ASSERT(henka_ui_font_sha256_hex(fixture.bytes, fixture.size, actual_sha256));
    HENKA_TEST_ASSERT(strcmp(actual_sha256, HENKA_UI_FONT_TEST_SHA256) == 0);

    desc = henka_ui_font_test_desc(&fixture);
    memset(&diagnostic, 0, sizeof(diagnostic));
    HENKA_TEST_ASSERT(henka_ui_font_resource_create(&desc, &resource, &diagnostic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(resource != NULL);
    HENKA_TEST_ASSERT(diagnostic.stage == HENKA_UI_FONT_STAGE_NONE);
    HENKA_TEST_ASSERT(henka_ui_font_resource_get_info(resource, &info));
    HENKA_TEST_ASSERT(strcmp(info.identity, desc.identity) == 0);
    HENKA_TEST_ASSERT(strcmp(info.sha256_hex, HENKA_UI_FONT_TEST_SHA256) == 0);
    HENKA_TEST_ASSERT(info.glyph_count == HENKA_UI_FONT_TEST_CODEPOINT_COUNT);
    HENKA_TEST_ASSERT(info.raster_pixel_height == 20U);
    HENKA_TEST_ASSERT(info.atlas_width == 512U && info.atlas_height == 512U);
    HENKA_TEST_ASSERT(info.ascent > 0.0f && info.descent < 0.0f && info.line_height > 0.0f);
    HENKA_TEST_ASSERT(henka_ui_font_resource_get_glyph(resource, '?', &glyph));
    HENKA_TEST_ASSERT(glyph.glyph_index != 0U && glyph.advance_x > 0.0f);
    HENKA_TEST_ASSERT(henka_ui_font_resource_get_atlas(resource, &atlas_width, &atlas_height) != NULL);
    HENKA_TEST_ASSERT(atlas_width == 512U && atlas_height == 512U);
    henka_ui_font_resource_destroy(resource);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_font_invalid_identity_and_source_are_not_published(void)
{
    static const uint8_t invalid_font[] = {0x00U, 0x01U, 0x02U, 0x03U};
    henka_ui_font_test_fixture fixture;
    henka_ui_font_desc desc;
    henka_ui_font_resource* resource = NULL;
    henka_ui_font_diagnostic diagnostic;
    char invalid_sha256[65U];
    uint8_t* corrupted;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    desc = henka_ui_font_test_desc(&fixture);
    HENKA_TEST_ASSERT(henka_ui_font_sha256_hex(invalid_font, sizeof(invalid_font), invalid_sha256));
    desc.source_bytes = invalid_font;
    desc.source_size = sizeof(invalid_font);
    desc.verified_sha256_hex = invalid_sha256;
    memset(&diagnostic, 0, sizeof(diagnostic));
    HENKA_TEST_ASSERT(henka_ui_font_resource_create(&desc, &resource, &diagnostic) == HENKA_ERROR_ASSET_SOURCE);
    HENKA_TEST_ASSERT(resource == NULL && diagnostic.stage == HENKA_UI_FONT_STAGE_PARSE);
    HENKA_TEST_ASSERT(strcmp(diagnostic.identity, desc.identity) == 0 && diagnostic.message[0] != '\0');

    desc = henka_ui_font_test_desc(&fixture);
    corrupted = (uint8_t*)malloc(fixture.size);
    HENKA_TEST_ASSERT(corrupted != NULL);
    memcpy(corrupted, fixture.bytes, fixture.size);
    corrupted[fixture.size - 1U] ^= 0x01U;
    {
        sandbox3d_editor_font_asset_desc asset;
        memset(&asset, 0, sizeof(asset));
        asset.bytes = corrupted;
        asset.byte_count = fixture.size;
        asset.asset_identity = desc.identity;
        asset.expected_sha256_hex = HENKA_UI_FONT_TEST_SHA256;
        asset.required_codepoints = fixture.codepoints;
        asset.required_codepoint_count = HENKA_UI_FONT_TEST_CODEPOINT_COUNT;
        asset.raster_pixel_height = desc.raster_pixel_height;
        asset.atlas_width = desc.atlas_width;
        asset.atlas_height = desc.atlas_height;
        memset(&diagnostic, 0, sizeof(diagnostic));
        HENKA_TEST_ASSERT(sandbox3d_editor_font_bootstrap_create(
            &asset, &resource, &diagnostic) == HENKA_ERROR_ASSET_SOURCE);
    }
    HENKA_TEST_ASSERT(resource == NULL && diagnostic.stage == HENKA_UI_FONT_STAGE_HASH);
    HENKA_TEST_ASSERT(strcmp(diagnostic.identity, desc.identity) == 0);
    free(corrupted);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_font_resource_rejects_variable_font_table(void)
{
    henka_ui_font_test_fixture fixture;
    henka_ui_font_desc desc;
    henka_ui_font_resource* resource = NULL;
    henka_ui_font_diagnostic diagnostic;
    uint8_t* variable_marked_bytes;
    uint32_t table_count;
    size_t index;
    bool marked = false;
    char digest[65U];

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    variable_marked_bytes = (uint8_t*)malloc(fixture.size);
    HENKA_TEST_ASSERT(variable_marked_bytes != NULL);
    memcpy(variable_marked_bytes, fixture.bytes, fixture.size);
    table_count = ((uint32_t)variable_marked_bytes[4U] << 8U) |
        (uint32_t)variable_marked_bytes[5U];
    for (index = 0U; index < (size_t)table_count; ++index)
    {
        const size_t tag_offset = 12U + index * 16U;
        if (memcmp(&variable_marked_bytes[tag_offset], "name", 4U) == 0)
        {
            memcpy(&variable_marked_bytes[tag_offset], "fvar", 4U);
            marked = true;
            break;
        }
    }
    HENKA_TEST_ASSERT(marked);
    HENKA_TEST_ASSERT(henka_ui_font_sha256_hex(variable_marked_bytes, fixture.size, digest));
    desc = henka_ui_font_test_desc(&fixture);
    desc.source_bytes = variable_marked_bytes;
    desc.verified_sha256_hex = digest;

    memset(&diagnostic, 0, sizeof(diagnostic));
    HENKA_TEST_ASSERT(henka_ui_font_resource_create(
        &desc, &resource, &diagnostic) == HENKA_ERROR_ASSET_SOURCE);
    HENKA_TEST_ASSERT(resource == NULL && diagnostic.stage == HENKA_UI_FONT_STAGE_PARSE);
    HENKA_TEST_ASSERT(diagnostic.message[0] != '\0');
    free(variable_marked_bytes);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_font_bounds_and_packing_fail_closed(void)
{
    henka_ui_font_test_fixture fixture;
    henka_ui_font_desc desc;
    henka_ui_font_resource* resource = NULL;
    henka_ui_font_diagnostic diagnostic;
    uint32_t too_many_codepoints[HENKA_UI_FONT_MAX_GLYPHS + 1U];
    size_t index;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    desc = henka_ui_font_test_desc(&fixture);
    desc.source_size = HENKA_UI_FONT_MAX_SOURCE_BYTES + 1U;
    memset(&diagnostic, 0, sizeof(diagnostic));
    HENKA_TEST_ASSERT(henka_ui_font_resource_create(&desc, &resource, &diagnostic) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(resource == NULL && diagnostic.stage == HENKA_UI_FONT_STAGE_SOURCE_LIMIT);

    desc = henka_ui_font_test_desc(&fixture);
    for (index = 0U; index < HENKA_UI_FONT_MAX_GLYPHS + 1U; ++index)
    {
        too_many_codepoints[index] = (uint32_t)(0x1000U + index);
    }
    desc.required_codepoints = too_many_codepoints;
    desc.required_codepoint_count = HENKA_UI_FONT_MAX_GLYPHS + 1U;
    HENKA_TEST_ASSERT(henka_ui_font_resource_create(&desc, &resource, &diagnostic) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(resource == NULL && diagnostic.stage == HENKA_UI_FONT_STAGE_GLYPH_SET);

    desc = henka_ui_font_test_desc(&fixture);
    desc.atlas_width = HENKA_UI_FONT_MAX_ATLAS_DIMENSION + 1U;
    HENKA_TEST_ASSERT(henka_ui_font_resource_create(&desc, &resource, &diagnostic) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(resource == NULL && diagnostic.stage == HENKA_UI_FONT_STAGE_ATLAS_DIMENSIONS);

    desc = henka_ui_font_test_desc(&fixture);
    desc.atlas_width = 8U;
    desc.atlas_height = 8U;
    HENKA_TEST_ASSERT(henka_ui_font_resource_create(&desc, &resource, &diagnostic) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(resource == NULL && diagnostic.stage == HENKA_UI_FONT_STAGE_ATLAS_PACK);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_font_text_layout_uses_shared_glyph_placements(void)
{
    static const char measurement_text[] = "AVATAR I/l/1 O/0: 123.45";
    static const char malformed_and_unsupported[] = "A\xf0(\x8c(\xf0\x9f\x98\x80" "B";
    static const char long_text[] = "HENKA TYPOGRAPHY LAYOUT TRUNCATION";
    henka_ui_font_test_fixture fixture;
    henka_ui_font_desc desc;
    henka_ui_font_resource* resource = NULL;
    henka_ui_font_diagnostic diagnostic;
    henka_ui_text_run run;
    float emitted_right = 0.0f;
    size_t index;
    bool saw_replacement = false;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    desc = henka_ui_font_test_desc(&fixture);
    HENKA_TEST_ASSERT(henka_ui_font_resource_create(&desc, &resource, &diagnostic) == HENKA_SUCCESS);

    memset(&run, 0, sizeof(run));
    HENKA_TEST_ASSERT(henka_ui_text_layout_build(
        resource,
        measurement_text,
        sizeof(measurement_text) - 1U,
        0.0f,
        &run,
        &diagnostic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!run.truncated && run.replacement_count == 0U);
    HENKA_TEST_ASSERT(run.glyph_count == sizeof(measurement_text) - 1U);
    HENKA_TEST_ASSERT(run.ascent > 0.0f && run.descent < 0.0f && run.line_height > 0.0f);
    for (index = 0U; index < run.glyph_count; ++index)
    {
        const henka_ui_text_glyph* glyph = &run.glyphs[index];
        const float glyph_right = glyph->pen_x + glyph->advance_x;
        if (glyph_right > emitted_right)
        {
            emitted_right = glyph_right;
        }
    }
    HENKA_TEST_ASSERT_FLOAT_CLOSE(run.advance_width, emitted_right, 0.0001f);
    HENKA_TEST_ASSERT(run.ink_right >= run.ink_left && run.ink_bottom >= run.ink_top);
    henka_ui_text_run_dispose(&run);

    HENKA_TEST_ASSERT(henka_ui_text_layout_build(
        resource,
        malformed_and_unsupported,
        sizeof(malformed_and_unsupported) - 1U,
        0.0f,
        &run,
        &diagnostic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(run.replacement_count == 3U);
    for (index = 0U; index < run.glyph_count; ++index)
    {
        if (run.glyphs[index].source_codepoint != run.glyphs[index].rendered_codepoint)
        {
            saw_replacement = true;
            HENKA_TEST_ASSERT(run.glyphs[index].rendered_codepoint == '?');
        }
    }
    HENKA_TEST_ASSERT(saw_replacement);
    henka_ui_text_run_dispose(&run);

    HENKA_TEST_ASSERT(henka_ui_text_layout_build(
        resource,
        long_text,
        sizeof(long_text) - 1U,
        34.0f,
        &run,
        &diagnostic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(run.truncated);
    HENKA_TEST_ASSERT(run.glyph_count > 0U && run.glyphs[run.glyph_count - 1U].rendered_codepoint == 0x2026U);
    HENKA_TEST_ASSERT(run.advance_width <= 34.0f);
    henka_ui_text_run_dispose(&run);

    HENKA_TEST_ASSERT(henka_ui_text_layout_build(
        resource,
        measurement_text,
        HENKA_UI_TEXT_MAX_BYTES + 1U,
        0.0f,
        &run,
        &diagnostic) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(diagnostic.stage == HENKA_UI_FONT_STAGE_TEXT);
    henka_ui_font_resource_destroy(resource);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_segmented_select_fits_compact_labels_at_200_percent(void)
{
    static const char* const labels[] = {"100", "125", "150", "200"};
    henka_ui_font_test_fixture fixture;
    henka_ui_font_resource* font = NULL;
    henka_ui_font_raster_profile* profile = NULL;
    henka_ui_context* ui = NULL;
    henka_ui_frame_desc frame_desc;
    size_t selected_index = 0U;
    size_t glyph_quad_count = 0U;
    size_t index;
    bool changed = true;
    bool labels_complete;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    HENKA_TEST_ASSERT(henka_ui_font_test_create_profile(
        &fixture, 0x520U, 1U, 1U, &font, &profile));
    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_ui_context_set_font_profile(ui, profile, 200U) == HENKA_SUCCESS);
    henka_ui_set_visible(ui, true);

    memset(&frame_desc, 0, sizeof(frame_desc));
    frame_desc.framebuffer_width = 1280;
    frame_desc.framebuffer_height = 720;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_segmented_select(
        ui,
        "compact.scale",
        (henka_ui_rect){0.0f, 0.0f, 129.0f, 24.0f},
        labels,
        sizeof(labels) / sizeof(labels[0]),
        &selected_index,
        &changed) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(selected_index == 0U && !changed);
    for (index = 0U; index < ui->draw_quad_count; ++index)
    {
        if (ui->draw_quads[index].kind == HENKA_UI_DRAW_QUAD_GLYPH)
        {
            ++glyph_quad_count;
        }
    }
    /* Four three-digit labels must remain whole at 32.25 logical pixels each;
     * ellipsis glyphs would make this count smaller. */
    labels_complete = glyph_quad_count == 12U;
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    henka_ui_destroy(ui);
    henka_ui_font_raster_profile_destroy(profile);
    henka_ui_font_resource_destroy(font);
    henka_ui_font_test_fixture_dispose(&fixture);
    HENKA_TEST_ASSERT(labels_complete);
}

static void henka_test_ui_font_allocation_failures_are_atomic(void)
{
    henka_ui_font_test_fixture fixture;
    henka_ui_font_desc desc;
    henka_ui_font_resource* resource;
    henka_ui_font_diagnostic diagnostic;
    const size_t allocations_before = henka_memory_get_allocation_count();
    bool reached_success = false;
    size_t failure_after;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    desc = henka_ui_font_test_desc(&fixture);
    for (failure_after = 0U; failure_after < 16U; ++failure_after)
    {
        resource = NULL;
        henka_memory_test_fail_after(failure_after);
        {
            const henka_result result = henka_ui_font_resource_create(&desc, &resource, &diagnostic);
            henka_memory_test_disable_failures();
            if (result == HENKA_SUCCESS)
            {
                HENKA_TEST_ASSERT(resource != NULL);
                henka_ui_font_resource_destroy(resource);
                reached_success = true;
                break;
            }
            HENKA_TEST_ASSERT(result == HENKA_ERROR_OUT_OF_MEMORY);
        }
        HENKA_TEST_ASSERT(resource == NULL);
        HENKA_TEST_ASSERT(diagnostic.stage == HENKA_UI_FONT_STAGE_ALLOCATION);
        HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == allocations_before);
    }
    HENKA_TEST_ASSERT(reached_success);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == allocations_before);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_font_application_bootstrap_verifies_asset_identity(void)
{
    henka_ui_font_test_fixture fixture;
    sandbox3d_editor_font_asset_desc asset;
    henka_ui_font_resource* resource = NULL;
    henka_ui_font_diagnostic diagnostic;
    henka_ui_font_info info;
    size_t allocations_before;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    memset(&asset, 0, sizeof(asset));
    asset.bytes = fixture.bytes;
    asset.byte_count = fixture.size;
    asset.asset_identity = "builtin/ui/test/AtkinsonHyperlegible-Regular.ttf";
    asset.expected_sha256_hex = "0000000000000000000000000000000000000000000000000000000000000000";
    asset.required_codepoints = fixture.codepoints;
    asset.required_codepoint_count = HENKA_UI_FONT_TEST_CODEPOINT_COUNT;
    asset.raster_pixel_height = 20U;
    asset.atlas_width = 512U;
    asset.atlas_height = 512U;
    allocations_before = henka_memory_get_allocation_count();

    memset(&diagnostic, 0, sizeof(diagnostic));
    HENKA_TEST_ASSERT(sandbox3d_editor_font_bootstrap_create(
        &asset, &resource, &diagnostic) == HENKA_ERROR_ASSET_SOURCE);
    HENKA_TEST_ASSERT(resource == NULL);
    HENKA_TEST_ASSERT(diagnostic.stage == HENKA_UI_FONT_STAGE_HASH);
    HENKA_TEST_ASSERT(strcmp(diagnostic.identity, asset.asset_identity) == 0);
    HENKA_TEST_ASSERT(diagnostic.message[0] != '\0');
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == allocations_before);

    asset.byte_count = HENKA_UI_FONT_MAX_SOURCE_BYTES + 1U;
    memset(&diagnostic, 0, sizeof(diagnostic));
    HENKA_TEST_ASSERT(sandbox3d_editor_font_bootstrap_create(
        &asset, &resource, &diagnostic) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(resource == NULL && diagnostic.stage == HENKA_UI_FONT_STAGE_SOURCE_LIMIT);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == allocations_before);

    asset.expected_sha256_hex = HENKA_UI_FONT_TEST_SHA256;
    asset.byte_count = fixture.size;
    HENKA_TEST_ASSERT(sandbox3d_editor_font_bootstrap_create(
        &asset, &resource, &diagnostic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(resource != NULL && diagnostic.stage == HENKA_UI_FONT_STAGE_NONE);
    HENKA_TEST_ASSERT(henka_ui_font_resource_get_info(resource, &info));
    HENKA_TEST_ASSERT(strcmp(info.identity, asset.asset_identity) == 0);
    HENKA_TEST_ASSERT(strcmp(info.sha256_hex, HENKA_UI_FONT_TEST_SHA256) == 0);
    henka_ui_font_resource_destroy(resource);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == allocations_before);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_font_application_bundle_loader_is_confined_and_verifies_bytes(void)
{
    static const char invalid_sha256[] =
        "0000000000000000000000000000000000000000000000000000000000000000";
    henka_ui_font_test_fixture fixture;
    sandbox3d_editor_font_file_asset_desc asset;
    henka_ui_font_resource* resource = NULL;
    henka_ui_font_diagnostic diagnostic;
    henka_ui_font_info info;
    size_t allocations_before;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    allocations_before = henka_memory_get_allocation_count();
    memset(&asset, 0, sizeof(asset));
    asset.asset_path = HENKA_UI_FONT_TEST_FIXTURE;
    asset.asset_identity = "test/AtkinsonHyperlegible-Regular.ttf";
    asset.expected_sha256_hex = HENKA_UI_FONT_TEST_SHA256;
    asset.required_codepoints = fixture.codepoints;
    asset.required_codepoint_count = HENKA_UI_FONT_TEST_CODEPOINT_COUNT;
    asset.raster_pixel_height = 20U;
    asset.atlas_width = 1024U;
    asset.atlas_height = 1024U;
    memset(&diagnostic, 0, sizeof(diagnostic));

    HENKA_TEST_ASSERT(sandbox3d_editor_font_bootstrap_load(
        ".", &asset, &resource, &diagnostic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(resource != NULL);
    HENKA_TEST_ASSERT(henka_ui_font_resource_get_info(resource, &info));
    HENKA_TEST_ASSERT(strcmp(info.sha256_hex, HENKA_UI_FONT_TEST_SHA256) == 0);
    henka_ui_font_resource_destroy(resource);
    resource = NULL;
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == allocations_before);

    asset.expected_sha256_hex = invalid_sha256;
    HENKA_TEST_ASSERT(sandbox3d_editor_font_bootstrap_load(
        ".", &asset, &resource, &diagnostic) == HENKA_ERROR_ASSET_SOURCE);
    HENKA_TEST_ASSERT(resource == NULL);
    HENKA_TEST_ASSERT(diagnostic.stage == HENKA_UI_FONT_STAGE_HASH);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == allocations_before);

    asset.expected_sha256_hex = HENKA_UI_FONT_TEST_SHA256;
    asset.asset_path = "tests/fixtures/fonts/missing-font.ttf";
    HENKA_TEST_ASSERT(sandbox3d_editor_font_bootstrap_load(
        ".", &asset, &resource, &diagnostic) == HENKA_ERROR_ASSET_SOURCE);
    HENKA_TEST_ASSERT(resource == NULL);
    HENKA_TEST_ASSERT(diagnostic.stage == HENKA_UI_FONT_STAGE_ASSET_READ);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == allocations_before);

    asset.asset_path = "../outside-font.ttf";
    HENKA_TEST_ASSERT(sandbox3d_editor_font_bootstrap_load(
        ".", &asset, &resource, &diagnostic) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(resource == NULL);
    HENKA_TEST_ASSERT(diagnostic.stage == HENKA_UI_FONT_STAGE_ASSET_READ);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == allocations_before);

    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_atlas_uses_effective_raster_pixel_size(void)
{
    static const uint16_t app_scales[HENKA_UI_FONT_APP_SCALE_COUNT] =
        {100U, 125U, 150U, 200U};
    static const uint32_t expected_pixel_heights[HENKA_UI_FONT_APP_SCALE_COUNT] =
        {30U, 38U, 45U, 60U};
    henka_ui_font_test_fixture fixture;
    henka_ui_font_resource* font = NULL;
    henka_ui_font_raster_profile* profile = NULL;
    size_t index;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    HENKA_TEST_ASSERT(henka_ui_font_test_create_profile(
        &fixture, 0x101U, 3U, 2U, &font, &profile));
    for (index = 0U; index < HENKA_UI_FONT_APP_SCALE_COUNT; ++index)
    {
        henka_ui_font_raster_profile_info profile_info;
        henka_ui_font_info font_info;
        const henka_ui_font_resource* raster_font =
            henka_ui_font_raster_profile_get_font(profile, app_scales[index]);
        uint32_t atlas_width = 0U;
        uint32_t atlas_height = 0U;
        const uint8_t* atlas = henka_ui_font_raster_profile_get_atlas(
            profile, app_scales[index], &atlas_width, &atlas_height);

        HENKA_TEST_ASSERT(raster_font != NULL);
        HENKA_TEST_ASSERT(henka_ui_font_resource_get_info(raster_font, &font_info));
        HENKA_TEST_ASSERT(henka_ui_font_raster_profile_get_info(
            profile, app_scales[index], &profile_info));
        HENKA_TEST_ASSERT(font_info.raster_pixel_height == expected_pixel_heights[index]);
        HENKA_TEST_ASSERT(profile_info.key.raster_pixel_height == expected_pixel_heights[index]);
        HENKA_TEST_ASSERT(profile_info.key.app_scale_percent == app_scales[index]);
        HENKA_TEST_ASSERT(atlas != NULL && atlas_width == 1024U && atlas_height == 1024U);
    }
    {
        henka_ui_font_glyph_info small_glyph;
        henka_ui_font_glyph_info large_glyph;
        const henka_ui_font_resource* small_font =
            henka_ui_font_raster_profile_get_font(profile, 100U);
        const henka_ui_font_resource* large_font =
            henka_ui_font_raster_profile_get_font(profile, 200U);
        HENKA_TEST_ASSERT(henka_ui_font_resource_get_glyph(small_font, (uint32_t)'A', &small_glyph));
        HENKA_TEST_ASSERT(henka_ui_font_resource_get_glyph(large_font, (uint32_t)'A', &large_glyph));
        HENKA_TEST_ASSERT(large_glyph.bitmap_width > small_glyph.bitmap_width);
        HENKA_TEST_ASSERT(large_glyph.bitmap_height > small_glyph.bitmap_height);
    }
    henka_ui_font_raster_profile_destroy(profile);
    henka_ui_font_resource_destroy(font);
    henka_ui_font_test_fixture_dispose(&fixture);
}

typedef enum henka_test_ui_centered_control
{
    HENKA_TEST_UI_CENTERED_BUTTON = 0,
    HENKA_TEST_UI_CENTERED_PRIMARY_BUTTON,
    HENKA_TEST_UI_CENTERED_SELECTABLE,
    HENKA_TEST_UI_CENTERED_TAB,
    HENKA_TEST_UI_CENTERED_SEGMENT,
    HENKA_TEST_UI_CENTERED_TOGGLE,
    HENKA_TEST_UI_CENTERED_DISCLOSURE_ROW,
    HENKA_TEST_UI_CENTERED_CONTROL_COUNT
} henka_test_ui_centered_control;

static void henka_test_ui_assert_centered_control_label(
    henka_ui_context* ui,
    uint16_t app_scale_percent,
    float output_scale,
    henka_ui_frame_desc frame_desc,
    henka_ui_rect bounds,
    henka_test_ui_centered_control control,
    const char* label)
{
    const char* segment_labels[] = {label, "I"};
    const float app_scale_factor = (float)app_scale_percent / 100.0f;
    float glyph_top = FLT_MAX;
    float glyph_bottom = -FLT_MAX;
    float ink_center_error_pixels;
    size_t glyph_count = 0U;
    size_t quad_index;
    henka_result result;
    bool toggle_value = false;
    bool ink_centered;

    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    switch (control)
    {
    case HENKA_TEST_UI_CENTERED_BUTTON:
        (void)henka_ui_button(ui, "font.centered.button", bounds, label);
        break;
    case HENKA_TEST_UI_CENTERED_PRIMARY_BUTTON:
        (void)henka_ui_primary_button(ui, "font.centered.primary", bounds, label);
        break;
    case HENKA_TEST_UI_CENTERED_SELECTABLE:
        (void)henka_ui_selectable(ui, "font.centered.selectable", bounds, label, false);
        break;
    case HENKA_TEST_UI_CENTERED_TAB:
        (void)henka_ui_tab(ui, "font.centered.tab", bounds, label, false);
        break;
    case HENKA_TEST_UI_CENTERED_SEGMENT:
    {
        size_t selected_index = 2U;
        bool changed = false;
        result = henka_ui_segmented_select(
            ui,
            "font.centered.segment",
            bounds,
            segment_labels,
            2U,
            &selected_index,
            &changed);
        HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(selected_index == 2U && !changed);
        break;
    }
    case HENKA_TEST_UI_CENTERED_TOGGLE:
        (void)henka_ui_toggle(
            ui, "font.centered.toggle", bounds, label, &toggle_value);
        HENKA_TEST_ASSERT(!toggle_value);
        break;
    case HENKA_TEST_UI_CENTERED_DISCLOSURE_ROW:
    {
        bool expanded = false;
        bool changed = false;
        result = henka_ui_disclosure_row(
            ui,
            "font.centered.disclosure",
            bounds,
            label,
            &expanded,
            &changed);
        HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(!expanded && !changed);
        break;
    }
    default:
        HENKA_TEST_ASSERT(false);
        break;
    }

    for (quad_index = 0U; quad_index < ui->draw_quad_count; ++quad_index)
    {
        const henka_ui_draw_quad* quad = &ui->draw_quads[quad_index];
        if (quad->kind == HENKA_UI_DRAW_QUAD_GLYPH)
        {
            if (quad->bounds.y < glyph_top)
            {
                glyph_top = quad->bounds.y;
            }
            if (quad->bounds.y + quad->bounds.height > glyph_bottom)
            {
                glyph_bottom = quad->bounds.y + quad->bounds.height;
            }
            ++glyph_count;
        }
    }
    HENKA_TEST_ASSERT(glyph_count ==
        strlen(label) + (control == HENKA_TEST_UI_CENTERED_SEGMENT ? 1U : 0U));
    HENKA_TEST_ASSERT(glyph_top >= bounds.y - 0.01f);
    HENKA_TEST_ASSERT(glyph_bottom <= bounds.y + bounds.height + 0.01f);
    ink_center_error_pixels = fabsf(
        ((glyph_top + glyph_bottom) * 0.5f - (bounds.y + bounds.height * 0.5f)) *
        output_scale * app_scale_factor);
    ink_centered = ink_center_error_pixels <= 1.0f;
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(ink_centered);
}

static void henka_test_ui_control_labels_use_centered_font_metrics(void)
{
    static const char* const labels[] = {"Hg", "menu"};
    static const uint32_t output_numerators[] = {1U, 5U, 3U, 2U};
    static const uint32_t output_denominators[] = {1U, 4U, 2U, 1U};
    static const float output_scales[] = {1.0f, 1.25f, 1.5f, 2.0f};
    static const uint16_t app_scales[] = {100U, 125U, 150U, 200U};
    henka_ui_font_test_fixture fixture;
    size_t output_index;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    for (output_index = 0U;
         output_index < sizeof(output_scales) / sizeof(output_scales[0]);
         ++output_index)
    {
        henka_ui_font_resource* font = NULL;
        henka_ui_font_raster_profile* profile = NULL;
        henka_ui_context* ui = NULL;
        size_t app_index;

        HENKA_TEST_ASSERT(henka_ui_font_test_create_profile(
            &fixture,
            UINT64_C(0x401) + (uint64_t)output_index,
            output_numerators[output_index],
            output_denominators[output_index],
            &font,
            &profile));
        HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
        henka_ui_set_visible(ui, true);

        for (app_index = 0U;
             app_index < sizeof(app_scales) / sizeof(app_scales[0]);
             ++app_index)
        {
            const henka_ui_rect button_bounds = {100.0f, 100.0f, 120.0f, 24.0f};
            henka_ui_frame_desc frame_desc;
            size_t label_index;
            size_t control_index;

            memset(&frame_desc, 0, sizeof(frame_desc));
            frame_desc.framebuffer_width = (int)(1280.0f * output_scales[output_index]);
            frame_desc.framebuffer_height = (int)(720.0f * output_scales[output_index]);
            frame_desc.mouse_position = (henka_vec2){-1.0f, -1.0f};

            HENKA_TEST_ASSERT(henka_ui_context_set_font_profile(
                ui, profile, app_scales[app_index]) == HENKA_SUCCESS);
            for (label_index = 0U;
                 label_index < sizeof(labels) / sizeof(labels[0]);
                 ++label_index)
            {
                for (control_index = 0U;
                     control_index < HENKA_TEST_UI_CENTERED_CONTROL_COUNT;
                     ++control_index)
                {
                    henka_test_ui_assert_centered_control_label(
                        ui,
                        app_scales[app_index],
                        output_scales[output_index],
                        frame_desc,
                        button_bounds,
                        (henka_test_ui_centered_control)control_index,
                        labels[label_index]);
                }
            }
        }

        henka_ui_destroy(ui);
        henka_ui_font_raster_profile_destroy(profile);
        henka_ui_font_resource_destroy(font);
    }
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_output_scale_aligns_draw_and_hit_coordinates(void)
{
    static const uint32_t output_numerators[] = {5U, 3U, 2U};
    static const uint32_t output_denominators[] = {4U, 2U, 1U};
    static const float output_scales[] = {1.25f, 1.5f, 2.0f};
    static const uint16_t app_scales[] = {100U, 125U, 150U, 200U};
    henka_ui_font_test_fixture fixture;
    bool all_hits_aligned = true;
    bool all_draws_aligned = true;
    bool all_output_scales_published = true;
    bool all_logical_mouse_positions_preserved = true;
    bool all_controls_contain_logical_mouse = true;
    bool all_logical_measurements_scale_consistent = true;
    bool failed_transform_was_atomic = true;
    size_t output_index;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    for (output_index = 0U; output_index < 3U; ++output_index)
    {
        henka_ui_font_resource* font = NULL;
        henka_ui_font_raster_profile* profile = NULL;
        henka_ui_context* ui = NULL;
        int reference_measurement_width = -1;
        int reference_measurement_height = -1;
        size_t app_index;

        HENKA_TEST_ASSERT(henka_ui_font_test_create_profile(
            &fixture,
            UINT64_C(0x301) + (uint64_t)output_index,
            output_numerators[output_index],
            output_denominators[output_index],
            &font,
            &profile));
        HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
        henka_ui_set_visible(ui, true);

        for (app_index = 0U; app_index < 4U; ++app_index)
        {
            const float app_scale = (float)app_scales[app_index] / 100.0f;
            const float combined_scale = output_scales[output_index] * app_scale;
            henka_ui_frame_desc frame_desc;
            henka_ui_draw_vertex vertices[256U];
            int measured_width = 0;
            int measured_height = 0;
            size_t vertex_count = 0U;

            memset(&frame_desc, 0, sizeof(frame_desc));
            frame_desc.framebuffer_width = (int)(1280.0f * combined_scale);
            frame_desc.framebuffer_height = (int)(720.0f * combined_scale);
            frame_desc.mouse_position = (henka_vec2){
                50.0f * combined_scale,
                40.0f * combined_scale};
            frame_desc.mouse_left_down = true;

            HENKA_TEST_ASSERT(henka_ui_context_set_font_profile(
                ui, profile, app_scales[app_index]) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(ui->font_app_scale_percent == app_scales[app_index]);
            {
                henka_vec2 logical_mouse = {-1.0f, -1.0f};
                HENKA_TEST_ASSERT(henka_ui_framebuffer_to_logical(
                    ui,
                    (henka_vec2){
                        50.0f * combined_scale,
                        40.0f * combined_scale},
                    &logical_mouse) == HENKA_SUCCESS);
                HENKA_TEST_ASSERT(fabsf(logical_mouse.x - 50.0f) < 0.001f);
                HENKA_TEST_ASSERT(fabsf(logical_mouse.y - 40.0f) < 0.001f);
            }
            if (fabsf(ui->output_scale - output_scales[output_index]) > 0.001f)
            {
                all_output_scales_published = false;
            }
            HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_ui_measure_text_for_context(
                ui,
                "I/l/1 O/0 125%",
                1.0f,
                &measured_width,
                &measured_height) == HENKA_SUCCESS);
            if (app_index == 0U)
            {
                reference_measurement_width = measured_width;
                reference_measurement_height = measured_height;
            }
            else if (abs(measured_width - reference_measurement_width) > 1 ||
                abs(measured_height - reference_measurement_height) > 1)
            {
                all_logical_measurements_scale_consistent = false;
            }
            if (fabsf(ui->mouse_position.x - 50.0f) > 0.001f ||
                fabsf(ui->mouse_position.y - 40.0f) > 0.001f)
            {
                all_logical_mouse_positions_preserved = false;
            }
            if (!henka_ui_rect_contains(
                    (henka_ui_rect){40.0f, 20.0f, 40.0f, 40.0f},
                    ui->mouse_position))
            {
                all_controls_contain_logical_mouse = false;
            }
            HENKA_TEST_ASSERT(henka_ui_overlay_rect(
                ui,
                (henka_ui_rect){10.0f, 20.0f, 30.0f, 40.0f},
                (henka_vec4){0.2f, 0.3f, 0.4f, 1.0f}) == HENKA_SUCCESS);
            (void)henka_ui_button(
                ui,
                "display_scale_hit",
                (henka_ui_rect){40.0f, 20.0f, 40.0f, 40.0f},
                "I");
            if (!henka_ui_get_wants_mouse(ui))
            {
                all_hits_aligned = false;
            }
            HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_ui_context_build_quad_vertices(
                ui, vertices, 256U, &vertex_count) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_ui_context_apply_output_transform(
                ui, vertices, vertex_count) == HENKA_SUCCESS);
            if (vertex_count < 6U ||
                fabsf(vertices[0U].x - 10.0f * combined_scale) > 0.001f ||
                fabsf(vertices[0U].y - 20.0f * combined_scale) > 0.001f ||
                fabsf(vertices[2U].x - 40.0f * combined_scale) > 0.001f ||
                fabsf(vertices[2U].y - 60.0f * combined_scale) > 0.001f)
            {
                all_draws_aligned = false;
            }
        }

        if (output_index == 2U)
        {
            henka_ui_frame_desc frame_desc;
            henka_ui_draw_vertex vertices[2U] = {{0}};
            const float original_first_x = 10.0f;
            const float original_first_y = 20.0f;
            const float original_second_x = FLT_MAX;
            const float original_second_y = 30.0f;

            memset(&frame_desc, 0, sizeof(frame_desc));
            frame_desc.framebuffer_width = 2560;
            frame_desc.framebuffer_height = 1440;
            frame_desc.mouse_position = (henka_vec2){40.0f, 40.0f};
            HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
            vertices[0U].x = original_first_x;
            vertices[0U].y = original_first_y;
            vertices[1U].x = original_second_x;
            vertices[1U].y = original_second_y;
            if (henka_ui_context_apply_output_transform(ui, vertices, 2U) !=
                    HENKA_ERROR_NUMERIC_RANGE ||
                vertices[0U].x != original_first_x ||
                vertices[0U].y != original_first_y ||
                vertices[1U].x != original_second_x ||
                vertices[1U].y != original_second_y)
            {
                failed_transform_was_atomic = false;
            }
        }

        henka_ui_destroy(ui);
        henka_ui_font_raster_profile_destroy(profile);
        henka_ui_font_resource_destroy(font);
    }
    henka_ui_font_test_fixture_dispose(&fixture);

    HENKA_TEST_ASSERT(all_output_scales_published);
    HENKA_TEST_ASSERT(all_logical_mouse_positions_preserved);
    HENKA_TEST_ASSERT(all_controls_contain_logical_mouse);
    HENKA_TEST_ASSERT(all_hits_aligned);
    HENKA_TEST_ASSERT(all_draws_aligned);
    HENKA_TEST_ASSERT(all_logical_measurements_scale_consistent);
    HENKA_TEST_ASSERT(failed_transform_was_atomic);
}

static void henka_test_ui_atlas_identity_includes_font_scale_output_and_context(void)
{
    henka_ui_font_test_fixture fixture;
    henka_ui_font_desc font_desc;
    henka_ui_font_resource* font = NULL;
    henka_ui_font_raster_profile* profile_a = NULL;
    henka_ui_font_raster_profile* profile_b = NULL;
    henka_ui_font_raster_profile* profile_c = NULL;
    henka_ui_font_raster_profile_desc desc_a;
    henka_ui_font_raster_profile_desc desc_b;
    henka_ui_font_raster_profile_desc desc_c;
    henka_ui_font_diagnostic diagnostic;
    henka_ui_font_atlas_key key_100;
    henka_ui_font_atlas_key key_125;
    henka_ui_font_atlas_key key_other_context;
    henka_ui_font_atlas_key key_other_output;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    font_desc = henka_ui_font_test_desc(&fixture);
    font_desc.atlas_width = 1024U;
    font_desc.atlas_height = 1024U;
    HENKA_TEST_ASSERT(henka_ui_font_resource_create(&font_desc, &font, &diagnostic) == HENKA_SUCCESS);
    desc_a = henka_ui_font_test_profile_desc(0x201U, 1U, 1U);
    desc_b = henka_ui_font_test_profile_desc(0x202U, 1U, 1U);
    desc_c = henka_ui_font_test_profile_desc(0x201U, 3U, 2U);
    HENKA_TEST_ASSERT(henka_ui_font_raster_profile_create(font, &desc_a, &profile_a, &diagnostic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_font_raster_profile_create(font, &desc_b, &profile_b, &diagnostic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_font_raster_profile_create(font, &desc_c, &profile_c, &diagnostic) == HENKA_SUCCESS);
    {
        henka_ui_font_raster_profile_info info;
        HENKA_TEST_ASSERT(henka_ui_font_raster_profile_get_info(profile_a, 100U, &info));
        key_100 = info.key;
        HENKA_TEST_ASSERT(henka_ui_font_raster_profile_get_info(profile_a, 125U, &info));
        key_125 = info.key;
        HENKA_TEST_ASSERT(henka_ui_font_raster_profile_get_info(profile_b, 100U, &info));
        key_other_context = info.key;
        HENKA_TEST_ASSERT(henka_ui_font_raster_profile_get_info(profile_c, 100U, &info));
        key_other_output = info.key;
    }
    HENKA_TEST_ASSERT(strcmp(key_100.font_sha256_hex, HENKA_UI_FONT_TEST_SHA256) == 0);
    HENKA_TEST_ASSERT(key_100.app_scale_percent == 100U && key_125.app_scale_percent == 125U);
    HENKA_TEST_ASSERT(key_100.raster_pixel_height != key_125.raster_pixel_height);
    HENKA_TEST_ASSERT(key_100.graphics_context_identity == 0x201U);
    HENKA_TEST_ASSERT(key_other_context.graphics_context_identity == 0x202U);
    HENKA_TEST_ASSERT(key_100.output_scale_numerator == 1U);
    HENKA_TEST_ASSERT(key_other_output.output_scale_numerator == 3U);
    HENKA_TEST_ASSERT(key_other_output.output_scale_denominator == 2U);
    henka_ui_font_raster_profile_destroy(profile_c);
    henka_ui_font_raster_profile_destroy(profile_b);
    henka_ui_font_raster_profile_destroy(profile_a);
    henka_ui_font_resource_destroy(font);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_atlas_cache_is_bounded_per_context(void)
{
    henka_ui_font_test_fixture fixture;
    henka_ui_font_resource* font = NULL;
    henka_ui_font_raster_profile* profile = NULL;
    size_t index;
    size_t total_coverage = 0U;

    HENKA_TEST_ASSERT(HENKA_UI_FONT_MAX_CONTEXTS == 6U);
    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    HENKA_TEST_ASSERT(henka_ui_font_test_create_profile(
        &fixture, 0x301U, 1U, 1U, &font, &profile));
    HENKA_TEST_ASSERT(henka_ui_font_raster_profile_get_entry_count(profile) ==
        HENKA_UI_FONT_APP_SCALE_COUNT);
    for (index = 0U; index < HENKA_UI_FONT_APP_SCALE_COUNT; ++index)
    {
        static const uint16_t scales[HENKA_UI_FONT_APP_SCALE_COUNT] =
            {100U, 125U, 150U, 200U};
        henka_ui_font_raster_profile_info info;
        HENKA_TEST_ASSERT(henka_ui_font_raster_profile_get_info(
            profile, scales[index], &info));
        HENKA_TEST_ASSERT(info.coverage_bytes == (size_t)1024U * 1024U);
        total_coverage += info.coverage_bytes;
    }
    HENKA_TEST_ASSERT(total_coverage <= (size_t)16U * 1024U * 1024U);
    {
        henka_ui_font_raster_profile_info unsupported;
        HENKA_TEST_ASSERT(!henka_ui_font_raster_profile_get_info(profile, 110U, &unsupported));
    }
    henka_ui_font_raster_profile_destroy(profile);
    henka_ui_font_resource_destroy(font);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_context_profile_replacement_is_atomic(void)
{
    henka_ui_frame_desc frame_desc;
    henka_ui_context* ui = NULL;
    henka_ui_font_test_fixture fixture;
    henka_ui_font_resource* font_a = NULL;
    henka_ui_font_resource* font_b = NULL;
    henka_ui_font_raster_profile* profile_a = NULL;
    henka_ui_font_raster_profile* profile_b = NULL;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    HENKA_TEST_ASSERT(henka_ui_font_test_create_profile(
        &fixture, 0x401U, 1U, 1U, &font_a, &profile_a));
    HENKA_TEST_ASSERT(henka_ui_font_test_create_profile(
        &fixture, 0x402U, 1U, 1U, &font_b, &profile_b));
    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_context_set_font_profile(ui, profile_a, 100U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(ui->font_profile == profile_a && ui->font_app_scale_percent == 100U);
    HENKA_TEST_ASSERT(henka_ui_context_set_font_profile(ui, profile_b, 125U) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(ui->font_profile == profile_a && ui->font_app_scale_percent == 100U);

    memset(&frame_desc, 0, sizeof(frame_desc));
    frame_desc.framebuffer_width = 1280;
    frame_desc.framebuffer_height = 720;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_context_set_font_profile(ui, profile_a, 200U) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(ui->font_profile == profile_a && ui->font_app_scale_percent == 100U);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_context_set_font_profile(ui, profile_a, 125U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(ui->font_profile == profile_a && ui->font_app_scale_percent == 125U);

    henka_ui_destroy(ui);
    henka_ui_font_raster_profile_destroy(profile_b);
    henka_ui_font_raster_profile_destroy(profile_a);
    henka_ui_font_resource_destroy(font_b);
    henka_ui_font_resource_destroy(font_a);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_font_scale_update_publishes_all_contexts_atomically(void)
{
    henka_ui_frame_desc frame_desc = {0};
    henka_ui_context* first_context = NULL;
    henka_ui_context* second_context = NULL;
    henka_ui_font_test_fixture fixture;
    henka_ui_font_resource* first_font = NULL;
    henka_ui_font_resource* second_font = NULL;
    henka_ui_font_raster_profile* first_profile = NULL;
    henka_ui_font_raster_profile* second_profile = NULL;
    henka_ui_font_context_scale_binding bindings[2U];
    henka_ui_font_context_scale_update* update = NULL;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    HENKA_TEST_ASSERT(henka_ui_font_test_create_profile(
        &fixture, 0x403U, 1U, 1U, &first_font, &first_profile));
    HENKA_TEST_ASSERT(henka_ui_font_test_create_profile(
        &fixture, 0x404U, 1U, 1U, &second_font, &second_profile));
    HENKA_TEST_ASSERT(henka_ui_create(&first_context) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_create(&second_context) == HENKA_SUCCESS);
    frame_desc.framebuffer_width = 1280;
    frame_desc.framebuffer_height = 720;
    bindings[0] = (henka_ui_font_context_scale_binding){first_context, first_profile};
    bindings[1] = (henka_ui_font_context_scale_binding){second_context, second_profile};

    HENKA_TEST_ASSERT(henka_ui_begin_frame(second_context, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_font_context_scale_update_prepare(
        bindings, 2U, 150U, &update) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(update == NULL);
    HENKA_TEST_ASSERT(first_context->font_profile == NULL);
    HENKA_TEST_ASSERT(second_context->font_profile == NULL);
    HENKA_TEST_ASSERT(henka_ui_end_frame(second_context) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_ui_font_context_scale_update_prepare(
        bindings, 2U, 150U, &update) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(update != NULL);
    HENKA_TEST_ASSERT(first_context->font_profile == NULL);
    HENKA_TEST_ASSERT(second_context->font_profile == NULL);
    henka_ui_font_context_scale_update_dispose(update);
    update = NULL;
    HENKA_TEST_ASSERT(first_context->font_profile == NULL);
    HENKA_TEST_ASSERT(second_context->font_profile == NULL);

    HENKA_TEST_ASSERT(henka_ui_font_context_scale_update_prepare(
        bindings, 2U, 150U, &update) == HENKA_SUCCESS);
    henka_ui_font_context_scale_update_publish(update);
    update = NULL;
    HENKA_TEST_ASSERT(first_context->font_profile == first_profile);
    HENKA_TEST_ASSERT(second_context->font_profile == second_profile);
    HENKA_TEST_ASSERT(first_context->font_app_scale_percent == 150U);
    HENKA_TEST_ASSERT(second_context->font_app_scale_percent == 150U);
    HENKA_TEST_ASSERT(first_context->font_context_identity == 0x403U);
    HENKA_TEST_ASSERT(second_context->font_context_identity == 0x404U);

    henka_ui_destroy(second_context);
    henka_ui_destroy(first_context);
    henka_ui_font_raster_profile_destroy(second_profile);
    henka_ui_font_raster_profile_destroy(first_profile);
    henka_ui_font_resource_destroy(second_font);
    henka_ui_font_resource_destroy(first_font);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_font_scale_update_keeps_unprofiled_contexts_scale_aware(void)
{
    henka_ui_context* first_context = NULL;
    henka_ui_context* second_context = NULL;
    henka_ui_font_context_scale_binding bindings[2U];
    henka_ui_font_context_scale_update* update = NULL;
    uint16_t first_initial_scale;
    uint16_t second_initial_scale;

    HENKA_TEST_ASSERT(henka_ui_create(&first_context) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_create(&second_context) == HENKA_SUCCESS);
    first_initial_scale = first_context->font_app_scale_percent;
    second_initial_scale = second_context->font_app_scale_percent;
    bindings[0] = (henka_ui_font_context_scale_binding){first_context, NULL};
    bindings[1] = (henka_ui_font_context_scale_binding){second_context, NULL};

    HENKA_TEST_ASSERT(henka_ui_font_context_scale_update_prepare(
        bindings, 2U, 110U, &update) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(update == NULL);
    HENKA_TEST_ASSERT(first_context->font_app_scale_percent == first_initial_scale);
    HENKA_TEST_ASSERT(second_context->font_app_scale_percent == second_initial_scale);
    HENKA_TEST_ASSERT(first_context->font_profile == NULL);
    HENKA_TEST_ASSERT(second_context->font_profile == NULL);
    HENKA_TEST_ASSERT(first_context->font_context_identity == 0U);
    HENKA_TEST_ASSERT(second_context->font_context_identity == 0U);

    HENKA_TEST_ASSERT(henka_ui_font_context_scale_update_prepare(
        bindings, 2U, 150U, &update) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(update != NULL);
    HENKA_TEST_ASSERT(first_context->font_profile == NULL);
    HENKA_TEST_ASSERT(second_context->font_profile == NULL);
    henka_ui_font_context_scale_update_publish(update);
    update = NULL;

    HENKA_TEST_ASSERT(first_context->font_profile == NULL);
    HENKA_TEST_ASSERT(second_context->font_profile == NULL);
    HENKA_TEST_ASSERT(first_context->font_context_identity == 0U);
    HENKA_TEST_ASSERT(second_context->font_context_identity == 0U);
    HENKA_TEST_ASSERT(first_context->font_app_scale_percent == 150U);
    HENKA_TEST_ASSERT(second_context->font_app_scale_percent == 150U);

    henka_ui_destroy(second_context);
    henka_ui_destroy(first_context);
}

static void henka_test_ui_mixed_glyph_solid_quads_keep_submission_order(void)
{
    const henka_vec4 glyph_color = {0.21f, 0.43f, 0.65f, 0.37f};
    henka_ui_draw_vertex vertices[18U];
    henka_ui_frame_desc frame_desc;
    henka_ui_context* ui = NULL;
    henka_ui_font_test_fixture fixture;
    henka_ui_font_resource* font = NULL;
    henka_ui_font_raster_profile* profile = NULL;
    size_t vertex_count = 0U;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    HENKA_TEST_ASSERT(henka_ui_font_test_create_profile(
        &fixture, 0x501U, 1U, 1U, &font, &profile));
    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_context_set_font_profile(ui, profile, 100U) == HENKA_SUCCESS);
    memset(&frame_desc, 0, sizeof(frame_desc));
    frame_desc.framebuffer_width = 1280;
    frame_desc.framebuffer_height = 720;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_overlay_rect(ui,
        (henka_ui_rect){1.0f, 2.0f, 3.0f, 4.0f},
        (henka_vec4){0.8f, 0.1f, 0.2f, 0.6f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_draw_font_text(ui, 10.0f, 12.0f, 1.0f, "A", glyph_color) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_overlay_rect(ui,
        (henka_ui_rect){20.0f, 21.0f, 22.0f, 23.0f},
        (henka_vec4){0.1f, 0.8f, 0.2f, 0.4f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(ui->draw_quad_count == 3U);
    HENKA_TEST_ASSERT(ui->draw_rect_count == 2U);
    HENKA_TEST_ASSERT(ui->draw_quads[0].kind == HENKA_UI_DRAW_QUAD_SOLID);
    HENKA_TEST_ASSERT(ui->draw_quads[1].kind == HENKA_UI_DRAW_QUAD_GLYPH);
    HENKA_TEST_ASSERT(ui->draw_quads[2].kind == HENKA_UI_DRAW_QUAD_SOLID);
    HENKA_TEST_ASSERT(ui->draw_quads[0].bounds.x == 1.0f);
    HENKA_TEST_ASSERT(ui->draw_quads[1].color.x == glyph_color.x);
    HENKA_TEST_ASSERT(ui->draw_quads[1].color.w == glyph_color.w);
    HENKA_TEST_ASSERT(ui->draw_quads[1].u1 > ui->draw_quads[1].u0);
    HENKA_TEST_ASSERT(ui->draw_quads[1].v1 > ui->draw_quads[1].v0);
    HENKA_TEST_ASSERT(ui->draw_quads[2].bounds.x == 20.0f);
    vertices[0].x = -73.0f;
    HENKA_TEST_ASSERT(henka_ui_context_build_quad_vertices(
        ui, vertices, 17U, &vertex_count) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(vertex_count == 0U && vertices[0].x == -73.0f);
    HENKA_TEST_ASSERT(henka_ui_context_build_quad_vertices(
        ui, vertices, 18U, &vertex_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(vertex_count == 18U);
    HENKA_TEST_ASSERT(vertices[0].x == 1.0f && vertices[0].y == 2.0f);
    HENKA_TEST_ASSERT(vertices[0].glyph_coverage == 0.0f);
    HENKA_TEST_ASSERT(vertices[6].x == ui->draw_quads[1].bounds.x);
    HENKA_TEST_ASSERT(vertices[6].glyph_coverage == 1.0f);
    HENKA_TEST_ASSERT(vertices[6].u == ui->draw_quads[1].u0);
    HENKA_TEST_ASSERT(vertices[6].v == ui->draw_quads[1].v0);
    HENKA_TEST_ASSERT(vertices[6].r == glyph_color.x && vertices[6].a == glyph_color.w);
    HENKA_TEST_ASSERT(vertices[12].x == 20.0f && vertices[12].glyph_coverage == 0.0f);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);
    henka_ui_destroy(ui);
    henka_ui_font_raster_profile_destroy(profile);
    henka_ui_font_resource_destroy(font);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_font_text_draw_preserves_line_breaks(void)
{
    henka_ui_frame_desc frame_desc;
    henka_ui_context* ui = NULL;
    henka_ui_font_test_fixture fixture;
    henka_ui_font_resource* font = NULL;
    henka_ui_font_raster_profile* profile = NULL;
    henka_ui_font_info font_info;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    HENKA_TEST_ASSERT(henka_ui_font_test_create_profile(
        &fixture, 0x601U, 1U, 1U, &font, &profile));
    HENKA_TEST_ASSERT(henka_ui_font_resource_get_info(
        henka_ui_font_raster_profile_get_font(profile, 100U), &font_info));
    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_context_set_font_profile(ui, profile, 100U) == HENKA_SUCCESS);

    memset(&frame_desc, 0, sizeof(frame_desc));
    frame_desc.framebuffer_width = 1280;
    frame_desc.framebuffer_height = 720;
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_draw_font_text(
        ui, 24.0f, 32.0f, 1.0f, "A\nA", (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(ui->draw_quad_count == 2U);
    HENKA_TEST_ASSERT(ui->draw_quads[0].kind == HENKA_UI_DRAW_QUAD_GLYPH);
    HENKA_TEST_ASSERT(ui->draw_quads[1].kind == HENKA_UI_DRAW_QUAD_GLYPH);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        ui->draw_quads[0].bounds.x, ui->draw_quads[1].bounds.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        ui->draw_quads[1].bounds.y - ui->draw_quads[0].bounds.y,
        font_info.line_height,
        0.0001f);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    henka_ui_destroy(ui);
    henka_ui_font_raster_profile_destroy(profile);
    henka_ui_font_resource_destroy(font);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_ui_context_measurement_uses_active_font_metrics(void)
{
    henka_ui_context* ui = NULL;
    henka_ui_font_test_fixture fixture;
    henka_ui_font_resource* font = NULL;
    henka_ui_font_raster_profile* profile = NULL;
    henka_ui_font_info font_info;
    henka_ui_text_run run;
    henka_ui_frame_desc frame_desc = {0};
    int narrow_width = 0;
    int wide_width = 0;
    int six_wide_width = 0;
    int one_line_height = 0;
    int two_line_height = 0;
    int measured_run_width = 0;
    int measured_run_height = 0;
    bool saw_font_glyph;
    size_t allocations_before_measurement;
    size_t wrapped_line_count = 0U;
    char wrapped_text[128];
    char limited_text[128];
    char hidden_text[128];
    char other_hidden_text[128];
    const char* hidden_input = "Long canonical scene object name";
    bool was_truncated = false;

    HENKA_TEST_ASSERT(henka_ui_font_test_fixture_load(&fixture));
    HENKA_TEST_ASSERT(henka_ui_font_test_create_profile(
        &fixture, 0x701U, 1U, 1U, &font, &profile));
    HENKA_TEST_ASSERT(henka_ui_font_resource_get_info(
        henka_ui_font_raster_profile_get_font(profile, 100U), &font_info));
    HENKA_TEST_ASSERT(henka_ui_create(&ui) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_context_set_font_profile(ui, profile, 100U) == HENKA_SUCCESS);

    allocations_before_measurement = henka_memory_get_allocation_count();
    HENKA_TEST_ASSERT(henka_ui_measure_text_for_context(
        ui, "iiii", 1.0f, &narrow_width, &one_line_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == allocations_before_measurement);
    HENKA_TEST_ASSERT(henka_ui_measure_text_for_context(
        ui, "WWWW", 1.0f, &wide_width, &measured_run_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(wide_width > narrow_width);
    HENKA_TEST_ASSERT(one_line_height == (int)ceilf(font_info.line_height));
    HENKA_TEST_ASSERT(measured_run_height == one_line_height);

    {
        size_t byte_offset = SIZE_MAX;
        int after_narrow_prefix = 0;
        int after_first_wide_glyph = 0;
        int ignored_height = 0;

        HENKA_TEST_ASSERT(henka_ui_measure_text_for_context(
            ui, "iiiiW", 1.0f, &after_first_wide_glyph, &ignored_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_ui_measure_text_for_context(
            ui, "iiii", 1.0f, &after_narrow_prefix, &ignored_height) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(sandbox3d_editor_text_byte_offset_at_x(
            ui, "iiiiWWWW", 1.0f,
            (float)after_narrow_prefix + 0.1f,
            &byte_offset) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(byte_offset == 4U);
        HENKA_TEST_ASSERT(sandbox3d_editor_text_byte_offset_at_x(
            ui, "iiiiWWWW", 1.0f,
            ((float)after_narrow_prefix + (float)after_first_wide_glyph) * 0.5f,
            &byte_offset) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(byte_offset == 5U);
        HENKA_TEST_ASSERT(sandbox3d_editor_text_byte_offset_at_x(
            ui, "iiiiWWWW", 1.0f, (float)wide_width * 2.0f,
            &byte_offset) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(byte_offset == 8U);
        HENKA_TEST_ASSERT(sandbox3d_editor_text_byte_offset_at_x(
            ui, "iiiiWWWW", 1.0f, -1.0f, &byte_offset) == HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(byte_offset == 0U);
    }

    HENKA_TEST_ASSERT(sandbox3d_editor_text_wrap(
        ui, "WWWW iii", (float)wide_width,
        wrapped_text, sizeof(wrapped_text), &wrapped_line_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(wrapped_line_count == 2U);
    HENKA_TEST_ASSERT(strcmp(wrapped_text, "WWWW\niii") == 0);
    HENKA_TEST_ASSERT(sandbox3d_editor_text_measure_wrapped_lines(
        ui, "WWWW iii", (float)wide_width, &wrapped_line_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(wrapped_line_count == 2U);
    HENKA_TEST_ASSERT(sandbox3d_editor_text_wrap(
        ui, "WWWW iii MMM", (float)wide_width,
        wrapped_text, sizeof(wrapped_text), &wrapped_line_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(wrapped_line_count >= 3U);
    HENKA_TEST_ASSERT(sandbox3d_editor_text_limit_wrapped_lines(
        ui, wrapped_text, 2U, (float)wide_width,
        limited_text, sizeof(limited_text), &wrapped_line_count,
        &was_truncated) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(was_truncated && wrapped_line_count == 2U);
    HENKA_TEST_ASSERT(strstr(limited_text, "...") != NULL);
    HENKA_TEST_ASSERT(henka_ui_measure_text_for_context(
        ui, "WWWWWW", 1.0f, &six_wide_width, &measured_run_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(six_wide_width > wide_width);
    HENKA_TEST_ASSERT(sandbox3d_editor_text_fit(
        ui, "WWWWWW", ((float)wide_width + (float)six_wide_width) * 0.5f,
        limited_text, sizeof(limited_text), &was_truncated) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(was_truncated &&
        strcmp(limited_text, "WWWWWW") != 0 && strstr(limited_text, "...") != NULL);
    HENKA_TEST_ASSERT(sandbox3d_editor_text_format_hidden_row(
        ui, hidden_input, 110.0f,
        hidden_text, sizeof(hidden_text), &was_truncated) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(was_truncated && strstr(hidden_text, " [Hidden]") != NULL);
    HENKA_TEST_ASSERT(strlen(hidden_text) >= strlen(" [Hidden]") &&
        strcmp(hidden_text + strlen(hidden_text) - strlen(" [Hidden]"), " [Hidden]") == 0);
    HENKA_TEST_ASSERT(strchr(hidden_text, '\n') == NULL);
    HENKA_TEST_ASSERT(strcmp(hidden_input, "Long canonical scene object name") == 0);
    HENKA_TEST_ASSERT(sandbox3d_editor_text_format_hidden_row(
        ui, "Another long scene object name", 110.0f,
        other_hidden_text, sizeof(other_hidden_text), &was_truncated) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(was_truncated && strstr(other_hidden_text, " [Hidden]") != NULL);
    HENKA_TEST_ASSERT(strlen(other_hidden_text) >= strlen(" [Hidden]") &&
        strcmp(other_hidden_text + strlen(other_hidden_text) - strlen(" [Hidden]"), " [Hidden]") == 0);
    HENKA_TEST_ASSERT(strcmp(hidden_text, other_hidden_text) != 0);
    HENKA_TEST_ASSERT(sandbox3d_editor_text_measure_wrapped_lines(
        ui, hidden_text, 110.0f, &wrapped_line_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(wrapped_line_count == 1U);

    memset(&run, 0, sizeof(run));
    HENKA_TEST_ASSERT(henka_ui_text_layout_build(
        henka_ui_font_raster_profile_get_font(profile, 100U),
        "AVATAR I/l/1 O/0: 123.45",
        strlen("AVATAR I/l/1 O/0: 123.45"),
        0.0f,
        &run,
        NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_measure_text_for_context(
        ui, "AVATAR I/l/1 O/0: 123.45", 1.0f,
        &measured_run_width, &measured_run_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(measured_run_width == (int)ceilf(run.advance_width));
    henka_ui_text_run_dispose(&run);

    HENKA_TEST_ASSERT(henka_ui_measure_text_for_context(
        ui, "A\nA", 1.0f, &measured_run_width, &two_line_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(two_line_height == (int)ceilf(font_info.line_height * 2.0f));

    HENKA_TEST_ASSERT(henka_ui_context_set_font_profile(ui, profile, 200U) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_ui_measure_text_for_context(
        ui, "WWWW", 1.0f, &measured_run_width, &measured_run_height) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(measured_run_width >= wide_width - 1 &&
        measured_run_width <= wide_width + 1);
    HENKA_TEST_ASSERT(measured_run_height >= one_line_height - 1 &&
        measured_run_height <= one_line_height + 1);

    memset(&run, 0, sizeof(run));
    HENKA_TEST_ASSERT(henka_ui_text_layout_build_limited(
        henka_ui_font_raster_profile_get_font(profile, 200U),
        "WWWWWWWW\nO/0 I/l/1",
        strlen("WWWWWWWW\nO/0 I/l/1"),
        90.0f,
        1U,
        &run,
        NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(run.truncated && run.line_count == 1U);
    HENKA_TEST_ASSERT(run.advance_width <= 90.0f);
    HENKA_TEST_ASSERT(run.glyph_count > 0U);
    HENKA_TEST_ASSERT(run.glyphs[run.glyph_count - 1U].rendered_codepoint == 0x2026U);
    henka_ui_text_run_dispose(&run);

    frame_desc.framebuffer_width = 1280;
    frame_desc.framebuffer_height = 720;
    henka_ui_set_visible(ui, true);
    HENKA_TEST_ASSERT(henka_ui_begin_frame(ui, &frame_desc) == HENKA_SUCCESS);
    (void)henka_ui_button(
        ui, "font_fit_control", (henka_ui_rect){10.0f, 10.0f, 90.0f, 28.0f}, "WWWWWWWW");
    HENKA_TEST_ASSERT(ui->draw_quad_count > 0U);
    saw_font_glyph = false;
    for (size_t quad_index = 0U; quad_index < ui->draw_quad_count; ++quad_index)
    {
        if (ui->draw_quads[quad_index].kind == HENKA_UI_DRAW_QUAD_GLYPH)
        {
            saw_font_glyph = true;
            break;
        }
    }
    HENKA_TEST_ASSERT(saw_font_glyph);
    HENKA_TEST_ASSERT(henka_ui_end_frame(ui) == HENKA_SUCCESS);

    henka_ui_destroy(ui);
    henka_ui_font_raster_profile_destroy(profile);
    henka_ui_font_resource_destroy(font);
    henka_ui_font_test_fixture_dispose(&fixture);
}

static void henka_test_platform_window_display_scale_contract(void)
{
    static const struct
    {
        float scale;
        uint32_t numerator;
        uint32_t denominator;
    } cases[] = {
        {1.0f, 1U, 1U},
        {1.25f, 5U, 4U},
        {1.5f, 3U, 2U},
        {1.75f, 7U, 4U},
        {2.0f, 2U, 1U},
        {1.3333333f, 4U, 3U}};
    size_t case_index;
    uint32_t numerator = 0U;
    uint32_t denominator = 0U;
    uint32_t native_to_content_numerator = 0U;
    uint32_t native_to_content_denominator = 0U;
    int content_width = 0;
    int content_height = 0;
    int native_width = 0;
    int native_height = 0;
    float framebuffer_x = -99.0f;
    float framebuffer_y = -99.0f;

    for (case_index = 0U; case_index < sizeof(cases) / sizeof(cases[0]); ++case_index)
    {
        HENKA_TEST_ASSERT(henka_platform_window_scale_from_display_scale(
            cases[case_index].scale, &numerator, &denominator));
        HENKA_TEST_ASSERT(numerator == cases[case_index].numerator);
        HENKA_TEST_ASSERT(denominator == cases[case_index].denominator);
    }

    HENKA_TEST_ASSERT(henka_platform_window_scale_point_to_framebuffer(
        101.5f, 35.25f, 5U, 4U, &framebuffer_x, &framebuffer_y));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(framebuffer_x, 126.875f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(framebuffer_y, 44.0625f, 0.0001f);

    numerator = 17U;
    denominator = 19U;
    HENKA_TEST_ASSERT(!henka_platform_window_scale_from_display_scale(
        NAN, &numerator, &denominator));
    HENKA_TEST_ASSERT(numerator == 17U && denominator == 19U);
    HENKA_TEST_ASSERT(!henka_platform_window_scale_from_display_scale(
        INFINITY, &numerator, &denominator));
    HENKA_TEST_ASSERT(!henka_platform_window_scale_from_display_scale(
        0.0f, &numerator, &denominator));
    HENKA_TEST_ASSERT(!henka_platform_window_scale_from_display_scale(
        4.01f, &numerator, &denominator));
    HENKA_TEST_ASSERT(!henka_platform_window_scale_from_display_scale(
        1.4142f, &numerator, &denominator));
    HENKA_TEST_ASSERT(numerator == 17U && denominator == 19U);

    HENKA_TEST_ASSERT(henka_platform_window_scale_from_native_geometry(
        2.0f, 1.0f,
        &native_to_content_numerator,
        &native_to_content_denominator));
    HENKA_TEST_ASSERT(native_to_content_numerator == 1U &&
        native_to_content_denominator == 2U);
    HENKA_TEST_ASSERT(henka_platform_window_scale_content_size_from_framebuffer(
        2560, 1440, 2U, 1U, &content_width, &content_height));
    HENKA_TEST_ASSERT(content_width == 1280 && content_height == 720);
    HENKA_TEST_ASSERT(henka_platform_window_scale_content_size_to_native(
        1280, 720, 1U, 2U, &native_width, &native_height));
    HENKA_TEST_ASSERT(native_width == 2560 && native_height == 1440);

    HENKA_TEST_ASSERT(henka_platform_window_scale_from_native_geometry(
        2.0f, 2.0f,
        &native_to_content_numerator,
        &native_to_content_denominator));
    HENKA_TEST_ASSERT(native_to_content_numerator == 1U &&
        native_to_content_denominator == 1U);
    HENKA_TEST_ASSERT(henka_platform_window_scale_from_native_geometry(
        1.5f, 1.0f,
        &native_to_content_numerator,
        &native_to_content_denominator));
    HENKA_TEST_ASSERT(native_to_content_numerator == 2U &&
        native_to_content_denominator == 3U);
    HENKA_TEST_ASSERT(henka_platform_window_scale_from_native_geometry(
        1.25f, 1.0f,
        &native_to_content_numerator,
        &native_to_content_denominator));
    HENKA_TEST_ASSERT(native_to_content_numerator == 4U &&
        native_to_content_denominator == 5U);

    native_to_content_numerator = 23U;
    native_to_content_denominator = 29U;
    HENKA_TEST_ASSERT(!henka_platform_window_scale_from_native_geometry(
        0.0f, 1.0f,
        &native_to_content_numerator,
        &native_to_content_denominator));
    HENKA_TEST_ASSERT(native_to_content_numerator == 23U &&
        native_to_content_denominator == 29U);
    content_width = 17;
    content_height = 19;
    HENKA_TEST_ASSERT(!henka_platform_window_scale_content_size_from_framebuffer(
        0, 720, 3U, 2U, &content_width, &content_height));
    HENKA_TEST_ASSERT(content_width == 0 && content_height == 0);
    native_width = 17;
    native_height = 19;
    HENKA_TEST_ASSERT(!henka_platform_window_scale_content_size_to_native(
        1280, 720, 0U, 1U, &native_width, &native_height));
    HENKA_TEST_ASSERT(native_width == 0 && native_height == 0);

    framebuffer_x = -99.0f;
    framebuffer_y = -99.0f;
    HENKA_TEST_ASSERT(!henka_platform_window_scale_point_to_framebuffer(
        10.0f, 20.0f, 5U, 0U, &framebuffer_x, &framebuffer_y));
    HENKA_TEST_ASSERT(framebuffer_x == -99.0f && framebuffer_y == -99.0f);
}

void henka_test_ui_font(void)
{
    henka_test_platform_window_display_scale_contract();
    henka_test_ui_font_sha256_and_static_font_resource();
    henka_test_ui_font_invalid_identity_and_source_are_not_published();
    henka_test_ui_font_resource_rejects_variable_font_table();
    henka_test_ui_font_bounds_and_packing_fail_closed();
    henka_test_ui_font_text_layout_uses_shared_glyph_placements();
    henka_test_ui_segmented_select_fits_compact_labels_at_200_percent();
    henka_test_ui_font_allocation_failures_are_atomic();
    henka_test_ui_font_application_bootstrap_verifies_asset_identity();
    henka_test_ui_atlas_uses_effective_raster_pixel_size();
    henka_test_ui_control_labels_use_centered_font_metrics();
    henka_test_ui_output_scale_aligns_draw_and_hit_coordinates();
    henka_test_ui_atlas_identity_includes_font_scale_output_and_context();
    henka_test_ui_atlas_cache_is_bounded_per_context();
    henka_test_ui_context_profile_replacement_is_atomic();
    henka_test_ui_font_scale_update_publishes_all_contexts_atomically();
    henka_test_ui_font_scale_update_keeps_unprofiled_contexts_scale_aware();
    henka_test_ui_font_application_bundle_loader_is_confined_and_verifies_bytes();
    henka_test_ui_mixed_glyph_solid_quads_keep_submission_order();
    henka_test_ui_font_text_draw_preserves_line_breaks();
    henka_test_ui_context_measurement_uses_active_font_metrics();
}
