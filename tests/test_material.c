#include "test_suite.h"

#include <string.h>
#include <stdint.h>

#include <henka/scene.h>
#include <henka/texture.h>

void henka_test_material(void)
{
    char description[128];
    char tiny_description[4];
    henka_material material;

    material = henka_material_default();
    HENKA_TEST_ASSERT(strcmp(henka_material_type_get_label(material.type), "Lit") == 0);
    HENKA_TEST_ASSERT(material.shader == NULL);
    HENKA_TEST_ASSERT(material.base_color_texture == NULL);
    HENKA_TEST_ASSERT(material.base_color_uv_set == 0);
    HENKA_TEST_ASSERT(material.normal_uv_set == 0);
    HENKA_TEST_ASSERT(material.metallic_roughness_uv_set == 0);
    HENKA_TEST_ASSERT(material.occlusion_uv_set == 0);
    HENKA_TEST_ASSERT(material.emissive_uv_set == 0);
    HENKA_TEST_ASSERT(!material.use_texture);
    HENKA_TEST_ASSERT(material.use_lighting);
    HENKA_TEST_ASSERT(material.metallic == 0.0f);
    HENKA_TEST_ASSERT(material.roughness == 0.5f);
    HENKA_TEST_ASSERT(material.specular_factor == 1.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.specular_color.x, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT(material.ior == 1.5f);
    HENKA_TEST_ASSERT(material.transmission == 0.0f);
    HENKA_TEST_ASSERT(material.thickness == 0.0f);
    HENKA_TEST_ASSERT(material.attenuation_distance == 10000.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.attenuation_color.z, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT(material.normal_scale == 1.0f);
    HENKA_TEST_ASSERT(material.occlusion_strength == 1.0f);
    HENKA_TEST_ASSERT(material.clearcoat == 0.0f);
    HENKA_TEST_ASSERT(material.clearcoat_roughness == 0.2f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.sheen_color.x, 0.0f, 0.0001);
    HENKA_TEST_ASSERT(material.sheen_roughness == 0.5f);
    HENKA_TEST_ASSERT(material.alpha_mode == HENKA_MATERIAL_ALPHA_OPAQUE);
    HENKA_TEST_ASSERT(material.cast_shadows);
    HENKA_TEST_ASSERT(material.receive_shadows);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.x, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.y, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.z, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.w, 1.0f, 0.0001);
    HENKA_TEST_ASSERT(henka_material_describe(&material, description, sizeof(description)) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strstr(description, "Material") != NULL);
    HENKA_TEST_ASSERT(henka_material_describe(&material, tiny_description, sizeof(tiny_description)) == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(tiny_description[sizeof(tiny_description) - 1U] == '\0');

    {
        henka_material valid = henka_material_default();
        valid.shader = (henka_shader*)(uintptr_t)1U;
        HENKA_TEST_ASSERT(henka_material_validate(&valid) == HENKA_SUCCESS);
        valid.type = HENKA_MATERIAL_TYPE_PROCEDURAL_RESERVED;
        HENKA_TEST_ASSERT(henka_material_validate(&valid) == HENKA_ERROR_INVALID_ARGUMENT);
        valid.type = HENKA_MATERIAL_TYPE_LIT;
        valid.emissive_color.x = 1.01f;
        HENKA_TEST_ASSERT(henka_material_validate(&valid) == HENKA_ERROR_INVALID_ARGUMENT);
        valid.emissive_color.x = 0.0f;
        valid.roughness = 0.01f;
        HENKA_TEST_ASSERT(henka_material_validate(&valid) == HENKA_ERROR_INVALID_ARGUMENT);
        valid.roughness = 0.5f;
        valid.clearcoat = 1.01f;
        HENKA_TEST_ASSERT(henka_material_validate(&valid) == HENKA_ERROR_INVALID_ARGUMENT);
        valid.clearcoat = 0.0f;
        valid.specular_factor = 1.01f;
        HENKA_TEST_ASSERT(henka_material_validate(&valid) == HENKA_ERROR_INVALID_ARGUMENT);
        valid.specular_factor = 1.0f;
        valid.ior = 0.9f;
        HENKA_TEST_ASSERT(henka_material_validate(&valid) == HENKA_ERROR_INVALID_ARGUMENT);
        valid.ior = 1.5f;
        valid.transmission = 1.01f;
        HENKA_TEST_ASSERT(henka_material_validate(&valid) == HENKA_ERROR_INVALID_ARGUMENT);
        valid.transmission = 0.0f;
        valid.thickness = 1.01f;
        HENKA_TEST_ASSERT(henka_material_validate(&valid) == HENKA_ERROR_INVALID_ARGUMENT);
        valid.thickness = 0.0f;
        valid.sheen_color.x = 1.01f;
        HENKA_TEST_ASSERT(henka_material_validate(&valid) == HENKA_ERROR_INVALID_ARGUMENT);
        valid.sheen_color.x = 0.0f;
        valid.base_color_uv_set = 2;
        HENKA_TEST_ASSERT(henka_material_validate(&valid) == HENKA_ERROR_INVALID_ARGUMENT);
    }

    {
        henka_texture_descriptor color = henka_texture_descriptor_default_color();
        henka_texture_descriptor normal = henka_texture_descriptor_default_normal();
        henka_texture_descriptor invalid = color;

        HENKA_TEST_ASSERT(color.color_space == HENKA_TEXTURE_COLOR_SPACE_SRGB);
        HENKA_TEST_ASSERT(color.generate_mipmaps);
        HENKA_TEST_ASSERT(color.min_filter == HENKA_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR);
        HENKA_TEST_ASSERT(henka_texture_descriptor_validate(&color) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(normal.color_space == HENKA_TEXTURE_COLOR_SPACE_LINEAR);
        HENKA_TEST_ASSERT(normal.usage == HENKA_TEXTURE_USAGE_NORMAL);
        HENKA_TEST_ASSERT(henka_texture_descriptor_validate(&normal) == HENKA_SUCCESS);
        invalid.generate_mipmaps = false;
        HENKA_TEST_ASSERT(henka_texture_descriptor_validate(&invalid) == HENKA_ERROR_INVALID_ARGUMENT);
        invalid = normal;
        invalid.color_space = HENKA_TEXTURE_COLOR_SPACE_SRGB;
        HENKA_TEST_ASSERT(henka_texture_descriptor_validate(&invalid) == HENKA_ERROR_INVALID_ARGUMENT);
    }
}
