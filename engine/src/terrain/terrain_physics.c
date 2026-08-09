#include <henka/terrain_physics.h>

#include <math.h>
#include <string.h>

#include <henka/memory.h>

typedef struct henka_terrain_physics_patch_record
{
    bool active;
    henka_terrain_physics_patch_desc desc;
    int32_t* heights_millimeters;
} henka_terrain_physics_patch_record;

struct henka_terrain_physics
{
    henka_terrain_physics_desc desc;
    henka_terrain_physics_patch_record* patches;
    uint32_t resident_patch_count;
    henka_terrain_physics_stats stats;
};

henka_terrain_physics_desc henka_terrain_physics_desc_default(void)
{
    return (henka_terrain_physics_desc){16U};
}

henka_result henka_terrain_physics_create(
    const henka_terrain_physics_desc* desc,
    henka_terrain_physics** out_physics)
{
    henka_terrain_physics* physics;
    if (out_physics == NULL || desc == NULL || desc->max_patches == 0U ||
        desc->max_patches > HENKA_TERRAIN_PHYSICS_MAX_PATCHES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_physics = NULL;
    physics = henka_calloc(1U, sizeof(*physics));
    if (physics == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    physics->patches = henka_calloc(desc->max_patches, sizeof(*physics->patches));
    if (physics->patches == NULL)
    {
        henka_free(physics);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    physics->desc = *desc;
    physics->stats.max_patches = desc->max_patches;
    *out_physics = physics;
    return HENKA_SUCCESS;
}

void henka_terrain_physics_destroy(henka_terrain_physics* physics)
{
    uint32_t index;
    if (physics == NULL)
    {
        return;
    }
    for (index = 0U; index < physics->desc.max_patches; ++index)
    {
        henka_free(physics->patches[index].heights_millimeters);
    }
    henka_free(physics->patches);
    henka_free(physics);
}

static uint32_t henka_terrain_physics_find_patch(
    const henka_terrain_physics* physics,
    henka_terrain_chunk_id chunk_id)
{
    uint32_t index;
    for (index = 0U; index < physics->desc.max_patches; ++index)
    {
        if (physics->patches[index].active &&
            henka_terrain_chunk_id_equal(physics->patches[index].desc.patch.chunk_id, chunk_id))
        {
            return index;
        }
    }
    return physics->desc.max_patches;
}

henka_result henka_terrain_physics_replace_patch(
    henka_terrain_physics* physics,
    const henka_terrain_physics_patch_desc* desc)
{
    uint64_t sample_count;
    uint32_t index;
    uint32_t existing_index;
    int32_t* candidate;
    if (physics == NULL || desc == NULL || desc->patch.heights_millimeters == NULL ||
        desc->patch.sample_edge < 2U || desc->patch.sample_edge > HENKA_TERRAIN_COLLISION_PATCH_EDGE ||
        desc->patch.sample_edge > UINT32_MAX / desc->patch.sample_edge ||
        !isfinite(desc->sample_spacing_meters) || desc->sample_spacing_meters <= 0.0F ||
        !isfinite(desc->origin_x_meters) || !isfinite(desc->origin_z_meters))
    {
        if (physics != NULL)
        {
            ++physics->stats.rejected_replacement_count;
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    sample_count = (uint64_t)desc->patch.sample_edge * desc->patch.sample_edge;
    candidate = henka_malloc((size_t)sample_count * sizeof(*candidate));
    if (candidate == NULL)
    {
        ++physics->stats.rejected_replacement_count;
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    memcpy(candidate, desc->patch.heights_millimeters, (size_t)sample_count * sizeof(*candidate));
    existing_index = henka_terrain_physics_find_patch(physics, desc->patch.chunk_id);
    index = existing_index;
    if (index >= physics->desc.max_patches)
    {
        for (index = 0U; index < physics->desc.max_patches; ++index)
        {
            if (!physics->patches[index].active)
            {
                break;
            }
        }
    }
    if (index >= physics->desc.max_patches)
    {
        henka_free(candidate);
        ++physics->stats.rejected_replacement_count;
        return HENKA_ERROR_LIMIT;
    }
    henka_free(physics->patches[index].heights_millimeters);
    physics->patches[index].active = true;
    physics->patches[index].desc = *desc;
    physics->patches[index].desc.patch.heights_millimeters = candidate;
    physics->patches[index].heights_millimeters = candidate;
    if (existing_index >= physics->desc.max_patches)
    {
        ++physics->resident_patch_count;
    }
    ++physics->stats.replacement_count;
    physics->stats.resident_patch_count = physics->resident_patch_count;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_physics_remove_patch(
    henka_terrain_physics* physics,
    henka_terrain_chunk_id chunk_id)
{
    uint32_t index;
    if (physics == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    index = henka_terrain_physics_find_patch(physics, chunk_id);
    if (index >= physics->desc.max_patches)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    henka_free(physics->patches[index].heights_millimeters);
    physics->patches[index] = (henka_terrain_physics_patch_record){0};
    if (physics->resident_patch_count > 0U)
    {
        --physics->resident_patch_count;
    }
    physics->stats.resident_patch_count = physics->resident_patch_count;
    return HENKA_SUCCESS;
}

static bool henka_terrain_physics_patch_contains(
    const henka_terrain_physics_patch_desc* desc,
    float world_x,
    float world_z)
{
    float extent = (float)(desc->patch.sample_edge - 1U) * desc->sample_spacing_meters;
    return world_x >= desc->origin_x_meters && world_x <= desc->origin_x_meters + extent &&
        world_z >= desc->origin_z_meters && world_z <= desc->origin_z_meters + extent;
}

henka_result henka_terrain_physics_sample(
    henka_terrain_physics* physics,
    float world_x_meters,
    float world_z_meters,
    henka_terrain_physics_hit* out_hit)
{
    uint32_t index;
    if (out_hit == NULL || physics == NULL || !isfinite(world_x_meters) || !isfinite(world_z_meters))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_hit = (henka_terrain_physics_hit){0};
    ++physics->stats.sample_query_count;
    for (index = 0U; index < physics->desc.max_patches; ++index)
    {
        const henka_terrain_physics_patch_desc* desc;
        float local_x;
        float local_z;
        float sample_x;
        float sample_z;
        uint32_t x;
        uint32_t z;
        uint32_t edge;
        float h00;
        float h10;
        float h01;
        float h11;
        float t_x;
        float t_z;
        float normal_x;
        float normal_y;
        float normal_z;
        float normal_length;
        if (!physics->patches[index].active)
        {
            continue;
        }
        desc = &physics->patches[index].desc;
        if (!henka_terrain_physics_patch_contains(desc, world_x_meters, world_z_meters))
        {
            continue;
        }
        edge = desc->patch.sample_edge;
        local_x = (world_x_meters - desc->origin_x_meters) / desc->sample_spacing_meters;
        local_z = (world_z_meters - desc->origin_z_meters) / desc->sample_spacing_meters;
        sample_x = floorf(local_x);
        sample_z = floorf(local_z);
        x = (uint32_t)sample_x;
        z = (uint32_t)sample_z;
        if (x + 1U >= edge) x = edge - 2U;
        if (z + 1U >= edge) z = edge - 2U;
        h00 = (float)physics->patches[index].heights_millimeters[z * edge + x] / 1000.0F;
        h10 = (float)physics->patches[index].heights_millimeters[z * edge + x + 1U] / 1000.0F;
        h01 = (float)physics->patches[index].heights_millimeters[(z + 1U) * edge + x] / 1000.0F;
        h11 = (float)physics->patches[index].heights_millimeters[(z + 1U) * edge + x + 1U] / 1000.0F;
        t_x = local_x - (float)x;
        t_z = local_z - (float)z;
        normal_x = h00 - h10;
        normal_y = 2.0F * desc->sample_spacing_meters;
        normal_z = h00 - h01;
        normal_length = sqrtf(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
        if (!isfinite(normal_length) || normal_length <= 0.0F)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        out_hit->hit = true;
        out_hit->chunk_id = desc->patch.chunk_id;
        out_hit->revision = desc->patch.revision;
        out_hit->generation = desc->patch.generation;
        out_hit->height_meters = h00 + (h10 - h00) * t_x +
            (h01 - h00) * t_z + (h00 - h10 - h01 + h11) * t_x * t_z;
        out_hit->normal = (henka_vec3){
            normal_x / normal_length, normal_y / normal_length, normal_z / normal_length};
        return HENKA_SUCCESS;
    }
    ++physics->stats.missed_query_count;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_physics_raycast(
    henka_terrain_physics* physics,
    henka_ray ray,
    float max_distance,
    henka_terrain_physics_hit* out_hit)
{
    float direction_length;
    henka_vec3 direction;
    float step_distance = 0.5F;
    bool previous_valid = false;
    float previous_distance = 0.0F;
    uint32_t step;

    if (physics == NULL || out_hit == NULL || !isfinite(max_distance) ||
        max_distance <= 0.0F || !isfinite(ray.origin.x) || !isfinite(ray.origin.y) ||
        !isfinite(ray.origin.z) || !isfinite(ray.direction.x) ||
        !isfinite(ray.direction.y) || !isfinite(ray.direction.z))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    direction_length = sqrtf(
        ray.direction.x * ray.direction.x +
        ray.direction.y * ray.direction.y +
        ray.direction.z * ray.direction.z);
    if (!isfinite(direction_length) || direction_length <= 0.0F)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    direction = (henka_vec3){
        ray.direction.x / direction_length,
        ray.direction.y / direction_length,
        ray.direction.z / direction_length};
    *out_hit = (henka_terrain_physics_hit){0};

    for (step = 0U; step <= HENKA_TERRAIN_PHYSICS_MAX_RAY_STEPS; ++step)
    {
        float distance = (float)step * step_distance;
        henka_vec3 position;
        henka_terrain_physics_hit sample_hit;
        henka_result result;
        if (distance > max_distance)
        {
            distance = max_distance;
        }
        position = (henka_vec3){
            ray.origin.x + direction.x * distance,
            ray.origin.y + direction.y * distance,
            ray.origin.z + direction.z * distance};
        result = henka_terrain_physics_sample(physics, position.x, position.z, &sample_hit);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (!sample_hit.hit)
        {
            previous_valid = false;
        }
        else if (position.y <= sample_hit.height_meters)
        {
            float low = previous_valid ? previous_distance : distance;
            float high = distance;
            uint32_t refinement;
            if (!previous_valid)
            {
                *out_hit = sample_hit;
                out_hit->distance = distance;
                out_hit->position = position;
                return HENKA_SUCCESS;
            }
            for (refinement = 0U; refinement < 8U; ++refinement)
            {
                float middle = (low + high) * 0.5F;
                henka_vec3 middle_position = {
                    ray.origin.x + direction.x * middle,
                    ray.origin.y + direction.y * middle,
                    ray.origin.z + direction.z * middle};
                henka_terrain_physics_hit middle_hit;
                result = henka_terrain_physics_sample(
                    physics, middle_position.x, middle_position.z, &middle_hit);
                if (result != HENKA_SUCCESS)
                {
                    return result;
                }
                if (middle_hit.hit && middle_position.y > middle_hit.height_meters)
                {
                    low = middle;
                }
                else
                {
                    high = middle;
                }
            }
            position = (henka_vec3){
                ray.origin.x + direction.x * high,
                ray.origin.y + direction.y * high,
                ray.origin.z + direction.z * high};
            result = henka_terrain_physics_sample(physics, position.x, position.z, out_hit);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            if (!out_hit->hit)
            {
                *out_hit = sample_hit;
            }
            out_hit->distance = high;
            out_hit->position = position;
            return HENKA_SUCCESS;
        }
        previous_valid = true;
        previous_distance = distance;
        if (distance >= max_distance)
        {
            break;
        }
    }
    return HENKA_SUCCESS;
}

void henka_terrain_physics_get_stats(
    const henka_terrain_physics* physics,
    henka_terrain_physics_stats* out_stats)
{
    if (out_stats == NULL)
    {
        return;
    }
    *out_stats = physics == NULL ? (henka_terrain_physics_stats){0} : physics->stats;
}
