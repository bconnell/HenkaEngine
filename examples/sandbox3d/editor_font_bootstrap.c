#include "editor_font_bootstrap.h"

#include <string.h>

henka_result sandbox3d_editor_font_bootstrap_create(
    const sandbox3d_editor_font_asset_desc* asset,
    henka_ui_font_resource** out_resource,
    henka_ui_font_diagnostic* out_diagnostic)
{
    char actual_sha256[65U];
    henka_ui_font_desc font_desc;

    henka_ui_font_set_diagnostic(
        out_diagnostic,
        HENKA_UI_FONT_STAGE_NONE,
        asset != NULL ? asset->asset_identity : NULL,
        NULL);
    if (asset == NULL || out_resource == NULL || *out_resource != NULL)
    {
        henka_ui_font_set_diagnostic(
            out_diagnostic,
            HENKA_UI_FONT_STAGE_ARGUMENT,
            asset != NULL ? asset->asset_identity : NULL,
            "editor font asset bootstrap arguments are invalid");
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (asset->bytes == NULL || asset->byte_count == 0U)
    {
        henka_ui_font_set_diagnostic(
            out_diagnostic,
            HENKA_UI_FONT_STAGE_ARGUMENT,
            asset->asset_identity,
            "editor font asset bytes are empty");
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (asset->byte_count > HENKA_UI_FONT_MAX_SOURCE_BYTES)
    {
        henka_ui_font_set_diagnostic(
            out_diagnostic,
            HENKA_UI_FONT_STAGE_SOURCE_LIMIT,
            asset->asset_identity,
            "editor font asset exceeds the 2 MiB source limit");
        return HENKA_ERROR_LIMIT;
    }
    if (!henka_ui_font_identity_valid(asset->asset_identity))
    {
        henka_ui_font_set_diagnostic(
            out_diagnostic,
            HENKA_UI_FONT_STAGE_IDENTITY,
            NULL,
            "editor font asset identity must be bounded printable ASCII");
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!henka_ui_font_hash_hex_valid(asset->expected_sha256_hex) ||
        !henka_ui_font_sha256_hex(asset->bytes, asset->byte_count, actual_sha256) ||
        !henka_ui_font_hash_matches(asset->expected_sha256_hex, actual_sha256))
    {
        henka_ui_font_set_diagnostic(
            out_diagnostic,
            HENKA_UI_FONT_STAGE_HASH,
            asset->asset_identity,
            "editor font asset bytes do not match the pinned SHA-256 identity");
        return HENKA_ERROR_ASSET_SOURCE;
    }

    memset(&font_desc, 0, sizeof(font_desc));
    font_desc.source_bytes = asset->bytes;
    font_desc.source_size = asset->byte_count;
    font_desc.identity = asset->asset_identity;
    font_desc.verified_sha256_hex = actual_sha256;
    font_desc.required_codepoints = asset->required_codepoints;
    font_desc.required_codepoint_count = asset->required_codepoint_count;
    font_desc.raster_pixel_height = asset->raster_pixel_height;
    font_desc.atlas_width = asset->atlas_width;
    font_desc.atlas_height = asset->atlas_height;
    return henka_ui_font_resource_create(&font_desc, out_resource, out_diagnostic);
}
