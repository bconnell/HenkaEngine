from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one anchor, found {count}")
    return text.replace(old, new, 1)


def replace_exact_count(text: str, old: str, new: str, expected: int, label: str) -> str:
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"{label}: expected {expected} anchors, found {count}")
    return text.replace(old, new)


def rewrite_function(text: str, token: str, rewrite) -> str:
    start = text.find(token)
    if start < 0:
        raise RuntimeError(f"function not found: {token}")
    next_start = text.find("\nstatic ", start + len(token))
    if next_start < 0:
        raise RuntimeError(f"could not bound function: {token}")
    end = next_start + 1
    return text[:start] + rewrite(text[start:end]) + text[end:]


# -----------------------------------------------------------------------------
# tests/test_sandbox3d_game_authoring.c
# Replace obsolete toy Material shaders with the real basic_lit contract and
# use reloadable glTF material definitions for both Prefab material workflows.
# -----------------------------------------------------------------------------
test_path = Path("tests/test_sandbox3d_game_authoring.c")
test_text = test_path.read_text(encoding="utf-8")

first_fn = "static bool test_prefab_material_override_persists_through_authoring(void)"
second_fn = "static bool test_prefab_apply_revert_participates_in_history(void)"

copy_helper = r'''static bool test_prefab_copy_file(
    const char* source_path,
    const char* destination_path)
{
    FILE* source = NULL;
    FILE* destination = NULL;
    unsigned char buffer[4096];
    size_t count;
    bool copied = true;

    if (source_path == NULL || destination_path == NULL)
    {
        return false;
    }
#if defined(_WIN32)
    if (fopen_s(&source, source_path, "rb") != 0)
    {
        source = NULL;
    }
    if (fopen_s(&destination, destination_path, "wb") != 0)
    {
        destination = NULL;
    }
#else
    source = fopen(source_path, "rb");
    destination = fopen(destination_path, "wb");
#endif
    if (source == NULL || destination == NULL)
    {
        if (source != NULL)
        {
            (void)fclose(source);
        }
        if (destination != NULL)
        {
            (void)fclose(destination);
        }
        return false;
    }
    while ((count = fread(buffer, 1U, sizeof(buffer), source)) > 0U)
    {
        if (fwrite(buffer, 1U, count, destination) != count)
        {
            copied = false;
            break;
        }
    }
    if (ferror(source))
    {
        copied = false;
    }
    if (fclose(source) != 0)
    {
        copied = false;
    }
    if (fclose(destination) != 0)
    {
        copied = false;
    }
    return copied;
}

'''

test_text = replace_once(
    test_text,
    first_fn,
    copy_helper + first_fn,
    "insert Prefab fixture copy helper",
)


def rewrite_first(fn: str) -> str:
    old_identity = '''    const char* material_identity =
        "materials/prefab-instance-authoring-persistence";
'''
    new_identity = '''    const char* material_gltf_path =
        "build/test_tmp/prefab_material_override_test.gltf";
    const char* material_identity = "prefab_material_override_test.gltf";
    static const char* material_gltf =
        "{\\\"asset\\\":{\\\"version\\\":\\\"2.0\\\"},"
        "\\\"buffers\\\":[{\\\"uri\\\":\\\"data:application/octet-stream;base64,"
        "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA\\\",\\\"byteLength\\\":36}],"
        "\\\"bufferViews\\\":[{\\\"buffer\\\":0,\\\"byteLength\\\":36}],"
        "\\\"accessors\\\":[{\\\"bufferView\\\":0,\\\"componentType\\\":5126,"
        "\\\"count\\\":3,\\\"type\\\":\\\"VEC3\\\"}],"
        "\\\"materials\\\":[{\\\"name\\\":\\\"Prefab Authoring Source Material\\\","
        "\\\"pbrMetallicRoughness\\\":{\\\"baseColorFactor\\\":[0.16,0.28,0.44,1.0],"
        "\\\"metallicFactor\\\":0.0,\\\"roughnessFactor\\\":0.63}}],"
        "\\\"meshes\\\":[{\\\"primitives\\\":[{\\\"attributes\\\":{\\\"POSITION\\\":0},"
        "\\\"material\\\":0}]}]}";
'''
    fn = replace_once(fn, old_identity, new_identity, "first material identity")

    old_fixture = '''    if (!test_prefab_write_text_file(
            vertex_shader_path,
            "#version 330 core\\n"
            "layout(location = 0) in vec3 a_position;\\n"
            "void main() { gl_Position = vec4(a_position, 1.0); }\\n") ||
        !test_prefab_write_text_file(
            fragment_shader_path,
            "#version 330 core\\n"
            "out vec4 frag_color;\\n"
            "void main() { frag_color = vec4(1.0); }\\n"))
    {
        goto cleanup;
    }
'''
    new_fixture = '''    if (!test_prefab_copy_file(
            "assets/shaders/basic_lit.vert",
            vertex_shader_path) ||
        !test_prefab_copy_file(
            "assets/shaders/basic_lit.frag",
            fragment_shader_path) ||
        !test_prefab_write_text_file(
            material_gltf_path,
            material_gltf))
    {
        goto cleanup;
    }
'''
    fn = replace_once(fn, old_fixture, new_fixture, "first shader/material fixture")

    old_setup = '''    source_material.shader = shader;
    source_material.name = "Prefab Authoring Source Material";
    source_material.base_color = (henka_vec4){0.16f, 0.28f, 0.44f, 1.0f};
    source_material.roughness = 0.63f;
    if (henka_assets_adopt_runtime_material(
            assets,
            material_identity,
            &source_material,
            &material_asset) != HENKA_SUCCESS ||
        material_asset == NULL ||
        henka_scene_create(&source_scene) != HENKA_SUCCESS)
'''
    new_setup = '''    if (henka_assets_load_gltf_material_asset(
            assets,
            material_identity,
            shader,
            &material_asset) != HENKA_SUCCESS ||
        material_asset == NULL ||
        henka_assets_get_material_asset_material(
            material_asset,
            &source_material) != HENKA_SUCCESS ||
        henka_scene_create(&source_scene) != HENKA_SUCCESS)
'''
    fn = replace_once(fn, old_setup, new_setup, "first reloadable material setup")
    fn = replace_exact_count(
        fn,
        "    (void)remove(fragment_shader_path);",
        "    (void)remove(fragment_shader_path);\n    (void)remove(material_gltf_path);",
        2,
        "first glTF cleanup",
    )
    return fn


test_text = rewrite_function(test_text, first_fn, rewrite_first)


def rewrite_second(fn: str) -> str:
    old_identity = '''    const char* material_identity =
        "materials/prefab-apply-revert-history";
'''
    new_identity = '''    const char* material_gltf_path =
        "build/test_tmp/prefab_apply_revert_history.gltf";
    const char* material_identity = "prefab_apply_revert_history.gltf";
    static const char* material_gltf =
        "{\\\"asset\\\":{\\\"version\\\":\\\"2.0\\\"},"
        "\\\"buffers\\\":[{\\\"uri\\\":\\\"data:application/octet-stream;base64,"
        "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA\\\",\\\"byteLength\\\":36}],"
        "\\\"bufferViews\\\":[{\\\"buffer\\\":0,\\\"byteLength\\\":36}],"
        "\\\"accessors\\\":[{\\\"bufferView\\\":0,\\\"componentType\\\":5126,"
        "\\\"count\\\":3,\\\"type\\\":\\\"VEC3\\\"}],"
        "\\\"materials\\\":[{\\\"name\\\":\\\"Prefab Apply Revert Source Material\\\","
        "\\\"pbrMetallicRoughness\\\":{\\\"baseColorFactor\\\":[0.18,0.31,0.47,1.0],"
        "\\\"metallicFactor\\\":0.0,\\\"roughnessFactor\\\":0.68}}],"
        "\\\"meshes\\\":[{\\\"primitives\\\":[{\\\"attributes\\\":{\\\"POSITION\\\":0},"
        "\\\"material\\\":0}]}]}";
'''
    fn = replace_once(fn, old_identity, new_identity, "second material identity")

    old_fixture = '''    if (!test_prefab_write_text_file(
            vertex_shader_path,
            "#version 330 core\\n"
            "layout(location = 0) in vec3 a_position;\\n"
            "void main() { gl_Position = vec4(a_position, 1.0); }\\n") ||
        !test_prefab_write_text_file(
            fragment_shader_path,
            "#version 330 core\\n"
            "out vec4 frag_color;\\n"
            "void main() { frag_color = vec4(1.0); }\\n"))
    {
        goto cleanup;
    }
'''
    new_fixture = '''    if (!test_prefab_copy_file(
            "assets/shaders/basic_lit.vert",
            vertex_shader_path) ||
        !test_prefab_copy_file(
            "assets/shaders/basic_lit.frag",
            fragment_shader_path) ||
        !test_prefab_write_text_file(
            material_gltf_path,
            material_gltf))
    {
        goto cleanup;
    }
'''
    fn = replace_once(fn, old_fixture, new_fixture, "second shader/material fixture")

    old_setup = '''    source_material.shader = shader;
    source_material.name = "Prefab Apply Revert Source Material";
    source_material.base_color = (henka_vec4){0.18f, 0.31f, 0.47f, 1.0f};
    source_material.roughness = 0.68f;
    if (henka_assets_adopt_runtime_material(
            assets,
            material_identity,
            &source_material,
            &material_asset) != HENKA_SUCCESS ||
        material_asset == NULL ||
        henka_scene_create(&source_scene) != HENKA_SUCCESS)
'''
    new_setup = '''    if (henka_assets_load_gltf_material_asset(
            assets,
            material_identity,
            shader,
            &material_asset) != HENKA_SUCCESS ||
        material_asset == NULL ||
        henka_assets_get_material_asset_material(
            material_asset,
            &source_material) != HENKA_SUCCESS ||
        henka_scene_create(&source_scene) != HENKA_SUCCESS)
'''
    fn = replace_once(fn, old_setup, new_setup, "second reloadable material setup")
    fn = replace_exact_count(
        fn,
        "    (void)remove(fragment_shader_path);",
        "    (void)remove(fragment_shader_path);\n    (void)remove(material_gltf_path);",
        2,
        "second glTF cleanup",
    )
    return fn


test_text = rewrite_function(test_text, second_fn, rewrite_second)
test_path.write_text(test_text, encoding="utf-8", newline="\n")


# -----------------------------------------------------------------------------
# examples/sandbox3d/game_authoring.c
# Save must merge Game Authoring's canonical manager-aware renderer/material
# capture after the generic bridge synchronization.
# -----------------------------------------------------------------------------
game_path = Path("examples/sandbox3d/game_authoring.c")
game_text = game_path.read_text(encoding="utf-8")
old_save = '''        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_scene_document_bridge_sync_object(
                candidate_bridge,
                document_id);
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_game_authoring_sync_prefab_transform_override(
                authoring,
                candidate_document,
                document_id);
        }
'''
new_save = '''        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_scene_document_bridge_sync_object(
                candidate_bridge,
                document_id);
        }
        if (result == HENKA_SUCCESS)
        {
            henka_scene_document_object candidate_object;
            henka_scene_document_object live_object;

            /*
             * The generic bridge intentionally owns only generic presentation
             * synchronization. Manager-aware material identity, scalar
             * overrides, and texture override paths are captured by Game
             * Authoring's canonical object builder. Merge only renderer state
             * so Prefab provenance, hierarchy, physics, behaviors, and other
             * document-owned fields remain authoritative in the candidate.
             */
            result = henka_scene_document_get_object(
                candidate_document,
                document_id,
                &candidate_object);
            if (result == HENKA_SUCCESS)
            {
                result = sandbox3d_game_authoring_build_object(
                    authoring,
                    entity,
                    &live_object);
            }
            if (result == HENKA_SUCCESS)
            {
                candidate_object.renderer = live_object.renderer;
                result = henka_scene_document_set_object(
                    candidate_document,
                    &candidate_object);
            }
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_game_authoring_sync_prefab_transform_override(
                authoring,
                candidate_document,
                document_id);
        }
'''
game_text = replace_once(game_text, old_save, new_save, "Game Authoring save material capture")
game_path.write_text(game_text, encoding="utf-8", newline="\n")


# -----------------------------------------------------------------------------
# engine/src/assets/assets.c
# A null shader may resolve an already-cached canonical glTF material asset.
# First load still requires an explicit shader, and a supplied mismatched shader
# remains rejected. This lets Prefab deserialization reuse manager authority
# instead of inventing a second shader/material truth.
# -----------------------------------------------------------------------------
assets_path = Path("engine/src/assets/assets.c")
assets_text = assets_path.read_text(encoding="utf-8")
material_fn = "henka_result henka_assets_load_gltf_material_asset("


def rewrite_material_loader(fn: str) -> str:
    old_validation = '''    if (manager == NULL || path == NULL || shader == NULL || out_asset == NULL || *out_asset != NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
'''
    new_validation = '''    if (manager == NULL || path == NULL || out_asset == NULL || *out_asset != NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
'''
    fn = replace_once(fn, old_validation, new_validation, "material loader initial validation")

    old_cached = '''    asset = henka_asset_manager_find_material_entry(manager, key);
    if (asset != NULL)
    {
        if (asset->material.shader != shader)
        {
            henka_free(key);
            henka_free(source_path);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        *out_asset = asset;
        henka_free(key);
        henka_free(source_path);
        return HENKA_SUCCESS;
    }

    result = henka_assets_build_gltf_material_instance(manager, source_path, shader, &candidate);
'''
    new_cached = '''    asset = henka_asset_manager_find_material_entry(manager, key);
    if (asset != NULL)
    {
        if (shader != NULL && asset->material.shader != shader)
        {
            henka_free(key);
            henka_free(source_path);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        *out_asset = asset;
        henka_free(key);
        henka_free(source_path);
        return HENKA_SUCCESS;
    }
    if (shader == NULL)
    {
        henka_free(key);
        henka_free(source_path);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_assets_build_gltf_material_instance(manager, source_path, shader, &candidate);
'''
    return replace_once(fn, old_cached, new_cached, "cached material shader authority")


assets_text = rewrite_function(assets_text, material_fn, rewrite_material_loader)
assets_path.write_text(assets_text, encoding="utf-8", newline="\n")


# -----------------------------------------------------------------------------
# engine/include/henka/assets.h
# Document the cache-only null-shader behavior explicitly.
# -----------------------------------------------------------------------------
header_path = Path("engine/include/henka/assets.h")
header_text = header_path.read_text(encoding="utf-8")
old_comment = '''/* glTF material loads require an initialized empty output slot. The returned
 * asset is borrowed and manager-owned; rejected or failed loads preserve a
 * non-empty caller slot and leave an empty slot empty. */
'''
new_comment = '''/* glTF material loads require an initialized empty output slot. The returned
 * asset is borrowed and manager-owned; rejected or failed loads preserve a
 * non-empty caller slot and leave an empty slot empty. A first load requires
 * a non-null shader. A null shader may resolve an already-cached canonical
 * material path, preserving the manager-owned definition's shader authority. */
'''
header_text = replace_once(header_text, old_comment, new_comment, "glTF material load contract docs")
header_path.write_text(header_text, encoding="utf-8", newline="\n")


# Final semantic gates.
checks = {
    test_path: [
        "test_prefab_copy_file(",
        "prefab_material_override_test.gltf",
        "prefab_apply_revert_history.gltf",
        "henka_assets_load_gltf_material_asset(",
        "assets/shaders/basic_lit.vert",
    ],
    game_path: [
        "candidate_object.renderer = live_object.renderer;",
        "sandbox3d_game_authoring_build_object(",
    ],
    assets_path: [
        "if (shader != NULL && asset->material.shader != shader)",
        "if (shader == NULL)",
    ],
    header_path: [
        "A null shader may resolve an already-cached canonical",
    ],
}
for path, tokens in checks.items():
    final = path.read_text(encoding="utf-8")
    for token in tokens:
        if token not in final:
            raise RuntimeError(f"{path}: missing final token: {token}")

print("Prefab material persistence repair transformation completed.")
