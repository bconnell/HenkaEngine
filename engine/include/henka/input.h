#ifndef HENKA_INPUT_H
#define HENKA_INPUT_H

#include <stdbool.h>

#include <henka/math.h>
#include <henka/result.h>

typedef enum henka_key
{
    HENKA_KEY_UNKNOWN = 0,
    HENKA_KEY_ESCAPE,
    HENKA_KEY_F,
    HENKA_KEY_W,
    HENKA_KEY_A,
    HENKA_KEY_S,
    HENKA_KEY_D,
    HENKA_KEY_Q,
    HENKA_KEY_E,
    HENKA_KEY_LEFT_ALT,
    HENKA_KEY_LEFT_SHIFT,
    HENKA_KEY_HOME,
    HENKA_KEY_TAB,
    HENKA_KEY_F1,
    HENKA_KEY_F2,
    HENKA_KEY_F3,
    HENKA_KEY_F4,
    HENKA_KEY_F5,
    HENKA_KEY_H,
    HENKA_KEY_COUNT
} henka_key;

typedef enum henka_mouse_button
{
    HENKA_MOUSE_BUTTON_UNKNOWN = 0,
    HENKA_MOUSE_BUTTON_LEFT,
    HENKA_MOUSE_BUTTON_RIGHT,
    HENKA_MOUSE_BUTTON_MIDDLE,
    HENKA_MOUSE_BUTTON_COUNT
} henka_mouse_button;

typedef enum henka_input_action
{
    HENKA_INPUT_ACTION_UNKNOWN = 0,
    HENKA_INPUT_ACTION_MOVE_FORWARD,
    HENKA_INPUT_ACTION_MOVE_BACK,
    HENKA_INPUT_ACTION_MOVE_LEFT,
    HENKA_INPUT_ACTION_MOVE_RIGHT,
    HENKA_INPUT_ACTION_MOVE_UP,
    HENKA_INPUT_ACTION_MOVE_DOWN,
    HENKA_INPUT_ACTION_INTERACT,
    HENKA_INPUT_ACTION_OPEN_PANELS,
    HENKA_INPUT_ACTION_CHANGE_LAYOUT,
    HENKA_INPUT_ACTION_TOGGLE_MOUSE_CAPTURE,
    HENKA_INPUT_ACTION_COUNT
} henka_input_action;

struct henka_engine;

bool henka_input_is_key_down(const struct henka_engine* engine, henka_key key);
bool henka_input_was_key_pressed(const struct henka_engine* engine, henka_key key);
bool henka_input_was_key_released(const struct henka_engine* engine, henka_key key);
bool henka_input_is_mouse_button_down(const struct henka_engine* engine, henka_mouse_button button);
bool henka_input_was_mouse_button_pressed(const struct henka_engine* engine, henka_mouse_button button);
bool henka_input_was_mouse_button_released(const struct henka_engine* engine, henka_mouse_button button);
henka_vec2 henka_input_get_mouse_position(const struct henka_engine* engine);
henka_vec2 henka_input_get_mouse_delta(const struct henka_engine* engine);
henka_vec2 henka_input_get_mouse_wheel_delta(const struct henka_engine* engine);
const char* henka_input_action_get_name(henka_input_action action);
henka_input_action henka_input_action_find_by_name(const char* name);
henka_result henka_input_bind_action_key(struct henka_engine* engine, henka_input_action action, henka_key key);
henka_result henka_input_bind_action_mouse_button(struct henka_engine* engine, henka_input_action action, henka_mouse_button button);
bool henka_input_action_is_down(const struct henka_engine* engine, henka_input_action action);
bool henka_input_action_was_pressed(const struct henka_engine* engine, henka_input_action action);
bool henka_input_action_was_released(const struct henka_engine* engine, henka_input_action action);

#endif
