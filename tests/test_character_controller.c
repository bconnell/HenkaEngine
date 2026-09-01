#include <math.h>
#include <string.h>

#include <henka/character_controller.h>
#include <henka/physics.h>

#include "test_suite.h"

static henka_character_controller_desc henka_test_character_controller_desc(void)
{
    henka_character_controller_desc desc = {0};
    desc.transform = henka_transform_identity();
    desc.transform.position = (henka_vec3){0.0f, 2.0f, 0.0f};
    desc.radius = 0.5f;
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

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_character_controller_create(
        world, &desc, &controller) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(controller != NULL);
    HENKA_TEST_ASSERT(henka_character_controller_get_state(
        controller, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.body != HENKA_INVALID_PHYSICS_BODY_ID);
    HENKA_TEST_ASSERT(state.transform.position.y == 2.0f);
    HENKA_TEST_ASSERT(!state.grounded);
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
    HENKA_TEST_ASSERT(state.transform.position.y > 0.45f && state.transform.position.y < 0.6f);
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
    henka_test_character_controller_teleport();
    henka_test_character_controller_failure_boundaries();
}
