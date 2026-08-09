# Sandbox showcase assets

The normal Windows Sandbox startup presents two repo-owned sample models:

- `cheeky_giraffe.gltf` — an original stylized-realistic Cheeky Giraffe mascot with a long neck, spots, eyes, lashes, ears, ossicones, and a cheeky smile.
- `original_realistic_rocket.gltf` — an original plausible launch vehicle with painted body sections, metal bands, heat-resistant engine hardware, and stabilization fins.

## Ownership and generation

The Sandbox target build runs `scripts/generate_showcase_assets.ps1` for both files. The generator creates deterministic bounded geometry, UVs, normals, tangent vec4 attributes, PBR material definitions, and a sibling binary buffer. It uses no third-party model, no external authoring application at runtime, and no copyrighted vehicle or character model.

The generated glTF files reference a build-local copy of the repo-owned `assets/textures/cube_albedo.png`. The CMake post-build step places the glTF, `.bin`, and model-local texture beside the executable, so packaged execution resolves all dependencies from the package rather than the repository root.

## Runtime path

Sandbox loads both files through `henka_assets_load_gltf_scene_asset` and instantiates them with `henka_assets_instantiate_gltf_scene`. Materials and texture dependencies remain manager-owned and use the same glTF material path as imported consumer assets. A missing or corrupted generated file fails initialization through the normal bounded asset error path; it is not replaced by hardcoded geometry in `main.c`.

The engineering primitive gallery remains available with:

```text
HenkaSandbox3D.exe --primitive-gallery
```

That opt-in path preserves the cube, sphere, marker, fallback, foliage, and realism validation samples used by renderer, material, physics, and editor QA.

These are representative engine samples. Human visual QA is still required before treating the showcase as production-quality art.
