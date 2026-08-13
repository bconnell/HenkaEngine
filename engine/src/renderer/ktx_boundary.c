#include "../henka_internal.h"

#include <ktx.h>

#include <limits.h>
#include <string.h>

#include <henka/memory.h>

#include "../core/checked.h"

static bool henka_ktx_checked_rgba8_size(int width, int height, size_t* out_size)
{
    size_t pixels;
    if (out_size == NULL || width <= 0 || height <= 0 ||
        !henka_checked_size_multiply((size_t)width, (size_t)height, &pixels) ||
        !henka_checked_size_multiply(pixels, 4U, out_size)) return false;
    return true;
}

static bool henka_ktx_vk_format_is_srgb(uint32_t vk_format, bool* out_is_srgb)
{
    if (out_is_srgb == NULL)
        return false;
    switch (vk_format)
    {
        case 37U:  /* VK_FORMAT_R8G8B8A8_UNORM */
        case 131U: /* VK_FORMAT_BC1_RGB_UNORM_BLOCK */
        case 133U: /* VK_FORMAT_BC1_RGBA_UNORM_BLOCK */
        case 137U: /* VK_FORMAT_BC3_UNORM_BLOCK */
        case 141U: /* VK_FORMAT_BC5_UNORM_BLOCK */
        case 145U: /* VK_FORMAT_BC7_UNORM_BLOCK */
        case 147U: /* VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK */
        case 151U: /* VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK */
        case 155U: /* VK_FORMAT_EAC_R11G11_UNORM_BLOCK */
        case 157U: /* VK_FORMAT_ASTC_4x4_UNORM_BLOCK */
            *out_is_srgb = false;
            return true;
        case 43U:  /* VK_FORMAT_R8G8B8A8_SRGB */
        case 132U: /* VK_FORMAT_BC1_RGB_SRGB_BLOCK */
        case 134U: /* VK_FORMAT_BC1_RGBA_SRGB_BLOCK */
        case 138U: /* VK_FORMAT_BC3_SRGB_BLOCK */
        case 146U: /* VK_FORMAT_BC7_SRGB_BLOCK */
        case 148U: /* VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK */
        case 152U: /* VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK */
        case 158U: /* VK_FORMAT_ASTC_4x4_SRGB_BLOCK */
            *out_is_srgb = true;
            return true;
        default:
            return false;
    }
}

static bool henka_ktx_vk_format_to_gpu_format(
    uint32_t vk_format,
    uint32_t capabilities,
    henka_ktx2_gpu_format* out_format,
    bool* out_compressed)
{
    bool supported;

    if (out_format == NULL || out_compressed == NULL)
        return false;
    *out_compressed = false;
    supported = false;
    switch (vk_format)
    {
        case 37U:
        case 43U:
            *out_format = HENKA_KTX2_GPU_FORMAT_RGBA8;
            return true;
        case 131U:
        case 132U:
            *out_format = HENKA_KTX2_GPU_FORMAT_BC1;
            supported = (capabilities & HENKA_KTX2_CAPABILITY_BC1_3) != 0U;
            break;
        case 133U:
        case 134U:
            *out_format = HENKA_KTX2_GPU_FORMAT_BC1_RGBA;
            supported = (capabilities & HENKA_KTX2_CAPABILITY_BC1_3) != 0U;
            break;
        case 137U:
        case 138U:
            *out_format = HENKA_KTX2_GPU_FORMAT_BC3;
            supported = (capabilities & HENKA_KTX2_CAPABILITY_BC3) != 0U;
            break;
        case 141U:
            *out_format = HENKA_KTX2_GPU_FORMAT_BC5;
            supported = (capabilities & HENKA_KTX2_CAPABILITY_BC5) != 0U;
            break;
        case 145U:
        case 146U:
            *out_format = HENKA_KTX2_GPU_FORMAT_BC7;
            supported = (capabilities & HENKA_KTX2_CAPABILITY_BC7) != 0U;
            break;
        case 147U:
        case 148U:
            *out_format = HENKA_KTX2_GPU_FORMAT_ETC2_RGB;
            supported = (capabilities & HENKA_KTX2_CAPABILITY_ETC2) != 0U;
            break;
        case 151U:
        case 152U:
            *out_format = HENKA_KTX2_GPU_FORMAT_ETC2_RGBA;
            supported = (capabilities & HENKA_KTX2_CAPABILITY_ETC2) != 0U;
            break;
        case 155U:
            *out_format = HENKA_KTX2_GPU_FORMAT_ETC2_RG;
            supported = (capabilities & HENKA_KTX2_CAPABILITY_ETC2) != 0U;
            break;
        case 157U:
        case 158U:
            *out_format = HENKA_KTX2_GPU_FORMAT_ASTC_4X4;
            supported = (capabilities & HENKA_KTX2_CAPABILITY_ASTC_4X4) != 0U;
            break;
        default:
            return false;
    }
    if (!supported)
        return false;
    *out_compressed = true;
    return true;
}

static bool henka_ktx_expected_level_size(
    henka_ktx2_gpu_format format,
    int width,
    int height,
    size_t* out_size)
{
    size_t blocks_x;
    size_t blocks_y;
    size_t block_count;
    size_t bytes_per_block;

    if (out_size == NULL || width <= 0 || height <= 0)
        return false;
    if (format == HENKA_KTX2_GPU_FORMAT_RGBA8)
    {
        return henka_checked_size_multiply((size_t)width, (size_t)height, out_size) &&
            henka_checked_size_multiply(*out_size, 4U, out_size);
    }

    if (format == HENKA_KTX2_GPU_FORMAT_BC1 ||
        format == HENKA_KTX2_GPU_FORMAT_BC1_RGBA ||
        format == HENKA_KTX2_GPU_FORMAT_ETC2_RGB)
    {
        bytes_per_block = 8U;
    }
    else if (format == HENKA_KTX2_GPU_FORMAT_BC3 ||
        format == HENKA_KTX2_GPU_FORMAT_BC5 ||
        format == HENKA_KTX2_GPU_FORMAT_BC7 ||
        format == HENKA_KTX2_GPU_FORMAT_ETC2_RGBA ||
        format == HENKA_KTX2_GPU_FORMAT_ETC2_RG ||
        format == HENKA_KTX2_GPU_FORMAT_ASTC_4X4)
    {
        bytes_per_block = 16U;
    }
    else
    {
        return false;
    }

    blocks_x = ((size_t)width + 3U) / 4U;
    blocks_y = ((size_t)height + 3U) / 4U;
    if (!henka_checked_size_multiply(blocks_x, blocks_y, &block_count) ||
        !henka_checked_size_multiply(block_count, bytes_per_block, out_size))
    {
        return false;
    }
    return true;
}

static bool henka_ktx_select_transcode_target(
    henka_texture_usage usage,
    uint32_t capabilities,
    ktx_transcode_fmt_e* out_target)
{
    if (out_target == NULL)
        return false;
    if (usage == HENKA_TEXTURE_USAGE_NORMAL &&
        (capabilities & HENKA_KTX2_CAPABILITY_BC5) != 0U)
    {
        *out_target = KTX_TTF_BC5_RG;
        return true;
    }
    if ((capabilities & HENKA_KTX2_CAPABILITY_BC7) != 0U)
    {
        *out_target = KTX_TTF_BC7_RGBA;
        return true;
    }
    if ((capabilities & (HENKA_KTX2_CAPABILITY_BC1_3 | HENKA_KTX2_CAPABILITY_BC3)) ==
        (HENKA_KTX2_CAPABILITY_BC1_3 | HENKA_KTX2_CAPABILITY_BC3))
    {
        *out_target = KTX_TTF_BC1_OR_3;
        return true;
    }
    if ((capabilities & HENKA_KTX2_CAPABILITY_ETC2) != 0U)
    {
        *out_target = usage == HENKA_TEXTURE_USAGE_NORMAL ?
            KTX_TTF_ETC2_EAC_RG11 : KTX_TTF_ETC2_RGBA;
        return true;
    }
    if ((capabilities & HENKA_KTX2_CAPABILITY_ASTC_4X4) != 0U)
    {
        *out_target = KTX_TTF_ASTC_4x4_RGBA;
        return true;
    }
    return false;
}

void henka_ktx2_upload_dispose(henka_ktx2_upload* upload)
{
    if (upload == NULL)
        return;
    henka_free(upload->data);
    memset(upload, 0, sizeof(*upload));
}

henka_result henka_ktx2_prepare_upload_with_mip_limit(
    const unsigned char* data,
    size_t data_size,
    henka_texture_usage usage,
    henka_texture_color_space color_space,
    uint32_t capabilities,
    uint32_t max_resident_mips,
    henka_ktx2_upload* out_upload)
{
    ktxTexture2* texture = NULL;
    ktxTexture* base_texture;
    henka_ktx2_upload upload;
    henka_ktx2_gpu_format format;
    ktx_transcode_fmt_e transcode_target;
    uint32_t level;
    size_t total_size = 0U;
    size_t selected_size = 0U;
    uint32_t resident_level_count;
    bool is_srgb;
    bool compressed;
    bool transcoded = false;
    henka_result result = HENKA_ERROR_ASSET_SOURCE;

    if (out_upload != NULL)
        memset(out_upload, 0, sizeof(*out_upload));
    memset(&upload, 0, sizeof(upload));
    if (data == NULL || data_size == 0U || data_size > HENKA_MAX_TEXTURE_ENCODED_BYTES ||
        out_upload == NULL || usage > HENKA_TEXTURE_USAGE_UI ||
        color_space > HENKA_TEXTURE_COLOR_SPACE_LINEAR ||
        ktxTexture2_CreateFromMemory(
            data,
            (ktx_size_t)data_size,
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT | KTX_TEXTURE_CREATE_SKIP_KVDATA_BIT,
            &texture) != KTX_SUCCESS || texture == NULL)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }

    base_texture = ktxTexture(texture);
    if (texture->baseWidth == 0U || texture->baseHeight == 0U ||
        texture->baseWidth > HENKA_MAX_TEXTURE_DIMENSION ||
        texture->baseHeight > HENKA_MAX_TEXTURE_DIMENSION ||
        texture->baseDepth > 1U || texture->numDimensions != 2U ||
        texture->numLayers != 1U || texture->numFaces != 1U ||
        texture->numLevels == 0U || texture->numLevels > 16U)
    {
        goto cleanup;
    }

    if (ktxTexture2_NeedsTranscoding(texture))
    {
        transcoded = true;
        if (henka_ktx_select_transcode_target(usage, capabilities, &transcode_target))
        {
            if (ktxTexture2_TranscodeBasis(texture, transcode_target, 0U) != KTX_SUCCESS)
                goto cleanup;
        }
        else if (ktxTexture2_TranscodeBasis(texture, KTX_TTF_RGBA32, 0U) != KTX_SUCCESS)
        {
            goto cleanup;
        }
    }

    if (!henka_ktx_vk_format_is_srgb(texture->vkFormat, &is_srgb))
        goto cleanup;
    if (!transcoded && is_srgb != (color_space == HENKA_TEXTURE_COLOR_SPACE_SRGB))
        goto cleanup;
    if (!henka_ktx_vk_format_to_gpu_format(
            texture->vkFormat, capabilities, &format, &compressed))
    {
        /* RGBA8 is the only universally supported fallback. A native
         * compressed payload cannot be safely decoded by libktx here. */
        if (texture->vkFormat != 37U && texture->vkFormat != 43U)
            goto cleanup;
        format = HENKA_KTX2_GPU_FORMAT_RGBA8;
        compressed = false;
    }
    /* Basis transcode targets describe block layout, not the source transfer
     * function. Preserve the validated semantic color-space contract when
     * selecting the OpenGL internal format instead of trusting the target's
     * VK_FORMAT_UNORM/SRGB spelling. */
    if (transcoded)
        is_srgb = color_space == HENKA_TEXTURE_COLOR_SPACE_SRGB;

    resident_level_count = max_resident_mips == 0U ||
        max_resident_mips > texture->numLevels ? texture->numLevels : max_resident_mips;
    if (resident_level_count == 0U)
        goto cleanup;
    upload.width = (int)texture->baseWidth;
    upload.height = (int)texture->baseHeight;
    upload.level_count = resident_level_count;
    upload.total_level_count = texture->numLevels;
    upload.compressed = compressed;
    upload.is_srgb = is_srgb;
    upload.format = format;
    for (level = 0U; level < texture->numLevels; ++level)
    {
        ktx_size_t image_offset = 0U;
        ktx_size_t image_size = ktxTexture_GetImageSize(base_texture, level);
        size_t expected_level_size;
        int level_width = upload.width >> level;
        int level_height = upload.height >> level;
        if (level_width < 1) level_width = 1;
        if (level_height < 1) level_height = 1;
        if (image_size == 0U ||
            !henka_ktx_expected_level_size(format, level_width, level_height, &expected_level_size) ||
            image_size != (ktx_size_t)expected_level_size ||
            ktxTexture_GetImageOffset(base_texture, level, 0U, 0U, &image_offset) != KTX_SUCCESS ||
            image_offset > base_texture->dataSize ||
            image_size > base_texture->dataSize - image_offset ||
            image_size > (ktx_size_t)SIZE_MAX ||
            !henka_checked_size_add(total_size, (size_t)image_size, &total_size) ||
            total_size > HENKA_MAX_TEXTURE_DECODED_BYTES)
        {
            goto cleanup;
        }
        if (level < resident_level_count)
        {
            if (!henka_checked_size_add(selected_size, (size_t)image_size, &selected_size) ||
                selected_size > HENKA_MAX_TEXTURE_DECODED_BYTES)
            {
                goto cleanup;
            }
            upload.levels[level].offset = selected_size - (size_t)image_size;
            upload.levels[level].size = (size_t)image_size;
            upload.levels[level].width = level_width;
            upload.levels[level].height = level_height;
        }
    }
    upload.data = henka_malloc(selected_size);
    if (upload.data == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    upload.data_size = selected_size;
    for (level = 0U; level < upload.level_count; ++level)
    {
        ktx_size_t image_offset = 0U;
        ktx_uint8_t* source_data = ktxTexture_GetData(base_texture);
        if (source_data == NULL ||
            ktxTexture_GetImageOffset(base_texture, level, 0U, 0U, &image_offset) != KTX_SUCCESS)
            goto cleanup;
        memcpy(
            upload.data + upload.levels[level].offset,
            source_data + image_offset,
            upload.levels[level].size);
    }
    *out_upload = upload;
    memset(&upload, 0, sizeof(upload));
    result = HENKA_SUCCESS;

cleanup:
    henka_ktx2_upload_dispose(&upload);
    ktxTexture_Destroy(base_texture);
    return result;
}

henka_result henka_ktx2_prepare_upload(
    const unsigned char* data,
    size_t data_size,
    henka_texture_usage usage,
    henka_texture_color_space color_space,
    uint32_t capabilities,
    henka_ktx2_upload* out_upload)
{
    return henka_ktx2_prepare_upload_with_mip_limit(
        data,
        data_size,
        usage,
        color_space,
        capabilities,
        0U,
        out_upload);
}

henka_result henka_ktx2_decode_rgba8(
    const unsigned char* data,
    size_t data_size,
    unsigned char** out_pixels,
    size_t* out_pixel_size,
    int* out_width,
    int* out_height,
    bool* out_is_srgb)
{
    ktxTexture2* texture = NULL;
    ktxTexture* base_texture;
    ktx_size_t image_offset = 0U;
    ktx_size_t image_size;
    ktx_uint8_t* source_pixels;
    unsigned char* pixels = NULL;
    size_t expected_size;
    size_t total_level_bytes = 0U;
    uint32_t level;
    henka_result result = HENKA_ERROR_ASSET_SOURCE;

    if (out_pixels != NULL) *out_pixels = NULL;
    if (out_pixel_size != NULL) *out_pixel_size = 0U;
    if (out_width != NULL) *out_width = 0;
    if (out_height != NULL) *out_height = 0;
    if (out_is_srgb != NULL) *out_is_srgb = false;
    if (data == NULL || data_size == 0U || data_size > HENKA_MAX_TEXTURE_ENCODED_BYTES ||
        out_pixels == NULL || out_pixel_size == NULL || out_width == NULL || out_height == NULL ||
        out_is_srgb == NULL ||
        data_size > (size_t)UINT64_MAX ||
        ktxTexture2_CreateFromMemory(
            data,
            (ktx_size_t)data_size,
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT | KTX_TEXTURE_CREATE_SKIP_KVDATA_BIT,
            &texture) != KTX_SUCCESS || texture == NULL)
    {
        return HENKA_ERROR_ASSET_SOURCE;
    }

    base_texture = ktxTexture(texture);
    if (texture->baseWidth == 0U || texture->baseHeight == 0U ||
        texture->baseWidth > HENKA_MAX_TEXTURE_DIMENSION || texture->baseHeight > HENKA_MAX_TEXTURE_DIMENSION ||
        texture->baseDepth > 1U || texture->numDimensions != 2U || texture->numLayers != 1U ||
        texture->numFaces != 1U || texture->numLevels == 0U || texture->numLevels > 16U)
    {
        goto cleanup;
    }

    if (ktxTexture2_NeedsTranscoding(texture))
    {
        if (ktxTexture2_TranscodeBasis(texture, KTX_TTF_RGBA32, 0U) != KTX_SUCCESS)
            goto cleanup;
    }
    /* The engine uploads RGBA8. KTX-Software has already decompressed
     * supercompression and Basis data at this point; reject other GPU-native
     * formats rather than silently treating block data as pixels. */
    if (texture->vkFormat != 37U && texture->vkFormat != 43U)
        goto cleanup;
    for (level = 0U; level < texture->numLevels; ++level)
    {
        size_t level_size;
        int level_width = (int)texture->baseWidth >> level;
        int level_height = (int)texture->baseHeight >> level;
        if (level_width < 1) level_width = 1;
        if (level_height < 1) level_height = 1;
        if (!henka_ktx_checked_rgba8_size(level_width, level_height, &level_size) ||
            level_size > HENKA_MAX_TEXTURE_DECODED_BYTES ||
            ktxTexture_GetImageOffset(base_texture, level, 0U, 0U, &image_offset) != KTX_SUCCESS ||
            ktxTexture_GetImageSize(base_texture, level) != (ktx_size_t)level_size ||
            image_offset > base_texture->dataSize ||
            level_size > base_texture->dataSize - image_offset ||
            !henka_checked_size_add(total_level_bytes, level_size, &total_level_bytes) ||
            total_level_bytes > HENKA_MAX_TEXTURE_DECODED_BYTES)
        {
            goto cleanup;
        }
    }
    if (!henka_ktx_checked_rgba8_size((int)texture->baseWidth, (int)texture->baseHeight, &expected_size) ||
        expected_size > HENKA_MAX_TEXTURE_DECODED_BYTES ||
        ktxTexture_GetImageOffset(base_texture, 0U, 0U, 0U, &image_offset) != KTX_SUCCESS ||
        (image_size = ktxTexture_GetImageSize(base_texture, 0U)) != (ktx_size_t)expected_size ||
        image_offset > base_texture->dataSize || image_size > base_texture->dataSize - image_offset ||
        ktxTexture_GetData(base_texture) == NULL)
    {
        goto cleanup;
    }
    source_pixels = ktxTexture_GetData(base_texture) + image_offset;
    pixels = henka_malloc(expected_size);
    if (pixels == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    memcpy(pixels, source_pixels, expected_size);
    *out_pixels = pixels;
    *out_pixel_size = expected_size;
    *out_width = (int)texture->baseWidth;
    *out_height = (int)texture->baseHeight;
    *out_is_srgb = ktxTexture2_GetOETF_e(texture) == KHR_DF_TRANSFER_SRGB;
    pixels = NULL;
    result = HENKA_SUCCESS;

cleanup:
    henka_free(pixels);
    ktxTexture_Destroy(base_texture);
    return result;
}
