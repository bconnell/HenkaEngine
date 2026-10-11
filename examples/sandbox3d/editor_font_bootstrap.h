#ifndef HENKA_SANDBOX3D_EDITOR_FONT_BOOTSTRAP_H
#define HENKA_SANDBOX3D_EDITOR_FONT_BOOTSTRAP_H

#include <stddef.h>
#include <stdint.h>

#include "../../engine/src/ui/ui_font_internal.h"

typedef struct sandbox3d_editor_font_asset_desc
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
} sandbox3d_editor_font_asset_desc;

/*
 * Validates the identity of bytes already obtained by the application asset
 * boundary, then constructs the opaque UI font resource. This function never
 * resolves or opens a path.
 */
henka_result sandbox3d_editor_font_bootstrap_create(
    const sandbox3d_editor_font_asset_desc* asset,
    henka_ui_font_resource** out_resource,
    henka_ui_font_diagnostic* out_diagnostic);

#endif
