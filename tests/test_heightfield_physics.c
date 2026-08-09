#include <stdint.h>

#include <henka/physics.h>

static henka_transform test_transform(float x, float y, float z)
{
    return (henka_transform){
        {x, y, z},
        {0.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f}};
}

int main(void)
{
    int32_t flat[9] = {0};
    int32_t slope[9] = {0, 0, 0, 1000, 1000, 1000, 2000, 2000, 2000};
    henka_physics_world* world = NULL;
    henka_physics_body_id terrain = HENKA_INVALID_PHYSICS_BODY_ID;
    henka_physics_body_id sphere = HENKA_INVALID_PHYSICS_BODY_ID;
    henka_physics_body_id box = HENKA_INVALID_PHYSICS_BODY_ID;
    henka_physics_body_desc terrain_desc = {0};
    henka_physics_body_desc sphere_desc = {0};
    henka_physics_body_state state;
    henka_physics_raycast_hit ray_hit;
    henka_physics_collider_desc collider;

    collider = henka_physics_collider_heightfield(3U, 3U, 1.0f, flat, (henka_vec3){0.0f, 0.0f, 0.0f});
    terrain_desc.type = HENKA_PHYSICS_BODY_STATIC;
    terrain_desc.transform = test_transform(0.0f, 0.0f, 0.0f);
    terrain_desc.mass = 0.0f;
    terrain_desc.material = henka_physics_material_default();
    terrain_desc.collider = collider;
    if (henka_physics_world_create(&world) != HENKA_SUCCESS ||
        henka_physics_body_create(world, &terrain_desc, &terrain) != HENKA_SUCCESS)
    {
        return 1;
    }

    /* Creation owns a copy: changing the borrowed source cannot change state. */
    flat[4] = 5000;
    if (henka_physics_body_get_state(world, terrain, &state) != HENKA_SUCCESS ||
        state.collider.shape != HENKA_PHYSICS_SHAPE_HEIGHTFIELD ||
        state.collider.data.heightfield.heights_millimeters[4] != 0)
    {
        henka_physics_world_destroy(world);
        return 2;
    }

    sphere_desc.type = HENKA_PHYSICS_BODY_DYNAMIC;
    sphere_desc.transform = test_transform(1.0f, 0.4f, 1.0f);
    sphere_desc.mass = 1.0f;
    sphere_desc.material = henka_physics_material_default();
    sphere_desc.collider = henka_physics_collider_sphere(0.5f);
    if (henka_physics_body_create(world, &sphere_desc, &sphere) != HENKA_SUCCESS ||
        henka_physics_world_step_fixed(world) != HENKA_SUCCESS ||
        henka_physics_body_get_state(world, sphere, &state) != HENKA_SUCCESS ||
        !state.grounded)
    {
        henka_physics_world_destroy(world);
        return 3;
    }

    if (henka_physics_world_raycast(
            world,
            (henka_ray){{0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
            10.0f,
            1U,
            &ray_hit) != HENKA_SUCCESS ||
        !ray_hit.hit || ray_hit.body != terrain || ray_hit.distance < 2.9f || ray_hit.distance > 3.1f)
    {
        henka_physics_world_destroy(world);
        return 4;
    }

    if (henka_physics_body_set_collider(
            world,
            terrain,
            henka_physics_collider_heightfield(3U, 3U, 1.0f, slope, (henka_vec3){0.0f, 0.0f, 0.0f})) != HENKA_SUCCESS ||
        henka_physics_body_get_state(world, terrain, &state) != HENKA_SUCCESS ||
        state.collider.data.heightfield.heights_millimeters[6] != 2000)
    {
        henka_physics_world_destroy(world);
        return 5;
    }

    sphere_desc.transform = test_transform(0.5f, 0.4f, 0.5f);
    sphere_desc.collider = henka_physics_collider_box((henka_vec3){0.4f, 0.4f, 0.4f});
    if (henka_physics_body_create(world, &sphere_desc, &box) != HENKA_SUCCESS ||
        henka_physics_world_step_fixed(world) != HENKA_SUCCESS ||
        henka_physics_body_get_state(world, box, &state) != HENKA_SUCCESS ||
        !state.grounded)
    {
        henka_physics_world_destroy(world);
        return 7;
    }

    /* Invalid dimensions fail without discarding the last valid heightfield. */
    collider = henka_physics_collider_heightfield(1U, 3U, 1.0f, slope, (henka_vec3){0.0f, 0.0f, 0.0f});
    if (henka_physics_body_set_collider(world, terrain, collider) == HENKA_SUCCESS ||
        henka_physics_body_get_state(world, terrain, &state) != HENKA_SUCCESS ||
        state.collider.data.heightfield.samples_x != 3U)
    {
        henka_physics_world_destroy(world);
        return 8;
    }

    henka_physics_world_destroy(world);
    return 0;
}
