# Rendering Realism

Henka's realism work is built as a layered, truthful rendering stack. The current renderer is a rasterized OpenGL path with physically based material inputs, HDR presentation, image-based lighting, local reflection probes, shadow maps, screen-space effects, and bounded temporal reconstruction. These systems improve realism without claiming capabilities that are not present.

## Current realism stack

The current Rendered path includes:

- glTF-oriented PBR material inputs for base color, metallic/roughness, normals, occlusion, emissive response, specular controls, IOR, transmission, volume attenuation, clearcoat, sheen, alpha modes, and double-sided rendering, plus runtime-authored subsurface amount/tint controls;
- a bounded direct-light backscatter and wrapped-light approximation for subsurface-tinted materials; this is not true multi-scatter diffusion, a skin/wax profile, thickness-texture SSS, or screen-space/ray-traced SSS;
- HDR environment lighting with transactionally derived 32-sample cosine-weighted irradiance, 32-sample GGX-prefiltered specular environment data across bounded mips, and a 32-sample split-sum BRDF lookup texture; this improves the rasterized environment response without claiming path tracing or full-scene global illumination;
- local reflection probes;
- directional, cascade, spot, and point shadow-map foundations;
- depth-derived ambient occlusion;
- bounded depth-derived screen-space reflections;
- bloom, exposure, ACES-style tone mapping, a restrained rendered grade, and reconstruction sharpening;
- bounded temporal history with motion, previous-depth, disocclusion, reactive-mask, and history-clamping safeguards.

Rendered post-processing keeps the bounded screen-space reflection and ambient-occlusion contributions in the linear HDR target until the single final exposure/tone-map/presentation transform. Reflection misses still retain the existing IBL, probe, or analytical fallback. This improves color-space correctness but does not turn the screen-space paths into full-scene reflections or production GTAO.

## Screen-space indirect diffuse lighting

Rendered presentation also contains a bounded screen-space indirect diffuse approximation. It reconstructs the current receiver position and normal from depth, samples nearby visible HDR surfaces in eight directions, rejects samples outside bounded distance and thickness limits, caps source radiance, and adds the gathered indirect contribution in HDR before bloom and tone mapping.

The purpose is to introduce visible local diffuse light transfer and color bleeding while preserving the existing OpenGL baseline.
The screen-space gather uses symmetric receiver-depth reconstruction, depth-edge confidence, and a small cross-filter on source radiance. Those filters intentionally suppress unstable high-frequency contributions at silhouettes, thin geometry, and other screen-space discontinuities instead of allowing the indirect term to amplify subpixel edge variation.

This is not full global illumination. The screen-space method cannot see geometry that is outside the current view, hidden behind another surface, or otherwise absent from the depth/color buffers. It is single-frame and bounded; it does not claim multi-bounce transport, probe-volume GI, hardware ray tracing, or path tracing.

## Direction

The next realism work should build from reference scenes. Effects outside those scenes should wait until a concrete visual need is identified. Important follow-up tracks are:

1. validate PBR energy response, texture color-space handling, normal-map behavior, roughness/metallic response, IBL calibration, and exposure against reference materials;
2. replace the bounded subsurface approximation with a production SSS solution when profile, thickness, ownership, and performance contracts are defined;
3. validate the screen-space indirect diffuse result for leaks, halos, over-brightening, camera-motion instability, and performance;
4. add a scene-space indirect-light solution that can represent off-screen and hidden contributors, such as a bounded irradiance-probe or probe-grid system;
5. improve local reflection-probe placement, blending, capture policy, and interaction with indirect diffuse lighting;
6. retain rasterization as the broad hardware baseline while designing future renderer-backend boundaries for optional hardware ray tracing;
7. consider a path-traced reference renderer later as a visual ground-truth tool, even if production games continue to use hybrid real-time rendering.

Spectral rendering is not part of the current implementation. It should remain research work until RGB PBR, indirect lighting, material import, color management, and reference-scene validation are mature.
