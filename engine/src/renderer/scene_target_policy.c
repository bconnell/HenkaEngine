#include <stddef.h>

#include "scene_target_policy.h"

bool henka_opengl_scene_target_requires_sync(
    const henka_opengl_scene_target_policy* policy)
{
    if (policy == NULL)
    {
        return true;
    }
    return !policy->hdr_ready ||
        !policy->hdr_dimensions_match ||
        !policy->bloom_ready ||
        !policy->bloom_dimensions_match ||
        !policy->temporal_ready ||
        !policy->temporal_dimensions_match;
}
