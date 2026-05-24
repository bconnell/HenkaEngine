#ifndef HENKA_SHADER_H
#define HENKA_SHADER_H

#include <henka/result.h>

typedef struct henka_engine henka_engine;
typedef struct henka_shader henka_shader;

henka_result henka_shader_create_from_files(henka_engine* engine, const char* vertex_path, const char* fragment_path, henka_shader** out_shader);
void henka_shader_destroy(henka_shader* shader);

#endif
