#include "test_suite.h"

#include <string.h>

#include "../examples/sandbox3d/editor_font_bootstrap.h"

void henka_test_sandbox3d_editor_font_bootstrap(void)
{
    sandbox3d_editor_font_candidate candidate =
        SANDBOX3D_EDITOR_FONT_CANDIDATE_NOTO_SANS;
    const sandbox3d_editor_font_file_asset_desc* atkinson;
    const sandbox3d_editor_font_file_asset_desc* noto;

    HENKA_TEST_ASSERT(sandbox3d_editor_font_candidate_resolve(
        NULL, true, true, &candidate));
    HENKA_TEST_ASSERT(candidate == SANDBOX3D_EDITOR_FONT_CANDIDATE_ATKINSON);
    HENKA_TEST_ASSERT(sandbox3d_editor_font_candidate_resolve(
        "noto", false, true, &candidate));
    HENKA_TEST_ASSERT(candidate == SANDBOX3D_EDITOR_FONT_CANDIDATE_ATKINSON);
    HENKA_TEST_ASSERT(sandbox3d_editor_font_candidate_resolve(
        "noto", true, false, &candidate));
    HENKA_TEST_ASSERT(candidate == SANDBOX3D_EDITOR_FONT_CANDIDATE_ATKINSON);
    HENKA_TEST_ASSERT(sandbox3d_editor_font_candidate_resolve(
        "atkinson", true, true, &candidate));
    HENKA_TEST_ASSERT(candidate == SANDBOX3D_EDITOR_FONT_CANDIDATE_ATKINSON);
    HENKA_TEST_ASSERT(sandbox3d_editor_font_candidate_resolve(
        "noto", true, true, &candidate));
    HENKA_TEST_ASSERT(candidate == SANDBOX3D_EDITOR_FONT_CANDIDATE_NOTO_SANS);
    HENKA_TEST_ASSERT(!sandbox3d_editor_font_candidate_resolve(
        "C:/arbitrary/font.ttf", true, true, &candidate));
    HENKA_TEST_ASSERT(candidate == SANDBOX3D_EDITOR_FONT_CANDIDATE_ATKINSON);
    HENKA_TEST_ASSERT(!sandbox3d_editor_font_candidate_resolve(
        "Noto", true, true, &candidate));
    HENKA_TEST_ASSERT(!sandbox3d_editor_font_candidate_resolve(
        "atkinson", true, true, NULL));

    atkinson = sandbox3d_editor_font_candidate_get_asset(
        SANDBOX3D_EDITOR_FONT_CANDIDATE_ATKINSON);
    noto = sandbox3d_editor_font_candidate_get_asset(
        SANDBOX3D_EDITOR_FONT_CANDIDATE_NOTO_SANS);
    HENKA_TEST_ASSERT(atkinson != NULL && noto != NULL);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_editor_font_candidate_get_alias(
            SANDBOX3D_EDITOR_FONT_CANDIDATE_ATKINSON),
        "atkinson") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_editor_font_candidate_get_alias(
            SANDBOX3D_EDITOR_FONT_CANDIDATE_NOTO_SANS),
        "noto") == 0);
    HENKA_TEST_ASSERT(strcmp(
        atkinson->asset_path,
        "assets/ui/fonts/AtkinsonHyperlegible-Regular.ttf") == 0);
    HENKA_TEST_ASSERT(strcmp(
        atkinson->asset_identity,
        "ui/fonts/AtkinsonHyperlegible-Regular.ttf") == 0);
    HENKA_TEST_ASSERT(strcmp(
        atkinson->expected_sha256_hex,
        "7FB917C89019896D0B52EE84B7CBB3304C18CB90B19A62F5E32712BD23E97669") == 0);
    HENKA_TEST_ASSERT(strcmp(
        noto->asset_path,
        "assets/ui/fonts/NotoSans-Regular.ttf") == 0);
    HENKA_TEST_ASSERT(strcmp(
        noto->asset_identity,
        "ui/fonts/NotoSans-Regular.ttf") == 0);
    HENKA_TEST_ASSERT(strcmp(
        noto->expected_sha256_hex,
        "478C558EA716033CD60C03438F628DFA75694DCF6B5F6D505A2F05FD2B4F3823") == 0);
    HENKA_TEST_ASSERT(atkinson->required_codepoints == noto->required_codepoints);
    HENKA_TEST_ASSERT(atkinson->required_codepoint_count ==
        noto->required_codepoint_count);
    HENKA_TEST_ASSERT(atkinson->raster_pixel_height ==
        noto->raster_pixel_height);
    HENKA_TEST_ASSERT(atkinson->atlas_width == noto->atlas_width);
    HENKA_TEST_ASSERT(atkinson->atlas_height == noto->atlas_height);
    HENKA_TEST_ASSERT(sandbox3d_editor_font_candidate_get_asset(
        (sandbox3d_editor_font_candidate)99) == NULL);
}
