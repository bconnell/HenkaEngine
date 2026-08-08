#include <math.h>

#include <henka/memory.h>
#include <henka/terrain_physics.h>

static int test_transactional_patch_ownership_and_sampling(void)
{
    henka_terrain_physics_desc desc = henka_terrain_physics_desc_default();
    henka_terrain_physics* physics = NULL;
    henka_terrain_physics_patch_desc patch = {0};
    henka_terrain_physics_hit hit;
    henka_terrain_physics_stats stats;
    int32_t* heights = NULL;
    uint32_t index;
    int result = 0;

    desc.max_patches = 2U;
    heights = henka_calloc(HENKA_TERRAIN_COLLISION_PATCH_SAMPLES, sizeof(*heights));
    if (heights == NULL || henka_terrain_physics_create(&desc, &physics) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < HENKA_TERRAIN_COLLISION_PATCH_SAMPLES; ++index)
    {
        heights[index] = 1000;
    }
    patch.patch = (henka_terrain_collision_patch){{0, 0}, 3U, 4U,
        HENKA_TERRAIN_COLLISION_PATCH_EDGE, heights};
    patch.sample_spacing_meters = 1.0F;
    patch.origin_x_meters = 0.0F;
    patch.origin_z_meters = 0.0F;
    if (henka_terrain_physics_replace_patch(physics, &patch) != HENKA_SUCCESS ||
        henka_terrain_physics_sample(physics, 2.5F, 3.5F, &hit) != HENKA_SUCCESS ||
        !hit.hit || hit.chunk_id.x != 0 || hit.revision != 3U || hit.generation != 4U ||
        fabsf(hit.height_meters - 1.0F) > 0.0001F || fabsf(hit.normal.y - 1.0F) > 0.0001F)
    {
        goto cleanup;
    }
    heights[0] = 2000;
    patch.patch.revision = 5U;
    if (henka_terrain_physics_replace_patch(physics, &patch) != HENKA_SUCCESS ||
        henka_terrain_physics_sample(physics, 0.0F, 0.0F, &hit) != HENKA_SUCCESS ||
        !hit.hit || fabsf(hit.height_meters - 2.0F) > 0.0001F)
    {
        goto cleanup;
    }
    henka_terrain_physics_get_stats(physics, &stats);
    if (stats.resident_patch_count != 1U || stats.replacement_count != 2U)
    {
        goto cleanup;
    }
    if (henka_terrain_physics_sample(physics, 1000.0F, 1000.0F, &hit) != HENKA_SUCCESS || hit.hit ||
        stats.missed_query_count != 0U)
    {
        /* A miss is expected; refresh the diagnostics before checking it. */
        henka_terrain_physics_get_stats(physics, &stats);
        if (hit.hit || stats.missed_query_count != 1U)
        {
            goto cleanup;
        }
    }
    result = 1;

cleanup:
    henka_terrain_physics_destroy(physics);
    henka_free(heights);
    return result;
}

int main(void)
{
    return test_transactional_patch_ownership_and_sampling() ? 0 : 1;
}
