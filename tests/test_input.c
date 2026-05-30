#include "test_suite.h"

#include <string.h>

#include <henka/engine.h>
#include <henka/input.h>

void henka_test_input(void)
{
    HENKA_TEST_ASSERT(strcmp(henka_input_action_get_name(HENKA_INPUT_ACTION_MOVE_FORWARD), "Move Forward") == 0);
    HENKA_TEST_ASSERT(henka_input_action_find_by_name("move_forward") == HENKA_INPUT_ACTION_MOVE_FORWARD);
    HENKA_TEST_ASSERT(henka_input_action_find_by_name("Move Forward") == HENKA_INPUT_ACTION_MOVE_FORWARD);
    HENKA_TEST_ASSERT(henka_input_action_find_by_name("toggle-mouse-capture") == HENKA_INPUT_ACTION_TOGGLE_MOUSE_CAPTURE);
    HENKA_TEST_ASSERT(HENKA_KEY_F != HENKA_KEY_UNKNOWN);
    HENKA_TEST_ASSERT(HENKA_KEY_HOME != HENKA_KEY_UNKNOWN);
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
}
