#ifndef HENKA_REFLECTION_PROBE_POLICY_H
#define HENKA_REFLECTION_PROBE_POLICY_H

#include <stdbool.h>

#include <henka/scene.h>

bool henka_reflection_probe_desc_equal(
    const henka_scene_reflection_probe_desc* left,
    const henka_scene_reflection_probe_desc* right);

#endif
