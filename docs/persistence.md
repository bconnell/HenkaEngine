# Persistence

Henka Engine now includes a small local-first persistence layer for settings and early project state.

## What it is for

The current persistence layer is meant for:

- sandbox preferences
- camera state
- simple local engine or project settings
- early external game prototypes that want a readable text format

It is not a finished save-game system yet.

## Current file format

Settings files use a simple `key=value` format.

Example:

```text
# Sandbox display settings
grid_visible=true
wireframe_enabled=false
mouse_sensitivity=0.002500
camera_position_x=0.000000
camera_position_y=2.400000
camera_position_z=8.600000
camera_yaw_radians=-1.570796
camera_pitch_radians=-0.220000
```

Blank lines and comment lines that start with `#` or `;` are accepted when loading.
Saved files are rewritten as plain `key=value` lines, so comment lines are not preserved yet.

## Current sandbox behavior

The sandbox stores local settings in:

- packaged run: `out/HenkaSandbox3D/user/sandbox3d.settings`
- development run: a `user/` folder beside the built sandbox executable

The current sandbox saves:

- debug grid visibility
- wireframe state
- mouse sensitivity
- camera movement speed
- camera position
- camera yaw
- camera pitch
- UI layout mode
- active utility panel
- selected scene object
- Scene Objects panel visibility
- Object Details panel visibility

The sandbox panel can also:

- save the current settings on demand
- reset the current sandbox settings back to defaults
- reset the camera to the default sandbox view
- adjust mouse sensitivity and camera speed before saving

## Missing or malformed settings

If the settings file is missing, the sandbox starts with safe defaults.

If the settings file contains invalid lines or invalid values:

- the sandbox keeps running
- valid values are still loaded when possible
- invalid values fall back to defaults
- a warning is printed to the console

The sandbox UI reflects the loaded state after startup, so the panel stays aligned with the current grid, wireframe, and camera settings.

## What is not supported yet

- cloud saves
- telemetry or analytics
- encryption
- registry storage
- binary serialization
- a general content database
- per-game save slot workflows

External games should decide their own save policy and data layout. The current Henka layer is a small foundation, not a complete answer for shipped game persistence.
