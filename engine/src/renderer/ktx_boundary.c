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
