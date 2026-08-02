#ifndef HENKA_SHADER_H
#define HENKA_SHADER_H

#include <stdint.h>

#include <henka/result.h>

typedef struct henka_engine henka_engine;
typedef struct henka_shader henka_shader;

typedef enum henka_shader_contract_type
{
    HENKA_SHADER_CONTRACT_MINIMAL_GEOMETRY = 0,
    HENKA_SHADER_CONTRACT_MATERIAL
} henka_shader_contract_type;

typedef struct henka_shader_contract_desc
{
    henka_shader_contract_type type;
    uint32_t version;
} henka_shader_contract_desc;

henka_shader_contract_desc henka_shader_contract_desc_default(
    henka_shader_contract_type type);
henka_result henka_shader_contract_desc_validate(
    const henka_shader_contract_desc* desc);

henka_result henka_shader_create_from_files(henka_engine* engine, const char* vertex_path, const char* fragment_path, henka_shader** out_shader);
henka_result henka_shader_create_from_files_with_contract(
    henka_engine* engine,
    const char* vertex_path,
    const char* fragment_path,
    const henka_shader_contract_desc* contract,
    henka_shader** out_shader);
/* Releases caller-owned shaders. Manager-owned borrowed shaders are ignored. */
void henka_shader_destroy(henka_shader* shader);

#endif
