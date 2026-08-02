#ifndef HENKA_MODEL_H
#define HENKA_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/result.h>
#include <henka/scene.h>

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

typedef struct henka_model_material_source
{
    /* Scalar values use the same authoritative material model as scenes. */
    henka_material material;
    char* name;
    /* Source-relative image URIs; GPU texture ownership stays with the asset manager. */
    char* base_color_uri;
    char* normal_uri;
    char* metallic_roughness_uri;
    char* occlusion_uri;
    char* emissive_uri;
    unsigned char* base_color_embedded_data;
    size_t base_color_embedded_size;
    unsigned char* normal_embedded_data;
    size_t normal_embedded_size;
    unsigned char* metallic_roughness_embedded_data;
    size_t metallic_roughness_embedded_size;
    unsigned char* occlusion_embedded_data;
    size_t occlusion_embedded_size;
    unsigned char* emissive_embedded_data;
    size_t emissive_embedded_size;
} henka_model_material_source;

#define HENKA_MODEL_MAX_SCENE_ITEMS 256U

typedef struct henka_model_scene_primitive
{
    henka_model_vertex* vertices;
    uint32_t vertex_count;
    uint32_t* indices;
    uint32_t index_count;
    uint32_t mesh_index;
    int material_index;
} henka_model_scene_primitive;

typedef struct henka_model_scene_node
{
    char* name;
    int parent_index;
    int mesh_index;
    int camera_index;
    int light_index;
    henka_transform local_transform;
    henka_transform world_transform;
    henka_mat4 local_matrix;
    henka_mat4 world_matrix;
} henka_model_scene_node;

typedef struct henka_model_scene_camera
{
    char* name;
    henka_camera camera;
} henka_model_scene_camera;

typedef enum henka_model_scene_light_type
{
    HENKA_MODEL_SCENE_LIGHT_POINT = 0,
    HENKA_MODEL_SCENE_LIGHT_SPOT,
    HENKA_MODEL_SCENE_LIGHT_DIRECTIONAL
} henka_model_scene_light_type;

typedef struct henka_model_scene_light
{
    char* name;
    henka_model_scene_light_type type;
    henka_vec3 color;
    float intensity;
    float range;
    float inner_cone_cosine;
    float outer_cone_cosine;
} henka_model_scene_light;

typedef struct henka_model_scene_data
{
    henka_model_scene_primitive primitives[HENKA_MODEL_MAX_SCENE_ITEMS];
    size_t primitive_count;
    henka_model_material_source materials[HENKA_MODEL_MAX_SCENE_ITEMS];
    bool material_present[HENKA_MODEL_MAX_SCENE_ITEMS];
    size_t material_count;
    henka_model_scene_node nodes[HENKA_MODEL_MAX_SCENE_ITEMS];
    size_t node_count;
    henka_model_scene_camera cameras[HENKA_MODEL_MAX_SCENE_ITEMS];
    size_t camera_count;
    henka_model_scene_light lights[HENKA_MODEL_MAX_SCENE_ITEMS];
    size_t light_count;
    size_t scene_count;
    size_t active_scene_index;
    size_t scene_root_offsets[HENKA_MODEL_MAX_SCENE_ITEMS];
    size_t scene_root_counts[HENKA_MODEL_MAX_SCENE_ITEMS];
    int scene_root_nodes[HENKA_MODEL_MAX_SCENE_ITEMS];
    size_t scene_root_node_count;
} henka_model_scene_data;

typedef struct henka_model_data
{
    henka_model_vertex* vertices;
    uint32_t vertex_count;
    uint32_t* indices;
    uint32_t index_count;
    bool has_material;
    henka_model_material_source material_source;
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
henka_result henka_model_scene_data_load_gltf(const char* path, henka_model_scene_data* out_scene);
henka_result henka_model_scene_data_load_gltf_from_memory(
    const void* data,
    size_t data_size,
    const char* label,
    henka_model_scene_data* out_scene);
void henka_model_scene_data_destroy(henka_model_scene_data* scene);
void henka_model_data_destroy(henka_model_data* model);
henka_result henka_mesh_create_from_model_data(henka_engine* engine, const henka_model_data* model, henka_mesh** out_mesh);
henka_result henka_mesh_create_from_obj(henka_engine* engine, const char* path, henka_mesh** out_mesh);
henka_result henka_mesh_create_from_gltf(henka_engine* engine, const char* path, henka_mesh** out_mesh);

#endif
