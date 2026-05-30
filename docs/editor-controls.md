# Editor controls

The sandbox includes local action-based viewport transform controls. Select a visible object, then start a transform with `M` or `G` for move, `R` for rotate, or `S` for scale. Move the mouse to preview the change. Press `X`, `Y`, or `Z` to constrain the active transform. Press `Enter` or `Left Mouse` to apply it, or press `Escape` or `Right Mouse` to restore the original transform.

`Left Ctrl` enables stepped adjustment while a transform is active. `Left Shift` enables finer adjustment. These are intentionally compact first-pass controls: move defaults to the X axis until constrained, rotate defaults to Y, and scale defaults to uniform scaling.

## Local profiles

The active profile and bindings appear in the sandbox Help utility. Controls are stored in the local `sandbox3d.settings` key/value file. The built-in profiles are protected defaults:

- `Henka Default`
- `Alternate Move`
- `Direct Transform`
- `Compact Tools`
- `Axis Focused`
- `Precision Layout`
- `Familiar Modeling`

Built-in profiles map only the transform actions currently supported by Henka. To keep the small runtime UI readable, custom profile creation and editing are config-based in this pass.

This example creates a named custom profile from `Henka Default`, makes it active, and keeps the standard transform bindings:

```text
controls.version=1
controls.active_profile=profile-my-controls
controls.custom_count=1
controls.custom.0.id=profile-my-controls
controls.custom.0.name=My Controls
controls.custom.0.base=0
controls.custom.0.move_tool=M,G
controls.custom.0.rotate_tool=R
controls.custom.0.scale_tool=S
controls.custom.0.constrain_x=X
controls.custom.0.constrain_y=Y
controls.custom.0.constrain_z=Z
controls.custom.0.confirm_transform=Enter,Mouse Left
controls.custom.0.cancel_transform=Escape,Mouse Right
controls.custom.0.snap_modifier=Left Ctrl
controls.custom.0.fine_adjustment_modifier=Left Shift
```

Profile names are trimmed and must not be blank or duplicated. Profile identifiers must be unique and remain stable when a profile is renamed. Invalid versions, profile references, keys, duplicate bindings, or unsupported inputs fall back to `Henka Default` without replacing the malformed control entries during an ordinary shutdown. `Reset Settings` restores the default profile.
