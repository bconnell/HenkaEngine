# UI

Henka Engine now includes a small in-window UI overlay for the sandbox.

## What it is

The current UI layer is an early engine-owned overlay that can draw:

- panels
- labels
- buttons
- toggles

It is meant to make engine samples easier to inspect and control without pulling in a larger third-party UI stack.

## What it is not

The current UI layer is not:

- a full editor
- a docking system
- a scene hierarchy
- an inspector
- an asset browser
- a full runtime UI framework

## Current design

The current UI path is intentionally dependency-conscious:

- no ImGui
- no FreeType
- no external UI library
- no bundled font file

Text is rendered from a small built-in glyph table that lives in Henka source code. That keeps the current UI self-contained and easy to package.

## Renderer path

The UI overlay draws through the existing renderer after the 3D scene.

- Applications use the public UI API through Henka headers.
- OpenGL details stay inside renderer implementation files.
- SDL details stay inside the platform layer.

This keeps the public UI-facing API small and engine-oriented instead of exposing backend types directly.

## Current text limitations

The built-in text path is intentionally simple:

- fixed-size glyphs
- basic ASCII-oriented coverage for current sandbox labels
- no kerning
- no shaping
- no word wrapping system
- no Unicode layout support

That is enough for the current sandbox panel, status text, and small runtime controls.

## Sandbox panel

In `henka_sandbox3d`, press `F4` to open the panel.

The current panel can:

- toggle the debug grid
- toggle wireframe
- adjust mouse sensitivity
- adjust camera speed
- reset the camera
- save sandbox settings
- reset sandbox settings
- print help
- print the scene legend

It also shows short state text for mouse capture, frame timing, camera position, and local runtime paths.

When the panel is open:

- mouse capture is released
- mouse look pauses
- camera movement pauses
- you can click the UI with the left mouse button
- `Escape` closes the panel before it returns to the usual mouse-capture and exit flow

## Future direction

This layer is a foundation for better engine-side inspection and sample controls. It is not yet meant to replace the planned editor work or a broader UI toolkit.
