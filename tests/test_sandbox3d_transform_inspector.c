#include "test_suite.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../examples/sandbox3d/transform_inspector.h"

static float henka_test_transform_inspector_quaternion_dot(
    henka_quat left,
    henka_quat right)
{
    return left.x * right.x + left.y * right.y +
        left.z * right.z + left.w * right.w;
}

static void henka_test_transform_inspector_candidate_conversion(void)
{
    sandbox3d_transform_inspector inspector;
    henka_transform original = henka_transform_identity();
    henka_transform candidate;
    bool refreshed = false;

    original.position = (henka_vec3){1.25f, -2.5f, 3.75f};
    original.rotation = henka_quat_from_euler(-0.61f, 1.22f, 0.37f);
    original.scale = (henka_vec3){1.5f, 0.75f, 2.0f};
    sandbox3d_transform_inspector_init(&inspector);
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_sync(
            &inspector,
            (henka_entity)42U,
            original,
            &refreshed) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!refreshed);
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_build_candidate(
            &inspector,
            &candidate) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(candidate.position.x, original.position.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(candidate.position.y, original.position.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(candidate.position.z, original.position.z, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(candidate.scale.x, original.scale.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(candidate.scale.y, original.scale.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(candidate.scale.z, original.scale.z, 0.0001f);
    HENKA_TEST_ASSERT(
        fabsf(henka_test_transform_inspector_quaternion_dot(
            candidate.rotation,
            original.rotation)) > 0.99999f);
    HENKA_TEST_ASSERT(
        !sandbox3d_transform_inspector_candidate_changes(
            &inspector,
            candidate));

    snprintf(inspector.fields[6], sizeof(inspector.fields[6]), "%s", "1.25");
    inspector.dirty = true;
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_build_candidate(
            &inspector,
            &candidate) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(candidate.position.x == original.position.x);
    HENKA_TEST_ASSERT(candidate.position.y == original.position.y);
    HENKA_TEST_ASSERT(candidate.position.z == original.position.z);
    HENKA_TEST_ASSERT(candidate.rotation.x == original.rotation.x);
    HENKA_TEST_ASSERT(candidate.rotation.y == original.rotation.y);
    HENKA_TEST_ASSERT(candidate.rotation.z == original.rotation.z);
    HENKA_TEST_ASSERT(candidate.rotation.w == original.rotation.w);
    HENKA_TEST_ASSERT(candidate.scale.x == 1.25f);

    snprintf(inspector.fields[0], sizeof(inspector.fields[0]), "%s", "12.5");
    snprintf(inspector.fields[3], sizeof(inspector.fields[3]), "%s", "20");
    snprintf(inspector.fields[4], sizeof(inspector.fields[4]), "%s", "-15");
    snprintf(inspector.fields[5], sizeof(inspector.fields[5]), "%s", "90");
    snprintf(inspector.fields[6], sizeof(inspector.fields[6]), "%s", "1.25");
    inspector.dirty = true;
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_build_candidate(
            &inspector,
            &candidate) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(candidate.position.x, 12.5f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(candidate.scale.x, 1.25f, 0.0001f);
    {
        const henka_quat expected = henka_quat_from_euler(
            -15.0f * (3.14159265358979323846f / 180.0f),
            90.0f * (3.14159265358979323846f / 180.0f),
            20.0f * (3.14159265358979323846f / 180.0f));
        HENKA_TEST_ASSERT(
            fabsf(henka_test_transform_inspector_quaternion_dot(
                candidate.rotation,
                expected)) > 0.99999f);
    }
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_candidate_changes(
            &inspector,
            candidate));
}

static void henka_test_transform_inspector_rejects_invalid_values(void)
{
    sandbox3d_transform_inspector inspector;
    henka_transform candidate;

    sandbox3d_transform_inspector_init(&inspector);
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_sync(
            &inspector,
            (henka_entity)7U,
            henka_transform_identity(),
            NULL) == HENKA_SUCCESS);
    snprintf(inspector.fields[0], sizeof(inspector.fields[0]), "%s", "nan");
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_build_candidate(
            &inspector,
            &candidate) == HENKA_ERROR_NUMERIC_RANGE);
    snprintf(inspector.fields[0], sizeof(inspector.fields[0]), "%s", "1.0oops");
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_build_candidate(
            &inspector,
            &candidate) == HENKA_ERROR_INVALID_ARGUMENT);
    snprintf(inspector.fields[0], sizeof(inspector.fields[0]), "%s", "1e999");
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_build_candidate(
            &inspector,
            &candidate) == HENKA_ERROR_NUMERIC_RANGE);
}

static void henka_test_transform_inspector_rebases_external_changes(void)
{
    sandbox3d_transform_inspector inspector;
    henka_transform external = henka_transform_identity();
    bool refreshed = false;

    sandbox3d_transform_inspector_init(&inspector);
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_sync(
            &inspector,
            (henka_entity)8U,
            external,
            NULL) == HENKA_SUCCESS);
    snprintf(inspector.fields[0], sizeof(inspector.fields[0]), "%s", "99");
    inspector.dirty = true;
    external.position.x = 4.0f;
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_sync(
            &inspector,
            (henka_entity)8U,
            external,
            &refreshed) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(refreshed);
    HENKA_TEST_ASSERT(!sandbox3d_transform_inspector_has_pending_edits(&inspector));
    HENKA_TEST_ASSERT(strcmp(inspector.fields[0], "4") == 0);

    snprintf(inspector.fields[0], sizeof(inspector.fields[0]), "%s", "99");
    inspector.dirty = true;
    external.position.x += 0.000001f;
    refreshed = false;
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_sync(
            &inspector,
            (henka_entity)8U,
            external,
            &refreshed) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(refreshed);
    HENKA_TEST_ASSERT(!sandbox3d_transform_inspector_has_pending_edits(&inspector));

    snprintf(inspector.fields[0], sizeof(inspector.fields[0]), "%s", "99");
    inspector.dirty = true;
    external.rotation.x = -external.rotation.x;
    external.rotation.y = -external.rotation.y;
    external.rotation.z = -external.rotation.z;
    external.rotation.w = -external.rotation.w;
    refreshed = true;
    HENKA_TEST_ASSERT(
        sandbox3d_transform_inspector_sync(
            &inspector,
            (henka_entity)8U,
            external,
            &refreshed) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!refreshed);
    HENKA_TEST_ASSERT(strcmp(inspector.fields[0], "99") == 0);
}

static void henka_test_transform_inspector_layout_bounds(void)
{
    const float widths[] = {242.0f, 280.0f, 360.0f, 640.0f};
    size_t width_index;

    for (width_index = 0U;
        width_index < sizeof(widths) / sizeof(widths[0]);
        ++width_index)
    {
        const henka_ui_rect bounds = {
            16.0f, 24.0f, widths[width_index], 28.0f};
        henka_ui_rect label;
        henka_ui_rect fields[3];
        size_t field_index;

        HENKA_TEST_ASSERT(
            sandbox3d_transform_inspector_layout_row(
                bounds,
                &label,
                fields) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(label.x >= bounds.x);
        HENKA_TEST_ASSERT(label.x + label.width <= fields[0].x);
        for (field_index = 0U; field_index < 3U; ++field_index)
        {
            HENKA_TEST_ASSERT(fields[field_index].x >= bounds.x);
            HENKA_TEST_ASSERT(fields[field_index].y >= bounds.y);
            HENKA_TEST_ASSERT(fields[field_index].x + fields[field_index].width <=
                bounds.x + bounds.width + 0.001f);
            HENKA_TEST_ASSERT(fields[field_index].y + fields[field_index].height <=
                bounds.y + bounds.height + 0.001f);
            if (field_index > 0U)
            {
                HENKA_TEST_ASSERT(fields[field_index - 1U].x +
                    fields[field_index - 1U].width <= fields[field_index].x);
            }
        }
    }
    {
        henka_ui_rect label;
        henka_ui_rect fields[3];
        HENKA_TEST_ASSERT(
            sandbox3d_transform_inspector_layout_row(
                (henka_ui_rect){0.0f, 0.0f, 241.0f, 28.0f},
                &label,
                fields) == HENKA_ERROR_LIMIT);
        HENKA_TEST_ASSERT(
            sandbox3d_transform_inspector_layout_row(
                (henka_ui_rect){0.0f, 0.0f, 280.0f, 23.0f},
                &label,
                fields) == HENKA_ERROR_LIMIT);
    }
}

static void henka_test_transform_inspector_action_layout(void)
{
    const float widths[] = {242.0f, 280.0f, 640.0f};
    size_t width_index;

    for (width_index = 0U;
        width_index < sizeof(widths) / sizeof(widths[0]);
        ++width_index)
    {
        const henka_ui_rect bounds = {
            12.0f, 36.0f, widths[width_index], 28.0f};
        henka_ui_rect label;
        henka_ui_rect apply;
        henka_ui_rect revert;

        HENKA_TEST_ASSERT(
            sandbox3d_transform_inspector_layout_actions(
                bounds, true, &label, &apply, &revert) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(label.x == bounds.x);
        HENKA_TEST_ASSERT(label.width > 0.0f);
        HENKA_TEST_ASSERT(label.x + label.width <= apply.x);
        HENKA_TEST_ASSERT(apply.x + apply.width < revert.x);
        HENKA_TEST_ASSERT(revert.x + revert.width <= bounds.x + bounds.width);
        HENKA_TEST_ASSERT(apply.y == bounds.y && revert.y == bounds.y);
        HENKA_TEST_ASSERT(apply.height == 24.0f && revert.height == 24.0f);
    }

    {
        henka_ui_rect label;
        henka_ui_rect apply;
        henka_ui_rect revert;
        HENKA_TEST_ASSERT(
            sandbox3d_transform_inspector_layout_actions(
                (henka_ui_rect){4.0f, 8.0f, 80.0f, 28.0f},
                false,
                &label,
                &apply,
                &revert) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(label.width == 80.0f);
        HENKA_TEST_ASSERT(apply.width == 0.0f && revert.width == 0.0f);
        HENKA_TEST_ASSERT(
            sandbox3d_transform_inspector_layout_actions(
                (henka_ui_rect){4.0f, 8.0f, 241.0f, 28.0f},
                true,
                &label,
                &apply,
                &revert) == HENKA_ERROR_LIMIT);
        HENKA_TEST_ASSERT(apply.width == 0.0f && revert.width == 0.0f);
    }
}

void henka_test_sandbox3d_transform_inspector(void)
{
    henka_test_transform_inspector_candidate_conversion();
    henka_test_transform_inspector_rejects_invalid_values();
    henka_test_transform_inspector_rebases_external_changes();
    henka_test_transform_inspector_layout_bounds();
    henka_test_transform_inspector_action_layout();
}
