#ifndef HENKA_MODEL_H
#define HENKA_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/result.h>

typedef struct henka_engine henka_engine;
typedef struct henka_mesh henka_mesh;

typedef struct henka_model_vertex
{
    henka_vec3 position;
    henka_vec3 normal;
    henka_vec2 uv;
    henka_vec4 color;
    /* Optional imported tangent frame. w is the handedness sign. */
    henka_vec4 tangent;
    bool tangent_valid;
} henka_model_vertex;

typedef struct henka_model_data
{
    henka_model_vertex* vertices;
    uint32_t vertex_count;
    uint32_t* indices;
    uint32_t index_count;
} henka_model_data;

/* OBJ loading enforces bounded source, record, output, numeric, and renderer-count limits. */
henka_result henka_model_data_load_obj(const char* path, henka_model_data* out_model);
henka_result henka_model_data_load_obj_from_memory(const char* source, const char* label, henka_model_data* out_model);
/* glTF 2.0 and GLB loading support bounded triangle primitives and core vertex attributes. */
henka_result henka_model_data_load_gltf(const char* path, henka_model_data* out_model);
henka_result henka_model_data_load_gltf_from_memory(
    const void* data,
    size_t data_size,
    const char* label,
    henka_model_data* out_model);
void henka_model_data_destroy(henka_model_data* model);
henka_result henka_mesh_create_from_model_data(henka_engine* engine, const henka_model_data* model, henka_mesh** out_mesh);
henka_result henka_mesh_create_from_obj(henka_engine* engine, const char* path, henka_mesh** out_mesh);
henka_result henka_mesh_create_from_gltf(henka_engine* engine, const char* path, henka_mesh** out_mesh);

#endif
