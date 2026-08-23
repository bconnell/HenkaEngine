#include "test_suite.h"

#include <stdint.h>

#include <henka/authoring_mesh.h>

#include "../examples/sandbox3d/modeling_selection.h"

void henka_test_sandbox3d_modeling_selection(void)
{
    sandbox3d_modeling_selection_session session;
    sandbox3d_modeling_selection_rect rect;
    const sandbox3d_modeling_selection_candidate candidates[] = {
        {7U, {45.0f, 45.0f}, 0.4f, true, false},
        {3U, {20.0f, 20.0f}, 0.2f, true, true},
        {5U, {30.0f, 30.0f}, 0.3f, false, true},
        {3U, {25.0f, 25.0f}, 0.2f, true, true},
        {11U, {90.0f, 90.0f}, 0.1f, true, true}};
    const uint32_t prior_ids[] = {9U, 3U};
    uint32_t result_ids[8];
    uint32_t active_id;
    size_t result_count;

    sandbox3d_modeling_selection_reset(&session);
    HENKA_TEST_ASSERT(!session.active);
    HENKA_TEST_ASSERT(!session.dragging);
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_operation_from_modifiers(false, false) ==
        SANDBOX3D_MODELING_SELECTION_REPLACE);
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_operation_from_modifiers(true, false) ==
        SANDBOX3D_MODELING_SELECTION_ADD);
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_operation_from_modifiers(false, true) ==
        SANDBOX3D_MODELING_SELECTION_SUBTRACT);
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_operation_from_modifiers(true, true) ==
        SANDBOX3D_MODELING_SELECTION_SUBTRACT);

    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_begin(
            &session,
            (henka_vec2){10.0f, 10.0f},
            100.0f,
            80.0f,
            SANDBOX3D_MODELING_SELECTION_REPLACE) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(session.active);
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_update(
            &session, (henka_vec2){12.0f, 12.0f}, 4.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!session.dragging);
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_update(
            &session, (henka_vec2){60.0f, 100.0f}, 4.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(session.dragging);
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_get_rect(&session, &rect) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(rect.minimum_x, 10.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(rect.minimum_y, 10.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(rect.maximum_x, 60.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(rect.maximum_y, 80.0f, 0.0001f);

    result_count = 99U;
    active_id = 99U;
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_build_result(
            &session,
            candidates,
            sizeof(candidates) / sizeof(candidates[0]),
            false,
            true,
            NULL,
            0U,
            result_ids,
            sizeof(result_ids) / sizeof(result_ids[0]),
            &result_count,
            &active_id) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result_count == 1U);
    HENKA_TEST_ASSERT(result_ids[0] == 3U);
    HENKA_TEST_ASSERT(active_id == 3U);

    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_build_result(
            &session,
            candidates,
            sizeof(candidates) / sizeof(candidates[0]),
            true,
            true,
            NULL,
            0U,
            result_ids,
            sizeof(result_ids) / sizeof(result_ids[0]),
            &result_count,
            &active_id) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result_count == 2U);
    HENKA_TEST_ASSERT(result_ids[0] == 3U);
    HENKA_TEST_ASSERT(result_ids[1] == 7U);
    HENKA_TEST_ASSERT(active_id == 7U);

    session.operation = SANDBOX3D_MODELING_SELECTION_ADD;
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_build_result(
            &session,
            candidates,
            sizeof(candidates) / sizeof(candidates[0]),
            true,
            false,
            prior_ids,
            sizeof(prior_ids) / sizeof(prior_ids[0]),
            result_ids,
            sizeof(result_ids) / sizeof(result_ids[0]),
            &result_count,
            &active_id) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result_count == 4U);
    HENKA_TEST_ASSERT(result_ids[0] == 3U);
    HENKA_TEST_ASSERT(result_ids[1] == 5U);
    HENKA_TEST_ASSERT(result_ids[2] == 7U);
    HENKA_TEST_ASSERT(result_ids[3] == 9U);
    HENKA_TEST_ASSERT(active_id == 7U);

    session.operation = SANDBOX3D_MODELING_SELECTION_SUBTRACT;
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_build_result(
            &session,
            candidates,
            sizeof(candidates) / sizeof(candidates[0]),
            true,
            false,
            prior_ids,
            sizeof(prior_ids) / sizeof(prior_ids[0]),
            result_ids,
            sizeof(result_ids) / sizeof(result_ids[0]),
            &result_count,
            &active_id) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result_count == 1U);
    HENKA_TEST_ASSERT(result_ids[0] == 9U);
    HENKA_TEST_ASSERT(active_id == 9U);

    session.operation = SANDBOX3D_MODELING_SELECTION_REPLACE;
    result_count = 99U;
    active_id = 99U;
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_build_result(
            &session,
            candidates,
            sizeof(candidates) / sizeof(candidates[0]),
            true,
            false,
            NULL,
            0U,
            result_ids,
            2U,
            &result_count,
            &active_id) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(result_count == 0U);
    HENKA_TEST_ASSERT(active_id == HENKA_AUTHORING_INVALID_ID);

    sandbox3d_modeling_selection_reset(&session);
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_selection_begin(
            &session,
            (henka_vec2){-1.0f, 10.0f},
            100.0f,
            80.0f,
            SANDBOX3D_MODELING_SELECTION_REPLACE) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(!session.active);
}
