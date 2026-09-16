from pathlib import Path

path = Path("tests/test_sandbox3d_game_authoring.c")
text = path.read_text(encoding="utf-8")
function_token = "static bool test_prefab_material_override_persists_through_authoring(void)"
start = text.find(function_token)
if start < 0:
    raise RuntimeError("target test function not found")
end = text.find("\nstatic ", start + len(function_token))
if end < 0:
    raise RuntimeError("target test function end not found")
end += 1
fn = text[start:end]

block_start_token = "    source_root = henka_scene_create_entity_named("
block_end_token = "    override_material = source_material;"
block_start = fn.find(block_start_token)
block_end = fn.find(block_end_token, block_start)
if block_start < 0 or block_end < 0:
    raise RuntimeError("checkpoint-5 setup block bounds not found")
old_block = fn[block_start:block_end]

for token in (
    "henka_scene_set_entity_material_asset(",
    "henka_assets_refresh_scene_material_bindings(",
    "refreshed_count != 1U",
    "henka_prefab_create_from_scene(",
    "henka_prefab_set_asset_path(",
    "henka_prefab_save_file(",
    "sandbox3d_game_authoring_create_with_engine(",
    "sandbox3d_game_authoring_instantiate_prefab_asset(",
    "sandbox3d_game_authoring_get_object_for_entity(",
    "loaded_object.source.asset_kind != HENKA_SCENE_DOCUMENT_ASSET_PREFAB",
):
    if token not in old_block:
        raise RuntimeError(f"checkpoint-5 source shape changed; missing {token}")

new_block = r'''    source_root = henka_scene_create_entity_named(
        source_scene, "Prefab Material Persistence Root");
    if (source_root == HENKA_INVALID_ENTITY)
    {
        fprintf(stderr, "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.1 source_root_create\n");
        goto cleanup;
    }
    if (henka_scene_set_entity_material_asset(
            source_scene, source_root, material_asset) != HENKA_SUCCESS)
    {
        fprintf(stderr, "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.2 source_material_asset_attach\n");
        goto cleanup;
    }
    if (henka_assets_refresh_scene_material_bindings(
            assets, source_scene, &refreshed_count) != HENKA_SUCCESS)
    {
        fprintf(stderr, "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.3 refresh_material_bindings_result\n");
        goto cleanup;
    }
    if (refreshed_count != 1U)
    {
        fprintf(stderr,
            "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.4 refresh_count actual=%zu expected=1\n",
            refreshed_count);
        goto cleanup;
    }
    if (henka_prefab_create_from_scene(
            source_scene, source_root, &prefab) != HENKA_SUCCESS)
    {
        fprintf(stderr, "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.5 prefab_create_from_scene\n");
        goto cleanup;
    }
    if (henka_prefab_set_asset_path(prefab, prefab_identity) != HENKA_SUCCESS)
    {
        fprintf(stderr, "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.6 prefab_set_asset_path\n");
        goto cleanup;
    }
    if (henka_prefab_save_file(
            prefab, assets, project_root, prefab_identity) != HENKA_SUCCESS)
    {
        fprintf(stderr, "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.7 prefab_save_file\n");
        goto cleanup;
    }
    if (henka_scene_create(&scene) != HENKA_SUCCESS)
    {
        fprintf(stderr, "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.8 target_scene_create\n");
        goto cleanup;
    }
    if (sandbox3d_game_authoring_create_with_engine(
            scene, scene_path, engine, &authoring) != HENKA_SUCCESS)
    {
        fprintf(stderr, "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.9 authoring_create\n");
        goto cleanup;
    }
    {
        const henka_result instantiate_result =
            sandbox3d_game_authoring_instantiate_prefab_asset(
                authoring,
                prefab_identity,
                henka_transform_identity(),
                &placed_root);
        if (instantiate_result != HENKA_SUCCESS)
        {
            fprintf(stderr,
                "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.10 prefab_instantiate result=%d\n",
                (int)instantiate_result);
            goto cleanup;
        }
    }
    if (placed_root == HENKA_INVALID_ENTITY)
    {
        fprintf(stderr, "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.11 placed_root_invalid\n");
        goto cleanup;
    }
    if (sandbox3d_game_authoring_get_object_for_entity(
            authoring,
            placed_root,
            &placed_id,
            &loaded_object) != HENKA_SUCCESS)
    {
        fprintf(stderr, "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.12 get_placed_object\n");
        goto cleanup;
    }
    if (placed_id == HENKA_INVALID_SCENE_DOCUMENT_ID)
    {
        fprintf(stderr, "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.13 placed_document_id_invalid\n");
        goto cleanup;
    }
    if (loaded_object.source.asset_kind != HENKA_SCENE_DOCUMENT_ASSET_PREFAB)
    {
        fprintf(stderr,
            "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint=5.14 asset_kind actual=%d expected=%d\n",
            (int)loaded_object.source.asset_kind,
            (int)HENKA_SCENE_DOCUMENT_ASSET_PREFAB);
        goto cleanup;
    }

'''

instrumented_fn = fn[:block_start] + new_block + fn[block_end:]
path.write_text(text[:start] + instrumented_fn + text[end:], encoding="utf-8", newline="\n")
print("Checkpoint 5 split into 5.1 through 5.14 diagnostics.")
