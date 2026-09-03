#include <math.h>
#include <string.h>

#include <henka/character_controller.h>
#include <henka/physics.h>
#include <henka/scene.h>

#include "test_suite.h"

static henka_character_controller_desc henka_test_character_controller_desc(void)
{
    henka_character_controller_desc desc = {0};
    desc.transform = henka_transform_identity();
    desc.transform.position = (henka_vec3){0.0f, 2.0f, 0.0f};
    desc.radius = 0.5f;
    desc.half_height = 0.75f;
    desc.max_speed = 3.0f;
    desc.jump_speed = 5.0f;
    desc.layer = 1U;
    desc.mask = HENKA_PHYSICS_ALL_LAYERS;
    return desc;
}

static void henka_test_character_controller_create_and_validate(void)
{
    henka_physics_world* world = NULL;
    henka_character_controller* controller = NULL;
    henka_character_controller_desc desc = henka_test_character_controller_desc();
    henka_character_controller_state state;
    henka_physics_body_state body_state;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &desc, &controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(controller != NULL);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(
        controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.body != HENKA_INVALID_PHYSICS_BODY_ID);
    HENKA_TEST_ASSERT(state.transform.position.y == 2.0f);
    HENKA_TEST_ASSERT(!state.grounded);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(
        world, state.body, &body_state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(body_state.collider.shape == HENKA_PHYSICS_SHAPE_CAPSULE);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        body_state.collider.data.capsule.half_height, 0.75f, 0.0001f);
    HENKA_TEST_ASSERT(henka_character_controller_set_planar_velocity(
        controller, (henka_vec3){1.0f, 0.0f, 2.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_planar_velocity(
        controller, (henka_vec3){1.0f, 0.1f, 2.0f}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_destroy(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_get_body_count(world) == 0U);
    henka_physics_world_destroy(world);
}

static void henka_test_character_controller_moves_and_jumps(void)
{
    henka_physics_world* world = NULL;
    henka_character_controller* controller = NULL;
    henka_character_controller_desc desc = henka_test_character_controller_desc();
    henka_physics_body_desc floor_desc = {0};
    henka_character_controller_state state;
    henka_physics_body_state body_state;
    henka_physics_body_id floor;
    size_t step;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    floor_desc.type = HENKA_PHYSICS_BODY_STATIC;
    floor_desc.transform = henka_transform_identity();
    floor_desc.material = henka_physics_material_default();
    floor_desc.collider = henka_physics_collider_plane(
        (henka_vec3){0.0f, 1.0f, 0.0f}, 0.0f);
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &floor_desc, &floor) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &desc, &controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(
        controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_planar_velocity(
        controller, (henka_vec3){100.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(
        world, state.body, &body_state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(fabsf(body_state.linear_velocity.x - 3.0f) < 0.001f);
    for (step = 0U; step < 180U; ++step)
    {
        HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_character_controller_sync_after_step(controller) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.transform.position.x > 1.0f);
    HENKA_TEST_ASSERT(state.transform.position.y > 1.2f && state.transform.position.y < 1.3f);
    HENKA_TEST_ASSERT(state.grounded);
    HENKA_TEST_ASSERT(state.velocity.x > 0.0f && state.velocity.x <= 3.0f);
    HENKA_TEST_ASSERT(henka_character_controller_queue_jump(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_sync_after_step(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.velocity.y > 0.0f);
    HENKA_TEST_ASSERT(!state.grounded);
    HENKA_TEST_ASSERT(henka_character_controller_destroy(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_destroy(world, floor) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);
}

static void henka_test_character_controller_movement_tuning(void)
{
    henka_physics_world* world = NULL;
    henka_character_controller* controller = NULL;
    henka_character_controller_desc desc = henka_test_character_controller_desc();
    henka_character_controller_state state;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &desc, &controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_movement_tuning(
        controller, 6.0f, 12.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_planar_velocity(
        controller, (henka_vec3){3.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.x, 0.1f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.z, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.x, 0.2f, 0.0001f);

    HENKA_TEST_ASSERT(henka_character_controller_set_planar_velocity(
        controller, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.x, 0.0f, 0.0001f);

    HENKA_TEST_ASSERT(henka_character_controller_set_movement_tuning(
        controller, NAN, 12.0f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_set_movement_tuning(
        controller, 6.0f, -1.0f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_destroy(controller) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);
}

static void henka_test_character_controller_air_control(void)
{
    henka_physics_world* world = NULL;
    henka_character_controller* controller = NULL;
    henka_character_controller_desc desc = henka_test_character_controller_desc();
    henka_character_controller_state state;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &desc, &controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_planar_velocity(
        controller, (henka_vec3){3.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.x, 3.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_character_controller_teleport(
        controller, desc.transform, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_air_control(
        controller, 0.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.x, 0.0f, 0.0001f);

    HENKA_TEST_ASSERT(henka_character_controller_set_air_control(
        controller, 0.5f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.x, 1.5f, 0.0001f);

    HENKA_TEST_ASSERT(henka_character_controller_set_air_control(
        controller, 1.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.x, 3.0f, 0.0001f);

    HENKA_TEST_ASSERT(henka_character_controller_set_air_control(
        controller, NAN) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_teleport(
        controller, desc.transform, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_planar_velocity(
        controller, (henka_vec3){3.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.x, 3.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_character_controller_set_air_control(
        controller, 1.1f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_teleport(
        controller, desc.transform, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.x, 3.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_character_controller_destroy(controller) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);
}

static void henka_test_character_controller_teleport(void)
{
    henka_physics_world* world = NULL;
    henka_character_controller* controller = NULL;
    henka_character_controller_desc desc = henka_test_character_controller_desc();
    henka_character_controller_state state;
    henka_transform destination = henka_transform_identity();
    henka_transform invalid_destination;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &desc, &controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_planar_velocity(
        controller, (henka_vec3){2.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
    destination.position = (henka_vec3){5.0f, 4.0f, -2.0f};
    HENKA_TEST_ASSERT(henka_character_controller_teleport(
        controller, destination, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.transform.position.x, 5.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.transform.position.y, 4.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.transform.position.z, -2.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.x, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.y, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.velocity.z, 0.0f, 0.0001f);

    invalid_destination = destination;
    invalid_destination.scale.y = 0.0f;
    HENKA_TEST_ASSERT(henka_character_controller_teleport(
        controller, invalid_destination, true) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.transform.position.x, 5.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.transform.position.y, 4.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.transform.position.z, -2.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_character_controller_destroy(controller) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);
}

static void henka_test_character_controller_slope_grounding(void)
{
    const float slope_angle = 20.0f * 3.14159265358979323846f / 180.0f;
    henka_physics_world* world = NULL;
    henka_character_controller* controller = NULL;
    henka_character_controller_desc desc = henka_test_character_controller_desc();
    henka_physics_body_desc floor_desc = {0};
    henka_character_controller_state state;
    henka_physics_body_id floor;
    size_t step;

    floor_desc.type = HENKA_PHYSICS_BODY_STATIC;
    floor_desc.transform = henka_transform_identity();
    floor_desc.material = henka_physics_material_default();
    floor_desc.collider = henka_physics_collider_plane(
        (henka_vec3){sinf(slope_angle), cosf(slope_angle), 0.0f}, 0.0f);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &floor_desc, &floor) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &desc, &controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_slope_limit(
        controller, 30.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_slope_limit(
        controller, NAN) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_set_slope_limit(
        controller, -1.0f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_set_slope_limit(
        controller, 91.0f) == HENKA_ERROR_INVALID_ARGUMENT);

    for (step = 0U; step < 180U; ++step)
    {
        HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_character_controller_sync_after_step(controller) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.grounded);
    HENKA_TEST_ASSERT(state.ground_normal.y > 0.9f);
    HENKA_TEST_ASSERT(state.ground_normal.x > 0.2f);
    HENKA_TEST_ASSERT(henka_character_controller_set_slope_limit(
        controller, 10.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!state.grounded);
    HENKA_TEST_ASSERT(state.ground_normal.y == 0.0f);
    HENKA_TEST_ASSERT(henka_character_controller_set_slope_limit(
        controller, 30.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.grounded);

    HENKA_TEST_ASSERT(henka_character_controller_destroy(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_destroy(world, floor) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);

    desc = henka_test_character_controller_desc();
    floor_desc.collider = henka_physics_collider_plane(
        (henka_vec3){sinf(50.0f * 3.14159265358979323846f / 180.0f),
            cosf(50.0f * 3.14159265358979323846f / 180.0f), 0.0f},
        0.0f);
    world = NULL;
    controller = NULL;
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &floor_desc, &floor) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &desc, &controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_slope_limit(
        controller, 30.0f) == HENKA_SUCCESS);
    for (step = 0U; step < 180U; ++step)
    {
        HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_character_controller_sync_after_step(controller) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!state.grounded);
    HENKA_TEST_ASSERT(state.ground_normal.y == 0.0f);
    HENKA_TEST_ASSERT(henka_character_controller_destroy(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_destroy(world, floor) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);
}

static void henka_test_character_controller_slides_along_wall(void)
{
    henka_physics_world* world = NULL;
    henka_character_controller* controller = NULL;
    henka_character_controller_desc controller_desc =
        henka_test_character_controller_desc();
    henka_physics_body_desc wall_desc = {0};
    henka_character_controller_state state;
    henka_physics_body_id wall;
    size_t step;

    controller_desc.transform.position = (henka_vec3){0.0f, 0.0f, 0.0f};
    wall_desc.type = HENKA_PHYSICS_BODY_STATIC;
    wall_desc.transform = henka_transform_identity();
    wall_desc.transform.position = (henka_vec3){2.0f, 0.0f, 0.0f};
    wall_desc.material = henka_physics_material_default();
    wall_desc.collider = henka_physics_collider_box(
        (henka_vec3){0.25f, 2.0f, 10.0f});

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world, &wall_desc, &wall) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &controller_desc, &controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_planar_velocity(
        controller, (henka_vec3){3.0f, 0.0f, 3.0f}) == HENKA_SUCCESS);

    for (step = 0U; step < 180U; ++step)
    {
        HENKA_TEST_ASSERT(henka_character_controller_prepare_step(
            controller) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_character_controller_sync_after_step(
            controller) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_character_controller_get_state(
        controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.transform.position.x < 1.4f);
    HENKA_TEST_ASSERT(state.transform.position.z > 2.0f);
    HENKA_TEST_ASSERT(state.velocity.x < 0.25f);
    HENKA_TEST_ASSERT(state.velocity.z > 1.8f);

    HENKA_TEST_ASSERT(henka_character_controller_destroy(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_destroy(world, wall) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);
}

static void henka_test_character_controller_traverses_walkable_slope(void)
{
    const float slope_angle = 20.0f * 3.14159265358979323846f / 180.0f;
    henka_physics_world* world = NULL;
    henka_character_controller* controller = NULL;
    henka_character_controller_desc desc = henka_test_character_controller_desc();
    henka_physics_body_desc floor_desc = {0};
    henka_character_controller_state state;
    henka_physics_body_id floor;
    henka_vec3 start_position;
    size_t step;

    desc.transform.position = (henka_vec3){-4.0f, 3.0f, 0.0f};
    floor_desc.type = HENKA_PHYSICS_BODY_STATIC;
    floor_desc.transform = henka_transform_identity();
    floor_desc.material = henka_physics_material_default();
    floor_desc.collider = henka_physics_collider_plane(
        (henka_vec3){sinf(slope_angle), cosf(slope_angle), 0.0f}, 0.0f);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world, &floor_desc, &floor) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &desc, &controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_set_slope_limit(
        controller, 30.0f) == HENKA_SUCCESS);
    for (step = 0U; step < 180U; ++step)
    {
        HENKA_TEST_ASSERT(henka_character_controller_prepare_step(
            controller) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_character_controller_sync_after_step(
            controller) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_character_controller_get_state(
        controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.grounded);
    start_position = state.transform.position;

    HENKA_TEST_ASSERT(henka_character_controller_set_planar_velocity(
        controller, (henka_vec3){-1.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    for (step = 0U; step < 60U; ++step)
    {
        HENKA_TEST_ASSERT(henka_character_controller_prepare_step(
            controller) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_character_controller_sync_after_step(
            controller) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_character_controller_get_state(
        controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.grounded);
    HENKA_TEST_ASSERT(state.transform.position.x < start_position.x - 0.5f);
    HENKA_TEST_ASSERT(state.transform.position.y > start_position.y + 0.1f);

    HENKA_TEST_ASSERT(henka_character_controller_destroy(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_destroy(world, floor) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);
}

static void henka_test_character_controller_follows_kinematic_platform(void)
{
    henka_physics_world* world = NULL;
    henka_character_controller* controller = NULL;
    henka_character_controller_desc controller_desc =
        henka_test_character_controller_desc();
    henka_physics_body_desc platform_desc = {0};
    henka_character_controller_state state;
    henka_physics_body_id platform;
    henka_vec3 start_position;
    size_t step;

    controller_desc.transform.position = (henka_vec3){0.0f, 2.5f, 0.0f};
    platform_desc.type = HENKA_PHYSICS_BODY_KINEMATIC;
    platform_desc.transform = henka_transform_identity();
    platform_desc.transform.position = (henka_vec3){0.0f, 0.25f, 0.0f};
    platform_desc.material = henka_physics_material_default();
    platform_desc.collider = henka_physics_collider_box(
        (henka_vec3){4.0f, 0.25f, 4.0f});

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world, &platform_desc, &platform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &controller_desc, &controller) == HENKA_SUCCESS);

    for (step = 0U; step < 240U; ++step)
    {
        HENKA_TEST_ASSERT(henka_character_controller_prepare_step(
            controller) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_character_controller_sync_after_step(
            controller) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_character_controller_get_state(
        controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.grounded);
    start_position = state.transform.position;

    HENKA_TEST_ASSERT(henka_physics_body_set_linear_velocity(
        world, platform, (henka_vec3){1.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    for (step = 0U; step < 60U; ++step)
    {
        HENKA_TEST_ASSERT(henka_character_controller_prepare_step(
            controller) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_character_controller_sync_after_step(
            controller) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_character_controller_get_state(
        controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.grounded);
    HENKA_TEST_ASSERT(state.transform.position.x > start_position.x + 0.25f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.transform.position.y, start_position.y, 0.0001f);

    HENKA_TEST_ASSERT(henka_physics_body_destroy(world, platform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(
        controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_sync_after_step(
        controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(
        controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!state.grounded);

    HENKA_TEST_ASSERT(henka_character_controller_destroy(controller) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);
}

static void henka_test_character_controller_syncs_linked_scene_entity(void)
{
    henka_scene* scene = NULL;
    henka_physics_world* world = NULL;
    henka_character_controller* controller = NULL;
    henka_character_controller_desc controller_desc =
        henka_test_character_controller_desc();
    henka_physics_body_desc floor_desc = {0};
    henka_character_controller_state state;
    henka_physics_body_id floor;
    henka_entity entity;
    henka_entity replacement;
    henka_transform scene_transform;
    henka_transform replacement_transform;
    henka_transform destination = henka_transform_identity();
    size_t step;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Character Controller");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    controller_desc.linked_scene = scene;
    controller_desc.linked_entity = entity;
    floor_desc.type = HENKA_PHYSICS_BODY_STATIC;
    floor_desc.transform = henka_transform_identity();
    floor_desc.material = henka_physics_material_default();
    floor_desc.collider = henka_physics_collider_plane(
        (henka_vec3){0.0f, 1.0f, 0.0f}, 0.0f);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world, &floor_desc, &floor) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &controller_desc, &controller) == HENKA_SUCCESS);

    for (step = 0U; step < 180U; ++step)
    {
        HENKA_TEST_ASSERT(henka_character_controller_prepare_step(
            controller) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_character_controller_sync_after_step(
            controller) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_character_controller_get_state(
        controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(
        scene, entity, &scene_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.position.x, state.transform.position.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.position.y, state.transform.position.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.position.z, state.transform.position.z, 0.0001f);

    destination.position = (henka_vec3){3.0f, 4.0f, -2.0f};
    HENKA_TEST_ASSERT(henka_character_controller_teleport(
        controller, destination, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(
        scene, entity, &scene_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.position.x, destination.position.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.position.y, destination.position.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.position.z, destination.position.z, 0.0001f);

    henka_scene_destroy_entity(scene, entity);
    replacement = henka_scene_create_entity_named(scene, "Replacement Character");
    HENKA_TEST_ASSERT(replacement != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(replacement != entity);
    replacement_transform = henka_transform_identity();
    replacement_transform.position = (henka_vec3){9.0f, 8.0f, 7.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
        scene, replacement, replacement_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(
        controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_sync_after_step(
        controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(
        scene, replacement, &scene_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.position.x, replacement_transform.position.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.position.y, replacement_transform.position.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.position.z, replacement_transform.position.z, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.rotation.x, replacement_transform.rotation.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.rotation.y, replacement_transform.rotation.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.rotation.z, replacement_transform.rotation.z, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.rotation.w, replacement_transform.rotation.w, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.scale.x, replacement_transform.scale.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.scale.y, replacement_transform.scale.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene_transform.scale.z, replacement_transform.scale.z, 0.0001f);

    HENKA_TEST_ASSERT(henka_character_controller_destroy(controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_destroy(world, floor) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);
    henka_scene_destroy(scene);
}

static void henka_test_character_controller_failure_boundaries(void)
{
    henka_physics_world* world = NULL;
    henka_character_controller* controller = NULL;
    henka_character_controller_desc desc = henka_test_character_controller_desc();
    henka_character_controller_state state;
    henka_character_controller_state before;

    HENKA_TEST_ASSERT(henka_character_controller_create(
        NULL, &desc, &controller) == HENKA_ERROR_INVALID_ARGUMENT);
    desc.radius = 0.0f;
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &desc, &controller) == HENKA_ERROR_INVALID_ARGUMENT);
    desc = henka_test_character_controller_desc();
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &desc, &controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_destroy(world, before.body) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_prepare_step(controller) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_set_slope_limit(
        controller, 30.0f) == HENKA_ERROR_INVALID_ARGUMENT);
    memset(&state, 0xA5, sizeof(state));
    HENKA_TEST_ASSERT(henka_character_controller_get_state(controller, &state) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(state.body == HENKA_INVALID_PHYSICS_BODY_ID);
    HENKA_TEST_ASSERT(henka_character_controller_set_movement_tuning(
        controller, 6.0f, 12.0f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_teleport(
        controller, henka_transform_identity(), true) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_set_planar_velocity(
        controller, (henka_vec3){1.0f, 0.0f, 0.0f}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_queue_jump(controller) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_character_controller_destroy(controller) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);
}

void henka_test_character_controller(void)
{
    henka_test_character_controller_create_and_validate();
    henka_test_character_controller_moves_and_jumps();
    henka_test_character_controller_movement_tuning();
    henka_test_character_controller_air_control();
    henka_test_character_controller_teleport();
    henka_test_character_controller_slope_grounding();
    henka_test_character_controller_slides_along_wall();
    henka_test_character_controller_traverses_walkable_slope();
    henka_test_character_controller_follows_kinematic_platform();
    henka_test_character_controller_syncs_linked_scene_entity();
    henka_test_character_controller_failure_boundaries();
}
