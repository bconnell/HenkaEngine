#ifndef HENKA_PHYSICS_INTERNAL_H
#define HENKA_PHYSICS_INTERNAL_H

#include <stddef.h>

#include <henka/physics.h>

/* Read-only internal state used by deterministic transaction tests. */
size_t henka_physics_test_get_current_pair_count(const henka_physics_world* world);
size_t henka_physics_test_get_previous_pair_count(const henka_physics_world* world);
float henka_physics_test_get_accumulator(const henka_physics_world* world);

#endif
