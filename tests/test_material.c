#include "test_suite.h"

#include <henka/scene.h>

void henka_test_material(void)
{
    henka_material material;

    material = henka_material_default();
    HENKA_TEST_ASSERT(material.shader == NULL);
    HENKA_TEST_ASSERT(material.base_color_texture == NULL);
    HENKA_TEST_ASSERT(!material.use_texture);
    HENKA_TEST_ASSERT(material.use_lighting);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.x, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.y, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.z, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.base_color.w, 1.0f, 0.0001);
}
