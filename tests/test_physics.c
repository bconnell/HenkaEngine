#include "test_suite.h"

#include <float.h>
#include <math.h>
#include <string.h>

#include <henka/core.h>
#include <henka/memory.h>
#include <henka/physics.h>

#include "../engine/src/core/memory_internal.h"
#include "../engine/src/core/physics_internal.h"

typedef struct henka_test_physics_snapshot
{
    henka_physics_body_state body_states[2];
    henka_physics_contact contacts[4];
    henka_physics_event events[8];
    henka_transform scene_transform;
    size_t contact_count;
    size_t current_pair_count;
    size_t event_count;
    size_t previous_pair_count;
    float accumulator;
} henka_test_physics_snapshot;

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

static bool henka_test_physics_float_is_finite(float value)
{
    return value == value && value >= -FLT_MAX && value <= FLT_MAX;
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

static size_t henka_test_count_pair_events(
    const henka_physics_world* world,
    henka_physics_event_type type,
    henka_physics_body_id first,
    henka_physics_body_id second)
{
    size_t index;
    size_t count;
    size_t matching_count;
    const henka_physics_event* events;

    events = henka_physics_world_get_events(world, &count);
    matching_count = 0U;
    for (index = 0U; index < count; ++index)
    {
        if (events[index].type == type &&
            ((events[index].contact.body_a == first &&
                events[index].contact.body_b == second) ||
                (events[index].contact.body_a == second &&
                    events[index].contact.body_b == first)))
        {
            ++matching_count;
        }
    }
    return matching_count;
}

static bool henka_test_capture_physics_snapshot(
    const henka_physics_world* world,
    henka_physics_body_id first,
    henka_physics_body_id second,
    const henka_scene* scene,
    henka_entity entity,
    henka_test_physics_snapshot* out_snapshot)
{
    const henka_physics_contact* contacts;
    const henka_physics_event* events;

    if (world == NULL || out_snapshot == NULL)
    {
        return false;
    }

    memset(out_snapshot, 0, sizeof(*out_snapshot));
    if (henka_physics_body_get_state(
            world,
            first,
            &out_snapshot->body_states[0]) != HENKA_SUCCESS ||
        henka_physics_body_get_state(
            world,
            second,
            &out_snapshot->body_states[1]) != HENKA_SUCCESS)
    {
        return false;
    }

    contacts = henka_physics_world_get_contacts(
        world,
        &out_snapshot->contact_count);
    events = henka_physics_world_get_events(
        world,
        &out_snapshot->event_count);
    if (out_snapshot->contact_count > 4U ||
        out_snapshot->event_count > 8U ||
        (out_snapshot->contact_count > 0U && contacts == NULL) ||
        (out_snapshot->event_count > 0U && events == NULL))
    {
        return false;
    }
    if (out_snapshot->contact_count > 0U)
    {
        memcpy(
            out_snapshot->contacts,
            contacts,
            sizeof(*contacts) * out_snapshot->contact_count);
    }
    if (out_snapshot->event_count > 0U)
    {
        memcpy(
            out_snapshot->events,
            events,
            sizeof(*events) * out_snapshot->event_count);
    }
    out_snapshot->current_pair_count =
        henka_physics_test_get_current_pair_count(world);
    out_snapshot->previous_pair_count =
        henka_physics_test_get_previous_pair_count(world);
    out_snapshot->accumulator =
        henka_physics_test_get_accumulator(world);
    if (scene != NULL &&
        henka_scene_get_entity_transform(
            scene,
            entity,
            &out_snapshot->scene_transform) != HENKA_SUCCESS)
    {
        return false;
    }
    return true;
}

static bool henka_test_physics_snapshots_equal(
    const henka_test_physics_snapshot* first,
    const henka_test_physics_snapshot* second)
{
    if (first == NULL || second == NULL ||
        first->contact_count != second->contact_count ||
        first->current_pair_count != second->current_pair_count ||
        first->event_count != second->event_count ||
        first->previous_pair_count != second->previous_pair_count ||
        first->accumulator != second->accumulator ||
        memcmp(
            first->body_states,
            second->body_states,
            sizeof(first->body_states)) != 0 ||
        memcmp(
            first->contacts,
            second->contacts,
            sizeof(first->contacts[0]) * first->contact_count) != 0 ||
        memcmp(
            first->events,
            second->events,
            sizeof(first->events[0]) * first->event_count) != 0 ||
        memcmp(
            &first->scene_transform,
            &second->scene_transform,
            sizeof(first->scene_transform)) != 0)
    {
        return false;
    }
    return true;
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

static void henka_test_physics_capsule_contacts_and_raycast(void)
{
    henka_physics_world* world;
    henka_physics_body_desc desc;
    henka_physics_body_id capsule;
    henka_physics_body_id floor;
    henka_physics_body_state state;
    henka_physics_raycast_hit hit;
    size_t count;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_DYNAMIC,
        henka_physics_collider_capsule(0.5f, 0.75f),
        (henka_vec3){0.0f, 1.2f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &capsule) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, capsule, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.collider.shape == HENKA_PHYSICS_SHAPE_CAPSULE);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.collider.data.capsule.radius, 0.5f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.collider.data.capsule.half_height, 0.75f, 0.0001f);
    HENKA_TEST_ASSERT(henka_physics_world_raycast(
        world,
        (henka_ray){{0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        10.0f,
        HENKA_PHYSICS_ALL_LAYERS,
        &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(hit.hit && hit.body == capsule);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.distance, 0.55f, 0.0001f);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_STATIC,
        henka_physics_collider_plane((henka_vec3){0.0f, 1.0f, 0.0f}, 0.0f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &floor) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_get_contacts(world, &count) != NULL && count == 1U);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, capsule, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.grounded && state.transform.position.y > 1.2f);
    henka_physics_world_destroy(world);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_DYNAMIC,
        henka_physics_collider_capsule(0.5f, 0.5f),
        (henka_vec3){0.75f, 0.6f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &capsule) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_STATIC,
        henka_physics_collider_box((henka_vec3){0.5f, 0.5f, 0.5f}),
        (henka_vec3){1.0f, 0.5f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &floor) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_get_contacts(world, &count) != NULL && count == 1U);
    henka_physics_world_destroy(world);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_STATIC,
        henka_physics_collider_capsule(0.5f, 0.5f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &capsule) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_DYNAMIC,
        henka_physics_collider_capsule(0.5f, 0.5f),
        (henka_vec3){0.75f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &floor) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_get_contacts(world, &count) != NULL && count == 1U);
    henka_physics_world_destroy(world);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_STATIC,
        henka_physics_collider_capsule(0.5f, 0.75f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.collider.data.capsule.half_height = -1.0f;
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world, &desc, &capsule) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(capsule == HENKA_INVALID_PHYSICS_BODY_ID);
    desc.collider.data.capsule.half_height = 0.75f;
    desc.transform.rotation = henka_quat_from_axis_angle(
        (henka_vec3){1.0f, 0.0f, 0.0f}, 45.0f * HENKA_DEG_TO_RAD);
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world, &desc, &capsule) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(capsule == HENKA_INVALID_PHYSICS_BODY_ID);
    desc.transform.rotation = henka_quat_from_axis_angle(
        (henka_vec3){0.0f, 1.0f, 0.0f}, 90.0f * HENKA_DEG_TO_RAD);
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world, &desc, &capsule) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_destroy(world, capsule) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);
}

static void henka_test_physics_capsule_box_separation(void)
{
    henka_physics_world* world = NULL;
    henka_physics_body_desc desc;
    henka_physics_body_id capsule;
    henka_physics_body_id wall;
    henka_physics_body_state state;
    const henka_physics_contact* contacts;
    size_t contact_count;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_DYNAMIC,
        henka_physics_collider_capsule(0.5f, 0.75f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &capsule) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_STATIC,
        henka_physics_collider_box((henka_vec3){0.25f, 2.0f, 10.0f}),
        (henka_vec3){2.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &wall) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    contacts = henka_physics_world_get_contacts(world, &contact_count);
    HENKA_TEST_ASSERT(contacts == NULL && contact_count == 0U);

    state = (henka_physics_body_state){0};
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, capsule, &state) == HENKA_SUCCESS);
    state.transform.position.x = 1.5f;
    HENKA_TEST_ASSERT(henka_physics_body_set_transform(
        world, capsule, state.transform, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    contacts = henka_physics_world_get_contacts(world, &contact_count);
    HENKA_TEST_ASSERT(contacts != NULL && contact_count == 1U);
    HENKA_TEST_ASSERT(contacts[0].body_a == capsule);
    HENKA_TEST_ASSERT(contacts[0].body_b == wall);
    HENKA_TEST_ASSERT(contacts[0].normal.x > 0.9f);

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
    henka_entity replacement;
    henka_physics_world* world;
    henka_physics_body_desc desc;
    henka_physics_body_id body;
    henka_transform replacement_transform;
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

    henka_scene_destroy_entity(scene, entity);
    replacement = henka_scene_create_entity_named(
        scene,
        "Replacement Physics Body");
    HENKA_TEST_ASSERT(replacement != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(replacement != entity);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, entity));

    replacement_transform = henka_transform_identity();
    replacement_transform.position =
        (henka_vec3){12.0f, 34.0f, 56.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
        scene,
        replacement,
        replacement_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(
        scene,
        replacement,
        &replacement_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        replacement_transform.position.x,
        12.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        replacement_transform.position.y,
        34.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        replacement_transform.position.z,
        56.0f,
        0.0001f);

    henka_physics_world_destroy(world);
    henka_scene_destroy(scene);
}

static void henka_test_physics_validation_and_tracking(void)
{
    size_t allocations_before;
    size_t contact_count;
    size_t event_count;
    henka_physics_body_desc desc;
    henka_physics_body_id first;
    henka_physics_body_id second;
    henka_physics_body_state state;
    henka_physics_world* world;
    henka_transform invalid_transform;

    allocations_before = henka_memory_get_allocation_count();
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);

    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_DYNAMIC,
        henka_physics_collider_sphere(1.0f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.transform.scale.x = 0.0f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &first) == HENKA_ERROR_INVALID_ARGUMENT);

    desc.transform = henka_transform_identity();
    desc.collider.is_trigger = true;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &first) == HENKA_SUCCESS);
    desc.transform.position.x = 0.5f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &second) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_get_contacts(world, &contact_count) != NULL && contact_count == 1U);
    HENKA_TEST_ASSERT(henka_physics_world_get_events(world, &event_count) != NULL && event_count == 1U);

    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, first, &state) == HENKA_SUCCESS);
    invalid_transform = state.transform;
    invalid_transform.scale.y = 0.001f;
    HENKA_TEST_ASSERT(henka_physics_body_set_transform(world, first, invalid_transform, true) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, first, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.transform.scale.y, 1.0f, 0.0001f);

    HENKA_TEST_ASSERT(henka_physics_body_destroy(world, second) == HENKA_SUCCESS);
    (void)henka_physics_world_get_contacts(world, &contact_count);
    (void)henka_physics_world_get_events(world, &event_count);
    HENKA_TEST_ASSERT(contact_count == 0U);
    HENKA_TEST_ASSERT(event_count == 2U);
    HENKA_TEST_ASSERT(henka_test_count_pair_events(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_EXIT,
        first,
        second) == 1U);

    henka_physics_world_destroy(world);
    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == allocations_before);
}

static void henka_test_physics_destroy_preserves_contact_continuity(void)
{
    henka_physics_body_desc desc;
    henka_physics_body_id first;
    henka_physics_body_id second;
    henka_physics_body_id destroyed;
    henka_physics_body_id fourth;
    henka_physics_body_id replacement;
    henka_physics_body_state state;
    henka_physics_world* world;
    size_t contact_count;
    size_t event_count;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world,
        (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);

    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_DYNAMIC,
        henka_physics_collider_sphere(1.0f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.collider.is_trigger = true;
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world,
        &desc,
        &first) == HENKA_SUCCESS);
    desc.transform.position.x = 0.5f;
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world,
        &desc,
        &second) == HENKA_SUCCESS);
    desc.transform.position.x = 10.0f;
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world,
        &desc,
        &destroyed) == HENKA_SUCCESS);
    desc.transform.position.x = 10.5f;
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world,
        &desc,
        &fourth) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_count_pair_events(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_ENTER,
        first,
        second) == 1U);
    HENKA_TEST_ASSERT(henka_test_count_pair_events(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_ENTER,
        destroyed,
        fourth) == 1U);

    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_count_pair_events(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_STAY,
        first,
        second) == 1U);
    HENKA_TEST_ASSERT(henka_test_count_pair_events(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_STAY,
        destroyed,
        fourth) == 1U);

    HENKA_TEST_ASSERT(henka_physics_body_destroy(
        world,
        destroyed) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_get_contacts(
        world,
        &contact_count) != NULL);
    HENKA_TEST_ASSERT(contact_count == 1U);
    HENKA_TEST_ASSERT(henka_physics_world_get_events(
        world,
        &event_count) != NULL);
    HENKA_TEST_ASSERT(event_count == 3U);
    HENKA_TEST_ASSERT(henka_test_count_pair_events(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_STAY,
        first,
        second) == 1U);
    HENKA_TEST_ASSERT(henka_test_count_pair_events(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_STAY,
        destroyed,
        fourth) == 1U);
    HENKA_TEST_ASSERT(henka_test_count_pair_events(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_EXIT,
        destroyed,
        fourth) == 1U);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(
        world,
        first,
        &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.colliding);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(
        world,
        second,
        &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.colliding);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(
        world,
        fourth,
        &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!state.colliding && !state.grounded);

    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_get_events(
        world,
        &event_count) != NULL);
    HENKA_TEST_ASSERT(event_count == 1U);
    HENKA_TEST_ASSERT(henka_test_count_pair_events(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_STAY,
        first,
        second) == 1U);
    HENKA_TEST_ASSERT(!henka_test_has_event(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_ENTER));
    HENKA_TEST_ASSERT(!henka_test_has_event(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_EXIT));

    desc.transform.position.x = 20.0f;
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world,
        &desc,
        &replacement) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(replacement != destroyed);
    HENKA_TEST_ASSERT(henka_physics_body_destroy(
        world,
        destroyed) == HENKA_ERROR_INVALID_ARGUMENT);

    henka_physics_world_destroy(world);
}

static void henka_test_physics_transactional_allocation_failure(void)
{
    static const size_t failure_points[] = {0U, 2U, 3U, 4U};
    size_t allocation_count;
    henka_test_physics_snapshot after_failure;
    henka_test_physics_snapshot before_failure;
    henka_physics_body_desc desc;
    henka_physics_body_id first;
    size_t failure_index;
    henka_physics_body_id second;
    henka_physics_body_state state_after;
    henka_physics_body_state state_before;
    henka_entity entity;
    henka_result result;
    henka_scene* scene;
    henka_physics_world* world;

    allocation_count = henka_memory_get_allocation_count();
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(
        scene,
        "Transactional Physics Body");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world,
        (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);

    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_KINEMATIC,
        henka_physics_collider_sphere(1.0f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.collider.is_trigger = true;
    desc.linear_velocity.x = 1.0f;
    desc.linked_scene = scene;
    desc.linked_entity = entity;
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world,
        &desc,
        &first) == HENKA_SUCCESS);
    desc.transform.position.x = 0.5f;
    desc.linear_velocity.x = 0.0f;
    desc.linked_scene = NULL;
    desc.linked_entity = HENKA_INVALID_ENTITY;
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world,
        &desc,
        &second) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_capture_physics_snapshot(
        world,
        first,
        second,
        scene,
        entity,
        &before_failure));
    HENKA_TEST_ASSERT(before_failure.contact_count == 1U);
    HENKA_TEST_ASSERT(before_failure.current_pair_count == 1U);
    HENKA_TEST_ASSERT(before_failure.previous_pair_count == 1U);
    HENKA_TEST_ASSERT(before_failure.event_count == 1U);

    for (failure_index = 0U;
        failure_index < sizeof(failure_points) / sizeof(failure_points[0]);
        ++failure_index)
    {
        size_t allocations_before_failure;

        allocations_before_failure = henka_memory_get_allocation_count();
        henka_memory_test_fail_after(failure_points[failure_index]);
        result = henka_physics_world_step_fixed(world);
        henka_memory_test_disable_failures();
        HENKA_TEST_ASSERT(result == HENKA_ERROR_OUT_OF_MEMORY);
        HENKA_TEST_ASSERT(henka_test_capture_physics_snapshot(
            world,
            first,
            second,
            scene,
            entity,
            &after_failure));
        HENKA_TEST_ASSERT(henka_test_physics_snapshots_equal(
            &before_failure,
            &after_failure));
        HENKA_TEST_ASSERT(
            henka_memory_get_allocation_count() ==
            allocations_before_failure);
    }

    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_has_event(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_STAY));
    HENKA_TEST_ASSERT(henka_physics_body_get_state(
        world,
        first,
        &state_after) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        state_after.transform.position.x >
        before_failure.body_states[0].transform.position.x);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(
        scene,
        entity,
        &after_failure.scene_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        after_failure.scene_transform.position.x,
        state_after.transform.position.x,
        0.000001f);

    state_before = state_after;
    henka_memory_test_fail_after(5U);
    result = henka_physics_world_step(
        world,
        henka_physics_world_get_fixed_timestep(world) * 2.0f);
    henka_memory_test_disable_failures();
    HENKA_TEST_ASSERT(result == HENKA_ERROR_OUT_OF_MEMORY);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(
        world,
        first,
        &state_after) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state_after.transform.position.x >
        state_before.transform.position.x);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state_after.transform.position.x,
        state_before.transform.position.x +
            state_before.linear_velocity.x *
                henka_physics_world_get_fixed_timestep(world),
        0.000001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        henka_physics_test_get_accumulator(world),
        henka_physics_world_get_fixed_timestep(world),
        0.000001f);
    HENKA_TEST_ASSERT(henka_test_has_event(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_STAY));

    state_before = state_after;
    HENKA_TEST_ASSERT(henka_physics_world_step(
        world,
        0.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(
        world,
        first,
        &state_after) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state_after.transform.position.x >
        state_before.transform.position.x);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        henka_physics_test_get_accumulator(world),
        0.0f,
        0.000001f);

    henka_physics_world_destroy(world);
    henka_scene_destroy(scene);
    HENKA_TEST_ASSERT(
        henka_memory_get_allocation_count() == allocation_count);
}

static void henka_test_physics_capacity_growth(void)
{
    enum
    {
        BODY_COUNT = 20
    };
    henka_physics_body_id body;
    henka_physics_body_desc desc;
    const henka_physics_event* events;
    henka_physics_world* world;
    size_t contact_count;
    size_t event_count;
    int index;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(world, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);

    for (index = 0; index < BODY_COUNT; ++index)
    {
        desc = henka_test_physics_body(
            HENKA_PHYSICS_BODY_DYNAMIC,
            henka_physics_collider_sphere(1.0f),
            (henka_vec3){0.0f, 0.0f, 0.0f});
        desc.collider.is_trigger = true;
        HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &body) == HENKA_SUCCESS);
    }

    HENKA_TEST_ASSERT(henka_physics_world_get_body_count(world) == (size_t)BODY_COUNT);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_get_contacts(world, &contact_count) != NULL);
    HENKA_TEST_ASSERT(contact_count == ((size_t)BODY_COUNT * (BODY_COUNT - 1U)) / 2U);

    events = henka_physics_world_get_events(world, &event_count);
    HENKA_TEST_ASSERT(events != NULL);
    HENKA_TEST_ASSERT(event_count == contact_count);

    henka_physics_world_destroy(world);
}

static void henka_test_physics_numeric_failures(void)
{
    size_t allocation_count;
    henka_physics_body_desc desc;
    henka_physics_body_desc second_desc;
    henka_physics_body_id body;
    henka_physics_body_id first;
    henka_physics_body_id second;
    henka_physics_body_id hazard;
    henka_physics_body_state before;
    henka_physics_body_state after;
    henka_physics_body_state first_before;
    henka_physics_body_state second_before;
    henka_physics_body_state first_after;
    henka_physics_body_state second_after;
    henka_physics_event saved_event;
    const henka_physics_event* events;
    size_t event_count;
    size_t current_pair_count;
    size_t previous_pair_count;
    henka_scene* scene;
    henka_entity entity;
    henka_transform scene_before;
    henka_transform scene_after;
    henka_physics_world* world;
    henka_result result;
    int recovery_index;

    allocation_count = henka_memory_get_allocation_count();

    /* A representable near-limit kinematic move remains supported. */
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_fixed_timestep(world, 0.5f) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_KINEMATIC,
        henka_physics_collider_sphere(0.5f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.linear_velocity.x = FLT_MAX * 0.25f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &body) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, body, &after) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_physics_float_is_finite(after.transform.position.x));
    HENKA_TEST_ASSERT(after.transform.position.x > 0.0f);
    henka_physics_world_destroy(world);

    /* Position overflow rolls back the body, scene link, and accumulator. */
    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Numeric Physics Body");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_fixed_timestep(world, 0.25f) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_KINEMATIC,
        henka_physics_collider_sphere(0.5f),
        (henka_vec3){FLT_MAX * 0.8f, 20.0f, 0.0f});
    desc.linear_velocity.x = FLT_MAX;
    desc.linked_scene = scene;
    desc.linked_entity = entity;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &body) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, body, &before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, entity, &scene_before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_physics_test_get_accumulator(world), 0.0f, 0.0f);
    result = henka_physics_world_step(world, 0.25f);
    HENKA_TEST_ASSERT(result == HENKA_ERROR_NUMERIC_RANGE);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, body, &after) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(memcmp(&before, &after, sizeof(before)) == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, entity, &scene_after) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(memcmp(&scene_before, &scene_after, sizeof(scene_before)) == 0);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_physics_test_get_accumulator(world), 0.0f, 0.0f);
    HENKA_TEST_ASSERT(henka_physics_body_set_linear_velocity(
        world,
        body,
        (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    henka_physics_world_destroy(world);
    henka_scene_destroy(scene);

    /* Gravity can overflow an otherwise finite velocity update. */
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_fixed_timestep(world, 1.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world,
        (henka_vec3){FLT_MAX, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_DYNAMIC,
        henka_physics_collider_sphere(0.5f),
        (henka_vec3){0.0f, 30.0f, 0.0f});
    desc.linear_velocity.x = FLT_MAX;
    desc.material.linear_damping = 0.0f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &body) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, body, &before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_ERROR_NUMERIC_RANGE);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, body, &after) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(memcmp(&before, &after, sizeof(before)) == 0);
    henka_physics_world_destroy(world);

    /* A finite angular vector whose magnitude exceeds float range is rejected. */
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_fixed_timestep(world, 1.0f) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_KINEMATIC,
        henka_physics_collider_sphere(0.5f),
        (henka_vec3){0.0f, 40.0f, 0.0f});
    desc.angular_velocity = (henka_vec3){FLT_MAX, FLT_MAX, 0.0f};
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &body) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, body, &before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_ERROR_NUMERIC_RANGE);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, body, &after) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(memcmp(&before, &after, sizeof(before)) == 0);
    henka_physics_world_destroy(world);

    /* Opposing near-limit velocities overflow response-relative velocity. */
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world,
        (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_fixed_timestep(
        world,
        nextafterf(0.0f, 1.0f)) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_DYNAMIC,
        henka_physics_collider_sphere(10.0f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.material.linear_damping = 0.0f;
    desc.linear_velocity.x = FLT_MAX;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &first) == HENKA_SUCCESS);
    desc.linear_velocity.x = -FLT_MAX;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &second) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, first, &first_before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, second, &second_before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_ERROR_NUMERIC_RANGE);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, first, &first_after) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, second, &second_after) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(memcmp(&first_before, &first_after, sizeof(first_before)) == 0);
    HENKA_TEST_ASSERT(memcmp(&second_before, &second_after, sizeof(second_before)) == 0);
    henka_physics_world_destroy(world);

    /* A finite correction that would make collider bounds unrepresentable rolls back. */
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world,
        (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_DYNAMIC,
        henka_physics_collider_sphere(FLT_MAX * 0.1f),
        (henka_vec3){FLT_MAX * 0.8f, 0.0f, 0.0f});
    desc.material.linear_damping = 0.0f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &first) == HENKA_SUCCESS);
    second_desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_STATIC,
        henka_physics_collider_sphere(FLT_MAX * 0.1f),
        (henka_vec3){FLT_MAX * 0.75f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &second_desc, &second) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, first, &first_before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, second, &second_before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_ERROR_NUMERIC_RANGE);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, first, &first_after) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, second, &second_after) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(memcmp(&first_before, &first_after, sizeof(first_before)) == 0);
    HENKA_TEST_ASSERT(memcmp(&second_before, &second_after, sizeof(second_before)) == 0);
    henka_physics_world_destroy(world);

    /* Numeric failure elsewhere preserves an unrelated STAY pair and recovers repeatedly. */
    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world,
        (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_fixed_timestep(world, 1.0f) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_KINEMATIC,
        henka_physics_collider_sphere(1.0f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.collider.is_trigger = true;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &first) == HENKA_SUCCESS);
    second_desc = desc;
    second_desc.type = HENKA_PHYSICS_BODY_STATIC;
    second_desc.transform.position.x = 0.5f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &second_desc, &second) == HENKA_SUCCESS);
    desc.collider.is_trigger = false;
    desc.transform.position = (henka_vec3){FLT_MAX * 0.5f, 50.0f, 0.0f};
    desc.linear_velocity.x = FLT_MAX;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &hazard) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_set_linear_velocity(
        world,
        hazard,
        (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_test_count_pair_events(
        world,
        HENKA_PHYSICS_EVENT_TRIGGER_STAY,
        first,
        second) == 1U);
    events = henka_physics_world_get_events(world, &event_count);
    HENKA_TEST_ASSERT(events != NULL && event_count == 1U);
    saved_event = events[0];
    current_pair_count = henka_physics_test_get_current_pair_count(world);
    previous_pair_count = henka_physics_test_get_previous_pair_count(world);
    for (recovery_index = 0; recovery_index < 3; ++recovery_index)
    {
        HENKA_TEST_ASSERT(henka_physics_body_set_linear_velocity(
            world,
            hazard,
            (henka_vec3){FLT_MAX, 0.0f, 0.0f}) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_ERROR_NUMERIC_RANGE);
        events = henka_physics_world_get_events(world, &event_count);
        HENKA_TEST_ASSERT(events != NULL && event_count == 1U);
        HENKA_TEST_ASSERT(memcmp(&saved_event, &events[0], sizeof(saved_event)) == 0);
        HENKA_TEST_ASSERT(henka_physics_test_get_current_pair_count(world) == current_pair_count);
        HENKA_TEST_ASSERT(henka_physics_test_get_previous_pair_count(world) == previous_pair_count);
        HENKA_TEST_ASSERT(henka_physics_body_set_linear_velocity(
            world,
            hazard,
            (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_physics_world_step_fixed(world) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_test_count_pair_events(
            world,
            HENKA_PHYSICS_EVENT_TRIGGER_STAY,
            first,
            second) == 1U);
    }
    henka_memory_test_fail_after(0U);
    result = henka_physics_world_step_fixed(world);
    henka_memory_test_disable_failures();
    HENKA_TEST_ASSERT(result == HENKA_ERROR_OUT_OF_MEMORY);
    henka_physics_world_destroy(world);

    HENKA_TEST_ASSERT(henka_memory_get_allocation_count() == allocation_count);
}

static void henka_test_physics_query_and_accumulator_hardening(void)
{
    henka_physics_world* world;
    henka_physics_body_desc desc;
    henka_physics_body_id body;
    henka_physics_body_id box;
    henka_physics_body_id plane;
    henka_physics_body_state state;
    henka_physics_debug_shape debug_shape;
    henka_physics_raycast_hit hit;
    float position_after_capped_step;

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world,
        (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);

    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_STATIC,
        henka_physics_collider_box((henka_vec3){1.0f, 1.0f, 1.0f}),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &box) == HENKA_SUCCESS);

    hit = (henka_physics_raycast_hit){
        true,
        box,
        {3.0f, 3.0f, 3.0f},
        {4.0f, 4.0f, 4.0f},
        5.0f};
    HENKA_TEST_ASSERT(henka_physics_world_raycast(
        world,
        (henka_ray){{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
        10.0f,
        HENKA_PHYSICS_ALL_LAYERS,
        &hit) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(!hit.hit);
    HENKA_TEST_ASSERT(hit.body == HENKA_INVALID_PHYSICS_BODY_ID);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.distance, 0.0f, 0.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.normal.x, 0.0f, 0.0f);

    HENKA_TEST_ASSERT(henka_physics_world_raycast(
        world,
        (henka_ray){{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        10.0f,
        HENKA_PHYSICS_ALL_LAYERS,
        &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(hit.hit && hit.body == box);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.distance, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.normal.x, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.normal.y, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.normal.z, 0.0f, 0.0001f);

    HENKA_TEST_ASSERT(henka_physics_world_raycast(
        world,
        (henka_ray){{0.0f, 0.0f, 0.0f}, {FLT_MAX, FLT_MAX, 0.0f}},
        10.0f,
        HENKA_PHYSICS_ALL_LAYERS,
        &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(hit.hit && hit.body == box);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.distance, 1.4142135f, 0.0002f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.normal.x, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.normal.y, 0.0f, 0.0001f);

    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_STATIC,
        henka_physics_collider_plane((henka_vec3){0.0f, 1.0f, 0.0f}, 0.0f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.collider.offset.y = 2.0f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &plane) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_raycast(
        world,
        (henka_ray){{3.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        10.0f,
        HENKA_PHYSICS_ALL_LAYERS,
        &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(hit.hit && hit.body == plane);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.distance, 3.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.point.y, 2.0f, 0.0001f);

    HENKA_TEST_ASSERT(henka_physics_body_destroy(world, plane) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_STATIC,
        henka_physics_collider_plane((henka_vec3){0.0f, 1.0f, 0.0f}, 0.0f),
        (henka_vec3){4.0f, 4.0f, 0.0f});
    desc.transform.rotation = henka_quat_from_axis_angle(
        (henka_vec3){0.0f, 0.0f, 1.0f},
        -90.0f * HENKA_DEG_TO_RAD);
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &plane) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_raycast(
        world,
        (henka_ray){{8.0f, 4.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        10.0f,
        HENKA_PHYSICS_ALL_LAYERS,
        &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(hit.hit && hit.body == plane);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.distance, 4.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.point.x, 4.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.normal.x, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(hit.normal.y, 0.0f, 0.0001f);

    debug_shape.body = box;
    debug_shape.colliding = true;
    debug_shape.grounded = true;
    HENKA_TEST_ASSERT(henka_physics_world_get_debug_shape(
        world,
        99U,
        &debug_shape) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(debug_shape.body == HENKA_INVALID_PHYSICS_BODY_ID);
    HENKA_TEST_ASSERT(!debug_shape.colliding && !debug_shape.grounded);

    state.id = box;
    state.linked_scene = (henka_scene*)world;
    state.linked_entity = 99U;
    HENKA_TEST_ASSERT(henka_physics_body_get_state(
        world,
        HENKA_INVALID_PHYSICS_BODY_ID,
        &state) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(state.id == HENKA_INVALID_PHYSICS_BODY_ID);
    HENKA_TEST_ASSERT(state.linked_scene == NULL);
    HENKA_TEST_ASSERT(state.linked_entity == HENKA_INVALID_ENTITY);

    body = 77U;
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_DYNAMIC,
        henka_physics_collider_sphere(0.5f),
        (henka_vec3){6.0f, 0.0f, 0.0f});
    desc.mass = nextafterf(0.0f, 1.0f);
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world,
        &desc,
        &body) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(body == HENKA_INVALID_PHYSICS_BODY_ID);

    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_STATIC,
        henka_physics_collider_box((henka_vec3){2.0f, 1.0f, 1.0f}),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    desc.transform.scale.x = FLT_MAX;
    HENKA_TEST_ASSERT(henka_physics_body_create(
        world,
        &desc,
        &body) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(body == HENKA_INVALID_PHYSICS_BODY_ID);

    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, box, &state) == HENKA_SUCCESS);
    state.transform.position.x = FLT_MAX;
    state.transform.scale.x = FLT_MAX;
    HENKA_TEST_ASSERT(henka_physics_body_set_transform(
        world,
        box,
        state.transform,
        true) == HENKA_ERROR_INVALID_ARGUMENT);
    desc.collider = henka_physics_collider_box(
        (henka_vec3){FLT_MAX, 1.0f, 1.0f});
    desc.collider.offset.x = FLT_MAX;
    HENKA_TEST_ASSERT(henka_physics_body_set_collider(
        world,
        box,
        desc.collider) == HENKA_ERROR_INVALID_ARGUMENT);

    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_STATIC,
        henka_physics_collider_plane(
            (henka_vec3){FLT_MAX, FLT_MAX, 0.0f},
            0.0f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &plane) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_raycast(
        world,
        (henka_ray){{3.0f, 3.0f, 3.0f}, {-1.0f, -1.0f, 0.0f}},
        10.0f,
        HENKA_PHYSICS_ALL_LAYERS,
        &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(hit.hit && hit.body == plane);
    HENKA_TEST_ASSERT(henka_test_physics_float_is_finite(hit.normal.x));
    HENKA_TEST_ASSERT(henka_test_physics_float_is_finite(hit.normal.y));
    henka_physics_world_destroy(world);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world,
        (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_fixed_timestep(world, 0.001f) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_KINEMATIC,
        henka_physics_collider_box((henka_vec3){0.5f, 0.5f, 0.5f}),
        (henka_vec3){0.0f, 5.0f, 0.0f});
    desc.linear_velocity.x = 1.0f;
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &body) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_step(world, 0.25f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, body, &state) == HENKA_SUCCESS);
    position_after_capped_step = state.transform.position.x;
    HENKA_TEST_ASSERT(position_after_capped_step > 0.015f);
    HENKA_TEST_ASSERT(position_after_capped_step < 0.017f);
    HENKA_TEST_ASSERT(henka_physics_world_step(world, 0.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, body, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.transform.position.x,
        position_after_capped_step,
        0.000001f);
    henka_physics_world_destroy(world);

    HENKA_TEST_ASSERT(henka_physics_world_create(&world) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_world_set_gravity(
        world,
        (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_SUCCESS);
    desc = henka_test_physics_body(
        HENKA_PHYSICS_BODY_DYNAMIC,
        henka_physics_collider_sphere(0.5f),
        (henka_vec3){0.0f, 0.0f, 0.0f});
    HENKA_TEST_ASSERT(henka_physics_body_create(world, &desc, &body) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_set_linear_velocity(
        world,
        body,
        (henka_vec3){FLT_MAX, 0.0f, 0.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_physics_body_apply_impulse(
        world,
        body,
        (henka_vec3){FLT_MAX, 0.0f, 0.0f}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_physics_body_get_state(world, body, &state) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(state.linear_velocity.x == FLT_MAX);
    henka_physics_world_destroy(world);
}
void henka_test_physics(void)
{
    HENKA_TEST_ASSERT(strcmp(henka_physics_body_type_get_label(HENKA_PHYSICS_BODY_DYNAMIC), "Dynamic") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_physics_shape_type_get_label(HENKA_PHYSICS_SHAPE_BOX), "AABB") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_physics_shape_type_get_label(HENKA_PHYSICS_SHAPE_CAPSULE), "Capsule") == 0);
    henka_test_physics_motion_and_materials();
    henka_test_physics_contacts_and_events();
    henka_test_physics_capsule_contacts_and_raycast();
    henka_test_physics_capsule_box_separation();
    henka_test_physics_shape_pairs_and_raycast();
    henka_test_physics_pair_filters_and_response();
    henka_test_physics_scene_link();
    henka_test_physics_validation_and_tracking();
    henka_test_physics_destroy_preserves_contact_continuity();
    henka_test_physics_transactional_allocation_failure();
    henka_test_physics_numeric_failures();
    henka_test_physics_query_and_accumulator_hardening();
    henka_test_physics_capacity_growth();
}
