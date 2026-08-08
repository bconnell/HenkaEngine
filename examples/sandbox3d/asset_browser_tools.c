#include "asset_browser_tools.h"

#include <stdio.h>
#include <string.h>

static const char* sandbox3d_asset_browser_usage_label(henka_texture_usage usage)
{
    switch (usage)
    {
        case HENKA_TEXTURE_USAGE_COLOR:
            return "Color";
        case HENKA_TEXTURE_USAGE_NORMAL:
            return "Normal";
        case HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS:
            return "Metal/Rough";
        case HENKA_TEXTURE_USAGE_OCCLUSION:
            return "Occlusion";
        case HENKA_TEXTURE_USAGE_EMISSIVE:
            return "Emissive";
        case HENKA_TEXTURE_USAGE_GENERIC_DATA:
            return "Generic";
        case HENKA_TEXTURE_USAGE_UI:
            return "UI";
        default:
            return "Unknown";
    }
}

static const char* sandbox3d_asset_browser_format_label(henka_texture_gpu_format format)
{
    switch (format)
    {
        case HENKA_TEXTURE_GPU_FORMAT_RGBA8:
            return "RGBA8";
        case HENKA_TEXTURE_GPU_FORMAT_BC1:
            return "BC1";
        case HENKA_TEXTURE_GPU_FORMAT_BC3:
            return "BC3";
        case HENKA_TEXTURE_GPU_FORMAT_BC5:
            return "BC5";
        case HENKA_TEXTURE_GPU_FORMAT_BC7:
            return "BC7";
        case HENKA_TEXTURE_GPU_FORMAT_ETC2_RGB:
            return "ETC2 RGB";
        case HENKA_TEXTURE_GPU_FORMAT_ETC2_RGBA:
            return "ETC2 RGBA";
        case HENKA_TEXTURE_GPU_FORMAT_ETC2_RG:
            return "ETC2 RG";
        case HENKA_TEXTURE_GPU_FORMAT_ASTC_4X4:
            return "ASTC 4x4";
        case HENKA_TEXTURE_GPU_FORMAT_UNKNOWN:
        default:
            return "Unknown";
    }
}

static henka_texture* sandbox3d_material_texture_for_slot(
    const henka_material* material,
    henka_material_texture_slot slot)
{
    if (material == NULL)
    {
        return NULL;
    }

    switch (slot)
    {
        case HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR:
            return material->base_color_texture;
        case HENKA_MATERIAL_TEXTURE_SLOT_NORMAL:
            return material->normal_texture;
        case HENKA_MATERIAL_TEXTURE_SLOT_METALLIC_ROUGHNESS:
            return material->metallic_roughness_texture;
        case HENKA_MATERIAL_TEXTURE_SLOT_OCCLUSION:
            return material->occlusion_texture;
        case HENKA_MATERIAL_TEXTURE_SLOT_EMISSIVE:
            return material->emissive_texture;
        default:
            return NULL;
    }
}

static henka_texture_usage sandbox3d_material_texture_slot_usage(
    henka_material_texture_slot slot)
{
    switch (slot)
    {
        case HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR:
            return HENKA_TEXTURE_USAGE_COLOR;
        case HENKA_MATERIAL_TEXTURE_SLOT_NORMAL:
            return HENKA_TEXTURE_USAGE_NORMAL;
        case HENKA_MATERIAL_TEXTURE_SLOT_METALLIC_ROUGHNESS:
            return HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS;
        case HENKA_MATERIAL_TEXTURE_SLOT_OCCLUSION:
            return HENKA_TEXTURE_USAGE_OCCLUSION;
        case HENKA_MATERIAL_TEXTURE_SLOT_EMISSIVE:
            return HENKA_TEXTURE_USAGE_EMISSIVE;
        default:
            return HENKA_TEXTURE_USAGE_GENERIC_DATA;
    }
}

static bool sandbox3d_asset_browser_type_matches(
    henka_asset_type requested_type,
    henka_asset_type metadata_type)
{
    return requested_type != HENKA_ASSET_TYPE_UNKNOWN
        ? metadata_type == requested_type
        : metadata_type != HENKA_ASSET_TYPE_UNKNOWN;
}

static char sandbox3d_asset_browser_ascii_lower(char value)
{
    return value >= 'A' && value <= 'Z' ? (char)(value + ('a' - 'A')) : value;
}

static bool sandbox3d_asset_browser_source_identity_equal(
    const char* left,
    const char* right)
{
    size_t index;

    if (left == NULL || right == NULL || left[0] == '\0' || right[0] == '\0')
    {
        return false;
    }
    for (index = 0U; left[index] != '\0' && right[index] != '\0'; ++index)
    {
        if (sandbox3d_asset_browser_ascii_lower(left[index]) !=
            sandbox3d_asset_browser_ascii_lower(right[index]))
        {
            return false;
        }
    }
    return left[index] == '\0' && right[index] == '\0';
}

static bool sandbox3d_asset_browser_is_first_identity(
    const henka_asset_manager* manager,
    size_t metadata_index,
    const henka_asset_metadata* metadata)
{
    size_t index;

    if (manager == NULL || metadata == NULL || metadata->source_path == NULL ||
        metadata->source_path[0] == '\0')
    {
        return true;
    }
    for (index = 0U; index < metadata_index; ++index)
    {
        henka_asset_metadata previous;
        if (henka_assets_get_metadata_at_index(manager, index, &previous) == HENKA_SUCCESS &&
            previous.type == metadata->type &&
            sandbox3d_asset_browser_source_identity_equal(
                previous.source_path, metadata->source_path))
        {
            return false;
        }
    }
    return true;
}

size_t sandbox3d_asset_browser_collect(
    const henka_asset_manager* manager,
    henka_asset_type type,
    sandbox3d_asset_browser_item* out_items,
    size_t capacity)
{
    size_t count;
    size_t index;
    size_t metadata_count;

    if (manager == NULL)
    {
        return 0U;
    }

    count = 0U;
    metadata_count = henka_assets_get_metadata_count(manager);
    for (index = 0U; index < metadata_count; ++index)
    {
        henka_asset_metadata metadata;

        if (henka_assets_get_metadata_at_index(manager, index, &metadata) != HENKA_SUCCESS ||
            !sandbox3d_asset_browser_type_matches(type, metadata.type))
        {
            continue;
        }
        if (!sandbox3d_asset_browser_is_first_identity(manager, index, &metadata))
        {
            continue;
        }

        if (out_items != NULL && count < capacity)
        {
            out_items[count].metadata_index = index;
            out_items[count].metadata = metadata;
        }
        ++count;
    }

    return count;
}

size_t sandbox3d_asset_browser_page_count(
    const henka_asset_manager* manager,
    henka_asset_type type,
    size_t page_size)
{
    const size_t total = sandbox3d_asset_browser_collect(manager, type, NULL, 0U);
    if (page_size == 0U)
    {
        return 0U;
    }
    return total / page_size + (total % page_size != 0U ? 1U : 0U);
}

size_t sandbox3d_asset_browser_collect_page(
    const henka_asset_manager* manager,
    henka_asset_type type,
    size_t page_index,
    size_t page_size,
    sandbox3d_asset_browser_item* out_items,
    size_t capacity)
{
    size_t total;
    size_t start;
    size_t end;
    size_t count;
    size_t ordinal;
    size_t index;

    if (manager == NULL || page_size == 0U ||
        (page_index > (size_t)-1 / page_size))
    {
        return 0U;
    }

    total = sandbox3d_asset_browser_collect(manager, type, NULL, 0U);
    start = page_index * page_size;
    if (start >= total)
    {
        return 0U;
    }
    end = start + page_size;
    if (end < start || end > total)
    {
        end = total;
    }

    count = 0U;
    ordinal = 0U;
    for (index = 0U; index < henka_assets_get_metadata_count(manager); ++index)
    {
        henka_asset_metadata metadata;
        if (henka_assets_get_metadata_at_index(manager, index, &metadata) != HENKA_SUCCESS ||
            !sandbox3d_asset_browser_type_matches(type, metadata.type))
        {
            continue;
        }
        if (!sandbox3d_asset_browser_is_first_identity(manager, index, &metadata))
        {
            continue;
        }
        if (ordinal >= start && ordinal < end)
        {
            if (out_items != NULL && count < capacity)
            {
                out_items[count].metadata_index = index;
                out_items[count].metadata = metadata;
            }
            ++count;
        }
        ++ordinal;
        if (ordinal >= end)
        {
            break;
        }
    }
    return count;
}

henka_result sandbox3d_format_material_texture_slot(
    const henka_asset_manager* manager,
    const henka_material* material,
    henka_material_texture_slot slot,
    sandbox3d_texture_slot_display* out_display)
{
    henka_asset_metadata metadata;
    henka_texture_info info;
    henka_texture* texture;
    henka_result metadata_result;
    henka_texture_usage usage;

    if (out_display != NULL)
    {
        memset(out_display, 0, sizeof(*out_display));
    }
    if (manager == NULL || material == NULL || out_display == NULL ||
        slot < HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR ||
        slot > HENKA_MATERIAL_TEXTURE_SLOT_EMISSIVE)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    usage = sandbox3d_material_texture_slot_usage(slot);
    snprintf(out_display->usage, sizeof(out_display->usage), "%s", sandbox3d_asset_browser_usage_label(usage));
    texture = sandbox3d_material_texture_for_slot(material, slot);
    if (texture == NULL)
    {
        out_display->assigned = false;
        snprintf(out_display->asset_identity, sizeof(out_display->asset_identity), "%s", "Unassigned");
        snprintf(out_display->dimensions, sizeof(out_display->dimensions), "%s", "-");
        snprintf(out_display->format, sizeof(out_display->format), "%s", "-");
        snprintf(out_display->mip_state, sizeof(out_display->mip_state), "%s", "-");
        snprintf(out_display->residency, sizeof(out_display->residency), "%s", "-");
        snprintf(out_display->state, sizeof(out_display->state), "%s", "Unassigned");
        return HENKA_SUCCESS;
    }

    if (henka_texture_get_info(texture, &info) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_display->assigned = true;
    metadata_result = henka_assets_get_texture_metadata(manager, texture, &metadata);
    snprintf(
        out_display->asset_identity,
        sizeof(out_display->asset_identity),
        "%s",
        metadata_result == HENKA_SUCCESS && metadata.source_path != NULL
            ? metadata.source_path
            : "(unregistered texture)");
    snprintf(out_display->dimensions, sizeof(out_display->dimensions), "%d x %d", info.width, info.height);
    snprintf(out_display->format, sizeof(out_display->format), "%s", sandbox3d_asset_browser_format_label(info.gpu_format));
    snprintf(out_display->mip_state, sizeof(out_display->mip_state), "%u/%u resident", info.resident_mip_count, info.mip_count);
    snprintf(out_display->residency, sizeof(out_display->residency), "%llu GPU bytes", (unsigned long long)info.resident_gpu_bytes);
    if (metadata_result == HENKA_SUCCESS && metadata.fallback)
    {
        snprintf(out_display->state, sizeof(out_display->state), "%s", "Fallback");
    }
    else if (metadata_result == HENKA_SUCCESS && !metadata.loaded)
    {
        snprintf(out_display->state, sizeof(out_display->state), "%s", "Error");
    }
    else if (info.backend_ready)
    {
        snprintf(out_display->state, sizeof(out_display->state), "%s", "Loaded");
    }
    else
    {
        snprintf(out_display->state, sizeof(out_display->state), "%s", "Not resident");
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_assign_material_instance_texture(
    const henka_asset_manager* manager,
    henka_material_instance* instance,
    henka_material_texture_slot slot,
    henka_texture* texture)
{
    henka_material_instance candidate;
    henka_result result;

    if (manager == NULL || instance == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (texture != NULL)
    {
        henka_asset_metadata metadata;
        if (henka_assets_get_texture_metadata(manager, texture, &metadata) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }

    candidate = *instance;
    result = henka_assets_material_instance_set_texture(&candidate, slot, texture);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    *instance = candidate;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_restore_material_instance_texture(
    const henka_asset_manager* manager,
    henka_material_instance* instance,
    henka_material_texture_slot slot)
{
    henka_material_instance candidate;
    henka_material_instance_parameter parameter;
    henka_result result;

    if (manager == NULL || instance == NULL ||
        slot < HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR ||
        slot > HENKA_MATERIAL_TEXTURE_SLOT_EMISSIVE)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    parameter = (henka_material_instance_parameter)(
        HENKA_MATERIAL_INSTANCE_BASE_COLOR_TEXTURE + slot);
    candidate = *instance;
    result = henka_assets_material_instance_reset_override(&candidate, parameter);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    *instance = candidate;
    return HENKA_SUCCESS;
}
