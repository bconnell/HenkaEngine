#ifndef HENKA_INPUT_H
#define HENKA_INPUT_H

#include <stdbool.h>

typedef enum henka_key
{
    HENKA_KEY_UNKNOWN = 0,
    HENKA_KEY_ESCAPE,
    HENKA_KEY_W,
    HENKA_KEY_A,
    HENKA_KEY_S,
    HENKA_KEY_D,
    HENKA_KEY_Q,
    HENKA_KEY_E,
    HENKA_KEY_LEFT_SHIFT,
    HENKA_KEY_F1,
    HENKA_KEY_H,
    HENKA_KEY_COUNT
} henka_key;

struct henka_engine;

bool henka_input_is_key_down(const struct henka_engine* engine, henka_key key);
bool henka_input_was_key_pressed(const struct henka_engine* engine, henka_key key);

#endif
