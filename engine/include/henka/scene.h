#ifndef HENKA_SCENE_H
#define HENKA_SCENE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/camera.h>
#include <henka/math.h>
#include <henka/mesh.h>
#include <henka/result.h>
#include <henka/shader.h>

typedef struct henka_scene henka_scene;
typedef uint32_t henka_entity;

#define HENKA_INVALID_ENTITY ((henka_entity)0)

typedef struct henka_material
{
    henka_shader* shader;
    henka_vec4 base_color;
    bool use_lighting;
} henka_material;

henka_result henka_scene_create(henka_scene** out_scene);
void henka_scene_destroy(henka_scene* scene);
henka_entity henka_scene_create_entity(henka_scene* scene);
void henka_scene_destroy_entity(henka_scene* scene, henka_entity entity);
bool henka_scene_is_entity_valid(const henka_scene* scene, henka_entity entity);
size_t henka_scene_get_entity_count(const henka_scene* scene);
henka_result henka_scene_set_entity_transform(henka_scene* scene, henka_entity entity, henka_transform transform);
henka_result henka_scene_set_entity_mesh(henka_scene* scene, henka_entity entity, henka_mesh* mesh);
henka_result henka_scene_set_entity_material(henka_scene* scene, henka_entity entity, henka_material material);
henka_result henka_scene_set_camera(henka_scene* scene, const henka_camera* camera);
void henka_scene_set_light_direction(henka_scene* scene, henka_vec3 light_direction);
void henka_scene_set_ambient_color(henka_scene* scene, henka_vec3 ambient_color);

#endif
