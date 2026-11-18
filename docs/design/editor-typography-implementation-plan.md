# Editor Typography Implementation Notes

This document summarizes the implementation boundaries and verification contract for Henka's editor typography. The detailed design and acceptance requirements are maintained in [Editor Typography and UI Scale](editor-typography.md).

## Resource ownership

- The application resource boundary reads verified bundled font bytes and supplies them to the opaque UI font resource. Core UI/text code does not discover arbitrary files or package paths.
- The UI font resource owns bounded glyph metrics, layout runs, and raster data. The editor and UI renderer consume that shared authority; they do not maintain separate measurement rules.
- Atkinson Hyperlegible Regular is the bundled production face. Noto Sans is retained only for explicit comparison validation. Both font assets and licenses identify their source and redistribution terms.

## Measurement and rendering

- One text-layout authority supplies measurement, fitting, truncation, row capacity, glyph placement, and drawing.
- Rasterization uses the effective app scale, output scale, font identity, and graphics-context identity. Glyph atlases are bounded and cached instead of being regenerated per frame.
- Mixed glyph and solid geometry preserves draw order. A frame cannot combine new layout metrics with an old raster profile.
- Any bitmap fallback is explicit, diagnostic, and excluded from successful packaged typography acceptance.

## Scale and persistence

- Editor UI Scale is one global persisted preference with the supported values 100%, 125%, 150%, and 200%. Workspace layouts consume the value but do not own or restore it.
- Legacy workspace-scale data is read only by the one-time migration. Existing layout geometry and records remain intact; later workspace restoration cannot override the global scale.
- A scale transition prepares metrics, layout, and every required context atlas before persisting or publishing the new state. Failure retains the previous complete working state and reports the failed stage.

## Display scale and graphics contexts

- Operating-system display scale is independent from Editor UI Scale and is applied once through the window/output transform.
- Display-scale changes use deterministic forward-draw and inverse-hit-test coverage. Native and detached contexts replace or reuse profiles within bounded cache limits.
- A non-100% Editor UI Scale capture does not substitute for packaged proof at a non-100% operating-system display scale.
- A detached OpenGL context is not visually certified by Win32 `PrintWindow`; until app-owned readback exists, require detached font/scale identity plus a subsequent successful engine render frame and record that no detached screenshot was captured.

## Verification contract

- Compare Atkinson and Noto with the same editor state, resolution, and native text size at all four Editor UI Scale values. Include common controls, compact rows, truncation, numbers, punctuation, and easily confused glyphs.
- Prove settings persistence, restart behavior, font identity, package inclusion, and explicit rejection of missing or corrupt production-font data.
- Recheck layout and hit regions at narrow and wide sizes, and exercise detached contexts and display-scale changes where the environment supports them.
- Use exact-source build and package provenance. Treat source tests, packaged interaction, visual comparison, and operating-system display-scale proof as separate evidence.
