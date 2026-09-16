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


# Verify the already-committed repair components before changing the final
# failing Prefab load boundary.
assets_c = read("engine/src/assets/assets.c")
for token in (
    "shader != NULL && asset->material.shader != shader",
    "if (shader == NULL)",
    "henka_assets_build_gltf_material_instance(manager, source_path, shader, &candidate)",
):
    if token not in assets_c:
        raise RuntimeError(f"expected cached material repair token missing: {token}")

game_c = read("examples/sandbox3d/game_authoring.c")
for token in (
    "candidate_object.renderer = live_object.renderer;",
    "sandbox3d_game_authoring_build_object(",
    "sandbox3d_game_authoring_sync_prefab_transform_override(",
):
    if token not in game_c:
        raise RuntimeError(f"expected Game Authoring save repair token missing: {token}")

test_c = read("tests/test_sandbox3d_game_authoring.c")
for token in (
    "static bool test_prefab_copy_file(",
    '"assets/shaders/basic_lit.vert"',
    '"assets/shaders/basic_lit.frag"',
    '"prefab_material_override_test.gltf"',
    '"prefab_apply_revert_history.gltf"',
    "henka_assets_load_gltf_material_asset(",
):
    if token not in test_c:
        raise RuntimeError(f"expected Prefab persistence test repair token missing: {token}")

# Final root fix: manager-owned material mode must not require the shader used
# only for inline material reconstruction. The downstream asset manager remains
# authoritative: a cached material can resolve with NULL; a cache miss with
# NULL still fails closed in henka_assets_load_gltf_material_asset().
prefab_path = "engine/src/core/prefab.c"
prefab_c = read(prefab_path)
old_guard = '''        if (asset_manager == NULL || inline_material_shader == NULL ||
            !henka_prefab_make_key(key, sizeof(key), index, "material_asset_path") ||
            !henka_settings_has_key(settings, key))'''
new_guard = '''        if (asset_manager == NULL ||
            !henka_prefab_make_key(key, sizeof(key), index, "material_asset_path") ||
            !henka_settings_has_key(settings, key))'''
prefab_c = replace_unique(
    prefab_c,
    old_guard,
    new_guard,
    "manager-owned Prefab material load guard",
)

# Preserve the separate inline-material requirement. This is an explicit
# fail-closed guard against accidentally broadening the repair.
inline_anchor = "if (shader == NULL || out_material == NULL ||"
if inline_anchor not in prefab_c:
    raise RuntimeError("inline material shader guard is missing after repair")
write(prefab_path, prefab_c)

# Remove every temporary GitHub-side diagnostic/repair helper. The final PR
# should contain only production/test changes, not debugging machinery.
temporary_paths = (
    ".github/repair/diagnose_prefab_material_persistence.py",
    ".github/workflows/diagnose-prefab-material-persistence.yml",
    ".github/workflows/repair-prefab-material-persistence.yml",
    "scripts/repair_prefab_material_persistence.py",
)
for relative in temporary_paths:
    path = ROOT / relative
    if not path.exists():
        raise RuntimeError(f"expected temporary helper is missing: {relative}")
    path.unlink()

expected_changes = {
    "engine/src/core/prefab.c",
    ".github/repair/diagnose_prefab_material_persistence.py",
    ".github/workflows/diagnose-prefab-material-persistence.yml",
    ".github/workflows/repair-prefab-material-persistence.yml",
    "scripts/repair_prefab_material_persistence.py",
}
changed = set(
    subprocess.check_output(
        ["git", "diff", "--name-only", "HEAD"],
        cwd=ROOT,
        text=True,
    ).splitlines()
)
if changed != expected_changes:
    raise RuntimeError(f"unexpected finalizer diff paths: {sorted(changed)}")

subprocess.run(["git", "diff", "--check"], cwd=ROOT, check=True)
print("Final Prefab material load guard applied; temporary helpers removed.")
