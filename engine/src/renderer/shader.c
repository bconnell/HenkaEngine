#include "henka_internal.h"

#include <henka/log.h>

henka_shader_contract_desc henka_shader_contract_desc_default(
    henka_shader_contract_type type)
{
    return (henka_shader_contract_desc){type, 1U};
}

henka_result henka_shader_contract_desc_validate(
    const henka_shader_contract_desc* desc)
{
    if (desc == NULL ||
        desc->type < HENKA_SHADER_CONTRACT_MINIMAL_GEOMETRY ||
        desc->type > HENKA_SHADER_CONTRACT_UI ||
        desc->version != 1U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_SUCCESS;
}

henka_result henka_shader_create_from_files(henka_engine* engine, const char* vertex_path, const char* fragment_path, henka_shader** out_shader)
{
    henka_shader_contract_desc contract =
        henka_shader_contract_desc_default(HENKA_SHADER_CONTRACT_MINIMAL_GEOMETRY);

    return henka_shader_create_from_files_with_contract(
        engine,
        vertex_path,
        fragment_path,
        &contract,
        out_shader);
}

henka_result henka_shader_create_from_files_with_contract(
    henka_engine* engine,
    const char* vertex_path,
    const char* fragment_path,
    const henka_shader_contract_desc* contract,
    henka_shader** out_shader)
{
    if (engine == NULL || out_shader == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_renderer_create_shader_from_files_with_contract(
        engine->renderer,
        vertex_path,
        fragment_path,
        contract,
        out_shader);
}

void henka_shader_destroy(henka_shader* shader)
{
    if (shader == NULL)
    {
        return;
    }

    if (shader->asset_manager_owned)
    {
        HENKA_LOG_WARN(
            "ignored an attempt to destroy a manager-owned borrowed shader");
        return;
    }

    henka_renderer_destroy_shader(shader);
}

void henka_shader_destroy_owned(henka_shader* shader)
{
    if (shader == NULL)
    {
        return;
    }

    shader->asset_manager_owned = false;
    henka_renderer_destroy_shader(shader);
}
