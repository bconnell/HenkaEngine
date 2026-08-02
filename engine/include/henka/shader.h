#ifndef HENKA_SHADER_H
#define HENKA_SHADER_H

#include <stdint.h>

#include <henka/result.h>

typedef struct henka_engine henka_engine;
typedef struct henka_shader henka_shader;

typedef enum henka_shader_contract_type
{
    HENKA_SHADER_CONTRACT_MINIMAL_GEOMETRY = 0,
    HENKA_SHADER_CONTRACT_MATERIAL,
    HENKA_SHADER_CONTRACT_SOLID,
    HENKA_SHADER_CONTRACT_WIREFRAME,
    HENKA_SHADER_CONTRACT_UNLIT,
    HENKA_SHADER_CONTRACT_VERTEX_COLOR_LIT,
    HENKA_SHADER_CONTRACT_VERTEX_COLOR_UNLIT,
    HENKA_SHADER_CONTRACT_SHADOW_OPAQUE,
    HENKA_SHADER_CONTRACT_SHADOW_MASKED,
    HENKA_SHADER_CONTRACT_ENVIRONMENT,
    HENKA_SHADER_CONTRACT_TONE_MAP,
    HENKA_SHADER_CONTRACT_IBL_CONVERSION,
    HENKA_SHADER_CONTRACT_IBL_IRRADIANCE,
    HENKA_SHADER_CONTRACT_IBL_PREFILTER,
    HENKA_SHADER_CONTRACT_BRDF_LUT,
    HENKA_SHADER_CONTRACT_REFLECTION_PROBE_CAPTURE,
    HENKA_SHADER_CONTRACT_POINT_SHADOW,
    HENKA_SHADER_CONTRACT_SPOT_SHADOW,
    HENKA_SHADER_CONTRACT_AO,
    HENKA_SHADER_CONTRACT_SSR,
    HENKA_SHADER_CONTRACT_PLANAR_REFLECTION,
    HENKA_SHADER_CONTRACT_MOTION_VECTORS,
    HENKA_SHADER_CONTRACT_TAA,
    HENKA_SHADER_CONTRACT_BLOOM,
    HENKA_SHADER_CONTRACT_DEBUG,
    HENKA_SHADER_CONTRACT_UI
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
