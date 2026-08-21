#include "test_suite.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <henka/engine.h>
#include <henka/input.h>

#include "../engine/src/henka_internal.h"

void henka_test_input(void)
{
    henka_engine engine;
    henka_engine_diagnostics diagnostics;
    henka_input_state input;
    henka_tool_window_state tool_window_state;
    henka_window_id chosen_id;
    henka_window_id next_candidate;
    henka_window_id occupied_ids[3];
    henka_result lifecycle_result;
    FILE* automation_file;
    const char* automation_path = "henka_input_automation_test.events";
    int height;
    int width;

    memset(&engine, 0, sizeof(engine));
    engine.exit_requested = true;
    lifecycle_result = henka_engine_begin_run_transition(&engine);
    HENKA_TEST_ASSERT(lifecycle_result == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        engine.run_state ==
        HENKA_ENGINE_RUN_STATE_INITIALIZING);
    HENKA_TEST_ASSERT(!engine.exit_requested);
    HENKA_TEST_ASSERT(
        henka_engine_begin_run_transition(&engine) ==
        HENKA_ERROR_INVALID_ARGUMENT);
    engine.run_state = HENKA_ENGINE_RUN_STATE_RUNNING;
    HENKA_TEST_ASSERT(henka_engine_should_continue_run(&engine));
    engine.exit_requested = true;
    HENKA_TEST_ASSERT(!henka_engine_should_continue_run(&engine));
    henka_engine_finish_run_transition(&engine);
    HENKA_TEST_ASSERT(
        engine.run_state == HENKA_ENGINE_RUN_STATE_STOPPED);
    HENKA_TEST_ASSERT(
        henka_engine_begin_run_transition(&engine) ==
        HENKA_ERROR_INVALID_ARGUMENT);

    memset(&diagnostics, 0x5a, sizeof(diagnostics));
    memset(&input, 0, sizeof(input));
    memset(&tool_window_state, 0, sizeof(tool_window_state));
    tool_window_state.mouse_position = (henka_vec2){12.0f, 34.0f};
    tool_window_state.mouse_wheel_delta = (henka_vec2){1.25f, -2.5f};
    tool_window_state.mouse_left_down = true;
    tool_window_state.mouse_left_pressed = true;
    HENKA_TEST_ASSERT(tool_window_state.mouse_position.x == 12.0f);
    HENKA_TEST_ASSERT(tool_window_state.mouse_position.y == 34.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(tool_window_state.mouse_wheel_delta.x, 1.25f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(tool_window_state.mouse_wheel_delta.y, -2.5f, 0.0001f);
    HENKA_TEST_ASSERT(tool_window_state.mouse_left_down);
    HENKA_TEST_ASSERT(tool_window_state.mouse_left_pressed);
    HENKA_TEST_ASSERT(!tool_window_state.mouse_left_released);
    HENKA_TEST_ASSERT(henka_engine_set_tool_window_position(NULL, 1U, 0, 0) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_engine_set_cursor(NULL, HENKA_CURSOR_DEFAULT) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_engine_set_cursor(&engine, (henka_cursor_shape)99) == HENKA_ERROR_INVALID_ARGUMENT);

    memcpy(engine.input.text_input, "typed", 6U);
    engine.input.text_input_size = 5U;
    HENKA_TEST_ASSERT(strcmp(henka_input_get_text_input(&engine), "typed") == 0);
    HENKA_TEST_ASSERT(henka_input_get_text_input_size(&engine) == 5U);
    henka_input_clear_text_input(&engine);
    HENKA_TEST_ASSERT(henka_input_get_text_input_size(&engine) == 0U);
    HENKA_TEST_ASSERT(henka_input_get_text_input(&engine)[0] == '\0');

    input.keys_down[HENKA_KEY_W] = true;
    input.keys_pressed[HENKA_KEY_W] = true;
    input.mouse_buttons_down[HENKA_MOUSE_BUTTON_LEFT] = true;
    input.mouse_buttons_pressed[HENKA_MOUSE_BUTTON_LEFT] = true;
    input.mouse_delta = (henka_vec2){4.0f, -3.0f};
    input.mouse_wheel_delta = (henka_vec2){1.0f, 2.0f};
    memcpy(input.text_input, "focus", 6U);
    input.text_input_size = 5U;
    henka_platform_release_input_on_focus_loss(&input);
    HENKA_TEST_ASSERT(!input.keys_down[HENKA_KEY_W]);
    HENKA_TEST_ASSERT(!input.keys_pressed[HENKA_KEY_W]);
    HENKA_TEST_ASSERT(input.keys_released[HENKA_KEY_W]);
    HENKA_TEST_ASSERT(!input.mouse_buttons_down[HENKA_MOUSE_BUTTON_LEFT]);
    HENKA_TEST_ASSERT(!input.mouse_buttons_pressed[HENKA_MOUSE_BUTTON_LEFT]);
    HENKA_TEST_ASSERT(input.mouse_buttons_released[HENKA_MOUSE_BUTTON_LEFT]);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(input.mouse_delta.x, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(input.mouse_delta.y, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(input.mouse_wheel_delta.x, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(input.mouse_wheel_delta.y, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT(input.text_input_size == 0U);
    HENKA_TEST_ASSERT(input.text_input[0] == '\0');

    occupied_ids[0] = 2U;
    occupied_ids[1] = 3U;
    occupied_ids[2] = UINT32_MAX;
    HENKA_TEST_ASSERT(henka_platform_choose_tool_window_id(
        2U,
        occupied_ids,
        3U,
        &chosen_id,
        &next_candidate));
    HENKA_TEST_ASSERT(chosen_id == 4U);
    HENKA_TEST_ASSERT(next_candidate == 5U);
    occupied_ids[0] = UINT32_MAX;
    occupied_ids[1] = 2U;
    HENKA_TEST_ASSERT(henka_platform_choose_tool_window_id(
        UINT32_MAX,
        occupied_ids,
        2U,
        &chosen_id,
        &next_candidate));
    HENKA_TEST_ASSERT(chosen_id == 3U);
    HENKA_TEST_ASSERT(next_candidate == 4U);
    chosen_id = 77U;
    next_candidate = 88U;
    HENKA_TEST_ASSERT(!henka_platform_choose_tool_window_id(
        2U,
        NULL,
        1U,
        &chosen_id,
        &next_candidate));
    HENKA_TEST_ASSERT(chosen_id == HENKA_INVALID_WINDOW_ID);
    HENKA_TEST_ASSERT(next_candidate == HENKA_INVALID_WINDOW_ID);

    memset(&tool_window_state, 0x5a, sizeof(tool_window_state));
    HENKA_TEST_ASSERT(henka_engine_get_tool_window_state(
        &engine,
        7U,
        &tool_window_state) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(tool_window_state.id == HENKA_INVALID_WINDOW_ID);
    HENKA_TEST_ASSERT(tool_window_state.native_window_id == 0U);
    HENKA_TEST_ASSERT(!tool_window_state.open);
    HENKA_TEST_ASSERT(!tool_window_state.mouse_left_down);

    HENKA_TEST_ASSERT(henka_engine_get_diagnostics(
        &engine,
        &diagnostics) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!diagnostics.multi_window_available);
    HENKA_TEST_ASSERT(!diagnostics.main_window_focused);
    HENKA_TEST_ASSERT(diagnostics.open_tool_window_count == 0U);

    width = 123;
    height = 456;
    HENKA_TEST_ASSERT(henka_engine_get_window_size(
        &engine,
        &width,
        &height) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(width == 0);
    HENKA_TEST_ASSERT(height == 0);
    width = 123;
    height = 456;
    HENKA_TEST_ASSERT(henka_engine_get_framebuffer_size(
        &engine,
        &width,
        &height) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(width == 0);
    HENKA_TEST_ASSERT(height == 0);

    HENKA_TEST_ASSERT(strcmp(henka_input_action_get_name(HENKA_INPUT_ACTION_MOVE_FORWARD), "Move Forward") == 0);
    HENKA_TEST_ASSERT(henka_input_action_find_by_name("move_forward") == HENKA_INPUT_ACTION_MOVE_FORWARD);
    HENKA_TEST_ASSERT(henka_input_action_find_by_name("Move Forward") == HENKA_INPUT_ACTION_MOVE_FORWARD);
    HENKA_TEST_ASSERT(henka_input_action_find_by_name("toggle-mouse-capture") == HENKA_INPUT_ACTION_TOGGLE_MOUSE_CAPTURE);
    HENKA_TEST_ASSERT(HENKA_KEY_F != HENKA_KEY_UNKNOWN);
    HENKA_TEST_ASSERT(HENKA_KEY_HOME != HENKA_KEY_UNKNOWN);
    HENKA_TEST_ASSERT(HENKA_KEY_UP != HENKA_KEY_UNKNOWN);
    HENKA_TEST_ASSERT(henka_input_key_find_by_name("down") == HENKA_KEY_DOWN);
    HENKA_TEST_ASSERT(HENKA_KEY_LEFT_ALT != HENKA_KEY_UNKNOWN);
    HENKA_TEST_ASSERT(strcmp(henka_input_key_get_name(HENKA_KEY_M), "M") == 0);
    HENKA_TEST_ASSERT(henka_input_key_find_by_name("left_ctrl") == HENKA_KEY_LEFT_CTRL);
    HENKA_TEST_ASSERT(henka_input_mouse_button_find_by_name("Mouse Left") == HENKA_MOUSE_BUTTON_LEFT);
    HENKA_TEST_ASSERT(strcmp(henka_input_action_get_name(HENKA_INPUT_ACTION_MOVE_TOOL), "Move Tool") == 0);
    HENKA_TEST_ASSERT(henka_input_action_find_by_name(NULL) == HENKA_INPUT_ACTION_UNKNOWN);
    HENKA_TEST_ASSERT(henka_input_action_find_by_name("missing") == HENKA_INPUT_ACTION_UNKNOWN);
    HENKA_TEST_ASSERT(strcmp(henka_window_event_route_to_string(HENKA_WINDOW_EVENT_ROUTE_MAIN), "Main") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_window_event_route_to_string(HENKA_WINDOW_EVENT_ROUTE_TOOL), "Tool") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_window_event_route_to_string(HENKA_WINDOW_EVENT_ROUTE_UNKNOWN), "Unknown") == 0);

    HENKA_TEST_ASSERT(henka_input_bind_action_key(
        &engine,
        HENKA_INPUT_ACTION_MOVE_FORWARD,
        HENKA_KEY_W) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_input_get_action_key_binding_count(
        &engine,
        HENKA_INPUT_ACTION_MOVE_FORWARD) == 1U);
    HENKA_TEST_ASSERT(henka_input_get_action_key_binding(
        &engine,
        HENKA_INPUT_ACTION_MOVE_FORWARD,
        0U) == HENKA_KEY_W);

    HENKA_TEST_ASSERT(henka_input_bind_action_key(
        &engine,
        HENKA_INPUT_ACTION_MOVE_FORWARD,
        HENKA_KEY_UNKNOWN) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_input_get_action_key_binding_count(
        &engine,
        HENKA_INPUT_ACTION_MOVE_FORWARD) == 1U);
    HENKA_TEST_ASSERT(henka_input_get_action_key_binding(
        &engine,
        HENKA_INPUT_ACTION_MOVE_FORWARD,
        0U) == HENKA_KEY_W);

    HENKA_TEST_ASSERT(henka_input_bind_action_mouse_button(
        &engine,
        HENKA_INPUT_ACTION_SELECT_TOOL,
        HENKA_MOUSE_BUTTON_RIGHT) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_input_bind_action_mouse_button(
        &engine,
        HENKA_INPUT_ACTION_SELECT_TOOL,
        HENKA_MOUSE_BUTTON_UNKNOWN) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_input_get_action_mouse_button_binding_count(
        &engine,
        HENKA_INPUT_ACTION_SELECT_TOOL) == 1U);
    HENKA_TEST_ASSERT(henka_input_get_action_mouse_button_binding(
        &engine,
        HENKA_INPUT_ACTION_SELECT_TOOL,
        0U) == HENKA_MOUSE_BUTTON_RIGHT);

    memset(&input, 0, sizeof(input));
    HENKA_TEST_ASSERT(!input.automation_input_owned);
#if defined(_WIN32)
    HENKA_TEST_ASSERT(fopen_s(&automation_file, automation_path, "wb") == 0);
#else
    automation_file = fopen(automation_path, "wb");
    HENKA_TEST_ASSERT(automation_file != NULL);
#endif
    HENKA_TEST_ASSERT(automation_file != NULL);
    HENKA_TEST_ASSERT(fputs("move 1 2\n", automation_file) >= 0);
    HENKA_TEST_ASSERT(fclose(automation_file) == 0);
    HENKA_TEST_ASSERT(henka_input_automation_begin(
        &input,
        automation_path));
    HENKA_TEST_ASSERT(input.automation_input_owned);
    HENKA_TEST_ASSERT(henka_input_automation_apply_event(
        &input,
        "move 123.0 234.0"));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(input.mouse_position.x, 123.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(input.mouse_position.y, 234.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_input_automation_apply_event(
        &input,
        "wheel 0.0 -1.0"));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(input.mouse_wheel_delta.y, -1.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_input_automation_apply_event(
        &input,
        "button left down 123.0 234.0"));
    HENKA_TEST_ASSERT(input.mouse_buttons_down[HENKA_MOUSE_BUTTON_LEFT]);
    HENKA_TEST_ASSERT(input.mouse_buttons_pressed[HENKA_MOUSE_BUTTON_LEFT]);
    HENKA_TEST_ASSERT(henka_input_automation_apply_event(
        &input,
        "button left up 123.0 234.0"));
    HENKA_TEST_ASSERT(!input.mouse_buttons_down[HENKA_MOUSE_BUTTON_LEFT]);
    HENKA_TEST_ASSERT(input.mouse_buttons_released[HENKA_MOUSE_BUTTON_LEFT]);
    input.mouse_position = (henka_vec2){123.0f, 234.0f};
    input.mouse_delta = (henka_vec2){0.0f, 0.0f};
    input.mouse_buttons_released[HENKA_MOUSE_BUTTON_LEFT] = false;
    HENKA_TEST_ASSERT(!henka_input_automation_apply_event(
        &input,
        "button left sideways 999.0 888.0"));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(input.mouse_position.x, 123.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(input.mouse_position.y, 234.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(input.mouse_delta.x, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(input.mouse_delta.y, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT(!input.mouse_buttons_down[HENKA_MOUSE_BUTTON_LEFT]);
    HENKA_TEST_ASSERT(!input.mouse_buttons_released[HENKA_MOUSE_BUTTON_LEFT]);
    HENKA_TEST_ASSERT(henka_input_automation_apply_event(
        &input,
        "key F5 down"));
    HENKA_TEST_ASSERT(input.keys_pressed[HENKA_KEY_F5]);
    HENKA_TEST_ASSERT(!henka_input_automation_apply_event(
        &input,
        "move 1.0 1.0 trailing"));
    HENKA_TEST_ASSERT(!henka_input_automation_apply_event(
        &input,
        "wheel 0.0 1025.0"));
    HENKA_TEST_ASSERT(!henka_input_automation_apply_event(
        &input,
        "key F5 down trailing"));
    henka_input_automation_release(&input);
    HENKA_TEST_ASSERT(!input.automation_input_owned);
    HENKA_TEST_ASSERT(input.automation_input_path[0] == '\0');
    HENKA_TEST_ASSERT(!henka_input_automation_apply_event(
        &input,
        "move 1.0 1.0"));
    HENKA_TEST_ASSERT(remove(automation_path) == 0);
}
