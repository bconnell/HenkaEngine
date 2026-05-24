#ifndef HENKA_INPUT_H
#define HENKA_INPUT_H

#include <stdbool.h>

typedef enum henka_key
{
    HENKA_KEY_UNKNOWN = 0,
    HENKA_KEY_ESCAPE,
    HENKA_KEY_COUNT
} henka_key;

struct henka_engine;

bool henka_input_is_key_down(const struct henka_engine* engine, henka_key key);
bool henka_input_was_key_pressed(const struct henka_engine* engine, henka_key key);

#endif
