#include "ui_font_internal.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>

#include "../core/checked.h"

#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STBRP_STATIC
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>
#define STBTT_STATIC
#define STBTT_malloc(size, user_data) ((void)(user_data), henka_malloc(size))
#define STBTT_free(pointer, user_data) ((void)(user_data), henka_free(pointer))
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define HENKA_UI_FONT_MAX_SFNT_TABLES 64U
#define HENKA_UI_FONT_ATLAS_PADDING 1U
#define HENKA_UI_FONT_FALLBACK_CODEPOINT ((uint32_t)'?')

typedef struct henka_ui_sha256_state
{
    uint32_t words[8];
    uint64_t bit_count;
    uint8_t block[64U];
    size_t block_size;
} henka_ui_sha256_state;

struct henka_ui_font_resource
{
    uint8_t* source_bytes;
    stbtt_fontinfo stb_font;
    henka_ui_font_info info;
    henka_ui_font_glyph_info* glyphs;
    uint8_t* atlas;
};

static const uint32_t g_henka_ui_sha256_round_constants[64U] =
{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static uint32_t henka_ui_rotr32(uint32_t value, unsigned int amount)
{
    return (value >> amount) | (value << (32U - amount));
}

static uint32_t henka_ui_read_be32(const uint8_t* bytes)
{
    return ((uint32_t)bytes[0] << 24U) |
        ((uint32_t)bytes[1] << 16U) |
        ((uint32_t)bytes[2] << 8U) |
        (uint32_t)bytes[3];
}

static void henka_ui_sha256_transform(henka_ui_sha256_state* state, const uint8_t block[64U])
{
    uint32_t schedule[64U];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    size_t index;

    for (index = 0U; index < 16U; ++index)
    {
        schedule[index] = henka_ui_read_be32(&block[index * 4U]);
    }
    for (index = 16U; index < 64U; ++index)
    {
        const uint32_t x = schedule[index - 15U];
        const uint32_t y = schedule[index - 2U];
        const uint32_t s0 = henka_ui_rotr32(x, 7U) ^ henka_ui_rotr32(x, 18U) ^ (x >> 3U);
        const uint32_t s1 = henka_ui_rotr32(y, 17U) ^ henka_ui_rotr32(y, 19U) ^ (y >> 10U);
        schedule[index] = schedule[index - 16U] + s0 + schedule[index - 7U] + s1;
    }

    a = state->words[0];
    b = state->words[1];
    c = state->words[2];
    d = state->words[3];
    e = state->words[4];
    f = state->words[5];
    g = state->words[6];
    h = state->words[7];
    for (index = 0U; index < 64U; ++index)
    {
        const uint32_t sum1 = henka_ui_rotr32(e, 6U) ^ henka_ui_rotr32(e, 11U) ^ henka_ui_rotr32(e, 25U);
        const uint32_t choose = (e & f) ^ ((~e) & g);
        const uint32_t temp1 = h + sum1 + choose + g_henka_ui_sha256_round_constants[index] + schedule[index];
        const uint32_t sum0 = henka_ui_rotr32(a, 2U) ^ henka_ui_rotr32(a, 13U) ^ henka_ui_rotr32(a, 22U);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    state->words[0] += a;
    state->words[1] += b;
    state->words[2] += c;
    state->words[3] += d;
    state->words[4] += e;
    state->words[5] += f;
    state->words[6] += g;
    state->words[7] += h;
}

static void henka_ui_sha256_update(henka_ui_sha256_state* state, const uint8_t* bytes, size_t byte_count)
{
    size_t offset = 0U;
    while (offset < byte_count)
    {
        size_t remaining = sizeof(state->block) - state->block_size;
        size_t copy_size = byte_count - offset;
        if (copy_size > remaining)
        {
            copy_size = remaining;
        }
        memcpy(&state->block[state->block_size], &bytes[offset], copy_size);
        state->block_size += copy_size;
        offset += copy_size;
        if (state->block_size == sizeof(state->block))
        {
            henka_ui_sha256_transform(state, state->block);
            state->bit_count += 512U;
            state->block_size = 0U;
        }
    }
}

bool henka_ui_font_sha256_hex(const void* bytes, size_t byte_count, char out_hex[65U])
{
    static const char hex[] = "0123456789ABCDEF";
    henka_ui_sha256_state state =
    {
        {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
         0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U},
        0U,
        {0U},
        0U
    };
    uint64_t total_bits;
    size_t index;

    if (out_hex == NULL || (bytes == NULL && byte_count > 0U) ||
        byte_count > HENKA_UI_FONT_MAX_SOURCE_BYTES)
    {
        return false;
    }
    out_hex[0] = '\0';
    henka_ui_sha256_update(&state, (const uint8_t*)bytes, byte_count);
    total_bits = state.bit_count + (uint64_t)state.block_size * 8U;
    state.block[state.block_size++] = 0x80U;
    if (state.block_size > 56U)
    {
        memset(&state.block[state.block_size], 0, sizeof(state.block) - state.block_size);
        henka_ui_sha256_transform(&state, state.block);
        state.block_size = 0U;
    }
    memset(&state.block[state.block_size], 0, 56U - state.block_size);
    for (index = 0U; index < 8U; ++index)
    {
        state.block[63U - index] = (uint8_t)(total_bits >> (index * 8U));
    }
    henka_ui_sha256_transform(&state, state.block);
    for (index = 0U; index < 8U; ++index)
    {
        const uint32_t word = state.words[index];
        const size_t output_index = index * 8U;
        out_hex[output_index] = hex[(word >> 28U) & 0x0fU];
        out_hex[output_index + 1U] = hex[(word >> 24U) & 0x0fU];
        out_hex[output_index + 2U] = hex[(word >> 20U) & 0x0fU];
        out_hex[output_index + 3U] = hex[(word >> 16U) & 0x0fU];
        out_hex[output_index + 4U] = hex[(word >> 12U) & 0x0fU];
        out_hex[output_index + 5U] = hex[(word >> 8U) & 0x0fU];
        out_hex[output_index + 6U] = hex[(word >> 4U) & 0x0fU];
        out_hex[output_index + 7U] = hex[word & 0x0fU];
    }
    out_hex[64U] = '\0';
    return true;
}

void henka_ui_font_set_diagnostic(
    henka_ui_font_diagnostic* diagnostic,
    henka_ui_font_stage stage,
    const char* identity,
    const char* message)
{
    if (diagnostic == NULL)
    {
        return;
    }
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->stage = stage;
    if (identity != NULL)
    {
        size_t length = 0U;
        while (length < HENKA_UI_FONT_MAX_IDENTITY_BYTES && identity[length] != '\0')
        {
            ++length;
        }
        memcpy(diagnostic->identity, identity, length);
        diagnostic->identity[length] = '\0';
    }
    if (message != NULL)
    {
        (void)snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
    }
}

bool henka_ui_font_identity_valid(const char* identity)
{
    size_t index = 0U;
    if (identity == NULL || identity[0] == '\0')
    {
        return false;
    }
    while (index <= HENKA_UI_FONT_MAX_IDENTITY_BYTES && identity[index] != '\0')
    {
        const unsigned char character = (unsigned char)identity[index];
        if (character < 0x21U || character > 0x7eU)
        {
            return false;
        }
        ++index;
    }
    return index > 0U && index <= HENKA_UI_FONT_MAX_IDENTITY_BYTES && identity[index] == '\0';
}

static int henka_ui_font_hex_value(char value)
{
    if (value >= '0' && value <= '9')
    {
        return value - '0';
    }
    value = (char)toupper((unsigned char)value);
    if (value >= 'A' && value <= 'F')
    {
        return 10 + value - 'A';
    }
    return -1;
}

bool henka_ui_font_hash_hex_valid(const char* hash_hex)
{
    size_t index;
    if (hash_hex == NULL)
    {
        return false;
    }
    for (index = 0U; index <= 64U; ++index)
    {
        if (index == 64U)
        {
            return hash_hex[index] == '\0';
        }
        if (hash_hex[index] == '\0')
        {
            return false;
        }
        if (henka_ui_font_hex_value(hash_hex[index]) < 0)
        {
            return false;
        }
    }
    return false;
}

bool henka_ui_font_hash_matches(const char* expected, const char* actual)
{
    size_t index;
    if (!henka_ui_font_hash_hex_valid(expected) || !henka_ui_font_hash_hex_valid(actual))
    {
        return false;
    }
    for (index = 0U; index < 64U; ++index)
    {
        const int value = henka_ui_font_hex_value(expected[index]);
        if (value < 0 ||
            (char)toupper((unsigned char)expected[index]) !=
                (char)toupper((unsigned char)actual[index]))
        {
            return false;
        }
    }
    return true;
}

static uint32_t henka_ui_font_read_be16(const uint8_t* bytes)
{
    return ((uint32_t)bytes[0] << 8U) | (uint32_t)bytes[1];
}

static uint32_t henka_ui_font_read_be32_at(const uint8_t* bytes)
{
    return ((uint32_t)bytes[0] << 24U) |
        ((uint32_t)bytes[1] << 16U) |
        ((uint32_t)bytes[2] << 8U) |
        (uint32_t)bytes[3];
}

static bool henka_ui_font_sfnt_table_present(
    const uint8_t* bytes,
    size_t source_size,
    const char tag[4])
{
    const uint32_t table_count = henka_ui_font_read_be16(&bytes[4U]);
    size_t index;
    for (index = 0U; index < table_count; ++index)
    {
        const size_t record_offset = 12U + index * 16U;
        if (memcmp(&bytes[record_offset], tag, 4U) == 0)
        {
            const uint32_t table_offset = henka_ui_font_read_be32_at(&bytes[record_offset + 8U]);
            const uint32_t table_size = henka_ui_font_read_be32_at(&bytes[record_offset + 12U]);
            return (size_t)table_offset <= source_size &&
                (size_t)table_size <= source_size - (size_t)table_offset;
        }
    }
    return false;
}

static bool henka_ui_font_sfnt_directory_valid(const uint8_t* bytes, size_t source_size)
{
    uint32_t table_count;
    size_t directory_size;
    size_t index;

    if (bytes == NULL || source_size < 12U ||
        (henka_ui_font_read_be32_at(bytes) != 0x00010000U && memcmp(bytes, "true", 4U) != 0))
    {
        return false;
    }
    table_count = henka_ui_font_read_be16(&bytes[4U]);
    if (table_count == 0U || table_count > HENKA_UI_FONT_MAX_SFNT_TABLES ||
        !henka_checked_size_multiply((size_t)table_count, 16U, &directory_size) ||
        directory_size > source_size - 12U)
    {
        return false;
    }
    for (index = 0U; index < table_count; ++index)
    {
        const size_t record_offset = 12U + index * 16U;
        const uint32_t table_offset = henka_ui_font_read_be32_at(&bytes[record_offset + 8U]);
        const uint32_t table_size = henka_ui_font_read_be32_at(&bytes[record_offset + 12U]);
        if ((size_t)table_offset > source_size ||
            (size_t)table_size > source_size - (size_t)table_offset)
        {
            return false;
        }
    }
    return !henka_ui_font_sfnt_table_present(bytes, source_size, "fvar") &&
        henka_ui_font_sfnt_table_present(bytes, source_size, "cmap") &&
        henka_ui_font_sfnt_table_present(bytes, source_size, "glyf") &&
        henka_ui_font_sfnt_table_present(bytes, source_size, "head") &&
        henka_ui_font_sfnt_table_present(bytes, source_size, "hhea") &&
        henka_ui_font_sfnt_table_present(bytes, source_size, "hmtx") &&
        henka_ui_font_sfnt_table_present(bytes, source_size, "loca") &&
        henka_ui_font_sfnt_table_present(bytes, source_size, "maxp");
}

static bool henka_ui_font_codepoint_valid(uint32_t codepoint)
{
    return codepoint <= 0x10ffffU && !(codepoint >= 0xd800U && codepoint <= 0xdfffU);
}

static int32_t henka_ui_font_find_glyph_position(
    const henka_ui_font_resource* resource,
    uint32_t codepoint)
{
    size_t index;
    for (index = 0U; index < resource->info.glyph_count; ++index)
    {
        if (resource->glyphs[index].codepoint == codepoint)
        {
            return (int32_t)index;
        }
    }
    return -1;
}

void henka_ui_font_resource_destroy(henka_ui_font_resource* resource)
{
    if (resource == NULL)
    {
        return;
    }
    henka_free(resource->atlas);
    henka_free(resource->glyphs);
    henka_free(resource->source_bytes);
    henka_free(resource);
}

henka_result henka_ui_font_resource_create(
    const henka_ui_font_desc* desc,
    henka_ui_font_resource** out_resource,
    henka_ui_font_diagnostic* out_diagnostic)
{
    henka_ui_font_resource* candidate = NULL;
    stbrp_rect* rectangles = NULL;
    stbrp_node* pack_nodes = NULL;
    size_t rectangle_bytes;
    size_t glyph_bytes;
    size_t node_bytes;
    size_t atlas_bytes;
    size_t index;
    bool has_fallback = false;
    float raster_scale;
    int ascent_units;
    int descent_units;
    int line_gap_units;
    henka_result result = HENKA_ERROR_INVALID_ARGUMENT;

    henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_NONE, NULL, NULL);
    if (desc == NULL || out_resource == NULL || *out_resource != NULL ||
        desc->source_bytes == NULL || desc->source_size == 0U ||
        desc->required_codepoints == NULL || desc->required_codepoint_count == 0U ||
        desc->raster_pixel_height == 0U || desc->raster_pixel_height > 256U ||
        desc->atlas_width == 0U || desc->atlas_height == 0U)
    {
        henka_ui_font_set_diagnostic(
            out_diagnostic, HENKA_UI_FONT_STAGE_ARGUMENT, desc != NULL ? desc->identity : NULL,
            "font resource arguments are invalid");
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (desc->source_size > HENKA_UI_FONT_MAX_SOURCE_BYTES)
    {
        henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_SOURCE_LIMIT,
            desc->identity, "font source exceeds the 2 MiB limit");
        return HENKA_ERROR_LIMIT;
    }
    if (!henka_ui_font_identity_valid(desc->identity))
    {
        henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_IDENTITY,
            NULL, "font identity must be bounded printable ASCII");
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (desc->required_codepoint_count > HENKA_UI_FONT_MAX_GLYPHS)
    {
        henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_GLYPH_SET,
            desc->identity, "required glyph set exceeds the 512-glyph limit");
        return HENKA_ERROR_LIMIT;
    }
    if (desc->atlas_width > HENKA_UI_FONT_MAX_ATLAS_DIMENSION ||
        desc->atlas_height > HENKA_UI_FONT_MAX_ATLAS_DIMENSION ||
        !henka_checked_size_multiply((size_t)desc->atlas_width, (size_t)desc->atlas_height, &atlas_bytes))
    {
        henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_ATLAS_DIMENSIONS,
            desc->identity, "font atlas dimensions exceed the 2048-by-2048 limit");
        return HENKA_ERROR_LIMIT;
    }
    if (!henka_ui_font_hash_hex_valid(desc->verified_sha256_hex))
    {
        henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_HASH,
            desc->identity, "font bytes must be hash-verified by the application bootstrap before parsing");
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (!henka_ui_font_sfnt_directory_valid(desc->source_bytes, desc->source_size))
    {
        henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_PARSE,
            desc->identity, "font is not a bounded static TrueType SFNT");
        return HENKA_ERROR_ASSET_SOURCE;
    }

    candidate = (henka_ui_font_resource*)henka_calloc(1U, sizeof(*candidate));
    if (candidate == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto allocation_failure;
    }
    candidate->source_bytes = (uint8_t*)henka_malloc(desc->source_size);
    if (candidate->source_bytes == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto allocation_failure;
    }
    memcpy(candidate->source_bytes, desc->source_bytes, desc->source_size);
    if (!stbtt_InitFont(&candidate->stb_font, candidate->source_bytes, 0))
    {
        henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_PARSE,
            desc->identity, "pinned stb_truetype rejected the verified font bytes");
        result = HENKA_ERROR_ASSET_SOURCE;
        goto failure;
    }
    if (!henka_checked_size_multiply(desc->required_codepoint_count,
            sizeof(*candidate->glyphs), &glyph_bytes) ||
        !henka_checked_size_multiply(desc->required_codepoint_count,
            sizeof(*rectangles), &rectangle_bytes) ||
        !henka_checked_size_multiply(desc->atlas_width, sizeof(*pack_nodes), &node_bytes))
    {
        henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_ALLOCATION,
            desc->identity, "font allocation size overflow");
        result = HENKA_ERROR_LIMIT;
        goto failure;
    }
    candidate->glyphs = (henka_ui_font_glyph_info*)henka_calloc(1U, glyph_bytes);
    rectangles = (stbrp_rect*)henka_calloc(1U, rectangle_bytes);
    pack_nodes = (stbrp_node*)henka_malloc(node_bytes);
    candidate->atlas = (uint8_t*)henka_calloc(1U, atlas_bytes);
    if (candidate->glyphs == NULL || rectangles == NULL || pack_nodes == NULL || candidate->atlas == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto allocation_failure;
    }

    raster_scale = stbtt_ScaleForPixelHeight(&candidate->stb_font, (float)desc->raster_pixel_height);
    if (!isfinite(raster_scale) || raster_scale <= 0.0f)
    {
        henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_PARSE,
            desc->identity, "font raster scale is invalid");
        result = HENKA_ERROR_ASSET_SOURCE;
        goto failure;
    }
    stbtt_GetFontVMetrics(&candidate->stb_font, &ascent_units, &descent_units, &line_gap_units);
    candidate->info.ascent = (float)ascent_units * raster_scale;
    candidate->info.descent = (float)descent_units * raster_scale;
    candidate->info.line_gap = (float)line_gap_units * raster_scale;
    candidate->info.line_height = candidate->info.ascent - candidate->info.descent + candidate->info.line_gap;
    if (!isfinite(candidate->info.ascent) || !isfinite(candidate->info.descent) ||
        !isfinite(candidate->info.line_gap) || !isfinite(candidate->info.line_height) ||
        candidate->info.ascent <= 0.0f || candidate->info.descent >= 0.0f ||
        candidate->info.line_height <= 0.0f)
    {
        henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_PARSE,
            desc->identity, "font vertical metrics are invalid");
        result = HENKA_ERROR_ASSET_SOURCE;
        goto failure;
    }
    candidate->info.glyph_count = desc->required_codepoint_count;
    candidate->info.raster_pixel_height = desc->raster_pixel_height;
    candidate->info.atlas_width = desc->atlas_width;
    candidate->info.atlas_height = desc->atlas_height;
    (void)snprintf(candidate->info.identity, sizeof(candidate->info.identity), "%s", desc->identity);
    (void)snprintf(candidate->info.sha256_hex, sizeof(candidate->info.sha256_hex), "%s",
        desc->verified_sha256_hex);

    for (index = 0U; index < desc->required_codepoint_count; ++index)
    {
        const uint32_t codepoint = desc->required_codepoints[index];
        int advance_units;
        int bearing_units;
        int bitmap_x0;
        int bitmap_y0;
        int bitmap_x1;
        int bitmap_y1;
        int bitmap_width;
        int bitmap_height;
        size_t previous;

        if (!henka_ui_font_codepoint_valid(codepoint))
        {
            henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_GLYPH_SET,
                desc->identity, "required glyph set contains an invalid Unicode scalar");
            result = HENKA_ERROR_ASSET_SOURCE;
            goto failure;
        }
        for (previous = 0U; previous < index; ++previous)
        {
            if (desc->required_codepoints[previous] == codepoint)
            {
                henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_GLYPH_SET,
                    desc->identity, "required glyph set contains duplicate codepoints");
                result = HENKA_ERROR_ASSET_SOURCE;
                goto failure;
            }
        }
        if (codepoint == HENKA_UI_FONT_FALLBACK_CODEPOINT)
        {
            has_fallback = true;
        }
        candidate->glyphs[index].codepoint = codepoint;
        candidate->glyphs[index].glyph_index =
            (uint32_t)stbtt_FindGlyphIndex(&candidate->stb_font, (int)codepoint);
        if (candidate->glyphs[index].glyph_index == 0U)
        {
            henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_GLYPH_SET,
                desc->identity, "font is missing a required UI glyph");
            result = HENKA_ERROR_ASSET_SOURCE;
            goto failure;
        }
        stbtt_GetCodepointHMetrics(&candidate->stb_font, (int)codepoint, &advance_units, &bearing_units);
        stbtt_GetCodepointBitmapBox(&candidate->stb_font, (int)codepoint,
            raster_scale, raster_scale, &bitmap_x0, &bitmap_y0, &bitmap_x1, &bitmap_y1);
        bitmap_width = bitmap_x1 - bitmap_x0;
        bitmap_height = bitmap_y1 - bitmap_y0;
        if (bitmap_width < 0 || bitmap_height < 0 || bitmap_width > (int)desc->atlas_width - 2 ||
            bitmap_height > (int)desc->atlas_height - 2)
        {
            henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_ATLAS_PACK,
                desc->identity, "required glyph exceeds the configured atlas dimensions");
            result = HENKA_ERROR_LIMIT;
            goto failure;
        }
        candidate->glyphs[index].advance_x = (float)advance_units * raster_scale;
        candidate->glyphs[index].bearing_x = (float)bearing_units * raster_scale;
        candidate->glyphs[index].bitmap_x = (float)bitmap_x0;
        candidate->glyphs[index].bitmap_y = (float)bitmap_y0;
        candidate->glyphs[index].bitmap_width = (uint32_t)bitmap_width;
        candidate->glyphs[index].bitmap_height = (uint32_t)bitmap_height;
        rectangles[index].id = (int)index;
        rectangles[index].w = (stbrp_coord)((bitmap_width > 0 ? bitmap_width : 1) + 2);
        rectangles[index].h = (stbrp_coord)((bitmap_height > 0 ? bitmap_height : 1) + 2);
    }
    if (!has_fallback)
    {
        henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_GLYPH_SET,
            desc->identity, "required glyph set must include the visible '?' fallback");
        result = HENKA_ERROR_ASSET_SOURCE;
        goto failure;
    }

    {
        stbrp_context pack_context;
        stbrp_init_target(&pack_context, (int)desc->atlas_width, (int)desc->atlas_height,
            pack_nodes, (int)desc->atlas_width);
        if (!stbrp_pack_rects(&pack_context, rectangles, (int)desc->required_codepoint_count))
        {
            henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_ATLAS_PACK,
                desc->identity, "required glyph set does not fit the bounded atlas");
            result = HENKA_ERROR_LIMIT;
            goto failure;
        }
    }
    for (index = 0U; index < desc->required_codepoint_count; ++index)
    {
        henka_ui_font_glyph_info* glyph = &candidate->glyphs[index];
        glyph->atlas_x = (uint32_t)rectangles[index].x + HENKA_UI_FONT_ATLAS_PADDING;
        glyph->atlas_y = (uint32_t)rectangles[index].y + HENKA_UI_FONT_ATLAS_PADDING;
        if (glyph->bitmap_width > 0U && glyph->bitmap_height > 0U)
        {
            const size_t atlas_offset = (size_t)glyph->atlas_y * (size_t)desc->atlas_width + (size_t)glyph->atlas_x;
            stbtt_MakeCodepointBitmap(&candidate->stb_font,
                &candidate->atlas[atlas_offset],
                (int)glyph->bitmap_width,
                (int)glyph->bitmap_height,
                (int)desc->atlas_width,
                raster_scale,
                raster_scale,
                (int)glyph->codepoint);
        }
    }

    henka_free(pack_nodes);
    henka_free(rectangles);
    henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_NONE, desc->identity, NULL);
    *out_resource = candidate;
    return HENKA_SUCCESS;

allocation_failure:
    henka_ui_font_set_diagnostic(out_diagnostic, HENKA_UI_FONT_STAGE_ALLOCATION,
        desc->identity, "font resource allocation failed");
    henka_ui_font_resource_destroy(candidate);
    henka_free(pack_nodes);
    henka_free(rectangles);
    return result;

failure:
    henka_ui_font_resource_destroy(candidate);
    henka_free(pack_nodes);
    henka_free(rectangles);
    return result;
}

bool henka_ui_font_resource_get_info(
    const henka_ui_font_resource* resource,
    henka_ui_font_info* out_info)
{
    if (resource == NULL || out_info == NULL)
    {
        return false;
    }
    *out_info = resource->info;
    return true;
}

bool henka_ui_font_resource_get_glyph(
    const henka_ui_font_resource* resource,
    uint32_t codepoint,
    henka_ui_font_glyph_info* out_glyph)
{
    const int32_t index = resource != NULL ? henka_ui_font_find_glyph_position(resource, codepoint) : -1;
    if (index < 0 || out_glyph == NULL)
    {
        return false;
    }
    *out_glyph = resource->glyphs[(size_t)index];
    return true;
}

bool henka_ui_font_resource_get_kerning(
    const henka_ui_font_resource* resource,
    uint32_t left_glyph_index,
    uint32_t right_glyph_index,
    float* out_kerning)
{
    float scale;
    int units;
    if (resource == NULL || out_kerning == NULL || left_glyph_index >= (uint32_t)resource->stb_font.numGlyphs ||
        right_glyph_index >= (uint32_t)resource->stb_font.numGlyphs)
    {
        return false;
    }
    scale = stbtt_ScaleForPixelHeight(&resource->stb_font, (float)resource->info.raster_pixel_height);
    units = stbtt_GetGlyphKernAdvance(
        &resource->stb_font, (int)left_glyph_index, (int)right_glyph_index);
    *out_kerning = (float)units * scale;
    return isfinite(*out_kerning);
}

const uint8_t* henka_ui_font_resource_get_atlas(
    const henka_ui_font_resource* resource,
    uint32_t* out_width,
    uint32_t* out_height)
{
    if (out_width != NULL)
    {
        *out_width = resource != NULL ? resource->info.atlas_width : 0U;
    }
    if (out_height != NULL)
    {
        *out_height = resource != NULL ? resource->info.atlas_height : 0U;
    }
    return resource != NULL ? resource->atlas : NULL;
}
