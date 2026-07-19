#ifndef HENKA_TEXTURE_H
#define HENKA_TEXTURE_H

#include <henka/result.h>

typedef struct henka_engine henka_engine;
typedef struct henka_texture henka_texture;

/*
 * Creates a texture from an image file after validating dimensions and the
 * decoded RGBA8 size. The caller owns the texture and must release it with
 * henka_texture_destroy.
 */
henka_result henka_texture_create_from_file(henka_engine* engine, const char* path, henka_texture** out_texture);

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

void henka_texture_destroy(henka_texture* texture);

#endif
