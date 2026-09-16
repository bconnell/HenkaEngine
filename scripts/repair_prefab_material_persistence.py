from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8").replace("\r\n", "\n").replace("\r", "\n")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_unique(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one anchor, found {count}")
    return text.replace(old, new, 1)


def function_bounds(text: str, fixture: str) -> tuple[int, int]:
    fixture_index = text.find(fixture)
    if fixture_index < 0 or text.find(fixture, fixture_index + len(fixture)) >= 0:
        raise RuntimeError(f"Fixture missing or ambiguous: {fixture}")
    start = text.rfind("static bool ", 0, fixture_index)
    next_fn = text.find("\nstatic bool ", fixture_index + len(fixture))
    if start < 0:
        raise RuntimeError(f"Containing function missing: {fixture}")
    end = len(text) if next_fn < 0 else next_fn + 1
    return start, end


def rewrite_function(text: str, fixture: str, callback) -> str:
    start, end = function_bounds(text, fixture)
    return text[:start] + callback(text[start:end]) + text[end:]


def replace_shader_fixture(function_text: str, label: str) -> str:
    start_token = "    if (!test_prefab_write_text_file("
    start = function_text.find(start_token)
    if start < 0 or function_text.find(start_token, start + len(start_token)) >= 0:
        raise RuntimeError(f"{label}: toy shader block missing or ambiguous")
    end_token = "\n\n    memset(&config, 0, sizeof(config));"
    end = function_text.find(end_token, start)
    if end < 0:
        raise RuntimeError(f"{label}: shader block end missing")
    replacement = '''    if (!test_prefab_copy_file(
            "assets/shaders/basic_lit.vert",
            vertex_shader_path) ||
        !test_prefab_copy_file(
            "assets/shaders/basic_lit.frag",
            fragment_shader_path))
    {
        goto cleanup;
    }'''
    return function_text[:start] + replacement + function_text[end:]


# -----------------------------------------------------------------------------
# 1. Asset manager: a NULL shader may resolve an already-loaded canonical
#    glTF material asset. A cache miss still requires shader authority.
# -----------------------------------------------------------------------------
assets_c_path = "engine/src/assets/assets.c"
assets_c = read(assets_c_path)
old_material_loader = '''    if (manager == NULL || path == NULL || shader == NULL || out_asset == NULL || *out_asset != NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS) return result;
    result = henka_assets_normalize_source_path(path, &source_path);
    if (result != HENKA_SUCCESS) { henka_free(key); return result; }
    asset = henka_asset_manager_find_material_entry(manager, key);
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

    result = henka_assets_build_gltf_material_instance(manager, source_path, shader, &candidate);'''
new_material_loader = '''    if (manager == NULL || path == NULL || out_asset == NULL || *out_asset != NULL)
        return HENKA_ERROR_INVALID_ARGUMENT;
    result = henka_assets_make_canonical_key(path, &key);
    if (result != HENKA_SUCCESS) return result;
    result = henka_assets_normalize_source_path(path, &source_path);
    if (result != HENKA_SUCCESS) { henka_free(key); return result; }
    asset = henka_asset_manager_find_material_entry(manager, key);
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

    result = henka_assets_build_gltf_material_instance(manager, source_path, shader, &candidate);'''
assets_c = replace_unique(
    assets_c, old_material_loader, new_material_loader,
    "cached glTF material resolution")
write(assets_c_path, assets_c)

assets_h_path = "engine/include/henka/assets.h"
assets_h = read(assets_h_path)
old_material_docs = '''/* glTF material loads require an initialized empty output slot. The returned
 * asset is borrowed and manager-owned; rejected or failed loads preserve a
 * non-empty caller slot and leave an empty slot empty. */'''
new_material_docs = '''/* glTF material loads require an initialized empty output slot. The returned
 * asset is borrowed and manager-owned; rejected or failed loads preserve a
 * non-empty caller slot and leave an empty slot empty. A null shader may only
 * resolve a material definition already loaded under the same canonical path;
 * a cache miss still requires explicit shader authority. */'''
assets_h = replace_unique(
    assets_h, old_material_docs, new_material_docs,
    "glTF material loader contract documentation")
write(assets_h_path, assets_h)


# -----------------------------------------------------------------------------
# 2. Game Authoring Save: merge canonical live renderer/material capture after
#    generic bridge synchronization without overwriting Prefab/document fields.
# -----------------------------------------------------------------------------
game_path = "examples/sandbox3d/game_authoring.c"
game = read(game_path)
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
        }'''
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

            /* The generic bridge owns generic presentation synchronization.
             * Manager-aware material identity and supported material overrides
             * are captured by Game Authoring's canonical object builder. Merge
             * only renderer state so Prefab provenance, hierarchy, physics,
             * behaviors, and other document-owned fields stay authoritative. */
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
        }'''
game = replace_unique(game, old_save, new_save, "Game Authoring save material capture")
write(game_path, game)


# -----------------------------------------------------------------------------
# 3. Prefab Game Authoring tests: use real Material shaders and reloadable
#    file-backed glTF material definitions for persistence tests.
# -----------------------------------------------------------------------------
test_path = "tests/test_sandbox3d_game_authoring.c"
test = read(test_path)

helper_token = "static bool test_prefab_write_text_file("
helper_start = test.find(helper_token)
if helper_start < 0 or test.find(helper_token, helper_start + len(helper_token)) >= 0:
    raise RuntimeError("test_prefab_write_text_file helper missing or ambiguous")
helper_end = test.find("\nstatic bool ", helper_start + len(helper_token))
if helper_end < 0:
    raise RuntimeError("Unable to bound test_prefab_write_text_file helper")
copy_helper = r'''
static bool test_prefab_copy_file(
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
        if (source != NULL) (void)fclose(source);
        if (destination != NULL) (void)fclose(destination);
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
    if (ferror(source)) copied = false;
    if (fclose(source) != 0) copied = false;
    if (fclose(destination) != 0) copied = false;
    return copied;
}
'''
if "static bool test_prefab_copy_file(" in test:
    raise RuntimeError("copy helper unexpectedly already present")
test = test[:helper_end] + copy_helper + test[helper_end:]

first_fixture = '"build/test_tmp/prefab_material_override_test.vert"'
second_fixture = '"build/test_tmp/prefab_apply_revert_history.vert"'
test = rewrite_function(test, first_fixture, lambda f: replace_shader_fixture(f, "Prefab material persistence"))
test = rewrite_function(test, second_fixture, lambda f: replace_shader_fixture(f, "Prefab Apply/Revert history"))

first_identity_old = '''    const char* material_identity =
        "materials/prefab-instance-authoring-persistence";'''
first_identity_new = '''    const char* material_gltf_path =
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
        "\\\"material\\\":0}]}]}";'''
test = replace_unique(test, first_identity_old, first_identity_new, "first reloadable material identity")

second_identity_old = '''    const char* material_identity =
        "materials/prefab-apply-revert-history";'''
second_identity_new = '''    const char* material_gltf_path =
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
        "\\\"material\\\":0}]}]}";'''
test = replace_unique(test, second_identity_old, second_identity_new, "second reloadable material identity")

first_setup_old = '''    source_material.shader = shader;
    source_material.name = "Prefab Authoring Source Material";
    source_material.base_color = (henka_vec4){0.16f, 0.28f, 0.44f, 1.0f};
    source_material.roughness = 0.63f;
    if (henka_assets_adopt_runtime_material(
            assets,
            material_identity,
            &source_material,
            &material_asset) != HENKA_SUCCESS ||
        material_asset == NULL ||
        henka_scene_create(&source_scene) != HENKA_SUCCESS)'''
first_setup_new = '''    if (!test_prefab_write_text_file(material_gltf_path, material_gltf) ||
        henka_assets_load_gltf_material_asset(
            assets,
            material_identity,
            shader,
            &material_asset) != HENKA_SUCCESS ||
        material_asset == NULL ||
        henka_assets_get_material_asset_material(
            material_asset,
            &source_material) != HENKA_SUCCESS ||
        henka_scene_create(&source_scene) != HENKA_SUCCESS)'''
test = replace_unique(test, first_setup_old, first_setup_new, "first reloadable material setup")

second_setup_old = '''    source_material.shader = shader;
    source_material.name = "Prefab Apply Revert Source Material";
    source_material.base_color = (henka_vec4){0.18f, 0.31f, 0.47f, 1.0f};
    source_material.roughness = 0.68f;
    if (henka_assets_adopt_runtime_material(
            assets,
            material_identity,
            &source_material,
            &material_asset) != HENKA_SUCCESS ||
        material_asset == NULL ||
        henka_scene_create(&source_scene) != HENKA_SUCCESS)'''
second_setup_new = '''    if (!test_prefab_write_text_file(material_gltf_path, material_gltf) ||
        henka_assets_load_gltf_material_asset(
            assets,
            material_identity,
            shader,
            &material_asset) != HENKA_SUCCESS ||
        material_asset == NULL ||
        henka_assets_get_material_asset_material(
            material_asset,
            &source_material) != HENKA_SUCCESS ||
        henka_scene_create(&source_scene) != HENKA_SUCCESS)'''
test = replace_unique(test, second_setup_old, second_setup_new, "second reloadable material setup")

# Add glTF cleanup anywhere each target function already removes its fragment fixture.
def add_cleanup(function_text: str, label: str) -> str:
    old = "    (void)remove(fragment_shader_path);"
    count = function_text.count(old)
    if count != 2:
        raise RuntimeError(f"{label}: expected two fragment cleanup points, found {count}")
    return function_text.replace(old, old + "\n    (void)remove(material_gltf_path);")

test = rewrite_function(test, first_fixture, lambda f: add_cleanup(f, "first Prefab test"))
test = rewrite_function(test, second_fixture, lambda f: add_cleanup(f, "second Prefab test"))

for required in (
    "test_prefab_copy_file(",
    '"assets/shaders/basic_lit.vert"',
    '"assets/shaders/basic_lit.frag"',
    '"prefab_material_override_test.gltf"',
    '"prefab_apply_revert_history.gltf"',
    "henka_assets_load_gltf_material_asset(",
    "henka_assets_get_material_asset_material(",
):
    if required not in test:
        raise RuntimeError(f"test repair missing token: {required}")
write(test_path, test)

# Source-scope proof.
expected = {
    "engine/include/henka/assets.h",
    "engine/src/assets/assets.c",
    "examples/sandbox3d/game_authoring.c",
    "tests/test_sandbox3d_game_authoring.c",
}
changed = set(subprocess.check_output(["git", "diff", "--name-only", "HEAD"], cwd=ROOT, text=True).splitlines())
if changed != expected:
    raise RuntimeError(f"unexpected changed paths: {sorted(changed)}")
subprocess.run(["git", "diff", "--check"], cwd=ROOT, check=True)

# Remove the temporary GitHub-side applicator and its self-triggering workflow.
for temporary in (
    ROOT / "scripts" / "repair_prefab_material_persistence.py",
    ROOT / ".github" / "workflows" / "repair-prefab-material-persistence.yml",
):
    if temporary.exists():
        temporary.unlink()

print("Prefab material persistence repair applied; temporary helpers removed.")
