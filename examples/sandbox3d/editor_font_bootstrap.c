#include "editor_font_bootstrap.h"

#include <henka/assets.h>
#include <henka/memory.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SANDBOX3D_EDITOR_FONT_ATLAS_WIDTH 1024U
#define SANDBOX3D_EDITOR_FONT_ATLAS_HEIGHT 1024U
#define SANDBOX3D_EDITOR_FONT_BASE_PIXEL_HEIGHT 16U

static const uint32_t g_editor_font_codepoints[] = {
    0x20U, 0x21U, 0x22U, 0x23U, 0x24U, 0x25U, 0x26U, 0x27U,
    0x28U, 0x29U, 0x2aU, 0x2bU, 0x2cU, 0x2dU, 0x2eU, 0x2fU,
    0x30U, 0x31U, 0x32U, 0x33U, 0x34U, 0x35U, 0x36U, 0x37U,
    0x38U, 0x39U, 0x3aU, 0x3bU, 0x3cU, 0x3dU, 0x3eU, 0x3fU,
    0x40U, 0x41U, 0x42U, 0x43U, 0x44U, 0x45U, 0x46U, 0x47U,
    0x48U, 0x49U, 0x4aU, 0x4bU, 0x4cU, 0x4dU, 0x4eU, 0x4fU,
    0x50U, 0x51U, 0x52U, 0x53U, 0x54U, 0x55U, 0x56U, 0x57U,
    0x58U, 0x59U, 0x5aU, 0x5bU, 0x5cU, 0x5dU, 0x5eU, 0x5fU,
    0x60U, 0x61U, 0x62U, 0x63U, 0x64U, 0x65U, 0x66U, 0x67U,
    0x68U, 0x69U, 0x6aU, 0x6bU, 0x6cU, 0x6dU, 0x6eU, 0x6fU,
    0x70U, 0x71U, 0x72U, 0x73U, 0x74U, 0x75U, 0x76U, 0x77U,
    0x78U, 0x79U, 0x7aU, 0x7bU, 0x7cU, 0x7dU, 0x7eU,
    0x00a0U, 0x00b0U, 0x00d7U, 0x2013U, 0x2014U, 0x2019U,
    0x2022U, 0x2026U};

static const sandbox3d_editor_font_file_asset_desc g_editor_font_candidates[] = {
    {
        "assets/ui/fonts/AtkinsonHyperlegible-Regular.ttf",
        "ui/fonts/AtkinsonHyperlegible-Regular.ttf",
        "7FB917C89019896D0B52EE84B7CBB3304C18CB90B19A62F5E32712BD23E97669",
        g_editor_font_codepoints,
        sizeof(g_editor_font_codepoints) / sizeof(g_editor_font_codepoints[0]),
        SANDBOX3D_EDITOR_FONT_BASE_PIXEL_HEIGHT,
        SANDBOX3D_EDITOR_FONT_ATLAS_WIDTH,
        SANDBOX3D_EDITOR_FONT_ATLAS_HEIGHT},
    {
        "assets/ui/fonts/NotoSans-Regular.ttf",
        "ui/fonts/NotoSans-Regular.ttf",
        "478C558EA716033CD60C03438F628DFA75694DCF6B5F6D505A2F05FD2B4F3823",
        g_editor_font_codepoints,
        sizeof(g_editor_font_codepoints) / sizeof(g_editor_font_codepoints[0]),
        SANDBOX3D_EDITOR_FONT_BASE_PIXEL_HEIGHT,
        SANDBOX3D_EDITOR_FONT_ATLAS_WIDTH,
        SANDBOX3D_EDITOR_FONT_ATLAS_HEIGHT}};

bool sandbox3d_editor_font_candidate_resolve(
    const char* requested_alias,
    bool automation_input_owned,
    bool automation_diagnostics_enabled,
    sandbox3d_editor_font_candidate* out_candidate)
{
    if (out_candidate == NULL)
    {
        return false;
    }
    *out_candidate = SANDBOX3D_EDITOR_FONT_CANDIDATE_ATKINSON;
    if (!automation_input_owned || !automation_diagnostics_enabled ||
        requested_alias == NULL || requested_alias[0] == '\0' ||
        strcmp(requested_alias, "atkinson") == 0)
    {
        return true;
    }
    if (strcmp(requested_alias, "noto") == 0)
    {
        *out_candidate = SANDBOX3D_EDITOR_FONT_CANDIDATE_NOTO_SANS;
        return true;
    }
    return false;
}

const sandbox3d_editor_font_file_asset_desc*
sandbox3d_editor_font_candidate_get_asset(
    sandbox3d_editor_font_candidate candidate)
{
    switch (candidate)
    {
        case SANDBOX3D_EDITOR_FONT_CANDIDATE_ATKINSON:
        case SANDBOX3D_EDITOR_FONT_CANDIDATE_NOTO_SANS:
            return &g_editor_font_candidates[(size_t)candidate];
        default:
            return NULL;
    }
}

const char* sandbox3d_editor_font_candidate_get_alias(
    sandbox3d_editor_font_candidate candidate)
{
    switch (candidate)
    {
        case SANDBOX3D_EDITOR_FONT_CANDIDATE_ATKINSON:
            return "atkinson";
        case SANDBOX3D_EDITOR_FONT_CANDIDATE_NOTO_SANS:
            return "noto";
        default:
            return "unknown";
    }
}

static void sandbox3d_editor_font_bootstrap_set_diagnostic(
    henka_ui_font_diagnostic* diagnostic,
    henka_ui_font_stage stage,
    const char* identity,
    const char* message)
{
    size_t identity_length = 0U;
    size_t message_length = 0U;

    if (diagnostic == NULL)
    {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->stage = stage;
    if (identity != NULL)
    {
        identity_length = strlen(identity);
        if (identity_length > HENKA_UI_FONT_MAX_IDENTITY_BYTES)
        {
            identity_length = HENKA_UI_FONT_MAX_IDENTITY_BYTES;
        }
        memcpy(diagnostic->identity, identity, identity_length);
        diagnostic->identity[identity_length] = '\0';
    }
    if (message != NULL)
    {
        message_length = strlen(message);
        if (message_length >= sizeof(diagnostic->message))
        {
            message_length = sizeof(diagnostic->message) - 1U;
        }
        memcpy(diagnostic->message, message, message_length);
        diagnostic->message[message_length] = '\0';
    }
}

henka_result sandbox3d_editor_font_bootstrap_create(
    const sandbox3d_editor_font_asset_desc* asset,
    henka_ui_font_resource** out_resource,
    henka_ui_font_diagnostic* out_diagnostic)
{
    return henka_ui_font_resource_create_from_asset(asset, out_resource, out_diagnostic);
}

henka_result sandbox3d_editor_font_bootstrap_load(
    const char* asset_base_path,
    const sandbox3d_editor_font_file_asset_desc* asset,
    henka_ui_font_resource** out_resource,
    henka_ui_font_diagnostic* out_diagnostic)
{
    char* resolved_path = NULL;
    FILE* file = NULL;
    long file_size;
    uint8_t* bytes = NULL;
    size_t byte_count;
    henka_result result;
    sandbox3d_editor_font_asset_desc memory_asset;

    if (out_resource != NULL && *out_resource != NULL)
    {
        sandbox3d_editor_font_bootstrap_set_diagnostic(
            out_diagnostic, HENKA_UI_FONT_STAGE_ARGUMENT,
            asset != NULL ? asset->asset_identity : NULL,
            "Font resource output must be empty before loading.");
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (asset_base_path == NULL || asset == NULL || asset->asset_path == NULL ||
        out_resource == NULL)
    {
        sandbox3d_editor_font_bootstrap_set_diagnostic(
            out_diagnostic, HENKA_UI_FONT_STAGE_ARGUMENT,
            asset != NULL ? asset->asset_identity : NULL,
            "Font asset base, descriptor, path, and empty output are required.");
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_resource = NULL;
    sandbox3d_editor_font_bootstrap_set_diagnostic(
        out_diagnostic, HENKA_UI_FONT_STAGE_NONE, asset->asset_identity, NULL);

    result = henka_assets_resolve_path(asset_base_path, asset->asset_path, &resolved_path);
    if (result != HENKA_SUCCESS || resolved_path == NULL)
    {
        sandbox3d_editor_font_bootstrap_set_diagnostic(
            out_diagnostic, HENKA_UI_FONT_STAGE_ASSET_READ, asset->asset_identity,
            "Font asset path could not be resolved beneath the product asset root.");
        if (result == HENKA_SUCCESS)
        {
            result = HENKA_ERROR_ASSET_SOURCE;
        }
        goto cleanup;
    }

#if defined(_MSC_VER)
    if (fopen_s(&file, resolved_path, "rb") != 0 || file == NULL)
#else
    file = fopen(resolved_path, "rb");
    if (file == NULL)
#endif
    {
        result = HENKA_ERROR_ASSET_SOURCE;
        sandbox3d_editor_font_bootstrap_set_diagnostic(
            out_diagnostic, HENKA_UI_FONT_STAGE_ASSET_READ, asset->asset_identity,
            "Font asset could not be opened.");
        goto cleanup;
    }
    if (fseek(file, 0L, SEEK_END) != 0)
    {
        result = HENKA_ERROR_ASSET_SOURCE;
        sandbox3d_editor_font_bootstrap_set_diagnostic(
            out_diagnostic, HENKA_UI_FONT_STAGE_ASSET_READ, asset->asset_identity,
            "Font asset size could not be read.");
        goto cleanup;
    }
    file_size = ftell(file);
    if (file_size <= 0L)
    {
        result = HENKA_ERROR_ASSET_SOURCE;
        sandbox3d_editor_font_bootstrap_set_diagnostic(
            out_diagnostic, HENKA_UI_FONT_STAGE_ASSET_READ, asset->asset_identity,
            "Font asset is empty or its size is unavailable.");
        goto cleanup;
    }
    if ((unsigned long long)file_size > (unsigned long long)HENKA_UI_FONT_MAX_SOURCE_BYTES ||
        (unsigned long long)file_size > (unsigned long long)SIZE_MAX)
    {
        result = HENKA_ERROR_LIMIT;
        sandbox3d_editor_font_bootstrap_set_diagnostic(
            out_diagnostic, HENKA_UI_FONT_STAGE_SOURCE_LIMIT, asset->asset_identity,
            "Font asset exceeds the configured source-byte limit.");
        goto cleanup;
    }
    if (fseek(file, 0L, SEEK_SET) != 0)
    {
        result = HENKA_ERROR_ASSET_SOURCE;
        sandbox3d_editor_font_bootstrap_set_diagnostic(
            out_diagnostic, HENKA_UI_FONT_STAGE_ASSET_READ, asset->asset_identity,
            "Font asset could not be rewound for reading.");
        goto cleanup;
    }

    byte_count = (size_t)file_size;
    bytes = henka_malloc(byte_count);
    if (bytes == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        sandbox3d_editor_font_bootstrap_set_diagnostic(
            out_diagnostic, HENKA_UI_FONT_STAGE_ALLOCATION, asset->asset_identity,
            "Font asset buffer allocation failed.");
        goto cleanup;
    }
    if (fread(bytes, 1U, byte_count, file) != byte_count || ferror(file))
    {
        result = HENKA_ERROR_ASSET_SOURCE;
        sandbox3d_editor_font_bootstrap_set_diagnostic(
            out_diagnostic, HENKA_UI_FONT_STAGE_ASSET_READ, asset->asset_identity,
            "Font asset read was incomplete.");
        goto cleanup;
    }
    if (fclose(file) != 0)
    {
        file = NULL;
        result = HENKA_ERROR_ASSET_SOURCE;
        sandbox3d_editor_font_bootstrap_set_diagnostic(
            out_diagnostic, HENKA_UI_FONT_STAGE_ASSET_READ, asset->asset_identity,
            "Font asset close failed after reading.");
        goto cleanup;
    }
    file = NULL;

    memset(&memory_asset, 0, sizeof(memory_asset));
    memory_asset.bytes = bytes;
    memory_asset.byte_count = byte_count;
    memory_asset.asset_identity = asset->asset_identity;
    memory_asset.expected_sha256_hex = asset->expected_sha256_hex;
    memory_asset.required_codepoints = asset->required_codepoints;
    memory_asset.required_codepoint_count = asset->required_codepoint_count;
    memory_asset.raster_pixel_height = asset->raster_pixel_height;
    memory_asset.atlas_width = asset->atlas_width;
    memory_asset.atlas_height = asset->atlas_height;
    result = sandbox3d_editor_font_bootstrap_create(
        &memory_asset, out_resource, out_diagnostic);

cleanup:
    if (file != NULL)
    {
        fclose(file);
    }
    henka_free(bytes);
    henka_free(resolved_path);
    return result;
}
