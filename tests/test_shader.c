#include "test_suite.h"

#include <henka/shader.h>

void henka_test_shader(void)
{
    henka_shader_contract_desc minimal;
    henka_shader_contract_desc material;
    henka_shader_contract_desc invalid;

    minimal = henka_shader_contract_desc_default(
        HENKA_SHADER_CONTRACT_MINIMAL_GEOMETRY);
    material = henka_shader_contract_desc_default(
        HENKA_SHADER_CONTRACT_MATERIAL);
    HENKA_TEST_ASSERT(minimal.type == HENKA_SHADER_CONTRACT_MINIMAL_GEOMETRY);
    HENKA_TEST_ASSERT(minimal.version == 1U);
    HENKA_TEST_ASSERT(material.type == HENKA_SHADER_CONTRACT_MATERIAL);
    HENKA_TEST_ASSERT(
        henka_shader_contract_desc_validate(&minimal) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_shader_contract_desc_validate(&material) == HENKA_SUCCESS);

    invalid = material;
    invalid.version = 0U;
    HENKA_TEST_ASSERT(
        henka_shader_contract_desc_validate(&invalid) ==
        HENKA_ERROR_INVALID_ARGUMENT);
    invalid = material;
    invalid.type = (henka_shader_contract_type)-1;
    HENKA_TEST_ASSERT(
        henka_shader_contract_desc_validate(&invalid) ==
        HENKA_ERROR_INVALID_ARGUMENT);
    invalid = material;
    invalid.type = (henka_shader_contract_type)(HENKA_SHADER_CONTRACT_UI + 1);
    HENKA_TEST_ASSERT(
        henka_shader_contract_desc_validate(&invalid) ==
        HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(
        henka_shader_contract_desc_validate(NULL) ==
        HENKA_ERROR_INVALID_ARGUMENT);
}
