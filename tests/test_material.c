#include "test_suite.h"

#include <string.h>

#include <henka/scene.h>

void henka_test_material(void)
{
    char description[128];
    char tiny_description[4];
    henka_material material;

    material = henka_material_default();
    HENKA_TEST_ASSERT(strcmp(henka_material_type_get_label(material.type), "Lit") == 0);
    HENKA_TEST_ASSERT(material.shader == NULL);
    HENKA_TEST_ASSERT(material.base_color_texture == NULL);
    HENKA_TEST_ASSERT(!material.use_texture);
    HENKA_TEST_ASSERT(material.use_lighting);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.x, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.y, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.z, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.w, 1.0f, 0.0001);
    HENKA_TEST_ASSERT(henka_material_describe(&material, description, sizeof(description)) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strstr(description, "Material") != NULL);
    HENKA_TEST_ASSERT(henka_material_describe(&material, tiny_description, sizeof(tiny_description)) == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(tiny_description[sizeof(tiny_description) - 1U] == '\0');
}
