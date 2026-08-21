#if defined(HENKA_RUNTIME_HEADLESS)
#include "../runtime_internal.h"
#else
#include "henka_internal.h"
#endif

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>

#include "../core/checked.h"

static bool henka_is_finite_float(float value);

static henka_material_layer henka_material_layer_default(
    henka_vec4 base_color,
    float metallic,
    float roughness,
    float texture_scale_meters)
{
    return (henka_material_layer){
        NULL,
        NULL,
        NULL,
        base_color,
        metallic,
        roughness,
        texture_scale_meters,
        1.0f};
}

static void henka_scene_bump_render_revision(henka_scene* scene)
{
    if (scene == NULL)
    {
        return;
    }
    scene->render_revision = scene->render_revision == UINT64_MAX ? 1U :
        scene->render_revision + 1U;
}

henka_material henka_material_default(void)
{
    henka_material material;

    material.name = "Material";
    material.type = HENKA_MATERIAL_TYPE_LIT;
    material.shader = NULL;
    material.base_color_texture = NULL;
    material.normal_texture = NULL;
    material.metallic_roughness_texture = NULL;
    material.occlusion_texture = NULL;
    material.emissive_texture = NULL;
    material.base_color_uv_set = 0;
    material.normal_uv_set = 0;
    material.metallic_roughness_uv_set = 0;
    material.occlusion_uv_set = 0;
    material.emissive_uv_set = 0;
    material.transmission_uv_set = 0;
    material.thickness_uv_set = 0;
    material.thickness_texture = NULL;
    material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};
    material.emissive_color = (henka_vec3){0.0f, 0.0f, 0.0f};
    material.metallic = 0.0f;
    material.roughness = 0.5f;
    material.specular_factor = 1.0f;
    material.specular_color = (henka_vec3){1.0f, 1.0f, 1.0f};
    material.ior = 1.5f;
    material.transmission = 0.0f;
    material.transmission_texture = NULL;
    material.thickness = 0.0f;
    material.attenuation_distance = 10000.0f;
    material.attenuation_color = (henka_vec3){1.0f, 1.0f, 1.0f};
    material.subsurface = 0.0f;
    material.subsurface_color = (henka_vec3){1.0f, 1.0f, 1.0f};
    material.normal_scale = 1.0f;
    material.occlusion_strength = 1.0f;
    material.emissive_strength = 0.0f;
    material.clearcoat = 0.0f;
    material.clearcoat_roughness = 0.2f;
    material.sheen_color = (henka_vec3){0.0f, 0.0f, 0.0f};
    material.sheen_roughness = 0.5f;
    material.terrain_layers_enabled = false;
    material.terrain_layers[0] = henka_material_layer_default(
        (henka_vec4){0.22f, 0.38f, 0.10f, 1.0f}, 0.0f, 0.92f, 4.0f);
    material.terrain_layers[1] = henka_material_layer_default(
        (henka_vec4){0.34f, 0.17f, 0.07f, 1.0f}, 0.0f, 0.98f, 3.0f);
    material.terrain_layers[2] = henka_material_layer_default(
        (henka_vec4){0.34f, 0.37f, 0.40f, 1.0f}, 0.0f, 0.82f, 5.0f);
    material.terrain_layers[3] = henka_material_layer_default(
        (henka_vec4){0.68f, 0.70f, 0.72f, 1.0f}, 0.0f, 0.70f, 6.0f);
    material.alpha_cutoff = 0.5f;
    material.use_texture = false;
    material.use_lighting = true;
    material.depth_test = true;
    material.alpha_mode = HENKA_MATERIAL_ALPHA_OPAQUE;
    material.double_sided = false;
    material.cast_shadows = true;
    material.receive_shadows = true;
    return material;
}

henka_material henka_material_terrain_default(void)
{
    henka_material material = henka_material_default();
    material.name = "Terrain PBR";
    material.terrain_layers_enabled = true;
    return material;
}

henka_scene_environment_desc henka_scene_environment_default(void)
{
    henka_scene_environment_desc environment = {0};

    environment.ground_color = (henka_vec3){0.035f, 0.045f, 0.065f};
    environment.horizon_color = (henka_vec3){0.16f, 0.19f, 0.24f};
    environment.zenith_color = (henka_vec3){0.055f, 0.08f, 0.14f};
    environment.intensity = 1.5f;
    environment.hdr_texture = NULL;
    environment.hdr_rotation = 0.0f;
    environment.mode = HENKA_SCENE_ENVIRONMENT_GRADIENT;
    environment.atmosphere = (henka_scene_atmosphere_desc){
        0.32f, 0.08f, 0.76f, 1.0f, 2.2f, 0.04f, 8.0f, 6360.0f,
        {0.20f, 0.24f, 0.18f}, 1.0f};
    environment.sun = (henka_scene_sun_desc){
        true, true, {-0.4f, -1.0f, -0.2f}, {1.0f, 0.96f, 0.90f}, 3.0f, 0.00465f};
    environment.moon = (henka_scene_moon_desc){
        true, false, {0.4f, 1.0f, 0.2f}, {0.52f, 0.62f, 1.0f}, 0.10f, 0.0045f};
    environment.stars = (henka_scene_stars_desc){true, 0.08f, 0.0f};
    environment.time_of_day_hours = 12.0f;
    environment.day_length_seconds = 600.0f;
    environment.time_scale = 1.0f;
    environment.time_of_day_enabled = false;
    return environment;
}

const char* henka_scene_environment_preset_get_label(
    henka_scene_environment_preset preset)
{
    switch (preset)
    {
        case HENKA_SCENE_ENVIRONMENT_PRESET_GOLDEN_HOUR:
            return "Golden Hour";
        case HENKA_SCENE_ENVIRONMENT_PRESET_MOONLIT_NIGHT:
            return "Moonlit Night";
        case HENKA_SCENE_ENVIRONMENT_PRESET_ALIEN_HAZE:
            return "Alien Haze";
        case HENKA_SCENE_ENVIRONMENT_PRESET_CLEAR_MIDDAY:
            return "Clear Midday";
        case HENKA_SCENE_ENVIRONMENT_PRESET_COUNT:
        default:
            return "Unknown";
    }
}

const char* henka_material_type_get_label(henka_material_type type)
{
    switch (type)
    {
        case HENKA_MATERIAL_TYPE_UNLIT:
            return "Unlit";
        case HENKA_MATERIAL_TYPE_VERTEX_COLOR:
            return "Vertex Color";
        case HENKA_MATERIAL_TYPE_PROCEDURAL_RESERVED:
            return "Reserved Procedural";
        case HENKA_MATERIAL_TYPE_LIT:
        default:
            return "Lit";
    }
}

#if defined(HENKA_RUNTIME_HEADLESS)
static bool henka_material_texture_matches(
    const henka_texture* texture,
    henka_texture_usage usage,
    henka_texture_color_space color_space)
{
    (void)texture;
    (void)usage;
    (void)color_space;
    return true;
}
#else
static bool henka_material_texture_matches(
    const henka_texture* texture,
    henka_texture_usage usage,
    henka_texture_color_space color_space)
{
    henka_texture_info info;

    if (texture == NULL)
    {
        return true;
    }
    if (henka_texture_get_info(texture, &info) != HENKA_SUCCESS ||
        !info.backend_ready || info.usage != usage || info.color_space != color_space)
    {
        return false;
    }
    return true;
}
#endif

henka_result henka_material_validate(const henka_material* material)
{
    if (material == NULL ||
#if !defined(HENKA_RUNTIME_HEADLESS)
        material->shader == NULL ||
#endif
        material->type < HENKA_MATERIAL_TYPE_LIT ||
        material->type > HENKA_MATERIAL_TYPE_VERTEX_COLOR ||
        !henka_is_finite_float(material->base_color.x) ||
        !henka_is_finite_float(material->base_color.y) ||
        !henka_is_finite_float(material->base_color.z) ||
        !henka_is_finite_float(material->base_color.w) ||
        !henka_is_finite_float(material->emissive_color.x) ||
        !henka_is_finite_float(material->emissive_color.y) ||
        !henka_is_finite_float(material->emissive_color.z) ||
        !henka_is_finite_float(material->metallic) ||
        !henka_is_finite_float(material->roughness) ||
        !henka_is_finite_float(material->specular_factor) ||
        !henka_is_finite_float(material->specular_color.x) ||
        !henka_is_finite_float(material->specular_color.y) ||
        !henka_is_finite_float(material->specular_color.z) ||
        !henka_is_finite_float(material->ior) ||
        !henka_is_finite_float(material->transmission) ||
        !henka_is_finite_float(material->thickness) ||
        !henka_is_finite_float(material->attenuation_distance) ||
        !henka_is_finite_float(material->attenuation_color.x) ||
        !henka_is_finite_float(material->attenuation_color.y) ||
        !henka_is_finite_float(material->attenuation_color.z) ||
        !henka_is_finite_float(material->subsurface) ||
        !henka_is_finite_float(material->subsurface_color.x) ||
        !henka_is_finite_float(material->subsurface_color.y) ||
        !henka_is_finite_float(material->subsurface_color.z) ||
        !henka_is_finite_float(material->normal_scale) ||
        !henka_is_finite_float(material->occlusion_strength) ||
        !henka_is_finite_float(material->emissive_strength) ||
        !henka_is_finite_float(material->clearcoat) ||
        !henka_is_finite_float(material->clearcoat_roughness) ||
        !henka_is_finite_float(material->sheen_color.x) ||
        !henka_is_finite_float(material->sheen_color.y) ||
        !henka_is_finite_float(material->sheen_color.z) ||
        !henka_is_finite_float(material->sheen_roughness) ||
        !henka_is_finite_float(material->alpha_cutoff) ||
        material->base_color.x < 0.0f || material->base_color.x > 1.0f ||
        material->base_color.y < 0.0f || material->base_color.y > 1.0f ||
        material->base_color.z < 0.0f || material->base_color.z > 1.0f ||
        material->base_color.w < 0.0f || material->base_color.w > 1.0f ||
        material->emissive_color.x < 0.0f || material->emissive_color.y < 0.0f ||
        material->emissive_color.z < 0.0f || material->emissive_color.x > 1.0f ||
        material->emissive_color.y > 1.0f || material->emissive_color.z > 1.0f ||
        material->metallic < 0.0f ||
        material->metallic > 1.0f || material->roughness < 0.045f ||
        material->roughness > 1.0f || material->normal_scale < 0.0f ||
        material->specular_factor < 0.0f || material->specular_factor > 1.0f ||
        material->specular_color.x < 0.0f || material->specular_color.x > 1.0f ||
        material->specular_color.y < 0.0f || material->specular_color.y > 1.0f ||
        material->specular_color.z < 0.0f || material->specular_color.z > 1.0f ||
        material->ior < 1.0f || material->ior > 3.0f ||
        material->transmission < 0.0f || material->transmission > 1.0f ||
        material->thickness < 0.0f || material->thickness > 1.0f ||
        material->attenuation_distance <= 0.0f || material->attenuation_distance > 1000000.0f ||
        material->attenuation_color.x < 0.0f || material->attenuation_color.x > 1.0f ||
        material->attenuation_color.y < 0.0f || material->attenuation_color.y > 1.0f ||
        material->attenuation_color.z < 0.0f || material->attenuation_color.z > 1.0f ||
        material->subsurface < 0.0f || material->subsurface > 1.0f ||
        material->subsurface_color.x < 0.0f || material->subsurface_color.x > 1.0f ||
        material->subsurface_color.y < 0.0f || material->subsurface_color.y > 1.0f ||
        material->subsurface_color.z < 0.0f || material->subsurface_color.z > 1.0f ||
        material->normal_scale > 4.0f || material->occlusion_strength < 0.0f ||
        material->occlusion_strength > 1.0f || material->emissive_strength < 0.0f ||
        material->emissive_strength > 100.0f || material->alpha_cutoff < 0.0f ||
        material->alpha_cutoff > 1.0f ||
        material->base_color_uv_set < 0 || material->base_color_uv_set > 1 ||
        material->normal_uv_set < 0 || material->normal_uv_set > 1 ||
        material->metallic_roughness_uv_set < 0 || material->metallic_roughness_uv_set > 1 ||
        material->occlusion_uv_set < 0 || material->occlusion_uv_set > 1 ||
        material->emissive_uv_set < 0 || material->emissive_uv_set > 1 ||
        material->transmission_uv_set < 0 || material->transmission_uv_set > 1 ||
        material->clearcoat < 0.0f || material->clearcoat > 1.0f ||
        material->clearcoat_roughness < 0.045f || material->clearcoat_roughness > 1.0f ||
        material->sheen_color.x < 0.0f || material->sheen_color.x > 1.0f ||
        material->sheen_color.y < 0.0f || material->sheen_color.y > 1.0f ||
        material->sheen_color.z < 0.0f || material->sheen_color.z > 1.0f ||
        material->sheen_roughness < 0.045f || material->sheen_roughness > 1.0f ||
        material->alpha_mode > HENKA_MATERIAL_ALPHA_BLENDED ||
        (!material->use_texture && material->base_color_texture != NULL) ||
        (material->use_texture && material->base_color_texture == NULL) ||
        !henka_material_texture_matches(material->base_color_texture, HENKA_TEXTURE_USAGE_COLOR, HENKA_TEXTURE_COLOR_SPACE_SRGB) ||
        !henka_material_texture_matches(material->normal_texture, HENKA_TEXTURE_USAGE_NORMAL, HENKA_TEXTURE_COLOR_SPACE_LINEAR) ||
        !henka_material_texture_matches(material->metallic_roughness_texture, HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS, HENKA_TEXTURE_COLOR_SPACE_LINEAR) ||
        !henka_material_texture_matches(material->occlusion_texture, HENKA_TEXTURE_USAGE_OCCLUSION, HENKA_TEXTURE_COLOR_SPACE_LINEAR) ||
        !henka_material_texture_matches(material->emissive_texture, HENKA_TEXTURE_USAGE_EMISSIVE, HENKA_TEXTURE_COLOR_SPACE_SRGB) ||
        !henka_material_texture_matches(material->transmission_texture, HENKA_TEXTURE_USAGE_GENERIC_DATA, HENKA_TEXTURE_COLOR_SPACE_LINEAR) ||
        !henka_material_texture_matches(material->thickness_texture, HENKA_TEXTURE_USAGE_GENERIC_DATA, HENKA_TEXTURE_COLOR_SPACE_LINEAR))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (material->terrain_layers_enabled)
    {
        size_t layer_index;
        for (layer_index = 0U; layer_index < HENKA_MATERIAL_TERRAIN_LAYER_COUNT; ++layer_index)
        {
            const henka_material_layer* layer = &material->terrain_layers[layer_index];
            if (!henka_is_finite_float(layer->base_color.x) ||
                !henka_is_finite_float(layer->base_color.y) ||
                !henka_is_finite_float(layer->base_color.z) ||
                !henka_is_finite_float(layer->base_color.w) ||
                !henka_is_finite_float(layer->metallic) ||
                !henka_is_finite_float(layer->roughness) ||
                !henka_is_finite_float(layer->texture_scale_meters) ||
                !henka_is_finite_float(layer->normal_scale) ||
                layer->base_color.x < 0.0f || layer->base_color.x > 1.0f ||
                layer->base_color.y < 0.0f || layer->base_color.y > 1.0f ||
                layer->base_color.z < 0.0f || layer->base_color.z > 1.0f ||
                layer->base_color.w < 0.0f || layer->base_color.w > 1.0f ||
                layer->metallic < 0.0f || layer->metallic > 1.0f ||
                layer->roughness < 0.045f || layer->roughness > 1.0f ||
                layer->texture_scale_meters <= 0.0f || layer->texture_scale_meters > 4096.0f ||
                layer->normal_scale < 0.0f || layer->normal_scale > 4.0f ||
                !henka_material_texture_matches(layer->base_color_texture, HENKA_TEXTURE_USAGE_COLOR, HENKA_TEXTURE_COLOR_SPACE_SRGB) ||
                !henka_material_texture_matches(layer->normal_texture, HENKA_TEXTURE_USAGE_NORMAL, HENKA_TEXTURE_COLOR_SPACE_LINEAR) ||
                !henka_material_texture_matches(layer->metallic_roughness_texture, HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS, HENKA_TEXTURE_COLOR_SPACE_LINEAR))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_material_describe(const henka_material* material, char* buffer, size_t buffer_size)
{
    const char* material_name;
    int written;

    if (material == NULL || buffer == NULL || buffer_size == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    material_name = (material->name != NULL && material->name[0] != '\0') ? material->name : "Material";
    written = snprintf(
        buffer,
        buffer_size,
        "%s | %s | %s%s | M %.2f R %.2f",
        material_name,
        henka_material_type_get_label(material->type),
        material->use_texture ? "Texture" : "Color",
        material->use_lighting ? " | Lit" : " | Unlit",
        material->metallic,
        material->roughness);
    if (written < 0 || (size_t)written >= buffer_size)
    {
        buffer[buffer_size - 1U] = '\0';
        return HENKA_ERROR_UNKNOWN;
    }

    return HENKA_SUCCESS;
}

#define HENKA_ENTITY_SLOT_BITS 21U
#define HENKA_ENTITY_SLOT_MASK \
    ((((henka_entity)1U) << HENKA_ENTITY_SLOT_BITS) - (henka_entity)1U)
#define HENKA_ENTITY_GENERATION_SHIFT HENKA_ENTITY_SLOT_BITS
#define HENKA_ENTITY_GENERATION_MASK \
    (UINT64_MAX >> HENKA_ENTITY_GENERATION_SHIFT)

static henka_entity henka_scene_make_entity(
    size_t index,
    uint64_t generation)
{
    const henka_entity slot = (henka_entity)index + (henka_entity)1U;

    if (index >= HENKA_MAX_SCENE_ENTITIES ||
        slot == HENKA_INVALID_ENTITY ||
        slot > HENKA_ENTITY_SLOT_MASK ||
        generation == 0U ||
        generation > HENKA_ENTITY_GENERATION_MASK)
    {
        return HENKA_INVALID_ENTITY;
    }

    return ((henka_entity)generation <<
            HENKA_ENTITY_GENERATION_SHIFT) |
        slot;
}

static bool henka_scene_decode_entity(
    henka_entity entity,
    size_t* out_index,
    uint64_t* out_generation)
{
    const henka_entity slot = entity & HENKA_ENTITY_SLOT_MASK;
    const uint64_t generation =
        (uint64_t)(entity >> HENKA_ENTITY_GENERATION_SHIFT);

    if (out_index != NULL)
    {
        *out_index = 0U;
    }
    if (out_generation != NULL)
    {
        *out_generation = 0U;
    }

    if (entity == HENKA_INVALID_ENTITY ||
        slot == HENKA_INVALID_ENTITY ||
        generation == 0U ||
        out_index == NULL ||
        out_generation == NULL)
    {
        return false;
    }

    *out_index = (size_t)(slot - (henka_entity)1U);
    *out_generation = generation;
    return true;
}

static void henka_scene_advance_entity_generation(
    henka_scene_entity_record* record)
{
    if (record == NULL)
    {
        return;
    }

    record->generation =
        (record->generation + 1U) &
        HENKA_ENTITY_GENERATION_MASK;
    if (record->generation == 0U)
    {
        record->generation = 1U;
    }
}

static henka_scene_entity_record* henka_scene_get_entity_record(
    henka_scene* scene,
    henka_entity entity)
{
    uint64_t generation;
    size_t index;

    if (scene == NULL ||
        !henka_scene_decode_entity(
            entity,
            &index,
            &generation) ||
        index >= scene->entity_capacity)
    {
        return NULL;
    }

    if (!scene->entities[index].active ||
        scene->entities[index].generation != generation)
    {
        return NULL;
    }

    return &scene->entities[index];
}

static henka_result henka_scene_duplicate_text(const char* value, char** out_copy)
{
    size_t allocation_size;
    char* copy;
    size_t length;

    if (out_copy == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_copy = NULL;
    if (value == NULL)
    {
        return HENKA_SUCCESS;
    }

    if (!henka_checked_c_string_length(value, HENKA_MAX_SCENE_TEXT_BYTES, &length) ||
        !henka_checked_size_add(length, 1U, &allocation_size))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    copy = henka_malloc(allocation_size);
    if (copy == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    memcpy(copy, value, allocation_size);
    *out_copy = copy;
    return HENKA_SUCCESS;
}

static bool henka_scene_double_fits_float(double value)
{
    return isfinite(value) && value >= -(double)FLT_MAX && value <= (double)FLT_MAX;
}

static bool henka_scene_try_transform_bounds(
    henka_bounds local_bounds,
    henka_transform transform,
    henka_bounds* out_world_bounds)
{
    double center_x;
    double center_y;
    double center_z;
    double extent_x;
    double extent_y;
    double extent_z;
    double scaled_center_x;
    double scaled_center_y;
    double scaled_center_z;
    double scaled_extent_x;
    double scaled_extent_y;
    double scaled_extent_z;
    henka_mat4 rotation;

    if (out_world_bounds == NULL)
    {
        return false;
    }

    *out_world_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    rotation = henka_mat4_rotation(transform.rotation);
    scaled_center_x = (double)local_bounds.center.x * (double)transform.scale.x;
    scaled_center_y = (double)local_bounds.center.y * (double)transform.scale.y;
    scaled_center_z = (double)local_bounds.center.z * (double)transform.scale.z;
    scaled_extent_x = fabs((double)local_bounds.extents.x * (double)transform.scale.x);
    scaled_extent_y = fabs((double)local_bounds.extents.y * (double)transform.scale.y);
    scaled_extent_z = fabs((double)local_bounds.extents.z * (double)transform.scale.z);

    center_x = (double)transform.position.x +
        (double)rotation.m[0] * scaled_center_x +
        (double)rotation.m[4] * scaled_center_y +
        (double)rotation.m[8] * scaled_center_z;
    center_y = (double)transform.position.y +
        (double)rotation.m[1] * scaled_center_x +
        (double)rotation.m[5] * scaled_center_y +
        (double)rotation.m[9] * scaled_center_z;
    center_z = (double)transform.position.z +
        (double)rotation.m[2] * scaled_center_x +
        (double)rotation.m[6] * scaled_center_y +
        (double)rotation.m[10] * scaled_center_z;
    extent_x =
        fabs((double)rotation.m[0]) * scaled_extent_x +
        fabs((double)rotation.m[4]) * scaled_extent_y +
        fabs((double)rotation.m[8]) * scaled_extent_z;
    extent_y =
        fabs((double)rotation.m[1]) * scaled_extent_x +
        fabs((double)rotation.m[5]) * scaled_extent_y +
        fabs((double)rotation.m[9]) * scaled_extent_z;
    extent_z =
        fabs((double)rotation.m[2]) * scaled_extent_x +
        fabs((double)rotation.m[6]) * scaled_extent_y +
        fabs((double)rotation.m[10]) * scaled_extent_z;

    if (!henka_scene_double_fits_float(center_x) ||
        !henka_scene_double_fits_float(center_y) ||
        !henka_scene_double_fits_float(center_z) ||
        !henka_scene_double_fits_float(extent_x) ||
        !henka_scene_double_fits_float(extent_y) ||
        !henka_scene_double_fits_float(extent_z) ||
        !henka_scene_double_fits_float(center_x - extent_x) ||
        !henka_scene_double_fits_float(center_x + extent_x) ||
        !henka_scene_double_fits_float(center_y - extent_y) ||
        !henka_scene_double_fits_float(center_y + extent_y) ||
        !henka_scene_double_fits_float(center_z - extent_z) ||
        !henka_scene_double_fits_float(center_z + extent_z))
    {
        return false;
    }

    out_world_bounds->center = (henka_vec3){
        (float)center_x,
        (float)center_y,
        (float)center_z};
    out_world_bounds->extents = (henka_vec3){
        (float)extent_x,
        (float)extent_y,
        (float)extent_z};
    return true;
}

static const float g_henka_scene_minimum_scale_magnitude = 0.01f;

static bool henka_is_finite_float(float value)
{
    return isfinite(value) != 0;
}

static bool henka_scale_component_is_valid(float value)
{
    return henka_is_finite_float(value) &&
        fabsf(value) >= g_henka_scene_minimum_scale_magnitude;
}

static bool henka_scale_vector_is_valid(henka_vec3 value)
{
    return henka_scale_component_is_valid(value.x) &&
        henka_scale_component_is_valid(value.y) &&
        henka_scale_component_is_valid(value.z);
}

static bool henka_scene_vec3_is_finite(henka_vec3 value)
{
    return henka_is_finite_float(value.x) &&
        henka_is_finite_float(value.y) &&
        henka_is_finite_float(value.z);
}

static bool henka_scene_bounds_are_valid(henka_bounds bounds)
{
    return henka_scene_vec3_is_finite(bounds.center) &&
        henka_scene_vec3_is_finite(bounds.extents) &&
        bounds.extents.x >= 0.0f &&
        bounds.extents.y >= 0.0f &&
        bounds.extents.z >= 0.0f;
}

static bool henka_scene_material_is_valid(henka_material material)
{
    return henka_material_validate(&material) == HENKA_SUCCESS;
}

static bool henka_transform_is_valid(henka_transform transform)
{
    return henka_is_finite_float(transform.position.x) &&
        henka_is_finite_float(transform.position.y) &&
        henka_is_finite_float(transform.position.z) &&
        henka_is_finite_float(transform.rotation.x) &&
        henka_is_finite_float(transform.rotation.y) &&
        henka_is_finite_float(transform.rotation.z) &&
        henka_is_finite_float(transform.rotation.w) &&
        henka_scale_vector_is_valid(transform.scale);
}

static henka_transform henka_transform_sanitize(henka_transform transform)
{
    transform.rotation = henka_quat_normalize(transform.rotation);
    return transform;
}

static bool henka_scene_ray_intersects_bounds(henka_ray ray, henka_bounds bounds, float* out_distance)
{
    const float epsilon = 0.00001f;
    henka_vec3 minimum;
    henka_vec3 maximum;
    float tmin;
    float tmax;
    int axis;

    minimum = henka_vec3_subtract(bounds.center, bounds.extents);
    maximum = henka_vec3_add(bounds.center, bounds.extents);
    tmin = 0.0f;
    tmax = 1000000.0f;

    for (axis = 0; axis < 3; ++axis)
    {
        const float origin = axis == 0 ? ray.origin.x : (axis == 1 ? ray.origin.y : ray.origin.z);
        const float direction = axis == 0 ? ray.direction.x : (axis == 1 ? ray.direction.y : ray.direction.z);
        const float min_value = axis == 0 ? minimum.x : (axis == 1 ? minimum.y : minimum.z);
        const float max_value = axis == 0 ? maximum.x : (axis == 1 ? maximum.y : maximum.z);

        if (fabsf(direction) < epsilon)
        {
            if (origin < min_value || origin > max_value)
            {
                return false;
            }

            continue;
        }

        {
            float t0 = (min_value - origin) / direction;
            float t1 = (max_value - origin) / direction;
            float near_value = t0 < t1 ? t0 : t1;
            float far_value = t0 > t1 ? t0 : t1;

            if (near_value > tmin)
            {
                tmin = near_value;
            }

            if (far_value < tmax)
            {
                tmax = far_value;
            }

            if (tmax < tmin)
            {
                return false;
            }
        }
    }

    if (out_distance != NULL)
    {
        *out_distance = tmin >= 0.0f ? tmin : tmax;
    }

    return tmax >= 0.0f;
}

static const henka_scene_entity_record* henka_scene_get_entity_record_const(const henka_scene* scene, henka_entity entity)
{
    return henka_scene_get_entity_record((henka_scene*)scene, entity);
}

static henka_result henka_scene_grow(henka_scene* scene)
{
    size_t allocation_size;
    size_t index;
    size_t new_capacity;
    henka_scene_entity_record* new_entities;
    size_t required;

    if (scene == NULL ||
        !henka_checked_size_add(scene->entity_capacity, 1U, &required) ||
        !henka_checked_capacity(
            scene->entity_capacity,
            required,
            8U,
            HENKA_MAX_SCENE_ENTITIES,
            &new_capacity) ||
        !henka_checked_size_multiply(new_capacity, sizeof(*new_entities), &allocation_size))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    new_entities = henka_realloc(scene->entities, allocation_size);
    if (new_entities == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    for (index = scene->entity_capacity; index < new_capacity; ++index)
    {
        new_entities[index].active = false;
        new_entities[index].generation = 1U;
        new_entities[index].visible = true;
        new_entities[index].flags = HENKA_SCENE_ENTITY_FLAG_NONE;
        new_entities[index].name = NULL;
        new_entities[index].tag = NULL;
        new_entities[index].transform = henka_transform_identity();
        new_entities[index].previous_transform = henka_transform_identity();
        new_entities[index].previous_transform_valid = false;
        new_entities[index].mesh = NULL;
        new_entities[index].material = henka_material_default();
        new_entities[index].material_asset = NULL;
        new_entities[index].material_asset_revision = 0U;
        new_entities[index].material_asset_overridden = false;
        new_entities[index].material_name = NULL;
        new_entities[index].has_local_bounds = false;
        new_entities[index].local_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
        new_entities[index].interaction = (henka_interaction_desc){false, 2.0f, NULL};
        new_entities[index].interaction_prompt = NULL;
    }

    scene->entities = new_entities;
    scene->entity_capacity = new_capacity;
    return HENKA_SUCCESS;
}

henka_result henka_scene_create(henka_scene** out_scene)
{
    henka_scene* scene;

    if (out_scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_scene = NULL;

    scene = henka_calloc(1U, sizeof(*scene));
    if (scene == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    scene->light_direction.x = -0.4f;
    scene->light_direction.y = -1.0f;
    scene->light_direction.z = -0.2f;
    scene->light_color = (henka_vec3){1.0f, 0.96f, 0.90f};
    scene->light_intensity = 3.0f;
    scene->ambient_color.x = 0.16f;
    scene->ambient_color.y = 0.18f;
    scene->ambient_color.z = 0.22f;
    scene->environment = henka_scene_environment_default();
    scene->fog = (henka_scene_fog_desc){
        false,
        HENKA_SCENE_FOG_LINEAR,
        (henka_vec3){0.16f, 0.19f, 0.24f},
        8.0f,
        80.0f,
        0.0f};
    scene->render_revision = 1U;

    *out_scene = scene;
    return HENKA_SUCCESS;
}

henka_result henka_scene_clone(
    const henka_scene* source,
    henka_scene** out_clone)
{
    henka_scene* clone;
    size_t allocation_size;
    size_t index;
    henka_result result;

    if (out_clone == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_clone = NULL;
    if (source == NULL || source->entity_count > HENKA_MAX_SCENE_ENTITIES ||
        source->entity_count > source->entity_capacity ||
        source->entity_capacity > HENKA_MAX_SCENE_ENTITIES ||
        !henka_checked_size_multiply(
            source->entity_capacity,
            sizeof(*source->entities),
            &allocation_size))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_scene_create(&clone);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    clone->camera = source->camera;
    clone->has_camera = source->has_camera;
    clone->light_direction = source->light_direction;
    clone->light_color = source->light_color;
    clone->light_intensity = source->light_intensity;
    clone->ambient_color = source->ambient_color;
    clone->environment = source->environment;
    memcpy(clone->reflection_probes, source->reflection_probes, sizeof(clone->reflection_probes));
    memcpy(clone->reflection_probe_active, source->reflection_probe_active, sizeof(clone->reflection_probe_active));
    clone->render_revision = source->render_revision;
    memcpy(clone->local_lights, source->local_lights, sizeof(clone->local_lights));
    memcpy(clone->local_light_active, source->local_light_active, sizeof(clone->local_light_active));
    clone->fog = source->fog;

    if (source->entity_capacity == 0U)
    {
        *out_clone = clone;
        return HENKA_SUCCESS;
    }

    clone->entities = henka_malloc(allocation_size);
    if (clone->entities == NULL)
    {
        henka_scene_destroy(clone);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    memset(clone->entities, 0, allocation_size);
    clone->entity_capacity = source->entity_capacity;
    clone->entity_count = source->entity_count;

    for (index = 0U; index < source->entity_capacity; ++index)
    {
        clone->entities[index] = source->entities[index];
        clone->entities[index].name = NULL;
        clone->entities[index].tag = NULL;
        clone->entities[index].material_name = NULL;
        clone->entities[index].interaction_prompt = NULL;

        result = henka_scene_duplicate_text(source->entities[index].name, &clone->entities[index].name);
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_duplicate_text(source->entities[index].tag, &clone->entities[index].tag);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_duplicate_text(
                source->entities[index].material_name,
                &clone->entities[index].material_name);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_duplicate_text(
                source->entities[index].interaction_prompt,
                &clone->entities[index].interaction_prompt);
        }
        if (result != HENKA_SUCCESS)
        {
            henka_scene_destroy(clone);
            return result;
        }

        clone->entities[index].material.name = clone->entities[index].material_name != NULL
            ? clone->entities[index].material_name
            : source->entities[index].material.name;
        clone->entities[index].interaction.prompt = clone->entities[index].interaction_prompt;
    }

    *out_clone = clone;
    return HENKA_SUCCESS;
}

void henka_scene_destroy(henka_scene* scene)
{
    size_t index;

    if (scene == NULL)
    {
        return;
    }

    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        henka_free(scene->entities[index].name);
        henka_free(scene->entities[index].tag);
        henka_free(scene->entities[index].material_name);
        henka_free(scene->entities[index].interaction_prompt);
    }

    henka_free(scene->entities);
    henka_free(scene);
}

henka_entity henka_scene_create_entity(henka_scene* scene)
{
    return henka_scene_create_entity_named(scene, NULL);
}

henka_entity henka_scene_create_entity_named(henka_scene* scene, const char* name)
{
    size_t index;
    char* copy;
    henka_result copy_result;

    if (scene == NULL || scene->entity_count >= HENKA_MAX_SCENE_ENTITIES)
    {
        return HENKA_INVALID_ENTITY;
    }

    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        if (!scene->entities[index].active)
        {
            scene->entities[index].active = true;
            scene->entities[index].visible = true;
            scene->entities[index].flags = HENKA_SCENE_ENTITY_FLAG_NONE;
            scene->entities[index].transform = henka_transform_identity();
            scene->entities[index].previous_transform = henka_transform_identity();
            scene->entities[index].previous_transform_valid = false;
            scene->entities[index].mesh = NULL;
            scene->entities[index].lod = (henka_scene_lod_desc){0};
            scene->entities[index].material = henka_material_default();
            scene->entities[index].material_asset = NULL;
            henka_free(scene->entities[index].name);
            henka_free(scene->entities[index].tag);
            henka_free(scene->entities[index].material_name);
            henka_free(scene->entities[index].interaction_prompt);
            scene->entities[index].name = NULL;
            scene->entities[index].tag = NULL;
            scene->entities[index].material_name = NULL;
            scene->entities[index].interaction_prompt = NULL;
            scene->entities[index].has_local_bounds = false;
            scene->entities[index].local_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}};
            scene->entities[index].interaction = (henka_interaction_desc){false, 2.0f, NULL};
            scene->entities[index].selection_owner = henka_scene_make_entity(
                index,
                scene->entities[index].generation);

            copy_result = henka_scene_duplicate_text(name, &copy);
            if (copy_result != HENKA_SUCCESS)
            {
                scene->entities[index].active = false;
                return HENKA_INVALID_ENTITY;
            }

            scene->entities[index].name = copy;
            scene->entity_count += 1U;
            henka_scene_bump_render_revision(scene);
            return henka_scene_make_entity(
                index,
                scene->entities[index].generation);
        }
    }

    if (henka_scene_grow(scene) != HENKA_SUCCESS)
    {
        return HENKA_INVALID_ENTITY;
    }

    scene->entities[scene->entity_count].active = true;
    scene->entities[scene->entity_count].visible = true;
    scene->entities[scene->entity_count].flags = HENKA_SCENE_ENTITY_FLAG_NONE;
    scene->entities[scene->entity_count].transform = henka_transform_identity();
    scene->entities[scene->entity_count].previous_transform = henka_transform_identity();
    scene->entities[scene->entity_count].previous_transform_valid = false;
    scene->entities[scene->entity_count].lod = (henka_scene_lod_desc){0};
    scene->entities[scene->entity_count].material = henka_material_default();
    scene->entities[scene->entity_count].has_local_bounds = false;
    scene->entities[scene->entity_count].local_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}};
    scene->entities[scene->entity_count].interaction = (henka_interaction_desc){false, 2.0f, NULL};
    scene->entities[scene->entity_count].selection_owner = henka_scene_make_entity(
        scene->entity_count,
        scene->entities[scene->entity_count].generation);
    copy_result = henka_scene_duplicate_text(name, &copy);
    if (copy_result != HENKA_SUCCESS)
    {
        scene->entities[scene->entity_count].active = false;
        return HENKA_INVALID_ENTITY;
    }

    scene->entities[scene->entity_count].name = copy;
    scene->entity_count += 1U;
    henka_scene_bump_render_revision(scene);
    return henka_scene_make_entity(
        scene->entity_count - 1U,
        scene->entities[scene->entity_count - 1U].generation);
}

void henka_scene_destroy_entity(henka_scene* scene, henka_entity entity)
{
    henka_scene_entity_record* record;
    size_t index;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return;
    }

    /* A logical owner is a presentation relationship, not an ownership
     * lifetime. Promote its children to independent roots before retiring the
     * owner so stale selection handles cannot survive a destroy/reuse cycle. */
    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        henka_scene_entity_record* child = &scene->entities[index];
        if (child->active && child->selection_owner == entity)
        {
            child->selection_owner = henka_scene_make_entity(index, child->generation);
        }
    }

    record->active = false;
    henka_scene_advance_entity_generation(record);
    record->visible = true;
    record->flags = HENKA_SCENE_ENTITY_FLAG_NONE;
    record->selection_owner = HENKA_INVALID_ENTITY;
    record->mesh = NULL;
    record->material = henka_material_default();
    record->material_asset = NULL;
    record->material_asset_revision = 0U;
    record->material_asset_overridden = false;
    henka_free(record->name);
    henka_free(record->tag);
    henka_free(record->material_name);
    henka_free(record->interaction_prompt);
    record->name = NULL;
    record->tag = NULL;
    record->material_name = NULL;
    record->interaction_prompt = NULL;
    record->has_local_bounds = false;
    record->interaction = (henka_interaction_desc){false, 2.0f, NULL};
    if (scene->entity_count > 0U)
    {
        scene->entity_count -= 1U;
    }
    henka_scene_bump_render_revision(scene);
}

bool henka_scene_is_entity_valid(const henka_scene* scene, henka_entity entity)
{
    return henka_scene_get_entity_record_const(scene, entity) != NULL;
}

bool henka_scene_is_entity_visible(const henka_scene* scene, henka_entity entity)
{
    const henka_scene_entity_record* record;

    record = henka_scene_get_entity_record_const(scene, entity);
    return record != NULL ? record->visible : false;
}

size_t henka_scene_get_entity_count(const henka_scene* scene)
{
    if (scene == NULL)
    {
        return 0U;
    }

    return scene->entity_count;
}

henka_entity henka_scene_get_entity_at_storage_index(
    const henka_scene* scene,
    size_t index)
{
    if (scene == NULL || index >= scene->entity_capacity ||
        !scene->entities[index].active)
    {
        return HENKA_INVALID_ENTITY;
    }
    return henka_scene_make_entity(index, scene->entities[index].generation);
}

henka_entity henka_scene_get_entity_at_index(const henka_scene* scene, size_t index)
{
    size_t active_index;
    size_t entity_index;

    if (scene == NULL)
    {
        return HENKA_INVALID_ENTITY;
    }

    active_index = 0U;
    for (entity_index = 0U; entity_index < scene->entity_capacity; ++entity_index)
    {
        if (!scene->entities[entity_index].active)
        {
            continue;
        }

        if (active_index == index)
        {
            return henka_scene_make_entity(
                entity_index,
                scene->entities[entity_index].generation);
        }

        active_index += 1U;
    }

    return HENKA_INVALID_ENTITY;
}

const char* henka_scene_get_entity_name(const henka_scene* scene, henka_entity entity)
{
    const henka_scene_entity_record* record;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return NULL;
    }

    return record->name;
}

const char* henka_scene_get_entity_tag(const henka_scene* scene, henka_entity entity)
{
    const henka_scene_entity_record* record;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return NULL;
    }

    return record->tag;
}

henka_result henka_scene_find_entity_by_name(const henka_scene* scene, const char* name, henka_entity* out_entity)
{
    size_t index;

    if (scene == NULL || name == NULL || name[0] == '\0' || out_entity == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_entity = HENKA_INVALID_ENTITY;

    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        if (!scene->entities[index].active || scene->entities[index].name == NULL)
        {
            continue;
        }

        if (strcmp(scene->entities[index].name, name) == 0)
        {
            *out_entity = henka_scene_make_entity(
                index,
                scene->entities[index].generation);
            return HENKA_SUCCESS;
        }
    }

    return HENKA_ERROR_UNKNOWN;
}

henka_result henka_scene_find_entity_by_tag(const henka_scene* scene, const char* tag, henka_entity* out_entity)
{
    size_t index;

    if (scene == NULL || tag == NULL || tag[0] == '\0' || out_entity == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_entity = HENKA_INVALID_ENTITY;
    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        if (!scene->entities[index].active || scene->entities[index].tag == NULL)
        {
            continue;
        }

        if (strcmp(scene->entities[index].tag, tag) == 0)
        {
            *out_entity = henka_scene_make_entity(
                index,
                scene->entities[index].generation);
            return HENKA_SUCCESS;
        }
    }

    return HENKA_ERROR_UNKNOWN;
}

henka_result henka_scene_get_entity_info(const henka_scene* scene, henka_entity entity, henka_scene_object_info* out_info)
{
    const henka_scene_entity_record* record;

    if (out_info == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_info->entity = entity;
    out_info->name = record->name;
    out_info->tag = record->tag;
    out_info->visible = record->visible;
    out_info->has_bounds = record->has_local_bounds;
    out_info->local_bounds = record->local_bounds;
    out_info->transform = record->transform;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_transform(const henka_scene* scene, henka_entity entity, henka_transform* out_transform)
{
    const henka_scene_entity_record* record;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL || out_transform == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_transform = record->transform;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_mesh(const henka_scene* scene, henka_entity entity, henka_mesh** out_mesh)
{
    const henka_scene_entity_record* record;

    if (out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_mesh = record->mesh;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_material(const henka_scene* scene, henka_entity entity, henka_material* out_material)
{
    const henka_scene_entity_record* record;

    if (out_material == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_material = record->material;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_material_asset(
    const henka_scene* scene,
    henka_entity entity,
    const henka_material_asset** out_asset)
{
    const henka_scene_entity_record* record;

    if (out_asset == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_asset = record->material_asset;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_local_bounds(const henka_scene* scene, henka_entity entity, henka_bounds* out_bounds)
{
    const henka_scene_entity_record* record;

    if (out_bounds == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL || !record->has_local_bounds)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_bounds = record->local_bounds;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_world_bounds(const henka_scene* scene, henka_entity entity, henka_bounds* out_bounds)
{
    const henka_scene_entity_record* record;

    if (out_bounds == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL || !record->has_local_bounds ||
        !henka_scene_try_transform_bounds(record->local_bounds, record->transform, out_bounds))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_interaction(const henka_scene* scene, henka_entity entity, henka_interaction_desc* out_interaction)
{
    const henka_scene_entity_record* record;

    if (out_interaction == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_interaction = record->interaction;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_flags(const henka_scene* scene, henka_entity entity, uint32_t* out_flags)
{
    const henka_scene_entity_record* record;

    if (out_flags == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_flags = record->flags;
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_selection_owner(
    const henka_scene* scene,
    henka_entity entity,
    henka_entity* out_owner)
{
    const henka_scene_entity_record* record;
    const henka_scene_entity_record* owner_record;

    if (out_owner == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_owner = HENKA_INVALID_ENTITY;
    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    owner_record = henka_scene_get_entity_record_const(scene, record->selection_owner);
    if (owner_record == NULL || (owner_record->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    *out_owner = record->selection_owner;
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_selection_owner(
    henka_scene* scene,
    henka_entity entity,
    henka_entity owner)
{
    henka_scene_entity_record* record;
    const henka_scene_entity_record* owner_record;

    record = henka_scene_get_entity_record(scene, entity);
    owner_record = henka_scene_get_entity_record_const(scene, owner);
    if (record == NULL || owner_record == NULL ||
        (record->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U ||
        owner_record->selection_owner != owner ||
        (owner_record->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->selection_owner = owner;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_transform(henka_scene* scene, henka_entity entity, henka_transform transform)
{
    henka_scene_entity_record* record;
    henka_transform sanitized_transform;
    henka_bounds world_bounds;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || !henka_transform_is_valid(transform))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    sanitized_transform = henka_transform_sanitize(transform);
    if (record->has_local_bounds &&
        !henka_scene_try_transform_bounds(record->local_bounds, sanitized_transform, &world_bounds))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->transform = sanitized_transform;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_translate_entity(henka_scene* scene, henka_entity entity, henka_vec3 delta)
{
    henka_transform transform;

    if (!henka_is_finite_float(delta.x) || !henka_is_finite_float(delta.y) || !henka_is_finite_float(delta.z))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_scene_get_entity_transform(scene, entity, &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    transform.position = henka_vec3_add(transform.position, delta);
    return henka_scene_set_entity_transform(scene, entity, transform);
}

henka_result henka_scene_rotate_entity(henka_scene* scene, henka_entity entity, henka_quat delta_rotation)
{
    henka_transform transform;

    if (!henka_is_finite_float(delta_rotation.x) ||
        !henka_is_finite_float(delta_rotation.y) ||
        !henka_is_finite_float(delta_rotation.z) ||
        !henka_is_finite_float(delta_rotation.w))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_scene_get_entity_transform(scene, entity, &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    transform.rotation = henka_quat_multiply(delta_rotation, transform.rotation);
    return henka_scene_set_entity_transform(scene, entity, transform);
}

henka_result henka_scene_scale_entity(henka_scene* scene, henka_entity entity, henka_vec3 scale_multiplier)
{
    henka_transform transform;

    if (!henka_is_finite_float(scale_multiplier.x) ||
        !henka_is_finite_float(scale_multiplier.y) ||
        !henka_is_finite_float(scale_multiplier.z))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_scene_get_entity_transform(scene, entity, &transform) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    transform.scale.x *= scale_multiplier.x;
    transform.scale.y *= scale_multiplier.y;
    transform.scale.z *= scale_multiplier.z;
    return henka_scene_set_entity_transform(scene, entity, transform);
}

henka_result henka_scene_set_entity_mesh(henka_scene* scene, henka_entity entity, henka_mesh* mesh)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->mesh = mesh;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_clear_entity_mesh(henka_scene* scene, henka_entity entity)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->mesh = NULL;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_material(henka_scene* scene, henka_entity entity, henka_material material)
{
    char* material_name;
    henka_scene_entity_record* record;
    henka_result result;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || !henka_scene_material_is_valid(material))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    material_name = NULL;
    if (material.name != NULL && material.name[0] != '\0')
    {
        result = henka_scene_duplicate_text(material.name, &material_name);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }

    henka_free(record->material_name);
    record->material_name = material_name;
    record->material = material;
    record->material.name = material_name != NULL ? material_name : "Material";
    if (record->material_asset != NULL)
    {
        record->material_asset_overridden = true;
    }
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_material_asset(
    henka_scene* scene,
    henka_entity entity,
    const henka_material_asset* asset)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->material_asset = asset;
    record->material_asset_revision = 0U;
    record->material_asset_overridden = false;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_apply_material_asset(
    henka_scene* scene,
    henka_entity entity,
    const henka_material_asset* asset,
    henka_material material,
    uint64_t revision)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || asset == NULL || revision == 0U ||
        !henka_scene_material_is_valid(material))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* The asset owns the stable name storage. This allocation-free refresh
     * path is safe to call from the frame lifecycle. */
    henka_free(record->material_name);
    record->material_name = NULL;
    record->material = material;
    record->material.name = material.name != NULL && material.name[0] != '\0'
        ? material.name
        : "Material";
    record->material_asset = asset;
    record->material_asset_revision = revision;
    record->material_asset_overridden = false;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_material_asset_state(
    const henka_scene* scene,
    henka_entity entity,
    uint64_t* out_revision,
    bool* out_overridden)
{
    const henka_scene_entity_record* record;

    if (out_revision != NULL) *out_revision = 0U;
    if (out_overridden != NULL) *out_overridden = false;
    if (scene == NULL || out_revision == NULL || out_overridden == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_revision = record->material_asset_revision;
    *out_overridden = record->material_asset_overridden;
    return HENKA_SUCCESS;
}

henka_result henka_scene_restore_material_asset_state(
    henka_scene* scene,
    henka_entity entity,
    const henka_material_asset* asset,
    uint64_t revision,
    bool overridden)
{
    henka_scene_entity_record* record = henka_scene_get_entity_record(scene, entity);

    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->material_asset = asset;
    record->material_asset_revision = revision;
    record->material_asset_overridden = overridden;
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_name(henka_scene* scene, henka_entity entity, const char* name)
{
    char* copy;
    henka_scene_entity_record* record;
    henka_result result;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_scene_duplicate_text(name, &copy);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    henka_free(record->name);
    record->name = copy;
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_tag(henka_scene* scene, henka_entity entity, const char* tag)
{
    char* copy;
    henka_scene_entity_record* record;
    henka_result result;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_scene_duplicate_text(tag, &copy);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    henka_free(record->tag);
    record->tag = copy;
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_visible(henka_scene* scene, henka_entity entity, bool visible)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->visible = visible;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_local_bounds(henka_scene* scene, henka_entity entity, henka_bounds bounds)
{
    henka_scene_entity_record* record;
    henka_bounds world_bounds;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || !henka_scene_bounds_are_valid(bounds) ||
        !henka_scene_try_transform_bounds(bounds, record->transform, &world_bounds))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->local_bounds = bounds;
    record->has_local_bounds = true;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_clear_entity_local_bounds(henka_scene* scene, henka_entity entity)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->has_local_bounds = false;
    record->local_bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_interaction(henka_scene* scene, henka_entity entity, const henka_interaction_desc* interaction)
{
    henka_scene_entity_record* record;
    char* prompt_copy;
    henka_result result;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL || interaction == NULL ||
        !henka_is_finite_float(interaction->max_distance) ||
        interaction->max_distance < 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_scene_duplicate_text(interaction->prompt, &prompt_copy);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    henka_free(record->interaction_prompt);
    record->interaction_prompt = prompt_copy;
    record->interaction = *interaction;
    record->interaction.prompt = prompt_copy;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_flags(henka_scene* scene, henka_entity entity, uint32_t flags)
{
    henka_scene_entity_record* record;

    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->flags = flags;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

bool henka_scene_is_entity_helper(const henka_scene* scene, henka_entity entity)
{
    const henka_scene_entity_record* record;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return false;
    }

    return (record->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U;
}

bool henka_scene_is_entity_transform_locked(const henka_scene* scene, henka_entity entity)
{
    const henka_scene_entity_record* record;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return false;
    }

    return (record->flags & HENKA_SCENE_ENTITY_FLAG_TRANSFORM_LOCKED) != 0U;
}

henka_interaction_result henka_scene_can_interact(const henka_scene* scene, henka_entity entity, henka_vec3 observer_position)
{
    const henka_scene_entity_record* record;
    float distance;

    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL || !henka_scene_vec3_is_finite(observer_position))
    {
        return HENKA_INTERACTION_RESULT_UNAVAILABLE;
    }

    if (!record->interaction.enabled)
    {
        return HENKA_INTERACTION_RESULT_DISABLED;
    }

    distance = henka_vec3_length(henka_vec3_subtract(record->transform.position, observer_position));
    if (distance > record->interaction.max_distance)
    {
        return HENKA_INTERACTION_RESULT_OUT_OF_RANGE;
    }

    return HENKA_INTERACTION_RESULT_AVAILABLE;
}

henka_result henka_scene_pick_entity(const henka_scene* scene, henka_ray ray, henka_entity* out_entity, float* out_distance)
{
    float best_distance;
    size_t index;
    bool found;

    if (out_entity == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_entity = HENKA_INVALID_ENTITY;
    if (out_distance != NULL)
    {
        *out_distance = 0.0f;
    }

    if (scene == NULL ||
        !henka_scene_vec3_is_finite(ray.origin) ||
        !henka_scene_vec3_is_finite(ray.direction))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    ray.direction = henka_vec3_normalize(ray.direction);
    if (henka_vec3_length(ray.direction) <= 0.000001f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    best_distance = 1000000.0f;
    found = false;
    for (index = 0U; index < scene->entity_capacity; ++index)
    {
        henka_bounds world_bounds;
        float distance;
        const henka_scene_entity_record* record = &scene->entities[index];

        if (!record->active || !record->visible || !record->has_local_bounds || (record->flags & HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U)
        {
            continue;
        }

        if (!henka_scene_try_transform_bounds(record->local_bounds, record->transform, &world_bounds) ||
            !henka_scene_ray_intersects_bounds(ray, world_bounds, &distance))
        {
            continue;
        }

        if (!found || distance < best_distance)
        {
            best_distance = distance;
            *out_entity = henka_scene_make_entity(
                index,
                scene->entities[index].generation);
            found = true;
        }
    }

    if (!found)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    if (out_distance != NULL)
    {
        *out_distance = best_distance;
    }

    return HENKA_SUCCESS;
}

henka_result henka_scene_set_camera(henka_scene* scene, const henka_camera* camera)
{
    if (scene == NULL || !henka_camera_is_valid(camera))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    scene->camera = *camera;
    scene->has_camera = true;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

void henka_scene_set_light_direction(henka_scene* scene, henka_vec3 light_direction)
{
    henka_vec3 normalized_direction;

    if (scene == NULL ||
        !henka_is_finite_float(light_direction.x) ||
        !henka_is_finite_float(light_direction.y) ||
        !henka_is_finite_float(light_direction.z))
    {
        return;
    }

    normalized_direction = henka_vec3_normalize(light_direction);
    if (henka_vec3_length(normalized_direction) <= 0.000001f)
    {
        return;
    }

    scene->light_direction = normalized_direction;
    henka_scene_bump_render_revision(scene);
}

void henka_scene_set_light_color(henka_scene* scene, henka_vec3 light_color)
{
    if (scene != NULL && henka_is_finite_float(light_color.x) &&
        henka_is_finite_float(light_color.y) && henka_is_finite_float(light_color.z) &&
        light_color.x >= 0.0f && light_color.y >= 0.0f && light_color.z >= 0.0f &&
        light_color.x <= 1.0f && light_color.y <= 1.0f && light_color.z <= 1.0f)
    {
        scene->light_color = light_color;
        henka_scene_bump_render_revision(scene);
    }
}

void henka_scene_set_light_intensity(henka_scene* scene, float light_intensity)
{
    if (scene != NULL && henka_is_finite_float(light_intensity) && light_intensity >= 0.0f && light_intensity <= 10000.0f)
    {
        scene->light_intensity = light_intensity;
        henka_scene_bump_render_revision(scene);
    }
}

void henka_scene_set_ambient_color(henka_scene* scene, henka_vec3 ambient_color)
{
    if (scene != NULL &&
        henka_is_finite_float(ambient_color.x) &&
        henka_is_finite_float(ambient_color.y) &&
        henka_is_finite_float(ambient_color.z) &&
        ambient_color.x >= 0.0f && ambient_color.y >= 0.0f && ambient_color.z >= 0.0f &&
        ambient_color.x <= 16.0f && ambient_color.y <= 16.0f && ambient_color.z <= 16.0f)
    {
        scene->ambient_color = ambient_color;
        henka_scene_bump_render_revision(scene);
    }
}

henka_result henka_scene_set_environment(
    henka_scene* scene,
    henka_scene_environment_desc environment)
{
    henka_scene_environment_desc defaults;
    float sun_direction_length;

    /* Preserve source compatibility for callers that used the original
     * six-field aggregate initializer before environment authority gained
     * procedural/time fields. An explicit new descriptor should come from
     * henka_scene_environment_default(), so zero remains a valid authored
     * value for individual scattering controls. */
    if (environment.day_length_seconds == 0.0f &&
        environment.time_of_day_hours == 0.0f &&
        environment.time_scale == 0.0f &&
        environment.mode == HENKA_SCENE_ENVIRONMENT_GRADIENT &&
        !environment.time_of_day_enabled &&
        !environment.sun.enabled &&
        !environment.moon.enabled &&
        !environment.stars.enabled &&
        environment.atmosphere.rayleigh_scattering == 0.0f &&
        environment.atmosphere.mie_scattering == 0.0f &&
        environment.atmosphere.mie_anisotropy == 0.0f &&
        environment.atmosphere.density == 0.0f &&
        environment.atmosphere.turbidity == 0.0f &&
        environment.atmosphere.ozone_absorption == 0.0f &&
        environment.atmosphere.atmosphere_height == 0.0f &&
        environment.atmosphere.planet_radius == 0.0f &&
        environment.atmosphere.ground_albedo.x == 0.0f &&
        environment.atmosphere.ground_albedo.y == 0.0f &&
        environment.atmosphere.ground_albedo.z == 0.0f &&
        environment.atmosphere.horizon_intensity == 0.0f)
    {
        defaults = henka_scene_environment_default();
        environment.mode = environment.hdr_texture != NULL ?
            HENKA_SCENE_ENVIRONMENT_HDRI : HENKA_SCENE_ENVIRONMENT_GRADIENT;
        environment.atmosphere = defaults.atmosphere;
        environment.sun = defaults.sun;
        environment.moon = defaults.moon;
        environment.stars = defaults.stars;
        environment.time_of_day_hours = defaults.time_of_day_hours;
        environment.day_length_seconds = defaults.day_length_seconds;
        environment.time_scale = defaults.time_scale;
        environment.time_of_day_enabled = defaults.time_of_day_enabled;
    }

    if (scene == NULL ||
        !henka_is_finite_float(environment.ground_color.x) ||
        !henka_is_finite_float(environment.ground_color.y) ||
        !henka_is_finite_float(environment.ground_color.z) ||
        !henka_is_finite_float(environment.horizon_color.x) ||
        !henka_is_finite_float(environment.horizon_color.y) ||
        !henka_is_finite_float(environment.horizon_color.z) ||
        !henka_is_finite_float(environment.zenith_color.x) ||
        !henka_is_finite_float(environment.zenith_color.y) ||
        !henka_is_finite_float(environment.zenith_color.z) ||
        !henka_is_finite_float(environment.intensity) ||
        !henka_is_finite_float(environment.hdr_rotation) ||
        environment.ground_color.x < 0.0f || environment.ground_color.x > 16.0f ||
        environment.ground_color.y < 0.0f || environment.ground_color.y > 16.0f ||
        environment.ground_color.z < 0.0f || environment.ground_color.z > 16.0f ||
        environment.horizon_color.x < 0.0f || environment.horizon_color.x > 16.0f ||
        environment.horizon_color.y < 0.0f || environment.horizon_color.y > 16.0f ||
        environment.horizon_color.z < 0.0f || environment.horizon_color.z > 16.0f ||
        environment.zenith_color.x < 0.0f || environment.zenith_color.x > 16.0f ||
        environment.zenith_color.y < 0.0f || environment.zenith_color.y > 16.0f ||
        environment.zenith_color.z < 0.0f || environment.zenith_color.z > 16.0f ||
        environment.intensity < 0.0f || environment.intensity > 16.0f ||
        environment.hdr_rotation < -1000000.0f ||
        environment.hdr_rotation > 1000000.0f ||
        environment.mode < HENKA_SCENE_ENVIRONMENT_GRADIENT ||
        environment.mode > HENKA_SCENE_ENVIRONMENT_PROCEDURAL ||
        !henka_is_finite_float(environment.atmosphere.rayleigh_scattering) ||
        !henka_is_finite_float(environment.atmosphere.mie_scattering) ||
        !henka_is_finite_float(environment.atmosphere.mie_anisotropy) ||
        !henka_is_finite_float(environment.atmosphere.density) ||
        !henka_is_finite_float(environment.atmosphere.turbidity) ||
        !henka_is_finite_float(environment.atmosphere.ozone_absorption) ||
        !henka_is_finite_float(environment.atmosphere.atmosphere_height) ||
        !henka_is_finite_float(environment.atmosphere.planet_radius) ||
        !henka_scene_vec3_is_finite(environment.atmosphere.ground_albedo) ||
        !henka_is_finite_float(environment.atmosphere.horizon_intensity) ||
        environment.atmosphere.rayleigh_scattering < 0.0f ||
        environment.atmosphere.rayleigh_scattering > 8.0f ||
        environment.atmosphere.mie_scattering < 0.0f ||
        environment.atmosphere.mie_scattering > 8.0f ||
        environment.atmosphere.mie_anisotropy < -0.99f ||
        environment.atmosphere.mie_anisotropy > 0.99f ||
        environment.atmosphere.density < 0.0f ||
        environment.atmosphere.density > 8.0f ||
        environment.atmosphere.turbidity < 0.0f ||
        environment.atmosphere.turbidity > 32.0f ||
        environment.atmosphere.ozone_absorption < 0.0f ||
        environment.atmosphere.ozone_absorption > 8.0f ||
        environment.atmosphere.atmosphere_height <= 0.0f ||
        environment.atmosphere.atmosphere_height > 100000.0f ||
        environment.atmosphere.planet_radius <= 0.0f ||
        environment.atmosphere.planet_radius > 1000000000.0f ||
        environment.atmosphere.ground_albedo.x < 0.0f ||
        environment.atmosphere.ground_albedo.y < 0.0f ||
        environment.atmosphere.ground_albedo.z < 0.0f ||
        environment.atmosphere.ground_albedo.x > 16.0f ||
        environment.atmosphere.ground_albedo.y > 16.0f ||
        environment.atmosphere.ground_albedo.z > 16.0f ||
        environment.atmosphere.horizon_intensity < 0.0f ||
        environment.atmosphere.horizon_intensity > 16.0f ||
        !henka_is_finite_float(environment.time_of_day_hours) ||
        !henka_is_finite_float(environment.day_length_seconds) ||
        !henka_is_finite_float(environment.time_scale) ||
        environment.time_of_day_hours < 0.0f ||
        environment.time_of_day_hours >= 24.0f ||
        environment.day_length_seconds <= 0.0f ||
        environment.day_length_seconds > 604800.0f ||
        environment.time_scale < -64.0f ||
        environment.time_scale > 64.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (environment.sun.enabled)
    {
        sun_direction_length = henka_vec3_length(environment.sun.direction);
        if (!henka_scene_vec3_is_finite(environment.sun.direction) ||
            !henka_scene_vec3_is_finite(environment.sun.color) ||
            !henka_is_finite_float(environment.sun.intensity) ||
            !henka_is_finite_float(environment.sun.angular_radius) ||
            sun_direction_length <= 0.000001f ||
            environment.sun.color.x < 0.0f || environment.sun.color.y < 0.0f || environment.sun.color.z < 0.0f ||
            environment.sun.color.x > 16.0f || environment.sun.color.y > 16.0f || environment.sun.color.z > 16.0f ||
            environment.sun.intensity < 0.0f || environment.sun.intensity > 64.0f ||
            environment.sun.angular_radius < 0.0001f || environment.sun.angular_radius > 0.5f)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    if (environment.moon.enabled)
    {
        sun_direction_length = henka_vec3_length(environment.moon.direction);
        if (!henka_scene_vec3_is_finite(environment.moon.direction) ||
            !henka_scene_vec3_is_finite(environment.moon.color) ||
            !henka_is_finite_float(environment.moon.intensity) ||
            !henka_is_finite_float(environment.moon.angular_radius) ||
            sun_direction_length <= 0.000001f ||
            environment.moon.color.x < 0.0f || environment.moon.color.y < 0.0f || environment.moon.color.z < 0.0f ||
            environment.moon.color.x > 16.0f || environment.moon.color.y > 16.0f || environment.moon.color.z > 16.0f ||
            environment.moon.intensity < 0.0f || environment.moon.intensity > 16.0f ||
            environment.moon.angular_radius < 0.0001f || environment.moon.angular_radius > 0.5f)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    if (!henka_is_finite_float(environment.stars.intensity) ||
        !henka_is_finite_float(environment.stars.rotation) ||
        environment.stars.intensity < 0.0f || environment.stars.intensity > 16.0f ||
        environment.stars.rotation < -1000000.0f || environment.stars.rotation > 1000000.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    scene->environment = environment;
    if (environment.sun.enabled)
    {
        henka_vec3 direction = henka_vec3_normalize(environment.sun.direction);
        float elevation = 1.0f;
        if (!environment.sun.manual_direction)
        {
            const float pi = 3.14159265358979323846f;
            float phase = (environment.time_of_day_hours - 6.0f) * pi / 12.0f;
            elevation = sinf(phase);
            {
                float horizontal = sqrtf(fmaxf(0.0f, 1.0f - elevation * elevation));
                float azimuth = environment.time_of_day_hours * pi / 12.0f;
                direction = (henka_vec3){
                    cosf(azimuth) * horizontal,
                    -elevation,
                    sinf(azimuth) * horizontal};
            }
        }
        scene->light_direction = direction;
        scene->light_color = environment.sun.color;
        scene->light_intensity = environment.sun.intensity *
            (environment.sun.manual_direction ? 1.0f : fmaxf(elevation, 0.03f));
    }
    if (scene->environment.moon.enabled && !scene->environment.moon.manual_direction)
    {
        scene->environment.moon.direction = scene->environment.sun.enabled ?
            henka_vec3_scale(scene->light_direction, -1.0f) :
            henka_vec3_normalize(environment.moon.direction);
    }
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_environment_preset(
    henka_scene* scene,
    henka_scene_environment_preset preset)
{
    henka_scene_environment_desc environment;
    henka_scene_environment_desc current;

    if (scene == NULL || preset < HENKA_SCENE_ENVIRONMENT_PRESET_CLEAR_MIDDAY ||
        preset >= HENKA_SCENE_ENVIRONMENT_PRESET_COUNT)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    environment = henka_scene_environment_default();
    if (henka_scene_get_environment(scene, &current) == HENKA_SUCCESS)
    {
        /* The preset changes environment presentation but does not take
         * ownership of or discard the existing manager-owned source. */
        environment.hdr_texture = current.hdr_texture;
    }
    environment.mode = HENKA_SCENE_ENVIRONMENT_PROCEDURAL;
    switch (preset)
    {
        case HENKA_SCENE_ENVIRONMENT_PRESET_GOLDEN_HOUR:
            environment.ground_color = (henka_vec3){0.12f, 0.055f, 0.025f};
            environment.horizon_color = (henka_vec3){0.72f, 0.26f, 0.08f};
            environment.zenith_color = (henka_vec3){0.10f, 0.13f, 0.28f};
            environment.intensity = 1.25f;
            environment.atmosphere.mie_scattering = 0.14f;
            environment.atmosphere.turbidity = 3.5f;
            environment.sun.color = (henka_vec3){1.0f, 0.68f, 0.42f};
            environment.sun.intensity = 2.0f;
            environment.moon.enabled = false;
            environment.stars.enabled = false;
            environment.time_of_day_hours = 17.5f;
            break;
        case HENKA_SCENE_ENVIRONMENT_PRESET_MOONLIT_NIGHT:
            environment.ground_color = (henka_vec3){0.006f, 0.009f, 0.022f};
            environment.horizon_color = (henka_vec3){0.035f, 0.055f, 0.12f};
            environment.zenith_color = (henka_vec3){0.004f, 0.008f, 0.025f};
            environment.intensity = 0.55f;
            environment.sun.intensity = 0.15f;
            environment.moon.intensity = 0.35f;
            environment.stars.intensity = 0.35f;
            environment.time_of_day_hours = 0.5f;
            break;
        case HENKA_SCENE_ENVIRONMENT_PRESET_ALIEN_HAZE:
            environment.ground_color = (henka_vec3){0.035f, 0.008f, 0.06f};
            environment.horizon_color = (henka_vec3){0.34f, 0.05f, 0.24f};
            environment.zenith_color = (henka_vec3){0.02f, 0.08f, 0.16f};
            environment.intensity = 1.4f;
            environment.atmosphere.rayleigh_scattering = 0.55f;
            environment.atmosphere.mie_scattering = 0.22f;
            environment.atmosphere.mie_anisotropy = -0.35f;
            environment.atmosphere.turbidity = 7.0f;
            environment.atmosphere.ground_albedo = (henka_vec3){0.16f, 0.04f, 0.22f};
            environment.sun.manual_direction = true;
            environment.sun.direction = (henka_vec3){-0.25f, -0.82f, 0.52f};
            environment.sun.color = (henka_vec3){0.46f, 0.72f, 1.0f};
            environment.sun.intensity = 2.4f;
            environment.moon.enabled = true;
            environment.moon.color = (henka_vec3){1.0f, 0.30f, 0.72f};
            environment.moon.intensity = 0.22f;
            environment.moon.angular_radius = 0.012f;
            environment.stars.intensity = 0.20f;
            environment.time_of_day_hours = 20.0f;
            break;
        case HENKA_SCENE_ENVIRONMENT_PRESET_CLEAR_MIDDAY:
        case HENKA_SCENE_ENVIRONMENT_PRESET_COUNT:
        default:
            environment.time_of_day_hours = 12.0f;
            break;
    }
    environment.time_of_day_enabled = false;
    return henka_scene_set_environment(scene, environment);
}

henka_result henka_scene_get_environment(
    const henka_scene* scene,
    henka_scene_environment_desc* out_environment)
{
    if (scene == NULL || out_environment == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_environment = scene->environment;
    return HENKA_SUCCESS;
}

henka_result henka_scene_advance_environment_time(
    henka_scene* scene,
    float delta_seconds)
{
    henka_scene_environment_desc environment;
    float hours;

    if (scene == NULL || !henka_is_finite_float(delta_seconds) ||
        delta_seconds < -86400.0f || delta_seconds > 86400.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    environment = scene->environment;
    if (!environment.time_of_day_enabled)
    {
        return HENKA_SUCCESS;
    }
    hours = environment.time_of_day_hours +
        delta_seconds * environment.time_scale / environment.day_length_seconds * 24.0f;
    hours = fmodf(hours, 24.0f);
    if (hours < 0.0f)
    {
        hours += 24.0f;
    }
    environment.time_of_day_hours = hours;
    return henka_scene_set_environment(scene, environment);
}

static henka_result henka_scene_validate_reflection_probe(
    const henka_scene_reflection_probe_desc* probe)
{
    if (probe == NULL ||
        !henka_scene_vec3_is_finite(probe->position) ||
        !henka_scene_vec3_is_finite(probe->extents) ||
        !henka_is_finite_float(probe->influence) ||
        probe->extents.x <= 0.0f || probe->extents.y <= 0.0f || probe->extents.z <= 0.0f ||
        probe->extents.x > 65536.0f || probe->extents.y > 65536.0f || probe->extents.z > 65536.0f ||
        probe->influence < 0.0f || probe->influence > 1000000.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_SUCCESS;
}

henka_result henka_scene_add_reflection_probe(
    henka_scene* scene,
    henka_scene_reflection_probe_desc probe,
    uint32_t* out_probe_index)
{
    uint32_t index;

    if (out_probe_index != NULL)
    {
        *out_probe_index = UINT32_MAX;
    }
    if (scene == NULL || out_probe_index == NULL ||
        henka_scene_validate_reflection_probe(&probe) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < HENKA_SCENE_MAX_REFLECTION_PROBES; ++index)
    {
        if (!scene->reflection_probe_active[index])
        {
            scene->reflection_probes[index] = probe;
            scene->reflection_probe_active[index] = true;
            *out_probe_index = index;
            henka_scene_bump_render_revision(scene);
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_LIMIT;
}

henka_result henka_scene_update_reflection_probe(
    henka_scene* scene,
    uint32_t probe_index,
    henka_scene_reflection_probe_desc probe)
{
    if (scene == NULL || probe_index >= HENKA_SCENE_MAX_REFLECTION_PROBES ||
        !scene->reflection_probe_active[probe_index] ||
        henka_scene_validate_reflection_probe(&probe) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    scene->reflection_probes[probe_index] = probe;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_remove_reflection_probe(
    henka_scene* scene,
    uint32_t probe_index)
{
    if (scene == NULL || probe_index >= HENKA_SCENE_MAX_REFLECTION_PROBES ||
        !scene->reflection_probe_active[probe_index])
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(&scene->reflection_probes[probe_index], 0, sizeof(scene->reflection_probes[probe_index]));
    scene->reflection_probe_active[probe_index] = false;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_reflection_probe(
    const henka_scene* scene,
    uint32_t probe_index,
    henka_scene_reflection_probe_desc* out_probe)
{
    if (scene == NULL || out_probe == NULL ||
        probe_index >= HENKA_SCENE_MAX_REFLECTION_PROBES ||
        !scene->reflection_probe_active[probe_index])
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_probe = scene->reflection_probes[probe_index];
    return HENKA_SUCCESS;
}

static henka_result henka_scene_validate_lod(const henka_scene_lod_desc* lod)
{
    uint32_t index;
    float previous_distance = 0.0f;

    if (lod == NULL || lod->level_count > HENKA_SCENE_MAX_LOD_LEVELS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (lod->level_count == 0U)
    {
        return HENKA_SUCCESS;
    }
    for (index = 0U; index < lod->level_count; ++index)
    {
        if (lod->meshes[index] == NULL ||
            !henka_is_finite_float(lod->max_distances[index]) ||
            lod->max_distances[index] <= previous_distance ||
            lod->max_distances[index] > 65536.0f)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        previous_distance = lod->max_distances[index];
    }
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_entity_lod(
    henka_scene* scene,
    henka_entity entity,
    henka_scene_lod_desc lod)
{
    henka_scene_entity_record* record;

    if (scene == NULL || henka_scene_validate_lod(&lod) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record = henka_scene_get_entity_record(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->lod = lod;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_entity_lod(
    const henka_scene* scene,
    henka_entity entity,
    henka_scene_lod_desc* out_lod)
{
    const henka_scene_entity_record* record;

    if (scene == NULL || out_lod == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record = henka_scene_get_entity_record_const(scene, entity);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_lod = record->lod;
    return HENKA_SUCCESS;
}

static henka_result henka_scene_validate_light(henka_scene_light_desc* light)
{
    henka_vec3 direction;

    if (light == NULL || light->type > HENKA_SCENE_LIGHT_SPOT ||
        !henka_scene_vec3_is_finite(light->position) ||
        !henka_scene_vec3_is_finite(light->direction) ||
        !henka_scene_vec3_is_finite(light->color) ||
        !henka_is_finite_float(light->intensity) ||
        !henka_is_finite_float(light->range) ||
        !henka_is_finite_float(light->inner_cone_cosine) ||
        !henka_is_finite_float(light->outer_cone_cosine) ||
        light->intensity < 0.0f || light->intensity > 100000.0f ||
        light->range <= 0.0f || light->range > 100000.0f ||
        light->color.x < 0.0f || light->color.y < 0.0f || light->color.z < 0.0f ||
        light->color.x > 16.0f || light->color.y > 16.0f || light->color.z > 16.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    direction = henka_vec3_normalize(light->direction);
    if (henka_vec3_length(direction) <= 0.000001f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    light->direction = direction;
    if (light->type == HENKA_SCENE_LIGHT_SPOT &&
        (light->inner_cone_cosine < 0.0f || light->inner_cone_cosine > 1.0f ||
         light->outer_cone_cosine < 0.0f || light->outer_cone_cosine > 1.0f ||
         light->inner_cone_cosine < light->outer_cone_cosine))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_SUCCESS;
}

henka_result henka_scene_add_light(
    henka_scene* scene,
    henka_scene_light_desc light,
    uint32_t* out_light_index)
{
    uint32_t index;

    if (out_light_index != NULL)
    {
        *out_light_index = UINT32_MAX;
    }
    if (scene == NULL || out_light_index == NULL ||
        henka_scene_validate_light(&light) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < HENKA_SCENE_MAX_LOCAL_LIGHTS; ++index)
    {
        if (!scene->local_light_active[index])
        {
            scene->local_lights[index] = light;
            scene->local_light_active[index] = true;
            *out_light_index = index;
            henka_scene_bump_render_revision(scene);
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_LIMIT;
}

henka_result henka_scene_update_light(
    henka_scene* scene,
    uint32_t light_index,
    henka_scene_light_desc light)
{
    if (scene == NULL || light_index >= HENKA_SCENE_MAX_LOCAL_LIGHTS ||
        !scene->local_light_active[light_index] ||
        henka_scene_validate_light(&light) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    scene->local_lights[light_index] = light;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_remove_light(
    henka_scene* scene,
    uint32_t light_index)
{
    if (scene == NULL || light_index >= HENKA_SCENE_MAX_LOCAL_LIGHTS ||
        !scene->local_light_active[light_index])
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(&scene->local_lights[light_index], 0, sizeof(scene->local_lights[light_index]));
    scene->local_light_active[light_index] = false;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_light(
    const henka_scene* scene,
    uint32_t light_index,
    henka_scene_light_desc* out_light)
{
    if (scene == NULL || out_light == NULL ||
        light_index >= HENKA_SCENE_MAX_LOCAL_LIGHTS ||
        !scene->local_light_active[light_index])
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_light = scene->local_lights[light_index];
    return HENKA_SUCCESS;
}

henka_result henka_scene_set_fog(henka_scene* scene, henka_scene_fog_desc fog)
{
    if (scene == NULL ||
        fog.mode > HENKA_SCENE_FOG_EXPONENTIAL_SQUARED ||
        !henka_is_finite_float(fog.color.x) ||
        !henka_is_finite_float(fog.color.y) ||
        !henka_is_finite_float(fog.color.z) ||
        !henka_is_finite_float(fog.start_distance) ||
        !henka_is_finite_float(fog.end_distance) ||
        !henka_is_finite_float(fog.density) ||
        fog.color.x < 0.0f || fog.color.x > 16.0f ||
        fog.color.y < 0.0f || fog.color.y > 16.0f ||
        fog.color.z < 0.0f || fog.color.z > 16.0f ||
        fog.start_distance < 0.0f || fog.start_distance > 65536.0f ||
        fog.end_distance <= fog.start_distance || fog.end_distance > 65536.0f ||
        fog.density < 0.0f || fog.density > 1.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    scene->fog = fog;
    henka_scene_bump_render_revision(scene);
    return HENKA_SUCCESS;
}

henka_result henka_scene_get_fog(
    const henka_scene* scene,
    henka_scene_fog_desc* out_fog)
{
    if (scene == NULL || out_fog == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_fog = scene->fog;
    return HENKA_SUCCESS;
}
