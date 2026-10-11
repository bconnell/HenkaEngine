#ifndef HENKA_UI_FONT_INTERNAL_H
#define HENKA_UI_FONT_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>

#define HENKA_UI_FONT_MAX_SOURCE_BYTES ((size_t)2097152U)
#define HENKA_UI_FONT_MAX_GLYPHS ((size_t)512U)
#define HENKA_UI_FONT_MAX_ATLAS_DIMENSION 2048U
#define HENKA_UI_FONT_MAX_IDENTITY_BYTES 127U
#define HENKA_UI_TEXT_MAX_BYTES ((size_t)4096U)

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
    HENKA_UI_FONT_STAGE_TEXT
} henka_ui_font_stage;

typedef struct henka_ui_font_diagnostic
{
    henka_ui_font_stage stage;
    char identity[HENKA_UI_FONT_MAX_IDENTITY_BYTES + 1U];
    char message[192];
} henka_ui_font_diagnostic;

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

typedef struct henka_ui_font_resource henka_ui_font_resource;

/* Bounded SHA-256/identity helpers used by the app bootstrap before publication. */
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
void henka_ui_font_resource_destroy(henka_ui_font_resource* resource);
bool henka_ui_font_resource_get_info(
    const henka_ui_font_resource* resource,
    henka_ui_font_info* out_info);
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

henka_result henka_ui_text_layout_build(
    const henka_ui_font_resource* resource,
    const char* utf8,
    size_t utf8_size,
    float max_width,
    henka_ui_text_run* out_run,
    henka_ui_font_diagnostic* out_diagnostic);
void henka_ui_text_run_dispose(henka_ui_text_run* run);

#endif
