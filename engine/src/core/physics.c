#include <henka/physics.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <henka/memory.h>

#include "checked.h"

typedef struct henka_physics_body_record
{
    bool active;
    henka_physics_body_state state;
    henka_vec3 force;
    henka_vec3 torque;
} henka_physics_body_record;

typedef struct henka_physics_pair
{
    henka_physics_contact contact;
} henka_physics_pair;

struct henka_physics_world
{
    henka_physics_body_record* bodies;
    size_t body_capacity;
    size_t body_count;
    henka_physics_body_id next_body_id;
    henka_vec3 gravity;
    float fixed_timestep;
    float accumulator;
    henka_physics_contact* contacts;
    size_t contact_count;
    size_t contact_capacity;
    henka_physics_pair* current_pairs;
    size_t current_pair_count;
    size_t current_pair_capacity;
    henka_physics_pair* previous_pairs;
    size_t previous_pair_count;
    size_t previous_pair_capacity;
    henka_physics_event* events;
    size_t event_count;
    size_t event_capacity;
};

static bool henka_physics_is_finite_vec3(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static float henka_physics_clamp(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static float henka_physics_abs(float value)
{
    return value < 0.0f ? -value : value;
}

static henka_vec3 henka_physics_abs_vec3(henka_vec3 value)
{
    return (henka_vec3){henka_physics_abs(value.x), henka_physics_abs(value.y), henka_physics_abs(value.z)};
}

static henka_vec3 henka_physics_collider_center(const henka_physics_body_state* body)
{
    return henka_vec3_add(body->transform.position, body->collider.offset);
}

static henka_vec3 henka_physics_box_extents(const henka_physics_body_state* body)
{
    henka_vec3 scale = henka_physics_abs_vec3(body->transform.scale);
    return (henka_vec3){
        body->collider.data.box.half_extents.x * scale.x,
        body->collider.data.box.half_extents.y * scale.y,
        body->collider.data.box.half_extents.z * scale.z};
}

static float henka_physics_sphere_radius(const henka_physics_body_state* body)
{
    henka_vec3 scale = henka_physics_abs_vec3(body->transform.scale);
    float maximum = scale.x > scale.y ? scale.x : scale.y;
    maximum = maximum > scale.z ? maximum : scale.z;
    return body->collider.data.sphere.radius * maximum;
}

static float henka_physics_inverse_mass(const henka_physics_body_state* body)
{
    if (body->type != HENKA_PHYSICS_BODY_DYNAMIC || body->mass <= 0.0f)
    {
        return 0.0f;
    }
    return 1.0f / body->mass;
}

static bool henka_physics_material_valid(henka_physics_material material)
{
    return isfinite(material.restitution) && isfinite(material.static_friction) &&
        isfinite(material.dynamic_friction) && isfinite(material.linear_damping) &&
        isfinite(material.angular_damping) && material.restitution >= 0.0f &&
        material.restitution <= 1.0f && material.static_friction >= 0.0f &&
        material.dynamic_friction >= 0.0f && material.linear_damping >= 0.0f &&
        material.angular_damping >= 0.0f;
}

static bool henka_physics_collider_valid(henka_physics_collider_desc collider)
{
    if (!henka_physics_is_finite_vec3(collider.offset) || collider.layer == 0U)
    {
        return false;
    }
    switch (collider.shape)
    {
        case HENKA_PHYSICS_SHAPE_SPHERE:
            return isfinite(collider.data.sphere.radius) && collider.data.sphere.radius > 0.0f;
        case HENKA_PHYSICS_SHAPE_BOX:
            return henka_physics_is_finite_vec3(collider.data.box.half_extents) &&
                collider.data.box.half_extents.x > 0.0f && collider.data.box.half_extents.y > 0.0f &&
                collider.data.box.half_extents.z > 0.0f;
        case HENKA_PHYSICS_SHAPE_PLANE:
            return henka_physics_is_finite_vec3(collider.data.plane.normal) &&
                henka_vec3_length(collider.data.plane.normal) > 0.0001f && isfinite(collider.data.plane.offset);
        default:
            return false;
    }
}

static const float g_henka_physics_minimum_scale_magnitude = 0.01f;

static bool henka_scale_component_is_valid(float value)
{
    return isfinite(value) && henka_physics_abs(value) >= g_henka_physics_minimum_scale_magnitude;
}

static bool henka_physics_transform_valid(henka_transform transform)
{
    return henka_physics_is_finite_vec3(transform.position) &&
        henka_scale_component_is_valid(transform.scale.x) &&
        henka_scale_component_is_valid(transform.scale.y) &&
        henka_scale_component_is_valid(transform.scale.z) &&
        isfinite(transform.rotation.x) && isfinite(transform.rotation.y) &&
        isfinite(transform.rotation.z) && isfinite(transform.rotation.w);
}

static henka_physics_body_record* henka_physics_find_body(henka_physics_world* world, henka_physics_body_id id)
{
    size_t index;
    if (world == NULL || id == HENKA_INVALID_PHYSICS_BODY_ID)
    {
        return NULL;
    }
    for (index = 0U; index < world->body_capacity; ++index)
    {
        if (world->bodies[index].active && world->bodies[index].state.id == id)
        {
            return &world->bodies[index];
        }
    }
    return NULL;
}

static const henka_physics_body_record* henka_physics_find_body_const(const henka_physics_world* world, henka_physics_body_id id)
{
    return henka_physics_find_body((henka_physics_world*)world, id);
}

static bool henka_physics_reserve(void** values, size_t element_size, size_t* capacity, size_t required)
{
    size_t allocation_size;
    size_t next_capacity;
    void* resized;

    if (values == NULL || capacity == NULL || element_size == 0U ||
        !henka_checked_capacity(*capacity, required, 8U, HENKA_MAX_PHYSICS_ITEMS, &next_capacity) ||
        !henka_checked_size_multiply(element_size, next_capacity, &allocation_size))
    {
        return false;
    }

    if (next_capacity == *capacity)
    {
        return true;
    }

    resized = henka_realloc(*values, allocation_size);
    if (resized == NULL)
    {
        return false;
    }

    *values = resized;
    *capacity = next_capacity;
    return true;
}

static bool henka_physics_push_contact(henka_physics_world* world, henka_physics_contact contact)
{
    size_t required;

    if (world == NULL || !henka_checked_size_add(world->contact_count, 1U, &required) ||
        !henka_physics_reserve((void**)&world->contacts, sizeof(*world->contacts), &world->contact_capacity, required))
    {
        return false;
    }

    world->contacts[world->contact_count] = contact;
    ++world->contact_count;
    return true;
}

static bool henka_physics_push_pair(henka_physics_world* world, henka_physics_contact contact)
{
    size_t required;

    if (world == NULL || !henka_checked_size_add(world->current_pair_count, 1U, &required) ||
        !henka_physics_reserve((void**)&world->current_pairs, sizeof(*world->current_pairs), &world->current_pair_capacity, required))
    {
        return false;
    }

    world->current_pairs[world->current_pair_count].contact = contact;
    ++world->current_pair_count;
    return true;
}

static bool henka_physics_push_event(henka_physics_world* world, henka_physics_event_type type, henka_physics_contact contact)
{
    size_t required;

    if (world == NULL || !henka_checked_size_add(world->event_count, 1U, &required) ||
        !henka_physics_reserve((void**)&world->events, sizeof(*world->events), &world->event_capacity, required))
    {
        return false;
    }

    world->events[world->event_count] = (henka_physics_event){type, contact};
    ++world->event_count;
    return true;
}

static bool henka_physics_pair_matches(henka_physics_contact first, henka_physics_contact second)
{
    return first.body_a == second.body_a && first.body_b == second.body_b && first.is_trigger == second.is_trigger;
}

static void henka_physics_write_scene_transform(const henka_physics_body_state* state)
{
    if (state->linked_scene != NULL && state->linked_entity != HENKA_INVALID_ENTITY &&
        henka_scene_is_entity_valid(state->linked_scene, state->linked_entity) &&
        !henka_scene_is_entity_helper(state->linked_scene, state->linked_entity))
    {
        (void)henka_scene_set_entity_transform(state->linked_scene, state->linked_entity, state->transform);
    }
}

static bool henka_physics_sphere_sphere(const henka_physics_body_state* a, const henka_physics_body_state* b, henka_physics_contact* contact)
{
    henka_vec3 a_center = henka_physics_collider_center(a);
    henka_vec3 b_center = henka_physics_collider_center(b);
    henka_vec3 delta = henka_vec3_subtract(b_center, a_center);
    float radius_sum = henka_physics_sphere_radius(a) + henka_physics_sphere_radius(b);
    float distance = henka_vec3_length(delta);
    if (distance >= radius_sum)
    {
        return false;
    }
    contact->normal = distance > 0.0001f ? henka_vec3_scale(delta, 1.0f / distance) : (henka_vec3){1.0f, 0.0f, 0.0f};
    contact->penetration = radius_sum - distance;
    contact->point = henka_vec3_add(a_center, henka_vec3_scale(contact->normal, henka_physics_sphere_radius(a)));
    return true;
}

static bool henka_physics_box_box(const henka_physics_body_state* a, const henka_physics_body_state* b, henka_physics_contact* contact)
{
    henka_vec3 a_center = henka_physics_collider_center(a);
    henka_vec3 b_center = henka_physics_collider_center(b);
    henka_vec3 a_extents = henka_physics_box_extents(a);
    henka_vec3 b_extents = henka_physics_box_extents(b);
    henka_vec3 delta = henka_vec3_subtract(b_center, a_center);
    float x = a_extents.x + b_extents.x - henka_physics_abs(delta.x);
    float y = a_extents.y + b_extents.y - henka_physics_abs(delta.y);
    float z = a_extents.z + b_extents.z - henka_physics_abs(delta.z);
    if (x <= 0.0f || y <= 0.0f || z <= 0.0f)
    {
        return false;
    }
    contact->penetration = x;
    contact->normal = (henka_vec3){delta.x < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f};
    if (y < contact->penetration)
    {
        contact->penetration = y;
        contact->normal = (henka_vec3){0.0f, delta.y < 0.0f ? -1.0f : 1.0f, 0.0f};
    }
    if (z < contact->penetration)
    {
        contact->penetration = z;
        contact->normal = (henka_vec3){0.0f, 0.0f, delta.z < 0.0f ? -1.0f : 1.0f};
    }
    contact->point = henka_vec3_add(a_center, henka_vec3_scale(contact->normal, contact->penetration * 0.5f));
    return true;
}

static bool henka_physics_sphere_box(const henka_physics_body_state* sphere, const henka_physics_body_state* box, henka_physics_contact* contact)
{
    henka_vec3 sphere_center = henka_physics_collider_center(sphere);
    henka_vec3 box_center = henka_physics_collider_center(box);
    henka_vec3 extents = henka_physics_box_extents(box);
    henka_vec3 relative = henka_vec3_subtract(sphere_center, box_center);
    henka_vec3 closest = {
        henka_physics_clamp(relative.x, -extents.x, extents.x),
        henka_physics_clamp(relative.y, -extents.y, extents.y),
        henka_physics_clamp(relative.z, -extents.z, extents.z)};
    henka_vec3 closest_world = henka_vec3_add(box_center, closest);
    henka_vec3 box_to_sphere = henka_vec3_subtract(sphere_center, closest_world);
    float radius = henka_physics_sphere_radius(sphere);
    float distance = henka_vec3_length(box_to_sphere);
    if (distance >= radius)
    {
        return false;
    }
    if (distance > 0.0001f)
    {
        contact->normal = henka_vec3_scale(box_to_sphere, -1.0f / distance);
        contact->penetration = radius - distance;
    }
    else
    {
        float px = extents.x - henka_physics_abs(relative.x);
        float py = extents.y - henka_physics_abs(relative.y);
        float pz = extents.z - henka_physics_abs(relative.z);
        contact->penetration = radius + px;
        contact->normal = (henka_vec3){relative.x < 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f};
        if (py < px && py <= pz)
        {
            contact->penetration = radius + py;
            contact->normal = (henka_vec3){0.0f, relative.y < 0.0f ? 1.0f : -1.0f, 0.0f};
        }
        else if (pz < px)
        {
            contact->penetration = radius + pz;
            contact->normal = (henka_vec3){0.0f, 0.0f, relative.z < 0.0f ? 1.0f : -1.0f};
        }
    }
    contact->point = closest_world;
    return true;
}

static bool henka_physics_shape_plane(const henka_physics_body_state* shape, const henka_physics_body_state* plane, henka_physics_contact* contact)
{
    henka_vec3 normal = henka_vec3_normalize(plane->collider.data.plane.normal);
    float offset = plane->collider.data.plane.offset + henka_vec3_dot(normal, plane->transform.position);
    float radius;
    float distance;
    if (shape->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE)
    {
        radius = henka_physics_sphere_radius(shape);
    }
    else if (shape->collider.shape == HENKA_PHYSICS_SHAPE_BOX)
    {
        henka_vec3 extents = henka_physics_box_extents(shape);
        radius = henka_physics_abs(normal.x) * extents.x + henka_physics_abs(normal.y) * extents.y + henka_physics_abs(normal.z) * extents.z;
    }
    else
    {
        return false;
    }
    distance = henka_vec3_dot(normal, henka_physics_collider_center(shape)) - offset;
    if (distance >= radius)
    {
        return false;
    }
    contact->normal = henka_vec3_scale(normal, -1.0f);
    contact->penetration = radius - distance;
    contact->point = henka_vec3_subtract(henka_physics_collider_center(shape), henka_vec3_scale(normal, distance));
    return true;
}

static bool henka_physics_detect_contact(const henka_physics_body_state* a, const henka_physics_body_state* b, henka_physics_contact* contact)
{
    bool found = false;
    henka_physics_contact swapped;
    *contact = (henka_physics_contact){a->id, b->id, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, a->collider.is_trigger || b->collider.is_trigger};
    if (a->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE && b->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE)
    {
        found = henka_physics_sphere_sphere(a, b, contact);
    }
    else if (a->collider.shape == HENKA_PHYSICS_SHAPE_BOX && b->collider.shape == HENKA_PHYSICS_SHAPE_BOX)
    {
        found = henka_physics_box_box(a, b, contact);
    }
    else if (a->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE && b->collider.shape == HENKA_PHYSICS_SHAPE_BOX)
    {
        found = henka_physics_sphere_box(a, b, contact);
    }
    else if (a->collider.shape == HENKA_PHYSICS_SHAPE_BOX && b->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE)
    {
        swapped = *contact;
        swapped.body_a = b->id;
        swapped.body_b = a->id;
        found = henka_physics_sphere_box(b, a, &swapped);
        if (found)
        {
            contact->normal = henka_vec3_scale(swapped.normal, -1.0f);
            contact->penetration = swapped.penetration;
            contact->point = swapped.point;
        }
    }
    else if (b->collider.shape == HENKA_PHYSICS_SHAPE_PLANE)
    {
        found = henka_physics_shape_plane(a, b, contact);
    }
    else if (a->collider.shape == HENKA_PHYSICS_SHAPE_PLANE)
    {
        swapped = *contact;
        swapped.body_a = b->id;
        swapped.body_b = a->id;
        found = henka_physics_shape_plane(b, a, &swapped);
        if (found)
        {
            contact->normal = henka_vec3_scale(swapped.normal, -1.0f);
            contact->penetration = swapped.penetration;
            contact->point = swapped.point;
        }
    }
    return found;
}

static void henka_physics_resolve_contact(henka_physics_body_record* a, henka_physics_body_record* b, const henka_physics_contact* contact)
{
    float inverse_a = henka_physics_inverse_mass(&a->state);
    float inverse_b = henka_physics_inverse_mass(&b->state);
    float inverse_sum = inverse_a + inverse_b;
    henka_vec3 relative_velocity;
    float normal_speed;
    float restitution;
    float normal_impulse;
    henka_vec3 impulse;
    henka_vec3 tangent;
    float tangent_length;
    float tangent_impulse;
    float static_friction;
    float friction;
    if (inverse_sum <= 0.0f || contact->is_trigger)
    {
        return;
    }
    {
        const float slop = 0.001f;
        const float correction_percent = 0.75f;
        float correction_depth = contact->penetration > slop ? contact->penetration - slop : 0.0f;
        henka_vec3 correction = henka_vec3_scale(contact->normal, correction_depth * correction_percent / inverse_sum);
        a->state.transform.position = henka_vec3_subtract(a->state.transform.position, henka_vec3_scale(correction, inverse_a));
        b->state.transform.position = henka_vec3_add(b->state.transform.position, henka_vec3_scale(correction, inverse_b));
    }
    relative_velocity = henka_vec3_subtract(b->state.linear_velocity, a->state.linear_velocity);
    normal_speed = henka_vec3_dot(relative_velocity, contact->normal);
    if (normal_speed >= 0.0f)
    {
        return;
    }
    restitution = a->state.material.restitution > b->state.material.restitution ?
        a->state.material.restitution : b->state.material.restitution;
    normal_impulse = -(1.0f + restitution) * normal_speed / inverse_sum;
    impulse = henka_vec3_scale(contact->normal, normal_impulse);
    a->state.linear_velocity = henka_vec3_subtract(a->state.linear_velocity, henka_vec3_scale(impulse, inverse_a));
    b->state.linear_velocity = henka_vec3_add(b->state.linear_velocity, henka_vec3_scale(impulse, inverse_b));
    relative_velocity = henka_vec3_subtract(b->state.linear_velocity, a->state.linear_velocity);
    tangent = henka_vec3_subtract(relative_velocity, henka_vec3_scale(contact->normal, henka_vec3_dot(relative_velocity, contact->normal)));
    tangent_length = henka_vec3_length(tangent);
    if (tangent_length <= 0.0001f)
    {
        return;
    }
    tangent = henka_vec3_scale(tangent, 1.0f / tangent_length);
    tangent_impulse = -henka_vec3_dot(relative_velocity, tangent) / inverse_sum;
    static_friction = sqrtf(a->state.material.static_friction * b->state.material.static_friction);
    friction = sqrtf(a->state.material.dynamic_friction * b->state.material.dynamic_friction);
    if (henka_physics_abs(tangent_impulse) > normal_impulse * static_friction)
    {
        tangent_impulse = tangent_impulse < 0.0f ? -normal_impulse * friction : normal_impulse * friction;
    }
    impulse = henka_vec3_scale(tangent, tangent_impulse);
    a->state.linear_velocity = henka_vec3_subtract(a->state.linear_velocity, henka_vec3_scale(impulse, inverse_a));
    b->state.linear_velocity = henka_vec3_add(b->state.linear_velocity, henka_vec3_scale(impulse, inverse_b));
}

static bool henka_physics_emit_events(henka_physics_world* world)
{
    size_t current_index;
    size_t previous_index;
    for (current_index = 0U; current_index < world->current_pair_count; ++current_index)
    {
        bool existed = false;
        henka_physics_contact current = world->current_pairs[current_index].contact;
        for (previous_index = 0U; previous_index < world->previous_pair_count; ++previous_index)
        {
            if (henka_physics_pair_matches(current, world->previous_pairs[previous_index].contact))
            {
                existed = true;
                break;
            }
        }
        if (!henka_physics_push_event(
                world,
                current.is_trigger ?
                    (existed ? HENKA_PHYSICS_EVENT_TRIGGER_STAY : HENKA_PHYSICS_EVENT_TRIGGER_ENTER) :
                    (existed ? HENKA_PHYSICS_EVENT_COLLISION_STAY : HENKA_PHYSICS_EVENT_COLLISION_ENTER),
                current))
        {
            return false;
        }
    }
    for (previous_index = 0U; previous_index < world->previous_pair_count; ++previous_index)
    {
        bool still_exists = false;
        henka_physics_contact previous = world->previous_pairs[previous_index].contact;
        for (current_index = 0U; current_index < world->current_pair_count; ++current_index)
        {
            if (henka_physics_pair_matches(previous, world->current_pairs[current_index].contact))
            {
                still_exists = true;
                break;
            }
        }
        if (!still_exists && !henka_physics_push_event(
                world,
                previous.is_trigger ? HENKA_PHYSICS_EVENT_TRIGGER_EXIT : HENKA_PHYSICS_EVENT_COLLISION_EXIT,
                previous))
        {
            return false;
        }
    }
    if (!henka_physics_reserve((void**)&world->previous_pairs, sizeof(*world->previous_pairs), &world->previous_pair_capacity, world->current_pair_count))
    {
        return false;
    }
    if (world->current_pair_count > 0U)
    {
        memcpy(world->previous_pairs, world->current_pairs, sizeof(*world->current_pairs) * world->current_pair_count);
    }
    world->previous_pair_count = world->current_pair_count;
    return true;
}

static henka_result henka_physics_substep(henka_physics_world* world, float delta_seconds)
{
    size_t index;
    size_t other_index;
    for (index = 0U; index < world->body_capacity; ++index)
    {
        henka_physics_body_record* body = &world->bodies[index];
        float inverse_mass;
        if (!body->active)
        {
            continue;
        }
        body->state.colliding = false;
        body->state.grounded = false;
        inverse_mass = henka_physics_inverse_mass(&body->state);
        if (body->state.type == HENKA_PHYSICS_BODY_DYNAMIC)
        {
            henka_vec3 acceleration = henka_vec3_add(world->gravity, henka_vec3_scale(body->force, inverse_mass));
            float linear_factor = henka_physics_clamp(1.0f - body->state.material.linear_damping * delta_seconds, 0.0f, 1.0f);
            float angular_factor = henka_physics_clamp(1.0f - body->state.material.angular_damping * delta_seconds, 0.0f, 1.0f);
            body->state.linear_velocity = henka_vec3_add(body->state.linear_velocity, henka_vec3_scale(acceleration, delta_seconds));
            body->state.angular_velocity = henka_vec3_add(body->state.angular_velocity, henka_vec3_scale(body->torque, inverse_mass * delta_seconds));
            body->state.linear_velocity = henka_vec3_scale(body->state.linear_velocity, linear_factor);
            body->state.angular_velocity = henka_vec3_scale(body->state.angular_velocity, angular_factor);
        }
        if (body->state.type == HENKA_PHYSICS_BODY_DYNAMIC || body->state.type == HENKA_PHYSICS_BODY_KINEMATIC)
        {
            float angular_speed;
            body->state.transform.position = henka_vec3_add(body->state.transform.position, henka_vec3_scale(body->state.linear_velocity, delta_seconds));
            angular_speed = henka_vec3_length(body->state.angular_velocity);
            if (angular_speed > 0.0001f)
            {
                henka_quat delta_rotation = henka_quat_from_axis_angle(henka_vec3_scale(body->state.angular_velocity, 1.0f / angular_speed), angular_speed * delta_seconds);
                body->state.transform.rotation = henka_quat_normalize(henka_quat_multiply(delta_rotation, body->state.transform.rotation));
            }
        }
        body->force = (henka_vec3){0.0f, 0.0f, 0.0f};
        body->torque = (henka_vec3){0.0f, 0.0f, 0.0f};
    }
    world->contact_count = 0U;
    world->current_pair_count = 0U;
    for (index = 0U; index < world->body_capacity; ++index)
    {
        henka_physics_body_record* a = &world->bodies[index];
        if (!a->active)
        {
            continue;
        }
        for (other_index = index + 1U; other_index < world->body_capacity; ++other_index)
        {
            henka_physics_body_record* b = &world->bodies[other_index];
            henka_physics_contact contact;
            if (!b->active || (a->state.type == HENKA_PHYSICS_BODY_STATIC && b->state.type == HENKA_PHYSICS_BODY_STATIC) ||
                (a->state.collider.mask & b->state.collider.layer) == 0U ||
                (b->state.collider.mask & a->state.collider.layer) == 0U)
            {
                continue;
            }
            if (henka_physics_detect_contact(&a->state, &b->state, &contact))
            {
                if (!henka_physics_push_contact(world, contact) || !henka_physics_push_pair(world, contact))
                {
                    return HENKA_ERROR_OUT_OF_MEMORY;
                }
                a->state.colliding = true;
                b->state.colliding = true;
                if (!contact.is_trigger)
                {
                    if (contact.normal.y < -0.5f && a->state.type == HENKA_PHYSICS_BODY_DYNAMIC)
                    {
                        a->state.grounded = true;
                    }
                    if (contact.normal.y > 0.5f && b->state.type == HENKA_PHYSICS_BODY_DYNAMIC)
                    {
                        b->state.grounded = true;
                    }
                    henka_physics_resolve_contact(a, b, &contact);
                }
            }
        }
    }
    if (!henka_physics_emit_events(world))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < world->body_capacity; ++index)
    {
        if (world->bodies[index].active)
        {
            henka_physics_write_scene_transform(&world->bodies[index].state);
        }
    }
    return HENKA_SUCCESS;
}

henka_physics_material henka_physics_material_default(void)
{
    return (henka_physics_material){0.1f, 0.6f, 0.45f, 0.05f, 0.05f};
}

henka_physics_collider_desc henka_physics_collider_sphere(float radius)
{
    henka_physics_collider_desc collider = {0};
    collider.shape = HENKA_PHYSICS_SHAPE_SPHERE;
    collider.data.sphere.radius = radius;
    collider.layer = 1U;
    collider.mask = HENKA_PHYSICS_ALL_LAYERS;
    return collider;
}

henka_physics_collider_desc henka_physics_collider_box(henka_vec3 half_extents)
{
    henka_physics_collider_desc collider = {0};
    collider.shape = HENKA_PHYSICS_SHAPE_BOX;
    collider.data.box.half_extents = half_extents;
    collider.layer = 1U;
    collider.mask = HENKA_PHYSICS_ALL_LAYERS;
    return collider;
}

henka_physics_collider_desc henka_physics_collider_plane(henka_vec3 normal, float offset)
{
    henka_physics_collider_desc collider = {0};
    collider.shape = HENKA_PHYSICS_SHAPE_PLANE;
    collider.data.plane.normal = henka_vec3_normalize(normal);
    collider.data.plane.offset = offset;
    collider.layer = 1U;
    collider.mask = HENKA_PHYSICS_ALL_LAYERS;
    return collider;
}

henka_result henka_physics_world_create(henka_physics_world** out_world)
{
    henka_physics_world* world;
    if (out_world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    world = henka_calloc(1U, sizeof(*world));
    if (world == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    world->gravity = (henka_vec3){0.0f, -9.81f, 0.0f};
    world->fixed_timestep = 1.0f / 60.0f;
    world->next_body_id = 1U;
    *out_world = world;
    return HENKA_SUCCESS;
}

void henka_physics_world_destroy(henka_physics_world* world)
{
    if (world == NULL)
    {
        return;
    }
    henka_free(world->bodies);
    henka_free(world->contacts);
    henka_free(world->current_pairs);
    henka_free(world->previous_pairs);
    henka_free(world->events);
    henka_free(world);
}

henka_result henka_physics_world_set_gravity(henka_physics_world* world, henka_vec3 gravity)
{
    if (world == NULL || !henka_physics_is_finite_vec3(gravity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    world->gravity = gravity;
    return HENKA_SUCCESS;
}

henka_vec3 henka_physics_world_get_gravity(const henka_physics_world* world)
{
    return world != NULL ? world->gravity : (henka_vec3){0.0f, 0.0f, 0.0f};
}

henka_result henka_physics_world_set_fixed_timestep(henka_physics_world* world, float fixed_timestep)
{
    if (world == NULL || !isfinite(fixed_timestep) || fixed_timestep <= 0.0f || fixed_timestep > 1.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    world->fixed_timestep = fixed_timestep;
    world->accumulator = 0.0f;
    return HENKA_SUCCESS;
}

float henka_physics_world_get_fixed_timestep(const henka_physics_world* world)
{
    return world != NULL ? world->fixed_timestep : 0.0f;
}

size_t henka_physics_world_get_body_count(const henka_physics_world* world)
{
    return world != NULL ? world->body_count : 0U;
}

henka_result henka_physics_body_create(
    henka_physics_world* world,
    const henka_physics_body_desc* desc,
    henka_physics_body_id* out_body)
{
    henka_physics_body_record* body;
    size_t index;
    size_t old_capacity;
    size_t required;

    if (world == NULL || desc == NULL || out_body == NULL ||
        desc->type < HENKA_PHYSICS_BODY_STATIC || desc->type > HENKA_PHYSICS_BODY_KINEMATIC ||
        !henka_physics_transform_valid(desc->transform) || !henka_physics_is_finite_vec3(desc->linear_velocity) ||
        !henka_physics_is_finite_vec3(desc->angular_velocity) || !henka_physics_material_valid(desc->material) ||
        !henka_physics_collider_valid(desc->collider) ||
        (desc->collider.shape == HENKA_PHYSICS_SHAPE_PLANE && desc->type != HENKA_PHYSICS_BODY_STATIC) ||
        (desc->type == HENKA_PHYSICS_BODY_DYNAMIC && (!isfinite(desc->mass) || desc->mass <= 0.0f)) ||
        world->next_body_id == UINT32_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < world->body_capacity; ++index)
    {
        if (!world->bodies[index].active)
        {
            break;
        }
    }

    old_capacity = world->body_capacity;
    if (index == world->body_capacity)
    {
        if (!henka_checked_size_add(world->body_capacity, 1U, &required) ||
            !henka_physics_reserve(
                (void**)&world->bodies,
                sizeof(*world->bodies),
                &world->body_capacity,
                required))
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }

    if (world->body_capacity > old_capacity)
    {
        size_t new_record_count;
        size_t new_record_bytes;

        new_record_count = world->body_capacity - old_capacity;
        if (!henka_checked_size_multiply(new_record_count, sizeof(*world->bodies), &new_record_bytes))
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }

        memset(&world->bodies[old_capacity], 0, new_record_bytes);
    }

    body = &world->bodies[index];
    memset(body, 0, sizeof(*body));
    body->active = true;
    body->state.id = world->next_body_id;
    ++world->next_body_id;
    body->state.type = desc->type;
    body->state.transform = desc->transform;
    body->state.initial_transform = desc->transform;
    body->state.mass = desc->type == HENKA_PHYSICS_BODY_DYNAMIC ? desc->mass : 0.0f;
    body->state.linear_velocity = desc->linear_velocity;
    body->state.angular_velocity = desc->angular_velocity;
    body->state.material = desc->material;
    body->state.collider = desc->collider;
    body->state.linked_scene = desc->linked_scene;
    body->state.linked_entity = desc->linked_entity;
    ++world->body_count;
    *out_body = body->state.id;
    henka_physics_write_scene_transform(&body->state);
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_destroy(henka_physics_world* world, henka_physics_body_id body)
{
    size_t index;
    henka_physics_body_record* record;

    record = henka_physics_find_body(world, body);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(record, 0, sizeof(*record));
    --world->body_count;
    world->contact_count = 0U;
    world->event_count = 0U;
    world->current_pair_count = 0U;
    world->previous_pair_count = 0U;
    for (index = 0U; index < world->body_capacity; ++index)
    {
        if (world->bodies[index].active)
        {
            world->bodies[index].state.colliding = false;
            world->bodies[index].state.grounded = false;
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_get_state(const henka_physics_world* world, henka_physics_body_id body, henka_physics_body_state* out_state)
{
    const henka_physics_body_record* record = henka_physics_find_body_const(world, body);
    if (record == NULL || out_state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_state = record->state;
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_set_transform(henka_physics_world* world, henka_physics_body_id body, henka_transform transform, bool clear_velocity)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    if (record == NULL || !henka_physics_transform_valid(transform))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->state.transform = transform;
    if (clear_velocity)
    {
        record->state.linear_velocity = (henka_vec3){0.0f, 0.0f, 0.0f};
        record->state.angular_velocity = (henka_vec3){0.0f, 0.0f, 0.0f};
    }
    henka_physics_write_scene_transform(&record->state);
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_set_type(henka_physics_world* world, henka_physics_body_id body, henka_physics_body_type type)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    if (record == NULL || type < HENKA_PHYSICS_BODY_STATIC || type > HENKA_PHYSICS_BODY_KINEMATIC ||
        (record->state.collider.shape == HENKA_PHYSICS_SHAPE_PLANE && type != HENKA_PHYSICS_BODY_STATIC))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->state.type = type;
    if (type != HENKA_PHYSICS_BODY_DYNAMIC)
    {
        record->state.mass = 0.0f;
    }
    else if (record->state.mass <= 0.0f)
    {
        record->state.mass = 1.0f;
    }
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_set_collider(henka_physics_world* world, henka_physics_body_id body, henka_physics_collider_desc collider)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    if (record == NULL || !henka_physics_collider_valid(collider) ||
        (collider.shape == HENKA_PHYSICS_SHAPE_PLANE && record->state.type != HENKA_PHYSICS_BODY_STATIC))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->state.collider = collider;
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_set_material(henka_physics_world* world, henka_physics_body_id body, henka_physics_material material)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    if (record == NULL || !henka_physics_material_valid(material))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->state.material = material;
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_set_linear_velocity(henka_physics_world* world, henka_physics_body_id body, henka_vec3 velocity)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    if (record == NULL || !henka_physics_is_finite_vec3(velocity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->state.linear_velocity = velocity;
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_set_angular_velocity(henka_physics_world* world, henka_physics_body_id body, henka_vec3 velocity)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    if (record == NULL || !henka_physics_is_finite_vec3(velocity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->state.angular_velocity = velocity;
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_apply_force(henka_physics_world* world, henka_physics_body_id body, henka_vec3 force)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    if (record == NULL || !henka_physics_is_finite_vec3(force) || record->state.type != HENKA_PHYSICS_BODY_DYNAMIC)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->force = henka_vec3_add(record->force, force);
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_apply_impulse(henka_physics_world* world, henka_physics_body_id body, henka_vec3 impulse)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    float inverse_mass;
    if (record == NULL || !henka_physics_is_finite_vec3(impulse) || record->state.type != HENKA_PHYSICS_BODY_DYNAMIC)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    inverse_mass = henka_physics_inverse_mass(&record->state);
    record->state.linear_velocity = henka_vec3_add(record->state.linear_velocity, henka_vec3_scale(impulse, inverse_mass));
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_apply_torque(henka_physics_world* world, henka_physics_body_id body, henka_vec3 torque)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    if (record == NULL || !henka_physics_is_finite_vec3(torque) || record->state.type != HENKA_PHYSICS_BODY_DYNAMIC)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->torque = henka_vec3_add(record->torque, torque);
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_clear_velocity(henka_physics_world* world, henka_physics_body_id body)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->state.linear_velocity = (henka_vec3){0.0f, 0.0f, 0.0f};
    record->state.angular_velocity = (henka_vec3){0.0f, 0.0f, 0.0f};
    return HENKA_SUCCESS;
}

henka_result henka_physics_world_step(henka_physics_world* world, float delta_seconds)
{
    unsigned int substeps = 0U;
    if (world == NULL || !isfinite(delta_seconds) || delta_seconds < 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    world->event_count = 0U;
    world->accumulator += delta_seconds > 0.25f ? 0.25f : delta_seconds;
    while (world->accumulator >= world->fixed_timestep && substeps < 16U)
    {
        henka_result result = henka_physics_substep(world, world->fixed_timestep);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        world->accumulator -= world->fixed_timestep;
        ++substeps;
    }
    return HENKA_SUCCESS;
}

henka_result henka_physics_world_step_fixed(henka_physics_world* world)
{
    if (world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    world->event_count = 0U;
    return henka_physics_substep(world, world->fixed_timestep);
}

henka_result henka_physics_world_reset(henka_physics_world* world)
{
    size_t index;
    if (world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < world->body_capacity; ++index)
    {
        if (world->bodies[index].active)
        {
            world->bodies[index].state.transform = world->bodies[index].state.initial_transform;
            world->bodies[index].state.linear_velocity = (henka_vec3){0.0f, 0.0f, 0.0f};
            world->bodies[index].state.angular_velocity = (henka_vec3){0.0f, 0.0f, 0.0f};
            world->bodies[index].state.colliding = false;
            world->bodies[index].state.grounded = false;
            world->bodies[index].force = (henka_vec3){0.0f, 0.0f, 0.0f};
            world->bodies[index].torque = (henka_vec3){0.0f, 0.0f, 0.0f};
            henka_physics_write_scene_transform(&world->bodies[index].state);
        }
    }
    world->accumulator = 0.0f;
    world->contact_count = 0U;
    world->event_count = 0U;
    world->current_pair_count = 0U;
    world->previous_pair_count = 0U;
    return HENKA_SUCCESS;
}

const henka_physics_contact* henka_physics_world_get_contacts(const henka_physics_world* world, size_t* out_count)
{
    if (out_count != NULL)
    {
        *out_count = world != NULL ? world->contact_count : 0U;
    }
    return world != NULL ? world->contacts : NULL;
}

const henka_physics_event* henka_physics_world_get_events(const henka_physics_world* world, size_t* out_count)
{
    if (out_count != NULL)
    {
        *out_count = world != NULL ? world->event_count : 0U;
    }
    return world != NULL ? world->events : NULL;
}

static bool henka_physics_raycast_sphere(const henka_physics_body_state* body, henka_ray ray, float maximum, float* distance, henka_vec3* normal)
{
    henka_vec3 offset = henka_vec3_subtract(ray.origin, henka_physics_collider_center(body));
    float b = henka_vec3_dot(offset, ray.direction);
    float c = henka_vec3_dot(offset, offset) - henka_physics_sphere_radius(body) * henka_physics_sphere_radius(body);
    float discriminant = b * b - c;
    float result;
    if (discriminant < 0.0f)
    {
        return false;
    }
    result = -b - sqrtf(discriminant);
    if (result < 0.0f)
    {
        result = -b + sqrtf(discriminant);
    }
    if (result < 0.0f || result > maximum)
    {
        return false;
    }
    *distance = result;
    *normal = henka_vec3_normalize(henka_vec3_subtract(henka_vec3_add(ray.origin, henka_vec3_scale(ray.direction, result)), henka_physics_collider_center(body)));
    return true;
}

static bool henka_physics_raycast_box(const henka_physics_body_state* body, henka_ray ray, float maximum, float* distance, henka_vec3* normal)
{
    henka_vec3 center = henka_physics_collider_center(body);
    henka_vec3 extents = henka_physics_box_extents(body);
    float minimum = 0.0f;
    float max_value = maximum;
    int axis;
    henka_vec3 hit_normal = {0.0f, 0.0f, 0.0f};
    float origins[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    float directions[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    float centers[3] = {center.x, center.y, center.z};
    float extent_values[3] = {extents.x, extents.y, extents.z};
    for (axis = 0; axis < 3; ++axis)
    {
        float low;
        float high;
        float sign = -1.0f;
        if (henka_physics_abs(directions[axis]) < 0.00001f)
        {
            if (origins[axis] < centers[axis] - extent_values[axis] || origins[axis] > centers[axis] + extent_values[axis])
            {
                return false;
            }
            continue;
        }
        low = (centers[axis] - extent_values[axis] - origins[axis]) / directions[axis];
        high = (centers[axis] + extent_values[axis] - origins[axis]) / directions[axis];
        if (low > high)
        {
            float temporary = low;
            low = high;
            high = temporary;
            sign = 1.0f;
        }
        if (low > minimum)
        {
            minimum = low;
            hit_normal = (henka_vec3){0.0f, 0.0f, 0.0f};
            if (axis == 0) hit_normal.x = sign;
            if (axis == 1) hit_normal.y = sign;
            if (axis == 2) hit_normal.z = sign;
        }
        if (high < max_value)
        {
            max_value = high;
        }
        if (minimum > max_value)
        {
            return false;
        }
    }
    *distance = minimum;
    *normal = hit_normal;
    return minimum <= maximum;
}

static bool henka_physics_raycast_plane(const henka_physics_body_state* body, henka_ray ray, float maximum, float* distance, henka_vec3* normal)
{
    henka_vec3 plane_normal = henka_vec3_normalize(body->collider.data.plane.normal);
    float denominator = henka_vec3_dot(plane_normal, ray.direction);
    float offset = body->collider.data.plane.offset + henka_vec3_dot(plane_normal, body->transform.position);
    float result;
    if (henka_physics_abs(denominator) < 0.00001f)
    {
        return false;
    }
    result = (offset - henka_vec3_dot(plane_normal, ray.origin)) / denominator;
    if (result < 0.0f || result > maximum)
    {
        return false;
    }
    *distance = result;
    *normal = denominator < 0.0f ? plane_normal : henka_vec3_scale(plane_normal, -1.0f);
    return true;
}

henka_result henka_physics_world_raycast(const henka_physics_world* world, henka_ray ray, float max_distance, uint32_t layer_mask, henka_physics_raycast_hit* out_hit)
{
    size_t index;
    float closest = FLT_MAX;
    if (world == NULL || out_hit == NULL || !henka_physics_is_finite_vec3(ray.origin) ||
        !henka_physics_is_finite_vec3(ray.direction) || henka_vec3_length(ray.direction) <= 0.0001f ||
        !isfinite(max_distance) || max_distance <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    ray.direction = henka_vec3_normalize(ray.direction);
    *out_hit = (henka_physics_raycast_hit){false, HENKA_INVALID_PHYSICS_BODY_ID, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f};
    for (index = 0U; index < world->body_capacity; ++index)
    {
        const henka_physics_body_state* body;
        float distance = 0.0f;
        henka_vec3 normal = {0.0f, 0.0f, 0.0f};
        bool hit = false;
        if (!world->bodies[index].active || (world->bodies[index].state.collider.layer & layer_mask) == 0U)
        {
            continue;
        }
        body = &world->bodies[index].state;
        if (body->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE)
        {
            hit = henka_physics_raycast_sphere(body, ray, max_distance, &distance, &normal);
        }
        else if (body->collider.shape == HENKA_PHYSICS_SHAPE_BOX)
        {
            hit = henka_physics_raycast_box(body, ray, max_distance, &distance, &normal);
        }
        else if (body->collider.shape == HENKA_PHYSICS_SHAPE_PLANE)
        {
            hit = henka_physics_raycast_plane(body, ray, max_distance, &distance, &normal);
        }
        if (hit && distance < closest)
        {
            closest = distance;
            out_hit->hit = true;
            out_hit->body = body->id;
            out_hit->distance = distance;
            out_hit->normal = normal;
            out_hit->point = henka_vec3_add(ray.origin, henka_vec3_scale(ray.direction, distance));
        }
    }
    return HENKA_SUCCESS;
}

size_t henka_physics_world_get_debug_shape_count(const henka_physics_world* world)
{
    return henka_physics_world_get_body_count(world);
}

henka_result henka_physics_world_get_debug_shape(const henka_physics_world* world, size_t index, henka_physics_debug_shape* out_shape)
{
    size_t body_index;
    size_t current = 0U;
    if (world == NULL || out_shape == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (body_index = 0U; body_index < world->body_capacity; ++body_index)
    {
        if (world->bodies[body_index].active)
        {
            if (current == index)
            {
                out_shape->body = world->bodies[body_index].state.id;
                out_shape->transform = world->bodies[body_index].state.transform;
                out_shape->collider = world->bodies[body_index].state.collider;
                out_shape->colliding = world->bodies[body_index].state.colliding;
                out_shape->grounded = world->bodies[body_index].state.grounded;
                return HENKA_SUCCESS;
            }
            ++current;
        }
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

const char* henka_physics_body_type_get_label(henka_physics_body_type type)
{
    switch (type)
    {
        case HENKA_PHYSICS_BODY_STATIC: return "Static";
        case HENKA_PHYSICS_BODY_DYNAMIC: return "Dynamic";
        case HENKA_PHYSICS_BODY_KINEMATIC: return "Kinematic";
        default: return "Unknown";
    }
}

const char* henka_physics_shape_type_get_label(henka_physics_shape_type type)
{
    switch (type)
    {
        case HENKA_PHYSICS_SHAPE_SPHERE: return "Sphere";
        case HENKA_PHYSICS_SHAPE_BOX: return "AABB";
        case HENKA_PHYSICS_SHAPE_PLANE: return "Plane";
        default: return "Unknown";
    }
}

const char* henka_physics_event_type_get_label(henka_physics_event_type type)
{
    switch (type)
    {
        case HENKA_PHYSICS_EVENT_COLLISION_ENTER: return "Collision Enter";
        case HENKA_PHYSICS_EVENT_COLLISION_STAY: return "Collision Stay";
        case HENKA_PHYSICS_EVENT_COLLISION_EXIT: return "Collision Exit";
        case HENKA_PHYSICS_EVENT_TRIGGER_ENTER: return "Trigger Enter";
        case HENKA_PHYSICS_EVENT_TRIGGER_STAY: return "Trigger Stay";
        case HENKA_PHYSICS_EVENT_TRIGGER_EXIT: return "Trigger Exit";
        default: return "Unknown";
    }
}
