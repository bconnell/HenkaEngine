# Roadmap

Henka Engine is an early-stage open source C engine. The current goal is to build a stable runtime foundation first, then grow toward practical tools for 3D, 2D, and 2.5D projects.

This roadmap is a direction guide, not a release schedule. Priorities may change as the engine matures, testing finds issues, or core systems need more hardening.

## Current focus

The current work is focused on making the 3D sandbox and engine core dependable enough to build on.

Current priorities include:

1. Stable engine startup and shutdown.
2. Clear platform, renderer, input, scene, and camera boundaries.
3. Reliable object selection and transform behavior.
4. Local settings and save-data foundations.
5. Asset loading for shaders, textures, and simple model data.
6. A packaged sandbox that can be tested without private setup.
7. Documentation that stays aligned with what the engine actually does.
8. Test coverage for core behavior that should not depend on manual QA.
9. A scoped rigid-body physics layer with sandbox inspection, debug visualization, and clearer body-type guidance.

## Near-term priorities

The next development work is focused on making existing systems easier to test, easier to use, and harder to break.

1. Add and harden local action commands for validated scene and object operations.
2. Keep viewport interaction test helpers aligned with the real sandbox behavior.
3. Finish manual QA for transform gizmos and fix any remaining interaction problems.
4. Keep Windows CI deterministic across build, test, package provenance, packaged startup, repository integrity, and external-project checks.
5. Continue improving sandbox usability, help text, first-run guidance, and visual feedback.
6. Keep object inspection, selection highlighting, transform tools, physics QA, and viewport behavior consistent.
7. Keep external game project templates working against the current engine.

## Workspace and tools

Henka is moving toward a practical developer workspace, but this should happen in layers.

Current workspace foundations include:

1. Docked panels with a dedicated Scene View.
2. Session-only native detached tool windows with close-to-redock recovery.
3. Resizable occupied dock regions and reset-layout recovery.
4. Visible workspace and viewport interaction diagnostics.
5. A multi-window platform foundation with a separate native test panel for render and event-routing validation.

Current runtime foundations also include rigid-body physics v1: fixed-step worlds, static/dynamic/kinematic bodies, sphere/AABB/plane collision, impulse response, friction, restitution, trigger events, raycasts, opt-in sandbox QA controls, and viewport selection highlighting for the selected real scene object.

Planned workspace improvements include:

1. Finish full controls and drag-back redocking for native detached tool windows.
2. Add saved workspace placement and dock sizes.
3. Add an in-window controls editor for the existing local keybinding profiles.
4. A detachable Scene View after multi-window rendering and viewport input are dependable.
5. A clearer scene hierarchy.
6. Numeric transform editing.
7. Undo and redo for basic scene operations.
8. Scene save and load support.

These features should appear only when they are wired into the engine, tested, documented, and useful.

## Asset and material workflow

The asset pipeline is still early. The goal is to move from loose sample assets toward a clearer project workflow.

Planned asset and material work includes:

1. Stronger asset metadata.
2. Better model import coverage.
3. Texture and material assignment tools.
4. Clear missing-asset fallback behavior.
5. Material editing.
6. Shader selection.
7. Procedural shader planning and safe parameter handling.
8. External project asset roots.

Procedural shader work should come after the material system is stable enough to support it cleanly.

## 2D and 2.5D direction

Henka is planned to support 2D and 2.5D as first-class workflows, not as afterthoughts.

Planned 2D work includes:

1. A dedicated 2D renderer path.
2. Sprites.
3. Texture regions.
4. Layers.
5. A 2D camera.
6. A focused 2D sample.

The first 2.5D camera foundation includes:

1. Perspective 3D, side, top-down, and isometric camera presets.
2. Stable exact-vertical top-down camera basis handling.
3. Orthographic zoom and frame-selected sizing.
4. Sandbox controls and local persistence for the selected camera preset.

Next 2.5D work includes:

1. Sprite-facing quad and texture-sampling foundations.
2. Transparent and cutout material render states.
3. Sprite and texture-region data.
4. Layered depth and deterministic sorting.
5. Parallax.
6. Movement-plane and physics-axis constraints.
7. Tools that make 2D-style layout in 3D space easier to manage.

## Longer-term systems

Longer-term work may include:

1. Expanded physics features such as joints, controllers, and additional collider types.
2. Audio.
3. Scripting or extension support.
4. Additional renderer backends.
5. Release packaging.
6. Versioned builds.
7. Checksums and release verification.
8. Provenance for release artifacts.

These systems will require careful design because they affect safety, project structure, and long-term maintenance.

## External project workflow

Henka Engine should remain a reusable engine repository. Real games built with Henka should live in separate repositories.

Planned external project work includes:

1. Stronger starter templates.
2. Clear project configuration.
3. Separate asset and scene roots.
4. Build guidance for external projects.
5. Packaging guidance for external projects.
6. Validation scripts that prove templates still work.

## Sponsorship supported work

Henka Engine is open source, and sponsorship helps support the time needed to continue development.

Funding can help with engine development, sandbox usability, documentation, examples, packaged builds, testing, asset workflow improvements, and future workspace tools.

Sponsorship does not change the license, purchase feature priority, or override the project roadmap. Roadmap decisions remain based on stability, maintainability, scope, and usefulness to the engine.

## Current limitations

Henka is still early. Some systems are intentionally limited while the engine foundation is being built.

Current limitations include:

1. The sandbox is an engine sample and QA target, not a game.
2. The transform gizmo workflow still needs manual desktop QA for visual feel and mouse comfort.
3. Scene saving and loading are not complete authoring workflows yet.
4. The UI is useful for inspection and testing, but it is not a full editor.
5. Workspace movement and sizing require desktop QA for feel. Detached placement is session-only, and full detached controls plus OS-title-bar drag-back docking still need implementation.
6. The native test panel and compact detached production-panel surfaces use multi-window rendering and event routing, but full detached controls and detachable Scene View are not implemented yet.
7. Asset loading is still limited.
8. The 2D workflow and the sprite, layer, parallax, animation, and movement-constraint parts of 2.5D are not implemented yet; the first 2.5D camera presets are available.
9. Physics v1 is intentionally limited to rigid bodies and primitive colliders; mesh collision, joints, character controllers, and advanced simulation remain future work.
10. Audio, scripting, and release distribution are later milestones.
