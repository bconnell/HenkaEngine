#ifndef HENKA_MESH_H
#define HENKA_MESH_H

#include <henka/result.h>

typedef struct henka_engine henka_engine;
typedef struct henka_mesh henka_mesh;
typedef struct henka_model_data henka_model_data;

henka_result henka_mesh_create_cube(henka_engine* engine, henka_mesh** out_mesh);
henka_result henka_mesh_create_plane(henka_engine* engine, float width, float depth, henka_mesh** out_mesh);
henka_result henka_mesh_create_debug_grid(henka_engine* engine, int half_extent, float spacing, henka_mesh** out_mesh);
henka_result henka_mesh_create_from_model_data(henka_engine* engine, const henka_model_data* model, henka_mesh** out_mesh);
henka_result henka_mesh_create_from_obj(henka_engine* engine, const char* path, henka_mesh** out_mesh);
void henka_mesh_destroy(henka_mesh* mesh);

#endif
