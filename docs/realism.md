# Rendering Realism

Henka's realism work is built as a layered, truthful rendering stack. The current renderer is a rasterized OpenGL path with physically based material inputs, HDR presentation, image-based lighting, local reflection probes, shadow maps, screen-space effects, and bounded temporal reconstruction. These systems improve realism without claiming capabilities that are not present.

## Current realism stack

The current Rendered path includes:

- glTF-oriented PBR material inputs for base color, metallic/roughness, normals, occlusion, emissive response, specular controls, IOR, transmission, volume attenuation, clearcoat, sheen, alpha modes, and double-sided rendering;
- HDR environment lighting with derived irradiance, prefiltered specular environment data, and a BRDF lookup texture;
- local reflection probes;
- directional, cascade, spot, and point shadow-map foundations;
- depth-derived ambient occlusion;
- bounded depth-derived screen-space reflections;
- bloom, exposure, ACES-style tone mapping, a restrained rendered grade, and reconstruction sharpening;
- bounded temporal history with motion, previous-depth, disocclusion, reactive-mask, and history-clamping safeguards.

## Screen-space indirect diffuse lighting

Rendered presentation also contains a bounded screen-space indirect diffuse approximation. It reconstructs the current receiver position and normal from depth, samples nearby visible HDR surfaces in eight directions, rejects samples outside bounded distance and thickness limits, caps source radiance, and adds the gathered indirect contribution in HDR before bloom and tone mapping.

The purpose is to introduce visible local diffuse light transfer and color bleeding while preserving the existing OpenGL baseline.
The screen-space gather uses symmetric receiver-depth reconstruction, depth-edge confidence, and a small cross-filter on source radiance. Those filters intentionally suppress unstable high-frequency contributions at silhouettes, thin geometry, and other screen-space discontinuities instead of allowing the indirect term to amplify subpixel edge variation.

This is not full global illumination. The screen-space method cannot see geometry that is outside the current view, hidden behind another surface, or otherwise absent from the depth/color buffers. It is single-frame and bounded; it does not claim multi-bounce transport, probe-volume GI, hardware ray tracing, or path tracing.

## Direction

The next realism work should build from reference scenes rather than adding unrelated effects. Important follow-up tracks are:

1. validate PBR energy response, texture color-space handling, normal-map behavior, roughness/metallic response, IBL calibration, and exposure against reference materials;
2. validate the screen-space indirect diffuse result for leaks, halos, over-brightening, camera-motion instability, and performance;
3. add a scene-space indirect-light solution that can represent off-screen and hidden contributors, such as a bounded irradiance-probe or probe-grid system;
4. improve local reflection-probe placement, blending, capture policy, and interaction with indirect diffuse lighting;
5. retain rasterization as the broad hardware baseline while designing future renderer-backend boundaries for optional hardware ray tracing;
6. consider a path-traced reference renderer later as a visual ground-truth tool, even if production games continue to use hybrid real-time rendering.

Spectral rendering is not part of the current implementation. It should remain research work until RGB PBR, indirect lighting, material import, color management, and reference-scene validation are mature.