#ifndef HENKA_RUNTIME_INTERNAL_H
#define HENKA_RUNTIME_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/runtime.h>

typedef struct henka_scene_entity_record
{
    bool active;
    uint64_t generation;
    bool visible;
    uint32_t flags;
    char* name;
    char* tag;
    henka_transform transform;
    henka_transform previous_transform;
    bool previous_transform_valid;
    henka_mesh* mesh;
    henka_scene_lod_desc lod;
    henka_material material;
    const henka_material_asset* material_asset;
    char* material_name;
    bool has_local_bounds;
    henka_bounds local_bounds;
    henka_interaction_desc interaction;
    char* interaction_prompt;
} henka_scene_entity_record;

struct henka_scene
{
    henka_scene_entity_record* entities;
    size_t entity_capacity;
    size_t entity_count;
    henka_camera camera;
    bool has_camera;
    henka_vec3 light_direction;
    henka_vec3 light_color;
    float light_intensity;
    henka_vec3 ambient_color;
    henka_scene_environment_desc environment;
    henka_scene_reflection_probe_desc reflection_probes[HENKA_SCENE_MAX_REFLECTION_PROBES];
    bool reflection_probe_active[HENKA_SCENE_MAX_REFLECTION_PROBES];
    uint64_t render_revision;
    henka_scene_light_desc local_lights[HENKA_SCENE_MAX_LOCAL_LIGHTS];
    bool local_light_active[HENKA_SCENE_MAX_LOCAL_LIGHTS];
    henka_scene_fog_desc fog;
};

#endif
