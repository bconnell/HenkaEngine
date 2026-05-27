#ifndef HENKA_PHYSICS_H
#define HENKA_PHYSICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/camera.h>
#include <henka/math.h>
#include <henka/result.h>
#include <henka/scene.h>

typedef struct henka_physics_world henka_physics_world;
typedef uint32_t henka_physics_body_id;

#define HENKA_INVALID_PHYSICS_BODY_ID ((henka_physics_body_id)0)
#define HENKA_PHYSICS_ALL_LAYERS UINT32_MAX

typedef enum henka_physics_body_type
{
    HENKA_PHYSICS_BODY_STATIC = 0,
    HENKA_PHYSICS_BODY_DYNAMIC,
    HENKA_PHYSICS_BODY_KINEMATIC
} henka_physics_body_type;

typedef enum henka_physics_shape_type
{
    HENKA_PHYSICS_SHAPE_SPHERE = 0,
    HENKA_PHYSICS_SHAPE_BOX,
    HENKA_PHYSICS_SHAPE_PLANE
} henka_physics_shape_type;

typedef enum henka_physics_event_type
{
    HENKA_PHYSICS_EVENT_COLLISION_ENTER = 0,
    HENKA_PHYSICS_EVENT_COLLISION_STAY,
    HENKA_PHYSICS_EVENT_COLLISION_EXIT,
    HENKA_PHYSICS_EVENT_TRIGGER_ENTER,
    HENKA_PHYSICS_EVENT_TRIGGER_STAY,
    HENKA_PHYSICS_EVENT_TRIGGER_EXIT
} henka_physics_event_type;

typedef struct henka_physics_material
{
    float restitution;
    float static_friction;
    float dynamic_friction;
    float linear_damping;
    float angular_damping;
} henka_physics_material;

typedef struct henka_physics_collider_desc
{
    henka_physics_shape_type shape;
    henka_vec3 offset;
    union
    {
        struct
        {
            float radius;
        } sphere;
        struct
        {
            henka_vec3 half_extents;
        } box;
        struct
        {
            henka_vec3 normal;
            float offset;
        } plane;
    } data;
    bool is_trigger;
    uint32_t layer;
    uint32_t mask;
} henka_physics_collider_desc;

typedef struct henka_physics_body_desc
{
    henka_physics_body_type type;
    henka_transform transform;
    float mass;
    henka_vec3 linear_velocity;
    henka_vec3 angular_velocity;
    henka_physics_material material;
    henka_physics_collider_desc collider;
    henka_scene* linked_scene;
    henka_entity linked_entity;
} henka_physics_body_desc;

typedef struct henka_physics_body_state
{
    henka_physics_body_id id;
    henka_physics_body_type type;
    henka_transform transform;
    henka_transform initial_transform;
    float mass;
    henka_vec3 linear_velocity;
    henka_vec3 angular_velocity;
    henka_physics_material material;
    henka_physics_collider_desc collider;
    henka_scene* linked_scene;
    henka_entity linked_entity;
    bool colliding;
    bool grounded;
} henka_physics_body_state;

typedef struct henka_physics_contact
{
    henka_physics_body_id body_a;
    henka_physics_body_id body_b;
    henka_vec3 normal;
    henka_vec3 point;
    float penetration;
    bool is_trigger;
} henka_physics_contact;

typedef struct henka_physics_event
{
    henka_physics_event_type type;
    henka_physics_contact contact;
} henka_physics_event;

typedef struct henka_physics_raycast_hit
{
    bool hit;
    henka_physics_body_id body;
    henka_vec3 point;
    henka_vec3 normal;
    float distance;
} henka_physics_raycast_hit;

typedef struct henka_physics_debug_shape
{
    henka_physics_body_id body;
    henka_transform transform;
    henka_physics_collider_desc collider;
    bool colliding;
    bool grounded;
} henka_physics_debug_shape;

henka_physics_material henka_physics_material_default(void);
henka_physics_collider_desc henka_physics_collider_sphere(float radius);
henka_physics_collider_desc henka_physics_collider_box(henka_vec3 half_extents);
henka_physics_collider_desc henka_physics_collider_plane(henka_vec3 normal, float offset);

henka_result henka_physics_world_create(henka_physics_world** out_world);
void henka_physics_world_destroy(henka_physics_world* world);
henka_result henka_physics_world_set_gravity(henka_physics_world* world, henka_vec3 gravity);
henka_vec3 henka_physics_world_get_gravity(const henka_physics_world* world);
henka_result henka_physics_world_set_fixed_timestep(henka_physics_world* world, float fixed_timestep);
float henka_physics_world_get_fixed_timestep(const henka_physics_world* world);
size_t henka_physics_world_get_body_count(const henka_physics_world* world);

henka_result henka_physics_body_create(henka_physics_world* world, const henka_physics_body_desc* desc, henka_physics_body_id* out_body);
henka_result henka_physics_body_destroy(henka_physics_world* world, henka_physics_body_id body);
henka_result henka_physics_body_get_state(const henka_physics_world* world, henka_physics_body_id body, henka_physics_body_state* out_state);
henka_result henka_physics_body_set_transform(henka_physics_world* world, henka_physics_body_id body, henka_transform transform, bool clear_velocity);
henka_result henka_physics_body_set_type(henka_physics_world* world, henka_physics_body_id body, henka_physics_body_type type);
henka_result henka_physics_body_set_collider(henka_physics_world* world, henka_physics_body_id body, henka_physics_collider_desc collider);
henka_result henka_physics_body_set_material(henka_physics_world* world, henka_physics_body_id body, henka_physics_material material);
henka_result henka_physics_body_set_linear_velocity(henka_physics_world* world, henka_physics_body_id body, henka_vec3 velocity);
henka_result henka_physics_body_set_angular_velocity(henka_physics_world* world, henka_physics_body_id body, henka_vec3 velocity);
henka_result henka_physics_body_apply_force(henka_physics_world* world, henka_physics_body_id body, henka_vec3 force);
henka_result henka_physics_body_apply_impulse(henka_physics_world* world, henka_physics_body_id body, henka_vec3 impulse);
henka_result henka_physics_body_apply_torque(henka_physics_world* world, henka_physics_body_id body, henka_vec3 torque);
henka_result henka_physics_body_clear_velocity(henka_physics_world* world, henka_physics_body_id body);

henka_result henka_physics_world_step(henka_physics_world* world, float delta_seconds);
henka_result henka_physics_world_step_fixed(henka_physics_world* world);
henka_result henka_physics_world_reset(henka_physics_world* world);
const henka_physics_contact* henka_physics_world_get_contacts(const henka_physics_world* world, size_t* out_count);
const henka_physics_event* henka_physics_world_get_events(const henka_physics_world* world, size_t* out_count);
henka_result henka_physics_world_raycast(const henka_physics_world* world, henka_ray ray, float max_distance, uint32_t layer_mask, henka_physics_raycast_hit* out_hit);
size_t henka_physics_world_get_debug_shape_count(const henka_physics_world* world);
henka_result henka_physics_world_get_debug_shape(const henka_physics_world* world, size_t index, henka_physics_debug_shape* out_shape);

const char* henka_physics_body_type_get_label(henka_physics_body_type type);
const char* henka_physics_shape_type_get_label(henka_physics_shape_type type);
const char* henka_physics_event_type_get_label(henka_physics_event_type type);

#endif
