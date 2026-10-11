#include "test_suite.h"

#include <henka/memory.h>

#include "../examples/sandbox3d/editor_font_bootstrap.h"
#include "../engine/src/core/memory_internal.h"
#include "../engine/src/ui/ui_font_internal.h"

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

void henka_test_ui_font(void)
{
    henka_test_ui_font_sha256_and_static_font_resource();
    henka_test_ui_font_invalid_identity_and_source_are_not_published();
    henka_test_ui_font_resource_rejects_variable_font_table();
    henka_test_ui_font_bounds_and_packing_fail_closed();
    henka_test_ui_font_text_layout_uses_shared_glyph_placements();
    henka_test_ui_font_allocation_failures_are_atomic();
    henka_test_ui_font_application_bootstrap_verifies_asset_identity();
}
