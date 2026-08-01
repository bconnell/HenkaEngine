#include "test_suite.h"

#include <string.h>

#include <henka/engine.h>

#include "../engine/src/henka_internal.h"

static void henka_test_viewport_shading_labels_and_parse(void)
{
    henka_viewport_shading_mode mode;

    HENKA_TEST_ASSERT(
        henka_viewport_shading_mode_is_valid(
            HENKA_VIEWPORT_SHADING_WIREFRAME));
    HENKA_TEST_ASSERT(
        henka_viewport_shading_mode_is_valid(
            HENKA_VIEWPORT_SHADING_SOLID));
    HENKA_TEST_ASSERT(
        henka_viewport_shading_mode_is_valid(
            HENKA_VIEWPORT_SHADING_MATERIAL_PREVIEW));
    HENKA_TEST_ASSERT(
        henka_viewport_shading_mode_is_valid(
            HENKA_VIEWPORT_SHADING_RENDERED));
    HENKA_TEST_ASSERT(
        !henka_viewport_shading_mode_is_valid(
            HENKA_VIEWPORT_SHADING_COUNT));
    HENKA_TEST_ASSERT(
        !henka_viewport_shading_mode_is_valid(
            (henka_viewport_shading_mode)-1));

    HENKA_TEST_ASSERT(
        strcmp(
            henka_viewport_shading_mode_get_label(
                HENKA_VIEWPORT_SHADING_WIREFRAME),
            "Wireframe") == 0);
    HENKA_TEST_ASSERT(
        strcmp(
            henka_viewport_shading_mode_get_label(
                HENKA_VIEWPORT_SHADING_MATERIAL_PREVIEW),
            "Material Preview") == 0);
    HENKA_TEST_ASSERT(
        strcmp(
            henka_viewport_shading_mode_get_setting_value(
                HENKA_VIEWPORT_SHADING_RENDERED),
            "rendered") == 0);

    mode = HENKA_VIEWPORT_SHADING_RENDERED;
    HENKA_TEST_ASSERT(
        henka_viewport_shading_mode_parse(
            "wireframe",
            &mode) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        mode == HENKA_VIEWPORT_SHADING_WIREFRAME);
    HENKA_TEST_ASSERT(
        henka_viewport_shading_mode_parse(
            "solid",
            &mode) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        mode == HENKA_VIEWPORT_SHADING_SOLID);
    HENKA_TEST_ASSERT(
        henka_viewport_shading_mode_parse(
            "material_preview",
            &mode) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        mode == HENKA_VIEWPORT_SHADING_MATERIAL_PREVIEW);
    HENKA_TEST_ASSERT(
        henka_viewport_shading_mode_parse(
            "rendered",
            &mode) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        mode == HENKA_VIEWPORT_SHADING_RENDERED);

    mode = HENKA_VIEWPORT_SHADING_RENDERED;
    HENKA_TEST_ASSERT(
        henka_viewport_shading_mode_parse(
            "invalid",
            &mode) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(
        mode == HENKA_VIEWPORT_SHADING_SOLID);
}

static void henka_test_viewport_shading_policy_matrix(void)
{
    henka_viewport_render_policy policy;

    HENKA_TEST_ASSERT(
        henka_viewport_render_policy_resolve(
            HENKA_VIEWPORT_SHADING_WIREFRAME,
            &policy) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(policy.polygon_wireframe);
    HENKA_TEST_ASSERT(!policy.sample_material_texture);
    HENKA_TEST_ASSERT(policy.force_unlit);

    HENKA_TEST_ASSERT(
        henka_viewport_render_policy_resolve(
            HENKA_VIEWPORT_SHADING_SOLID,
            &policy) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!policy.polygon_wireframe);
    HENKA_TEST_ASSERT(!policy.use_material_base_color);
    HENKA_TEST_ASSERT(!policy.sample_material_texture);
    HENKA_TEST_ASSERT(policy.use_preview_lighting);

    HENKA_TEST_ASSERT(
        henka_viewport_render_policy_resolve(
            HENKA_VIEWPORT_SHADING_MATERIAL_PREVIEW,
            &policy) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(policy.use_material_base_color);
    HENKA_TEST_ASSERT(policy.sample_material_texture);
    HENKA_TEST_ASSERT(policy.use_preview_lighting);
    HENKA_TEST_ASSERT(policy.use_hdr_presentation);
    HENKA_TEST_ASSERT(!policy.use_scene_lighting);

    HENKA_TEST_ASSERT(
        henka_viewport_render_policy_resolve(
            HENKA_VIEWPORT_SHADING_RENDERED,
            &policy) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(policy.use_material_base_color);
    HENKA_TEST_ASSERT(policy.sample_material_texture);
    HENKA_TEST_ASSERT(policy.use_scene_lighting);
    HENKA_TEST_ASSERT(policy.use_hdr_presentation);
    HENKA_TEST_ASSERT(!policy.use_preview_lighting);

    HENKA_TEST_ASSERT(
        henka_viewport_render_policy_resolve(
            HENKA_VIEWPORT_SHADING_COUNT,
            &policy) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(
        henka_viewport_render_policy_resolve(
            HENKA_VIEWPORT_SHADING_SOLID,
            NULL) == HENKA_ERROR_INVALID_ARGUMENT);
}

void henka_test_viewport_shading(void)
{
    henka_test_viewport_shading_labels_and_parse();
    henka_test_viewport_shading_policy_matrix();
}
