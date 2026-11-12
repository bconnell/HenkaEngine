#ifndef HENKA_UI_FONT_INTERNAL_H
#define HENKA_UI_FONT_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/ui_font.h>

#define HENKA_UI_FONT_MAX_GLYPHS ((size_t)512U)
#define HENKA_UI_FONT_MAX_ATLAS_DIMENSION 2048U
#define HENKA_UI_TEXT_MAX_BYTES ((size_t)4096U)
#define HENKA_UI_FONT_APP_SCALE_COUNT 4U
#define HENKA_UI_FONT_MAX_CONTEXTS 6U

typedef struct henka_ui_font_desc
{
    const uint8_t* source_bytes;
    size_t source_size;
    const char* identity;
    const char* verified_sha256_hex;
    const uint32_t* required_codepoints;
    size_t required_codepoint_count;
    uint32_t raster_pixel_height;
    uint32_t atlas_width;
    uint32_t atlas_height;
} henka_ui_font_desc;

typedef struct henka_ui_font_glyph_info
{
    uint32_t codepoint;
    uint32_t glyph_index;
    float advance_x;
    float bearing_x;
    float bitmap_x;
    float bitmap_y;
    uint32_t bitmap_width;
    uint32_t bitmap_height;
    uint32_t atlas_x;
    uint32_t atlas_y;
} henka_ui_font_glyph_info;

typedef struct henka_ui_text_glyph
{
    uint32_t source_codepoint;
    uint32_t rendered_codepoint;
    uint32_t glyph_index;
    float pen_x;
    float x;
    float y;
    float width;
    float height;
    float advance_x;
    uint32_t atlas_x;
    uint32_t atlas_y;
} henka_ui_text_glyph;

typedef struct henka_ui_text_run
{
    henka_ui_text_glyph* glyphs;
    size_t glyph_count;
    size_t line_count;
    size_t replacement_count;
    bool truncated;
    float advance_width;
    float ink_left;
    float ink_top;
    float ink_right;
    float ink_bottom;
    float ascent;
    float descent;
    float line_height;
} henka_ui_text_run;

/* Hash, parser, glyph, atlas, and draw-command details remain engine-private. */
bool henka_ui_font_sha256_hex(
    const void* bytes,
    size_t byte_count,
    char out_hex[65U]);
bool henka_ui_font_hash_hex_valid(const char* hash_hex);
bool henka_ui_font_hash_matches(const char* expected, const char* actual);
bool henka_ui_font_identity_valid(const char* identity);
void henka_ui_font_set_diagnostic(
    henka_ui_font_diagnostic* diagnostic,
    henka_ui_font_stage stage,
    const char* identity,
    const char* message);

henka_result henka_ui_font_resource_create(
    const henka_ui_font_desc* desc,
    henka_ui_font_resource** out_resource,
    henka_ui_font_diagnostic* out_diagnostic);
bool henka_ui_font_resource_get_glyph(
    const henka_ui_font_resource* resource,
    uint32_t codepoint,
    henka_ui_font_glyph_info* out_glyph);
bool henka_ui_font_resource_get_kerning(
    const henka_ui_font_resource* resource,
    uint32_t left_glyph_index,
    uint32_t right_glyph_index,
    float* out_kerning);
const uint8_t* henka_ui_font_resource_get_atlas(
    const henka_ui_font_resource* resource,
    uint32_t* out_width,
    uint32_t* out_height);

const henka_ui_font_resource* henka_ui_font_raster_profile_get_font(
    const henka_ui_font_raster_profile* profile,
    uint16_t app_scale_percent);
const uint8_t* henka_ui_font_raster_profile_get_atlas(
    const henka_ui_font_raster_profile* profile,
    uint16_t app_scale_percent,
    uint32_t* out_width,
    uint32_t* out_height);
uint64_t henka_ui_font_raster_profile_get_context_identity(
    const henka_ui_font_raster_profile* profile);

henka_result henka_ui_text_layout_build(
    const henka_ui_font_resource* resource,
    const char* utf8,
    size_t utf8_size,
    float max_width,
    henka_ui_text_run* out_run,
    henka_ui_font_diagnostic* out_diagnostic);
void henka_ui_text_run_dispose(henka_ui_text_run* run);

#endif
