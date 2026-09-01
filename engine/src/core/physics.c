#include <henka/physics.h>

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <henka/memory.h>

#include "checked.h"
#include "physics_internal.h"

typedef struct henka_physics_body_record
{
    bool active;
    henka_physics_body_state state;
    henka_vec3 force;
    henka_vec3 torque;
    int32_t* owned_heightfield_heights_millimeters;
    size_t owned_heightfield_sample_count;
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

static bool henka_physics_double_fits_float(double value);

static bool henka_physics_is_finite_vec3(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool henka_physics_try_float(double value, float* out_value)
{
    if (out_value == NULL || !henka_physics_double_fits_float(value))
    {
        return false;
    }
    *out_value = (float)value;
    return true;
}

static bool henka_physics_try_vec3(
    double x,
    double y,
    double z,
    henka_vec3* out_value)
{
    if (out_value == NULL ||
        !henka_physics_double_fits_float(x) ||
        !henka_physics_double_fits_float(y) ||
        !henka_physics_double_fits_float(z))
    {
        return false;
    }
    *out_value = (henka_vec3){(float)x, (float)y, (float)z};
    return true;
}

static bool henka_physics_try_scale_vec3(
    henka_vec3 value,
    double scale,
    henka_vec3* out_value)
{
    return isfinite(scale) && henka_physics_try_vec3(
        (double)value.x * scale,
        (double)value.y * scale,
        (double)value.z * scale,
        out_value);
}

static bool henka_physics_try_add_scaled_vec3(
    henka_vec3 value,
    henka_vec3 addend,
    double scale,
    henka_vec3* out_value)
{
    return isfinite(scale) && henka_physics_try_vec3(
        (double)value.x + (double)addend.x * scale,
        (double)value.y + (double)addend.y * scale,
        (double)value.z + (double)addend.z * scale,
        out_value);
}

static bool henka_physics_quaternion_valid(henka_quat value)
{
    double maximum;
    double x;
    double y;
    double z;
    double w;
    double norm;

    if (!isfinite(value.x) || !isfinite(value.y) ||
        !isfinite(value.z) || !isfinite(value.w))
    {
        return false;
    }
    maximum = fmax(fabs((double)value.x), fmax(
        fabs((double)value.y), fmax(
            fabs((double)value.z), fabs((double)value.w))));
    if (maximum <= 0.000001)
    {
        return false;
    }
    x = (double)value.x / maximum;
    y = (double)value.y / maximum;
    z = (double)value.z / maximum;
    w = (double)value.w / maximum;
    norm = sqrt(x * x + y * y + z * z + w * w) * maximum;
    return isfinite(norm) && norm > 0.000001;
}

static bool henka_physics_quaternion_normalized(henka_quat value)
{
    double norm;

    if (!henka_physics_quaternion_valid(value))
    {
        return false;
    }
    norm = hypot(
        hypot((double)value.x, (double)value.y),
        hypot((double)value.z, (double)value.w));
    return isfinite(norm) && fabs(norm - 1.0) <= 0.001;
}

static bool henka_physics_try_add_vec3(henka_vec3 left, henka_vec3 right, henka_vec3* out_value)
{
    henka_vec3 result;

    if (out_value == NULL)
    {
        return false;
    }

    result = henka_vec3_add(left, right);
    if (!henka_physics_is_finite_vec3(result))
    {
        return false;
    }

    *out_value = result;
    return true;
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

static henka_vec3 henka_physics_plane_world_normal(const henka_physics_body_state* body)
{
    henka_vec3 local_normal;

    local_normal = henka_vec3_normalize(body->collider.data.plane.normal);
    return henka_vec3_normalize(henka_quat_rotate_vec3(body->transform.rotation, local_normal));
}

static float henka_physics_plane_world_offset(
    const henka_physics_body_state* body,
    henka_vec3 normalized_world_normal)
{
    return body->collider.data.plane.offset +
        henka_vec3_dot(normalized_world_normal, henka_physics_collider_center(body));
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

static bool henka_physics_capsule_transform_is_upright(henka_transform transform)
{
    henka_vec3 axis = henka_vec3_normalize(henka_quat_rotate_vec3(
        transform.rotation, (henka_vec3){0.0f, 1.0f, 0.0f}));

    return henka_vec3_length(axis) > 0.0001f &&
        henka_physics_abs(axis.x) <= 0.0001f &&
        henka_physics_abs(axis.z) <= 0.0001f &&
        henka_physics_abs(henka_physics_abs(axis.y) - 1.0f) <= 0.0001f;
}

static float henka_physics_capsule_radius(const henka_physics_body_state* body)
{
    henka_vec3 scale = henka_physics_abs_vec3(body->transform.scale);
    float maximum = scale.x > scale.z ? scale.x : scale.z;
    return body->collider.data.capsule.radius * maximum;
}

static float henka_physics_capsule_half_height(const henka_physics_body_state* body)
{
    return body->collider.data.capsule.half_height *
        henka_physics_abs(body->transform.scale.y);
}

static bool henka_physics_heightfield_dimensions_valid(
    const henka_physics_collider_desc* collider,
    size_t* out_sample_count)
{
    size_t sample_count;

    if (collider == NULL || out_sample_count == NULL ||
        collider->data.heightfield.samples_x < 2U ||
        collider->data.heightfield.samples_z < 2U ||
        collider->data.heightfield.samples_x > 4096U ||
        collider->data.heightfield.samples_z > 4096U ||
        collider->data.heightfield.heights_millimeters == NULL ||
        !isfinite(collider->data.heightfield.cell_spacing) ||
        collider->data.heightfield.cell_spacing <= 0.0f ||
        !henka_physics_is_finite_vec3(collider->data.heightfield.origin))
    {
        return false;
    }
    if (!henka_checked_size_multiply(
            (size_t)collider->data.heightfield.samples_x,
            (size_t)collider->data.heightfield.samples_z,
            &sample_count))
    {
        return false;
    }
    *out_sample_count = sample_count;
    return true;
}

static bool henka_physics_transform_supports_heightfield(henka_transform transform)
{
    return fabsf(transform.scale.x - 1.0f) <= 0.0001f &&
        fabsf(transform.scale.y - 1.0f) <= 0.0001f &&
        fabsf(transform.scale.z - 1.0f) <= 0.0001f &&
        fabsf(transform.rotation.x) <= 0.0001f &&
        fabsf(transform.rotation.y) <= 0.0001f &&
        fabsf(transform.rotation.z) <= 0.0001f &&
        fabsf(transform.rotation.w - 1.0f) <= 0.0001f;
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
        case HENKA_PHYSICS_SHAPE_CAPSULE:
            return isfinite(collider.data.capsule.radius) &&
                collider.data.capsule.radius > 0.0f &&
                isfinite(collider.data.capsule.half_height) &&
                collider.data.capsule.half_height >= 0.0f;
        case HENKA_PHYSICS_SHAPE_BOX:
            return henka_physics_is_finite_vec3(collider.data.box.half_extents) &&
                collider.data.box.half_extents.x > 0.0f && collider.data.box.half_extents.y > 0.0f &&
                collider.data.box.half_extents.z > 0.0f;
        case HENKA_PHYSICS_SHAPE_PLANE:
            return henka_physics_is_finite_vec3(collider.data.plane.normal) &&
                henka_vec3_length(collider.data.plane.normal) > 0.0001f && isfinite(collider.data.plane.offset);
        case HENKA_PHYSICS_SHAPE_HEIGHTFIELD:
            return henka_physics_heightfield_dimensions_valid(&collider, &(size_t){0});
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
        henka_physics_quaternion_valid(transform.rotation);
}

static bool henka_physics_double_fits_float(double value)
{
    return isfinite(value) && value >= -(double)FLT_MAX && value <= (double)FLT_MAX;
}

static bool henka_physics_geometry_valid(
    henka_transform transform,
    henka_physics_collider_desc collider)
{
    double center_x;
    double center_y;
    double center_z;
    double extent_x;
    double extent_y;
    double extent_z;

    center_x = (double)transform.position.x + (double)collider.offset.x;
    center_y = (double)transform.position.y + (double)collider.offset.y;
    center_z = (double)transform.position.z + (double)collider.offset.z;
    if (!henka_physics_double_fits_float(center_x) ||
        !henka_physics_double_fits_float(center_y) ||
        !henka_physics_double_fits_float(center_z))
    {
        return false;
    }

    switch (collider.shape)
    {
        case HENKA_PHYSICS_SHAPE_SPHERE:
        {
            double maximum_scale;

            maximum_scale = fmax(
                fabs((double)transform.scale.x),
                fmax(
                    fabs((double)transform.scale.y),
                    fabs((double)transform.scale.z)));
            extent_x = (double)collider.data.sphere.radius * maximum_scale;
            extent_y = extent_x;
            extent_z = extent_x;
            break;
        }

        case HENKA_PHYSICS_SHAPE_CAPSULE:
        {
            henka_vec3 scale;
            double maximum_horizontal_scale;
            double radius;
            double half_height;

            if (!henka_physics_capsule_transform_is_upright(transform))
            {
                return false;
            }
            scale = henka_physics_abs_vec3(transform.scale);
            maximum_horizontal_scale = scale.x > scale.z ? scale.x : scale.z;
            radius = (double)collider.data.capsule.radius * maximum_horizontal_scale;
            half_height = (double)collider.data.capsule.half_height * (double)scale.y;
            extent_x = radius;
            extent_z = radius;
            extent_y = radius + half_height;
            break;
        }

        case HENKA_PHYSICS_SHAPE_BOX:
            extent_x = (double)collider.data.box.half_extents.x *
                fabs((double)transform.scale.x);
            extent_y = (double)collider.data.box.half_extents.y *
                fabs((double)transform.scale.y);
            extent_z = (double)collider.data.box.half_extents.z *
                fabs((double)transform.scale.z);
            break;

        case HENKA_PHYSICS_SHAPE_PLANE:
        {
            double world_offset;
            henka_vec3 local_normal;
            henka_vec3 world_normal;

            local_normal = henka_vec3_normalize(collider.data.plane.normal);
            world_normal = henka_vec3_normalize(
                henka_quat_rotate_vec3(transform.rotation, local_normal));
            if (henka_vec3_length(local_normal) <= 0.0001f ||
                henka_vec3_length(world_normal) <= 0.0001f)
            {
                return false;
            }

            world_offset = (double)collider.data.plane.offset +
                (double)world_normal.x * center_x +
                (double)world_normal.y * center_y +
                (double)world_normal.z * center_z;
            return henka_physics_double_fits_float(world_offset);
        }

        case HENKA_PHYSICS_SHAPE_HEIGHTFIELD:
        {
            size_t sample_count;
            double width;
            double depth;

            if (!henka_physics_heightfield_dimensions_valid(&collider, &sample_count) ||
                !henka_physics_transform_supports_heightfield(transform))
            {
                return false;
            }
            (void)sample_count;
            width = (double)(collider.data.heightfield.samples_x - 1U) *
                (double)collider.data.heightfield.cell_spacing;
            depth = (double)(collider.data.heightfield.samples_z - 1U) *
                (double)collider.data.heightfield.cell_spacing;
            return henka_physics_double_fits_float(width) && width > 0.0 &&
                henka_physics_double_fits_float(depth) && depth > 0.0 &&
                henka_physics_double_fits_float(
                    center_x + (double)collider.data.heightfield.origin.x + width) &&
                henka_physics_double_fits_float(
                    center_z + (double)collider.data.heightfield.origin.z + depth) &&
                henka_physics_double_fits_float(
                    center_y + (double)collider.data.heightfield.origin.y);
        }

        default:
            return false;
    }

    return extent_x >= (double)FLT_MIN &&
        extent_y >= (double)FLT_MIN &&
        extent_z >= (double)FLT_MIN &&
        henka_physics_double_fits_float(extent_x) &&
        henka_physics_double_fits_float(extent_y) &&
        henka_physics_double_fits_float(extent_z) &&
        henka_physics_double_fits_float(center_x - extent_x) &&
        henka_physics_double_fits_float(center_x + extent_x) &&
        henka_physics_double_fits_float(center_y - extent_y) &&
        henka_physics_double_fits_float(center_y + extent_y) &&
        henka_physics_double_fits_float(center_z - extent_z) &&
        henka_physics_double_fits_float(center_z + extent_z);
}

static bool henka_physics_body_candidate_valid(
    const henka_physics_body_record* body)
{
    return body != NULL && body->active &&
        body->state.type >= HENKA_PHYSICS_BODY_STATIC &&
        body->state.type <= HENKA_PHYSICS_BODY_KINEMATIC &&
        henka_physics_transform_valid(body->state.transform) &&
        henka_physics_transform_valid(body->state.initial_transform) &&
        henka_physics_quaternion_normalized(
            body->state.transform.rotation) &&
        henka_physics_quaternion_normalized(
            body->state.initial_transform.rotation) &&
        henka_physics_is_finite_vec3(body->state.linear_velocity) &&
        henka_physics_is_finite_vec3(body->state.angular_velocity) &&
        henka_physics_is_finite_vec3(body->force) &&
        henka_physics_is_finite_vec3(body->torque) &&
        henka_physics_material_valid(body->state.material) &&
        henka_physics_collider_valid(body->state.collider) &&
        (body->state.type != HENKA_PHYSICS_BODY_DYNAMIC ||
            (isfinite(body->state.mass) && body->state.mass > 0.0f &&
                isfinite(1.0f / body->state.mass))) &&
        henka_physics_geometry_valid(
            body->state.transform,
            body->state.collider) &&
        (body->state.collider.shape != HENKA_PHYSICS_SHAPE_HEIGHTFIELD ||
            (body->owned_heightfield_heights_millimeters != NULL &&
                body->owned_heightfield_sample_count > 0U &&
                body->state.collider.data.heightfield.heights_millimeters ==
                    body->owned_heightfield_heights_millimeters));
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

static bool henka_physics_contact_involves_body(
    henka_physics_contact contact,
    henka_physics_body_id body)
{
    return contact.body_a == body || contact.body_b == body;
}

static bool henka_physics_contact_involves_pair(
    henka_physics_contact contact,
    henka_physics_body_id first,
    henka_physics_body_id second)
{
    return (contact.body_a == first && contact.body_b == second) ||
        (contact.body_a == second && contact.body_b == first);
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

typedef enum henka_physics_contact_status
{
    HENKA_PHYSICS_CONTACT_NONE = 0,
    HENKA_PHYSICS_CONTACT_FOUND,
    HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE
} henka_physics_contact_status;

static bool henka_physics_contact_valid(const henka_physics_contact* contact)
{
    double normal_length;

    if (contact == NULL || !henka_physics_is_finite_vec3(contact->normal) ||
        !henka_physics_is_finite_vec3(contact->point) ||
        !isfinite(contact->penetration) || contact->penetration < 0.0f)
    {
        return false;
    }
    normal_length = hypot(
        hypot((double)contact->normal.x, (double)contact->normal.y),
        (double)contact->normal.z);
    return isfinite(normal_length) && fabs(normal_length - 1.0) <= 0.001;
}

static henka_physics_contact_status henka_physics_sphere_sphere(
    const henka_physics_body_state* a,
    const henka_physics_body_state* b,
    henka_physics_contact* contact)
{
    henka_vec3 a_center = henka_physics_collider_center(a);
    henka_vec3 b_center = henka_physics_collider_center(b);
    double dx = (double)b_center.x - (double)a_center.x;
    double dy = (double)b_center.y - (double)a_center.y;
    double dz = (double)b_center.z - (double)a_center.z;
    double radius_a = (double)henka_physics_sphere_radius(a);
    double radius_sum = radius_a + (double)henka_physics_sphere_radius(b);
    double distance = hypot(hypot(dx, dy), dz);
    if (distance >= radius_sum)
    {
        return HENKA_PHYSICS_CONTACT_NONE;
    }
    if (distance > 0.0001)
    {
        if (!henka_physics_try_vec3(dx / distance, dy / distance, dz / distance, &contact->normal))
        {
            return HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE;
        }
    }
    else
    {
        contact->normal = (henka_vec3){1.0f, 0.0f, 0.0f};
    }
    if (!henka_physics_try_float(radius_sum - distance, &contact->penetration) ||
        !henka_physics_try_vec3(
            (double)a_center.x + (double)contact->normal.x * radius_a,
            (double)a_center.y + (double)contact->normal.y * radius_a,
            (double)a_center.z + (double)contact->normal.z * radius_a,
            &contact->point))
    {
        return HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE;
    }
    return HENKA_PHYSICS_CONTACT_FOUND;
}

static henka_physics_contact_status henka_physics_box_box(
    const henka_physics_body_state* a,
    const henka_physics_body_state* b,
    henka_physics_contact* contact)
{
    henka_vec3 a_center = henka_physics_collider_center(a);
    henka_vec3 b_center = henka_physics_collider_center(b);
    henka_vec3 a_extents = henka_physics_box_extents(a);
    henka_vec3 b_extents = henka_physics_box_extents(b);
    double dx = (double)b_center.x - (double)a_center.x;
    double dy = (double)b_center.y - (double)a_center.y;
    double dz = (double)b_center.z - (double)a_center.z;
    double x = (double)a_extents.x + (double)b_extents.x - fabs(dx);
    double y = (double)a_extents.y + (double)b_extents.y - fabs(dy);
    double z = (double)a_extents.z + (double)b_extents.z - fabs(dz);
    double penetration;
    henka_vec3 normal;
    if (x <= 0.0 || y <= 0.0 || z <= 0.0)
    {
        return HENKA_PHYSICS_CONTACT_NONE;
    }
    penetration = x;
    normal = (henka_vec3){dx < 0.0 ? -1.0f : 1.0f, 0.0f, 0.0f};
    if (y < penetration)
    {
        penetration = y;
        normal = (henka_vec3){0.0f, dy < 0.0 ? -1.0f : 1.0f, 0.0f};
    }
    if (z < penetration)
    {
        penetration = z;
        normal = (henka_vec3){0.0f, 0.0f, dz < 0.0 ? -1.0f : 1.0f};
    }
    contact->normal = normal;
    if (!henka_physics_try_float(penetration, &contact->penetration) ||
        !henka_physics_try_vec3(
            (double)a_center.x + (double)normal.x * penetration * 0.5,
            (double)a_center.y + (double)normal.y * penetration * 0.5,
            (double)a_center.z + (double)normal.z * penetration * 0.5,
            &contact->point))
    {
        return HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE;
    }
    return HENKA_PHYSICS_CONTACT_FOUND;
}

static henka_physics_contact_status henka_physics_sphere_box(
    const henka_physics_body_state* sphere,
    const henka_physics_body_state* box,
    henka_physics_contact* contact)
{
    henka_vec3 sphere_center = henka_physics_collider_center(sphere);
    henka_vec3 box_center = henka_physics_collider_center(box);
    henka_vec3 extents = henka_physics_box_extents(box);
    double rx = (double)sphere_center.x - (double)box_center.x;
    double ry = (double)sphere_center.y - (double)box_center.y;
    double rz = (double)sphere_center.z - (double)box_center.z;
    double cx = fmax(-(double)extents.x, fmin(rx, (double)extents.x));
    double cy = fmax(-(double)extents.y, fmin(ry, (double)extents.y));
    double cz = fmax(-(double)extents.z, fmin(rz, (double)extents.z));
    double wx = (double)box_center.x + cx;
    double wy = (double)box_center.y + cy;
    double wz = (double)box_center.z + cz;
    double dx = (double)sphere_center.x - wx;
    double dy = (double)sphere_center.y - wy;
    double dz = (double)sphere_center.z - wz;
    double radius = (double)henka_physics_sphere_radius(sphere);
    double distance = hypot(hypot(dx, dy), dz);
    double penetration;
    if (distance >= radius)
    {
        return HENKA_PHYSICS_CONTACT_NONE;
    }
    if (distance > 0.0001)
    {
        if (!henka_physics_try_vec3(-dx / distance, -dy / distance, -dz / distance, &contact->normal))
        {
            return HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE;
        }
        penetration = radius - distance;
    }
    else
    {
        double px = (double)extents.x - fabs(rx);
        double py = (double)extents.y - fabs(ry);
        double pz = (double)extents.z - fabs(rz);
        penetration = radius + px;
        contact->normal = (henka_vec3){rx < 0.0 ? 1.0f : -1.0f, 0.0f, 0.0f};
        if (py < px && py <= pz)
        {
            penetration = radius + py;
            contact->normal = (henka_vec3){0.0f, ry < 0.0 ? 1.0f : -1.0f, 0.0f};
        }
        else if (pz < px)
        {
            penetration = radius + pz;
            contact->normal = (henka_vec3){0.0f, 0.0f, rz < 0.0 ? 1.0f : -1.0f};
        }
    }
    if (!henka_physics_try_float(penetration, &contact->penetration) ||
        !henka_physics_try_vec3(wx, wy, wz, &contact->point))
    {
        return HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE;
    }
    return HENKA_PHYSICS_CONTACT_FOUND;
}

static float henka_physics_capsule_like_radius(
    const henka_physics_body_state* body)
{
    return body->collider.shape == HENKA_PHYSICS_SHAPE_CAPSULE ?
        henka_physics_capsule_radius(body) : henka_physics_sphere_radius(body);
}

static float henka_physics_capsule_like_half_height(
    const henka_physics_body_state* body)
{
    return body->collider.shape == HENKA_PHYSICS_SHAPE_CAPSULE ?
        henka_physics_capsule_half_height(body) : 0.0f;
}

static henka_physics_contact_status henka_physics_capsule_capsule(
    const henka_physics_body_state* a,
    const henka_physics_body_state* b,
    henka_physics_contact* contact)
{
    henka_vec3 a_center = henka_physics_collider_center(a);
    henka_vec3 b_center = henka_physics_collider_center(b);
    double a_radius = (double)henka_physics_capsule_like_radius(a);
    double b_radius = (double)henka_physics_capsule_like_radius(b);
    double a_half_height = (double)henka_physics_capsule_like_half_height(a);
    double b_half_height = (double)henka_physics_capsule_like_half_height(b);
    double a_bottom = (double)a_center.y - a_half_height;
    double a_top = (double)a_center.y + a_half_height;
    double b_bottom = (double)b_center.y - b_half_height;
    double b_top = (double)b_center.y + b_half_height;
    double a_y;
    double b_y;
    double dx;
    double dy;
    double dz;
    double distance;
    double radius_sum = a_radius + b_radius;

    if (a_top < b_bottom)
    {
        a_y = a_top;
        b_y = b_bottom;
    }
    else if (b_top < a_bottom)
    {
        a_y = a_bottom;
        b_y = b_top;
    }
    else
    {
        double overlap_bottom = a_bottom > b_bottom ? a_bottom : b_bottom;
        double overlap_top = a_top < b_top ? a_top : b_top;
        a_y = (overlap_bottom + overlap_top) * 0.5;
        b_y = a_y;
    }
    dx = (double)b_center.x - (double)a_center.x;
    dy = b_y - a_y;
    dz = (double)b_center.z - (double)a_center.z;
    distance = hypot(hypot(dx, dy), dz);
    if (distance >= radius_sum)
    {
        return HENKA_PHYSICS_CONTACT_NONE;
    }
    if (distance > 0.0001)
    {
        if (!henka_physics_try_vec3(
                dx / distance, dy / distance, dz / distance,
                &contact->normal))
        {
            return HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE;
        }
    }
    else
    {
        contact->normal = (henka_vec3){1.0f, 0.0f, 0.0f};
    }
    if (!henka_physics_try_float(radius_sum - distance, &contact->penetration) ||
        !henka_physics_try_vec3(
            (double)a_center.x + (double)contact->normal.x * a_radius,
            a_y + (double)contact->normal.y * a_radius,
            (double)a_center.z + (double)contact->normal.z * a_radius,
            &contact->point))
    {
        return HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE;
    }
    return HENKA_PHYSICS_CONTACT_FOUND;
}

static henka_physics_contact_status henka_physics_capsule_box(
    const henka_physics_body_state* capsule,
    const henka_physics_body_state* box,
    henka_physics_contact* contact)
{
    henka_vec3 capsule_center = henka_physics_collider_center(capsule);
    henka_vec3 box_center = henka_physics_collider_center(box);
    henka_vec3 extents = henka_physics_box_extents(box);
    double radius = (double)henka_physics_capsule_radius(capsule);
    double half_height = (double)henka_physics_capsule_half_height(capsule);
    double capsule_bottom = (double)capsule_center.y - half_height;
    double capsule_top = (double)capsule_center.y + half_height;
    double box_min_x = (double)box_center.x - (double)extents.x;
    double box_max_x = (double)box_center.x + (double)extents.x;
    double box_min_y = (double)box_center.y - (double)extents.y;
    double box_max_y = (double)box_center.y + (double)extents.y;
    double box_min_z = (double)box_center.z - (double)extents.z;
    double box_max_z = (double)box_center.z + (double)extents.z;
    double capsule_point_y;
    double box_point_y;
    double capsule_point_x = (double)capsule_center.x;
    double box_point_x = fmax(box_min_x, fmin((double)capsule_center.x, box_max_x));
    double capsule_point_z = (double)capsule_center.z;
    double box_point_z = fmax(box_min_z, fmin((double)capsule_center.z, box_max_z));
    double dx;
    double dy;
    double dz;
    double distance;
    double penetration;

    if (capsule_top < box_min_y)
    {
        capsule_point_y = capsule_top;
        box_point_y = box_min_y;
    }
    else if (capsule_bottom > box_max_y)
    {
        capsule_point_y = capsule_bottom;
        box_point_y = box_max_y;
    }
    else
    {
        capsule_point_y = fmax(
            capsule_bottom,
            fmin((double)box_center.y, capsule_top));
        box_point_y = capsule_point_y;
    }
    dx = capsule_point_x - box_point_x;
    dy = capsule_point_y - box_point_y;
    dz = capsule_point_z - box_point_z;
    distance = hypot(hypot(dx, dy), dz);
    if (distance >= radius)
    {
        return HENKA_PHYSICS_CONTACT_NONE;
    }
    if (distance > 0.0001)
    {
        if (!henka_physics_try_vec3(
                -dx / distance, -dy / distance, -dz / distance,
                &contact->normal))
        {
            return HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE;
        }
        penetration = radius - distance;
    }
    else
    {
        double distance_x = (double)extents.x - fabs((double)capsule_center.x - (double)box_center.x);
        double distance_y = (double)extents.y - fabs(capsule_point_y - (double)box_center.y);
        double distance_z = (double)extents.z - fabs((double)capsule_center.z - (double)box_center.z);

        penetration = radius + distance_x;
        contact->normal = (henka_vec3){
            capsule_center.x < box_center.x ? 1.0f : -1.0f, 0.0f, 0.0f};
        if (distance_y < distance_x && distance_y <= distance_z)
        {
            penetration = radius + distance_y;
            contact->normal = (henka_vec3){
                0.0f, capsule_point_y < box_center.y ? 1.0f : -1.0f, 0.0f};
        }
        else if (distance_z < distance_x)
        {
            penetration = radius + distance_z;
            contact->normal = (henka_vec3){
                0.0f, 0.0f, capsule_center.z < box_center.z ? 1.0f : -1.0f};
        }
    }
    if (!henka_physics_try_float(penetration, &contact->penetration) ||
        !henka_physics_try_vec3(
            box_point_x, box_point_y, box_point_z, &contact->point))
    {
        return HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE;
    }
    return HENKA_PHYSICS_CONTACT_FOUND;
}

static henka_physics_contact_status henka_physics_shape_plane(
    const henka_physics_body_state* shape,
    const henka_physics_body_state* plane,
    henka_physics_contact* contact)
{
    henka_vec3 normal = henka_physics_plane_world_normal(plane);
    henka_vec3 center = henka_physics_collider_center(shape);
    double plane_center_x = (double)plane->transform.position.x + (double)plane->collider.offset.x;
    double plane_center_y = (double)plane->transform.position.y + (double)plane->collider.offset.y;
    double plane_center_z = (double)plane->transform.position.z + (double)plane->collider.offset.z;
    double offset = (double)plane->collider.data.plane.offset +
        (double)normal.x * plane_center_x +
        (double)normal.y * plane_center_y +
        (double)normal.z * plane_center_z;
    double radius;
    double distance;
    if (shape->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE)
    {
        radius = (double)henka_physics_sphere_radius(shape);
    }
    else if (shape->collider.shape == HENKA_PHYSICS_SHAPE_CAPSULE)
    {
        radius = (double)henka_physics_capsule_radius(shape) +
            (double)henka_physics_capsule_half_height(shape) * henka_physics_abs(normal.y);
    }
    else if (shape->collider.shape == HENKA_PHYSICS_SHAPE_BOX)
    {
        henka_vec3 extents = henka_physics_box_extents(shape);
        radius = fabs((double)normal.x) * (double)extents.x +
            fabs((double)normal.y) * (double)extents.y +
            fabs((double)normal.z) * (double)extents.z;
    }
    else
    {
        return HENKA_PHYSICS_CONTACT_NONE;
    }
    distance = (double)normal.x * (double)center.x +
        (double)normal.y * (double)center.y +
        (double)normal.z * (double)center.z - offset;
    if (distance >= radius)
    {
        return HENKA_PHYSICS_CONTACT_NONE;
    }
    contact->normal = (henka_vec3){-normal.x, -normal.y, -normal.z};
    if (!henka_physics_try_float(radius - distance, &contact->penetration) ||
        !henka_physics_try_vec3(
            (double)center.x - (double)normal.x * distance,
            (double)center.y - (double)normal.y * distance,
            (double)center.z - (double)normal.z * distance,
            &contact->point))
    {
        return HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE;
    }
    return HENKA_PHYSICS_CONTACT_FOUND;
}

static bool henka_physics_heightfield_sample(
    const henka_physics_body_state* body,
    float world_x,
    float world_z,
    float* out_height,
    henka_vec3* out_normal)
{
    const uint32_t samples_x = body->collider.data.heightfield.samples_x;
    const uint32_t samples_z = body->collider.data.heightfield.samples_z;
    const float spacing = body->collider.data.heightfield.cell_spacing;
    const henka_vec3 origin = henka_vec3_add(
        henka_physics_collider_center(body),
        body->collider.data.heightfield.origin);
    const int32_t* heights = body->collider.data.heightfield.heights_millimeters;
    const float width = (float)(samples_x - 1U) * spacing;
    const float depth = (float)(samples_z - 1U) * spacing;
    float local_x;
    float local_z;
    float grid_x;
    float grid_z;
    uint32_t x;
    uint32_t z;
    float tx;
    float tz;
    float h00;
    float h10;
    float h01;
    float h11;
    float left;
    float right;
    float down;
    float up;
    float normal_length;

    if (out_height == NULL || out_normal == NULL ||
        world_x < origin.x || world_x > origin.x + width ||
        world_z < origin.z || world_z > origin.z + depth)
    {
        return false;
    }
    local_x = (world_x - origin.x) / spacing;
    local_z = (world_z - origin.z) / spacing;
    grid_x = floorf(local_x);
    grid_z = floorf(local_z);
    x = (uint32_t)grid_x;
    z = (uint32_t)grid_z;
    if (x + 1U >= samples_x) x = samples_x - 2U;
    if (z + 1U >= samples_z) z = samples_z - 2U;
    tx = local_x - (float)x;
    tz = local_z - (float)z;
    h00 = (float)heights[z * samples_x + x] / 1000.0f + origin.y;
    h10 = (float)heights[z * samples_x + x + 1U] / 1000.0f + origin.y;
    h01 = (float)heights[(z + 1U) * samples_x + x] / 1000.0f + origin.y;
    h11 = (float)heights[(z + 1U) * samples_x + x + 1U] / 1000.0f + origin.y;
    *out_height = h00 + (h10 - h00) * tx + (h01 - h00) * tz +
        (h00 - h10 - h01 + h11) * tx * tz;

    left = (float)heights[z * samples_x + (x > 0U ? x - 1U : x)] / 1000.0f + origin.y;
    right = (float)heights[z * samples_x + (x + 1U < samples_x ? x + 1U : x)] / 1000.0f + origin.y;
    down = (float)heights[(z > 0U ? z - 1U : z) * samples_x + x] / 1000.0f + origin.y;
    up = (float)heights[(z + 1U < samples_z ? z + 1U : z) * samples_x + x] / 1000.0f + origin.y;
    *out_normal = henka_vec3_normalize((henka_vec3){left - right, 2.0f * spacing, down - up});
    normal_length = henka_vec3_length(*out_normal);
    return isfinite(*out_height) && isfinite(normal_length) && normal_length > 0.0001f;
}

static henka_physics_contact_status henka_physics_shape_heightfield(
    const henka_physics_body_state* shape,
    const henka_physics_body_state* heightfield,
    henka_physics_contact* contact)
{
    henka_vec3 center = henka_physics_collider_center(shape);
    henka_vec3 normal = {0.0f, 1.0f, 0.0f};
    float surface_height = -FLT_MAX;
    float gap;
    float support;
    float height;
    size_t sample_index;
    henka_vec3 sample_normal;

    if (shape->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE)
    {
        if (!henka_physics_heightfield_sample(
                heightfield, center.x, center.z, &surface_height, &normal))
        {
            return HENKA_PHYSICS_CONTACT_NONE;
        }
        support = henka_physics_sphere_radius(shape);
    }
    else if (shape->collider.shape == HENKA_PHYSICS_SHAPE_CAPSULE)
    {
        if (!henka_physics_heightfield_sample(
                heightfield, center.x, center.z, &surface_height, &normal))
        {
            return HENKA_PHYSICS_CONTACT_NONE;
        }
        support = henka_physics_capsule_radius(shape) +
            henka_physics_capsule_half_height(shape) * henka_physics_abs(normal.y);
    }
    else if (shape->collider.shape == HENKA_PHYSICS_SHAPE_BOX)
    {
        const henka_vec3 extents = henka_physics_box_extents(shape);
        const float offsets_x[2] = {-extents.x, extents.x};
        const float offsets_z[2] = {-extents.z, extents.z};
        bool found = false;
        for (sample_index = 0U; sample_index < 4U; ++sample_index)
        {
            const float sample_x = center.x + offsets_x[sample_index & 1U];
            const float sample_z = center.z + offsets_z[(sample_index >> 1U) & 1U];
            if (henka_physics_heightfield_sample(
                    heightfield, sample_x, sample_z, &height, &sample_normal) &&
                (!found || height > surface_height))
            {
                surface_height = height;
                normal = sample_normal;
                found = true;
            }
        }
        if (!found)
        {
            return HENKA_PHYSICS_CONTACT_NONE;
        }
        support = fabsf(normal.x) * extents.x + fabsf(normal.y) * extents.y +
            fabsf(normal.z) * extents.z;
    }
    else
    {
        return HENKA_PHYSICS_CONTACT_NONE;
    }
    gap = center.y - surface_height;
    if (!isfinite(gap) || gap >= support)
    {
        return HENKA_PHYSICS_CONTACT_NONE;
    }
    contact->normal = henka_vec3_scale(normal, -1.0f);
    if (!henka_physics_try_float(support - gap, &contact->penetration) ||
        !henka_physics_try_vec3(
            (double)center.x - (double)normal.x * (double)gap,
            (double)center.y - (double)normal.y * (double)gap,
            (double)center.z - (double)normal.z * (double)gap,
            &contact->point))
    {
        return HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE;
    }
    return HENKA_PHYSICS_CONTACT_FOUND;
}

static henka_physics_contact_status henka_physics_detect_contact(
    const henka_physics_body_state* a,
    const henka_physics_body_state* b,
    henka_physics_contact* contact)
{
    henka_physics_contact_status status = HENKA_PHYSICS_CONTACT_NONE;
    henka_physics_contact swapped;
    *contact = (henka_physics_contact){a->id, b->id, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f, a->collider.is_trigger || b->collider.is_trigger};
    if (a->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE && b->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE)
    {
        status = henka_physics_sphere_sphere(a, b, contact);
    }
    else if (a->collider.shape == HENKA_PHYSICS_SHAPE_BOX && b->collider.shape == HENKA_PHYSICS_SHAPE_BOX)
    {
        status = henka_physics_box_box(a, b, contact);
    }
    else if (a->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE && b->collider.shape == HENKA_PHYSICS_SHAPE_BOX)
    {
        status = henka_physics_sphere_box(a, b, contact);
    }
    else if (a->collider.shape == HENKA_PHYSICS_SHAPE_BOX && b->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE)
    {
        swapped = *contact;
        swapped.body_a = b->id;
        swapped.body_b = a->id;
        status = henka_physics_sphere_box(b, a, &swapped);
        if (status == HENKA_PHYSICS_CONTACT_FOUND)
        {
            contact->normal = henka_vec3_scale(swapped.normal, -1.0f);
            contact->penetration = swapped.penetration;
            contact->point = swapped.point;
        }
    }
    else if ((a->collider.shape == HENKA_PHYSICS_SHAPE_CAPSULE ||
              a->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE) &&
             (b->collider.shape == HENKA_PHYSICS_SHAPE_CAPSULE ||
              b->collider.shape == HENKA_PHYSICS_SHAPE_SPHERE))
    {
        status = henka_physics_capsule_capsule(a, b, contact);
    }
    else if (a->collider.shape == HENKA_PHYSICS_SHAPE_CAPSULE &&
             b->collider.shape == HENKA_PHYSICS_SHAPE_BOX)
    {
        status = henka_physics_capsule_box(a, b, contact);
    }
    else if (a->collider.shape == HENKA_PHYSICS_SHAPE_BOX &&
             b->collider.shape == HENKA_PHYSICS_SHAPE_CAPSULE)
    {
        swapped = *contact;
        swapped.body_a = b->id;
        swapped.body_b = a->id;
        status = henka_physics_capsule_box(b, a, &swapped);
        if (status == HENKA_PHYSICS_CONTACT_FOUND)
        {
            contact->normal = henka_vec3_scale(swapped.normal, -1.0f);
            contact->penetration = swapped.penetration;
            contact->point = swapped.point;
        }
    }
    else if (b->collider.shape == HENKA_PHYSICS_SHAPE_PLANE)
    {
        status = henka_physics_shape_plane(a, b, contact);
    }
    else if (a->collider.shape == HENKA_PHYSICS_SHAPE_PLANE)
    {
        swapped = *contact;
        swapped.body_a = b->id;
        swapped.body_b = a->id;
        status = henka_physics_shape_plane(b, a, &swapped);
        if (status == HENKA_PHYSICS_CONTACT_FOUND)
        {
            contact->normal = henka_vec3_scale(swapped.normal, -1.0f);
            contact->penetration = swapped.penetration;
            contact->point = swapped.point;
        }
    }
    else if (b->collider.shape == HENKA_PHYSICS_SHAPE_HEIGHTFIELD)
    {
        status = henka_physics_shape_heightfield(a, b, contact);
    }
    else if (a->collider.shape == HENKA_PHYSICS_SHAPE_HEIGHTFIELD)
    {
        swapped = *contact;
        swapped.body_a = b->id;
        swapped.body_b = a->id;
        status = henka_physics_shape_heightfield(b, a, &swapped);
        if (status == HENKA_PHYSICS_CONTACT_FOUND)
        {
            contact->normal = henka_vec3_scale(swapped.normal, -1.0f);
            contact->penetration = swapped.penetration;
            contact->point = swapped.point;
        }
    }
    if (status == HENKA_PHYSICS_CONTACT_FOUND &&
        !henka_physics_contact_valid(contact))
    {
        return HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE;
    }
    return status;
}

static bool henka_physics_resolve_contact(
    henka_physics_body_record* a,
    henka_physics_body_record* b,
    const henka_physics_contact* contact)
{
    double inverse_a = (double)henka_physics_inverse_mass(&a->state);
    double inverse_b = (double)henka_physics_inverse_mass(&b->state);
    double inverse_sum = inverse_a + inverse_b;
    henka_vec3 relative_velocity;
    double normal_speed;
    double restitution;
    double normal_impulse;
    henka_vec3 tangent;
    double tangent_length;
    double tangent_impulse;
    double static_friction;
    double friction;
    henka_vec3 next_a;
    henka_vec3 next_b;
    if (!isfinite(inverse_sum) || inverse_sum < 0.0 ||
        !henka_physics_contact_valid(contact))
    {
        return false;
    }
    if (inverse_sum <= 0.0 || contact->is_trigger)
    {
        return true;
    }
    {
        const double slop = 0.001;
        const double correction_percent = 0.75;
        double correction_depth = (double)contact->penetration > slop ?
            (double)contact->penetration - slop : 0.0;
        double correction_scale = correction_depth * correction_percent / inverse_sum;
        if (!isfinite(correction_scale) ||
            !henka_physics_try_add_scaled_vec3(
                a->state.transform.position,
                contact->normal,
                -correction_scale * inverse_a,
                &next_a) ||
            !henka_physics_try_add_scaled_vec3(
                b->state.transform.position,
                contact->normal,
                correction_scale * inverse_b,
                &next_b))
        {
            return false;
        }
        a->state.transform.position = next_a;
        b->state.transform.position = next_b;
    }
    if (!henka_physics_try_vec3(
            (double)b->state.linear_velocity.x - (double)a->state.linear_velocity.x,
            (double)b->state.linear_velocity.y - (double)a->state.linear_velocity.y,
            (double)b->state.linear_velocity.z - (double)a->state.linear_velocity.z,
            &relative_velocity))
    {
        return false;
    }
    normal_speed = (double)relative_velocity.x * (double)contact->normal.x +
        (double)relative_velocity.y * (double)contact->normal.y +
        (double)relative_velocity.z * (double)contact->normal.z;
    if (!isfinite(normal_speed))
    {
        return false;
    }
    if (normal_speed >= 0.0)
    {
        return true;
    }
    restitution = a->state.material.restitution > b->state.material.restitution ?
        a->state.material.restitution : b->state.material.restitution;
    normal_impulse = -(1.0 + restitution) * normal_speed / inverse_sum;
    if (!isfinite(normal_impulse) ||
        !henka_physics_try_add_scaled_vec3(
            a->state.linear_velocity,
            contact->normal,
            -normal_impulse * inverse_a,
            &next_a) ||
        !henka_physics_try_add_scaled_vec3(
            b->state.linear_velocity,
            contact->normal,
            normal_impulse * inverse_b,
            &next_b))
    {
        return false;
    }
    a->state.linear_velocity = next_a;
    b->state.linear_velocity = next_b;
    if (!henka_physics_try_vec3(
            (double)next_b.x - (double)next_a.x,
            (double)next_b.y - (double)next_a.y,
            (double)next_b.z - (double)next_a.z,
            &relative_velocity))
    {
        return false;
    }
    normal_speed = (double)relative_velocity.x * (double)contact->normal.x +
        (double)relative_velocity.y * (double)contact->normal.y +
        (double)relative_velocity.z * (double)contact->normal.z;
    if (!henka_physics_try_vec3(
            (double)relative_velocity.x - (double)contact->normal.x * normal_speed,
            (double)relative_velocity.y - (double)contact->normal.y * normal_speed,
            (double)relative_velocity.z - (double)contact->normal.z * normal_speed,
            &tangent))
    {
        return false;
    }
    tangent_length = hypot(hypot((double)tangent.x, (double)tangent.y), (double)tangent.z);
    if (!isfinite(tangent_length))
    {
        return false;
    }
    if (tangent_length <= 0.0001)
    {
        return true;
    }
    if (!henka_physics_try_scale_vec3(tangent, 1.0 / tangent_length, &tangent))
    {
        return false;
    }
    tangent_impulse = -(
        (double)relative_velocity.x * (double)tangent.x +
        (double)relative_velocity.y * (double)tangent.y +
        (double)relative_velocity.z * (double)tangent.z) / inverse_sum;
    static_friction = sqrt(
        (double)a->state.material.static_friction *
        (double)b->state.material.static_friction);
    friction = sqrt(
        (double)a->state.material.dynamic_friction *
        (double)b->state.material.dynamic_friction);
    if (!isfinite(tangent_impulse) || !isfinite(static_friction) || !isfinite(friction))
    {
        return false;
    }
    if (fabs(tangent_impulse) > normal_impulse * static_friction)
    {
        tangent_impulse = tangent_impulse < 0.0 ?
            -normal_impulse * friction : normal_impulse * friction;
    }
    if (!henka_physics_try_add_scaled_vec3(
            a->state.linear_velocity,
            tangent,
            -tangent_impulse * inverse_a,
            &next_a) ||
        !henka_physics_try_add_scaled_vec3(
            b->state.linear_velocity,
            tangent,
            tangent_impulse * inverse_b,
            &next_b))
    {
        return false;
    }
    a->state.linear_velocity = next_a;
    b->state.linear_velocity = next_b;
    return true;
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

static void henka_physics_release_candidate(henka_physics_world* candidate)
{
    size_t index;
    if (candidate == NULL)
    {
        return;
    }
    for (index = 0U; index < candidate->body_capacity; ++index)
    {
        henka_free(candidate->bodies[index].owned_heightfield_heights_millimeters);
        candidate->bodies[index].owned_heightfield_heights_millimeters = NULL;
    }
    henka_free(candidate->bodies);
    henka_free(candidate->contacts);
    henka_free(candidate->current_pairs);
    henka_free(candidate->previous_pairs);
    henka_free(candidate->events);
    candidate->bodies = NULL;
    candidate->contacts = NULL;
    candidate->current_pairs = NULL;
    candidate->previous_pairs = NULL;
    candidate->events = NULL;
}

static henka_result henka_physics_prepare_candidate(
    const henka_physics_world* world,
    bool preserve_events,
    henka_physics_world* candidate)
{
    size_t allocation_size;

    if (world == NULL || candidate == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *candidate = *world;
    candidate->bodies = NULL;
    candidate->contacts = NULL;
    candidate->contact_count = 0U;
    candidate->contact_capacity = 0U;
    candidate->current_pairs = NULL;
    candidate->current_pair_count = 0U;
    candidate->current_pair_capacity = 0U;
    candidate->previous_pairs = NULL;
    candidate->previous_pair_count = 0U;
    candidate->previous_pair_capacity = 0U;
    candidate->events = NULL;
    candidate->event_count = 0U;
    candidate->event_capacity = 0U;

    if (world->body_capacity > 0U)
    {
        if (!henka_checked_size_multiply(
                world->body_capacity,
                sizeof(*world->bodies),
                &allocation_size))
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        candidate->bodies = henka_malloc(allocation_size);
        if (candidate->bodies == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        memcpy(candidate->bodies, world->bodies, allocation_size);
        for (size_t index = 0U; index < world->body_capacity; ++index)
        {
            henka_physics_body_record* body = &candidate->bodies[index];
            size_t bytes;
            if (!body->active || body->state.collider.shape != HENKA_PHYSICS_SHAPE_HEIGHTFIELD)
            {
                continue;
            }
            if (!henka_checked_size_multiply(
                    body->owned_heightfield_sample_count,
                    sizeof(int32_t),
                    &bytes))
            {
                henka_physics_release_candidate(candidate);
                return HENKA_ERROR_OUT_OF_MEMORY;
            }
            body->owned_heightfield_heights_millimeters = henka_malloc(bytes);
            if (body->owned_heightfield_heights_millimeters == NULL)
            {
                henka_physics_release_candidate(candidate);
                return HENKA_ERROR_OUT_OF_MEMORY;
            }
            memcpy(
                body->owned_heightfield_heights_millimeters,
                world->bodies[index].owned_heightfield_heights_millimeters,
                bytes);
            body->state.collider.data.heightfield.heights_millimeters =
                body->owned_heightfield_heights_millimeters;
        }
    }

    if (world->previous_pair_count > 0U)
    {
        if (!henka_physics_reserve(
                (void**)&candidate->previous_pairs,
                sizeof(*candidate->previous_pairs),
                &candidate->previous_pair_capacity,
                world->previous_pair_count))
        {
            henka_physics_release_candidate(candidate);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        memcpy(
            candidate->previous_pairs,
            world->previous_pairs,
            sizeof(*candidate->previous_pairs) * world->previous_pair_count);
        candidate->previous_pair_count = world->previous_pair_count;
    }

    if (preserve_events && world->event_count > 0U)
    {
        if (!henka_physics_reserve(
                (void**)&candidate->events,
                sizeof(*candidate->events),
                &candidate->event_capacity,
                world->event_count))
        {
            henka_physics_release_candidate(candidate);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        memcpy(
            candidate->events,
            world->events,
            sizeof(*candidate->events) * world->event_count);
        candidate->event_count = world->event_count;
    }

    return HENKA_SUCCESS;
}

static henka_result henka_physics_simulate_candidate(
    henka_physics_world* world,
    float delta_seconds)
{
    size_t index;
    size_t other_index;
    for (index = 0U; index < world->body_capacity; ++index)
    {
        henka_physics_body_record* body = &world->bodies[index];
        double inverse_mass;
        if (!body->active)
        {
            continue;
        }
        if (!henka_physics_body_candidate_valid(body))
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        body->state.colliding = false;
        body->state.grounded = false;
        inverse_mass = (double)henka_physics_inverse_mass(&body->state);
        if (body->state.type == HENKA_PHYSICS_BODY_DYNAMIC)
        {
            henka_vec3 acceleration;
            henka_vec3 velocity;
            double linear_factor = 1.0 -
                (double)body->state.material.linear_damping *
                    (double)delta_seconds;
            double angular_factor = 1.0 -
                (double)body->state.material.angular_damping *
                    (double)delta_seconds;
            linear_factor = fmax(0.0, fmin(1.0, linear_factor));
            angular_factor = fmax(0.0, fmin(1.0, angular_factor));
            if (!henka_physics_try_vec3(
                    (double)world->gravity.x + (double)body->force.x * inverse_mass,
                    (double)world->gravity.y + (double)body->force.y * inverse_mass,
                    (double)world->gravity.z + (double)body->force.z * inverse_mass,
                    &acceleration) ||
                !henka_physics_try_add_scaled_vec3(
                    body->state.linear_velocity,
                    acceleration,
                    (double)delta_seconds,
                    &velocity) ||
                !henka_physics_try_scale_vec3(
                    velocity,
                    linear_factor,
                    &body->state.linear_velocity) ||
                !henka_physics_try_add_scaled_vec3(
                    body->state.angular_velocity,
                    body->torque,
                    inverse_mass * (double)delta_seconds,
                    &velocity) ||
                !henka_physics_try_scale_vec3(
                    velocity,
                    angular_factor,
                    &body->state.angular_velocity))
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
        }
        if (body->state.type == HENKA_PHYSICS_BODY_DYNAMIC || body->state.type == HENKA_PHYSICS_BODY_KINEMATIC)
        {
            double angular_speed;
            double angular_delta;
            if (!henka_physics_try_add_scaled_vec3(
                    body->state.transform.position,
                    body->state.linear_velocity,
                    (double)delta_seconds,
                    &body->state.transform.position))
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            angular_speed = hypot(
                hypot(
                    (double)body->state.angular_velocity.x,
                    (double)body->state.angular_velocity.y),
                (double)body->state.angular_velocity.z);
            angular_delta = angular_speed * (double)delta_seconds;
            if (!henka_physics_double_fits_float(angular_speed) ||
                !henka_physics_double_fits_float(angular_delta))
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            if (angular_speed > 0.0001)
            {
                henka_vec3 axis;
                henka_quat delta_rotation;
                henka_quat rotation;
                if (!henka_physics_try_scale_vec3(
                        body->state.angular_velocity,
                        1.0 / angular_speed,
                        &axis))
                {
                    return HENKA_ERROR_NUMERIC_RANGE;
                }
                delta_rotation = henka_quat_from_axis_angle(
                    axis,
                    (float)angular_delta);
                if (!henka_physics_quaternion_valid(delta_rotation))
                {
                    return HENKA_ERROR_NUMERIC_RANGE;
                }
                rotation = henka_quat_multiply(
                    delta_rotation,
                    body->state.transform.rotation);
                if (!henka_physics_quaternion_valid(rotation))
                {
                    return HENKA_ERROR_NUMERIC_RANGE;
                }
                body->state.transform.rotation = rotation;
            }
        }
        body->force = (henka_vec3){0.0f, 0.0f, 0.0f};
        body->torque = (henka_vec3){0.0f, 0.0f, 0.0f};
        if (!henka_physics_body_candidate_valid(body))
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
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
            henka_physics_contact_status contact_status;
            if (!b->active || (a->state.type == HENKA_PHYSICS_BODY_STATIC && b->state.type == HENKA_PHYSICS_BODY_STATIC) ||
                (a->state.collider.mask & b->state.collider.layer) == 0U ||
                (b->state.collider.mask & a->state.collider.layer) == 0U)
            {
                continue;
            }
            contact_status = henka_physics_detect_contact(
                &a->state,
                &b->state,
                &contact);
            if (contact_status == HENKA_PHYSICS_CONTACT_NUMERIC_FAILURE)
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            if (contact_status == HENKA_PHYSICS_CONTACT_FOUND)
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
                    if (!henka_physics_resolve_contact(a, b, &contact) ||
                        !henka_physics_body_candidate_valid(a) ||
                        !henka_physics_body_candidate_valid(b))
                    {
                        return HENKA_ERROR_NUMERIC_RANGE;
                    }
                }
            }
        }
    }
    if (!henka_physics_emit_events(world))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    return HENKA_SUCCESS;
}

static void henka_physics_commit_candidate(
    henka_physics_world* world,
    henka_physics_world* candidate)
{
    henka_physics_body_record* old_bodies;
    henka_physics_contact* old_contacts;
    henka_physics_pair* old_current_pairs;
    henka_physics_event* old_events;
    henka_physics_pair* old_previous_pairs;
    size_t old_body_capacity;
    size_t index;

    old_bodies = world->bodies;
    old_body_capacity = world->body_capacity;
    old_contacts = world->contacts;
    old_current_pairs = world->current_pairs;
    old_previous_pairs = world->previous_pairs;
    old_events = world->events;

    world->bodies = candidate->bodies;
    world->body_capacity = candidate->body_capacity;
    world->contacts = candidate->contacts;
    world->contact_count = candidate->contact_count;
    world->contact_capacity = candidate->contact_capacity;
    world->current_pairs = candidate->current_pairs;
    world->current_pair_count = candidate->current_pair_count;
    world->current_pair_capacity = candidate->current_pair_capacity;
    world->previous_pairs = candidate->previous_pairs;
    world->previous_pair_count = candidate->previous_pair_count;
    world->previous_pair_capacity = candidate->previous_pair_capacity;
    world->events = candidate->events;
    world->event_count = candidate->event_count;
    world->event_capacity = candidate->event_capacity;

    candidate->bodies = NULL;
    candidate->contacts = NULL;
    candidate->current_pairs = NULL;
    candidate->previous_pairs = NULL;
    candidate->events = NULL;

    for (index = 0U; index < old_body_capacity; ++index)
    {
        henka_free(old_bodies[index].owned_heightfield_heights_millimeters);
    }
    henka_free(old_bodies);
    henka_free(old_contacts);
    henka_free(old_current_pairs);
    henka_free(old_previous_pairs);
    henka_free(old_events);

    for (index = 0U; index < world->body_capacity; ++index)
    {
        if (world->bodies[index].active)
        {
            henka_physics_write_scene_transform(&world->bodies[index].state);
        }
    }
}

static henka_result henka_physics_substep(
    henka_physics_world* world,
    float delta_seconds,
    bool preserve_events)
{
    henka_physics_world candidate;
    henka_result result;

    memset(&candidate, 0, sizeof(candidate));
    result = henka_physics_prepare_candidate(
        world,
        preserve_events,
        &candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_physics_simulate_candidate(
        &candidate,
        delta_seconds);
    if (result != HENKA_SUCCESS)
    {
        henka_physics_release_candidate(&candidate);
        return result;
    }

    henka_physics_commit_candidate(world, &candidate);
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

henka_physics_collider_desc henka_physics_collider_capsule(
    float radius,
    float half_height)
{
    henka_physics_collider_desc collider = {0};
    collider.shape = HENKA_PHYSICS_SHAPE_CAPSULE;
    collider.data.capsule.radius = radius;
    collider.data.capsule.half_height = half_height;
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

henka_physics_collider_desc henka_physics_collider_heightfield(
    uint32_t samples_x,
    uint32_t samples_z,
    float cell_spacing,
    int32_t* heights_millimeters,
    henka_vec3 origin)
{
    henka_physics_collider_desc collider = {0};
    collider.shape = HENKA_PHYSICS_SHAPE_HEIGHTFIELD;
    collider.data.heightfield.samples_x = samples_x;
    collider.data.heightfield.samples_z = samples_z;
    collider.data.heightfield.cell_spacing = cell_spacing;
    collider.data.heightfield.heights_millimeters = heights_millimeters;
    collider.data.heightfield.origin = origin;
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

    *out_world = NULL;
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
    size_t index;
    if (world == NULL)
    {
        return;
    }
    for (index = 0U; index < world->body_capacity; ++index)
    {
        henka_free(world->bodies[index].owned_heightfield_heights_millimeters);
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
    henka_transform normalized_transform;
    int32_t* heightfield_copy = NULL;
    size_t heightfield_sample_count = 0U;
    size_t heightfield_bytes = 0U;
    size_t index;
    size_t old_capacity;
    size_t required;

    if (out_body == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_body = HENKA_INVALID_PHYSICS_BODY_ID;
    if (world == NULL || desc == NULL ||
        !henka_physics_transform_valid(desc->transform))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    normalized_transform = desc->transform;
    normalized_transform.rotation = henka_quat_normalize(
        normalized_transform.rotation);
    if (desc->type < HENKA_PHYSICS_BODY_STATIC || desc->type > HENKA_PHYSICS_BODY_KINEMATIC ||
        !henka_physics_is_finite_vec3(desc->linear_velocity) ||
        !henka_physics_is_finite_vec3(desc->angular_velocity) || !henka_physics_material_valid(desc->material) ||
        !henka_physics_collider_valid(desc->collider) ||
        !henka_physics_geometry_valid(normalized_transform, desc->collider) ||
        ((desc->collider.shape == HENKA_PHYSICS_SHAPE_PLANE ||
            desc->collider.shape == HENKA_PHYSICS_SHAPE_HEIGHTFIELD) &&
            desc->type != HENKA_PHYSICS_BODY_STATIC) ||
        (desc->type == HENKA_PHYSICS_BODY_DYNAMIC &&
            (!isfinite(desc->mass) || desc->mass <= 0.0f || !isfinite(1.0f / desc->mass))) ||
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
    if (desc->collider.shape == HENKA_PHYSICS_SHAPE_HEIGHTFIELD)
    {
        if (!henka_physics_heightfield_dimensions_valid(
                &desc->collider,
                &heightfield_sample_count) ||
            !henka_checked_size_multiply(
                heightfield_sample_count,
                sizeof(int32_t),
                &heightfield_bytes))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        heightfield_copy = henka_malloc(heightfield_bytes);
        if (heightfield_copy == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        memcpy(
            heightfield_copy,
            desc->collider.data.heightfield.heights_millimeters,
            heightfield_bytes);
    }
    memset(body, 0, sizeof(*body));
    body->active = true;
    body->state.id = world->next_body_id;
    ++world->next_body_id;
    body->state.type = desc->type;
    body->state.transform = normalized_transform;
    body->state.initial_transform = normalized_transform;
    body->state.mass = desc->type == HENKA_PHYSICS_BODY_DYNAMIC ? desc->mass : 0.0f;
    body->state.linear_velocity = desc->linear_velocity;
    body->state.angular_velocity = desc->angular_velocity;
    body->state.material = desc->material;
    body->state.collider = desc->collider;
    body->owned_heightfield_heights_millimeters = heightfield_copy;
    body->owned_heightfield_sample_count = heightfield_sample_count;
    if (heightfield_copy != NULL)
    {
        body->state.collider.data.heightfield.heights_millimeters = heightfield_copy;
    }
    body->state.linked_scene = desc->linked_scene;
    body->state.linked_entity = desc->linked_entity;
    ++world->body_count;
    *out_body = body->state.id;
    henka_physics_write_scene_transform(&body->state);
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_destroy(henka_physics_world* world, henka_physics_body_id body)
{
    size_t affected_pair_count;
    size_t index;
    size_t read_index;
    henka_physics_body_record* record;
    size_t required_event_count;
    size_t write_index;

    record = henka_physics_find_body(world, body);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    affected_pair_count = 0U;
    for (index = 0U; index < world->current_pair_count; ++index)
    {
        if (henka_physics_contact_involves_body(
                world->current_pairs[index].contact,
                body))
        {
            ++affected_pair_count;
        }
    }
    if (!henka_checked_size_add(
            world->event_count,
            affected_pair_count,
            &required_event_count) ||
        !henka_physics_reserve(
            (void**)&world->events,
            sizeof(*world->events),
            &world->event_capacity,
            required_event_count))
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    for (index = 0U; index < world->body_capacity; ++index)
    {
        henka_physics_body_record* survivor;
        bool affected;

        survivor = &world->bodies[index];
        if (!survivor->active || survivor == record)
        {
            continue;
        }

        affected = false;
        for (read_index = 0U;
            read_index < world->contact_count;
            ++read_index)
        {
            if (henka_physics_contact_involves_pair(
                    world->contacts[read_index],
                    body,
                    survivor->state.id))
            {
                affected = true;
                break;
            }
        }
        if (!affected)
        {
            continue;
        }

        survivor->state.colliding = false;
        survivor->state.grounded = false;
        for (read_index = 0U;
            read_index < world->contact_count;
            ++read_index)
        {
            henka_physics_contact contact;

            contact = world->contacts[read_index];
            if (henka_physics_contact_involves_body(contact, body))
            {
                continue;
            }
            if (contact.body_a == survivor->state.id)
            {
                survivor->state.colliding = true;
                if (!contact.is_trigger &&
                    contact.normal.y < -0.5f &&
                    survivor->state.type == HENKA_PHYSICS_BODY_DYNAMIC)
                {
                    survivor->state.grounded = true;
                }
            }
            else if (contact.body_b == survivor->state.id)
            {
                survivor->state.colliding = true;
                if (!contact.is_trigger &&
                    contact.normal.y > 0.5f &&
                    survivor->state.type == HENKA_PHYSICS_BODY_DYNAMIC)
                {
                    survivor->state.grounded = true;
                }
            }
        }
    }

    write_index = 0U;
    for (read_index = 0U; read_index < world->contact_count; ++read_index)
    {
        if (!henka_physics_contact_involves_body(
                world->contacts[read_index],
                body))
        {
            world->contacts[write_index] = world->contacts[read_index];
            ++write_index;
        }
    }
    world->contact_count = write_index;

    write_index = 0U;
    for (read_index = 0U;
        read_index < world->current_pair_count;
        ++read_index)
    {
        henka_physics_contact contact;

        contact = world->current_pairs[read_index].contact;
        if (henka_physics_contact_involves_body(contact, body))
        {
            world->events[world->event_count] = (henka_physics_event){
                contact.is_trigger ?
                    HENKA_PHYSICS_EVENT_TRIGGER_EXIT :
                    HENKA_PHYSICS_EVENT_COLLISION_EXIT,
                contact};
            ++world->event_count;
        }
        else
        {
            world->current_pairs[write_index] =
                world->current_pairs[read_index];
            ++write_index;
        }
    }
    world->current_pair_count = write_index;

    write_index = 0U;
    for (read_index = 0U;
        read_index < world->previous_pair_count;
        ++read_index)
    {
        if (!henka_physics_contact_involves_body(
                world->previous_pairs[read_index].contact,
                body))
        {
            world->previous_pairs[write_index] =
                world->previous_pairs[read_index];
            ++write_index;
        }
    }
    world->previous_pair_count = write_index;

    henka_free(record->owned_heightfield_heights_millimeters);
    memset(record, 0, sizeof(*record));
    --world->body_count;
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_get_state(
    const henka_physics_world* world,
    henka_physics_body_id body,
    henka_physics_body_state* out_state)
{
    const henka_physics_body_record* record;

    if (out_state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_state = (henka_physics_body_state){0};
    out_state->id = HENKA_INVALID_PHYSICS_BODY_ID;
    record = henka_physics_find_body_const(world, body);
    if (record == NULL)
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
    transform.rotation = henka_quat_normalize(transform.rotation);
    if (!henka_physics_geometry_valid(transform, record->state.collider))
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
        ((record->state.collider.shape == HENKA_PHYSICS_SHAPE_PLANE ||
            record->state.collider.shape == HENKA_PHYSICS_SHAPE_HEIGHTFIELD) &&
            type != HENKA_PHYSICS_BODY_STATIC))
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
    int32_t* heightfield_copy = NULL;
    size_t sample_count = 0U;
    size_t bytes = 0U;
    if (record == NULL ||
        !henka_physics_collider_valid(collider) ||
        !henka_physics_geometry_valid(record->state.transform, collider) ||
        !henka_physics_geometry_valid(record->state.initial_transform, collider) ||
        ((collider.shape == HENKA_PHYSICS_SHAPE_PLANE ||
            collider.shape == HENKA_PHYSICS_SHAPE_HEIGHTFIELD) &&
            record->state.type != HENKA_PHYSICS_BODY_STATIC))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (collider.shape == HENKA_PHYSICS_SHAPE_HEIGHTFIELD)
    {
        if (!henka_physics_heightfield_dimensions_valid(&collider, &sample_count) ||
            !henka_checked_size_multiply(sample_count, sizeof(int32_t), &bytes))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        heightfield_copy = henka_malloc(bytes);
        if (heightfield_copy == NULL)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        memcpy(heightfield_copy, collider.data.heightfield.heights_millimeters, bytes);
    }
    henka_free(record->owned_heightfield_heights_millimeters);
    record->state.collider = collider;
    record->owned_heightfield_heights_millimeters = heightfield_copy;
    record->owned_heightfield_sample_count = sample_count;
    if (heightfield_copy != NULL)
    {
        record->state.collider.data.heightfield.heights_millimeters = heightfield_copy;
    }
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

henka_result henka_physics_body_apply_force(
    henka_physics_world* world,
    henka_physics_body_id body,
    henka_vec3 force)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    henka_vec3 accumulated_force;
    henka_vec3 acceleration;
    float inverse_mass;

    if (record == NULL || !henka_physics_is_finite_vec3(force) ||
        record->state.type != HENKA_PHYSICS_BODY_DYNAMIC ||
        !henka_physics_try_add_vec3(record->force, force, &accumulated_force))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    inverse_mass = henka_physics_inverse_mass(&record->state);
    acceleration = henka_vec3_scale(accumulated_force, inverse_mass);
    if (!henka_physics_is_finite_vec3(acceleration))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->force = accumulated_force;
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_apply_impulse(
    henka_physics_world* world,
    henka_physics_body_id body,
    henka_vec3 impulse)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    henka_vec3 delta_velocity;
    henka_vec3 next_velocity;
    float inverse_mass;

    if (record == NULL || !henka_physics_is_finite_vec3(impulse) ||
        record->state.type != HENKA_PHYSICS_BODY_DYNAMIC)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    inverse_mass = henka_physics_inverse_mass(&record->state);
    delta_velocity = henka_vec3_scale(impulse, inverse_mass);
    if (!henka_physics_is_finite_vec3(delta_velocity) ||
        !henka_physics_try_add_vec3(record->state.linear_velocity, delta_velocity, &next_velocity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->state.linear_velocity = next_velocity;
    return HENKA_SUCCESS;
}

henka_result henka_physics_body_apply_torque(
    henka_physics_world* world,
    henka_physics_body_id body,
    henka_vec3 torque)
{
    henka_physics_body_record* record = henka_physics_find_body(world, body);
    henka_vec3 accumulated_torque;
    henka_vec3 angular_acceleration;
    float inverse_mass;

    if (record == NULL || !henka_physics_is_finite_vec3(torque) ||
        record->state.type != HENKA_PHYSICS_BODY_DYNAMIC ||
        !henka_physics_try_add_vec3(record->torque, torque, &accumulated_torque))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    inverse_mass = henka_physics_inverse_mass(&record->state);
    angular_acceleration = henka_vec3_scale(accumulated_torque, inverse_mass);
    if (!henka_physics_is_finite_vec3(angular_acceleration))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    record->torque = accumulated_torque;
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
    const unsigned int maximum_substeps = 16U;
    float pending_time;
    unsigned int substeps = 0U;

    if (world == NULL || !isfinite(delta_seconds) || delta_seconds < 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    pending_time = world->accumulator +
        (delta_seconds > 0.25f ? 0.25f : delta_seconds);
    while (pending_time >= world->fixed_timestep &&
        substeps < maximum_substeps)
    {
        henka_result result = henka_physics_substep(
            world,
            world->fixed_timestep,
            substeps > 0U);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }

        pending_time -= world->fixed_timestep;
        world->accumulator = pending_time;
        ++substeps;
    }

    if (substeps == 0U)
    {
        world->event_count = 0U;
        world->accumulator = pending_time;
    }
    else if (substeps == maximum_substeps &&
        pending_time >= world->fixed_timestep)
    {
        world->accumulator = fmodf(
            pending_time,
            world->fixed_timestep);
    }

    return HENKA_SUCCESS;
}

henka_result henka_physics_world_step_fixed(henka_physics_world* world)
{
    if (world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_physics_substep(
        world,
        world->fixed_timestep,
        false);
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
        if (world->bodies[index].active &&
            (!henka_physics_transform_valid(
                world->bodies[index].state.initial_transform) ||
                !henka_physics_quaternion_normalized(
                    world->bodies[index].state.initial_transform.rotation) ||
                !henka_physics_geometry_valid(
                    world->bodies[index].state.initial_transform,
                    world->bodies[index].state.collider)))
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
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

size_t henka_physics_test_get_current_pair_count(
    const henka_physics_world* world)
{
    return world != NULL ? world->current_pair_count : 0U;
}

size_t henka_physics_test_get_previous_pair_count(
    const henka_physics_world* world)
{
    return world != NULL ? world->previous_pair_count : 0U;
}

float henka_physics_test_get_accumulator(const henka_physics_world* world)
{
    return world != NULL ? world->accumulator : 0.0f;
}

static bool henka_physics_raycast_sphere_at(
    henka_vec3 center,
    float radius,
    henka_ray ray,
    float maximum,
    float* distance,
    henka_vec3* normal)
{
    henka_vec3 offset = henka_vec3_subtract(ray.origin, center);
    float b = henka_vec3_dot(offset, ray.direction);
    float c = henka_vec3_dot(offset, offset) - radius * radius;
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
    *normal = henka_vec3_normalize(henka_vec3_subtract(
        henka_vec3_add(ray.origin, henka_vec3_scale(ray.direction, result)), center));
    return true;
}

static bool henka_physics_raycast_sphere(
    const henka_physics_body_state* body,
    henka_ray ray,
    float maximum,
    float* distance,
    henka_vec3* normal)
{
    return henka_physics_raycast_sphere_at(
        henka_physics_collider_center(body),
        henka_physics_sphere_radius(body),
        ray,
        maximum,
        distance,
        normal);
}

static bool henka_physics_raycast_capsule(
    const henka_physics_body_state* body,
    henka_ray ray,
    float maximum,
    float* distance,
    henka_vec3* normal)
{
    henka_vec3 center = henka_physics_collider_center(body);
    float radius = henka_physics_capsule_radius(body);
    float half_height = henka_physics_capsule_half_height(body);
    double origin_x = (double)ray.origin.x - (double)center.x;
    double origin_z = (double)ray.origin.z - (double)center.z;
    double direction_x = (double)ray.direction.x;
    double direction_z = (double)ray.direction.z;
    double quadratic_a = direction_x * direction_x + direction_z * direction_z;
    double quadratic_b = 2.0 * (origin_x * direction_x + origin_z * direction_z);
    double quadratic_c = origin_x * origin_x + origin_z * origin_z -
        (double)radius * (double)radius;
    double discriminant = quadratic_b * quadratic_b -
        4.0 * quadratic_a * quadratic_c;
    double roots[2];
    float best = maximum;
    bool found = false;
    size_t index;

    if (henka_physics_raycast_sphere_at(
            (henka_vec3){center.x, center.y - half_height, center.z},
            radius,
            ray,
            best,
            distance,
            normal))
    {
        best = *distance;
        found = true;
    }
    if (henka_physics_raycast_sphere_at(
            (henka_vec3){center.x, center.y + half_height, center.z},
            radius,
            ray,
            best,
            distance,
            normal))
    {
        best = *distance;
        found = true;
    }

    if (quadratic_a > 0.0000000001 && discriminant >= 0.0 && isfinite(discriminant))
    {
        double root_delta = sqrt(discriminant);
        roots[0] = (-quadratic_b - root_delta) / (2.0 * quadratic_a);
        roots[1] = (-quadratic_b + root_delta) / (2.0 * quadratic_a);
        for (index = 0U; index < 2U; ++index)
        {
            double root = roots[index];
            double y;
            henka_vec3 point;
            double radial_length;

            if (!isfinite(root) || root < 0.0 || root > (double)best)
            {
                continue;
            }
            y = (double)ray.origin.y + (double)ray.direction.y * root;
            if (y < (double)center.y - (double)half_height ||
                y > (double)center.y + (double)half_height)
            {
                continue;
            }
            point = henka_vec3_add(
                ray.origin,
                henka_vec3_scale(ray.direction, (float)root));
            radial_length = hypot(
                (double)point.x - (double)center.x,
                (double)point.z - (double)center.z);
            if (!isfinite(radial_length) || radial_length <= 0.0001 ||
                !henka_physics_double_fits_float(root))
            {
                continue;
            }
            best = (float)root;
            *distance = best;
            *normal = (henka_vec3){
                (float)(((double)point.x - (double)center.x) / radial_length),
                0.0f,
                (float)(((double)point.z - (double)center.z) / radial_length)};
            found = true;
        }
    }
    return found;
}

static bool henka_physics_raycast_box(
    const henka_physics_body_state* body,
    henka_ray ray,
    float maximum,
    float* distance,
    henka_vec3* normal)
{
    henka_vec3 center = henka_physics_collider_center(body);
    henka_vec3 extents = henka_physics_box_extents(body);
    float minimum = 0.0f;
    float maximum_value = FLT_MAX;
    bool origin_inside = true;
    int axis;
    henka_vec3 entry_normal = {0.0f, 0.0f, 0.0f};
    henka_vec3 exit_normal = {0.0f, 0.0f, 0.0f};
    float origins[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    float directions[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    float centers[3] = {center.x, center.y, center.z};
    float extent_values[3] = {extents.x, extents.y, extents.z};

    for (axis = 0; axis < 3; ++axis)
    {
        float low;
        float high;
        henka_vec3 near_normal = {0.0f, 0.0f, 0.0f};
        henka_vec3 far_normal = {0.0f, 0.0f, 0.0f};

        if (origins[axis] < centers[axis] - extent_values[axis] ||
            origins[axis] > centers[axis] + extent_values[axis])
        {
            origin_inside = false;
        }

        if (henka_physics_abs(directions[axis]) < 0.00001f)
        {
            if (origins[axis] < centers[axis] - extent_values[axis] ||
                origins[axis] > centers[axis] + extent_values[axis])
            {
                return false;
            }
            continue;
        }

        low = (centers[axis] - extent_values[axis] - origins[axis]) / directions[axis];
        high = (centers[axis] + extent_values[axis] - origins[axis]) / directions[axis];
        if (axis == 0)
        {
            near_normal.x = -1.0f;
            far_normal.x = 1.0f;
        }
        else if (axis == 1)
        {
            near_normal.y = -1.0f;
            far_normal.y = 1.0f;
        }
        else
        {
            near_normal.z = -1.0f;
            far_normal.z = 1.0f;
        }

        if (low > high)
        {
            float temporary = low;
            henka_vec3 temporary_normal = near_normal;
            low = high;
            high = temporary;
            near_normal = far_normal;
            far_normal = temporary_normal;
        }

        if (low > minimum)
        {
            minimum = low;
            entry_normal = near_normal;
        }
        if (high < maximum_value)
        {
            maximum_value = high;
            exit_normal = far_normal;
        }
        if (minimum > maximum_value)
        {
            return false;
        }
    }

    if (origin_inside)
    {
        if (maximum_value < 0.0f || maximum_value > maximum)
        {
            return false;
        }

        *distance = maximum_value;
        *normal = exit_normal;
        return true;
    }

    if (minimum < 0.0f || minimum > maximum)
    {
        return false;
    }

    *distance = minimum;
    *normal = entry_normal;
    return true;
}

static bool henka_physics_raycast_plane(const henka_physics_body_state* body, henka_ray ray, float maximum, float* distance, henka_vec3* normal)
{
    henka_vec3 plane_normal = henka_physics_plane_world_normal(body);
    float denominator = henka_vec3_dot(plane_normal, ray.direction);
    float offset = henka_physics_plane_world_offset(body, plane_normal);
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

static bool henka_physics_raycast_heightfield(
    const henka_physics_body_state* body,
    henka_ray ray,
    float maximum,
    float* distance,
    henka_vec3* normal)
{
    float step = fmaxf(body->collider.data.heightfield.cell_spacing * 0.5f, 0.01f);
    uint32_t steps;
    float previous_t = 0.0f;
    float previous_value = 0.0f;
    bool previous_valid = false;
    uint32_t index;

    if (!isfinite(step) || step <= 0.0f ||
        maximum / step > 4096.0f)
    {
        return false;
    }
    steps = (uint32_t)ceilf(maximum / step);
    if (steps == 0U) steps = 1U;
    for (index = 0U; index <= steps; ++index)
    {
        float t = index == steps ? maximum : fminf(maximum, (float)index * step);
        henka_vec3 point = henka_vec3_add(ray.origin, henka_vec3_scale(ray.direction, t));
        float surface;
        henka_vec3 surface_normal;
        float value;
        if (!henka_physics_heightfield_sample(body, point.x, point.z, &surface, &surface_normal))
        {
            previous_valid = false;
            continue;
        }
        value = point.y - surface;
        if (value <= 0.0f || (previous_valid && previous_value > 0.0f && value <= 0.0f))
        {
            float low = previous_valid ? previous_t : t;
            float high = t;
            uint32_t iteration;
            if (!previous_valid)
            {
                low = t;
                high = t;
            }
            for (iteration = 0U; iteration < 8U && high > low; ++iteration)
            {
                float middle = (low + high) * 0.5f;
                henka_vec3 middle_point = henka_vec3_add(
                    ray.origin, henka_vec3_scale(ray.direction, middle));
                float middle_surface;
                henka_vec3 middle_normal;
                if (!henka_physics_heightfield_sample(
                        body, middle_point.x, middle_point.z, &middle_surface, &middle_normal) ||
                    middle_point.y - middle_surface <= 0.0f)
                {
                    high = middle;
                }
                else
                {
                    low = middle;
                }
            }
            *distance = high;
            point = henka_vec3_add(ray.origin, henka_vec3_scale(ray.direction, high));
            if (!henka_physics_heightfield_sample(
                    body, point.x, point.z, &surface, &surface_normal))
            {
                return false;
            }
            *normal = henka_vec3_dot(surface_normal, ray.direction) > 0.0f ?
                henka_vec3_scale(surface_normal, -1.0f) : surface_normal;
            return true;
        }
        previous_t = t;
        previous_value = value;
        previous_valid = true;
    }
    return false;
}

henka_result henka_physics_world_raycast(const henka_physics_world* world, henka_ray ray, float max_distance, uint32_t layer_mask, henka_physics_raycast_hit* out_hit)
{
    size_t index;
    float closest = FLT_MAX;
    henka_vec3 normalized_direction;

    if (out_hit == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_hit = (henka_physics_raycast_hit){
        false,
        HENKA_INVALID_PHYSICS_BODY_ID,
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        0.0f};
    if (world == NULL ||
        !henka_physics_is_finite_vec3(ray.origin) ||
        !henka_physics_is_finite_vec3(ray.direction) ||
        !isfinite(max_distance) ||
        max_distance <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    normalized_direction = henka_vec3_normalize(ray.direction);
    if (henka_vec3_length(normalized_direction) <= 0.0001f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    ray.direction = normalized_direction;
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
        else if (body->collider.shape == HENKA_PHYSICS_SHAPE_CAPSULE)
        {
            hit = henka_physics_raycast_capsule(body, ray, max_distance, &distance, &normal);
        }
        else if (body->collider.shape == HENKA_PHYSICS_SHAPE_BOX)
        {
            hit = henka_physics_raycast_box(body, ray, max_distance, &distance, &normal);
        }
        else if (body->collider.shape == HENKA_PHYSICS_SHAPE_PLANE)
        {
            hit = henka_physics_raycast_plane(body, ray, max_distance, &distance, &normal);
        }
        else if (body->collider.shape == HENKA_PHYSICS_SHAPE_HEIGHTFIELD)
        {
            hit = henka_physics_raycast_heightfield(body, ray, max_distance, &distance, &normal);
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

    if (out_shape == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_shape = (henka_physics_debug_shape){0};
    out_shape->body = HENKA_INVALID_PHYSICS_BODY_ID;
    if (world == NULL)
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
        case HENKA_PHYSICS_SHAPE_CAPSULE: return "Capsule";
        case HENKA_PHYSICS_SHAPE_BOX: return "AABB";
        case HENKA_PHYSICS_SHAPE_PLANE: return "Plane";
        case HENKA_PHYSICS_SHAPE_HEIGHTFIELD: return "Heightfield";
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
