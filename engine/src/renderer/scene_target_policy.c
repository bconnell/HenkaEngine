#include <stddef.h>

#include "scene_target_policy.h"

bool henka_opengl_scene_target_requires_sync(
    const henka_opengl_scene_target_policy* policy)
{
    return henka_opengl_scene_target_requires_hdr_sync(policy) ||
        henka_opengl_scene_target_requires_bloom_sync(policy) ||
        henka_opengl_scene_target_requires_temporal_sync(policy);
}

bool henka_opengl_scene_target_requires_hdr_sync(
    const henka_opengl_scene_target_policy* policy)
{
    return policy == NULL ||
        !policy->hdr_ready ||
        !policy->hdr_dimensions_match;
}

bool henka_opengl_scene_target_requires_bloom_sync(
    const henka_opengl_scene_target_policy* policy)
{
    return policy == NULL ||
        !policy->bloom_ready ||
        !policy->bloom_dimensions_match;
}

bool henka_opengl_scene_target_requires_temporal_sync(
    const henka_opengl_scene_target_policy* policy)
{
    return policy == NULL ||
        !policy->temporal_ready ||
        !policy->temporal_dimensions_match;
}
