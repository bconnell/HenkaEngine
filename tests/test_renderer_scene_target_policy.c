#include <stdio.h>

#include "../engine/src/renderer/scene_target_policy.h"

static henka_opengl_scene_target_policy valid_policy(void)
{
    return (henka_opengl_scene_target_policy){
        true,
        true,
        true,
        true,
        true,
        true};
}

int main(void)
{
    henka_opengl_scene_target_policy policy = valid_policy();

    if (!henka_opengl_scene_target_requires_hdr_sync(NULL) ||
        !henka_opengl_scene_target_requires_bloom_sync(NULL) ||
        !henka_opengl_scene_target_requires_temporal_sync(NULL))
    {
        fprintf(stderr, "null scene-target policy did not fail closed for each target\n");
        return 1;
    }

    if (henka_opengl_scene_target_requires_sync(&policy))
    {
        fprintf(stderr, "ready scene targets incorrectly require synchronization\n");
        return 1;
    }

    if (!henka_opengl_scene_target_should_use_hdr(true, &policy) ||
        henka_opengl_scene_target_should_use_hdr(false, &policy))
    {
        fprintf(stderr, "HDR presentation request did not follow the ready target policy\n");
        return 1;
    }

    if (henka_opengl_scene_target_requires_hdr_sync(&policy) ||
        henka_opengl_scene_target_requires_bloom_sync(&policy) ||
        henka_opengl_scene_target_requires_temporal_sync(&policy))
    {
        fprintf(stderr, "ready scene targets incorrectly require per-target synchronization\n");
        return 1;
    }

    policy.bloom_ready = false;
    if (!henka_opengl_scene_target_requires_sync(&policy))
    {
        fprintf(stderr, "unavailable bloom target did not require synchronization\n");
        return 1;
    }
    if (henka_opengl_scene_target_requires_hdr_sync(&policy) ||
        !henka_opengl_scene_target_requires_bloom_sync(&policy) ||
        henka_opengl_scene_target_requires_temporal_sync(&policy))
    {
        fprintf(stderr, "bloom retry did not stay isolated to the unavailable target\n");
        return 1;
    }

    policy = valid_policy();
    policy.bloom_dimensions_match = false;
    if (!henka_opengl_scene_target_requires_sync(&policy))
    {
        fprintf(stderr, "stale bloom target dimensions did not require synchronization\n");
        return 1;
    }
    if (henka_opengl_scene_target_requires_hdr_sync(&policy) ||
        !henka_opengl_scene_target_requires_bloom_sync(&policy) ||
        henka_opengl_scene_target_requires_temporal_sync(&policy))
    {
        fprintf(stderr, "bloom dimension retry did not stay isolated to the unavailable target\n");
        return 1;
    }

    policy = valid_policy();
    policy.hdr_ready = false;
    if (!henka_opengl_scene_target_requires_sync(&policy))
    {
        fprintf(stderr, "unavailable HDR target did not require synchronization\n");
        return 1;
    }
    if (!henka_opengl_scene_target_requires_hdr_sync(&policy) ||
        henka_opengl_scene_target_requires_bloom_sync(&policy) ||
        henka_opengl_scene_target_requires_temporal_sync(&policy))
    {
        fprintf(stderr, "HDR retry did not stay isolated to the unavailable target\n");
        return 1;
    }
    if (henka_opengl_scene_target_should_use_hdr(true, &policy))
    {
        fprintf(stderr, "unavailable HDR target did not select direct framebuffer fallback\n");
        return 1;
    }

    policy = valid_policy();
    policy.hdr_dimensions_match = false;
    if (henka_opengl_scene_target_should_use_hdr(true, &policy))
    {
        fprintf(stderr, "stale HDR dimensions did not select direct framebuffer fallback\n");
        return 1;
    }

    if (henka_opengl_scene_target_should_use_hdr(true, NULL))
    {
        fprintf(stderr, "null HDR target policy did not fail closed to direct framebuffer fallback\n");
        return 1;
    }

    policy = valid_policy();
    policy.temporal_dimensions_match = false;
    if (!henka_opengl_scene_target_requires_sync(&policy))
    {
        fprintf(stderr, "stale temporal target dimensions did not require synchronization\n");
        return 1;
    }
    if (henka_opengl_scene_target_requires_hdr_sync(&policy) ||
        henka_opengl_scene_target_requires_bloom_sync(&policy) ||
        !henka_opengl_scene_target_requires_temporal_sync(&policy))
    {
        fprintf(stderr, "temporal retry did not stay isolated to the unavailable target\n");
        return 1;
    }

    puts("scene target synchronization policy passed");
    return 0;
}
