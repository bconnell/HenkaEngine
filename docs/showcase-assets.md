# Sandbox showcase assets

The normal Windows Sandbox startup presents two repo-owned sample models:

- `cheeky_giraffe.gltf` — an original stylized-realistic, animated-film-oriented Cheeky Giraffe mascot with smooth high-curvature body and head forms, surface-fitted spots, a readable mane row, eyes, lashes, ears, ossicones, nostrils, and a cheeky smile.
- `original_realistic_rocket.gltf` — an original stylized modern launch vehicle with a staged core, ogive-like fairing sections, interstage and engine-skirt hardware, integrated metal bands, a clustered heat-resistant engine section, and tapered stabilization fins.

## Ownership and generation

The Sandbox target build runs `scripts/generate_showcase_assets.ps1` for both files. The generator creates deterministic bounded geometry, UVs, normals, tangent vec4 attributes, PBR material definitions, and a sibling binary buffer. It uses no third-party model, no external authoring application at runtime, and no copyrighted vehicle or character model.

The generated glTF files use material factors plus the renderer-supported clearcoat and restrained sheen extensions; they do not bind the unrelated primitive-gallery cube texture. Their glTF and sibling binary buffers are copied beside the executable, so packaged execution resolves the showcase from package-owned files without a repository-root or runtime authoring dependency. These two models are the public visual reference set and should be regenerated and manually reviewed as renderer, material, lighting, and geometry capabilities improve.

## Runtime path

Sandbox loads both files through `henka_assets_load_gltf_scene_asset` and instantiates them with `henka_assets_instantiate_gltf_scene`. Materials and texture dependencies remain manager-owned and use the same glTF material path as imported consumer assets. A missing or corrupted generated file fails initialization through the normal bounded asset error path; it is not replaced by hardcoded geometry in `main.c`.

The engineering primitive gallery remains available with:

```text
HenkaSandbox3D.exe --primitive-gallery
```

That opt-in path preserves the cube, sphere, marker, fallback, foliage, and realism validation samples used by renderer, material, physics, and editor QA.

The generator rejects non-finite vertex data, invalid frames, out-of-range indices, degenerate triangles, and inward-wound faces before writing either asset. These remain representative engine samples rather than movie-production assets; human visual QA across Solid, Material Preview, and Rendered remains a required acceptance gate.
