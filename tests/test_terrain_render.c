#include <math.h>

#include <henka/terrain_render.h>

static int test_default_descriptor(void)
{
    henka_terrain_render_desc desc = henka_terrain_render_desc_default();
    return desc.max_resident_chunks > 0U &&
        desc.max_pending_requests > 0U &&
        desc.lod_max_distances[0] < desc.lod_max_distances[1] &&
        desc.lod_max_distances[1] < desc.lod_max_distances[2] &&
        desc.lod_max_distances[2] < desc.lod_max_distances[3] &&
        isfinite(desc.lod_hysteresis) && desc.lod_hysteresis >= 0.0f &&
        desc.lod_hysteresis < 1.0f &&
        desc.material.type == HENKA_MATERIAL_TYPE_VERTEX_COLOR;
}

static int test_invalid_boundaries(void)
{
    henka_terrain_render_desc desc = henka_terrain_render_desc_default();
    henka_terrain_render_runtime* runtime = NULL;
    desc.lod_max_distances[2] = desc.lod_max_distances[1];
    return henka_terrain_render_runtime_create(NULL, NULL, NULL, &desc, &runtime) == HENKA_ERROR_INVALID_ARGUMENT &&
        runtime == NULL;
}

int main(void)
{
    return test_default_descriptor() && test_invalid_boundaries() ? 0 : 1;
}
