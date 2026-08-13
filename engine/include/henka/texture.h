#ifndef HENKA_TEXTURE_H
#define HENKA_TEXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>

typedef struct henka_engine henka_engine;
typedef struct henka_texture henka_texture;

typedef enum henka_texture_color_space
{
    HENKA_TEXTURE_COLOR_SPACE_SRGB = 0,
    HENKA_TEXTURE_COLOR_SPACE_LINEAR
} henka_texture_color_space;

typedef enum henka_texture_filter
{
    HENKA_TEXTURE_FILTER_NEAREST = 0,
    HENKA_TEXTURE_FILTER_LINEAR,
    HENKA_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR
} henka_texture_filter;

typedef enum henka_texture_wrap
{
    HENKA_TEXTURE_WRAP_CLAMP_TO_EDGE = 0,
    HENKA_TEXTURE_WRAP_REPEAT,
    HENKA_TEXTURE_WRAP_MIRRORED_REPEAT
} henka_texture_wrap;

typedef enum henka_texture_usage
{
    HENKA_TEXTURE_USAGE_COLOR = 0,
    HENKA_TEXTURE_USAGE_NORMAL,
    HENKA_TEXTURE_USAGE_METALLIC_ROUGHNESS,
    HENKA_TEXTURE_USAGE_OCCLUSION,
    HENKA_TEXTURE_USAGE_EMISSIVE,
    HENKA_TEXTURE_USAGE_GENERIC_DATA,
    HENKA_TEXTURE_USAGE_UI
} henka_texture_usage;

typedef enum henka_texture_alpha_mode
{
    HENKA_TEXTURE_ALPHA_OPAQUE = 0,
    HENKA_TEXTURE_ALPHA_MASKED,
    HENKA_TEXTURE_ALPHA_BLEND
} henka_texture_alpha_mode;

typedef enum henka_texture_source_class
{
    HENKA_TEXTURE_SOURCE_CLASS_UNKNOWN = 0,
    HENKA_TEXTURE_SOURCE_CLASS_LDR_8_BIT,
    HENKA_TEXTURE_SOURCE_CLASS_HDR,
    HENKA_TEXTURE_SOURCE_CLASS_16_BIT
} henka_texture_source_class;

/* The backend format currently resident on the GPU. RGBA8 is also the
 * truthful fallback for compressed sources when no compatible block format
 * is available. */
typedef enum henka_texture_gpu_format
{
    HENKA_TEXTURE_GPU_FORMAT_UNKNOWN = 0,
    HENKA_TEXTURE_GPU_FORMAT_RGBA8,
    HENKA_TEXTURE_GPU_FORMAT_BC1,
    HENKA_TEXTURE_GPU_FORMAT_BC3,
    HENKA_TEXTURE_GPU_FORMAT_BC5,
    HENKA_TEXTURE_GPU_FORMAT_BC7,
    HENKA_TEXTURE_GPU_FORMAT_ETC2_RGB,
    HENKA_TEXTURE_GPU_FORMAT_ETC2_RGBA,
    HENKA_TEXTURE_GPU_FORMAT_ETC2_RG,
    HENKA_TEXTURE_GPU_FORMAT_ASTC_4X4,
    HENKA_TEXTURE_GPU_FORMAT_BC1_RGBA
} henka_texture_gpu_format;

typedef enum henka_texture_failure_category
{
    HENKA_TEXTURE_FAILURE_NONE = 0,
    HENKA_TEXTURE_FAILURE_INVALID_ARGUMENT,
    HENKA_TEXTURE_FAILURE_REJECTED_PATH,
    HENKA_TEXTURE_FAILURE_MISSING_OR_UNREADABLE,
    HENKA_TEXTURE_FAILURE_ENCODED_SIZE_LIMIT,
    HENKA_TEXTURE_FAILURE_UNSUPPORTED_SOURCE,
    HENKA_TEXTURE_FAILURE_INVALID_DIMENSIONS,
    HENKA_TEXTURE_FAILURE_DECODED_SIZE_LIMIT,
    HENKA_TEXTURE_FAILURE_DECODE_CORRUPTION,
    HENKA_TEXTURE_FAILURE_ALLOCATION,
    HENKA_TEXTURE_FAILURE_RENDERER
} henka_texture_failure_category;

typedef struct henka_texture_descriptor
{
    henka_texture_color_space color_space;
    henka_texture_filter min_filter;
    henka_texture_filter mag_filter;
    henka_texture_wrap wrap_u;
    henka_texture_wrap wrap_v;
    bool generate_mipmaps;
    bool vertical_flip;
    henka_texture_usage usage;
    float anisotropy;
} henka_texture_descriptor;

typedef struct henka_texture_info
{
    int width;
    int height;
    int original_channel_count;
    size_t source_byte_size;
    henka_texture_color_space color_space;
    henka_texture_descriptor descriptor;
    henka_texture_usage usage;
    henka_texture_alpha_mode alpha_mode;
    henka_texture_source_class source_class;
    henka_texture_failure_category last_failure;
    bool fallback_alias;
    bool backend_ready;
    bool gpu_compressed;
    henka_texture_gpu_format gpu_format;
    uint64_t resident_gpu_bytes;
    uint32_t resident_mip_count;
    uint32_t mip_count;
    uint64_t content_revision;
} henka_texture_info;

henka_texture_descriptor henka_texture_descriptor_default_color(void);
henka_texture_descriptor henka_texture_descriptor_default_data(void);
henka_texture_descriptor henka_texture_descriptor_default_normal(void);
henka_result henka_texture_descriptor_validate(const henka_texture_descriptor* descriptor);

/*
 * Creates a texture from an image file using the compatibility color-texture
 * descriptor. The source is read once into a bounded buffer and that exact
 * buffer is used for inspection and decode. 8-bit LDR sources are decoded to
 * straight-alpha RGBA8; Radiance HDR sources are decoded to linear RGBA32F
 * and uploaded as RGBA16F. Other 16-bit sources are rejected rather than
 * silently quantized.
 */
henka_result henka_texture_create_from_file(henka_engine* engine, const char* path, henka_texture** out_texture);
henka_result henka_texture_create_from_file_with_descriptor(
    henka_engine* engine,
    const char* path,
    const henka_texture_descriptor* descriptor,
    henka_texture** out_texture);

/*
 * Creates a texture from a bounded KTX2 byte buffer. The buffer remains
 * caller-owned and is only read during the call. The renderer selects a
 * supported GPU-compressed format for Basis payloads when available,
 * preserves checked KTX2 mip levels, and falls back to bounded RGBA8 upload
 * only when the source can be safely transcoded. Unsupported native payloads
 * return HENKA_ERROR_ASSET_SOURCE for manager fallback handling.
 */
henka_result henka_texture_create_from_ktx2_memory(
    henka_engine* engine,
    const unsigned char* data,
    size_t data_size,
    const henka_texture_descriptor* descriptor,
    henka_texture** out_texture);

/*
 * Creates a texture from RGBA8 pixel data after validating dimensions and
 * byte-count limits. The caller must provide enough readable pixel data and
 * owns the texture until henka_texture_destroy is called.
 */
henka_result henka_texture_create_from_rgba8(
    henka_engine* engine,
    int width,
    int height,
    const unsigned char* pixels,
    henka_texture** out_texture);
henka_result henka_texture_create_from_rgba8_with_descriptor(
    henka_engine* engine,
    int width,
    int height,
    const unsigned char* pixels,
    const henka_texture_descriptor* descriptor,
    henka_texture** out_texture);
henka_result henka_texture_create_from_rgba32f_with_descriptor(
    henka_engine* engine,
    int width,
    int height,
    const float* pixels,
    const henka_texture_descriptor* descriptor,
    henka_texture** out_texture);

henka_result henka_texture_get_info(
    const henka_texture* texture,
    henka_texture_info* out_info);

/* Releases caller-owned textures. Manager-owned borrowed textures are ignored. */
void henka_texture_destroy(henka_texture* texture);

#endif
