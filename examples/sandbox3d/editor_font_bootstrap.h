#ifndef HENKA_SANDBOX3D_EDITOR_FONT_BOOTSTRAP_H
#define HENKA_SANDBOX3D_EDITOR_FONT_BOOTSTRAP_H

#include <stddef.h>
#include <stdint.h>

#include <henka/ui_font.h>

typedef henka_ui_font_asset_desc sandbox3d_editor_font_asset_desc;

typedef struct sandbox3d_editor_font_file_asset_desc
{
    const char* asset_path;
    const char* asset_identity;
    const char* expected_sha256_hex;
    const uint32_t* required_codepoints;
    size_t required_codepoint_count;
    uint32_t raster_pixel_height;
    uint32_t atlas_width;
    uint32_t atlas_height;
} sandbox3d_editor_font_file_asset_desc;

typedef enum sandbox3d_editor_font_candidate
{
    SANDBOX3D_EDITOR_FONT_CANDIDATE_ATKINSON = 0,
    SANDBOX3D_EDITOR_FONT_CANDIDATE_NOTO_SANS = 1
} sandbox3d_editor_font_candidate;

/* The comparison hook accepts only two repository-owned aliases, and only
 * when both automation ownership and diagnostics are enabled. An absent or
 * untrusted override selects the product default, Atkinson. */
bool sandbox3d_editor_font_candidate_resolve(
    const char* requested_alias,
    bool automation_input_owned,
    bool automation_diagnostics_enabled,
    sandbox3d_editor_font_candidate* out_candidate);
const sandbox3d_editor_font_file_asset_desc*
sandbox3d_editor_font_candidate_get_asset(
    sandbox3d_editor_font_candidate candidate);
const char* sandbox3d_editor_font_candidate_get_alias(
    sandbox3d_editor_font_candidate candidate);

/*
 * Validates the identity of bytes already obtained by the application asset
 * boundary, then constructs the opaque UI font resource. This function never
 * resolves or opens a path.
 */
henka_result sandbox3d_editor_font_bootstrap_create(
    const sandbox3d_editor_font_asset_desc* asset,
    henka_ui_font_resource** out_resource,
    henka_ui_font_diagnostic* out_diagnostic);

/* Resolves and reads a bounded font file beneath the product asset base path,
 * then delegates content verification and font construction to the canonical
 * UI font resource. Core UI/text code never opens paths. */
henka_result sandbox3d_editor_font_bootstrap_load(
    const char* asset_base_path,
    const sandbox3d_editor_font_file_asset_desc* asset,
    henka_ui_font_resource** out_resource,
    henka_ui_font_diagnostic* out_diagnostic);

#endif
