# Sandbox showcase assets

The normal Windows Sandbox startup presents two repo-owned sample models:

- `cheeky_giraffe.gltf` — an original stylized-realistic, animated-film-oriented Cheeky Giraffe mascot with smooth high-curvature body and head forms, surface-fitted spots, a readable mane row, modeled inner ears, connected tan ossicone stalks, and outward-angled dark-brown caps, layered eye whites/irises/pupils/highlights, nostrils, and a shaped mouth line. The presentation remains expressive by design, but the face is built from separately shaded anatomical features rather than a single cartoon eye material.
- `original_realistic_rocket.gltf` — an original stylized modern launch vehicle with a staged core, ogive-like fairing sections, interstage and engine-skirt hardware, dark avionics separation bands, clustered bell/nozzle geometry, a restrained heat response, and tapered stabilization fins.

## Ownership and generation

The Sandbox target build runs `scripts/generate_showcase_assets.ps1` for both files. The generator creates deterministic bounded geometry, UVs, normals, tangent vec4 attributes, PBR material definitions, and a sibling binary buffer. It uses no third-party model, no external authoring application at runtime, and no copyrighted vehicle or character model.

The generated glTF files use material factors plus the renderer-supported clearcoat, restrained sheen, and emissive-strength extensions; they do not bind the unrelated primitive-gallery cube texture. Their glTF and sibling binary buffers are copied beside the executable, so packaged execution resolves the showcase from package-owned files without a repository-root or runtime authoring dependency. These two models are the public visual reference set and should be regenerated and manually reviewed after material, lighting, geometry, and shading improvements. They are deliberately generic public samples; project-specific scenes and visual references are not part of this repository.

The packaged studio HDR fixture is also generated in memory and now contains bounded asymmetric warm-key and cool-fill area-light lobes. This gives clearcoat, brushed metal, and emissive engine details a stable highlight structure through the derived IBL path while retaining the direct local-light and shadow fixtures. It is an authored lighting reference, not a claim of photographic HDRI coverage.

The Sandbox studio floor uses a bounded 64 m graphite plane beneath the
independent debug grid. The larger surface keeps its finite far edge outside
ordinary showcase framing, so Rendered and Material Preview do not acquire an
unintended diagonal environment seam.

## In-engine visual acceptance

The normal Windows graphical path is the source of truth for showcase review. The
existing same-camera capture covers Solid, Material Preview, and Rendered, while
the dedicated mascot inspection capture launches the real Sandbox repeatedly
with the generated glTF scene loaded and records a normal startup frame plus
Giraffe close front, close three-quarter, close profile, and wide silhouette
Rendered views, a Giraffe front Material Preview comparison, and Rocket close
front, close three-quarter, and profile Rendered views. The dedicated views hide
only editor chrome for application-only inspection; they do not replace the
scene, material, lighting, asset-manager, or renderer path.

```powershell
.\scripts\capture_visual_evidence_windows.ps1 `
  -Configuration Debug `
  -OutputDirectory (Join-Path (Get-Location) "build\visual_evidence_giraffe") `
  -IncludeStartupShowcase `
  -IncludeGiraffeInspection
.\scripts\check_showcase_visual_evidence_windows.ps1 `
  -InputDirectory (Join-Path (Get-Location) "build\visual_evidence_giraffe")
```

The capture process waits up to a bounded 20 seconds for a CAPTURE_READY record
from the real application. That record proves both named subject groups are
visible, their authoritative bounds and render meshes are ready, the final
Scene View viewport is known, the front camera is level, the combined midpoint
is centered, both projected rectangles have safety margins, and three settled
frames have completed. The validator also requires matching readiness metadata
for Solid, Material Preview, and Rendered before checking the images. It then
checks that the evidence identifies the Henka Sandbox executable,
contains all required views, and rejects missing, flat, low-chroma, or
dimension-invalid frames. Those objective guards do not constitute human visual
approval: inspect the retained images for silhouette, facial-feature assembly,
neck/head attachment, eyes, ears, ossicones, nostrils, mouth, mane, spots,
intersections, gaps, and material/shading defects. Generated evidence remains
repo-local and is not committed.

## Runtime path

Sandbox loads both files through `henka_assets_load_gltf_scene_asset` and instantiates them with `henka_assets_instantiate_gltf_scene`. Materials and texture dependencies remain manager-owned and use the same glTF material path as imported consumer assets. A missing or corrupted generated file fails initialization through the normal bounded asset error path; it is not replaced by hardcoded geometry in `main.c`.

The engineering primitive gallery remains available with:

```text
HenkaSandbox3D.exe --primitive-gallery
```

That opt-in path preserves the cube, sphere, marker, fallback, foliage, and realism validation samples used by renderer, material, physics, and editor QA.

The generator rejects non-finite vertex data, invalid frames, out-of-range indices, degenerate triangles, and inward-wound faces before writing either asset. These remain representative engine samples rather than movie-production assets; human visual QA across Solid, Material Preview, and Rendered remains a required acceptance gate.
