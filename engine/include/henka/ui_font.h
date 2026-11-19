#ifndef HENKA_UI_FONT_H
#define HENKA_UI_FONT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>
#include <henka/ui.h>

#define HENKA_UI_FONT_MAX_SOURCE_BYTES ((size_t)2097152U)
#define HENKA_UI_FONT_MAX_IDENTITY_BYTES 127U

typedef enum henka_ui_font_stage
{
    HENKA_UI_FONT_STAGE_NONE = 0,
    HENKA_UI_FONT_STAGE_ARGUMENT,
    HENKA_UI_FONT_STAGE_SOURCE_LIMIT,
    HENKA_UI_FONT_STAGE_IDENTITY,
    HENKA_UI_FONT_STAGE_HASH,
    HENKA_UI_FONT_STAGE_PARSE,
    HENKA_UI_FONT_STAGE_GLYPH_SET,
    HENKA_UI_FONT_STAGE_ATLAS_DIMENSIONS,
    HENKA_UI_FONT_STAGE_ATLAS_PACK,
    HENKA_UI_FONT_STAGE_ALLOCATION,
    HENKA_UI_FONT_STAGE_RASTER_PROFILE,
    HENKA_UI_FONT_STAGE_TEXT,
    HENKA_UI_FONT_STAGE_ASSET_READ
} henka_ui_font_stage;

typedef struct henka_ui_font_diagnostic
{
    henka_ui_font_stage stage;
    char identity[HENKA_UI_FONT_MAX_IDENTITY_BYTES + 1U];
    char message[192];
} henka_ui_font_diagnostic;

typedef struct henka_ui_font_asset_desc
{
    const uint8_t* bytes;
    size_t byte_count;
    const char* asset_identity;
    const char* expected_sha256_hex;
    const uint32_t* required_codepoints;
    size_t required_codepoint_count;
    uint32_t raster_pixel_height;
    uint32_t atlas_width;
    uint32_t atlas_height;
} henka_ui_font_asset_desc;

typedef struct henka_ui_font_info
{
    char identity[HENKA_UI_FONT_MAX_IDENTITY_BYTES + 1U];
    char sha256_hex[65U];
    size_t glyph_count;
    uint32_t raster_pixel_height;
    uint32_t atlas_width;
    uint32_t atlas_height;
    float ascent;
    float descent;
    float line_gap;
    float line_height;
} henka_ui_font_info;

typedef struct henka_ui_font_resource henka_ui_font_resource;
typedef struct henka_ui_font_raster_profile henka_ui_font_raster_profile;
typedef struct henka_ui_font_context_scale_update henka_ui_font_context_scale_update;

typedef struct henka_ui_font_context_scale_binding
{
    henka_ui_context* context;
    const henka_ui_font_raster_profile* profile;
} henka_ui_font_context_scale_binding;

typedef struct henka_ui_font_raster_profile_desc
{
    /* Stable, nonzero identity for the graphics context that owns this UI
     * output. GPU residency is additionally keyed by the renderer's actual
     * current graphics context. */
    uint64_t graphics_context_identity;
    uint32_t output_scale_numerator;
    uint32_t output_scale_denominator;
    uint32_t atlas_width;
    uint32_t atlas_height;
} henka_ui_font_raster_profile_desc;

typedef struct henka_ui_font_atlas_key
{
    char font_sha256_hex[65U];
    char glyph_set_sha256_hex[65U];
    uint16_t app_scale_percent;
    uint32_t output_scale_numerator;
    uint32_t output_scale_denominator;
    uint32_t raster_pixel_height;
    uint32_t atlas_width;
    uint32_t atlas_height;
    uint64_t graphics_context_identity;
} henka_ui_font_atlas_key;

/* Compare all source, repertoire, raster, atlas, and context inputs that
 * determine whether a GPU atlas can be reused. */
bool henka_ui_font_atlas_key_equal(
    const henka_ui_font_atlas_key* left,
    const henka_ui_font_atlas_key* right);

typedef struct henka_ui_font_raster_profile_info
{
    henka_ui_font_atlas_key key;
    uint32_t atlas_width;
    uint32_t atlas_height;
    size_t coverage_bytes;
} henka_ui_font_raster_profile_info;

/* Creates a font resource only when the bounded bytes match the declared
 * content identity. The caller owns byte acquisition; this API never opens a
 * path or consults installed system fonts. */
henka_result henka_ui_font_resource_create_from_asset(
    const henka_ui_font_asset_desc* asset,
    henka_ui_font_resource** out_resource,
    henka_ui_font_diagnostic* out_diagnostic);
void henka_ui_font_resource_destroy(henka_ui_font_resource* resource);
bool henka_ui_font_resource_get_info(
    const henka_ui_font_resource* resource,
    henka_ui_font_info* out_info);

bool henka_ui_font_app_scale_is_supported(uint16_t app_scale_percent);
henka_result henka_ui_font_raster_profile_create(
    const henka_ui_font_resource* font,
    const henka_ui_font_raster_profile_desc* desc,
    henka_ui_font_raster_profile** out_profile,
    henka_ui_font_diagnostic* out_diagnostic);
void henka_ui_font_raster_profile_destroy(
    henka_ui_font_raster_profile* profile);
size_t henka_ui_font_raster_profile_get_entry_count(
    const henka_ui_font_raster_profile* profile);
bool henka_ui_font_raster_profile_get_info(
    const henka_ui_font_raster_profile* profile,
    uint16_t app_scale_percent,
    henka_ui_font_raster_profile_info* out_info);

/* Prepares a bounded multi-context scale transition without changing live UI
 * state. Publish is a no-fail swap and consumes the update. */
henka_result henka_ui_font_context_scale_update_prepare(
    const henka_ui_font_context_scale_binding* bindings,
    size_t binding_count,
    uint16_t app_scale_percent,
    henka_ui_font_context_scale_update** out_update);
void henka_ui_font_context_scale_update_publish(
    henka_ui_font_context_scale_update* update);
void henka_ui_font_context_scale_update_dispose(
    henka_ui_font_context_scale_update* update);

henka_result henka_ui_context_set_font_profile(
    henka_ui_context* context,
    const henka_ui_font_raster_profile* profile,
    uint16_t app_scale_percent);

#endif
