#include "test_suite.h"

#include <string.h>

#include <henka/physics.h>

static henka_physics_body_desc henka_test_physics_body(
    henka_physics_body_type type,
    henka_physics_collider_desc collider,
    henka_vec3 position)
{
    henka_physics_body_desc desc = {0};
    desc.type = type;
    desc.transform = henka_transform_identity();
    desc.transform.position = position;
    desc.mass = 1.0f;
    desc.material = henka_physics_material_default();
    desc.collider = collider;
    return desc;
}

static bool henka_test_has_event(const henka_physics_world* world, henka_physics_event_type type)
{
    size_t index;
    size_t count;
    const henka_physics_event* events = henka_physics_world_get_events(world, &count);
    for (index = 0U; index < count; ++index)
    {
        if (events[index].type == type)
        {
            return true;
        }
    }
    return false;
}

static void henka_test_physics_motion_and_materials(void)
{
    henka_physics_world* world;
    henka_physics_body_id ground;
    henka_physics_body_id dynamic;
    henka_physics_body_id stationary;
    henka_physics_body_id kinematic;
    henka_physics_body_desc desc;
    henka_physics_body_state state;
    int index;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_physics_world_get_fixed_timestep(world), 1.0f / 60.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_physics_world_get_gravity(world).y, -9.81f, 0.0001f);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_STATIC, henka_physics_collider_plane((henka_vec3){0.0f, 1.0f, 0.0f}, 0.0f), (henka_vec3){0.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &ground) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_sphere(0.5f), (henka_vec3){0.0f, 2.0f, 0.0f});
    desc.material.linear_damping = 0.5f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &dynamic) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_STATIC, henka_physics_collider_box((henka_vec3){0.5f, 0.5f, 0.5f}), (henka_vec3){3.0f, 3.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &stationary) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_KINEMATIC, henka_physics_collider_box((henka_vec3){0.5f, 0.5f, 0.5f}), (henka_vec3){5.0f, 3.0f, 0.0f});
    desc.linear_velocity.x = 1.0f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &kinematic) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_physics_body_apply_impulse(world, dynamic, (henka_vec3){1.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_apply_force(world, dynamic, (henka_vec3){0.0f, 4.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_apply_torque(world, dynamic, (henka_vec3){0.0f, 0.0f, 2.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, dynamic, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.transform.position.y < 2.0f);
    HENKA_TEST_ASSERT(state.linear_velocity.x > 0.0f && state.linear_velocity.x < 1.0f);
    HENKA_TEST_ASSERT(state.angular_velocity.z > 0.0f);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, stationary, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.transform.position.y, 3.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_physics_body_set_collider(world, stationary, henka_physics_collider_sphere(0.8f)) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, stationary, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.collider.shape == HENKA_PHYSICS_SHAPE_SPHERE);
    HENKA_TEST_ASSERT(henka_physics_body_set_type(world, ground, HENKA_PHYSICS_BODY_DYNAMIC) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, kinematic, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.transform.position.x > 5.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.transform.position.y, 3.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_physics_body_set_type(world, stationary, HENKA_PHYSICS_BODY_DYNAMIC) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_clear_velocity(world, stationary) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, stationary, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.type == HENKA_PHYSICS_BODY_DYNAMIC);
    HENKA_TEST_ASSERT(state.transform.position.y < 3.0f);
    HENKA_TEST_ASSERT(henka_physics_body_set_type(world, stationary, HENKA_PHYSICS_BODY_KINEMATIC) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, stationary, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.type == HENKA_PHYSICS_BODY_KINEMATIC);

    for (index = 0; index < 180; ++index)
    {
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, dynamic, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.transform.position.y > 0.35f);
    HENKA_TEST_ASSERT(state.transform.position.y < 0.7f);
    HENKA_TEST_ASSERT(state.colliding || state.grounded);
    HENKA_TEST_ASSERT(henka_physics_world_get_contacts(world, NULL) != NULL);
    HENKA_TEST_ASSERT(henka_physics_world_reset(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, dynamic, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.transform.position.y, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.linear_velocity.y, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_physics_body_destroy(world, dynamic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, dynamic, &state) == HENKA_ERROR_INVALID_ARGUMENT);
    henka_physics_world_destroy(world);
}

static void henka_test_physics_contacts_and_events(void)
{
    henka_physics_world* world;
    henka_physics_body_id first;
    henka_physics_body_id second;
    henka_physics_body_desc desc;
    henka_physics_body_state state;
    size_t count;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_sphere(1.0f), (henka_vec3){0.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &first) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_sphere(1.0f), (henka_vec3){1.5f, 0.0f, 0.0f});
    desc.linear_velocity.x = -1.0f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &second) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_get_contacts(world, &count) != NULL && count == 1U);
    HENKA_TEST_ASSERT(henka_test_has_event(world, HENKA_PHYSICS_EVENT_COLLISION_ENTER));
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, first, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.transform.position.x < 0.0f);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_has_event(world, HENKA_PHYSICS_EVENT_COLLISION_STAY) ||
        henka_test_has_event(world, HENKA_PHYSICS_EVENT_COLLISION_EXIT));
    HENKA_TEST_ASSERT(henka_physics_body_set_transform(world, second, (henka_transform){{10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}}, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_has_event(world, HENKA_PHYSICS_EVENT_COLLISION_EXIT));
    henka_physics_world_destroy(world);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_sphere(0.75f), (henka_vec3){0.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &first) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_STATIC, henka_physics_collider_box((henka_vec3){1.0f, 1.0f, 1.0f}), (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.collider.is_trigger = true;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &second) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_has_event(world, HENKA_PHYSICS_EVENT_TRIGGER_ENTER));
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, first, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.transform.position.x, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_has_event(world, HENKA_PHYSICS_EVENT_TRIGGER_STAY));
    state.transform.position.x = 4.0f;
    HENKA_TEST_ASSERT(henka_physics_body_set_transform(world, first, state.transform, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_has_event(world, HENKA_PHYSICS_EVENT_TRIGGER_EXIT));
    henka_physics_world_destroy(world);
}

static void henka_test_physics_shape_pairs_and_raycast(void)
{
    henka_physics_world* world;
    henka_physics_body_desc desc;
    henka_physics_body_id sphere;
    henka_physics_body_id box;
    henka_physics_body_id plane;
    henka_physics_raycast_hit hit;
    henka_physics_debug_shape debug_shape;
    size_t count;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_sphere(0.5f), (henka_vec3){0.0f, 0.4f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &sphere) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_STATIC, henka_physics_collider_box((henka_vec3){1.0f, 1.0f, 1.0f}), (henka_vec3){2.0f, 0.4f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &box) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_STATIC, henka_physics_collider_plane((henka_vec3){0.0f, 1.0f, 0.0f}, 0.0f), (henka_vec3){0.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &plane) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_get_contacts(world, &count) != NULL && count >= 1U);
    HENKA_TEST_ASSERT(henka_physics_world_raycast(world, (henka_ray){{0.0f, 0.4f, 3.0f}, {0.0f, 0.0f, -1.0f}}, 10.0f, HENKA_PHYSICS_ALL_LAYERS, &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(hit.hit && hit.body == sphere);
    HENKA_TEST_ASSERT(henka_physics_world_raycast(world, (henka_ray){{2.0f, 0.4f, 3.0f}, {0.0f, 0.0f, -1.0f}}, 10.0f, HENKA_PHYSICS_ALL_LAYERS, &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(hit.hit && hit.body == box);
    HENKA_TEST_ASSERT(henka_physics_world_raycast(world, (henka_ray){{8.0f, 4.0f, 0.0f}, {0.0f, -1.0f, 0.0f}}, 10.0f, HENKA_PHYSICS_ALL_LAYERS, &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(hit.hit && hit.body == plane);
    HENKA_TEST_ASSERT(henka_physics_world_raycast(world, (henka_ray){{8.0f, 4.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}, 2.0f, HENKA_PHYSICS_ALL_LAYERS, &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!hit.hit);
    HENKA_TEST_ASSERT(henka_physics_world_get_debug_shape_count(world) == 3U);
    HENKA_TEST_ASSERT(henka_physics_world_get_debug_shape(world, 0U, &debug_shape) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(debug_shape.body == sphere);
    henka_physics_world_destroy(world);
}

static void henka_test_physics_shape_pair(
    henka_physics_collider_desc first_collider,
    henka_vec3 first_position,
    henka_physics_collider_desc second_collider,
    henka_vec3 second_position)
{
    henka_physics_world* world;
    henka_physics_body_desc desc;
    henka_physics_body_id body;
    size_t count;
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_DYNAMIC, first_collider, first_position);
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &body) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_STATIC, second_collider, second_position);
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &body) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    (void)henka_physics_world_get_contacts(world, &count);
    HENKA_TEST_ASSERT(count == 1U);
    henka_physics_world_destroy(world);
}

static void henka_test_physics_pair_filters_and_response(void)
{
    henka_physics_world* world;
    henka_physics_body_desc desc;
    henka_physics_body_id ground;
    henka_physics_body_id dynamic;
    henka_physics_body_state state;
    henka_transform before;
    size_t count;
    int index;

    henka_test_physics_shape_pair(
        henka_physics_collider_sphere(0.75f), (henka_vec3){0.0f, 0.0f, 0.0f},
        henka_physics_collider_box((henka_vec3){0.75f, 0.75f, 0.75f}), (henka_vec3){0.9f, 0.0f, 0.0f});
    henka_test_physics_shape_pair(
        henka_physics_collider_box((henka_vec3){0.75f, 0.75f, 0.75f}), (henka_vec3){0.0f, 0.0f, 0.0f},
        henka_physics_collider_box((henka_vec3){0.75f, 0.75f, 0.75f}), (henka_vec3){1.0f, 0.0f, 0.0f});
    henka_test_physics_shape_pair(
        henka_physics_collider_box((henka_vec3){0.5f, 0.5f, 0.5f}), (henka_vec3){0.0f, 0.3f, 0.0f},
        henka_physics_collider_plane((henka_vec3){0.0f, 1.0f, 0.0f}, 0.0f), (henka_vec3){0.0f, 0.0f, 0.0f});

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_sphere(1.0f), (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.collider.layer = 1U;
    desc.collider.mask = 1U;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &ground) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_sphere(1.0f), (henka_vec3){1.0f, 0.0f, 0.0f});
    desc.collider.layer = 2U;
    desc.collider.mask = 2U;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &dynamic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    (void)henka_physics_world_get_contacts(world, &count);
    HENKA_TEST_ASSERT(count == 0U);
    henka_physics_world_destroy(world);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_STATIC, henka_physics_collider_box((henka_vec3){1.0f, 1.0f, 1.0f}), (henka_vec3){0.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &ground) == HENKA_SUCCESS);
    desc.transform.position.x = 0.5f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &dynamic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    (void)henka_physics_world_get_contacts(world, &count);
    HENKA_TEST_ASSERT(count == 0U);
    henka_physics_world_destroy(world);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_sphere(0.5f), (henka_vec3){0.0f, 2.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &dynamic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, dynamic, &state) == HENKA_SUCCESS);
    before = state.transform;
    HENKA_TEST_ASSERT(henka_physics_world_step(world, (1.0f / 60.0f) * 0.5f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, dynamic, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.transform.position.y, before.position.y, 0.0001f);
    HENKA_TEST_ASSERT(henka_physics_world_step(world, (1.0f / 60.0f) * 0.5f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, dynamic, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.transform.position.y < before.position.y);
    henka_physics_world_destroy(world);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_STATIC, henka_physics_collider_plane((henka_vec3){0.0f, 1.0f, 0.0f}, 0.0f), (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.material.dynamic_friction = 1.0f;
    desc.material.static_friction = 1.0f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &ground) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_box((henka_vec3){0.5f, 0.5f, 0.5f}), (henka_vec3){0.0f, 0.45f, 0.0f});
    desc.linear_velocity = (henka_vec3){3.0f, -1.0f, 0.0f};
    desc.material.dynamic_friction = 1.0f;
    desc.material.static_friction = 1.0f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &dynamic) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, dynamic, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.linear_velocity.x < 3.0f);
    henka_physics_world_destroy(world);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_STATIC, henka_physics_collider_plane((henka_vec3){0.0f, 1.0f, 0.0f}, 0.0f), (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.material.restitution = 0.9f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &ground) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_sphere(0.5f), (henka_vec3){0.0f, 1.0f, 0.0f});
    desc.material.restitution = 0.9f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &dynamic) == HENKA_SUCCESS);
    for (index = 0; index < 60; ++index)
    {
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, dynamic, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.transform.position.y > 0.5f || state.linear_velocity.y > 0.0f);
    henka_physics_world_destroy(world);
}

static void henka_test_physics_scene_link(void)
{
    henka_scene* scene;
    henka_entity entity;
    henka_physics_world* world;
    henka_physics_body_desc desc;
    henka_physics_body_id body;
    henka_transform transform;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Physics Body");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    desc = henka_test_physics_body(HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_sphere(0.5f), (henka_vec3){0.0f, 2.0f, 0.0f});
    desc.linked_scene = scene;
    desc.linked_entity = entity;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &body) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, entity, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(transform.position.y < 2.0f);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_helper(scene, entity));
    HENKA_TEST_ASSERT(henka_physics_world_reset(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, entity, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.y, 2.0f, 0.0001f);
    henka_physics_world_destroy(world);
    henka_scene_destroy(scene);
}

void henka_test_physics(void)
{
    HENKA_TEST_ASSERT(strcmp(henka_physics_body_type_get_label(HENKA_PHYSICS_BODY_DYNAMIC), "Dynamic") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_physics_shape_type_get_label(HENKA_PHYSICS_SHAPE_BOX), "AABB") == 0);
    henka_test_physics_motion_and_materials();
    henka_test_physics_contacts_and_events();
    henka_test_physics_shape_pairs_and_raycast();
    henka_test_physics_pair_filters_and_response();
    henka_test_physics_scene_link();
}
