#include "test_suite.h"

#include <float.h>
#include <string.h>

#include <henka/core.h>
#include <henka/scene.h>

#include "../engine/src/core/checked.h"
#include "../engine/src/henka_internal.h"

static void henka_test_scene_capacity_growth(void)
{
    enum
    {
        ENTITY_COUNT = 40
    };
    henka_entity entities[ENTITY_COUNT];
    henka_entity replacement;
    henka_scene* scene;
    henka_transform replacement_transform;
    int index;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_render_revision(scene) != 0U);
    for (index = 0; index < ENTITY_COUNT; ++index)
    {
        entities[index] = henka_scene_create_entity(scene);
        HENKA_TEST_ASSERT(entities[index] != HENKA_INVALID_ENTITY);
    }

    {
        const uint64_t initial_revision = henka_scene_get_render_revision(scene);
        HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
            scene,
            entities[0],
            henka_transform_identity()) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_scene_get_render_revision(scene) > initial_revision);
    }

    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == (size_t)ENTITY_COUNT);
    henka_scene_destroy_entity(scene, entities[7]);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == (size_t)ENTITY_COUNT - 1U);

    replacement = henka_scene_create_entity_named(scene, "Replacement");
    HENKA_TEST_ASSERT(replacement != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(replacement != entities[7]);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, entities[7]));
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(scene, replacement));
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == (size_t)ENTITY_COUNT);
    HENKA_TEST_ASSERT(
        henka_scene_get_entity_at_storage_index(scene, 7U) == replacement);
    HENKA_TEST_ASSERT(
        henka_scene_get_entity_at_storage_index(scene, 0U) == entities[0]);
    HENKA_TEST_ASSERT(
        henka_scene_get_entity_at_storage_index(scene, ENTITY_COUNT) == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, replacement), "Replacement") == 0);

    replacement_transform = henka_transform_identity();
    replacement_transform.position.x = 8.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
        scene,
        replacement,
        replacement_transform) == HENKA_SUCCESS);
    replacement_transform.position.x = -12.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
        scene,
        entities[7],
        replacement_transform) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(
        scene,
        replacement,
        &replacement_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        replacement_transform.position.x,
        8.0f,
        0.0001f);

    henka_scene_destroy(scene);
}

void henka_test_scene(void)
{
    henka_bounds bounds;
    henka_camera camera;
    uint32_t flags;
    henka_scene* scene;
    henka_scene* cloned_scene;
    henka_entity found;
    henka_entity first;
    henka_entity helper;
    henka_entity listed;
    henka_ray ray;
    henka_scene_object_info info;
    char interaction_prompt[] = "Inspect sample";
    henka_material material;
    char material_name[] = "Mutable Material";
    char overlong_text[HENKA_MAX_SCENE_TEXT_BYTES + 2U];
    henka_entity second;
    henka_entity selection_root;
    henka_entity selection_child;
    henka_interaction_desc interaction;
    henka_interaction_desc read_interaction;
    henka_material read_material;
    const henka_material_asset* material_asset;
    const henka_material_asset* read_material_asset;
    henka_transform transform;
    henka_transform read_back;
    henka_scene_environment_desc environment;
    henka_scene_environment_desc read_environment;
    henka_scene_environment_desc invalid_environment;
    henka_scene_fog_desc fog;
    henka_scene_fog_desc read_fog;
    henka_mesh* read_mesh;
    henka_scene_light_desc light;
    henka_scene_light_desc read_light;
    henka_scene_lod_desc lod;
    henka_scene_lod_desc read_lod;
    henka_scene_reflection_probe_desc reflection_probe;
    henka_scene_reflection_probe_desc read_reflection_probe;
    uint32_t reflection_probe_indices[HENKA_SCENE_MAX_REFLECTION_PROBES];
    uint32_t reflection_probe_index;
    uint32_t light_indices[HENKA_SCENE_MAX_LOCAL_LIGHTS];
    uint32_t light_index;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(scene != NULL);
    HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_environment.intensity, 1.5f, 0.0001f);
    HENKA_TEST_ASSERT(read_environment.moon.enabled);
    HENKA_TEST_ASSERT(read_environment.stars.enabled);
    environment = (henka_scene_environment_desc){
        (henka_vec3){0.02f, 0.03f, 0.05f},
        (henka_vec3){0.14f, 0.18f, 0.24f},
        (henka_vec3){0.06f, 0.09f, 0.16f},
        2.0f};
    HENKA_TEST_ASSERT(henka_scene_set_environment(scene, environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_environment.horizon_color.y, 0.18f, 0.0001f);
    environment.intensity = 17.0f;
    HENKA_TEST_ASSERT(henka_scene_set_environment(scene, environment) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_environment.intensity, 2.0f, 0.0001f);
    environment = henka_scene_environment_default();
    environment.mode = HENKA_SCENE_ENVIRONMENT_PROCEDURAL;
    environment.sun.manual_direction = false;
    environment.time_of_day_enabled = true;
    environment.time_of_day_hours = 6.0f;
    HENKA_TEST_ASSERT(henka_scene_set_environment(scene, environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_advance_environment_time(scene, 60.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_environment.time_of_day_hours, 8.4f, 0.0001f);
    {
        henka_scene_environment_desc orbital_environment = henka_scene_environment_default();
        orbital_environment.mode = HENKA_SCENE_ENVIRONMENT_PROCEDURAL;
        orbital_environment.sun.manual_direction = true;
        orbital_environment.sun.direction = henka_vec3_normalize((henka_vec3){0.25f, -0.8f, 0.5f});
        orbital_environment.moon.enabled = true;
        orbital_environment.moon.manual_direction = false;
        HENKA_TEST_ASSERT(henka_scene_set_environment(scene, orbital_environment) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            henka_vec3_dot(read_environment.sun.direction, read_environment.moon.direction),
            -1.0f,
            0.0001f);
    }
    environment.time_of_day_enabled = true;
    environment.time_of_day_hours = 8.4f;
    HENKA_TEST_ASSERT(henka_scene_set_environment(scene, environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
    invalid_environment = read_environment;
    invalid_environment.atmosphere.rayleigh_scattering = -1.0f;
    HENKA_TEST_ASSERT(henka_scene_set_environment(scene, invalid_environment) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_environment.time_of_day_hours, 8.4f, 0.0001f);
    invalid_environment = read_environment;
    invalid_environment.moon.intensity = -1.0f;
    HENKA_TEST_ASSERT(henka_scene_set_environment(scene, invalid_environment) == HENKA_ERROR_INVALID_ARGUMENT);
    {
        henka_scene_environment_preset preset;
        const char* preset_label;
        for (preset = HENKA_SCENE_ENVIRONMENT_PRESET_CLEAR_MIDDAY;
             preset < HENKA_SCENE_ENVIRONMENT_PRESET_COUNT;
             ++preset)
        {
            preset_label = henka_scene_environment_preset_get_label(preset);
            HENKA_TEST_ASSERT(preset_label != NULL);
            HENKA_TEST_ASSERT(strcmp(preset_label, "Unknown") != 0);
            HENKA_TEST_ASSERT(henka_scene_set_environment_preset(scene, preset) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(read_environment.mode == HENKA_SCENE_ENVIRONMENT_PROCEDURAL);
            HENKA_TEST_ASSERT(!read_environment.time_of_day_enabled);
        }
        invalid_environment = read_environment;
        HENKA_TEST_ASSERT(henka_scene_set_environment_preset(
            scene,
            (henka_scene_environment_preset)HENKA_SCENE_ENVIRONMENT_PRESET_COUNT) == HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(read_environment.intensity, invalid_environment.intensity, 0.0001f);
    }
    light = (henka_scene_light_desc){
        HENKA_SCENE_LIGHT_POINT,
        (henka_vec3){1.0f, 2.0f, 3.0f},
        (henka_vec3){0.0f, -1.0f, 0.0f},
        (henka_vec3){1.0f, 0.8f, 0.6f},
        20.0f,
        12.0f,
        1.0f,
        0.5f,
        true};
    HENKA_TEST_ASSERT(henka_scene_add_light(scene, light, &light_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(light_index < HENKA_SCENE_MAX_LOCAL_LIGHTS);
    HENKA_TEST_ASSERT(henka_scene_get_light(scene, light_index, &read_light) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_light.position.x, 1.0f, 0.0001f);
    light.type = HENKA_SCENE_LIGHT_SPOT;
    light.inner_cone_cosine = 0.8f;
    light.outer_cone_cosine = 0.9f;
    HENKA_TEST_ASSERT(henka_scene_update_light(scene, light_index, light) == HENKA_ERROR_INVALID_ARGUMENT);
    light.outer_cone_cosine = 0.5f;
    HENKA_TEST_ASSERT(henka_scene_update_light(scene, light_index, light) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_remove_light(scene, light_index) == HENKA_SUCCESS);
    light.enabled = true;
    light.type = HENKA_SCENE_LIGHT_POINT;
    for (light_index = 0U; light_index < HENKA_SCENE_MAX_LOCAL_LIGHTS; ++light_index)
    {
        HENKA_TEST_ASSERT(henka_scene_add_light(scene, light, &light_indices[light_index]) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_scene_add_light(scene, light, &light_index) == HENKA_ERROR_LIMIT);
    for (light_index = 0U; light_index < HENKA_SCENE_MAX_LOCAL_LIGHTS; ++light_index)
    {
        HENKA_TEST_ASSERT(henka_scene_remove_light(scene, light_indices[light_index]) == HENKA_SUCCESS);
    }
    reflection_probe = (henka_scene_reflection_probe_desc){
        (henka_vec3){0.0f, 1.0f, -2.0f},
        (henka_vec3){4.0f, 2.0f, 5.0f},
        1.0f,
        true,
        true};
    HENKA_TEST_ASSERT(henka_scene_add_reflection_probe(scene, reflection_probe, &reflection_probe_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(reflection_probe_index == 0U);
    HENKA_TEST_ASSERT(henka_scene_get_reflection_probe(scene, reflection_probe_index, &read_reflection_probe) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_reflection_probe.extents.z, 5.0f, 0.0001f);
    reflection_probe.position.x = 2.0f;
    HENKA_TEST_ASSERT(henka_scene_update_reflection_probe(scene, reflection_probe_index, reflection_probe) == HENKA_SUCCESS);
    reflection_probe.extents.x = 0.0f;
    HENKA_TEST_ASSERT(henka_scene_update_reflection_probe(scene, reflection_probe_index, reflection_probe) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_remove_reflection_probe(scene, reflection_probe_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_reflection_probe(scene, reflection_probe_index, &read_reflection_probe) == HENKA_ERROR_INVALID_ARGUMENT);
    reflection_probe.extents.x = 4.0f;
    for (reflection_probe_index = 0U; reflection_probe_index < HENKA_SCENE_MAX_REFLECTION_PROBES; ++reflection_probe_index)
    {
        HENKA_TEST_ASSERT(henka_scene_add_reflection_probe(scene, reflection_probe, &reflection_probe_indices[reflection_probe_index]) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_scene_add_reflection_probe(scene, reflection_probe, &reflection_probe_index) == HENKA_ERROR_LIMIT);
    for (reflection_probe_index = 0U; reflection_probe_index < HENKA_SCENE_MAX_REFLECTION_PROBES; ++reflection_probe_index)
    {
        HENKA_TEST_ASSERT(henka_scene_remove_reflection_probe(scene, reflection_probe_indices[reflection_probe_index]) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_scene_get_fog(scene, &read_fog) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!read_fog.enabled);
    fog = (henka_scene_fog_desc){
        true,
        HENKA_SCENE_FOG_EXPONENTIAL,
        (henka_vec3){0.18f, 0.21f, 0.26f},
        6.0f,
        64.0f,
        0.02f};
    HENKA_TEST_ASSERT(henka_scene_set_fog(scene, fog) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_fog(scene, &read_fog) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_fog.enabled);
    HENKA_TEST_ASSERT(read_fog.mode == HENKA_SCENE_FOG_EXPONENTIAL);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_fog.end_distance, 64.0f, 0.0001f);
    fog.end_distance = 5.0f;
    HENKA_TEST_ASSERT(henka_scene_set_fog(scene, fog) == HENKA_ERROR_INVALID_ARGUMENT);
    fog.end_distance = 64.0f;
    fog.mode = (henka_scene_fog_mode)99;
    HENKA_TEST_ASSERT(henka_scene_set_fog(scene, fog) == HENKA_ERROR_INVALID_ARGUMENT);
    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD,
        16.0f / 9.0f,
        0.1f,
        100.0f);
    HENKA_TEST_ASSERT(henka_scene_set_camera(scene, &camera) == HENKA_SUCCESS);
    camera.aspect_ratio = 0.0f;
    HENKA_TEST_ASSERT(henka_scene_set_camera(scene, &camera) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 0U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 0U) == HENKA_INVALID_ENTITY);

    first = henka_scene_create_entity_named(scene, "Ground");
    second = henka_scene_create_entity(scene);
    HENKA_TEST_ASSERT(first != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(second != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_selection_owner(scene, first, &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == first);
    HENKA_TEST_ASSERT(henka_scene_set_entity_selection_owner(scene, second, first) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_selection_owner(scene, second, &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == first);
    HENKA_TEST_ASSERT(henka_scene_set_entity_selection_owner(scene, first, second) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_set_entity_selection_owner(scene, second, HENKA_INVALID_ENTITY) == HENKA_ERROR_INVALID_ARGUMENT);
    selection_root = henka_scene_create_entity_named(scene, "Selection Root");
    selection_child = henka_scene_create_entity_named(scene, "Selection Child");
    HENKA_TEST_ASSERT(selection_root != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(selection_child != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_selection_owner(
        scene, selection_child, selection_root) == HENKA_SUCCESS);
    henka_scene_destroy_entity(scene, selection_root);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, selection_root));
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(scene, selection_child));
    HENKA_TEST_ASSERT(henka_scene_get_entity_selection_owner(
        scene, selection_child, &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == selection_child);
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(scene, first));
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(scene, second));
    HENKA_TEST_ASSERT(henka_scene_clear_entity_mesh(scene, first) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, first, &read_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_mesh == NULL);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 3U);
    lod = (henka_scene_lod_desc){0};
    HENKA_TEST_ASSERT(henka_scene_set_entity_lod(scene, first, lod) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_lod(scene, first, &read_lod) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_lod.level_count == 0U);
    lod.level_count = 1U;
    lod.max_distances[0] = 32.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_lod(scene, first, lod) == HENKA_ERROR_INVALID_ARGUMENT);
    listed = henka_scene_get_entity_at_index(scene, 0U);
    HENKA_TEST_ASSERT(listed == first);
    listed = henka_scene_get_entity_at_index(scene, 1U);
    HENKA_TEST_ASSERT(listed == second);
    listed = henka_scene_get_entity_at_index(scene, 2U);
    HENKA_TEST_ASSERT(listed == selection_child);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 3U) == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_is_entity_visible(scene, first));
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, first), "Ground") == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_name(scene, second) == NULL);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(scene, "Ground", &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == first);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(scene, "Missing", &found) == HENKA_ERROR_UNKNOWN);

    transform = henka_transform_identity();
    transform.position.x = 5.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, first, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, first, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.position.x, 5.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_translate_entity(scene, first, (henka_vec3){-2.0f, 1.0f, 0.5f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, first, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.position.x, 3.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.position.y, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_rotate_entity(scene, first, henka_quat_from_axis_angle((henka_vec3){0.0f, 1.0f, 0.0f}, 90.0f * HENKA_DEG_TO_RAD)) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_scale_entity(scene, first, (henka_vec3){2.0f, -1.0f, 0.5f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, first, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.scale.x, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.scale.y, -1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.scale.z, 0.5f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_scale_entity(scene, first, (henka_vec3){1.0f, 0.0f, 1.0f}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, first, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.scale.y, -1.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, first, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_visible(scene, first));
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, first, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_is_entity_visible(scene, first));
    HENKA_TEST_ASSERT(henka_scene_set_entity_name(scene, second, "Marker") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, second), "Marker") == 0);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(scene, "Marker", &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    HENKA_TEST_ASSERT(henka_scene_set_entity_tag(scene, second, "marker") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_tag(scene, second), "marker") == 0);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_tag(scene, "marker", &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    helper = henka_scene_create_entity_named(scene, "Transform Gizmo");
    HENKA_TEST_ASSERT(helper != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(scene, helper, HENKA_SCENE_ENTITY_FLAG_HELPER) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_is_entity_helper(scene, helper));
    HENKA_TEST_ASSERT(henka_scene_get_entity_flags(scene, helper, &flags) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT((flags & HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U);
    HENKA_TEST_ASSERT(henka_scene_set_entity_selection_owner(scene, second, helper) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(
        scene,
        first,
        HENKA_SCENE_ENTITY_FLAG_TRANSFORM_LOCKED) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_is_entity_transform_locked(scene, first));
    HENKA_TEST_ASSERT(henka_scene_get_entity_flags(scene, first, &flags) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT((flags & HENKA_SCENE_ENTITY_FLAG_TRANSFORM_LOCKED) != 0U);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_transform_locked(scene, second));
    bounds = (henka_bounds){{0.0f, 0.5f, 0.0f}, {0.5f, 0.5f, 0.5f}};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(scene, second, bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(scene, second, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.extents.x, 0.5f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(
        scene,
        second,
        (henka_bounds){{0.0f, 0.0f, 0.0f}, {-1.0f, 1.0f, 1.0f}}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(
        scene,
        second,
        (henka_bounds){{NAN, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}) == HENKA_ERROR_INVALID_ARGUMENT);
    bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(scene, helper, bounds) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.rotation = (henka_quat){FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, helper, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, helper, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.rotation.x, 0.5f, 0.0002f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.rotation.w, 0.5f, 0.0002f);
    transform = henka_transform_identity();
    transform.position.x = FLT_MAX;
    transform.scale = (henka_vec3){FLT_MAX, 1.0f, 1.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, helper, transform) == HENKA_ERROR_INVALID_ARGUMENT);
    bounds = (henka_bounds){{9.0f, 9.0f, 9.0f}, {8.0f, 8.0f, 8.0f}};
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_bounds(
        scene,
        HENKA_INVALID_ENTITY,
        &bounds) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.x, 0.0f, 0.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.extents.x, 0.0f, 0.0f);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){10.0f, 0.0f, 0.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, helper, transform) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){1.0f, 0.0f, 0.0f};
    transform.rotation = henka_quat_from_axis_angle((henka_vec3){0.0f, 1.0f, 0.0f}, 45.0f * HENKA_DEG_TO_RAD);
    transform.scale = (henka_vec3){-2.0f, 1.0f, 0.5f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, second, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, second, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.scale.x, -2.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_bounds(scene, second, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(bounds.extents.x > 0.5f);
    HENKA_TEST_ASSERT(bounds.extents.z > 0.25f);
    material = henka_material_default();
    material.name = material_name;
    material.shader = (henka_shader*)(uintptr_t)1U;
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(scene, second, material) == HENKA_SUCCESS);
    material_name[0] = 'X';
    HENKA_TEST_ASSERT(henka_scene_get_entity_material(scene, second, &read_material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(read_material.name, "Mutable Material") == 0);
    material_asset = (const henka_material_asset*)(uintptr_t)0x1234U;
    read_material_asset = NULL;
    HENKA_TEST_ASSERT(henka_scene_get_entity_material_asset(
        scene, second, &read_material_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_material_asset == NULL);
    HENKA_TEST_ASSERT(henka_scene_set_entity_material_asset(
        scene, second, material_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material_asset(
        scene, second, &read_material_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_material_asset == material_asset);
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(scene, second, material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material_asset(
        scene, second, &read_material_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_material_asset == material_asset);
    HENKA_TEST_ASSERT(henka_scene_set_entity_material_asset(
        scene, second, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material_asset(
        scene, second, &read_material_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_material_asset == NULL);
    material.base_color.x = NAN;
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(scene, second, material) == HENKA_ERROR_INVALID_ARGUMENT);
    material.base_color.x = 1.0f;
    material.use_texture = true;
    material.base_color_texture = NULL;
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(scene, second, material) == HENKA_ERROR_INVALID_ARGUMENT);

    interaction = (henka_interaction_desc){true, 3.5f, interaction_prompt};
    HENKA_TEST_ASSERT(henka_scene_set_entity_interaction(scene, second, &interaction) == HENKA_SUCCESS);
    strcpy_s(interaction_prompt, sizeof(interaction_prompt), "Changed prompt");
    HENKA_TEST_ASSERT(henka_scene_get_entity_interaction(scene, second, &read_interaction) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_interaction.enabled);
    HENKA_TEST_ASSERT(strcmp(read_interaction.prompt, "Inspect sample") == 0);
    interaction.max_distance = -1.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_interaction(scene, second, &interaction) == HENKA_ERROR_INVALID_ARGUMENT);
    interaction.max_distance = NAN;
    HENKA_TEST_ASSERT(henka_scene_set_entity_interaction(scene, second, &interaction) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_can_interact(scene, second, (henka_vec3){NAN, 0.0f, 0.0f}) == HENKA_INTERACTION_RESULT_UNAVAILABLE);
    HENKA_TEST_ASSERT(henka_scene_can_interact(scene, second, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_INTERACTION_RESULT_AVAILABLE);
    HENKA_TEST_ASSERT(henka_scene_get_entity_info(scene, second, &info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(info.entity == second);
    HENKA_TEST_ASSERT(strcmp(info.tag, "marker") == 0);
    ray.origin = (henka_vec3){1.0f, 0.5f, 3.0f};
    ray.direction = (henka_vec3){0.0f, 0.0f, 0.0f};
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    ray.direction = (henka_vec3){NAN, 0.0f, -1.0f};
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    ray.origin = (henka_vec3){NAN, 0.5f, 3.0f};
    ray.direction = (henka_vec3){0.0f, 0.0f, -1.0f};
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_ERROR_INVALID_ARGUMENT);

    ray.origin = (henka_vec3){1.0f, 0.5f, 3.0f};
    ray.direction = henka_vec3_normalize((henka_vec3){0.0f, 0.0f, -1.0f});
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    ray.direction = (henka_vec3){0.0f, 0.0f, -FLT_MAX};
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    HENKA_TEST_ASSERT(henka_scene_translate_entity(scene, second, (henka_vec3){2.0f, 0.0f, -1.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_bounds(scene, second, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.x, 3.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.z, -1.0f, 0.0001f);
    ray.origin = (henka_vec3){3.0f, 0.5f, 2.0f};
    ray.direction = henka_vec3_normalize((henka_vec3){0.0f, 0.0f, -1.0f});
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    HENKA_TEST_ASSERT(henka_scene_scale_entity(scene, second, (henka_vec3){1.5f, 2.0f, 1.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_bounds(scene, second, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(bounds.extents.x > 0.75f);
    HENKA_TEST_ASSERT(bounds.extents.y >= 1.0f);
    ray.origin = (henka_vec3){10.0f, 0.0f, 3.0f};
    ray.direction = henka_vec3_normalize((henka_vec3){0.0f, 0.0f, -1.0f});
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, second, false) == HENKA_SUCCESS);
    ray.origin = (henka_vec3){3.0f, 0.5f, 2.0f};
    ray.direction = henka_vec3_normalize((henka_vec3){0.0f, 0.0f, -1.0f});
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, second, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_name(scene, second, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_name(scene, second) == NULL);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, HENKA_INVALID_ENTITY, &read_back) == HENKA_ERROR_INVALID_ARGUMENT);
    transform = henka_transform_identity();
    transform.position.x = NAN;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, second, transform) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_translate_entity(scene, HENKA_INVALID_ENTITY, (henka_vec3){1.0f, 0.0f, 0.0f}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_rotate_entity(scene, HENKA_INVALID_ENTITY, henka_quat_identity()) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_scale_entity(scene, second, (henka_vec3){INFINITY, 1.0f, 1.0f}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(scene, NULL, &found) == HENKA_ERROR_INVALID_ARGUMENT);
    memset(overlong_text, 'n', sizeof(overlong_text));
    overlong_text[sizeof(overlong_text) - 1U] = '\0';
    HENKA_TEST_ASSERT(henka_scene_set_entity_name(scene, second, overlong_text) == HENKA_ERROR_INVALID_ARGUMENT);

    cloned_scene = NULL;
    HENKA_TEST_ASSERT(henka_scene_clone(scene, &cloned_scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(cloned_scene != NULL);
    HENKA_TEST_ASSERT(cloned_scene != scene);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(cloned_scene) ==
        henka_scene_get_entity_count(scene));
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(cloned_scene, second));
    HENKA_TEST_ASSERT(henka_scene_get_entity_info(cloned_scene, second, &info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(info.entity == second);
    HENKA_TEST_ASSERT(info.tag != NULL && strcmp(info.tag, "marker") == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material(cloned_scene, second, &read_material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(read_material.name, material_name) == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_interaction(cloned_scene, second, &read_interaction) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_interaction.prompt != NULL && strcmp(read_interaction.prompt, "Inspect sample") == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_selection_owner(cloned_scene, second, &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == first);
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
        cloned_scene,
        second,
        (henka_transform){
            (henka_vec3){42.0f, 0.0f, 0.0f},
            henka_quat_identity(),
            (henka_vec3){1.0f, 1.0f, 1.0f}}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, second, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_back.position.x != 42.0f);
    henka_scene_destroy(cloned_scene);

    henka_scene_destroy_entity(scene, first);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, first));
    HENKA_TEST_ASSERT(henka_scene_get_entity_selection_owner(scene, second, &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 3U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 0U) == second);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 1U) == helper);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 2U) == selection_child);

    henka_scene_destroy(scene);

    henka_test_scene_capacity_growth();
}
