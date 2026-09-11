#ifndef HENKA_OPENGL_SCENE_TARGET_POLICY_H
#define HENKA_OPENGL_SCENE_TARGET_POLICY_H

#include <stdbool.h>

typedef struct henka_opengl_scene_target_policy
{
    bool hdr_ready;
    bool hdr_dimensions_match;
    bool bloom_ready;
    bool bloom_dimensions_match;
    bool temporal_ready;
    bool temporal_dimensions_match;
} henka_opengl_scene_target_policy;

bool henka_opengl_scene_target_should_use_hdr(
    bool hdr_requested,
    const henka_opengl_scene_target_policy* policy);

bool henka_opengl_scene_target_requires_sync(
    const henka_opengl_scene_target_policy* policy);

bool henka_opengl_scene_target_requires_hdr_sync(
    const henka_opengl_scene_target_policy* policy);

bool henka_opengl_scene_target_requires_bloom_sync(
    const henka_opengl_scene_target_policy* policy);

bool henka_opengl_scene_target_requires_temporal_sync(
    const henka_opengl_scene_target_policy* policy);

#endif
