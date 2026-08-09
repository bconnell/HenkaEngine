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
        desc.material.type == HENKA_MATERIAL_TYPE_LIT &&
        desc.material.terrain_layers_enabled &&
        desc.material.terrain_layers[0].texture_scale_meters > 0.0f &&
        desc.material.terrain_layers[1].texture_scale_meters > 0.0f &&
        desc.material.terrain_layers[2].texture_scale_meters > 0.0f &&
        desc.material.terrain_layers[3].texture_scale_meters > 0.0f;
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
