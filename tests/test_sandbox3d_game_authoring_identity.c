#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <henka/core.h>
#include <henka/scene.h>
#include <henka/scene_document.h>

#include "../examples/sandbox3d/game_authoring.h"

int main(void)
{
    const char* relative_path =
        "build/test_tmp/game_authoring_identity_watermark.hscene";
    const henka_scene_document_id watermark_id = UINT64_C(500);
    henka_scene* scene = NULL;
    henka_camera camera;
    sandbox3d_game_authoring* authoring = NULL;
    henka_scene_document* replacement = NULL;
    henka_scene_document* loaded = NULL;
    henka_scene_document_object watermark_object;
    henka_scene_document_object loaded_object;
    henka_material authored_material;
    henka_material changed_material;
    henka_material restored_material;
    henka_scene_document_id replacement_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id new_object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id loaded_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id unchanged_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id asset_override_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_entity new_entity = HENKA_INVALID_ENTITY;
    henka_entity asset_override_entity = HENKA_INVALID_ENTITY;
    henka_result load_result = HENKA_ERROR_INVALID_ARGUMENT;
    henka_result register_result = HENKA_ERROR_INVALID_ARGUMENT;
    int result = 1;

    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD,
        1.0f,
        0.1f,
        100.0f);
    if (henka_scene_create(&scene) != HENKA_SUCCESS ||
        henka_scene_set_camera(scene, &camera) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_create(scene, relative_path, &authoring) != HENKA_SUCCESS ||
        henka_scene_document_create(&replacement) != HENKA_SUCCESS)
    {
        fprintf(stderr, "game authoring identity test failed during setup\n");
        goto cleanup;
    }

    watermark_object = henka_scene_document_object_default();
    watermark_object.id = watermark_id;
    if (henka_scene_document_add_object(
            replacement, &watermark_object, &replacement_id) != HENKA_SUCCESS ||
        replacement_id != watermark_id ||
        henka_scene_document_remove_object(replacement, replacement_id) != HENKA_SUCCESS ||
        henka_scene_document_save_file(replacement, ".", relative_path) != HENKA_SUCCESS ||
        henka_scene_document_create(&loaded) != HENKA_SUCCESS ||
        henka_scene_document_load_file(loaded, ".", relative_path) != HENKA_SUCCESS)
    {
        fprintf(stderr, "game authoring identity test failed during file setup\n");
        goto cleanup;
    }
    loaded_object = henka_scene_document_object_default();
    if (henka_scene_document_add_object(
            loaded, &loaded_object, &loaded_id) != HENKA_SUCCESS ||
        loaded_id != watermark_id + 1U ||
        (load_result = sandbox3d_game_authoring_load(authoring, ".")) != HENKA_SUCCESS ||
        (new_entity = henka_scene_create_entity_named(
             scene, "Identity Watermark New Object")) == HENKA_INVALID_ENTITY)
    {
        fprintf(
            stderr,
            "game authoring identity test failed to preserve watermark "
            "(loaded=%llu load=%d register=%d new=%llu)\n",
            (unsigned long long)loaded_id,
            (int)load_result,
            (int)register_result,
            (unsigned long long)new_object_id);
        goto cleanup;
    }

    authored_material = henka_material_default();
    authored_material.shader = (henka_shader*)1;
    authored_material.name = "Identity Authored Material";
    authored_material.base_color = (henka_vec4){0.18f, 0.42f, 0.86f, 1.0f};
    authored_material.metallic = 0.72f;
    authored_material.roughness = 0.19f;
    authored_material.emissive_color = (henka_vec3){0.03f, 0.07f, 0.11f};
    authored_material.emissive_strength = 2.4f;
    if (henka_scene_set_entity_material(scene, new_entity, authored_material) != HENKA_SUCCESS ||
        (register_result = sandbox3d_game_authoring_register_entity(
            authoring, new_entity, &new_object_id)) != HENKA_SUCCESS ||
        new_object_id < watermark_id + 1U ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, new_entity, &unchanged_id, &loaded_object) != HENKA_SUCCESS ||
        unchanged_id != new_object_id ||
        fabsf(loaded_object.renderer.base_color.x - authored_material.base_color.x) > 0.0001f ||
        fabsf(loaded_object.renderer.base_color.y - authored_material.base_color.y) > 0.0001f ||
        fabsf(loaded_object.renderer.base_color.z - authored_material.base_color.z) > 0.0001f ||
        fabsf(loaded_object.renderer.metallic - authored_material.metallic) > 0.0001f ||
        fabsf(loaded_object.renderer.roughness - authored_material.roughness) > 0.0001f ||
        fabsf(loaded_object.renderer.emissive.x - authored_material.emissive_color.x) > 0.0001f ||
        fabsf(loaded_object.renderer.emissive.y - authored_material.emissive_color.y) > 0.0001f ||
        fabsf(loaded_object.renderer.emissive.z - authored_material.emissive_color.z) > 0.0001f ||
        fabsf(loaded_object.renderer.emissive_strength - authored_material.emissive_strength) > 0.0001f)
    {
        fprintf(stderr, "game authoring identity test failed to capture renderer authority\n");
        goto cleanup;
    }

    asset_override_entity = henka_scene_create_entity_named(
        scene,
        "Asset Override Boundary");
    if (asset_override_entity == HENKA_INVALID_ENTITY ||
        henka_scene_set_entity_material_asset(
            scene,
            asset_override_entity,
            (const henka_material_asset*)(uintptr_t)1U) != HENKA_SUCCESS ||
        henka_scene_set_entity_material(
            scene,
            asset_override_entity,
            authored_material) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_register_entity(
            authoring,
            asset_override_entity,
            &asset_override_id) != HENKA_ERROR_INVALID_ARGUMENT ||
        asset_override_id != HENKA_INVALID_SCENE_DOCUMENT_ID)
    {
        fprintf(stderr, "game authoring identity test failed to reject asset override capture\n");
        goto cleanup;
    }

    if (sandbox3d_game_authoring_save(authoring, ".") != HENKA_SUCCESS ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring,
            new_entity,
            &unchanged_id,
            &loaded_object) != HENKA_SUCCESS ||
        unchanged_id != new_object_id ||
        strcmp(loaded_object.name, "Identity Watermark New Object") != 0)
    {
        fprintf(stderr, "game authoring identity test failed during baseline save\n");
        goto cleanup;
    }

    changed_material = authored_material;
    changed_material.name = "Changed Runtime Material";
    changed_material.base_color = (henka_vec4){0.91f, 0.12f, 0.07f, 1.0f};
    changed_material.metallic = 0.04f;
    changed_material.roughness = 0.88f;
    changed_material.emissive_color = (henka_vec3){0.0f, 0.0f, 0.0f};
    changed_material.emissive_strength = 0.0f;
    if (henka_scene_set_entity_material(scene, new_entity, changed_material) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(authoring, ".") != HENKA_SUCCESS ||
        henka_scene_get_entity_material(scene, new_entity, &restored_material) != HENKA_SUCCESS ||
        fabsf(restored_material.base_color.x - authored_material.base_color.x) > 0.0001f ||
        fabsf(restored_material.base_color.y - authored_material.base_color.y) > 0.0001f ||
        fabsf(restored_material.base_color.z - authored_material.base_color.z) > 0.0001f ||
        fabsf(restored_material.metallic - authored_material.metallic) > 0.0001f ||
        fabsf(restored_material.roughness - authored_material.roughness) > 0.0001f ||
        fabsf(restored_material.emissive_color.x - authored_material.emissive_color.x) > 0.0001f ||
        fabsf(restored_material.emissive_color.y - authored_material.emissive_color.y) > 0.0001f ||
        fabsf(restored_material.emissive_color.z - authored_material.emissive_color.z) > 0.0001f ||
        fabsf(restored_material.emissive_strength - authored_material.emissive_strength) > 0.0001f)
    {
        fprintf(stderr, "game authoring identity test failed to materialize renderer authority\n");
        goto cleanup;
    }

    henka_scene_document_destroy(replacement);
    replacement = NULL;
    if (henka_scene_document_create(&replacement) != HENKA_SUCCESS)
    {
        fprintf(stderr, "game authoring identity test failed creating invalid candidate\n");
        goto cleanup;
    }
    watermark_object.id = watermark_id + 2U;
    if (henka_scene_document_add_object(
            replacement, &watermark_object, &replacement_id) != HENKA_SUCCESS ||
        replacement_id != watermark_id + 2U ||
        henka_scene_document_save_file(replacement, ".", relative_path) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(authoring, ".") != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring,
            new_entity,
            &unchanged_id,
            &loaded_object) != HENKA_SUCCESS ||
        unchanged_id != new_object_id ||
        strcmp(loaded_object.name, "Identity Watermark New Object") != 0)
    {
        fprintf(stderr, "game authoring identity test failed rejecting unexpected ID\n");
        goto cleanup;
    }

    henka_scene_document_destroy(replacement);
    replacement = NULL;
    if (henka_scene_document_create(&replacement) != HENKA_SUCCESS ||
        henka_scene_document_save_file(replacement, ".", relative_path) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(authoring, ".") != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring,
            new_entity,
            &unchanged_id,
            &loaded_object) != HENKA_SUCCESS ||
        unchanged_id != new_object_id ||
        strcmp(loaded_object.name, "Identity Watermark New Object") != 0)
    {
        fprintf(stderr, "game authoring identity test failed rejecting missing ID\n");
        goto cleanup;
    }

    result = 0;

cleanup:
    (void)remove(relative_path);
    henka_scene_document_destroy(loaded);
    henka_scene_document_destroy(replacement);
    sandbox3d_game_authoring_destroy(authoring);
    henka_scene_destroy_entity(scene, asset_override_entity);
    henka_scene_destroy(scene);
    return result;
}
