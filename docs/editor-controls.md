# Editor Controls

> **Status:** Available bounded viewport-transform controls

The Sandbox provides local action-based transform controls for selected visible objects.

## Transform controls

| Action | Input |
| --- | --- |
| Move | `M` or `G` |
| Rotate | `R` |
| Scale | `S` |
| Constrain X | `X` |
| Constrain Y | `Y` |
| Constrain Z | `Z` |
| Apply | `Enter` or `Left Mouse` |
| Cancel and restore original transform | `Escape` or `Right Mouse` |
| Stepped adjustment | Hold `Left Ctrl` |
| Fine adjustment | Hold `Left Shift` |

Move the mouse after starting a transform to preview the result.

Current defaults are:

- Move starts on the X axis until constrained.
- Rotate starts on the Y axis until constrained.
- Scale starts as uniform scaling.

## Local profiles

The active profile and bindings appear in the Sandbox Help utility. Profiles are stored in the local `sandbox3d.settings` key/value file.

### Built-in profiles

The protected built-in profiles are:

- `Henka Default`
- `Alternate Move`
- `Direct Transform`
- `Compact Tools`
- `Axis Focused`
- `Precision Layout`
- `Familiar Modeling`

Built-in profiles expose the transform actions currently supported by Henka. Custom profile creation and editing are configuration-based in the current UI foundation.

## Custom profile example

The following configuration creates `My Controls` from the `Henka Default` base and keeps the standard transform bindings:

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

## Validation and fallback behavior

Profile names are trimmed and must be unique and non-empty. Profile identifiers must also be unique and remain stable across profile renames.

The loader validates:

- format version;
- active-profile references;
- binding keys;
- duplicate bindings;
- supported input names;
- custom profile IDs and names.

Invalid control configuration activates `Henka Default`. The malformed entries remain untouched during ordinary shutdown so the file can be inspected and corrected.

`Reset Settings` restores the default profile.
